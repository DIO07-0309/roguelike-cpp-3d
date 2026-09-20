// M6-HD2D: HD-2D 渲染器实现 — 垂直切片第一步 (骨架 + 相机 + 地形/billboard)
// 切片范围: 透视相机 45° 俯视 / 墙体盒子伪光照 / 实体 billboard / 基础后处理
#include "hd2d_renderer.h"
#include "hd2d_part_geometry.h"
#include "hd2d_scene_builder.h"
#include "hd2d_shader_bank.h"               // M6-v2c: GLSL 加载
#include "hd2d_post_fx.h"                   // M6-v2c: bloom 链
#include "hd2d_shadow_caster.h"             // M6-v2e: 光空间深度 RT
#include "core/logger.h"                    // P1-C9: 3D 激活日志
#include "rlgl.h"                           // M6-v2a: rl 原语 (贴图地板/墙)
#include "core/scene_tree.h"                // M6-v2c: main_target (bloom 源)
#include "scenes/game_scene.h"
#include "world/game_map.h"
#include "entities/player.h"
#include "entities/monster.h"
#include <algorithm>
#include <cmath>                            // A1.1: tanf/sqrtf/fabsf (outline 换算)
#include <cstring>                          // M6-v2g: strcmp (biome 预设)

bool g_hd2d_mode = false;
int  g_hd2d_autoshot = 0;     // M6-i.1: >0 = 第 N 帧自动截图一次 (取证)

// ── A1.1: 3D-aware billboard outline 参数 ──
// 描边宽度在世界空间定义 (基准 kOutlineBaseWorldW px), 按相机距离换算屏幕
// 像素后 clamp [1,4] 再转 UV 偏移 → 远近视觉厚度稳定 (HD-2D), 不做固定 texel
namespace {
constexpr float kOutlineBaseWorldW = 2.0f;   // 基准描边宽 (世界单位)
constexpr float kOutlineMinScreenPx = 1.0f;  // 远距最小可辨识保护
constexpr float kOutlineMaxScreenPx = 4.0f;  // 近距 Boss 过粗保护
constexpr float kOutlineAlphaCutoff = 0.5f;  // 剪影阈值 (与 depth pass 同源)
}  // namespace

HD2DRenderer& HD2DRenderer::inst() {
    static HD2DRenderer renderer;
    return renderer;
}

bool HD2DRenderer::ensure_init(int target_w, int target_h) {
    if (_ready) return true;
    _target_w = target_w;
    _target_h = target_h;
    _setup_camera();
    _load_terrain_shaders();      // M6-v2c: 雾/岩浆 (失败自动回退)
    _load_outline_shader();
    if (!_part_color.initialize(_outline_shader))
        LOG_WARN("HD2D part cutout unavailable; using static player billboards");
    _make_blob_shadow_tex();     // M6-v2c: blob shadow 程序纹理
    _make_mote_glow_tex();       // A2.1: 氛围粒子软光纹理
    _ready = true;
    LOG_INFO("HD2D: 3D 表现层已激活");  // P1-C9: 确认 3D 分支生效 (回退静默时日志可辨)
    return true;
}

void HD2DRenderer::shutdown() {
    _draw_items.clear();
    if (_blob_shadow_tex.id > 0) UnloadTexture(_blob_shadow_tex);
    _blob_shadow_tex = {};
    if (_mote_glow_tex.id > 0) UnloadTexture(_mote_glow_tex);   // A2.1
    _mote_glow_tex = {};
    _fog_ok = _lava_ok = _outline_ok = false;
    _part_color = {};
    _ready = false;
}

