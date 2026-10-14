#include "game_map.h"
#include "config.h"
#include "core/logger.h"
#include "resource_manager.h"                 // M4f: 纹理缓存
#include "game/rendering/sprite_renderer.h"   // M4f: 像素绘制
#include "game/rendering/door_renderer.h"     // Door sprites + anim
#include <cmath>
#include <cstdio>
#include <cstring>                   // M6-i.1: strcmp (biome 短名映射)
#include <unordered_map>

GameMap::GameMap(int w, int h, int ts)
    : width(w), height(h), tile_size(ts),
      pixel_width(w * ts), pixel_height(h * ts) {
    _init_walls();
}

void GameMap::_init_walls() {
    _tiles.resize(height);
    for (int y = 0; y < height; y++) {
        _tiles[y].resize(width);
        for (int x = 0; x < width; x++) {
            _tiles[y][x] = Tile::wall();
        }
    }
}

Vector2 GameMap::tile_to_pixel(int tx, int ty) const {
    return {(float)tx * tile_size, (float)ty * tile_size};
}
std::pair<int,int> GameMap::pixel_to_tile(float px, float py) const {
    return {(int)(px / tile_size), (int)(py / tile_size)};
}

void GameMap::load_from_template(const std::vector<std::string>& tmpl) {
    for (int y = 0; y < std::min((int)tmpl.size(), height); y++) {
        const auto& line = tmpl[y];
        for (int x = 0; x < std::min((int)line.size(), width); x++) {
            if (line[x] == '#') _tiles[y][x] = Tile::wall();
            else if (line[x] == '.') _tiles[y][x] = Tile::floor();
            else if (line[x] == 'D') _tiles[y][x] = Tile::door();  // Phase 2
        }
    }
}

void GameMap::set_tile(int x, int y, TileType t) {
    if (_in_bounds(x, y)) {
        _tiles[y][x].type = t;
        _tiles[y][x].is_walkable = (t != TileType::WALL);
        // Batch 1: 门态归一化 — 非 DOOR tile 门态必须为 NONE, DOOR tile 置默认 OPEN
        _tiles[y][x].door_state = (t == TileType::DOOR) ? DoorState::OPEN : DoorState::NONE;
    }
}

bool GameMap::is_walkable(int tx, int ty) const {
    if (!_in_bounds(tx, ty)) return false;
    return _tiles[ty][tx].is_walkable;
}

// P1-C7: Sim 决策层可走判定 — 单一入口统一三套 walkable 语义 (见 game_map.h 注释)
bool GameMap::is_passable_sim(int tx, int ty) const {
    if (!_in_bounds(tx, ty)) return false;
    if (_tiles[ty][tx].type == TileType::WALL) return false;
    DoorState ds = door_state_at(tx, ty);
    if (ds == DoorState::LOCKED || ds == DoorState::SEALED) return false;
    return true;
}

bool GameMap::is_rect_walkable(Rectangle r) const {
    float pts[8][2] = {
        {r.x, r.y}, {r.x + r.width - 1, r.y},
        {r.x, r.y + r.height - 1}, {r.x + r.width - 1, r.y + r.height - 1},
        {r.x + r.width/2, r.y}, {r.x + r.width/2, r.y + r.height - 1},
        {r.x, r.y + r.height/2}, {r.x + r.width - 1, r.y + r.height/2}
    };
    for (auto& [px, py] : pts) {
        auto [tx, ty] = pixel_to_tile(px, py);
        if (!is_walkable(tx, ty)) return false;
    }
    return true;
}

bool GameMap::_in_bounds(int tx, int ty) const {
    return tx >= 0 && tx < width && ty >= 0 && ty < height;
}

// ── Phase 1: FOV ──────────────────────────────────────────

bool GameMap::isVisible(int x, int y) const {
    return _in_bounds(x, y) && _tiles[y][x].is_visible;
}

bool GameMap::isExplored(int x, int y) const {
    return _in_bounds(x, y) && _tiles[y][x].is_explored;
}

bool GameMap::isBossVisible(int x, int y) const {
    return _in_bounds(x, y) && _tiles[y][x].boss_visible;
}

bool GameMap::blocks_sight(int x, int y) const {
    if (!_in_bounds(x, y)) return true;
    const Tile& t = _tiles[y][x];
    if (t.type == TileType::WALL) return true;
    if (t.type == TileType::DOOR) return t.door_state != DoorState::OPEN;
    return false;
}

// ── M6-v2d: 两 tile 间视线 (Bresenham; 中途挡视 tile 即断) ──
// 只读查询 (名条遮挡裁剪); 端点自身不检查 (玩家/怪所站 tile 通常可见)
bool GameMap::has_line_of_sight(int x0, int y0, int x1, int y1) const {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int cx = x0, cy = y0;
    while (cx != x1 || cy != y1) {
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; cx += sx; }
        if (e2 <= dx) { err += dx; cy += sy; }
        if (cx == x1 && cy == y1) break;      // 端点不检查
        if (blocks_sight(cx, cy)) return false;
    }
    return true;
}

// ── Batch 1: Door 状态 API ─────────────────────────────────

