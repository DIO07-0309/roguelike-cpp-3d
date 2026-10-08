#pragma once
// G8.4: QAgent — tabular Q-learning agent.
// Uses discretized Observation→state_key for Q-table lookup.
// select(): epsilon-greedy from Q-table.
// update(): standard Q-learning update rule.

#include "observation.h"
#include "ai/mcts/action.h"
#include "ai/mcts/simulation_state.h"
#include <unordered_map>
#include <string>
#include <vector>

namespace rl {

struct QActionStats {
    std::string action_name;
    int count = 0;
    double avg_q = 0;
};

class QAgent {
public:
    QAgent(double alpha = 0.1, double gamma = 0.9, double epsilon = 0.1)
        : _alpha(alpha), _gamma(gamma), _epsilon(epsilon) {}

    // Epsilon-greedy action selection
    mcts::CombatAction select(const mcts::SimulationState& state, uint32_t& seed);

    // Q(s,a) ← Q(s,a) + α * (reward + γ*max_{a'∈next_legal} Q(s',a') - Q(s,a))
    // Q3.15: done=true 时 target 只等于 reward — 终局不得自举未来价值 (B1 fix)
    // Q3.15: 有效学习率按 (s,a) 访问次数衰减 α/(1+0.05n) — 满足 Σα²<∞ 收敛条件 (B2 fix)
    // v0.9.36 G32-E: max 只遍历 next_legal — 旧实现遍历全部 10 个动作,
    //   把非法动作(冷却中的技能/越界移动)的价值算进 bootstrap, 系统性高估
    //   target。表格 Q 下是轻微高估, 但若日后接神经网络会被直接拟合进去。
    // next_legal 必传: 空集 = 当前无合法动作, 按终局处理 (max = 0)。
    // 刻意不用默认参数 —— 让"忘记传 mask"变成编译错误而非静默 bug。
    void update(const Observation& obs, mcts::CombatAction action,
                double reward, const Observation& next_obs,
                const std::vector<mcts::CombatAction>& next_legal,
                bool done = false);

    // Stats
    size_t table_size() const { return _q.size(); }
    std::vector<QActionStats> action_distribution() const;

    // Q 表持久化 (JSON, 训练结果落盘供下次续训)
    bool save(const std::string& path) const;
    bool load(const std::string& path);

    // v0.9.30: 纯 exploit 决策 (镜像运行时用, 无需 SimulationState)
    // v0.9.36 G32-E: legal 必传 — 旧实现遍历全部动作, 会选中冷却中的技能。
    //   legal 空集 → 返回 -1 不接管, 交仲裁链下级层。
    int exploit_action(const Observation& obs,
                       const std::vector<mcts::CombatAction>& legal) const;

    // Hyperparameters
    double alpha() const { return _alpha; }
    double gamma() const { return _gamma; }
    double epsilon() const { return _epsilon; }
    void set_epsilon(double e) { _epsilon = e; }  // v0.9.29: 训练退火

private:
    std::string _make_key(const std::string& obs_key, int action_id) const;
    double _q_value(const std::string& obs_key, int action_id) const;
    // v0.9.36 G32-E: 只在合法动作里取最优 (G32-E-B1)
    mcts::CombatAction _best_action(
        const std::string& obs_key,
        const std::vector<mcts::CombatAction>& legal) const;
    // v0.9.36 G32-E: bootstrap 上界, 只遍历 next_legal (G32-E-B1)
    double _bootstrap_max(
        const std::string& next_key,
        const std::vector<mcts::CombatAction>& next_legal) const;

    std::unordered_map<std::string, double> _q;  // key:"obs:action" → q-value
    std::unordered_map<std::string, int> _sa_visits;  // Q3.15: (s,a) 访问计数 → 学习率衰减
    double _alpha, _gamma, _epsilon;
};

} // namespace rl
