#include "floor_manager.h"
#include <cmath>          // P1-A4: hypotf 出生房距离判定
#include "player.h"
#include "monster.h"
#include "ai.h"
#include "game_map.h"
#include "combat_system.h"
#include "config.h"
#include "floor_config.h"
#include "growth_curve.h"

// D1: 从 FloorConfig 读取概率 → 返回怪物类型 (G5.3: 12 slots)
static const char* _pick_monster_type(const FloorConfig& cfg) {
    int w[12];
    for (int i = 0; i < 12; i++) w[i] = cfg.enemy_weights[i];
    int total = 0;
    for (int i = 0; i < 12; i++) total += w[i];
    if (total <= 0) return "slime";
    int roll = (int)(rng() % (uint32_t)total);
    int sum = 0;
    for (int i = 0; i < 12; i++) {
        if (w[i] == 0) continue;
        sum += w[i];
        if (roll < sum) {
            switch (i) {
                case 0: return (rng() % 3 == 0) ? "orc" : "slime";
                case 1: return "archer";
                case 2: return "shaman";
                case 3: return "bomber";
                case 4: return "tank";
                case 5: return "elite";
                case 6: return (cfg.floor >= 6 && cfg.floor <= 10 && rng() % 2 == 0) ? "lightning_orb" : "charger";   // D8; 批次8: F6-10 火山轮换
                case 7: return "summoner";  // D8
                case 8: return (rng()%2==0)?"skeleton_archer":"goblin_hunter";   // G5.3: Sniper
                case 9: return (rng()%2==0)?"dark_mage":"void_walker";           // G5.3: Controller
                case 10:return (rng()%2==0)?"shadow_assassin":"night_stalker";   // G5.3: Ambush
                case 11:return (rng()%2==0)?"stone_guardian":"iron_sentinel";    // G5.3: Guardian
            }
        }
    }
    return "slime";
}

// G14: 单怪生成尝试 — 校验 tile 可走/非门/非锁门 + rect 级可走 + 距出生房 3 格
//      原缺陷: 只查 is_walkable(tile), OPEN 门可落怪 → 怪占门位, Room Encounter
//      门组 LOCKED 后怪被锁进异常位置 (sim stuck# 怪 tile 异常根因之一)
static bool _try_place_monster(GameMap* map, int stx, int sty,
                               const std::pair<int,int>& spawn_room,
                               const FloorConfig& cfg, const GrowthCurve& gc,
                               std::vector<std::unique_ptr<Monster>>& out_monsters) {
    if (map->tile_at(stx, sty) == TileType::DOOR) return false;      // 门 tile 不落怪
    DoorState ds = map->door_state_at(stx, sty);
    if (ds == DoorState::LOCKED || ds == DoorState::SEALED) return false;
    float d0 = hypotf((float)(stx - spawn_room.first),
                      (float)(sty - spawn_room.second));
    if (d0 <= 3.0f) return false;                                    // 出生房半径内不落
    Rectangle r = { (float)(stx * 32), (float)(sty * 32), 32.0f, 32.0f };
    if (!map->is_rect_walkable(r)) return false;                     // rect 级最终校验
    auto [px, py] = map->tile_to_pixel(stx, sty);
    const char* type = _pick_monster_type(cfg);
    auto* m = spawn_monster(px, py, type);
    m->combat.max_hp = (int)(m->combat.max_hp * gc.monster_hp);
    m->combat.current_hp = m->combat.max_hp;
    m->combat.attack = (int)(m->combat.attack * gc.monster_atk);
    m->entity.sync_rect();
    if (m->ai) m->ai->team_coop_chance = cfg.team_coop_chance;
    out_monsters.emplace_back(m);
    return true;
}

void FloorManager::spawn_floor_monsters(int floor_number, GameMap* map,
                                         std::vector<std::unique_ptr<Monster>>& out_monsters,
                                         const std::vector<std::pair<int,int>>& rooms) {
    const FloorConfig* cfg = get_floor_config(floor_number);
    const GrowthCurve& gc = g_growth.curve(floor_number);
    int count = cfg->monster_count;

    // P1-A4-fix: 出生房(rooms[0])永不刷怪 — 安全屋惯例 (原取模环绕落回出生房)
    const int room_count = (int)rooms.size();
    if (room_count < 2) return;   // 只有出生房 → 不刷 (防御)
    int ri = 1;
    while ((int)out_monsters.size() < count && ri < 500) {
        auto [tx, ty] = rooms[1 + (ri % (room_count - 1))];   // 房间中心 + 随机偏移
        int off_x = (int)(rng() % 5) - 2;
        int off_y = (int)(rng() % 5) - 2;
        _try_place_monster(map, tx + off_x, ty + off_y, rooms[0], *cfg, gc, out_monsters);
        ri++;
    }
}

bool FloorManager::is_floor_cleared(const std::vector<std::unique_ptr<Monster>>& monsters) {
    for (auto& m : monsters)
        if (m->combat.is_alive) return false;
    return true;
}

int FloorManager::check_floor_transition(const InputMap& input, int current_floor,
                                          GameMap* map, const Player* player,
                                          std::pair<int,int> stairs_pos) {
    if (!player || !map) return -1;

    auto [tx, ty] = map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width / 2,
        player->entity.rect.y + player->entity.rect.height / 2);

    if (std::make_pair(tx, ty) != stairs_pos) return -1;
    if (!input.is_action_just_pressed("descend")) return -1;

    if (current_floor >= MAX_FLOORS) return MAX_FLOORS + 1; // F15通关后 → 胜利
    return current_floor + 1; // 下楼 (F14→F15 返回15, 调用方判断 >MAX_FLOORS 才通关)
}
