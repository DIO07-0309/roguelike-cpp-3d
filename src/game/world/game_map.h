#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "entity.h"
#include "raylib.h"
#include "special_room.h"
#include "biome.h"   // M4f: TilePalette

// 前向声明 (避免循环依赖)
enum class EventType : int;

// ============================================================
// D2 Step5: ArenaObject — 战场环境元素
// ============================================================
enum class ArenaObjectType { EXPLOSIVE_BARREL, HEALING_TOTEM, POISON_POOL, ROCK, SPIKE };

struct ArenaObject {
    ArenaObjectType type;
    int tile_x, tile_y;
    bool active = true;
    float timer = 0.0f;
};

// ============================================================
// Tile / GameMap — 地图数据结构
// ============================================================
enum class TileType { FLOOR, WALL, STAIRS_DOWN, LAVA, DOOR };  // Phase 2: DOOR — 门 tile

// Door 状态:
//   OPEN:   is_walkable=true,  blocks_sight=false (透视线, 默认态)
//   CLOSED: is_walkable=false, blocks_sight=true  (挡人+挡视线, E 键可开)
//   LOCKED: is_walkable=false, blocks_sight=true  (挡人+挡视线, E 键不可开, Room Encounter 封门)
//   SEALED: is_walkable=false, blocks_sight=true  (不可开, 未来扩展)
enum class DoorState : uint8_t { NONE = 0, OPEN = 1, CLOSED = 2, LOCKED = 3, SEALED = 4 };

struct Tile {
    TileType type = TileType::WALL;
    bool is_walkable = false;
    bool is_visible = false;    // 当前帧是否在玩家 FOV 内
    bool is_explored = false;   // 是否曾被玩家探索过
    bool boss_visible = false;  // 当前帧是否在 Boss FOV 内
    DoorState door_state = DoorState::NONE;  // Batch 1: 仅 DOOR tile 非 NONE

    static Tile floor()  { return {TileType::FLOOR, true, false, false, false, DoorState::NONE}; }
    static Tile wall()   { return {TileType::WALL, false, false, false, false, DoorState::NONE}; }
    static Tile stairs() { return {TileType::STAIRS_DOWN, true, false, false, false, DoorState::NONE}; }
    static Tile lava()   { return {TileType::LAVA, true, false, false, false, DoorState::NONE}; }
    static Tile door()   { return {TileType::DOOR, true, false, false, false, DoorState::OPEN}; }
};

class GameMap {
public:
    int width, height, tile_size;
    int pixel_width, pixel_height;

    GameMap(int w, int h, int ts);

    void load_from_template(const std::vector<std::string>& tmpl);
    void set_tile(int x, int y, TileType type);

    bool is_walkable(int tx, int ty) const;
    bool is_rect_walkable(Rectangle rect) const;
    TileType tile_at(int tx, int ty) const {  // M4b: tile 类型查询 (lava 感知)
        return (_in_bounds(tx, ty)) ? _tiles[ty][tx].type : TileType::WALL;
    }

    // 特殊房间 (B8)
    std::vector<SpecialRoom> special_rooms;
    SpecialRoom* get_special_room_at(int tile_x, int tile_y);
    const SpecialRoom* get_special_room_at(int tile_x, int tile_y) const;

    // D2 Step5: 战场元素
    std::vector<ArenaObject> arena_objects;
    ArenaObject*       get_arena_at(int tile_x, int tile_y);
    const ArenaObject* get_arena_at(int tile_x, int tile_y) const;

    // D4 Step1: 动态事件
    int   event_room_index = -1;
    int   event_tile_x = 0, event_tile_y = 0;
    bool  event_triggered = false;
    EventType event_type = (EventType)0;  // EventType::NONE

    Vector2 tile_to_pixel(int tx, int ty) const;
    std::pair<int,int> pixel_to_tile(float px, float py) const;