DoorState GameMap::door_state_at(int tx, int ty) const {
    if (!_in_bounds(tx, ty)) return DoorState::NONE;
    const Tile& t = _tiles[ty][tx];
    return (t.type == TileType::DOOR) ? t.door_state : DoorState::NONE;
}

bool GameMap::set_door_state(int tx, int ty, DoorState s) {
    if (!_in_bounds(tx, ty)) return false;
    Tile& t = _tiles[ty][tx];
    if (t.type != TileType::DOOR) return false;   // 仅 DOOR tile 可设门态
    if (t.door_state == s) return true;
    DoorState old = t.door_state;
    t.door_state = s;
    t.is_walkable = (s == DoorState::OPEN);
    DoorRenderer::inst().on_state_change(tx, ty, old, s);
    return true;
}

bool GameMap::is_door(int tx, int ty) const {
    return _in_bounds(tx, ty) && _tiles[ty][tx].type == TileType::DOOR;
}

// Batch 2B: R1 接触开门 — 玩家移动矩形沿移动方向的前缘若有 CLOSED 门则开启
bool GameMap::try_open_door_toward(Rectangle r, float mx, float my) {
    if (mx == 0.0f && my == 0.0f) return false;
    bool opened = false;
    // 用矩形中心 tile + 移动方向偏移, 检查目标 tile 是否为 CLOSED 门
    auto [cx, cy] = pixel_to_tile(r.x + r.width / 2, r.y + r.height / 2);
    int step_x = mx > 0 ? 1 : (mx < 0 ? -1 : 0);
    int step_y = my > 0 ? 1 : (my < 0 ? -1 : 0);
    int tx = cx + step_x, ty = cy + step_y;
    if (_in_bounds(tx, ty) && _tiles[ty][tx].type == TileType::DOOR &&
        _tiles[ty][tx].door_state == DoorState::CLOSED) {
        _tiles[ty][tx].door_state = DoorState::OPEN;
        _tiles[ty][tx].is_walkable = true;
        opened = true;
    }
    return opened;
}

// 门组原子关闭 — 房间所有门同时 CLOSED (E 键可开)
bool GameMap::close_room_doors(const std::vector<std::pair<int,int>>& door_tiles) {
    bool all_ok = true;
    for (auto& [tx, ty] : door_tiles) {
        if (!set_door_state(tx, ty, DoorState::CLOSED)) all_ok = false;
    }
    return all_ok;
}

// 门组原子锁定 — 房间所有门同时 LOCKED (E 键不可开, Room Encounter 封门)
bool GameMap::lock_room_doors(const std::vector<std::pair<int,int>>& door_tiles) {
    bool all_ok = true;
    for (auto& [tx, ty] : door_tiles) {
        if (!set_door_state(tx, ty, DoorState::LOCKED)) all_ok = false;
    }
    return all_ok;
}

// 门组原子开启 — 房间所有门同时 OPEN
bool GameMap::open_room_doors(const std::vector<std::pair<int,int>>& door_tiles) {
    bool all_ok = true;
    for (auto& [tx, ty] : door_tiles) {
        if (!set_door_state(tx, ty, DoorState::OPEN)) all_ok = false;
    }
    return all_ok;
}



void GameMap::reset_visibility() {
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            _tiles[y][x].is_visible = false;
            _tiles[y][x].is_explored = false;
        }
}

void GameMap::update_fov(int cx, int cy, int radius) {
    // 1. 清除所有 is_visible
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            _tiles[y][x].is_visible = false;

    // 2. 射线投射: 360 条, 每度一条
    for (int deg = 0; deg < 360; deg++) {
        float rad = deg * DEG2RAD;
        float dx = cosf(rad);
        float dy = sinf(rad);

        for (float dist = 0; dist <= radius; dist += 0.5f) {
            int tx = cx + static_cast<int>(roundf(dx * dist));
            int ty = cy + static_cast<int>(roundf(dy * dist));
            if (!_in_bounds(tx, ty)) break;

            bool was_explored = _tiles[ty][tx].is_explored;
            _tiles[ty][tx].is_visible = true;
            _tiles[ty][tx].is_explored = true;
            if (!was_explored) _explored_count++;   // G11.2: 探索统计 (情绪 vignette)

            if (blocks_sight(tx, ty)) break;
        }
    }
}

void GameMap::update_boss_fov(int cx, int cy, int radius) {
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            _tiles[y][x].boss_visible = false;

    for (int deg = 0; deg < 360; deg++) {
        float rad = deg * DEG2RAD;
        float dx = cosf(rad);
        float dy = sinf(rad);
        for (float dist = 0; dist <= radius; dist += 0.5f) {
            int tx = cx + static_cast<int>(roundf(dx * dist));
            int ty = cy + static_cast<int>(roundf(dy * dist));
            if (!_in_bounds(tx, ty)) break;
            _tiles[ty][tx].boss_visible = true;
            if (blocks_sight(tx, ty)) break;
        }
    }
}