// ── M6-v2c: 地形 shader 懒加载 + uniform 位置缓存 ──
void HD2DRenderer::_load_terrain_shaders() {
    auto& bank = HD2DShaderBank::inst();
    _fog_shader = bank.load("hd2d_fog", "hd2d_world");
    _fog_ok = _fog_shader.id > 0;
    if (_fog_ok) {
        _fog_viewpos_loc = GetShaderLocation(_fog_shader, "viewPos");
        _fog_color_loc = GetShaderLocation(_fog_shader, "fogColor");
        _fog_start_loc = GetShaderLocation(_fog_shader, "fogStart");
        _fog_end_loc = GetShaderLocation(_fog_shader, "fogEnd");
        // 默认雾参数 (距离按相机距离; 贴图 tile 不暗但远处入雾)
        float fog_start = 760.0f, fog_end = 1250.0f;
        // M6-i.1: 520/900 → 760/1250 — 原值把相机 800px 处的视野内地板
        // 混合 60% 深夜雾色 (近黑); 推迟起点保住玩家视野内材质可读性
        SetShaderValue(_fog_shader, _fog_start_loc, &fog_start,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(_fog_shader, _fog_end_loc, &fog_end,
                       SHADER_UNIFORM_FLOAT);
        _cache_v2e_uniform_locs();
    }
    _lava_shader = bank.load("hd2d_lava", "hd2d_world");
    _lava_ok = _lava_shader.id > 0;
    if (_lava_ok)
        _lava_time_loc = GetShaderLocation(_lava_shader, "uTime");
}

// ── A1.1: billboard 真轮廓 shader 加载 (alpha-mask 8 邻域; 失败→回退旧描边) ──
void HD2DRenderer::_load_outline_shader() {
    _outline_shader = HD2DShaderBank::inst().load("hd2d_billboard_outline",
                                                  "hd2d_world");
    _outline_ok = _outline_shader.id > 0;
    if (!_outline_ok) return;   // bank 已 LOG_WARN; _draw_billboard 走旧 4 向路径
    _outline_off_loc = GetShaderLocation(_outline_shader, "uTexelOffset");
    _outline_color_loc = GetShaderLocation(_outline_shader, "uOutlineColor");
    _outline_thresh_loc = GetShaderLocation(_outline_shader, "uAlphaThreshold");
    // A3: 实体接收阴影 — 同名 uniform 的 loc 独立缓存 (fs 缺失时=-1→自动禁用)
    _out_shadow_map_loc = GetShaderLocation(_outline_shader, "shadowMap");
    _out_shadow_mvp_loc = GetShaderLocation(_outline_shader, "lightViewProj");
    _out_shadow_on_loc = GetShaderLocation(_outline_shader, "shadowEnabled");
    _out_shadow_texel_loc = GetShaderLocation(_outline_shader, "shadowTexel");
    _out_shadow_bias_loc = GetShaderLocation(_outline_shader, "shadowBias");
    SetShaderValue(_outline_shader, _outline_thresh_loc, &kOutlineAlphaCutoff,
                   SHADER_UNIFORM_FLOAT);
}

// ── A1.1: 每帧公共换算 — 1 世界单位在屏幕上占多少像素 ──
// (透视: screen_px = world / (2·d·tan(fovy/2)) · RT 高; 取相机→焦点距离,
//  与 billboard 距焦距离差异在俯视切片内可忽略, 保持单一公共值)
void HD2DRenderer::_update_px_per_world() {
    float dx = _camera.position.x - _camera.target.x;
    float dy = _camera.position.y - _camera.target.y;
    float dz = _camera.position.z - _camera.target.z;
    float d = sqrtf(dx * dx + dy * dy + dz * dz);
    _px_per_world = (d > 1.0f)
        ? (float)_target_h / (2.0f * d * tanf(_camera.fovy * DEG2RAD * 0.5f))
        : 1.56f;   // 退化保护: 440 距离/50°/640 高的标称值
}

// ── M6-v2e: 阴影/点光 uniform 位置缓存 (地形 shader 一次) ──
void HD2DRenderer::_cache_v2e_uniform_locs() {
    _shadow_map_loc = GetShaderLocation(_fog_shader, "shadowMap");
    _shadow_mvp_loc = GetShaderLocation(_fog_shader, "lightViewProj");
    _shadow_on_loc = GetShaderLocation(_fog_shader, "shadowEnabled");
    _shadow_texel_loc = GetShaderLocation(_fog_shader, "shadowTexel");
    _shadow_bias_loc = GetShaderLocation(_fog_shader, "shadowBias");
    _pl_count_loc = GetShaderLocation(_fog_shader, "pointLightCount");
    _pl_pos_loc = GetShaderLocation(_fog_shader, "pointLightPos");
    _pl_color_loc = GetShaderLocation(_fog_shader, "pointLightColor");
    _pl_range_loc = GetShaderLocation(_fog_shader, "pointLightRange");
}

// ── M6-v2c: 雾 uniforms 上传 (视点距离 smoothstep + 常量雾色) ──
void HD2DRenderer::_upload_fog_uniforms() {
    Vector3 view_pos = _camera.position;
    SetShaderValue(_fog_shader, _fog_viewpos_loc, &view_pos,
                   SHADER_UNIFORM_VEC3);
    // M6-i.1: 雾色 12,14,24 → 28,30,48 (深夜蓝黑提到暗蓝, 避免雾区纯黑)
    // i.1-fix2: 雾色从蓝 (28,30,48) 改暖暗 (30,26,22) — 远景入雾不再拉冷群系
    Vector4 fog_color = {30.0f / 255.0f, 26.0f / 255.0f, 22.0f / 255.0f, 1.0f};
    SetShaderValue(_fog_shader, _fog_color_loc, &fog_color,
                   SHADER_UNIFORM_VEC4);
    _upload_shadow_uniforms();                     // v2e: 阴影
    _upload_point_lights();                         // v2e: 点光源 (LAVA+玩家)
}

// ── M6-v2e: 阴影 uniforms 上传 (每帧; 深度纹理包装 Texture2D 走标准 API) ──
void HD2DRenderer::_upload_shadow_uniforms() {
    _upload_shadow_to(_fog_shader, _shadow_map_loc, _shadow_mvp_loc,
                      _shadow_on_loc, _shadow_texel_loc, _shadow_bias_loc);
}

// A3: 阴影参数上传共用 (地形 fog / billboard outline 两套 loc 同一数据源)
void HD2DRenderer::_upload_shadow_to(Shader sh, int map_loc, int mvp_loc,
                                     int on_loc, int texel_loc, int bias_loc) {
    auto& shadow = HD2DShadowCaster::inst();
    bool active = shadow.is_ready() && map_loc >= 0;
    float enabled = active ? 1.0f : 0.0f;
    SetShaderValue(sh, on_loc, &enabled, SHADER_UNIFORM_FLOAT);
    if (!active) return;
    // raylib 5.0: proj*view = lightViewProj (raymath row-major, shader 内
    // mat4 列主序 — SetShaderValueMatrix 内部已做转置适配)
    Matrix light_vp = MatrixMultiply(shadow.light_proj(), shadow.light_view());
    SetShaderValueMatrix(sh, mvp_loc, light_vp);
    float texel = shadow.depth_texel();             // 1/depth 尺寸 (PCF 步长)
    SetShaderValue(sh, texel_loc, &texel, SHADER_UNIFORM_FLOAT);
    float bias = shadow.texel_world_size() * 1.5f;  // 世界 texel → 深度补偿
    SetShaderValue(sh, bias_loc, &bias, SHADER_UNIFORM_FLOAT);
    // 深度纹理 (rlgl 裸 id) 包装 Texture2D POD → SetShaderValueTexture
    Texture2D depth_tex_pod = {};
    depth_tex_pod.id = shadow.depth_tex_id();
    depth_tex_pod.width = shadow.map_width();
    depth_tex_pod.height = shadow.map_height();
    depth_tex_pod.mipmaps = 1;
    SetShaderValueTexture(sh, map_loc, depth_tex_pod);
}

// ── M6-v2e/v2g: 点光源收集上传 (LAVA tile 聚类 + 玩家暖光; 只读) ──
// v2g: 网格聚类 — LAVA tile 按 4-tile 网格分桶, 每桶 1 个代表光
// (火山层岩浆 tile 数量不设限, 光源数恒 ≤7; 桶内首 tile 即锚点)
void HD2DRenderer::_upload_point_lights() {
    if (_pl_count_loc < 0) return;
    Vector3 positions[8];
    Vector3 colors[8];
    float ranges[8];
    int count = 0;
    long seen_buckets[7] = {-1, -1, -1, -1, -1, -1, -1};   // 已亮桶指纹
    const float bucket_span = TILE_SIZE * 4.0f;            // 聚类网格宽
    for (const auto& item : _draw_items) {
        if (count >= 7) break;
        if (item.kind != HD2DDrawItem::Kind::FLOOR_TILE || !item.is_lava
            || item.texture.id != 0)
            continue;
        long bx = (long)(item.world_pos.x / bucket_span);
        long bz = (long)(item.world_pos.z / bucket_span);
        long key = bx * 100003L + bz;
        bool dup = false;
        for (int i = 0; i < count && !dup; i++) dup = (seen_buckets[i] == key);
        if (dup) continue;                                 // 桶已亮, 跳过
        seen_buckets[count] = key;
        positions[count] = {item.world_pos.x, 6.0f, item.world_pos.z};
        colors[count] = {0.55f, 0.22f, 0.06f};
        ranges[count] = TILE_SIZE * 3.5f;                  // v2g: 4→3.5 tile
        count++;
    }
    // 玩家随身暖光 (火把感; _camera_focus 即玩家世界 x/z)
    positions[count] = {_camera_focus.x, 14.0f, _camera_focus.z};
    colors[count] = {0.16f, 0.12f, 0.07f};
    ranges[count] = TILE_SIZE * 2.5f;
    count++;
    SetShaderValue(_fog_shader, _pl_count_loc, &count, SHADER_UNIFORM_INT);
    SetShaderValueV(_fog_shader, _pl_pos_loc, positions,
                    SHADER_UNIFORM_VEC3, count);
    SetShaderValueV(_fog_shader, _pl_color_loc, colors,
                    SHADER_UNIFORM_VEC3, count);
    SetShaderValueV(_fog_shader, _pl_range_loc, ranges,
                    SHADER_UNIFORM_FLOAT, count);
}

// ── M6-v2c: blob shadow 程序纹理 — 128x128 径向渐变 (中心黑→边透明) ──
// A5-T5-fix: 渐变半径 48px < 半图 64px, 留出透明边距; 消除矩形边界
void HD2DRenderer::_make_blob_shadow_tex() {
    if (_blob_shadow_tex.id > 0) return;
    const int size = 128;
    const float radius = 48.0f;
    Image img = GenImageColor(size, size, Color{0, 0, 0, 0});
    const float cx = (size - 1) * 0.5f, cy = (size - 1) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist >= radius) continue;
            float t = dist / radius;
            float a = cosf(t * 3.14159265f * 0.5f);
            ImageDrawPixel(&img, x, y, Color{0, 0, 0,
                (unsigned char)(a * a * 200.0f)});
        }
    }
    _blob_shadow_tex = LoadTextureFromImage(img);
    UnloadImage(img);
}

// ── A2.1: 软光纹理 — 白色径向渐变 (中心亮→边缘透明), additive 粒子用 ──
void HD2DRenderer::_make_mote_glow_tex() {
    if (_mote_glow_tex.id > 0) return;
    Image img = GenImageGradientRadial(32, 32, 0.0f,
                                       Color{255, 255, 255, 255},
                                       Color{255, 255, 255, 0});
    _mote_glow_tex = LoadTextureFromImage(img);
    UnloadImage(img);
}

