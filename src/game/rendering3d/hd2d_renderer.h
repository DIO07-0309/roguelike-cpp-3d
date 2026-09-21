#pragma once
#include "raylib.h"
#include "hd2d_part_geometry.h"
#include <vector>
#include <memory>

class GameScene;
class GameMap;
struct HD2DDrawItem;

namespace Game {
class WeatherSystem;
}

// ============================================================
// M6-HD2D: HD-2D 渲染器 — 2D 逻辑层不动, 3D 表现层切片
// 设计约束 (P1-C8 教训, 必须遵守):
//   1. 本模块只读 GameScene 状态, 绝不写 gameplay 状态
//   2. 视觉随机只吃 visual_rng (RNG-001/002 红线)
//   3. sim 无头模式不初始化 3D (main.cpp 已保证)
//   4. 切换开关 g_hd2d_mode: true 走本渲染器, false 走原 2D 路径
// ============================================================

// A2.1: 氛围粒子群系性格 (纯渲染语义, 不进 gameplay)
enum class MoteStyle : int { DUST = 0, EMBER = 1, FIREFLY = 2 };

// 一帧的 3D 绘制项 (由 HD2DSceneBuilder 从 GameScene 状态提取)
struct HD2DDrawItem {
    enum class Kind { FLOOR_TILE, WALL_BLOCK, ENTITY_BILLBOARD, FX_QUAD,
                      PORTAL_RING,                    // M6-v2a: 挑战传送门竖立光环
                      PROJECTILE_BODY, WARNING_RING, TRAJECTORY_LINE,  // M6-v2b
                      CONE_FAN, ENTITY_LINK,          // M6-v2b: 扇形/实体连线
                      AMBIENT_MOTE,                   // M6-v2e: 氛围粒子微光点
                      DOOR_PANEL, ROOM_ICON,         // M6-v2h: 门/特殊房间图标
                      FLOOR_DECAL,                   // M6-j: 地板装饰贴片
                      STAIR_STEP,                    // N5: 楼梯 3D 立方
                      FX_PARTICLE,                   // A10: 3D 粒子
                      FX_RING_3D,                    // A10: 3D 光环
                      FX_BEAM_3D,                    // A10: 3D 光束
                      FX_EXPLOSION_3D };            // A10: 3D 爆炸
    Kind kind = Kind::FLOOR_TILE;
    int tile_x = 0;                 // 世界 tile 坐标 (32px/格)
    int tile_y = 0;
    Vector3 world_pos = {0, 0, 0};  // 3D 位置 (世界坐标直转, y=高度)
    float size = 32.0f;            // billboard 宽 / tile 边长 / ring 半径
    float height = 32.0f;          // 墙高 / 门环半径 / line 终点偏移
    Color tint = WHITE;
    Color top_tint = {};           // A3.2-fix2: 顶面色调映射 (a=0 → 渲染器回退 tint×1.1)
    float scale_w = 1.0f, scale_h = 1.0f;  // B3: billboard 形变 (1,1 = 原行为)
    Texture2D texture = {};        // billboard/地板贴图 (0 = 纯色)
    Rectangle tex_src = {};        // 贴图源矩形 (帧动画)
    bool flip_x = false;
    float sort_y = 0.0f;           // billboard 深度排序键 (世界 y)
    bool portal_entry = true;      // PORTAL_RING: 入口(蓝)/返回(绿)
    // ── M6-v2b: 投射物/预警/轨迹 附加数据 ──
    Vector3 end_pos = {0, 0, 0};   // TRAJECTORY_LINE 终点
    bool piercing = false;         // PROJECTILE_BODY: 穿透弹(金光/拖尾加长)
    int element = 0;               // PROJECTILE_BODY: 元素色 (0无/1火/2冰)
    // ── M6-v2b: Boss 扇形预警 (CONE_FAN) ──
    float fan_angle = 0.0f;        // 朝向 (弧度; x/z 平面)
    float fan_half_deg = 45.0f;    // 半角 (度)
    bool is_lava = false;          // M6-v2c: FLOOR_TILE 为 LAVA → 岩浆 shader
    Vector2 trail_dir = {0, 0};   // M6-v2d: PROJECTILE_BODY 速度向量 px/s (拖尾)
    int door_state = 0;           // M6-v2h: DOOR_PANEL 四态 (DoorState 枚举值)
    int door_axis = 0;            // M6-n: 门朝向 0=贴东西墙(面板朝±Z) 1=贴南北墙(朝±X)
    bool outline = false;         // M6-n: 实体描边 (玩家/怪; 4向偏移深色底)
    bool pro_mode = false;
    float rot_deg = 0.0f;
    Vector2 pivot_uv_px = {0, 0};
    Vector2 part_offset = {0, 0};
    float blob_width = 0.0f;
    MoteStyle mote_style = MoteStyle::DUST;  // A2.1: AMBIENT_MOTE 群系性格
};

