// P0-M2: teleport target selection — see sim_ai_teleport.h for contract
#include "sim_ai_teleport.h"
#include "world/game_map.h"
#include "world/room_manager.h"
#include "entities/monster.h"
#include <cmath>

namespace {
bool _tile_occupied(int tx, int ty, const std::vector<Monster*>& others) {
    Rectangle tile_r = { (float)(tx * 32), (float)(ty * 32), 32.0f, 32.0f };
    for (const Monster* m : others) {
        if (!m || !m->combat.is_alive) continue;
        if (CheckCollisionRecs(tile_r, m->entity.rect)) return true;
    }
    return false;
}

bool _tile_valid_for_landing(const GameMap* map, int tx, int ty) {
    if (map->tile_at(tx, ty) == TileType::WALL) return false;
    DoorState ds = map->door_state_at(tx, ty);
    if (ds == DoorState::LOCKED || ds == DoorState::SEALED) return false;
    // G14: CLOSED 门允许作为落点 (is_walkable=false 曾被误拦; 落地后开门)
    if (ds == DoorState::CLOSED) return true;
    return map->is_walkable(tx, ty);
}
} // namespace

// ── G53: sim_ai_teleport_target 拆出的三个 tier ─────────────

// Tier 1: 同房间 melee 安全距离 (r=2 优先, 再 r=1 的环) —
//         能打到但不被围 (48-80px)
template <typename TryFn>
static bool _teleport_tier1(int mtx, int mty, int monster_room,
                            const RoomManager* rooms, TryFn try_tile) {
    if (monster_room < 0) return false;
    for (int r = 2; r >= 1; r--) {
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dy) != r) continue;  // ring only
                int x = mtx + dx, y = mty + dy;
                if (rooms->room_at(x, y) != monster_room) continue;
                if (try_tile(x, y)) return true;
            }
    }
    return false;
}

// Tier 2: 同房间任意合法格 (怪物脚下 + r=1 全方块, 适配小房间)
template <typename TryFn>
static bool _teleport_tier2(int mtx, int mty, int monster_room,
                            const RoomManager* rooms, TryFn try_tile) {
    if (monster_room < 0) return false;
    if (try_tile(mtx, mty)) return true;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int x = mtx + dx, y = mty + dy;
            if (rooms->room_at(x, y) != monster_room) continue;
            if (try_tile(x, y)) return true;
        }
    return false;
}

// Tier 3: 不限房间, 怪物附近 r<=2 的环 (最后手段)
template <typename TryFn>
static bool _teleport_tier3(int mtx, int mty, TryFn try_tile) {
    for (int r = 1; r <= 2; r++)
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dy) != r) continue;
                int x = mtx + dx, y = mty + dy;
                if (try_tile(x, y)) return true;
            }
    return false;
}

TeleportResult sim_ai_teleport_target(const TeleportQuery& q) {
    TeleportResult out;
    if (!q.target || !q.map || !q.rooms) return out;
    const Monster* t = q.target;

    int mtx = (int)(t->entity.rect.x + t->entity.rect.width / 2) / 32;
    int mty = (int)(t->entity.rect.y + t->entity.rect.height / 2) / 32;
    int monster_room = q.rooms->room_at(mtx, mty);

    // P0-M2 v3: tiered same-room search.
    // Tier 1: same-room tiles at melee-safe distance (1.5-2.5 tiles) —
    //         attack range without face-tanking multiple monsters.
    // Tier 2: same-room any valid tile (fallback for tiny rooms).
    // Tier 3: any valid walkable tile (last resort).
    auto try_tile = [&](int x, int y) -> bool {
        if (x < 0 || y < 0 || x >= q.map->width || y >= q.map->height) return false;
        if (!_tile_valid_for_landing(q.map, x, y)) return false;
        if (_tile_occupied(x, y, q.extra_monsters)) return false;
        out.found = true; out.tile_x = x; out.tile_y = y;
        return true;
    };

    if (_teleport_tier1(mtx, mty, monster_room, q.rooms, try_tile)) return out;
    if (_teleport_tier2(mtx, mty, monster_room, q.rooms, try_tile)) return out;
    if (_teleport_tier3(mtx, mty, try_tile)) return out;
    return out;  // honest failure
}
