#include "boss_behavior.h"
#include "relationship_system.h"
#include <algorithm>
#include <unordered_map>

void BossMemory::tick(float dt, bool reset) {
    timer += dt;
    if (timer > 30.0f || reset) {
        attacks = skills = heals = dodges = combos_max = potions_used = 0;
        timer = 0;
    }
}

BossPersonality boss_personality_for_floor(int floor) {
    if (floor == 5)  return BossPersonality::AGGRESSIVE;
    if (floor == 10) return BossPersonality::AREA_CONTROL;
    return BossPersonality::MANIPULATOR;  // floor 15
}

const char* decision_name(BossDecision d) {
    switch (d) {
        case BossDecision::CHASE:   return "CHASE";
        case BossDecision::RETREAT: return "RETREAT";
        case BossDecision::CHARGE:  return "CHARGE";
        case BossDecision::SUMMON:  return "SUMMON";
        case BossDecision::DEFEND:  return "DEFEND";
        case BossDecision::RANGED:  return "RANGED";
        case BossDecision::MELEE:   return "MELEE";
        case BossDecision::SPECIAL: return "SPECIAL";
        case BossDecision::PHASE_CHANGE: return "PHASE";
        case BossDecision::LAST_STAND:   return "LAST_STAND";
        default: return "IDLE";
    }
}

const char* personality_name(BossPersonality p) {
    switch (p) {
        case BossPersonality::AGGRESSIVE:    return "AGGRESSIVE";
        case BossPersonality::AREA_CONTROL:  return "AREA_CTRL";
        case BossPersonality::MANIPULATOR:   return "MANIPULATOR";
    }
    return "?";
}

// 权重计算核心
// ── G53: decision_weight 拆出的单决策评分 ───────────────────
// 每条 if 都是"一个情境因子给一个权重"; 抽成函数后 main 变成查表分发,
// 每个决策可独立调整而不影响其他。

static int decision_weight_chase(const BossContext& ctx, BossPersonality pers,
                                 const BossMemory&, const WorldState&, int) {
    int w = 0;
    if (ctx.player_far)  w += 30;
    if (ctx.player_near) w -= 15;
    if (ctx.hp_pct > 0.5f) w += 10;
    if (pers == BossPersonality::AGGRESSIVE) w += 20;
    return w;
}

static int decision_weight_retreat(const BossContext& ctx, BossPersonality pers,
                                   const BossMemory& mem, const WorldState&, int) {
    int w = 0;
    if (ctx.player_near && mem.attacks > 10) w += 25;
    if (ctx.hp_pct < 0.3f) w += 20;
    if (pers == BossPersonality::MANIPULATOR) w += 15;
    if (pers == BossPersonality::AGGRESSIVE) w -= 10; // 狂战士不后退
    return w;
}

static int decision_weight_charge(const BossContext& ctx, BossPersonality pers,
                                  const BossMemory& mem, const WorldState&, int) {
    int w = 0;
    if (ctx.player_far)            w += 25;
    if (ctx.player_low_hp)         w += 20;
    if (ctx.hp_pct < 0.3f)         w -= 10;
    if (mem.dodges > 8)            w += 15;  // 玩家爱翻滚→冲锋
    if (pers == BossPersonality::AGGRESSIVE) w += 15;
    if (ctx.near_arena_danger)     w += 10;  // 逼向危险区
    return w;
}

static int decision_weight_summon(const BossContext& ctx, BossPersonality pers,
                                  const BossMemory& mem, const WorldState& ws, int) {
    int w = 0;
    if (ctx.hp_pct < 0.5f)         w += 15;
    if (ws.has(WorldFlag::Blood_Ritual)) w += 10;
    if (mem.skills > 6)            w += 10;  // 玩家愛技能→召怪干扰
    if (pers == BossPersonality::MANIPULATOR) w += 20;
    if (pers == BossPersonality::AREA_CONTROL) w += 10;
    return w;
}

static int decision_weight_defend(const BossContext& ctx, BossPersonality pers,
                                  const BossMemory& mem, const WorldState&, int) {
    int w = 0;
    if (ctx.player_combo_high)     w += 25;
    if (mem.combos_max >= 10)      w += 15;
    if (ctx.hp_pct < 0.25f)        w += 10;
    if (pers == BossPersonality::MANIPULATOR) w += 10;
    return w;
}

