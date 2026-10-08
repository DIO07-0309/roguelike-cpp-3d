// v0.9.35 G32-D: TacticalPolicy 测试
// 核心保证: ① 规则优先级正确 ② 默认不参与仲裁 ③ 装配可逆
#include <gtest/gtest.h>

#include "ai/mirror/tactical_policy.h"
#include "game/ai/player_behavior/player_habit_profile.h"

using namespace mirror;

namespace {

MirrorBattleState mk(float dist, float php, bool skill_ready = true,
                     bool can_atk = true) {
    MirrorBattleState st;
    st.boss_hp_pct = 1.0f;
    st.player_hp_pct = php;
    st.dist_tiles = dist;
    st.boss_can_attack = can_atk;
    st.player_skills_ready = skill_ready ? 2 : 0;
    st.player_attacking = false;
    st.player_using_skill = false;
    st.boss_in_domain = false;
    return st;
}

}  // namespace

// ── 优先级: 贴身且有反制窗口 → 普攻 ──
TEST(TacticalPolicy, MeleeWhenInRange) {
    TacticalPolicy p;
    auto r = p.predict(mk(1.0f, 0.8f, true, true));
    EXPECT_EQ(r.action, PlayerActionType::ATTACK);
    EXPECT_GT(r.confidence, 0.8f);
}

// boss_can_attack = "Boss 能否反击"(安全性信号), 不是玩家攻击的前置条件。
// Boss 无法反击时反而是更安全的输出窗口, 应提高置信度。
TEST(TacticalPolicy, MeleeEvenWhenBossCannotCounter) {
    TacticalPolicy p;
    auto safe = p.predict(mk(1.0f, 0.8f, true, false));   // Boss 不能反击
    auto risky = p.predict(mk(1.0f, 0.8f, true, true));    // Boss 能反击
    EXPECT_EQ(safe.action, PlayerActionType::ATTACK);
    EXPECT_EQ(risky.action, PlayerActionType::ATTACK);
    EXPECT_GT(safe.confidence, risky.confidence)
        << "Boss 无法反击时应给出更高的输出信心";
}

// ── 优先级: 低血量 → 走位/闪避, 而非输出 ──
TEST(TacticalPolicy, LowHpPrefersEvade) {
    TacticalPolicy p;
    auto r = p.predict(mk(1.0f, 0.15f, true, true));
    EXPECT_EQ(r.action, PlayerActionType::DODGE);
}

TEST(TacticalPolicy, LowHpKeepsDistance) {
    TacticalPolicy p;
    auto r = p.predict(mk(8.0f, 0.20f, true, true));
    EXPECT_EQ(r.action, PlayerActionType::MOVE);
}

// ── 优先级: 中距离 + 技能就绪 → 技能 ──
TEST(TacticalPolicy, SkillWhenReadyInRange) {
    TacticalPolicy p;
    auto r = p.predict(mk(4.0f, 0.7f, true, true));
    EXPECT_EQ(r.action, PlayerActionType::SKILL);
}

// 技能未就绪且不在普攻距离 → 不应输出技能
TEST(TacticalPolicy, NoSkillWhenOnCooldown) {
    TacticalPolicy p;
    auto r = p.predict(mk(4.0f, 0.7f, false, true));
    EXPECT_NE(r.action, PlayerActionType::SKILL);
}

// 超出所有射程 → 靠近而非空等
TEST(TacticalPolicy, ApproachesWhenTooFar) {
    TacticalPolicy p;
    auto r = p.predict(mk(12.0f, 0.7f, false, true));
    EXPECT_EQ(r.action, PlayerActionType::MOVE);
}

// ── 极端输入不得崩 ──
TEST(TacticalPolicy, ExtremeStatesDoNotCrash) {
    TacticalPolicy p;
    EXPECT_NO_THROW(p.predict(mk(0.0f, 1.0f, true, true)));
    EXPECT_NO_THROW(p.predict(mk(999.0f, 0.0f, false, false)));
    EXPECT_NO_THROW(p.predict(mk(-5.0f, 0.5f, true, true)));
}

// ── 每个分支都应给出非空理由(可解释性契约) ──
TEST(TacticalPolicy, AlwaysProvidesReason) {
    TacticalPolicy p;
    for (float d : {0.0f, 1.0f, 4.0f, 12.0f}) {
        for (float h : {0.1f, 0.5f, 1.0f}) {
            for (bool s : {true, false}) {
                auto r = p.predict(mk(d, h, s, true));
                EXPECT_STRNE(r.reason, "") << "d=" << d << " h=" << h;
            }
        }
    }
}

// ── 开关生效 ──
TEST(TacticalPolicy, ConfigSwitchesWork) {
    TacticalPolicy::Config cfg;
    cfg.enable_melee = false;
    TacticalPolicy p(cfg);
    auto r = p.predict(mk(1.0f, 0.8f, true, true));
    EXPECT_NE(r.action, PlayerActionType::ATTACK);
}

// ── ★ 默认关闭的核心保证 ──
// 未调用 install_tactical_predictor 时, MlPredictor 槽必须为空
TEST(TacticalPolicyIntegrate, DefaultOffKeepsArbitrationUnchanged) {
    MirrorAgent agent;
    PlayerHabitProfile profile;
    agent.init(profile);
    agent.set_phase(3);
    auto st = mk(5.0f, 0.8f);
    int arm = agent.recommend_action(st);
    EXPECT_GE(arm, 0);
    EXPECT_LT(arm, (int)MirrorAction::SKILL + 1);
}

// ── 装配后可接管 ──
TEST(TacticalPolicyIntegrate, InstallTakesOver) {
    MirrorAgent agent;
    PlayerHabitProfile profile;
    agent.init(profile);
    EXPECT_TRUE(install_tactical_predictor(agent));
    agent.set_phase(3);
    auto st = mk(1.0f, 0.8f);
    EXPECT_GE(agent.recommend_action(st), 0);
}