void GameMap::mark_boss_room(int cx, int cy) {
    // A6: Boss 出场时强制标记所在房间为已探索
    // 解决镜头聚焦到 Boss 后视野虚空问题 (未渲染区域)
    // 从 Boss 位置向四周扩散，找到房间边界并标记所有地板 tile
    // 简单实现: 半径 6 tile (覆盖典型 Boss 房间)
    int radius = 6;
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (!_in_bounds(x, y)) continue;
            bool was_explored = _tiles[y][x].is_explored;
            _tiles[y][x].is_explored = true;
            _tiles[y][x].is_visible = true;  // 也标记为可见, 确保渲染
            if (!was_explored) _explored_count++;
        }
    }
}

SpecialRoom* GameMap::get_special_room_at(int tile_x, int tile_y) {
    for (auto& sr : special_rooms) {
        if (tile_x >= sr.rx && tile_x < sr.rx + sr.rw &&
            tile_y >= sr.ry && tile_y < sr.ry + sr.rh)
            return &sr;
    }
    return nullptr;
}

const SpecialRoom* GameMap::get_special_room_at(int tile_x, int tile_y) const {
    for (auto& sr : special_rooms) {
        if (tile_x >= sr.rx && tile_x < sr.rx + sr.rw &&
            tile_y >= sr.ry && tile_y < sr.ry + sr.rh)
            return &sr;
    }
    return nullptr;
}

void GameMap::set_palette(const TilePalette* palette) {
    _has_palette = (palette != nullptr);
    if (_has_palette) _palette = *palette;
}

// M5-A: 群系贴图 key — biomes.json id → "wall_<短名>"/"floor_<短名>"
// M6-i.1 修复: 全 id (forgotten_prison) 与 sprites.json 短名 (prison)
// 不匹配 → M5 起贴图查找永远 miss (全群系走程序化 tint, 即 P3 审计
// "群系贴图仅 tint 差异"的根因)。映射表对齐 sprites.json key。
static const char* _biome_short_id(const char* biome_id) {
    if (biome_id && biome_id[0]) {
        if (strcmp(biome_id, "forgotten_prison") == 0) return "prison";
        if (strcmp(biome_id, "ash_volcano") == 0)      return "volcano";
        if (strcmp(biome_id, "void_abyss") == 0)       return "abyss";
    }
    return "";
}

static const char* _biome_tile_key(const char* biome_id,
                                   const char* kind /* "wall"|"floor" */) {
    static char key[48];
    const char* short_id = _biome_short_id(biome_id);
    if (short_id[0]) {
        snprintf(key, sizeof(key), "%s_%s", kind, short_id);
        return key;
    }
    return kind;
}

// Phase 1: 颜色变暗 (已探索但不在视野内时使用)
static Color _dim(Color c, float brightness) {
    return {
        (unsigned char)(c.r * brightness),
        (unsigned char)(c.g * brightness),
        (unsigned char)(c.b * brightness),
        c.a
    };
}

// G11.1-C: 特殊房间像素徽记 — 替代 ASCII 字符的 20x20 程序化小图形
// 每种房间一个可辨识轮廓: 宝箱/祭坛十字/泉水波/金币/锤/书/骰子/柱/问号
// ── G53: 特殊房间徽记 (原 57 行 switch, 每类一个绘制函数) ──
// 共用的取色/偏移收进 EmblemCtx, 绘制体只认 ctx 不认原始坐标 —
// 每类徽记独立增删, 不会再牵动其他人的 case。
struct EmblemCtx {
    float ox, oy;   // 徽记区左上 (20x20)
    Color m;        // 主体色 (亮/暗 triggered 态)
    Color a;        // 点缀色
    Color dark;     // 暗部
    float bright;   // 原亮度 (泉水等固有色的叠加用)
};

using EmblemDraw = void (*)(const EmblemCtx&);

// 宝箱: 箱体 + 锁扣 + 掀盖
static void _emblem_treasure(const EmblemCtx& c) {
    DrawRectangle(c.ox, c.oy + 8, 20, 12, c.dark);
    DrawRectangleLines(c.ox, c.oy + 8, 20, 12, c.m);
    DrawRectangle(c.ox + 2, c.oy + 2, 16, 7, c.m);
    DrawRectangle(c.ox + 8, c.oy + 10, 4, 6, c.a);
}

// 祭坛: 台座 + 发光十字
static void _emblem_altar(const EmblemCtx& c) {
    DrawRectangle(c.ox + 2, c.oy + 14, 16, 6, c.dark);
    DrawRectangle(c.ox + 5, c.oy + 10, 10, 4, c.dark);
    DrawRectangle(c.ox + 8, c.oy + 1, 4, 10, c.a);
    DrawRectangle(c.ox + 4, c.oy + 4, 12, 4, c.a);
}

// 泉水: 圆池 + 涟漪 (池水色不随 triggered 变, 按原始亮度叠加)
static void _emblem_fountain(const EmblemCtx& c) {
    Color water = _dim(Color{70, 130, 170, 255}, c.bright);
    DrawCircle(c.ox + 10, c.oy + 10, 9, c.dark);
    DrawCircle(c.ox + 10, c.oy + 10, 7, water);
    DrawRing({c.ox + 10, c.oy + 10}, 3, 5, 0, 360, 12, c.m);
}