// 单房间切片: 960x640 目标 → 3D 透视相机 + 地形 + billboard
class HD2DRenderer {
public:
    static HD2DRenderer& inst();

    // 生命周期 (首次 --hd2d 启动时懒初始化; 失败则回退 2D)
    bool ensure_init(int target_w, int target_h);
    void shutdown();
    bool is_ready() const { return _ready; }

    // 主入口: 用 GameScene 状态构建绘制列表并渲染一帧
    // (内部: clear → 相机 → 场景构建 → 绘制 → 后处理回 2D target)
    void render_frame(GameScene& gs);

    // M6-v2a: 世界坐标 → 屏幕坐标投影 (名字标签/E 气泡等屏幕空间 UI 用;
    // 相机每帧 render_frame 后有效; 投影失败返回 {-1,-1})
    Vector2 world_to_screen(Vector3 world_pos, float y_offset = 0.0f) const;

    // M6-v2e: 3D 相机 shake (2D shake_offset 同源值; x/y 世界偏移,
    // render_frame 消费后自动清零)
    void set_camera_shake(float offset_x, float offset_y) {
        _shake_offset = {offset_x, 0.0f, offset_y};
    }

private:
    HD2DRenderer() = default;
    bool _ready = false;
    int _target_w = 960;
    int _target_h = 640;

    Camera3D _camera = {};
    Vector3 _camera_focus = {0, 0, 0};
    Vector3 _shake_offset = {0, 0, 0};  // M6-v2e: 3D 相机 shake (帧内消费)
    float _camera_yaw = 0.0f;       // 观察朝向 (切片固定 45° 俯角)

    // 场景数据缓存
    std::vector<HD2DDrawItem> _draw_items;

    // 切片内简单光照 (无 shader 依赖版: 环境光 + 方向光)
    Vector3 _light_dir = {0.35f, -1.0f, 0.25f};

    // ── M6-v2c: 地形 shader (距离雾/岩浆) + blob shadow 纹理 ──
    Shader _fog_shader = {};        // 地形 shader (雾+阴影+点光; 失败→回退默认)
    Shader _lava_shader = {};       // 岩浆动画 (失败→回退纯色 tile)
    bool _fog_ok = false;
    bool _lava_ok = false;
    Texture2D _blob_shadow_tex = {};// 径向渐变阴影贴图 (billboard 脚下)
    Texture2D _mote_glow_tex = {};  // A2.1: 氛围粒子软光 billboard 贴图
    int _lava_time_loc = -1;        // 岩浆 uTime uniform 位置缓存
    int _fog_viewpos_loc = -1;      // 雾 uniforms 位置缓存
    int _fog_color_loc = -1;
    int _fog_start_loc = -1;
    int _fog_end_loc = -1;
    // ── M6-v2e: 阴影/点光 uniforms (地形 shader) ──
    int _shadow_map_loc = -1;        // shadowMap sampler
    int _shadow_mvp_loc = -1;        // lightViewProj
    int _shadow_on_loc = -1;         // shadowEnabled
    int _shadow_texel_loc = -1;      // shadowTexel (PCF 步长)
    int _shadow_bias_loc = -1;       // shadowBias
    int _pl_count_loc = -1;          // 点光源 count
    int _pl_pos_loc = -1;            // 点光源 pos[8]
    int _pl_color_loc = -1;           // 点光源 color[8]
    int _pl_range_loc = -1;          // 点光源 range[8]
    // ── A1.1: 3D-aware billboard 真轮廓 (alpha-mask 描边 shader) ──
    Shader _outline_shader = {};     // hd2d_billboard_outline (失败→回退 4 向偏移)
    bool _outline_ok = false;
    hd2d::PartColorShader _part_color;
    int _outline_off_loc = -1;       // uTexelOffset (世界宽→屏幕 clamp→UV)
    int _outline_color_loc = -1;     // uOutlineColor
    int _outline_thresh_loc = -1;    // uAlphaThreshold
    // A3: 实体接收阴影 (与地形同字段名 sampler/mat/参数, loc 独立)
    int _out_shadow_map_loc = -1;
    int _out_shadow_mvp_loc = -1;
    int _out_shadow_on_loc = -1;
    int _out_shadow_texel_loc = -1;
    int _out_shadow_bias_loc = -1;
    float _px_per_world = 1.56f;     // 每帧：相机距离/FOV → 世界单位屏幕像素数
    // ── M6-v2g: bloom 三档手调 preset (监狱/深渊/火山) ──
    static void _apply_bloom_biome_preset(const GameMap* map);