// ── 相机: 45° 俯视透视, focus 跟随玩家 (2D 世界像素 → 3D x/z, 高度 → y) ──
void HD2DRenderer::_setup_camera() {
    _camera.position = {0, 640, 320};
    _camera.target = {0, 0, 0};
    _camera.up = {0, 1, 0};
    _camera.fovy = 50.0f;
    _camera.projection = CAMERA_PERSPECTIVE;
    _camera_yaw = 0.0f;
}

void HD2DRenderer::render_frame(GameScene& gs) {
    if (!_ready) return;

    // 1. 只读提取绘制列表 (scene_builder 无 gameplay 副作用)
    _draw_items.clear();
    hd2d::build_scene(gs, _draw_items, _part_color.ready());

    // 2. 相机聚焦玩家世界坐标 (+ M6-v2e: shake 偏移, 帧内消费)
    _camera_focus = {0, 0, 0};
    if (gs.player) {
        _camera_focus.x = gs.player->entity.rect.x;
        _camera_focus.z = gs.player->entity.rect.y;
    }
    _camera_focus.x += _shake_offset.x;
    _camera_focus.z += _shake_offset.z;
    _shake_offset = {0, 0, 0};
    // 3. 相机定位 (v2f: 提前到深度 pass 前 — billboard 深度几何朝向
    // 需当帧相机, 不吃上一帧残值; _draw_scene 内复用不再重算)
    // M6-i.1: 640→440 拉近相机 (可见 ~12x19 tile), 房间/材质占屏更大
    float cam_dist = 440.0f;
    _camera.position = {
        _camera_focus.x,
        _camera_focus.y + cam_dist * 0.7071f,
        _camera_focus.z + cam_dist * 0.7071f
    };
    _camera.target = _camera_focus;
    _update_px_per_world();    // A1.1: outline 世界→屏幕换算 (每帧一次)
    // M6-v2e/v2f: 光空间深度 pass (墙 + billboard 剪影; 失败时主 pass 走 blob 回退)
    // outer_fbo = scene_tree 主 RT (EndTextureMode 盲绑 FBO 0 的同源坑:
    // 深度 pass 后必须恢复主 RT 绑定, 否则主场景画到屏幕 FBO 上丢失)
    {
        auto& shadow = HD2DShadowCaster::inst();
        auto* tree = gs.get_tree();
        if (shadow.ensure_init(_target_w, _target_h) && tree) {
            shadow.update_light_camera(_camera_focus, &_camera);
            shadow.render_depth(gs, _draw_items, tree->main_target().id);
        }
    }
    _draw_scene();
    _apply_post_processing(gs);
}

// ── 场景绘制: 分 kind 绘制 (地形 → 实体 → 特效; 相机已在 render_frame 定位) ──
void HD2DRenderer::_draw_scene() {
    // M6-i.2: {16,13,11}→{40,35,30} — 黑边暖灰化，与监狱地板 [122,112,88] 过渡更自然
    BeginMode3D(_camera);
    ClearBackground({40, 35, 30, 255});

    _draw_terrain_pass();                    // M6-v2c: 雾 shader 包裹地形批
    for (const auto& item : _draw_items)
        if (item.kind == HD2DDrawItem::Kind::FLOOR_DECAL) _draw_floor_decal(item);
    // N5: 楼梯立方 — 地形/贴花之后, 实体之前 (3D 立体块, 深度测试正常)
    for (const auto& item : _draw_items)
        if (item.kind == HD2DDrawItem::Kind::STAIR_STEP) _draw_stair_step(item);
    // M6-v2b: 贴地层 (预警圈/射程环/扇形/危险区) — 地形之上, 实体之下
    for (const auto& item : _draw_items) {
        if (item.kind == HD2DDrawItem::Kind::WARNING_RING) _draw_warning_ring(item);
        else if (item.kind == HD2DDrawItem::Kind::TRAJECTORY_LINE)
            _draw_trajectory_line(item);
        else if (item.kind == HD2DDrawItem::Kind::CONE_FAN)
            _draw_cone_fan(item);
    }
    for (const auto& item : _draw_items)
        if (item.kind == HD2DDrawItem::Kind::ENTITY_LINK) _draw_entity_link(item);
    for (const auto& item : _draw_items)
        if (item.kind == HD2DDrawItem::Kind::PORTAL_RING) _draw_portal_ring(item);
    for (const auto& item : _draw_items)
        if (item.kind == HD2DDrawItem::Kind::ENTITY_BILLBOARD && !hd2d::isGhostPart(item))
            _draw_billboard(item);
    for (const auto& item : _draw_items)
        if (item.kind == HD2DDrawItem::Kind::DOOR_PANEL) _draw_door_panel(item);
    for (const auto& item : _draw_items)
        if (item.kind == HD2DDrawItem::Kind::ROOM_ICON) _draw_room_icon(item);
    for (const auto& item : _draw_items)
        if (item.kind == HD2DDrawItem::Kind::PROJECTILE_BODY) _draw_projectile_body(item);
    for (const auto* ghost : hd2d::orderedGhostParts(_draw_items, _camera))
        _draw_billboard(*ghost);
    for (const auto& item : _draw_items)
        if (item.kind == HD2DDrawItem::Kind::FX_QUAD) _draw_fx_quad(item);
    _draw_ambient_batch();                            // A2.1 (替 v2e 逐颗球)
    EndMode3D();
}