// 商店 / 神龛: 金币堆 + "$" 记号
static void _emblem_coin_pile(const EmblemCtx& c) {
    DrawCircle(c.ox + 7, c.oy + 13, 4, c.a);
    DrawCircle(c.ox + 14, c.oy + 12, 3, c.a);
    DrawCircle(c.ox + 10, c.oy + 7, 4, c.m);
    DrawText("$", (int)c.ox + 7, (int)c.oy + 4, 10, c.m);
}

// 铁匠: 锤头 + 柄
static void _emblem_blacksmith(const EmblemCtx& c) {
    DrawRectangle(c.ox + 2, c.oy + 3, 12, 7, c.dark);
    DrawRectangle(c.ox + 5, c.oy + 4, 9, 5, c.m);
    DrawRectangle(c.ox + 12, c.oy + 9, 3, 11, c.a);
}

// 图书馆: 打开的书
static void _emblem_library(const EmblemCtx& c) {
    DrawRectangle(c.ox + 1, c.oy + 6, 9, 10, c.dark);
    DrawRectangle(c.ox + 10, c.oy + 6, 9, 10, c.dark);
    DrawRectangle(c.ox + 2, c.oy + 7, 7, 8, c.m);
    DrawRectangle(c.ox + 11, c.oy + 7, 7, 8, c.m);
}

// 赌徒: 骰子 + 三点
static void _emblem_gambler(const EmblemCtx& c) {
    DrawRectangle(c.ox + 2, c.oy + 2, 16, 16, c.dark);
    DrawRectangleLines(c.ox + 2, c.oy + 2, 16, 16, c.m);
    DrawCircle(c.ox + 7, c.oy + 7, 2, c.m);
    DrawCircle(c.ox + 13, c.oy + 13, 2, c.m);
    DrawCircle(c.ox + 10, c.oy + 10, 2, c.a);
}

// SECRET/其他: 问号菱形 (default)
static void _emblem_unknown(const EmblemCtx& c) {
    DrawRectangle(c.ox + 6, c.oy + 1, 8, 4, c.m);
    DrawRectangle(c.ox + 12, c.oy + 3, 3, 6, c.m);
    DrawRectangle(c.ox + 9, c.oy + 8, 4, 3, c.m);
    DrawRectangle(c.ox + 9, c.oy + 13, 4, 4, c.a);
}

// 房间类型 -> 绘制函数
static const std::unordered_map<SpecialRoomType, EmblemDraw>& _emblem_table() {
    static const std::unordered_map<SpecialRoomType, EmblemDraw> tbl = {
        {SpecialRoomType::TREASURE,    _emblem_treasure},
        {SpecialRoomType::ALTAR,       _emblem_altar},
        {SpecialRoomType::FOUNTAIN,    _emblem_fountain},
        {SpecialRoomType::SHOP,        _emblem_coin_pile},
        {SpecialRoomType::SHRINE,      _emblem_coin_pile},
        {SpecialRoomType::BLACKSMITH,  _emblem_blacksmith},
        {SpecialRoomType::LIBRARY,     _emblem_library},
        {SpecialRoomType::GAMBLER,     _emblem_gambler},
    };
    return tbl;
}

static void _draw_room_emblem(SpecialRoomType type, float dx, float dy,
                              float bright, bool triggered) {
    EmblemCtx c;
    c.ox = dx + 6; c.oy = dy + 6;   // 20x20 徽记区
    Color main_c = triggered ? Color{110, 105, 100, 255} : Color{235, 225, 190, 255};
    Color accent = triggered ? Color{80, 78, 75, 255} : Color{212, 160, 23, 255};
    c.m = _dim(main_c, bright);
    c.a = _dim(accent, bright);
    c.dark = _dim(Color{40, 34, 28, 255}, bright);
    c.bright = bright;

    const auto& tbl = _emblem_table();
    auto it = tbl.find(type);
    if (it != tbl.end()) it->second(c);
    else _emblem_unknown(c);
}
void GameMap::draw(float cam_x, float cam_y, int sw, int sh) const {
    TileCtx ctx = setupTileTextures();
    drawTileLayer(cam_x, cam_y, sw, sh, ctx);
}

void GameMap::drawTileLayer(float cam_x, float cam_y, int sw, int sh, const TileCtx& ctx) const {
    int sc = std::max(0, (int)(cam_x / tile_size));
    int sr = std::max(0, (int)(cam_y / tile_size));
    int ec = std::min(width, (int)((cam_x + sw) / tile_size) + 1);
    int er = std::min(height, (int)((cam_y + sh) / tile_size) + 1);

    for (int y = sr; y < er; y++) {
        for (int x = sc; x < ec; x++) {
            float dx = x * tile_size - cam_x;
            float dy = y * tile_size - cam_y;
            const Tile& t = _tiles[y][x];
            if (!t.is_explored) continue;
            float bright = t.is_visible ? 1.0f : 0.6f;

            if (t.type == TileType::WALL) {
                drawWallTile(dx, dy, t, bright, ctx);
            } else if (t.type == TileType::FLOOR) {
                drawFloorTile(x, y, dx, dy, t, bright, ctx);
            } else if (t.type == TileType::STAIRS_DOWN) {
                drawStairsDown(dx, dy, bright);
            } else if (t.type == TileType::LAVA) {
                drawLava(dx, dy, bright);
            } else if (t.type == TileType::DOOR) {
                drawDoorTile(x, y, t.door_state, bright, cam_x, cam_y);
            }

            drawBossFOVOverlay(dx, dy, t);
            drawEventMarker(x, y, dx, dy, bright);
            drawArenaElement(x, y, dx, dy, bright);
        }
    }

    _draw_footstep_layer(cam_x, cam_y, sw, sh);
}

