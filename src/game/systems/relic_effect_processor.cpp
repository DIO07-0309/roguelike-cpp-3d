#include "relic_effect_processor.h"
#include "combat_system.h"
#include "combat_stats.h"
#include "player.h"
#include "monster.h"
#include <algorithm>
#include <cmath>

// G15: 本文件已删除全部无调用方 API（on_hit/on_pre_damage/on_hurt 实例版、
// static_on_kill、on_relic_acquired/on_relic_removed、tick 空体、
// set_enabled/is_enabled/reset_runtime、_runtime、_passive_applied 及其
// 3 个孤儿私有 helper）。去重：_apply_passive_stat / _remove_passive_stat
// 与 static 版逐字重复，已删；apply-once 不变量下沉到 Player::add_relic。
// 删除后类不再持有状态，_enabled 相关 guard 随之移除（原恒为 true）。

float RelicEffectProcessor::_rng_float() {
    return (float)(rng() % 1000) / 1000.0f;
}

void RelicEffectProcessor::on_kill(
    Player* player, Monster* monster,
    std::vector<Monster*>& all_monsters)
{
    if (!player) return;

    for (auto& relic : player->relics) {
        auto* def = get_relic_def(relic.id);
        if (!def || def->effects.empty()) continue;
        for (auto& eff : def->effects) {
            if (eff.trigger == RelicTrigger::ON_KILL) {
                _apply_on_kill(player, monster, all_monsters, eff);
            }
        }
    }
}

void RelicEffectProcessor::on_floor_enter(Player* player) {
    if (!player) return;

    for (auto& relic : player->relics) {
        auto* def = get_relic_def(relic.id);
        if (!def || def->effects.empty()) continue;
        for (auto& eff : def->effects) {
            if (eff.trigger == RelicTrigger::ON_FLOOR_ENTER) {
                _apply_on_floor_enter(player, eff);
            }
        }
    }
    // PASSIVE stats 不在此重放 —— 由 Player::add_relic 在获得时一次性施加
}

void RelicEffectProcessor::_apply_on_kill(
    Player* player, Monster* monster,
    std::vector<Monster*>& all_monsters,
    const RelicEffectDef& eff)
{
    if (_rng_float() > eff.chance) return;

    if (eff.type == RelicEffectType::DEAL_AOE_DAMAGE) {
        int dmg = static_cast<int>(
            player->combat.get_effective_attack() * eff.value);
        for (auto& m : all_monsters) {
            if (m != monster && m->combat.is_alive) {
                m->combat.take_damage(dmg);
            }
        }
    } else if (eff.type == RelicEffectType::HEAL) {
        int heal = static_cast<int>(
            eff.value * player->combat.max_hp);
        player->combat.heal(heal);
    } else if (eff.type == RelicEffectType::ADD_BUFF) {
        apply_buff(player, eff.buff_id, eff.value2);
    }
}

void RelicEffectProcessor::_apply_on_floor_enter(
    Player* player, const RelicEffectDef& eff)
{
    if (eff.type == RelicEffectType::ADD_BUFF) {
        apply_buff(player, eff.buff_id, eff.value2);
    }
}

// ── Static entry points ──

void RelicEffectProcessor::static_on_hit(Player* player, Monster* target) {
    if (!player || !target) return;
    for (auto& relic : player->relics) {
        auto* def = get_relic_def(relic.id);
        if (!def || def->effects.empty()) continue;
        for (auto& eff : def->effects) {
            if (eff.trigger == RelicTrigger::ON_HIT) {
                if ((float)(rng() % 1000) / 1000.0f > eff.chance) continue;
                if (eff.type == RelicEffectType::ADD_BUFF) {
                    apply_buff(target, eff.buff_id, eff.value2);
                }
            }
        }
    }
}

void RelicEffectProcessor::static_on_pre_damage(
    DamageContext& ctx, Player* player)
{
    if (!player) return;
    for (auto& relic : player->relics) {
        auto* def = get_relic_def(relic.id);
        if (!def || def->effects.empty()) continue;
        for (auto& eff : def->effects) {
            if (eff.trigger == RelicTrigger::PRE_DAMAGE) {
                if (eff.type == RelicEffectType::DAMAGE_REDUCTION) {
                    if ((float)(rng() % 1000) / 1000.0f < eff.chance) {
                        ctx.final_damage = static_cast<int>(
                            ctx.final_damage * (1.0f - eff.value));
                    }
                }
            }
        }
    }
}

// PASSIVE apply/remove — 调用点: Player::add_relic / remove_relic, RewardManager
void RelicEffectProcessor::apply_passive_for_relic(
    Player* player, const std::string& relic_id)
{
    if (!player || relic_id.empty()) return;
    auto* def = get_relic_def(relic_id);
    if (!def || def->effects.empty()) return;
    for (auto& eff : def->effects) {
        if (eff.trigger == RelicTrigger::PASSIVE) {
            if (eff.stat == "physical_defense")
                player->combat.physical_defense += static_cast<int>(eff.value);
            else if (eff.stat == "magical_defense")
                player->combat.magical_defense += static_cast<int>(eff.value);
            else if (eff.stat == "max_hp") {
                player->combat.max_hp += static_cast<int>(eff.value);
                player->combat.current_hp += static_cast<int>(eff.value);
            }
        }
    }
}

void RelicEffectProcessor::remove_passive_for_relic(
    Player* player, const std::string& relic_id)
{
    if (!player || relic_id.empty()) return;
    auto* def = get_relic_def(relic_id);
    if (!def || def->effects.empty()) return;
    for (auto& eff : def->effects) {
        if (eff.trigger == RelicTrigger::PASSIVE) {
            if (eff.stat == "physical_defense")
                player->combat.physical_defense -= static_cast<int>(eff.value);
            else if (eff.stat == "magical_defense")
                player->combat.magical_defense -= static_cast<int>(eff.value);
            else if (eff.stat == "max_hp") {
                player->combat.max_hp -= static_cast<int>(eff.value);
                player->combat.current_hp = std::min(
                    player->combat.current_hp, player->combat.max_hp);
            }
        }
    }
}
