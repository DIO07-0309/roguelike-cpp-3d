// M6-v2e/v2f: 光空间深度 RT — rlgl 原语 depth-only fbo + 墙/billboard 深度 pass
// 深度纹理走 GL_DEPTH_ATTACHMENT (无色彩附件; GPU depth-only 可用)
// v2f: 实体剪影进深度 pass (hd2d_depth.fs alpha-discard, 透明像素不写深度)
#include "hd2d_shadow_caster.h"
#include "hd2d_renderer.h"          // HD2DDrawItem
#include "hd2d_part_geometry.h"     // A5-T4: pro 件同一几何进剪影
#include "hd2d_shader_bank.h"       // v2f: depth shader 加载
#include "core/logger.h"
#include "rlgl.h"
#include "config.h"                 // TILE_SIZE
#include <cmath>

HD2DShadowCaster& HD2DShadowCaster::inst() {
    static HD2DShadowCaster caster;
    return caster;
}

bool HD2DShadowCaster::ensure_init(int scene_w, int scene_h) {
    if (_ready) return true;
    _ready = _create_depth_target(scene_w / 2, scene_h / 2);
    if (_ready) {
        _load_depth_shader();                  // v2f: 剪影 shader (失败可降级)
        LOG_INFO("HD2D shadow: 深度 RT 就绪 (%dx%d)", _map_w, _map_h);
    } else {
        LOG_WARN("HD2D shadow: 深度 RT 不支持, 回退 blob shadow");
    }
    return _ready;
}

