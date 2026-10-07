#pragma once
// v0.9.32 G32: DqnPolicy — 把 DqnNet 与合法性约束绑成一个可用的决策单元。
//
// 存在理由: 网络只输出 10 维 Q 值, **不做动作选择**。mask 与 argmax 规则
// 必须与 Python 侧 action_mask.py 的 masked_argmax 完全一致, 否则会出现
// "Q 值算对了但动作选错"的静默错误。这套规则集中在此, 不允许散落到调用方。
//
// 与 Python 侧的对应关系 (改动必须同步):
//   build_input()      → build_input()
//   masked_argmax()    → select_action()
//   legal_mask         → 由调用方用 mcts::get_possible_actions 生成
#include <string>
#include <vector>
#include "ai/rl/dqn_net.h"

namespace rl {

class DqnPolicy {
public:
    DqnPolicy() = default;

    // 载入权重。失败返回 false (policy 保持未就绪, 调用方须检查 ready())
    bool load(const std::string& weights_path);

    bool ready() const { return _net.loaded(); }
    int input_dim() const { return _net.input_dim(); }

    // 构造 18 维网络输入 = 8 维观测 + 10 维 mask
    // obs 须为 Observation::to_vector() 的输出 (长度 8)
    // legal_mask 长度须为 10, 索引与 mcts::CombatAction 一致, 值为 1 表示合法
    static std::vector<float> build_input(const std::vector<float>& obs8,
                                          const std::vector<float>& legal_mask);

    // 在合法动作里取最大 Q。legal 为空时返回 CombatAction::WAIT
    // (对齐 Python masked_argmax 的全非法兜底)
    int select_action(const std::vector<float>& obs8,
                      const std::vector<float>& legal_mask) const;

    // 暴露原始 Q 值 (调试 / 测试用)
    bool q_values(const std::vector<float>& obs8,
                  const std::vector<float>& legal_mask,
                  std::vector<float>& out_q) const;

private:
    DqnNet _net;
};

} // namespace rl
