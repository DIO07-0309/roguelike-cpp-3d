#pragma once
// v0.9.33 G32-B: DqnMirrorBridge — 把 DqnPolicy 适配成 MirrorAgent 的 MlPredictor。
//
// 为什么需要桥接层 (G31 遗留的语义鸿沟):
//   MlPredictor 签名: MirrorBattleState → PlayerActionType   ←玩家"意图类型"
//   DqnPolicy 输出: CombatAction (10 个按键级动作)← 玩家"战斗动作"
// 两者不是一回事: PlayerActionType 只有 ATTACK/SKILL/MOVE/DODGE/HEAL,
// CombatAction 还细分 4 个技能槽与 4 个移动方向。直接对接必须补两层映射。
//
// ★ 为何不写成 MirrorAgent::set_dqn_policy():
//   mirror_agent.{h,cpp} 在本仓库的并行开发中被反复 git checkout 还原
//   (实测 5 次, 含本轮刚加的 set_dqn_policy 与 G31-B5 修复被冲掉)。
//   故本模块**零侵入** —— 它只产出一个 MlPredictor, 由调用方自己传给
//   MirrorAgent::set_ml_predictor(), 不改动 MirrorAgent 任何一行。
//   装配代码见 dqn_mirror_bridge.cpp 的 install_dqn_predictor()。
//
// ★ 默认关闭: install_dqn_predictor() 加载失败返回 nullptr, 调用方直接
//   忽略即可, 此时仲裁链与改动前完全一致。
#include <memory>
#include <string>
#include <vector>

#include "ai/rl/dqn_policy.h"
#include "ai/rl/observation.h"
#include "game/ai/player_behavior/player_action.h"

// MirrorBattleState 定义在 mirror_agent.h 的**全局命名空间**(不是rl::),
// 但那个头文件会 include 本模块的间接依赖 → 循环依赖。故此处只声明,
// 由 .cpp include mirror_agent.h 取得完整定义。
struct MirrorBattleState;
class MirrorAgent;

namespace rl {

// CombatAction → PlayerActionType 的折叠规则 (与 combat_action_to_arm 同源思路):
//   ATTACK→ ATTACK
//   SKILL_1..4        → SKILL      (4 个槽位折叠成 1 类)
//   MOVE_UP/DOWN/L/R  → MOVE       (4 个方向折叠成 1 类; 该枚举无方向信息)
//   WAIT              → NONE       (不视为决策动作, 让 Boss 走常规应对)
int combat_action_to_player_action(int combat_action);

class DqnMirrorBridge {
public:
    DqnMirrorBridge() = default;

    // 载入权重。失败返回 false (bridge 保持未就绪)
    bool load(const std::string& weights_path);

    bool ready() const { return _policy.ready(); }

    // 主入口: 由当前局面推断玩家的下一个意图类型
    // 未就绪 / 输入不合法时返回 PlayerActionType::NONE
    PlayerActionType predict(const MirrorBattleState& st, int player_style);

    // 显式干预: 外部(如 replay 分析工具)可传入已算好的合法集
    void set_legal_override(const std::vector<float>* mask) { _legal_override = mask; }

private:
    // MirrorBattleState → 8 维观测 (与 mirror_agent.cpp 的 mirror_state_to_obs
    // 同字段映射, 但此处自带, 避免依赖 mirror_agent 的 static 函数)
    static Observation mirror_state_to_obs(const MirrorBattleState& st,
                                           int style);

    DqnPolicy _policy;
    const std::vector<float>* _legal_override = nullptr;
};

// ── 装配入口 (零侵入 MirrorAgent) ──
// 成功: agent 的 MlPredictor 插槽被填上, DQNN 层开始参与仲裁链。
// 失败或权重不可用: 返回 nullptr 且**不改动 agent**, 仲裁链保持原状。
//
// 用法:
//   auto fn = rl::install_dqn_predictor(agent, "tools/rl/dqn_weights.bin", style);
//   if (!fn) LOG_WARN("DQN 未接入 (权重缺失), 镜像层走原路径");
std::shared_ptr<DqnMirrorBridge> install_dqn_predictor(
    MirrorAgent& agent, const std::string& weights_path, int player_style);

} // namespace rl