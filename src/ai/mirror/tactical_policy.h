#pragma once
// v0.9.35 G32-D: TacticalPolicy — 规则式玩家意图预测器(零侵入, 默认关闭)
//
// 定位: G32 的 RL 路线实测结论是「玩具 env 里规则策略(60%)优于 DQN(37%)/
// PPO(19%)」, 故把**可用的规则策略**落成代码, 而非继续训不出效果的模型。
//
// 与既有层的区别与互补:
//   - behavior_clone_table : 预测玩家**意图**(ATTACK/SKILL/HEAL/DODGE),
//     输入 dist + hp + skills_ready, 用于镜像反制
//   - tactical_chain_table : n-gram 战术序列(SKILL_1→COMBO_2 这种)
//   - tactical_policy      : 本类。基于 RL 训练中学到的**优先级结构**
//     (普攻 > 技能 > 走位 > 等待), 但用规则实现, 可解释、可调、零推理开销。
//
// 为什么不直接复用 RL 权重: 实测 DQN/PPO 的 attack 占比≈0, 贪心胜率低于
// 规则基线; 且 RL 权重随 Observation 维度变化而失效(8→14), 每次调整特征
// 都要重训。规则层无此耦合。
//
// ★ 默认不参与仲裁: 本类只提供 predict(), 由调用方决定是否使用。
//   接入方式见文件末尾 install_tactical_predictor()。
#pragma once

#include "ai/mirror/mirror_agent.h"
#include "game/ai/player_behavior/player_action.h"

#include <string>

namespace mirror {

// ── 全局开关 (命令行 --tactical-mirror) ──
// 默认 false: TacticalPolicy 不参与仲裁, 镜像 Boss 行为与接入前完全一致。
// 置 true 才在 _init_mirror_boss 里调用 install_tactical_predictor()。
// 为什么默认关: RL 路线实测结论是"规则策略优于学习算法", 但它终究是新
// 逻辑, 需人工打一场 Boss 验收后才敢常开。
inline bool& tactical_mirror_enabled() {
    static bool enabled = false;
    return enabled;
}

// 意图预测结果 — 附带置信与依据, 便于 HUD/调试展示
struct TacticalIntent {
    PlayerActionType action = PlayerActionType::NONE;
    float confidence = 0.0f;   // 规则式打分, 非概率
    const char* reason = "";  // 决策依据(可解释性)
};

class TacticalPolicy {
public:
    // 规则参数 — 阈值取自 resources/weapons.json 的 base_range 量级
    struct Config {
        float melee_range = 2.5f;      // 普攻有效距离
        float skill_range = 5.0f;      // 技能有效距离
        float flee_hp_ratio = 0.25f;    // 血量低于此值 → 倾向拉开距离
        float skill_hp_floor = 0.35f;   // 血量低于此值 → 不用治疗向技能
        bool enable_melee = true;
        bool enable_skill = true;
        bool enable_kite = true;
    };

    // 注意: 构造函数默认值不能用 `= Config{}` —— 嵌套类的默认成员初始化器
    // 在外层类尚未完成定义时不可用 (GCC: required before the end of its
    // enclosing class)。故用 Config() 由编译器生成的形参默认。
    TacticalPolicy() : TacticalPolicy(Config()) {}
    explicit TacticalPolicy(const Config& cfg) : _cfg(cfg) {}

    // 主入口: 由当前局面推断玩家的下一个意图
    TacticalIntent predict(const MirrorBattleState& st) const;

    const Config& config() const { return _cfg; }

    static const char* intent_name(PlayerActionType t);

private:
    Config _cfg;
};

}  // namespace mirror

namespace mirror {
// ── 零侵入装配入口(与 install_dqn_predictor 同模式) ──
// 成功: agent 的 MlPredictor 插槽被填上。失败返回 false 且**不改动 agent**,
//      仲裁链保持原状。
// 用法:
//   if (!mirror::install_tactical_predictor(agent))
//       LOG_WARN("战术层未接入, 镜像走原路径");
bool install_tactical_predictor(MirrorAgent& agent,
                                const TacticalPolicy::Config& cfg
                                    = TacticalPolicy::Config());

// 仅在 tactical_mirror_enabled() 为 true 时才装配 —— 供 BossSystemDirector
// 调用, 避免调用点散落 if 判断。
bool install_tactical_predictor_if_enabled(
    MirrorAgent& agent, const TacticalPolicy::Config& cfg
        = TacticalPolicy::Config());
}  // namespace mirror