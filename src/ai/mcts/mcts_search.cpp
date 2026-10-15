// G8.3: MCTS search implementation
#include "ai/mcts/mcts_search.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace mcts {

// ── Combat simulation (simplified damage model) ─────────────
namespace detail {

static float dist(const PlayerSnapshot& p, const MonsterSnapshot& m) {
    return std::abs(p.x - m.x) + std::abs(p.y - m.y);
}

static int calc_damage(int atk, int def) {
    int dmg = atk - (int)(def * 0.5f);
    return std::max(1, dmg);
}

// ── G53: apply_action 拆出的动作段 ──────────────────────────

// 打最近的怪 (range 内; 伤害带 ±20% swing, 用 lcg 保证同 seed 可复现)
template <typename RngT>
static void mcts_hit_nearest(SimulationState& state, float range, int dmg_base,
                             RngT lcg) {
    MonsterSnapshot* best = nullptr; float bd = 999;
    for (auto& m : state.monsters) {
        if (!m.alive) continue;
        float d = dist(state.player, m);
        if (d < range && d < bd) { bd = d; best = &m; }
    }
    if (best) {
        int dmg = (int)(dmg_base * (0.8f + 0.4f * lcg()));
        best->hp -= (float)dmg;
        if (best->hp <= 0) best->alive = false;
    }
}

// 技能: 槽位 → 1.5× 攻击力的范围 3 伤害 (共 4 槽)
template <typename RngT>
static void mcts_apply_skill(SimulationState& state, int slot, RngT lcg) {
    int dmg = (int)(state.player.attack * 1.5f); // skill multiplier
    mcts_hit_nearest(state, 3.0f, dmg, lcg);
    state.player.skill_cooldowns[slot] = 3.0f;
}

// 怪物反击 (简化: 每只近战怪 40% 概率打一下)
template <typename RngT>
static void mcts_monster_retaliate(SimulationState& state, RngT lcg) {
    for (auto& m : state.monsters) {
        if (!m.alive) continue;
        float d = dist(state.player, m);
        if (d < 1.5f && lcg() < 0.4f) { // ~40% chance per monster per tick
            int dmg = calc_damage(m.attack, state.player.pdef);
            state.player.hp -= (float)dmg;
            if (state.player.hp <= 0) {
                state.player.alive = false;
                state.terminal = true;
            }
        }
    }
}

static void apply_action(SimulationState& state, CombatAction a, uint32_t& rng_seed) {
    auto& p = state.player;
    if (!p.alive) return;

    auto lcg = [&]() -> float {
        rng_seed = rng_seed * 1664525u + 1013904223u;
        return (float)(rng_seed & 0x7FFFFFFF) / 2147483648.0f;
    };

    switch (a) {
    case CombatAction::ATTACK:
        mcts_hit_nearest(state, 1.5f, p.attack, lcg);
        p.attack_cooldown = 0.5f;
        break;
    case CombatAction::SKILL_1:
    case CombatAction::SKILL_2:
    case CombatAction::SKILL_3:
    case CombatAction::SKILL_4:
        mcts_apply_skill(state, (int)a - (int)CombatAction::SKILL_1, lcg);
        break;
    case CombatAction::MOVE_UP:    p.y -= 1; break;
    case CombatAction::MOVE_DOWN:  p.y += 1; break;
    case CombatAction::MOVE_LEFT:  p.x -= 1; break;
    case CombatAction::MOVE_RIGHT: p.x += 1; break;
    default: break; // WAIT
    }

    // Decay cooldowns
    if (p.attack_cooldown > 0) p.attack_cooldown -= 0.25f;
    for (int i = 0; i < 4; i++)
        if (p.skill_cooldowns[i] > 0) p.skill_cooldowns[i] -= 0.25f;

    mcts_monster_retaliate(state, lcg);

    state.depth++;
    // Check victory
    bool any_alive = false;
    for (auto& m : state.monsters) if (m.alive) { any_alive = true; break; }
    if (!any_alive) { state.victory = true; state.terminal = true; }
}

static double evaluate_terminal(const SimulationState& state) {
    if (state.victory) return 1000.0;
    if (!state.player.alive) return -1000.0;
    // Heuristic: HP remaining + enemy deaths
    double score = state.player.hp * 2.0;
    for (auto& m : state.monsters) {
        if (!m.alive) score += 200.0;
        else score -= m.hp * 1.5;
    }
    // Depth penalty:
    score -= state.depth * 5.0;
    return score;
}

// Q3.15 (A1 fix): 奖励归一化到 [0,1] — UCT 探索常数 C=√2 的理论前提是奖励在该区间。
// 原始量纲 ±1000 下兄弟节点 Q 差值(数百)永远压过探索项上界(~2.5) → UCT 退化为纯贪心。
// sigmoid(score/250): 胜≈0.998, 负≈0.002, 中间启发式分数平滑映射, 区分度保留。
static double normalize_reward(double raw_score) {
    return 1.0 / (1.0 + std::exp(-raw_score / 250.0));
}

} // namespace detail

