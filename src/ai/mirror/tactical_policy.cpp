// v0.9.35 G32-D: TacticalPolicy 实现
//
// 优先级结构来自 RL 训练实测 (tools/rl/eval_ppo.py):
//   普攻 (attack) > 技能 (skill) > 走位 (kite) > 等待
// 依据: 玩具 env 里解析 DPS 普攻 1.912 > 斩击 0.447, 且普攻冷却仅
// 3.4 步而技能 32~320 步 —— 冷却窗口决定了大部分时间只能等, 这是
// **真实战斗也成立的结论**, 故规则层直接采纳。
#include "ai/mirror/tactical_policy.h"

namespace mirror {

namespace {
    // 把 0~1 置信度映射成便于比较的分值
    float score(float base, float extra) { return base + extra; }
}  // namespace

const char* TacticalPolicy::intent_name(PlayerActionType t) {
    switch (t) {
    case PlayerActionType::ATTACK: return "attack";
    case PlayerActionType::SKILL:  return "skill";
    case PlayerActionType::MOVE:   return "move";
    case PlayerActionType::DODGE:  return "dodge";
    case PlayerActionType::HEAL:   return "heal";
    default:                       return "none";
    }
}

// ── G53: 4 条优先级规则拆出 (每条都是"适用则填 out 并返回 true") ──

// 优先级 1: 低血量走位 (存活优先于输出)
// 玩家濒死时"拉开距离等技能冷却"优于"贴脸换血"
bool TacticalPolicy::_rule_flee(const MirrorBattleState& st,
                                TacticalIntent& out) const {
    const float php = st.player_hp_pct;
    if (!_cfg.enable_kite || php > _cfg.flee_hp_ratio) return false;
    if (st.dist_tiles < _cfg.melee_range) {
        out = {PlayerActionType::DODGE, score(0.60f, php * 0.30f),
               "低血量且贴身 → 闪避拉开"};
    } else {
        out = {PlayerActionType::MOVE, score(0.55f, php * 0.30f),
               "低血量 → 保持距离等冷却"};
    }
    return true;
}

// 优先级 2: 贴身普攻 (DPS 最高且冷却最短)
//
// ★ boss_can_attack 的语义澄清: 它表示"**Boss 能否反击**"(安全性
//   信号), **不是**"玩家能否攻击"。故它只影响"要不要贪刀"这一
//   决策的激进程度, 不作为普攻的前置条件 ——
//   初版误把它当玩家攻击条件, 导致 Boss 无法反击时反而不敢普攻。
bool TacticalPolicy::_rule_melee(const MirrorBattleState& st,
                                 TacticalIntent& out) const {
    if (!_cfg.enable_melee || st.dist_tiles > _cfg.melee_range) return false;
    // Boss 无法反击 = 安全输出窗口 → 更高置信; 否则略保守
    const float safety = st.boss_can_attack ? 0.0f : 0.06f;
    const float closeness = (1.0f - st.dist_tiles / _cfg.melee_range) * 0.09f;
    out = {PlayerActionType::ATTACK, 0.90f + safety + closeness,
           st.boss_can_attack ? "贴身 → 普攻(Boss 可反击, 需谨慎)"
                              : "贴身且 Boss 无法反击 → 安全普攻"};
    return true;
}

// 优先级 3: 技能就绪且在射程内 (血量越低越倾向用技能抢节奏)
bool TacticalPolicy::_rule_skill(const MirrorBattleState& st,
                                 TacticalIntent& out) const {
    if (!_cfg.enable_skill) return false;
    if (st.player_skills_ready <= 0) return false;
    if (st.dist_tiles > _cfg.skill_range) return false;
    if (st.player_hp_pct <= _cfg.skill_hp_floor) return false;
    const float urgency = 1.0f - st.player_hp_pct;
    out = {PlayerActionType::SKILL, score(0.75f, urgency * 0.12f),
           "技能就绪且在射程 → 释放"};
    return true;
}

// 优先级 4: 距离偏远 → 靠近 (而非空等); 技能就绪时更积极
bool TacticalPolicy::_rule_approach(const MirrorBattleState& st,
                                    TacticalIntent& out) const {
    if (!_cfg.enable_kite || st.dist_tiles <= _cfg.skill_range) return false;
    if (st.player_skills_ready > 0) {
        out = {PlayerActionType::MOVE, score(0.50f, 0.05f),
               "技能就绪但超出射程 → 靠近"};
    } else {
        out = {PlayerActionType::MOVE, score(0.40f, 0.05f),
               "超出所有射程 → 靠近"};
    }
    return true;
}

TacticalIntent TacticalPolicy::predict(const MirrorBattleState& st) const {
    TacticalIntent out;
    // 优先级顺序即求值顺序: 低血量走位 > 贴身普攻 > 技能 > 靠近
    if (_rule_flee(st, out)) return out;
    if (_rule_melee(st, out)) return out;
    if (_rule_skill(st, out)) return out;
    if (_rule_approach(st, out)) return out;
    // 兜底: 冷却中, 无可执行动作
    return {PlayerActionType::NONE, 0.20f, "冷却中 → 等待"};
}

}  // namespace mirror

namespace mirror {
bool install_tactical_predictor(MirrorAgent& agent,
                                const TacticalPolicy::Config& cfg) {
    // 用 shared_ptr 捕获, 保证 lambda 生命周期安全
    auto policy = std::make_shared<TacticalPolicy>(cfg);
    agent.set_ml_predictor([policy](const MirrorBattleState& st) {
        return policy->predict(st).action;
    });
    return true;
}

bool install_tactical_predictor_if_enabled(
    MirrorAgent& agent, const TacticalPolicy::Config& cfg) {
    if (!tactical_mirror_enabled()) return false;
    return install_tactical_predictor(agent, cfg);
}
}  // namespace mirror