    void _setup_camera();
    void _load_terrain_shaders();   // v2c: 雾/岩浆 shader 懒加载+缓存 loc
    void _load_outline_shader();    // A1.1: billboard 真轮廓 shader
    void _update_px_per_world();    // A1.1: 每帧世界单位→屏幕像素比例换算
    void _cache_v2e_uniform_locs(); // v2e: 阴影/点光 uniform 位置
    void _make_blob_shadow_tex();   // v2c: 径向渐变程序纹理
    void _make_mote_glow_tex();     // A2.1: 软光程序纹理 (additive 粒子)
    void _upload_fog_uniforms();    // v2c: 视点+雾色 → 地形 shader
    void _upload_shadow_uniforms(); // v2e: 光矩阵/深度纹理/参数 → 地形 shader
    void _upload_shadow_to(Shader sh, int map_loc, int mvp_loc, int on_loc,
                           int texel_loc, int bias_loc);  // A3: 共用体
    void _upload_point_lights();    // v2e: LAVA tile+玩家暖光 → uniform
    void _draw_scene();              // (相机已由 render_frame 定位)
    void _draw_terrain_pass();      // v2c: 地形批 (雾 shader 包裹/岩浆分流)
    void _draw_floor_tile(const HD2DDrawItem& item);
    void _draw_wall_block(const HD2DDrawItem& item);
    void _wall_quad(float u0, float u1, float v0, float v1,
                    Vector3 pos, float e, float h);   // M6-v2a: 墙体贴图侧面
    void _wall_top_quad(float u0, float u1, float v0, float v1,
                        Vector3 pos, float e, float h, Color tint);  // A3.2: 同贴图顶面
    void _draw_billboard(const HD2DDrawItem& item);
    void _draw_outline_fallback(const HD2DDrawItem& item, const Rectangle& src,
                                Vector3 pos, float w, float h);
    void _draw_billboard_outline(const HD2DDrawItem& item, const Rectangle& src,
                                 Vector3 pos, float w, float h);  // A1.1
    void _draw_blob_shadow(Vector3 pos, float w);   // M6-v2c: 接地阴影
    void _draw_fx_quad(const HD2DDrawItem& item);
    void _draw_fx_particle(const HD2DDrawItem& item);    // A10: 3D 粒子
    void _draw_fx_ring_3d(const HD2DDrawItem& item);     // A10: 3D 光环
    void _draw_fx_beam_3d(const HD2DDrawItem& item);     // A10: 3D 光束
    void _draw_fx_explosion_3d(const HD2DDrawItem& item); // A10: 3D 爆炸
    void _draw_weather_particles(class Game::WeatherSystem& weather);  // 天气粒子
    void _draw_portal_ring(const HD2DDrawItem& item);   // M6-v2a: 挑战传送门
    void _draw_door_panel(const HD2DDrawItem& item);    // M6-v2h: 门 (四态)
    void _draw_lock_badge(Vector3 pos, float door_h);  // M6-v2h: 锁徽记
    void _draw_room_icon(const HD2DDrawItem& item);     // M6-v2h: 房间图标
    void _draw_floor_decal(const HD2DDrawItem& item);   // M6-j: 地板装饰
    void _draw_stair_step(const HD2DDrawItem& item);    // N5: 楼梯立方
    void _draw_projectile_body(const HD2DDrawItem& item);  // M6-v2b
    void _draw_projectile_trail(const HD2DDrawItem& item, Color c);  // M6-v2d
    void _draw_warning_ring(const HD2DDrawItem& item);     // M6-v2b: 贴地预警/射程环
    void _draw_trajectory_line(const HD2DDrawItem& item);  // M6-v2b
    void _draw_cone_fan(const HD2DDrawItem& item);          // M6-v2b: Boss 扇形预警
    void _draw_entity_link(const HD2DDrawItem& item);       // M6-v2b: 实体连线
    void _draw_ambient_batch();     // A2.1: 氛围粒子单批软光 (替 M6-v2e 逐颗球)
    bool _ambient_billboard_basis(Vector3& right, Vector3& up) const;  // A2.1
    void _apply_post_processing(GameScene& gs);

    HD2DRenderer(const HD2DRenderer&) = delete;
    HD2DRenderer& operator=(const HD2DRenderer&) = delete;
};

// 全局开关 (main.cpp --hd2d 设置; 默认 false = 原 2D 路径)
extern bool g_hd2d_mode;
