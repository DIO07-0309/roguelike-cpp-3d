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
    TeleportResult fallback;
    fallback.found = false;

    auto try_tile = [&](int x, int y) -> bool {
        if (x < 0 || y < 0 || x >= q.map->width || y >= q.map->height) return false;
        if (!_tile_valid_for_landing(q.map, x, y)) return false;
        if (_tile_occupied(x, y, q.extra_monsters)) return false;
        out.found = true; out.tile_x = x; out.tile_y = y;
        return true;
    };

    if (monster_room >= 0) {
        // Tier 1: ring r=2 then r=1.5-approx (dist 48-80px) preferred
        for (int r = 2; r >= 1; r--) {
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++) {
                    if (abs(dx) != r && abs(dy) != r) continue;  // ring only
                    int x = mtx + dx, y = mty + dy;
                    if (q.rooms->room_at(x, y) != monster_room) continue;
                    if (try_tile(x, y)) return out;
                }
        }
        // Tier 2: monster tile itself or r=1 full square (tiny rooms)
        if (try_tile(mtx, mty)) return out;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int x = mtx + dx, y = mty + dy;
                if (q.rooms->room_at(x, y) != monster_room) continue;
                if (try_tile(x, y)) return out;
            }
    }

    // Tier 3: any valid walkable tile near monster (r<=2 rings)
    for (int r = 1; r <= 2; r++)
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dy) != r) continue;
                int x = mtx + dx, y = mty + dy;
                if (try_tile(x, y)) { fallback = out; fallback.found = true; return fallback; }
            }
    return out;  // honest failure
}