    // M4f: biome palette (绘制程序化像素纹理的基色)
    void set_palette(const TilePalette* palette);
    const TilePalette& palette() const { return _palette; }
    bool has_palette() const { return _has_palette; }

    // M5-A: 群系 id (prison/volcano/abyss) — set_palette 时由 GameScene 一并注入,
    // draw() 据此选择 wall_<id>/floor_<id> 群系贴图
    void set_biome_id(const char* id) { _biome_id = id ? id : ""; }
    const char* biome_id() const { return _biome_id.c_str(); }

    // Phase 1: FOV 可见性
    bool isVisible(int x, int y) const;
    bool isExplored(int x, int y) const;
    bool isBossVisible(int x, int y) const;
    bool blocks_sight(int x, int y) const;
    void update_fov(int center_x, int center_y, int radius);
    void update_boss_fov(int center_x, int center_y, int radius);
    void reset_visibility();
    
    // A6: Boss 出场时强制标记周围区域为已探索 (解决镜头聚焦后视野虚空问题)
    void mark_boss_area(int center_x, int center_y, int radius);

    // M6-v2d: 两 tile 间视线 (Bresenham 步进, 端点不检查) — 名条遮挡裁剪用
    // 只读查询, 无副作用; 越界 tile 按挡视线处理 (保守)
    bool has_line_of_sight(int x0, int y0, int x1, int y1) const;

    // Batch 1: Door 状态 API (仅对 DOOR tile 生效)
    DoorState door_state_at(int tx, int ty) const;   // 非 DOOR tile 返回 NONE
    bool set_door_state(int tx, int ty, DoorState s); // 非 DOOR tile 返回 false; OPEN/CLOSED 切换 is_walkable
    bool is_door(int tx, int ty) const;

    // Batch 2B: R1 接触开门 — 玩家移动矩形前方 (沿移动方向) 若有 CLOSED 门则开启
    // 返回: 是否开启了一扇门 (开启后由调用方触发 FOV 重算)
    bool try_open_door_toward(Rectangle moved_rect, float mx, float my);

    // Batch 2C: 门组原子开闭 (E3) — 多门房间全组一致
    bool close_room_doors(const std::vector<std::pair<int,int>>& door_tiles);  // → CLOSED (E 键可开)
    bool lock_room_doors(const std::vector<std::pair<int,int>>& door_tiles);   // → LOCKED (E 键不可开)
    bool open_room_doors(const std::vector<std::pair<int,int>>& door_tiles);   // → OPEN

    void draw(float cam_x, float cam_y, int screen_w, int screen_h) const;

    // G11.2: 探索足迹 — 玩家踏过地板留下渐隐脚印 (纯渲染, 不影响逻辑)
    void mark_footstep(int tx, int ty);
    int  explored_tile_count() const;      // 情绪 vignette 数据源
    void tick_footsteps(float dt);        // 足迹生命周期 (每帧调用)

    // G11.2: 足迹数据结构 (环形缓冲, 固定 32 个不逐 tile 存储)
    struct Footstep {
        int tx, ty;            // tile 坐标
        float life;            // 剩余秒数 (2.5s 渐隐)
    };
    static constexpr int FOOTSTEP_MAX = 32;

    // M6-n N1: HD2D 脚印渲染只读访问
    const Footstep* get_footsteps() const { return _footsteps; }
    int get_footstep_head() const { return _footstep_head; }
    static int get_footstep_max() { return FOOTSTEP_MAX; }

private:
    std::vector<std::vector<Tile>> _tiles;
    bool _in_bounds(int tx, int ty) const;
    void _init_walls();
    TilePalette _palette;      // M4f: 当前 biome 调色板
    bool _has_palette = false;
    std::string _biome_id;     // M5-A: 群系 id (空=通用贴图)

    Footstep _footsteps[FOOTSTEP_MAX] = {};
    int _footstep_head = 0;
    int _explored_count = 0;    // is_explored=true 的 tile 数 (set 时累加)
};
