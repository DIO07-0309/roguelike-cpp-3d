#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include <cstdint>

class Player;
class Monster;
class GameMap;
class AudioServer;

// from item.h
struct DroppedItem;

// from vfx_server.h
struct Effect;

// ============================================================
// CombatCoordinator — 战斗相关的跨系统调度
// 接入现有 CombatSystem，不重复实现伤害公式
// ============================================================
class CombatCoordinator {
public:

    // 技能释放 (含时停special-case + 伤害队列 + SFX + VFX + D2 heavy强化)
    // P1-C4-fix(UAF): pending_damage 存 instance_id (原裸指针见 game_scene.h)
    static std::string use_skill(int index, Player* player,
                                  std::vector<std::unique_ptr<Monster>>& monsters,
                                  GameMap* map, std::vector<Effect>& effects,
                                  AudioServer* audio, double game_time,
                                  float& time_stop_remaining,
                                  std::vector<std::pair<uint64_t, int>>& pending_damage,
                                  bool is_heavy = false);

    // D2 Step2: 技能命中后的 heavy 增强 VFX
    static void skill_heavy_vfx(Player* player, const std::string& skill_name,
                                 std::vector<Effect>& effects);

    // G21: on_monster_killed / cleanup_dead_monsters 已删除 — 互相引用的死代码簇
    // (唯一调用点在 cleanup_dead_monsters 内部, 后者 0 个外部调用者)。
    // 真实击杀结算走 GameSceneCombat::on_monster_killed。
};