// ── depth-only fbo: rlgl 原语组装 (fbo + depth 纹理附件) ──
bool HD2DShadowCaster::_create_depth_target(int map_w, int map_h) {
    _map_w = map_w;
    _map_h = map_h;
    _fbo_id = rlLoadFramebuffer(map_w, map_h);
    if (_fbo_id == 0) return false;
    // useRenderBuffer=false → 深度可采样纹理 (非 renderbuffer)
    _depth_tex_id = rlLoadTextureDepth(map_w, map_h, false);
    if (_depth_tex_id == 0) { _fbo_id = 0; return false; }
    rlFramebufferAttach(_fbo_id, _depth_tex_id, RL_ATTACHMENT_DEPTH,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    if (!rlFramebufferComplete(_fbo_id)) {
        rlUnloadFramebuffer(_fbo_id);
        _fbo_id = 0;
        _depth_tex_id = 0;
        return false;
    }
    return true;
}

void HD2DShadowCaster::shutdown() {
    if (_fbo_id > 0) rlUnloadFramebuffer(_fbo_id);
    if (_depth_tex_id > 0) rlUnloadTexture(_depth_tex_id);
    _fbo_id = 0;
    _depth_tex_id = 0;
    _ready = false;
}

// ── v2f: alpha-discard 剪影 shader (hd2d_depth.fs + 共享 hd2d_world.vs) ──
bool HD2DShadowCaster::_load_depth_shader() {
    auto& bank = HD2DShaderBank::inst();
    _depth_shader = bank.load("hd2d_depth", "hd2d_world");
    _depth_shader_ok = bank.is_valid("hd2d_depth");
    if (!_depth_shader_ok)
        LOG_WARN("HD2D shadow: depth shader 缺失, billboard 不投影 (墙照常)");
    return _depth_shader_ok;
}

// ── 光空间相机: 45° 方向光正交投影, 罩住视野半径 ~16 tile ──
// (与 renderer._light_dir 同源的固定方向; 光随相机焦点平移)
void HD2DShadowCaster::update_light_camera(Vector3 cam_focus,
                                           const Camera3D* view_camera) {
    _view_camera_override = view_camera;       // v2f: billboard 朝向源
    float world_radius = TILE_SIZE * 17.0f;   // 覆盖 ±16 tile 视野 + 1 缓冲
    _texel_world_size = (world_radius * 2.0f) / (float)_map_w;

    // 光位置 = 焦点上方沿 -light_dir 反向 (0.35, -1, 0.25)
    Vector3 light_pos = {
        cam_focus.x - 0.35f * world_radius,
        cam_focus.y + world_radius,
        cam_focus.z - 0.25f * world_radius
    };
    _light_camera.position = light_pos;
    _light_camera.target = cam_focus;
    _light_camera.up = {0, 1, 0};
    _light_camera.fovy = world_radius * 2.0f;    // 正交 fovy = 世界高度
    _light_camera.projection = CAMERA_ORTHOGRAPHIC;

    _light_view = MatrixLookAt(_light_camera.position, _light_camera.target,
                               _light_camera.up);
    float r = world_radius;
    _light_proj = MatrixOrtho(-r, r, -r, r, 0.0, world_radius * 4.0);
}

// ── 深度 pass: 墙 + billboard 实体 (v2f) ──
void HD2DShadowCaster::render_depth(
        const GameScene& gs, const std::vector<HD2DDrawItem>& items,
        unsigned int outer_fbo) {
    if (!_ready) return;
    // 备份主相机矩阵 (rlSetMatrix* 直接替换内部状态, 需手动还原)
    Matrix saved_proj = rlGetMatrixProjection();
    Matrix saved_modelview = rlGetMatrixModelview();

    rlEnableFramebuffer(_fbo_id);
    rlViewport(0, 0, _map_w, _map_h);
    rlSetMatrixProjection(_light_proj);
    rlSetMatrixModelview(_light_view);
    rlClearScreenBuffers();                     // depth-only fbo: 清深度
    rlEnableDepthTest();

    // v2f: billboard 剪影 (alpha-discard; 需主相机朝向参数保持几何一致)
    Camera3D view_camera = {};
    const Camera3D* cam_ptr = _view_camera_override;
    if (!cam_ptr) {
        view_camera.position = {0, 640, 320};
        view_camera.target = {0, 0, 0};
        view_camera.up = {0, 1, 0};
        view_camera.fovy = 50.0f;
        view_camera.projection = CAMERA_PERSPECTIVE;
        cam_ptr = &view_camera;
    }
    if (_depth_shader_ok) {
        BeginShaderMode(_depth_shader);
        for (const auto& item : items)
            if (item.kind == HD2DDrawItem::Kind::ENTITY_BILLBOARD
                && item.texture.id > 0)
                _draw_billboard_depth(item, *cam_ptr);
        EndShaderMode();
    }
    for (const auto& item : items)
        if (item.kind == HD2DDrawItem::Kind::WALL_BLOCK) _draw_wall_depth(item);

    rlDisableDepthTest();
    // 还原: 绑回外层渲染目标 (caller 注入; 5.0 无当前 FBO 查询 API)
    rlEnableFramebuffer(outer_fbo);
    rlSetMatrixProjection(saved_proj);
    rlSetMatrixModelview(saved_modelview);
    rlViewport(0, 0, _map_w * 2, _map_h * 2);   // = scene_w/h (map 为其一半)
}

// ── v2f: billboard 深度几何 — 复用 DrawBillboardRec 顶点公式 ──
// 相机参数只参与朝向数学 (面朝相机方向), 顶点是世界坐标 → 被当前
// rlSetMatrix 的光空间矩阵变换; 与主 pass _draw_billboard 几何同源
// (含 flip_x 负宽源矩形处理), 剪影像素 = 主 pass 可见像素
void HD2DShadowCaster::_draw_billboard_depth(const HD2DDrawItem& item,
                                             const Camera3D& view_camera) {
    if (item.pro_mode) {
        hd2d::drawPartQuad(item, view_camera, hd2d::PartPass::Shadow);
        return;
    }
    Vector3 pos = item.world_pos;
    float w = item.size;
    float h = item.size * 1.5f;
    Rectangle src = item.tex_src.width > 0 ? item.tex_src
        : Rectangle{0, 0, (float)item.texture.width, (float)item.texture.height};
    if (item.flip_x) src.width = -src.width;
    DrawBillboardRec(view_camera, item.texture, src,
                     {pos.x, h * 0.5f, pos.z}, {w, h}, WHITE);
}

// ── 单墙深度 quad ×4 面 + 顶面 (复用 renderer._wall_quad 顶点公式) ──
void HD2DShadowCaster::_draw_wall_depth(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float h = item.height;
    float e = item.size * 0.5f;
    rlBegin(RL_QUADS);
    // 北/南/东/西 + 顶 (纯深度, 无 UV/颜色需求; 法线省略)
    // 北
    rlVertex3f(pos.x - e, 0, pos.z + e); rlVertex3f(pos.x + e, 0, pos.z + e);
    rlVertex3f(pos.x + e, h, pos.z + e); rlVertex3f(pos.x - e, h, pos.z + e);
    // 南
    rlVertex3f(pos.x - e, 0, pos.z - e); rlVertex3f(pos.x + e, 0, pos.z - e);
    rlVertex3f(pos.x + e, h, pos.z - e); rlVertex3f(pos.x - e, h, pos.z - e);
    // 东
    rlVertex3f(pos.x + e, 0, pos.z - e); rlVertex3f(pos.x + e, 0, pos.z + e);
    rlVertex3f(pos.x + e, h, pos.z + e); rlVertex3f(pos.x + e, h, pos.z - e);
    // 西
    rlVertex3f(pos.x - e, 0, pos.z - e); rlVertex3f(pos.x - e, 0, pos.z + e);
    rlVertex3f(pos.x - e, h, pos.z + e); rlVertex3f(pos.x - e, h, pos.z - e);
    // 顶
    rlVertex3f(pos.x - e, h, pos.z - e); rlVertex3f(pos.x + e, h, pos.z - e);
    rlVertex3f(pos.x + e, h, pos.z + e); rlVertex3f(pos.x - e, h, pos.z + e);
    rlEnd();
}
