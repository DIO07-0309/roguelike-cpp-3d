// v0.9.33 G32-B: DqnMirrorBridge 实现 — MirrorBattleState → DQN → PlayerActionType
#include "ai/rl/dqn_mirror_bridge.h"
#include "ai/mirror/mirror_agent.h"
#include "ai/mcts/action.h"

namespace rl {

int combat_action_to_player_action(int combat_action) {
    switch ((mcts::CombatAction)combat_action) {
    case mcts::CombatAction::ATTACK:
        return (int)PlayerActionType::ATTACK;
    case mcts::CombatAction::SKILL_1:
    case mcts::CombatAction::SKILL_2:
    case mcts::CombatAction::SKILL_3:
    case mcts::CombatAction::SKILL_4:
        return (int)PlayerActionType::SKILL;
    case mcts::CombatAction::MOVE_UP:
    case mcts::CombatAction::MOVE_DOWN:
    case mcts::CombatAction::MOVE_LEFT:
    case mcts::CombatAction::MOVE_RIGHT:
        return (int)PlayerActionType::MOVE;
    default:
        return (int)PlayerActionType::NONE;   // WAIT 不算决策动作
    }
}

bool DqnMirrorBridge::load(const std::string& weights_path) {
    return _policy.load(weights_path);
}

Observation DqnMirrorBridge::mirror_state_to_obs(
    const struct MirrorBattleState& st, int style) {
    // 字段映射与 mirror_agent.cpp:13 的 mirror_state_to_obs 完全一致
    // (单 Boss 战: enemy_count=1, boss_present=1, buff_count=0)
    Observation o;
    o.player_hp_ratio = st.player_hp_pct;
    o.player_attack = 10.0f;
    o.enemy_count = 1.0f;
    o.nearest_enemy_dist = st.dist_tiles;
    o.strongest_hp_ratio = st.boss_hp_pct;
    o.boss_present = 1.0f;
    o.buff_count = 0.0f;
    o.player_style = (float)style;
    return o;
}

PlayerActionType DqnMirrorBridge::predict(const struct MirrorBattleState& st,
                                          int player_style) {
    if (!_policy.ready()) return PlayerActionType::NONE;
    Observation obs = mirror_state_to_obs(st, player_style);

    std::vector<float> mask;
    if (_legal_override) {
        mask = *_legal_override;
    } else {
        // 无外部约束时按 mirror_legal_actions 的同一规则构造:
        // ATTACK 受 boss_can_attack 门控; SKILL/MOVE/WAIT 恒可用
        // (MirrorBattleState 缺技能冷却与坐标, 这是信息量不足下的近似,
        //  与 mirror_agent.cpp:319 保持一致)
        mask.assign((size_t)mcts::CombatAction::COUNT, 1.0f);
        if (!st.boss_can_attack) mask[(size_t)mcts::CombatAction::ATTACK] = 0.0f;
    }

    int act = _policy.select_action(obs.to_vector(), mask);
    if (act < 0) return PlayerActionType::NONE;
    return (PlayerActionType)combat_action_to_player_action(act);
}

std::shared_ptr<DqnMirrorBridge> install_dqn_predictor(
    MirrorAgent& agent, const std::string& weights_path, int player_style) {
    auto bridge = std::make_shared<DqnMirrorBridge>();
    if (!bridge->load(weights_path)) return nullptr;   // 未就绪 → 不改动 agent
    // 捕获 shared_ptr 而非裸指针: lambda 生命周期可能长于本函数栈帧
    agent.set_ml_predictor([bridge, player_style](const MirrorBattleState& st) {
        return bridge->predict(st, player_style);
    });
    return bridge;
}

} // namespace rl