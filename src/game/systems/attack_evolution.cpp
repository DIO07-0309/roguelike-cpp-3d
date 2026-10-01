#include "attack_evolution.h"
#include "player.h"
#include "build_score.h"
#include "core/event_bus.h"
#include "core/logger.h"

// ============================================================
// G1 Step1: AttackEvolutionManager 实现
// ============================================================

static bool _s_listener_registered = false;

void AttackEvolutionManager::register_listener() {
    // 防止重复注册 (GameScene 析构后重建场景时可能再次调用 _ready)
    if (_s_listener_registered) return;
    EventBus::inst().subscribe(GameEventType::RELIC_GAIN, _on_relic_gained, "AtkEvo");
    _s_listener_registered = true;
    LOG_INFO("[ATK_EVO] EventBus listener registered");
}

void AttackEvolutionManager::check_unlock(Player* player) {
    if (!player) return;
    auto& evo = player->attack_evo;

    int relic_count = (int)player->relics.size();

    // Lv1 → Lv2: 3 个 Relic (必须逐级 — 不能跳级)
    if (evo.level == 1 && relic_count >= 3) {
        int old_level = evo.level;
        evo.level = 2;
        EventBus::inst().emit(GameEventType::ATTACK_EVOLVED, player,
                               2 | (old_level << 16)); // val = new_level | (old_level << 16)
        LOG_INFO("[ATK_EVO] 普攻进化: 剑气 (Lv1→Lv2)  Relics=%d", relic_count);
        return;
    }

    // Lv2 → Lv3: 5 个 Relic + Build 确认
    if (evo.level == 2 && relic_count >= 5 && has_confirmed_build(player)) {
        int old_level = evo.level;
        evo.level = 3;
        EventBus::inst().emit(GameEventType::ATTACK_EVOLVED, player,
                               3 | (old_level << 16));
        LOG_INFO("[ATK_EVO] 普攻进化: 旋风斩 (Lv2→Lv3)  Relics=%d", relic_count);
    }

    // 注意: check_unlock 不需要 evo.level >= 3 的守卫
    // 因为 Lv1→2 和 Lv2→3 各自独立条件, 且 level 递增不可逆
    // 即使被重复调用 (如双 EventBus listener), 不会误触
}

const char* AttackEvolutionManager::current_name(const Player* player) {
    if (!player) return "铁剑";
    return player->attack_evo.name();
}

int AttackEvolutionManager::current_level(const Player* player) {
    if (!player) return 1;
    return player->attack_evo.level;
}

// ── EventBus Callback ──
void AttackEvolutionManager::_on_relic_gained(const GameEvent& ev) {
    if (!ev.sender) return;
    Player* player = static_cast<Player*>(ev.sender);
    check_unlock(player);
}