// G53: drawTileLayer 拆出的脚步残影层 (第二遍遍历)
void GameMap::_draw_footstep_layer(float cam_x, float cam_y, int sw, int sh) const {
    for (int i = 0; i < FOOTSTEP_MAX; i++) {
        const Footstep& fs = _footsteps[i];
        if (fs.life <= 0) continue;
        float fx = fs.tx * (float)tile_size - cam_x;
        float fy = fs.ty * (float)tile_size - cam_y;
        if (fx + tile_size < 0 || fx > sw || fy + tile_size < 0 || fy > sh) continue;
        if (!_in_bounds(fs.tx, fs.ty) || !_tiles[fs.ty][fs.tx].is_explored) continue;
        drawFootstepOverlay(fx, fy, fs.life, i);
    }
}

GameMap::TileCtx GameMap::setupTileTextures() const {
    Color wall_c = _has_palette ? _palette.wall_face : Color{60, 60, 80, 255};
    Color floor_c = _has_palette ? _palette.floor_base : Color{25, 25, 35, 255};
    char wall_key[40], floor_key[40];
    snprintf(wall_key, sizeof(wall_key), "wall_%02x%02x%02x", wall_c.r, wall_c.g, wall_c.b);
    snprintf(floor_key, sizeof(floor_key), "floor_%02x%02x%02x", floor_c.r, floor_c.g, floor_c.b);
    auto& rm = ResourceManager::inst();
    SpriteDef wall_def, floor_def;
    Texture2D wall_data = rm.sprite_by_key(_biome_tile_key(_biome_id.c_str(), "wall"), wall_def);
    Texture2D floor_data = rm.sprite_by_key(_biome_tile_key(_biome_id.c_str(), "floor"), floor_def);
    if (wall_data.id <= 0) { wall_def = SpriteDef{}; wall_data = rm.sprite_by_key("wall", wall_def); }
    if (floor_data.id <= 0) { floor_def = SpriteDef{}; floor_data = rm.sprite_by_key("floor", floor_def); }
    TileCtx ctx;
    ctx.wall_tex = wall_data.id > 0 ? wall_data : rm.procedural_tile(wall_key, wall_c, true);
    ctx.floor_tex = floor_data.id > 0 ? floor_data : rm.procedural_tile(floor_key, floor_c, false);
    ctx.wall_data = wall_data;
    ctx.floor_data = floor_data;
    ctx.wall_def = wall_def;
    ctx.floor_def = floor_def;
    ctx.tile_def.frame_w = tile_size;
    ctx.tile_def.frame_h = tile_size;
    ctx.wall_c = wall_c;
    ctx.floor_c = floor_c;
    return ctx;
}

void GameMap::drawWallTile(float dx, float dy, const Tile& t, float bright, const TileCtx& ctx) const {
    if (ctx.wall_tex.id > 0) {
        const SpriteDef& wd = ctx.wall_data.id > 0 ? ctx.wall_def : ctx.tile_def;
        Color wall_tint = _has_palette ? _palette.wall_face : Color{255, 255, 255, 255};
        SpriteRenderer::draw_sprite(ctx.wall_tex, wd, 0,
            {dx, dy, (float)tile_size, (float)tile_size},
            _dim(wall_tint, bright));
        return;
    }
    Color top_c = _has_palette ? _palette.wall_top : Color{75, 72, 68, 255};
    DrawRectangle(dx, dy, tile_size, tile_size / 3, _dim(top_c, bright));
    DrawRectangle(dx, dy + tile_size / 3, tile_size, tile_size - tile_size / 3,
                  _dim(ctx.wall_c, bright));
}

void GameMap::drawFloorTile(int x, int y, float dx, float dy, const Tile& t, float bright,
                            const TileCtx& ctx) const {
    const SpecialRoom* sr = get_special_room_at(x, y);
    if (sr) {
        drawSpecialRoomFloor(x, y, dx, dy, sr, bright, ctx);
        return;
    }
    bool painted = ctx.floor_tex.id > 0;
    if (painted) {
        const SpriteDef& fd = ctx.floor_data.id > 0 ? ctx.floor_def : ctx.tile_def;
        Color floor_tint = _has_palette ? _palette.floor_base : Color{255, 255, 255, 255};
        if (_has_palette) {
            unsigned int h = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u;
            h = (h ^ (h >> 13)) % 100u;
            if (h < 6u) floor_tint = _palette.floor_dirt;
            else if (h < 10u) floor_tint = _palette.floor_b;
        }
        SpriteRenderer::draw_sprite(ctx.floor_tex, fd, 0,
            {dx, dy, (float)tile_size, (float)tile_size},
            _dim(floor_tint, bright));
    } else {
        DrawRectangle(dx, dy, tile_size, tile_size, _dim(ctx.floor_c, bright));
    }
    DrawRectangleLines(dx, dy, tile_size, tile_size, _dim({35, 35, 45, 255}, bright));
    drawFloorDecorations(x, y, dx, dy, t, bright);
    drawFloorContactShadow(x, y, dx, dy);
}

