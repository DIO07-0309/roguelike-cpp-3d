// G8.4: CombatEnvironment implementation
#include "ai/rl/environment.h"
#include "ai/mcts/combat_evaluator.h"
#include <cstdlib>
#include <functional>

namespace rl {

Observation CombatEnvironment::reset(const mcts::SimulationState& initial) {
    _state = initial.clone();
    _rng_seed = _state.rng.seed;
    return Observation::from_state(_state);
}

// ── G50: step 子步骤 (原 100 行拆 3 段; 拆分是为可读性, 不改变任何顺序) ──
//
// ★ 顺序即语义: lcg() 的调用次序决定 RNG 流。步骤函数必须严格按
//   "动作结算 → 冷却衰减 → 怪物反击 → 终局判定" 的次序调用, 且每段内部
//   的 lcg() 调用次数与原内联版逐次相同 — 多调/少调一次都会让整条序列
//   错位, 且不会有任何报错。已加注释标明每段的 lcg 调用点。

// 伤害浮动: base * (0.8 + 0.4*rand), 下限 1。
// ★ 每次调用消费一次 lcg — ATTACK 与 SKILL 的伤害判定都必须经由此函数,
//   否则两侧 RNG 流会与 MCTS 分叉 (且不报错)。
static int rl_roll_damage(int base, const std::function<float()>& lcg) {
    return std::max(1, (int)(base * (0.8f + 0.4f * lcg())));
}

// 动作结算: ATTACK / SKILL_n / MOVE_* (lcg 只在伤害浮动处调用)
// 取最近的邻接活怪 (曼哈顿距离 < 1.5f); 无则 nullptr
static mcts::MonsterSnapshot* rl_nearest_adjacent(mcts::SimulationState& state) {
    auto& p = state.player;
    float best_d = 999;
    mcts::MonsterSnapshot* best = nullptr;
    for (auto& m : state.monsters) {
        if (!m.alive) continue;
        float d = std::abs(m.x - p.x) + std::abs(m.y - p.y);
        if (d < 1.5f && d < best_d) { best_d = d; best = &m; }
    }
    return best;
}

// AOE 技能: 打 3.0f 内所有活怪 (逐个吃 lcg), 并置该技能 CD
static void rl_aoe_hit(mcts::SimulationState& state, int attack, int skill_index,
                       const std::function<float()>& lcg) {
    auto& p = state.player;
    for (auto& m : state.monsters) {
        if (!m.alive) continue;
        float d = std::abs(m.x - p.x) + std::abs(m.y - p.y);
        if (d >= 3.0f) continue;
        int base = (int)(attack * 1.5f) - (int)(m.pdef * 0.5f);
        m.hp -= (float)rl_roll_damage(base, lcg);
        if (m.hp <= 0) m.alive = false;
    }
    p.skill_cooldowns[skill_index] = 3.0f;
}

static void rl_apply_action(mcts::SimulationState& state, mcts::CombatAction action,
                            const std::function<float()>& lcg) {
    auto& p = state.player;
    switch (action) {
    case mcts::CombatAction::ATTACK:
        // 打最近的活怪 (曼哈顿距离 < 1.5f); 不命中则不消耗 lcg
        if (auto* best = rl_nearest_adjacent(state)) {
            int base = p.attack - (int)(best->pdef * 0.5f);
            best->hp -= (float)rl_roll_damage(base, lcg);
            if (best->hp <= 0) best->alive = false;
        }
        p.attack_cooldown = 0.5f;
        break;
    case mcts::CombatAction::SKILL_1:
    case mcts::CombatAction::SKILL_2:
    case mcts::CombatAction::SKILL_3:
    case mcts::CombatAction::SKILL_4:
        // AOE: 3.0f 内所有活怪, 逐个吃 lcg
        rl_aoe_hit(state, p.attack, (int)action - (int)mcts::CombatAction::SKILL_1,
                   lcg);
        break;
    case mcts::CombatAction::MOVE_UP:    p.y -= 1; break;
    case mcts::CombatAction::MOVE_DOWN:  p.y += 1; break;
    case mcts::CombatAction::MOVE_LEFT:  p.x -= 1; break;
    case mcts::CombatAction::MOVE_RIGHT: p.x += 1; break;
    default: break; // WAIT
    }
}

// 怪物反击: 邻接 (d < 1.5f) 的活怪逐个过 40% 概率 (每个都吃一次 lcg,
// 无论是否命中 — 与 MCTS 侧保持一致, 否则两侧 RNG 流会分叉)
static void rl_monster_retaliation(mcts::SimulationState& state,
                                   const std::function<float()>& lcg) {
    auto& p = state.player;
    for (auto& m : state.monsters) {
        if (!m.alive) continue;
        float d = std::abs(m.x - p.x) + std::abs(m.y - p.y);
        if (d < 1.5f && lcg() < 0.4f) {
            int dmg = std::max(1, m.attack - (int)(p.pdef * 0.5f));
            p.hp -= (float)dmg;
            if (p.hp <= 0) p.alive = false;
        }
    }
}

// 冷却衰减 + 终局判定 + 奖励计算。
// ⚠ 不含任何 lcg 调用 — 这一步只做确定性算术, 保证调用它不会扰动 RNG 流。
// prev_hp / prev_alive 必须是"动作结算前"的快照, 奖励据此算增量。
static double rl_finish_step(mcts::SimulationState& state,
                             float prev_hp, int prev_alive) {
    auto& p = state.player;

    // Cooldown decay
    if (p.attack_cooldown > 0) p.attack_cooldown -= 0.25f;
    for (int i = 0; i < 4; i++)
        if (p.skill_cooldowns[i] > 0) p.skill_cooldowns[i] -= 0.25f;

    // Terminal check
    int alive_now = 0;
    for (auto& m : state.monsters) if (m.alive) alive_now++;
    if (alive_now == 0) { state.victory = true; state.terminal = true; }
    if (!p.alive) state.terminal = true;

    // Reward: kill bonus (incremental) - HP change penalty - terminal bonuses
    // Q3.15 (B3 fix): was "+50 per dead monster per step" — a corpse paid out
    // forever, coupling total return to remaining steps. Now one-shot per kill,
    // computed from prev_alive vs current alive count.
    double reward = 50.0 * (prev_alive - alive_now);
    float hp_delta = p.hp - prev_hp;
    if (hp_delta < 0) reward += hp_delta;   // penalty for taking damage
    if (state.victory) reward += 200.0;
    if (!p.alive) reward -= 200.0;
    return reward;
}

StepResult CombatEnvironment::step(mcts::CombatAction action) {
    if (_state.is_terminal()) {
        return {Observation::from_state(_state), 0, true};
    }

    // Apply action using same combat model as MCTS
    _state.depth++;
    auto& p = _state.player;

    auto lcg = [this]() -> float {
        _rng_seed = _rng_seed * 1664525u + 1013904223u;
        return (float)(_rng_seed & 0x7FFFFFFF) / 2147483648.0f;
    };

    float prev_hp = p.hp;
    int prev_alive = 0;
    for (auto& m : _state.monsters) if (m.alive) prev_alive++;

    rl_apply_action(_state, action, lcg);        // [lcg] 伤害浮动
    rl_monster_retaliation(_state, lcg);        // [lcg] 每个邻接怪一次
    const double reward = rl_finish_step(_state, prev_hp, prev_alive);  // 无 lcg

    return {Observation::from_state(_state), reward, _state.terminal};
}

EpisodeStats CombatEnvironment::run_episode(const mcts::SimulationState& initial, int max_steps) {
    EpisodeStats ep;
    reset(initial);
    while (!is_done() && ep.steps < max_steps) {
        auto actions = mcts::get_possible_actions(_state);
        if (actions.empty()) break;
        mcts::CombatAction a = actions[_rng_seed % actions.size()];
        _rng_seed = _rng_seed * 1664525u + 1013904223u;
        auto sr = step(a);
        ep.total_reward += sr.reward;
        ep.steps++;
    }
    ep.won = _state.victory;
    return ep;
}

} // namespace rl