// ── A2.1: billboard 切平面基 (软光纹理中心对称, 手性无关紧要) ──
bool HD2DRenderer::_ambient_billboard_basis(Vector3& right, Vector3& up) const {
    Vector3 fwd = { _camera.target.x - _camera.position.x,
                    _camera.target.y - _camera.position.y,
                    _camera.target.z - _camera.position.z };
    float fl = sqrtf(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
    if (fl < 1e-6f) return false;
    fwd = { fwd.x / fl, fwd.y / fl, fwd.z / fl };
    right = { -fwd.z, 0.0f, fwd.x };                 // cross(fwd, worldUp)
    float rl = sqrtf(right.x * right.x + right.z * right.z);
    if (rl < 1e-6f) return false;                     // 正俯视退化
    right = { right.x / rl, 0.0f, right.z / rl };
    up = { right.y * fwd.z - right.z * fwd.y,
           right.z * fwd.x - right.x * fwd.z,
           right.x * fwd.y - right.y * fwd.x };       // cross(right, fwd)
    return true;
}

// ── A2.1: 氛围粒子单批软光 — 相机朝向 quad + 程序软光纹理, 一次 rlgl 批 ──
// (替 M6-v2e 逐颗 DrawSphere: 24 次 blend 切换/约 1.2 万三角面 → 1 批 24 quad;
//  风格几何/闪烁/透明度包络全在 scene_builder 计算, 本函数只忠实画 quad)
void HD2DRenderer::_draw_ambient_batch() {
    if (_mote_glow_tex.id <= 0) return;
    Vector3 right, up;
    if (!_ambient_billboard_basis(right, up)) return;
    BeginBlendMode(BLEND_ADDITIVE);
    rlSetTexture(_mote_glow_tex.id);
    unsigned int cur_tex = _mote_glow_tex.id;
    rlBegin(RL_QUADS);
    for (const auto& item : _draw_items) {
        if (item.kind != HD2DDrawItem::Kind::AMBIENT_MOTE) continue;
        // A2.2: 群系 PNG 优先 (同帧同群系, 实际只切 1 次); 缺失→程序化软光
        if (item.texture.id > 0 && item.texture.id != cur_tex) {
            cur_tex = item.texture.id;
            rlSetTexture(cur_tex);
        }
        // 软光晕 quad 大于可视核; 像素贴图核即图案, 放大系数收紧
        float r = item.size * (item.texture.id > 0 ? 1.3f : 1.6f);
        Vector3 c = item.world_pos;
        Color t = item.tint;                          // builder 已含包络/闪烁
        rlColor4ub(t.r, t.g, t.b, t.a);
        rlTexCoord2f(0, 0); rlVertex3f(c.x - right.x * r - up.x * r,
                                       c.y - right.y * r - up.y * r,
                                       c.z - right.z * r - up.z * r);
        rlTexCoord2f(1, 0); rlVertex3f(c.x + right.x * r - up.x * r,
                                       c.y + right.y * r - up.y * r,
                                       c.z + right.z * r - up.z * r);
        rlTexCoord2f(1, 1); rlVertex3f(c.x + right.x * r + up.x * r,
                                       c.y + right.y * r + up.y * r,
                                       c.z + right.z * r + up.z * r);
        rlTexCoord2f(0, 1); rlVertex3f(c.x - right.x * r + up.x * r,
                                       c.y - right.y * r + up.y * r,
                                       c.z - right.z * r + up.z * r);
    }
    rlEnd();
    rlSetTexture(0);
    EndBlendMode();
}

// ── M6-v2c: 地形批 — 两遍分区 (lava 一遍 / 其余一遍), 每帧仅 2 次 shader 切换 ──
// 逐 tile 交替切换会触发 rlgl batch flush (火山层可达数百次/帧), 故按
// shader 分组连续绘制; fog 失败回退默认管线 (FOV 压暗 tint 语义保留)
void HD2DRenderer::_draw_terrain_pass() {
    if (!_fog_ok && !_lava_ok) {                 // 双回退: 纯默认管线
        for (const auto& item : _draw_items) {
            if (item.kind == HD2DDrawItem::Kind::FLOOR_TILE)
                _draw_floor_tile(item);
            else if (item.kind == HD2DDrawItem::Kind::WALL_BLOCK)
                _draw_wall_block(item);
        }
        return;
    }
    // Pass 1: 岩浆 tile 单批 (共享 uTime)
    if (_lava_ok) {
        float now = (float)GetTime();
        BeginShaderMode(_lava_shader);
        SetShaderValue(_lava_shader, _lava_time_loc, &now,
                       SHADER_UNIFORM_FLOAT);
        for (const auto& item : _draw_items)
            if (item.kind == HD2DDrawItem::Kind::FLOOR_TILE && item.is_lava)
                _draw_floor_tile(item);
        EndShaderMode();
    }
    // Pass 2: 其余地形 (雾管线或默认)
    if (_fog_ok) {
        _upload_fog_uniforms();
        BeginShaderMode(_fog_shader);
    }
    for (const auto& item : _draw_items) {
        bool is_floor = item.kind == HD2DDrawItem::Kind::FLOOR_TILE;
        bool is_wall = item.kind == HD2DDrawItem::Kind::WALL_BLOCK;
        if ((!is_floor && !is_wall) || (is_floor && item.is_lava && _lava_ok))
            continue;                            // lava 已画 / 非地形跳过
        is_floor ? _draw_floor_tile(item) : _draw_wall_block(item);
    }
    if (_fog_ok) EndShaderMode();
}

// ── 地板: XZ 贴地 quad (v2a: 贴图原语采样 / 纯色 DrawPlane 回退) ──
void HD2DRenderer::_draw_floor_tile(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    if (item.texture.id > 0) {
        // rlGL 原语: 顶点顺序保持与 DrawPlane 一致 (法线朝上),
        // UV 按贴图整帧铺满单格 (tex_src 已归一化到 [0,1])
        Rectangle uv = item.tex_src.width > 0 ? item.tex_src
            : Rectangle{0, 0, (float)item.texture.width, (float)item.texture.height};
        float u0 = uv.x / (float)item.texture.width;
        float u1 = (uv.x + uv.width) / (float)item.texture.width;
        float v0 = uv.y / (float)item.texture.height;
        float v1 = (uv.y + uv.height) / (float)item.texture.height;
        float e = item.size * 0.5f;
        rlSetTexture(item.texture.id);
        rlBegin(RL_QUADS);
        rlColor4ub(item.tint.r, item.tint.g, item.tint.b, item.tint.a);
        rlNormal3f(0, 1, 0);
        rlTexCoord2f(u0, v0); rlVertex3f(pos.x - e, 0.01f, pos.z - e);
        rlTexCoord2f(u1, v0); rlVertex3f(pos.x + e, 0.01f, pos.z - e);
        rlTexCoord2f(u1, v1); rlVertex3f(pos.x + e, 0.01f, pos.z + e);
        rlTexCoord2f(u0, v1); rlVertex3f(pos.x - e, 0.01f, pos.z + e);
        rlEnd();
        rlSetTexture(0);
    } else {
        DrawPlane(pos, {item.size, item.size}, item.tint);
    }
}

// ── 墙体: 拉伸盒子 (v2a: 贴图四侧 + 顶面; 无贴图回退纯色伪光照) ──
void HD2DRenderer::_draw_wall_block(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float h = item.height;
    float e = item.size * 0.5f;
    // A3.2-fix2: 顶面色 = builder 的 wall_top 色调映射 (a=0 → 旧规 tint×1.1)
    Color top_col = item.top_tint.a > 0
        ? item.top_tint
        : Color{(unsigned char)std::min(item.tint.r * 1.1f, 255.0f),
                (unsigned char)std::min(item.tint.g * 1.1f, 255.0f),
                (unsigned char)std::min(item.tint.b * 1.1f, 255.0f),
                item.tint.a};

    if (item.texture.id > 0) {
        Rectangle uv = item.tex_src.width > 0 ? item.tex_src
            : Rectangle{0, 0, (float)item.texture.width, (float)item.texture.height};
        float u0 = uv.x / (float)item.texture.width;
        float u1 = (uv.x + uv.width) / (float)item.texture.width;
        float v0 = uv.y / (float)item.texture.height;
        float v1 = (uv.y + uv.height) / (float)item.texture.height;
        rlSetTexture(item.texture.id);
        rlDisableBackfaceCulling();  // A3.2-fix: 顶点在顶面的 quad 不被剔除
        rlBegin(RL_QUADS);
        rlColor4ub(item.tint.r, item.tint.g, item.tint.b, item.tint.a);
        _wall_quad(u0, u1, v0, v1, pos, e, h);   // 侧面 ×4 (共享 UV)
        _wall_top_quad(u0, u1, v0, v1, pos, e, h, top_col);  // 顶面
        rlEnd();
        rlSetTexture(0);
        return;
    }

    Color side = {
        (unsigned char)(item.tint.r * 0.7f),
        (unsigned char)(item.tint.g * 0.7f),
        (unsigned char)(item.tint.b * 0.7f), item.tint.a
    };
    DrawCube({pos.x, h * 0.5f, pos.z}, item.size, h, item.size, side);
    DrawCube({pos.x, h, pos.z}, item.size, 1.0f, item.size, top_col);
}

// ── 墙体侧面 quad ×4 (rlBegin 内调用; 顶点序 = 从外侧看逆时针) ──
void HD2DRenderer::_wall_quad(float u0, float u1, float v0, float v1,
                              Vector3 pos, float e, float h) {
    // 北面 (+z 朝向相机): 屏幕正面
    rlNormal3f(0, 0, 1);
    rlTexCoord2f(u0, v1); rlVertex3f(pos.x - e, 0, pos.z + e);
    rlTexCoord2f(u1, v1); rlVertex3f(pos.x + e, 0, pos.z + e);
    rlTexCoord2f(u1, v0); rlVertex3f(pos.x + e, h, pos.z + e);
    rlTexCoord2f(u0, v0); rlVertex3f(pos.x - e, h, pos.z + e);
    // 南面
    rlNormal3f(0, 0, -1);
    rlTexCoord2f(u1, v1); rlVertex3f(pos.x - e, 0, pos.z - e);
    rlTexCoord2f(u0, v1); rlVertex3f(pos.x + e, 0, pos.z - e);
    rlTexCoord2f(u0, v0); rlVertex3f(pos.x + e, h, pos.z - e);
    rlTexCoord2f(u1, v0); rlVertex3f(pos.x - e, h, pos.z - e);
    // 东面
    rlNormal3f(1, 0, 0);
    rlTexCoord2f(u0, v1); rlVertex3f(pos.x + e, 0, pos.z - e);
    rlTexCoord2f(u1, v1); rlVertex3f(pos.x + e, 0, pos.z + e);
    rlTexCoord2f(u1, v0); rlVertex3f(pos.x + e, h, pos.z + e);
    rlTexCoord2f(u0, v0); rlVertex3f(pos.x + e, h, pos.z - e);
    // 西面
    rlNormal3f(-1, 0, 0);
    rlTexCoord2f(u1, v1); rlVertex3f(pos.x - e, 0, pos.z - e);
    rlTexCoord2f(u0, v1); rlVertex3f(pos.x - e, 0, pos.z + e);
    rlTexCoord2f(u0, v0); rlVertex3f(pos.x - e, h, pos.z + e);
    rlTexCoord2f(u1, v0); rlVertex3f(pos.x - e, h, pos.z - e);
}

// ── A3.2: 墙顶面 — 同贴图水平 quad (与 caster 深度顶面 y=h 共面; 顶点序同地板) ──
// A3.2-fix2: 色由调用方决定 (builder 传 wall_top 色调映射), 此处纯发射
void HD2DRenderer::_wall_top_quad(float u0, float u1, float v0, float v1,
                                  Vector3 pos, float e, float h, Color top) {
    rlColor4ub(top.r, top.g, top.b, top.a);
    rlNormal3f(0, 1, 0);
    rlTexCoord2f(u0, v0); rlVertex3f(pos.x - e, h, pos.z - e);
    rlTexCoord2f(u1, v0); rlVertex3f(pos.x + e, h, pos.z - e);
    rlTexCoord2f(u1, v1); rlVertex3f(pos.x + e, h, pos.z + e);
    rlTexCoord2f(u0, v1); rlVertex3f(pos.x - e, h, pos.z + e);
}

// ── Billboard: 面向相机的精灵, 脚点落地, 帧矩形裁剪 (v2a: flip_x 接线) ──
void HD2DRenderer::_draw_billboard(const HD2DDrawItem& item) {
    if (item.pro_mode) {
        _part_color.draw(item, _camera);
        if (item.blob_width > 0) _draw_blob_shadow(item.world_pos, item.blob_width);
        return;
    }
    Vector3 pos = item.world_pos;
    float w = item.size * item.scale_w;
    float h = item.size * 1.5f * item.scale_h;
    if (item.texture.id <= 0) {
        DrawCube({pos.x, h * 0.5f, pos.z}, w * 0.5f, h, w * 0.25f, item.tint);
        _draw_blob_shadow(pos, w);
        return;
    }
    Rectangle src = item.tex_src.width > 0 ? item.tex_src
        : Rectangle{0, 0, (float)item.texture.width, (float)item.texture.height};
    // flip_x: 负宽源矩形 (raylib DrawTexturePro 惯例; billboard 同理取负 w)
    if (item.flip_x) src.width = -src.width;
    // A1.1: 3D-aware alpha-mask 真轮廓 (单 draw; 描边不进 shadow map)
    if (item.outline && _outline_ok) {
        _draw_billboard_outline(item, src, pos, w, h);
        _draw_blob_shadow(pos, w);
        return;
    }
    // M6-n 回退: 4 向偏移深色底稿 (仅 outline shader 加载失败时)
    if (item.outline) _draw_outline_fallback(item, src, pos, w, h);
    DrawBillboardRec(_camera, item.texture, src,
                     {pos.x, h * 0.5f, pos.z}, {w, h}, item.tint);
    _draw_blob_shadow(pos, w);     // M6-v2c: 径向渐变接地阴影
}

// ── M6-n 回退: 4 向偏移深色底稿 (仅 outline shader 加载失败时) ──
void HD2DRenderer::_draw_outline_fallback(const HD2DDrawItem& item,
                                          const Rectangle& src, Vector3 pos, float w, float h) {
    Color edge{24, 24, 27, 220};
    float off = item.size * 0.02f;
    for (int i = 0; i < 4; i++) {
        Vector3 shift = pos;
        if (i == 0) shift.x -= off;
        else if (i == 1) shift.x += off;
        else if (i == 2) shift.z -= off;
        else shift.z += off;
        DrawBillboardRec(_camera, item.texture, src,
                         {shift.x, h * 0.5f, shift.z}, {w, h}, edge);
    }
}

// ── A1.1: 3D-aware 描边路径 — 世界基准宽 → 屏幕 px clamp[1,4] → UV 偏移,
// 轮廓贴合精灵剪影 (8 邻域 alpha-mask 在 shader 内), 单 DrawBillboardRec ──
void HD2DRenderer::_draw_billboard_outline(const HD2DDrawItem& item,
                                           const Rectangle& src,
                                           Vector3 pos, float w, float h) {
    float px = _px_per_world;
    // 世界基准宽 → 屏幕像素 (clamp 保证远距仍可辨 / 近距 Boss 不过粗)
    float screen_w = fminf(kOutlineMaxScreenPx,
                           fmaxf(kOutlineMinScreenPx, kOutlineBaseWorldW * px));
    float eff_world_w = screen_w / px;      // 反推有效世界宽
    float frame_w = fabsf(src.width);       // 帧矩形宽 (图集 px; flip_x 为负取绝对)
    float tex_w = item.texture.width > 0 ? (float)item.texture.width : 1.0f;
    // world → UV: 帧矩形占图集 UV 宽 / billboard 世界宽
    float uv_off = (frame_w > 0.0f && w > 0.0f)
        ? eff_world_w * frame_w / (tex_w * w)
        : eff_world_w / tex_w;
    Vector4 outline_color = {24.0f / 255.0f, 24.0f / 255.0f, 27.0f / 255.0f,
                             220.0f / 255.0f};   // 与旧回退同色 (中性深色)
    BeginShaderMode(_outline_shader);
    _upload_shadow_to(_outline_shader, _out_shadow_map_loc, _out_shadow_mvp_loc,
                      _out_shadow_on_loc, _out_shadow_texel_loc,
                      _out_shadow_bias_loc);             // A3: 与地形同源阴影
    SetShaderValue(_outline_shader, _outline_off_loc, &uv_off,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(_outline_shader, _outline_color_loc, &outline_color,
                   SHADER_UNIFORM_VEC4);
    DrawBillboardRec(_camera, item.texture, src,
                     {pos.x, h * 0.5f, pos.z}, {w, h}, item.tint);
    EndShaderMode();
}

// ── M6-v2c: blob shadow — 径向渐变纹理贴地 quad (失败回退黑扁片) ──
// v2f: 实体剪影已进深度 pass; blob 只补接地感 (alpha 再降)
void HD2DRenderer::_draw_blob_shadow(Vector3 pos, float w) {
    if (_blob_shadow_tex.id <= 0) {
        DrawCube({pos.x, 0.05f, pos.z}, w * 0.55f, 0.08f, w * 0.35f,
                 {0, 0, 0, 100});
        return;
    }
    float half_x = w * 0.31f, half_z = w * 0.21f;   // 椭圆约 0.62w x 0.42w
    auto& shadow = HD2DShadowCaster::inst();
    bool entity_cast = shadow.entity_shadow_ready();   // v2f: 剪影生效?
    unsigned char alpha = entity_cast ? 40
                        : shadow.is_ready() ? 60      // v2e: 仅墙投影
                        : 120;                          // 全回退: blob 主角
    rlSetTexture(_blob_shadow_tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, alpha);
    rlNormal3f(0, 1, 0);
    rlTexCoord2f(0, 0); rlVertex3f(pos.x - half_x, 0.06f, pos.z - half_z);
    rlTexCoord2f(1, 0); rlVertex3f(pos.x + half_x, 0.06f, pos.z - half_z);
    rlTexCoord2f(1, 1); rlVertex3f(pos.x + half_x, 0.06f, pos.z + half_z);
    rlTexCoord2f(0, 1); rlVertex3f(pos.x - half_x, 0.06f, pos.z + half_z);
    rlEnd();
    rlSetTexture(0);
}

// ── 特效: 发光脉冲片 (轻量; v2 换 additive shader) ──
void HD2DRenderer::_draw_fx_quad(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float pulse = 0.8f + 0.2f * sinf((float)GetTime() * 6.0f + pos.x * 0.1f);
    float s = item.size * pulse;
    DrawCube({pos.x, 0.15f, pos.z}, s, 0.06f, s * 0.6f, item.tint);
}

// ── M6-v2a: 挑战传送门 — 竖立脉冲双环 (2D 双层圆的 3D 对应物) ──
// 入口=蓝 (80,180,255) / 返回=绿 (100,255,150); 与 2D draw_challenge_portal 同色
// 环面绕 Y 转 45° 朝向相机 (相机方向 X+Z 45°, 竖立不倒)
void HD2DRenderer::_draw_portal_ring(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float pulse = 0.8f + 0.2f * sinf((float)GetTime() * 3.0f);
    float radius = item.height * pulse;
    Color outer = item.portal_entry
        ? Color{80, 180, 255, (unsigned char)(200 * pulse)}
        : Color{100, 255, 150, (unsigned char)(200 * pulse)};
    Color inner = item.portal_entry
        ? Color{150, 220, 255, (unsigned char)(220 * pulse)}
        : Color{180, 255, 200, (unsigned char)(220 * pulse)};
    // 竖立圆环: 面朝相机 (默认法线 +Z, 绕 Y 转 45° 对准 X+Z 视线)
    DrawCircle3D({pos.x, radius * 0.9f, pos.z}, radius,
                 {0, 1, 0}, 45.0f, outer);
    DrawCircle3D({pos.x, radius * 0.9f, pos.z}, radius * 0.62f,
                 {0, 1, 0}, 45.0f, inner);
    // 地面基准圈 (定位感, 防悬空) — 贴地平躺
    DrawCircle3D({pos.x, 0.1f, pos.z}, radius * 0.5f, {1, 0, 0}, 90.0f,
                  Color{outer.r, outer.g, outer.b, 120});
}

// ── M6-j: 地板装饰 — 贴地 decal quad (裂缝/苔藓/符文; 32px 平铺) ──
// N1-fix: 关闭深度测试 — decal 从45°角看比地板"更远"会被深度缓冲丢弃
void HD2DRenderer::_draw_floor_decal(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float e = item.size * 0.5f;
    rlDisableDepthTest();
    rlDrawRenderBatchActive();                 // 强制刷新确保深度状态生效
    // M6-l: 支持无纹理纯色 quad (Boss FOV 红雾) + 使用 item.tint
    if (item.texture.id > 0) {
        rlSetTexture(item.texture.id);
        rlBegin(RL_QUADS);
        rlColor4ub(item.tint.r, item.tint.g, item.tint.b, item.tint.a);
        rlNormal3f(0, 1, 0);
        rlTexCoord2f(0, 0); rlVertex3f(pos.x - e, pos.y, pos.z - e);
        rlTexCoord2f(1, 0); rlVertex3f(pos.x + e, pos.y, pos.z - e);
        rlTexCoord2f(1, 1); rlVertex3f(pos.x + e, pos.y, pos.z + e);
        rlTexCoord2f(0, 1); rlVertex3f(pos.x - e, pos.y, pos.z + e);
        rlEnd();
        rlSetTexture(0);
    } else {
        DrawPlane({pos.x, pos.y, pos.z}, {item.size, item.size},
                  {item.tint.r, item.tint.g, item.tint.b, item.tint.a});
    }
    rlEnableDepthTest();
    rlDrawRenderBatchActive();                 // 恢复深度状态
}

// ── N5: 楼梯台阶 — 真实 3D 立方块 (DrawCube 自带面光照) ──
// 4 个立方由 builder 逐级下沉/缩小, 视觉上形成阶梯
void HD2DRenderer::_draw_stair_step(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float depth = item.height;                  // z 深度复用 height 字段
    Color face = item.tint;
    DrawCube(pos, item.size, 2.5f, depth, face);
}

// ── M6-v2h: 门 — 竖立贴图面板 (四态纹理; 锁=红罩 / 封=紫脉冲十字) ──
// M6-n: billboard → wall-aligned (door_axis 探测两侧墙走向; 面板贴墙不飘)
void HD2DRenderer::_draw_door_panel(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float h = item.height;
    float half = item.size * 0.5f;
    if (item.texture.id > 0 && item.door_axis == 0) {
        // 贴东西走向墙: 面板沿 X 展开, 法线 ±Z (面朝相机侧)
        float facing = (_camera.position.z >= pos.z) ? 1.0f : -1.0f;
        rlSetTexture(item.texture.id);
        rlBegin(RL_QUADS);
        rlColor4ub(item.tint.r, item.tint.g, item.tint.b, item.tint.a);
        rlNormal3f(0, 0, facing);
        rlTexCoord2f(0, 1); rlVertex3f(pos.x - half, 0, pos.z);
        rlTexCoord2f(1, 1); rlVertex3f(pos.x + half, 0, pos.z);
        rlTexCoord2f(1, 0); rlVertex3f(pos.x + half, h, pos.z);
        rlTexCoord2f(0, 0); rlVertex3f(pos.x - half, h, pos.z);
        rlEnd();
        rlSetTexture(0);
    } else if (item.texture.id > 0) {
        // 贴南北走向墙: 面板沿 Z 展开, 法线 ±X
        float facing = (_camera.position.x >= pos.x) ? 1.0f : -1.0f;
        rlSetTexture(item.texture.id);
        rlBegin(RL_QUADS);
        rlColor4ub(item.tint.r, item.tint.g, item.tint.b, item.tint.a);
        rlNormal3f(facing, 0, 0);
        rlTexCoord2f(0, 1); rlVertex3f(pos.x, 0, pos.z - half);
        rlTexCoord2f(1, 1); rlVertex3f(pos.x, 0, pos.z + half);
        rlTexCoord2f(1, 0); rlVertex3f(pos.x, h, pos.z + half);
        rlTexCoord2f(0, 0); rlVertex3f(pos.x, h, pos.z - half);
        rlEnd();
        rlSetTexture(0);
    } else {
        // 无贴图回退: 棕色立板 (2D DOOR 色 130,90,50), 按 axis 取向
        float depth = 6.0f, width = item.size * 0.9f;
        if (item.door_axis == 1) { float t = depth; depth = width; width = t; }
        DrawCube({pos.x, item.height * 0.5f, pos.z}, width,
                 item.height, depth, Color{130, 90, 50, 255});
    }
    // 门楣横梁 (提高辨识; 按 axis 取向)
    DrawCube({pos.x, h + 1.0f, pos.z},
             item.door_axis == 0 ? item.size + 2.0f : 1.5f, 2.0f,
             item.door_axis == 0 ? 1.5f : item.size + 2.0f,
             Color{70, 45, 25, 255});
    // LOCKED: 红色半罩 + 小锁徽记 (2D DoorRenderer overlay 语义; 按 axis 取向)
    if ((DoorState)item.door_state == DoorState::LOCKED) {
        float w = item.door_axis == 0 ? item.size * 0.92f : 2.5f;
        float d = item.door_axis == 0 ? 2.5f : item.size * 0.92f;
        DrawCube({pos.x, item.height * 0.5f, pos.z}, w,
                 item.height * 0.98f, d, Color{180, 40, 40, 90});
        _draw_lock_badge(pos, item.height);
    }
    // SEALED: 紫脉冲十字 (贴门面; 按 axis 取向)
    if ((DoorState)item.door_state == DoorState::SEALED) {
        float pulse = 0.7f + 0.3f * sinf((float)GetTime() * 3.0f);
        Color seal = {(unsigned char)(160 * pulse), 50,
                      (unsigned char)(220 * pulse), 200};
        Vector3 c = {pos.x, item.height * 0.55f, pos.z};
        if (item.door_axis == 0) {
            DrawCube(c, 2.0f, 12.0f, 2.0f, seal);
            DrawCube(c, 12.0f, 2.0f, 2.0f, seal);
        } else {
            DrawCube(c, 2.0f, 12.0f, 2.0f, seal);
            DrawCube(c, 2.0f, 2.0f, 12.0f, seal);
        }
    }
}

// ── M6-v2h: 锁徽记 — 门板中央小锁 (2D _draw_lock_icon 3D 对应) ──
void HD2DRenderer::_draw_lock_badge(Vector3 pos, float door_h) {
    Vector3 c = {pos.x, door_h * 0.55f, pos.z};
    DrawCube(c, 8.0f, 6.0f, 3.0f, Color{200, 50, 50, 220});   // 锁体
    DrawCube({c.x, c.y + 5.0f, c.z}, 6.0f, 4.0f, 3.0f,
             Color{230, 190, 60, 220});                        // 锁环
}

// ── M6-v2h: 特殊房间图标 — 贴地菱形底 + 上方 billboard 图标 ──
// (2D 中心 75% 尺寸图标 + 菱形嵌纹的 3D 对应; 未触发才画)
void HD2DRenderer::_draw_room_icon(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    // 贴地菱形底板 (房间主色提亮; 半透明)
    Color base = item.tint;
    base.a = 120;
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(base.r, base.g, base.b, base.a);
    float m = item.size * 0.6f;
    rlVertex3f(pos.x, 0.12f, pos.z - m); rlVertex3f(pos.x + m, 0.12f, pos.z);
    rlVertex3f(pos.x, 0.12f, pos.z + m);
    rlVertex3f(pos.x, 0.12f, pos.z - m); rlVertex3f(pos.x, 0.12f, pos.z + m);
    rlVertex3f(pos.x - m, 0.12f, pos.z);
    rlEnd();
    // 图标本体: 略浮空 billboard (与地面物品同款式样)
    if (item.texture.id > 0) {
        Rectangle src = item.tex_src.width > 0 ? item.tex_src
            : Rectangle{0, 0, (float)item.texture.width,
                        (float)item.texture.height};
        DrawBillboardRec(_camera, item.texture, src,
                         {pos.x, item.size * 0.55f, pos.z},
                         {item.size, item.size}, WHITE);
    }
}

// ── M6-v2b: 投射物弹体 — 发光球 + 拖尾 (2D 三态配色同源) ──
void HD2DRenderer::_draw_projectile_body(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float r = item.size * 0.5f;
    // 元素/归属配色: 穿透=金 / 敌方元素(火红/冰蓝/暗红) / 玩家=土金
    Color core, glow;
    if (item.piercing) {
        core = {255, 255, 220, 255}; glow = {255, 180, 40, 100};
    } else if (item.element == 1) {
        core = {255, 200, 150, 255}; glow = {255, 80, 30, 120};
    } else if (item.element == 2) {
        core = {220, 240, 255, 255}; glow = {80, 180, 255, 120};
    } else {
        core = {255, 255, 220, 180};
        glow = item.tint;                          // 归属色 (敌红/玩家土)
        glow.a = 150;
    }
    _draw_projectile_trail(item, glow);            // M6-v2d: 反速度渐隐拖尾
    DrawSphere(pos, r * 2.0f, glow);               // 外晕
    DrawSphere(pos, r, core);                      // 芯
}

// ── M6-v2d: 投射物拖尾 — 反速度 3 段渐隐 (2D back 线 0.03s 的 3D 加强版) ──
// 基准对齐 2D 穿透弹 DrawLineEx back 线; additive 混合加亮, 纯视觉无状态
void HD2DRenderer::_draw_projectile_trail(const HD2DDrawItem& item, Color c) {
    if (item.trail_dir.x == 0.0f && item.trail_dir.y == 0.0f) return;
    Vector3 pos = item.world_pos;
    BeginBlendMode(BLEND_ADDITIVE);
    for (int seg = 0; seg < 3; seg++) {
        float t0 = 0.03f * (seg + 1);              // 0.03/0.06/0.09s 反向
        Vector3 back = {pos.x - item.trail_dir.x * t0, pos.y,
                        pos.z - item.trail_dir.y * t0};
        Color tc = {c.r, c.g, c.b,
                    (unsigned char)(c.a * (3 - seg) / 6)};  // 递减 50/33/17%
        rlSetTexture(0);
        rlBegin(RL_LINES);
        rlColor4ub(tc.r, tc.g, tc.b, tc.a);
        rlVertex3f(pos.x, pos.y, pos.z);
        rlVertex3f(back.x, back.y, back.z);
        rlEnd();
        pos = back;                                // 段段相接
    }
    EndBlendMode();
}

// ── M6-v2b: rl 原语空心圆环 (贴地) — DrawCircle3D 是实心面, 环必须手画 ──
static void _draw_flat_ring(Vector3 center, float radius, float y,
                            Color c, int segments = 48) {
    rlSetTexture(0);
    rlBegin(RL_LINES);
    rlColor4ub(c.r, c.g, c.b, c.a);
    float step = 360.0f / segments;
    for (int i = 0; i < segments; i++) {
        float a0 = DEG2RAD * (step * i);
        float a1 = DEG2RAD * (step * (i + 1));
        rlVertex3f(center.x + cosf(a0) * radius, y, center.z + sinf(a0) * radius);
        rlVertex3f(center.x + cosf(a1) * radius, y, center.z + sinf(a1) * radius);
    }
    rlEnd();
}

// ── M6-v2b: 贴地预警环 — AOE 危险圈 / 射程指示 (element<0 单环, >=0 双环带) ──
void HD2DRenderer::_draw_warning_ring(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float fade = item.height;                      // builder 存的 fade (0~1)
    float pulse = 1.0f + sinf((float)GetTime() * 12.0f) * 0.12f;
    Color c = item.tint;
    c.a = (unsigned char)(c.a * fade);

    if (item.element >= 0.0f) {
        // 双环带 (NUNCHAKU 射程): element=内半径, size=外半径 — 带填充+双环线
        float inner = item.element, outer = item.size;
        DrawCircle3D({pos.x, 0.10f, pos.z}, (inner + outer) * 0.5f,
                     {1, 0, 0}, 90.0f,
                     Color{c.r, c.g, c.b, (unsigned char)(c.a * 3 / 10)});
        _draw_flat_ring(pos, inner, 0.14f, c);
        _draw_flat_ring(pos, outer, 0.14f, c);
        return;
    }
    // 单环: AOE 危险圈 (脉冲) / SPEAR+CROSSBOW 射程环
    float radius = item.size * pulse;
    _draw_flat_ring(pos, radius, 0.14f, c);
    _draw_flat_ring(pos, radius - 3.0f, 0.15f,
                    Color{c.r, c.g, c.b, (unsigned char)(c.a * 2 / 3)});
    // 中心淡晕 (危险区感, 实心薄圆)
    DrawCircle3D({pos.x, 0.11f, pos.z}, radius * 0.8f, {1, 0, 0}, 90.0f,
                 Color{c.r, c.g, c.b, (unsigned char)(c.a * 1 / 4)});
}

// ── M6-v2b: 投射物轨迹预警线 (WARNING 相点弹; 2D _preview 同源) ──
void HD2DRenderer::_draw_trajectory_line(const HD2DDrawItem& item) {
    Vector3 from = item.world_pos;
    Vector3 to = item.end_pos;
    float fade = item.height;                      // builder 存的 fade
    float pulse = 1.0f + sinf((float)GetTime() * 12.0f) * 0.12f;
    Color c = item.tint;
    c.a = (unsigned char)(c.a * fade);
    // 主线 (宽 = 用双线近似 2D DrawLineEx 的厚度)
    rlSetTexture(0);
    rlBegin(RL_LINES);
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlVertex3f(from.x, from.y, from.z);
    rlVertex3f(to.x, to.y, to.z);
    // 平行偏移线 (粗线视觉)
    rlVertex3f(from.x + 1.5f * pulse, from.y, from.z);
    rlVertex3f(to.x + 1.5f * pulse, to.y, to.z);
    rlEnd();
    // 落点脉冲圈 (2D DrawCircle 同源)
    _draw_flat_ring(to, 4.0f * pulse, to.y, c, 16);
}

// ── M6-v2b: Boss 扇形预警 — 贴地三角扇 (rlGL 原语, 半透明警示面) ──
void HD2DRenderer::_draw_cone_fan(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float reach = item.size;
    float half = DEG2RAD * item.fan_half_deg;
    float pulse = 0.85f + 0.15f * sinf((float)GetTime() * 10.0f);
    Color c = item.tint;
    c.a = (unsigned char)(c.a * pulse);
    const int SEG = 12;                       // 扇形细分 (2D 同为 12 段)
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(c.r, c.g, c.b, c.a);
    for (int i = 0; i < SEG; i++) {
        float a0 = item.fan_angle - half + (2 * half) * i / SEG;
        float a1 = item.fan_angle - half + (2 * half) * (i + 1) / SEG;
        rlVertex3f(pos.x, 0.16f, pos.z);
        rlVertex3f(pos.x + cosf(a0) * reach, 0.16f, pos.z + sinf(a0) * reach);
        rlVertex3f(pos.x + cosf(a1) * reach, 0.16f, pos.z + sinf(a1) * reach);
    }
    rlEnd();
    // 边缘线 (两条半径 + 外弧)
    Color lc = c; lc.a = (unsigned char)(lc.a * 0.8f);
    rlBegin(RL_LINES);
    rlColor4ub(lc.r, lc.g, lc.b, lc.a);
    rlVertex3f(pos.x, 0.17f, pos.z);
    rlVertex3f(pos.x + cosf(item.fan_angle - half) * reach, 0.17f,
               pos.z + sinf(item.fan_angle - half) * reach);
    rlVertex3f(pos.x, 0.17f, pos.z);
    rlVertex3f(pos.x + cosf(item.fan_angle + half) * reach, 0.17f,
               pos.z + sinf(item.fan_angle + half) * reach);
    for (int i = 0; i < SEG; i++) {
        float a0 = item.fan_angle - half + (2 * half) * i / SEG;
        float a1 = item.fan_angle - half + (2 * half) * (i + 1) / SEG;
        rlVertex3f(pos.x + cosf(a0) * reach, 0.17f, pos.z + sinf(a0) * reach);
        rlVertex3f(pos.x + cosf(a1) * reach, 0.17f, pos.z + sinf(a1) * reach);
    }
    rlEnd();
}

// ── M6-v2b: 实体连线 — Tank守护/关系线 (两点 rl 线, 2D DrawLineEx 同源色) ──
void HD2DRenderer::_draw_entity_link(const HD2DDrawItem& item) {
    rlSetTexture(0);
    rlBegin(RL_LINES);
    rlColor4ub(item.tint.r, item.tint.g, item.tint.b, item.tint.a);
    rlVertex3f(item.world_pos.x, item.world_pos.y, item.world_pos.z);
    rlVertex3f(item.end_pos.x, item.end_pos.y, item.end_pos.z);
    rlEnd();
}

// ── M6-v2a: 世界→屏幕投影 (GetWorldToScreen 封装; 相机就绪后有效) ──
Vector2 HD2DRenderer::world_to_screen(Vector3 world_pos, float y_offset) const {
    if (!_ready) return {-1, -1};
    Vector2 s = GetWorldToScreen(
        {world_pos.x, world_pos.y + y_offset, world_pos.z}, _camera);
    return s;
}

// ── M6-v2g: bloom 三档手调 preset (监狱/深渊/火山) ──
// v2g: 环境光 0.78 抬亮地板后，低阈值会提取中亮地板 → bloom 炸全白
// (实测 238); 各档阈值只吃高光
void HD2DRenderer::_apply_bloom_biome_preset(const GameMap* map) {
    const char* biome = map ? map->biome_id() : "";
    float threshold = 0.72f, softness = 0.22f, intensity = 0.12f;
    if (strcmp(biome, "ash_volcano") == 0)
        threshold = 0.82f, softness = 0.14f, intensity = 0.15f;  // 火山
    else if (strcmp(biome, "void_abyss") == 0)
        threshold = 0.74f, softness = 0.20f, intensity = 0.18f;  // 深渊
    HD2DPostFX::inst().set_params(threshold, softness, intensity);
}

// ── M6-v2c: 后处理 — bloom 链 + 夜色分级 + 地平雾带 (shader 版) ──
// bloom: 场景 RT → 亮部提取/模糊 (PostFX 内部嵌套 RT, 已自恢复 FBO);
//   之后 additive 叠加回本层 (当前绘制目标 = scene_tree 主 RT)
// 夜色/雾带: 2D 叠加保留 (与 bloom 不冲突; 雾带在 bloom 之下画)
void HD2DRenderer::_apply_post_processing(GameScene& gs) {
    // 1. bloom 链 (需要场景已画完; 当前在 scene_tree 主 RT 绘制流内)
    auto& fx = HD2DPostFX::inst();
    auto* tree = gs.get_tree();
    if (tree && fx.ensure_init(_target_w, _target_h)) {
        _apply_bloom_biome_preset(gs.game_map.get());   // M6-v2g 手调三档
        fx.process(tree->main_target());
        // 夜色分级 + 地平雾带 (bloom 之下)
        // i.1-fix2: 夜色从蓝 (20,18,46) 改暖暗 (30,24,18) — 蓝罩把全屏
        // 群系材质拉冷 (实测暖 32%→29%, 冷 0.6%→51%); 保留分级不染色相
        DrawRectangle(0, 0, _target_w, _target_h, {30, 24, 18, 14});
        for (int i = 0; i < 6; i++) {
            int alpha = 50 - i * 8;
            if (alpha <= 0) break;
            DrawRectangle(0, _target_h - (6 - i) * 24, _target_w, 24,
                          {22, 18, 16, (unsigned char)alpha});
        }
        fx.draw_overlay();          // additive bloom 叠加 (最上层)
    }
}