void GameMap::drawFloorDecorations(int x, int y, float dx, float dy, const Tile& t,
                                   float bright) const {
    if (!_has_palette || !t.is_visible) return;
    unsigned int dh = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u;
    dh = (dh ^ (dh >> 13)) % 100u;
    if (dh < 3u) {
        Color rc = _dim({100, 140, 220, 120}, bright);
        DrawLineEx({dx + 8, dy + 24}, {dx + 16, dy + 8}, 1, rc);
        DrawLineEx({dx + 16, dy + 8}, {dx + 24, dy + 24}, 1, rc);
        DrawCircle(dx + 16, dy + 17, 2, _dim({140, 180, 255, 100}, bright));
        return;
    }
    if (dh < 7u) {
        Color cc = _dim(_palette.floor_joint, bright);
        DrawLineEx({dx + 4, dy + 6}, {dx + 14, dy + 16}, 1, cc);
        DrawLineEx({dx + 14, dy + 16}, {dx + 11, dy + 26}, 1, cc);
        DrawLineEx({dx + 14, dy + 16}, {dx + 26, dy + 13}, 1, cc);
        return;
    }
    if (dh < 9u) {
        Color mc = _dim(_palette.wall_moss, bright);
        DrawCircle(dx + 10, dy + 22, 3, mc);
        DrawCircle(dx + 13, dy + 24, 2, mc);
        DrawCircle(dx + 22, dy + 12, 2.5f, mc);
    }
}

void GameMap::drawFloorContactShadow(int x, int y, float dx, float dy) const {
    auto _is_wall = [&](int nx, int ny) {
        return nx >= 0 && nx < width && ny >= 0 && ny < height
            && _tiles[ny][nx].type == TileType::WALL;
    };
    for (int s = 0; s < 4; s++) {
        unsigned char sa = (unsigned char)(80 - s * 18);
        if (_is_wall(x, y - 1)) DrawRectangle(dx, dy + s, tile_size, 1, {8, 7, 6, sa});
        if (_is_wall(x - 1, y)) DrawRectangle(dx + s, dy, 1, tile_size, {8, 7, 6, sa});
        if (_is_wall(x + 1, y)) DrawRectangle(dx + tile_size - 1 - s, dy, 1, tile_size, {8, 7, 6, sa});
    }
}

void GameMap::drawSpecialRoomFloor(int x, int y, float dx, float dy, const SpecialRoom* sr,
                                   float bright, const TileCtx& ctx) const {
    Color base;
    switch (sr->type) {
        case SpecialRoomType::ALTAR:      base = {60, 44, 22, 255}; break;
        case SpecialRoomType::TREASURE:   base = {34, 46, 76, 255}; break;
        case SpecialRoomType::FOUNTAIN:   base = {28, 56, 34, 255}; break;
        case SpecialRoomType::SHOP:       base = {62, 56, 24, 255}; break;
        case SpecialRoomType::BLACKSMITH: base = {68, 40, 28, 255}; break;
        case SpecialRoomType::LIBRARY:    base = {22, 38, 68, 255}; break;
        case SpecialRoomType::GAMBLER:    base = {56, 22, 60, 255}; break;
        case SpecialRoomType::SHRINE:     base = {52, 52, 16, 255}; break;
        case SpecialRoomType::SECRET:     base = {62, 16, 16, 255}; break;
        default: base = {25, 25, 35, 255}; break;
    }
    if (sr->triggered) {
        base.r = (unsigned char)(base.r * 0.55f);
        base.g = (unsigned char)(base.g * 0.55f);
        base.b = (unsigned char)(base.b * 0.55f);
    }
    DrawRectangle(dx, dy, tile_size, tile_size, _dim(base, bright));
    DrawRectangleLines(dx, dy, tile_size, tile_size, _dim({35, 35, 45, 255}, bright));
    if (!sr->triggered) {
        Color inlay = _dim(Color{(unsigned char)(base.r * 2),
                                 (unsigned char)(base.g * 2),
                                 (unsigned char)(base.b * 2), 150}, bright);
        float mid = tile_size / 2.0f, half = 6.0f;
        DrawTriangle({dx + mid, dy + mid - half}, {dx + mid + half, dy + mid},
                     {dx + mid, dy + mid + half}, inlay);
        DrawTriangle({dx + mid, dy + mid - half}, {dx + mid - half, dy + mid},
                     {dx + mid, dy + mid + half}, inlay);
    }
    if (x == sr->cx && y == sr->cy) drawSpecialRoomIcon(x, y, dx, dy, sr, bright, ctx);
}

