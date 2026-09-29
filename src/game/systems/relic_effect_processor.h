#pragma once
#include "relic_effect.h"
#include "damage_context.h"
#include <vector>
#include <string>

class Player;
class Monster;

// G15: 删除无调用方的 API 面 —— on_hit/on_pre_damage/on_hurt 实例版
// (docs/M1A_1_IMPL_PLAN.md:223-225 计划接线, 实际从未接上; 生产走 static 版),
// static_on_kill (生产用实例 on_kill), on_relic_acquired/on_relic_removed
// (apply-once 不变量已下沉到 Player::add_relic), set_enabled/is_enabled/
// reset_runtime/_runtime/_passive_applied (无人调用, _enabled 恒 true)。
class RelicEffectProcessor {
public:
    void on_kill(Player* player, Monster* monster,
                 std::vector<Monster*>& all_monsters);
    void on_floor_enter(Player* player);

    // Static entry points (follows ElementResolver pattern)
    static void static_on_hit(Player* player, Monster* target);
    static void static_on_pre_damage(DamageContext& ctx, Player* player);
    // PASSIVE apply/remove — 调用点: Player::add_relic / remove_relic, RewardManager
    static void apply_passive_for_relic(Player* player, const std::string& relic_id);
    static void remove_passive_for_relic(Player* player, const std::string& relic_id);

private:
    void _apply_on_kill(Player* player, Monster* monster,
                        std::vector<Monster*>& all_monsters,
                        const RelicEffectDef& eff);
    void _apply_on_floor_enter(Player* player, const RelicEffectDef& eff);

    float _rng_float();
};
