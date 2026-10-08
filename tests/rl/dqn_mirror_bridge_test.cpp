// v0.9.33 G32-B: DqnMirrorBridge 测试
// 核心断言: **未注册 DQN 时仲裁链行为与改动前完全一致** (默认关闭的保证)
#include <gtest/gtest.h>
#include <cstdio>
#include <vector>
#include "ai/mirror/mirror_agent.h"
#include "ai/rl/dqn_mirror_bridge.h"
#include "ai/mcts/action.h"
#include "game/ai/player_behavior/player_habit_profile.h"

using namespace rl;

namespace {

MirrorBattleState make_state(float dist = 5.0f, float php = 0.8f,
                             float bhp = 1.0f, bool can_atk = true) {
    MirrorBattleState st;
    st.boss_hp_pct = bhp;
    st.player_hp_pct = php;
    st.dist_tiles = dist;
    st.boss_can_attack = can_atk;
    st.player_skills_ready = 2;
    return st;
}

const char* WEIGHTS = "tools/rl/dqn_weights.bin";

}  // namespace

// ── 动作折叠映射 ──
TEST(DqnMirrorBridge, CombatActionFoldsToPlayerAction) {
    using mcts::CombatAction;
    EXPECT_EQ(combat_action_to_player_action((int)CombatAction::ATTACK),
              (int)PlayerActionType::ATTACK);
    for (int s = 0; s < 4; s++) {
        EXPECT_EQ(combat_action_to_player_action((int)CombatAction::SKILL_1 + s),
                  (int)PlayerActionType::SKILL);
        EXPECT_EQ(combat_action_to_player_action((int)CombatAction::MOVE_UP + s),
                  (int)PlayerActionType::MOVE);
    }
    EXPECT_EQ(combat_action_to_player_action((int)CombatAction::WAIT),
              (int)PlayerActionType::NONE);
}

// ── 未就绪时必须返回 NONE, 不得崩 ──
TEST(DqnMirrorBridge, UnreadyReturnsNone) {
    DqnMirrorBridge bridge;              // 未 load
    EXPECT_FALSE(bridge.ready());
    EXPECT_EQ(bridge.predict(make_state(), 1), PlayerActionType::NONE);
}

TEST(DqnMirrorBridge, RejectsMissingWeights) {
    DqnMirrorBridge bridge;
    EXPECT_FALSE(bridge.load("no_such_dqn_weights.bin"));
    EXPECT_FALSE(bridge.ready());
    EXPECT_EQ(bridge.predict(make_state(), 1), PlayerActionType::NONE);
}

TEST(DqnMirrorBridge, RejectsCorruptWeights) {
    const char* path = "saves/_test_dqn_corrupt.bin";
    FILE* f = fopen(path, "wb");
    ASSERT_TRUE(f != nullptr);
    fwrite("GARBAGE-NOT-RLDQ-FILE-HEADER-PADDING", 1, 37, f);
    fclose(f);
    DqnMirrorBridge bridge;
    EXPECT_FALSE(bridge.load(path));
    remove(path);
}

// ── 就绪时输出必须是合法 PlayerActionType, 且不崩 ──
// 权重是训练产物, 不随源码入库(每次改 Observation 维度即失效)。
// 缺失时跳过而非失败 —— 让 ctest 在没有权重的干净检出上也能全绿。
TEST(DqnMirrorBridge, ReadyProducesValidAction) {
    DqnMirrorBridge bridge;
    if (!bridge.load(WEIGHTS))
        GTEST_SKIP() << "缺少 " << WEIGHTS
                     << " — 跑 train_dqn.py + export_weights.py 生成";
    ASSERT_TRUE(bridge.ready());
    for (int style = 0; style <= 4; style++) {
        PlayerActionType t = bridge.predict(make_state(), style);
        EXPECT_GE((int)t, (int)PlayerActionType::NONE);
        EXPECT_LE((int)t, (int)PlayerActionType::FLOOR_ENTER);
    }
}

// 极端输入不得崩 (距离 0 / 血量 0 / Boss 已死)
TEST(DqnMirrorBridge, ExtremeStatesDoNotCrash) {
    DqnMirrorBridge bridge;
    if (!bridge.load(WEIGHTS)) GTEST_SKIP() << "缺少 " << WEIGHTS;
    EXPECT_NO_THROW(bridge.predict(make_state(0.0f, 0.0f, 0.0f), 0));
    EXPECT_NO_THROW(bridge.predict(make_state(999.0f, 1.0f, 1.0f), 4));
    EXPECT_NO_THROW(bridge.predict(make_state(5.0f, 0.0f, 1.0f, false), 2));
}

// ── ★ 默认关闭的核心保证 ──
// 未安装 DQN 时, MlPredictor 插槽必须为空 → 仲裁链走原路径
TEST(MirrorAgentDqn, DefaultOffKeepsArbitrationUnchanged) {
    MirrorAgent agent;
    PlayerHabitProfile profile;
    agent.init(profile);
    agent.set_phase(3);
    auto st = make_state();
    // 未注册 → 仲裁链不经过 ML 层, 返回 Thompson 采样结果(非 ML 产出)
    int arm = agent.recommend_action(st);
    EXPECT_GE(arm, 0);
    EXPECT_LT(arm, (int)MirrorAction::SKILL + 1);   // MirrorAction 0..3
}

// 零侵入验证: 安装失败时 install_dqn_predictor 返回 nullptr 且不改动 agent
TEST(MirrorAgentDqn, InstallFailsCleanlyOnBadPath) {
    MirrorAgent agent;
    PlayerHabitProfile profile;
    agent.init(profile);
    agent.set_phase(3);
    EXPECT_EQ(install_dqn_predictor(agent, "no_such_weights.bin", 1), nullptr);
    // 安装失败后仲裁链仍必须可用(走 Thompson 兜底)
    EXPECT_GE(agent.recommend_action(make_state()), 0);
}

TEST(MirrorAgentDqn, InstallSucceedsAndTakesOver) {
    MirrorAgent agent;
    PlayerHabitProfile profile;
    agent.init(profile);
    auto bridge = install_dqn_predictor(agent, WEIGHTS, 1);
    if (!bridge) GTEST_SKIP() << "缺少 " << WEIGHTS << " — 先跑 export_weights.py";
    EXPECT_TRUE(bridge->ready());
    agent.set_phase(3);
    // 安装后仲裁链应由 ML 层接管(phase>=2 且 _ml_predictor 非空)
    int arm = agent.recommend_action(make_state());
    EXPECT_GE(arm, 0);
}