void GameMap::drawSpecialRoomIcon(int x, int y, float dx, float dy, const SpecialRoom* sr,
                                  float bright, const TileCtx& ctx) const {
    const char* pkey = nullptr;
    switch (sr->type) {
        case SpecialRoomType::ALTAR:      pkey = "room_altar"; break;
        case SpecialRoomType::TREASURE:   pkey = "room_chest"; break;
        case SpecialRoomType::FOUNTAIN:   pkey = "room_spring"; break;
        case SpecialRoomType::SHOP:       pkey = "room_shop"; break;
        case SpecialRoomType::BLACKSMITH: pkey = "room_blacksmith"; break;
        case SpecialRoomType::LIBRARY:    pkey = "room_library"; break;
        case SpecialRoomType::GAMBLER:    pkey = "room_gambler"; break;
        case SpecialRoomType::SHRINE:     pkey = "room_shrine"; break;
        case SpecialRoomType::SECRET:     pkey = "room_secret"; break;
        default: break;
    }
    SpriteDef pdef;
    Texture2D ptex = pkey ? ResourceManager::inst().sprite_by_key(pkey, pdef) : Texture2D{0};
    if (ptex.id > 0 && !sr->triggered) {
        float isz = tile_size * 0.75f;
        SpriteRenderer::draw_sprite(ptex, pdef, 0,
            {dx + (tile_size - isz) / 2, dy + (tile_size - isz) / 2, isz, isz});
        return;
    }
    _draw_room_emblem(sr->type, dx, dy, bright, sr->triggered);
}

void GameMap::drawStairsDown(float dx, float dy, float bright) const {
    DrawRectangle(dx, dy, tile_size, tile_size, _dim({28, 22, 12, 255}, bright));
    for (int s = 0; s < 4; s++) {
        Color step_c = _dim(Color{(unsigned char)(60 + s * 14),
                                  (unsigned char)(48 + s * 12),
                                  (unsigned char)(26 + s * 8), 255}, bright);
        DrawRectangle(dx + s * 4, dy + s * 8, tile_size - s * 8, 8, step_c);
        DrawRectangleLines(dx + s * 4, dy + s * 8, tile_size - s * 8, 8,
                           _dim({90, 75, 40, 200}, bright));
    }
    Color sc = _dim({255, 200, 50, 255}, bright);
    for (int c = 0; c < 3; c++) {
        float cy = dy + 4 + c * 6;
        DrawTriangle({dx + 10, cy}, {dx + 16, cy + 4}, {dx + 22, cy}, sc);
        DrawTriangle({dx + 10, cy + 8}, {dx + 16, cy + 4}, {dx + 22, cy + 8}, sc);
    }
}

void GameMap::drawLava(float dx, float dy, float bright) const {
    DrawRectangle(dx, dy, tile_size, tile_size, _dim({90, 20, 10, 255}, bright));
    float pulse = 0.7f + 0.3f * sinf((float)GetTime() * 4.0f);
    Color crack = _dim({255, 120, 30, (unsigned char)(150 * pulse)}, bright);
    DrawRectangle(dx + tile_size / 2 - 1, dy + 2, 2, tile_size - 4, crack);
    DrawRectangle(dx + 2, dy + tile_size / 2 - 1, tile_size - 4, 2, crack);
    DrawLineEx({dx + 4, dy + 4}, {dx + tile_size - 4, dy + tile_size - 4}, 1,
               _dim({255, 90, 20, (unsigned char)(110 * pulse)}, bright));
    DrawLineEx({dx + tile_size - 4, dy + 4}, {dx + 4, dy + tile_size - 4}, 1,
               _dim({255, 90, 20, (unsigned char)(110 * pulse)}, bright));
    DrawCircle(dx + tile_size / 2, dy + tile_size / 2, 6.0f + 2.0f * pulse,
               _dim({255, 100, 20, (unsigned char)(90 * pulse)}, bright));
    DrawCircle(dx + tile_size / 2, dy + tile_size / 2, 4.0f,
               _dim({255, 190, 80, (unsigned char)(170 + 60 * pulse)}, bright));
}

void GameMap::drawDoorTile(int x, int y, DoorState door_state, float bright,
                           float cam_x, float cam_y) const {
    DoorRenderer::inst().draw_door(x, y, door_state, bright, cam_x, cam_y);
}

void GameMap::drawBossFOVOverlay(float dx, float dy, const Tile& t) const {
    if (t.boss_visible && t.is_explored && !t.is_visible)
        DrawRectangle(dx, dy, tile_size, tile_size, {180, 40, 40, 50});
}

void GameMap::drawEventMarker(int x, int y, float dx, float dy, float bright) const {
    if (event_room_index < 0 || x != event_tile_x || y != event_tile_y) return;
    float pulse = 2 + sinf((float)GetTime() * 5) * 2;
    Color ec = event_triggered ? Color{80, 80, 80, 120} : Color{255, 200, 100, 200};
    DrawRectangleLinesEx({dx - pulse, dy - pulse, tile_size + pulse * 2, tile_size + pulse * 2},
                         float(1.5), _dim(ec, bright));
    DrawText("?", dx + 10, dy + 5, 18,
             _dim(event_triggered ? Color{100, 100, 100, 200} : Color{255, 220, 100, 255}, bright));
}

