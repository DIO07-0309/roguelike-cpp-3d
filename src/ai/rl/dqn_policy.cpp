// v0.9.32 G32: DqnPolicy 实现 — 输入构造 + masked argmax
// 必须与 tools/rl/action_mask.py 逐行对应 (build_input / masked_argmax)
#include "ai/rl/dqn_policy.h"
#include "ai/mcts/action.h"

namespace rl {

bool DqnPolicy::load(const std::string& weights_path) {
    return _net.load(weights_path);
}

std::vector<float> DqnPolicy::build_input(const std::vector<float>& obs8,
                                          const std::vector<float>& legal_mask) {
    // 观测 14 维 (Observation::to_vector 顺序, v0.9.34 G32-C 起):
    //   0 hp_ratio 1 attack 2 enemy_count 3 nearest_dist
    //   4 strongest 5 boss 6 buff_count 7 style
    //   8 player_x 9 player_y 10 enemy_x 11 enemy_y
    //  12 atk_cd_ratio 13 skill_ready_ratio
    // 前 8 维在此缩放, 后 6 维在 C++ Observation::from_state 里已归一化,
    // 直接透传。
    // 缩放系数必须与 Python build_input 完全相同, 否则 serve skew。
    std::vector<float> x;
    x.reserve(24);
    if (obs8.size() >= 14) {
        x.push_back(obs8[0]);                      // hp_ratio 已在 [0,1]
        x.push_back(obs8[1] / 50.0f);              // attack
        x.push_back(obs8[2] / 10.0f);              // enemy_count
        x.push_back(obs8[3] / 20.0f);              // nearest_dist
        x.push_back(obs8[4]);                      // strongest
        x.push_back(obs8[5]);                      // boss_present
        x.push_back(obs8[6] / 10.0f);              // buff_count
        x.push_back(obs8[7] / 4.0f);               // player_style
        for (int i = 8; i < 14; i++) x.push_back(obs8[i]);  // 位置 + 冷却
    } else {
        x.assign(14, 0.0f);
    }
    for (int i = 0; i < (int)mcts::CombatAction::COUNT; i++)
        x.push_back(i < (int)legal_mask.size() ? legal_mask[i] : 0.0f);
    return x;
}

bool DqnPolicy::q_values(const std::vector<float>& obs8,
                         const std::vector<float>& legal_mask,
                         std::vector<float>& out_q) const {
    if (!_net.loaded()) return false;
    auto x = build_input(obs8, legal_mask);
    if ((int)x.size() != _net.input_dim()) return false;
    _net.forward(x, out_q);
    return (int)out_q.size() == _net.output_dim();
}

int DqnPolicy::select_action(const std::vector<float>& obs8,
                             const std::vector<float>& legal_mask) const {
    std::vector<float> q;
    if (!q_values(obs8, legal_mask, q)) return (int)mcts::CombatAction::WAIT;

    // 全非法 → 兜底 WAIT (对齐 Python: if mask.sum() == 0 → WAIT)
    bool any_legal = false;
    for (float m : legal_mask) if (m > 0.0f) { any_legal = true; break; }
    if (!any_legal) return (int)mcts::CombatAction::WAIT;

    // 只在合法动作里取 max。Q 值可以是负的(挨打掉血),所以用 best 初始化为
    // 首个合法动作的 Q 而不是 0 / -inf, 避免"所有合法 Q 都为负"时误选。
    int best = -1;
    float best_q = 0.0f;
    for (int i = 0; i < (int)q.size() && i < (int)legal_mask.size(); i++) {
        if (legal_mask[i] <= 0.0f) continue;
        if (best < 0 || q[i] > best_q) { best_q = q[i]; best = i; }
    }
    return best < 0 ? (int)mcts::CombatAction::WAIT : best;
}

} // namespace rl