// ═══════════════════════════════════════════════════════════
//  MCTS public API
// ═══════════════════════════════════════════════════════════

CombatAction MCTS::search(const SimulationState& state) {
    // Build root
    auto root = std::make_unique<MCTSNode>();
    root->state = state.clone();

    for (int i = 0; i < _iterations; i++) {
        // 1. Selection
        MCTSNode* node = select(root.get());
        // 2. Expansion
        if (!node->is_terminal() && !node->is_fully_expanded()) {
            expand(node);
            if (!node->children.empty()) {
                // Q3.15 (A2 fix): expand-all 后按迭代序轮换首个模拟起点 —
                // 原恒取 children.back()(WAIT) 使每个新节点的首轮统计系统性偏向等待分支
                node = node->children[i % node->children.size()].get();
            }
        }
        // 3. Simulation
        double reward = simulate(node->state, state.rng.next() + i * 7919u);
        // 4. Backpropagation
        backpropagate(node, reward);
    }

    // Pick best child by visit count
    MCTSNode* best = nullptr;
    int best_visits = -1;
    for (auto& c : root->children) {
        if (c->visits > best_visits) {
            best_visits = c->visits;
            best = c.get();
        }
    }
    if (best) return best->action;

    // Fallback: any possible action
    auto actions = get_possible_actions(state);
    return actions.empty() ? CombatAction::WAIT : actions[0];
}

MCTSNode* MCTS::select(MCTSNode* node) {
    while (!node->is_terminal()) {
        if (!node->is_fully_expanded()) return node;
        // UCT selection
        MCTSNode* best = nullptr;
        double best_uct = -1e9;
        for (auto& c : node->children) {
            double uct = c->uct_value();
            if (uct > best_uct) { best_uct = uct; best = c.get(); }
        }
        if (!best) return node;
        node = best;
    }
    return node;
}

void MCTS::expand(MCTSNode* node) {
    auto actions = get_possible_actions(node->state);
    for (auto a : actions) {
        auto child = std::make_unique<MCTSNode>();
        child->action = a;
        child->parent = node;
        child->state = node->state.clone();
        uint32_t seed = child->state.rng.next() + node->children.size() * 7331u;
        detail::apply_action(child->state, a, seed);
        node->children.push_back(std::move(child));
    }
}

double MCTS::simulate(const SimulationState& state, uint32_t rng_seed) {
    SimulationState sim = state.clone();
    int max_depth = 10;
    uint32_t rng = rng_seed;
    while (!sim.is_terminal() && sim.depth < max_depth) {
        auto actions = get_possible_actions(sim);
        if (actions.empty()) break;
        CombatAction a = actions[(rng + sim.depth) % actions.size()];
        detail::apply_action(sim, a, rng);
    }
    return detail::normalize_reward(detail::evaluate_terminal(sim));
}

void MCTS::backpropagate(MCTSNode* leaf, double reward) {
    // Q3.15 (A3 fix): 标准无折扣回传 — 原 0.95 衰减使不同深度节点的
    // reward/visits 均值不可比，而 UCT 恰在同一父节点下比较兄弟均值。
    MCTSNode* node = leaf;
    while (node) {
        node->visits++;
        node->reward += reward;
        node = node->parent;
    }
}

} // namespace mcts