void GameMap::drawArenaElement(int x, int y, float dx, float dy, float bright) const {
    auto* arena = get_special_room_at(x, y) ? nullptr : get_arena_at(x, y);
    if (!arena) return;
    if (arena->type == ArenaObjectType::EXPLOSIVE_BARREL) drawArenaBarrel(arena, dx, dy, bright);
    else if (arena->type == ArenaObjectType::HEALING_TOTEM) drawArenaTotem(dx, dy, bright);
    else if (arena->type == ArenaObjectType::POISON_POOL) drawArenaPoisonPool(dx, dy, bright);
    else if (arena->type == ArenaObjectType::ROCK) drawArenaRock(dx, dy, bright);
    else if (arena->type == ArenaObjectType::SPIKE) drawArenaSpike(dx, dy, bright);
}

void GameMap::drawArenaBarrel(const ArenaObject* arena, float dx, float dy, float bright) const {
    if (arena->timer > 0.0f) {
        float fuse = 0.5f + 0.5f * sinf((float)GetTime() * 14.0f);
        DrawRectangle(dx + 4, dy + 4, tile_size - 8, tile_size - 8,
                      _dim({220, 70, 30, 255}, bright));
        DrawRectangleLines(dx + 2, dy + 2, tile_size - 4, tile_size - 4,
                           _dim({255, 90, 40, (unsigned char)(200 * fuse)}, bright));
    } else {
        DrawRectangle(dx + 6, dy + 6, tile_size - 12, tile_size - 12,
                      _dim({180, 100, 30, 255}, bright));
    }
    DrawText("!", dx + 12, dy + 5, 18, _dim({255, 200, 50, 255}, bright));
}

void GameMap::drawArenaTotem(float dx, float dy, float bright) const {
    DrawRectangle(dx + 4, dy + 4, tile_size - 8, tile_size - 8,
                  _dim({30, 140, 60, 255}, bright));
    DrawText("+", dx + 11, dy + 5, 18, _dim({100, 255, 120, 255}, bright));
}

void GameMap::drawArenaPoisonPool(float dx, float dy, float bright) const {
    float pulse = 0.6f + 0.4f * sinf((float)GetTime() * 5.0f);
    DrawRectangle(dx + 2, dy + 2, tile_size - 4, tile_size - 4,
                  _dim({40, 150, 55, 210}, bright));
    DrawRectangleLines(dx + 2, dy + 2, tile_size - 4, tile_size - 4,
                       _dim({90, 250, 90, (unsigned char)(170 * pulse)}, bright));
    DrawText("~", dx + 10, dy + 5, 16, _dim({170, 255, 170, 230}, bright));
}

void GameMap::drawArenaRock(float dx, float dy, float bright) const {
    DrawRectangle(dx + 2, dy + 8, tile_size - 4, tile_size - 10,
                  _dim({100, 95, 100, 255}, bright));
    DrawRectangleLines(dx + 2, dy + 8, tile_size - 4, tile_size - 10,
                       _dim({130, 125, 130, 255}, bright));
}

void GameMap::drawArenaSpike(float dx, float dy, float bright) const {
    DrawTriangle({dx + tile_size / 2, dy + 2}, {dx + 2, dy + tile_size - 4},
                 {dx + tile_size - 2, dy + tile_size - 4},
                 _dim({180, 50, 50, 255}, bright));
}

void GameMap::drawFootstepOverlay(float fx, float fy, float life, int idx) const {
    float fade = life / 2.5f;
    unsigned char a = (unsigned char)(95 * fade);
    bool alt = (idx & 1) == 0;
    float ox = alt ? -4.0f : 4.0f;
    Color fc{220, 200, 130, a};
    DrawEllipse(fx + tile_size / 2 + ox, fy + tile_size / 2 - 3, 3.2f, 5.0f, fc);
    DrawEllipse(fx + tile_size / 2 - ox, fy + tile_size / 2 + 4, 3.2f, 5.0f, fc);
    DrawCircleLines(fx + tile_size / 2, fy + tile_size / 2,
                    (float)tile_size * 0.42f, {212, 160, 23, (unsigned char)(a / 2)});
}

ArenaObject* GameMap::get_arena_at(int tile_x, int tile_y) {
    for (auto& ao : arena_objects)
        if (ao.tile_x == tile_x && ao.tile_y == tile_y && ao.active)
            return &ao;
    return nullptr;
}

// ── G11.2: 探索足迹 ──────────────────────────────────────────
void GameMap::mark_footstep(int tx, int ty) {
    // 环形覆盖: 最旧的足迹被挤掉; 同 tile 去重 (静止不刷)
    int idx = _footstep_head;
    for (int i = 0; i < FOOTSTEP_MAX; i++) {
        int j = (_footstep_head + i) % FOOTSTEP_MAX;
        if (_footsteps[j].life > 0 && _footsteps[j].tx == tx && _footsteps[j].ty == ty)
            return;
    }
    _footsteps[idx] = {tx, ty, 2.5f};
    _footstep_head = (_footstep_head + 1) % FOOTSTEP_MAX;
}

void GameMap::tick_footsteps(float dt) {
    for (auto& fs : _footsteps)
        if (fs.life > 0) fs.life -= dt;
}

int GameMap::explored_tile_count() const {
    return _explored_count;
}

const ArenaObject* GameMap::get_arena_at(int tile_x, int tile_y) const {
    for (auto& ao : arena_objects)
        if (ao.tile_x == tile_x && ao.tile_y == tile_y && ao.active)
            return &ao;
    return nullptr;
}
