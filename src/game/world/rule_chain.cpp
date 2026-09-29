#include "rule_chain.h"
#include "world_state.h"
#include "core/event_bus.h"
#include "core/service_locator.h"
#include "core/logger.h"
#include "director/gameplay_system_director.h"

// ============================================================
// G1 Step4: RuleChainManager 实现
// ============================================================

static bool _s_registered = false;

void RuleChainManager::register_listener() {
    if (_s_registered) return;
    EventBus::inst().subscribe(GameEventType::BOSS_DEAD, _on_boss_dead, "RuleChain");
    _s_registered = true;
    LOG_INFO("[RULE_CHAIN] Listener registered");
}

void RuleChainManager::unregister_listener() {
    // EventBus 是进程级全局单例，回调幂等 — 无需显式注销
}

// ── Boss → 规则映射 ──
std::vector<std::string> RuleChainManager::rules_for_boss(int boss_floor) {
    switch (boss_floor) {
        case 5:  return {"shadow_charge", "summon_priority"};
        case 10: return {"arena_movement", "shield_patience"};
        case 15: return {"rule_override"};
        default: return {};
    }
}

// ── 规则展示名称 ──
const char* RuleChainManager::rule_display_name(const std::string& rule_id) {
    if (rule_id == "shadow_charge")   return "暗影冲锋";
    if (rule_id == "summon_priority") return "召唤优先";
    if (rule_id == "arena_movement")  return "竞技场移动";
    if (rule_id == "shield_patience") return "护盾耐心";
    if (rule_id == "rule_override")   return "规则覆写";
    return rule_id.c_str();
}

void RuleChainManager::activate_for_boss(int boss_floor, WorldState& ws) {
    auto rules = rules_for_boss(boss_floor);
    if (rules.empty()) return;

    int activated = 0;
    for (auto& r : rules) {
        std::string key = "rule_" + r;
        if (ws.counter(key) == 0) {
            ws.add_counter(key, 1);
            activated++;

            // G1 Step4: 发送 BOSS_RULE_ACTIVATED 事件供 Presentation 展示
            EventBus::inst().emit(GameEventType::BOSS_RULE_ACTIVATED,
                                  nullptr, boss_floor, 0.0f, r.c_str());

            LOG_INFO("[RULE_CHAIN] Rule activated: %s (%s) — Boss F%d",
                     r.c_str(), rule_display_name(r), boss_floor);
        }
    }
    if (activated > 0) {
        ws.add_counter("active_rule_count", activated);
        LOG_INFO("[RULE_CHAIN] Total %d rules activated for Boss F%d",
                 activated, boss_floor);
    }
}

// ── EventBus Callback ──
void RuleChainManager::_on_boss_dead(const GameEvent& ev) {
    int boss_floor = ev.int_val;   // BOSS_DEAD 事件中 int_val = current_floor
    if (boss_floor <= 0) return;

    auto* gs = ServiceLocator::get<GameplaySystemDirector>();
    if (!gs) {
        LOG_INFO("[RULE_CHAIN] WARNING: GameplaySystemDirector not found in ServiceLocator");
        return;
    }

    activate_for_boss(boss_floor, gs->world_state);
}