static int decision_weight_ranged(const BossContext& ctx, BossPersonality pers,
                                  const BossMemory&, const WorldState&, int) {
    // mem reserved for D5 Step4
    int w = 0;
    if (ctx.player_near)           w += 20;
    if (pers == BossPersonality::AREA_CONTROL) w += 15;
    return w;
}

static int decision_weight_melee(const BossContext& ctx, BossPersonality pers,
                                 const BossMemory&, const WorldState&, int) {
    int w = 0;
    if (ctx.player_near)           w += 30;
    if (pers == BossPersonality::AGGRESSIVE) w += 20;
    if (ctx.player_using_skill)    w += 10;
    return w;
}

static int decision_weight_phase_change(const BossContext& ctx,
                                        BossPersonality, const BossMemory&,
                                        const WorldState&, int) {
    int w = 0;
    if (ctx.hp_pct < 0.35f && ctx.hp_pct > 0.25f) w += 50;
    if (ctx.last_stand) w -= 30; // LastStand期间不再PhaseChange
    return w;
}

static int decision_weight_last_stand(const BossContext& ctx,
                                      BossPersonality, const BossMemory&,
                                      const WorldState&, int) {
    if (ctx.hp_pct < 0.15f && !ctx.last_stand) return 80;
    return 0;
}

using DecisionWeightFn = int (*)(const BossContext&, BossPersonality,
                                 const BossMemory&, const WorldState&, int);

static const std::unordered_map<BossDecision, DecisionWeightFn>&
decision_weight_table() {
    static const std::unordered_map<BossDecision, DecisionWeightFn> tbl = {
        {BossDecision::CHASE,          decision_weight_chase},
        {BossDecision::RETREAT,        decision_weight_retreat},
        {BossDecision::CHARGE,         decision_weight_charge},
        {BossDecision::SUMMON,         decision_weight_summon},
        {BossDecision::DEFEND,         decision_weight_defend},
        {BossDecision::RANGED,         decision_weight_ranged},
        {BossDecision::MELEE,          decision_weight_melee},
        {BossDecision::PHASE_CHANGE,   decision_weight_phase_change},
        {BossDecision::LAST_STAND,     decision_weight_last_stand},
    };
    return tbl;
}

int decision_weight(BossDecision d, const BossContext& ctx,
                     BossPersonality pers, const BossMemory& mem,
                     const WorldState& ws, int boss_floor) {
    const auto& tbl = decision_weight_table();
    auto it = tbl.find(d);
    if (it != tbl.end()) return it->second(ctx, pers, mem, ws, boss_floor);
    return 0;
}

// 主入口: 评估当前最优决策
BossDecision evaluate_boss_decision(int boss_floor,
    const BossContext& ctx,
    const WorldState& ws,
    const RelationshipSystem& rels,
    BossBehaviorState& state,
    float dt) {
    (void)rels; // reserved for D5 Step4

    state.decision_timer -= dt;
    if (state.decision_timer > 0) return state.current; // 决策冷却中

    BossPersonality pers = state.personality;

    // 计算所有决策权重
    const int N = 11;
    BossDecision all[N] = {
        BossDecision::CHASE, BossDecision::RETREAT, BossDecision::CHARGE,
        BossDecision::SUMMON, BossDecision::DEFEND, BossDecision::RANGED,
        BossDecision::MELEE, BossDecision::SPECIAL,
        BossDecision::PHASE_CHANGE, BossDecision::LAST_STAND, BossDecision::IDLE
    };
    int best_w = -999; BossDecision best = BossDecision::IDLE;
    for (int i = 0; i < N; i++) {
        int w = decision_weight(all[i], ctx, pers, state.memory, ws, boss_floor);
        state.weights[i] = w;
        if (w > best_w) { best_w = w; best = all[i]; }
    }

    state.last_decision = state.current;
    state.current = best;
    state.decision_name = decision_name(best);
    state.personality_name = personality_name(pers);
    state.decision_timer = 0.5f; // 每0.5s重新评估
    return best;
}
