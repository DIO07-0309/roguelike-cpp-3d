// G8.4: QAgent tests
#include <gtest/gtest.h>
#include <cstdio>
#include <vector>
#include "ai/rl/q_agent.h"
#include "ai/rl/observation.h"
#include "ai/mcts/simulation_state.h"
#include "ai/mcts/action.h"

using namespace rl;
using namespace mcts;

TEST(QAgent, SelectReturnsValidAction) {
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.x = 5; s.player.y = 5;
    s.player.attack = 10;
    s.player.alive = true;
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 3, 0, 3, 1, 0, {}, true});
    QAgent qa(0.1, 0.9, 0.1);
    uint32_t seed = 42;
    auto action = qa.select(s, seed);
    EXPECT_GE((int)action, 0);
    EXPECT_LT((int)action, (int)CombatAction::COUNT);
}

TEST(QAgent, EmptyTableSize) {
    QAgent qa;
    EXPECT_EQ(qa.table_size(), 0u);
}

TEST(QAgent, UpdateIncreasesTable) {
    QAgent qa;
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.alive = true;
    auto obs = Observation::from_state(s);
    qa.update(obs, CombatAction::ATTACK, 10.0, obs, get_possible_actions(s));
    EXPECT_GT(qa.table_size(), 0u);
}

TEST(QAgent, QValueConverges) {
    QAgent qa(0.5, 0.9, 0.0); // epsilon=0, always greedy
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.x = 5; s.player.y = 5;
    s.player.alive = true;
    // Adjacent enemy so ATTACK is a legal action in this state
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 5, 6, 3, 1, 0, {}, true});
    auto obs = Observation::from_state(s);

    // Repeated updates with positive reward should increase Q for attack
    for (int i = 0; i < 20; i++)
        qa.update(obs, CombatAction::ATTACK, 5.0, obs, get_possible_actions(s));

    uint32_t seed = 42;
    auto action = qa.select(s, seed);
    // With epsilon=0 and many positive updates to ATTACK, should select ATTACK
    EXPECT_EQ(action, CombatAction::ATTACK);
}

TEST(QAgent, EpsilonExploration) {
    QAgent qa(0.1, 0.9, 1.0); // epsilon=1.0, always explore
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.alive = true;
    s.player.x = 5; s.player.y = 5;
    s.player.attack = 10;
    s.player.alive = true;
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 3, 0, 3, 1, 0, {}, true});
    uint32_t seed = 42;
    // With epsilon=1.0, should pick various actions over multiple calls
    std::vector<int> seen((int)CombatAction::COUNT, 0);
    for (int i = 0; i < 50; i++) {
        auto a = qa.select(s, seed);
        seen[(int)a]++;
    }
    int non_zero = 0;
    for (auto c : seen) if (c > 0) non_zero++;
    EXPECT_GT(non_zero, 1); // should explore at least 2 different actions
}

TEST(QAgent, Hyperparameters) {
    QAgent qa(0.1, 0.95, 0.2);
    EXPECT_DOUBLE_EQ(qa.alpha(), 0.1);
    EXPECT_DOUBLE_EQ(qa.gamma(), 0.95);
    EXPECT_DOUBLE_EQ(qa.epsilon(), 0.2);
}

// ═══ v0.9.36 G32-E 回归测试 ═══
// 每条都针对一个已修缺陷, 且**对旧实现反向验证会失败**。

// G32-E-B2: exploit_action 必须只返回合法动作
// 旧实现遍历全部动作 → 只给 ATTACK 高 Q 时, 限制合法集为 WAIT 仍会返回 ATTACK
TEST(QAgent, ExploitStaysInsideLegalSet) {
    QAgent qa(0.5, 0.9, 0.0);
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.x = 5; s.player.y = 5; s.player.attack = 10; s.player.alive = true;
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 5, 6, 3, 1, 0, {}, true});
    auto obs = Observation::from_state(s);
    auto legal = get_possible_actions(s);
    qa.update(obs, CombatAction::ATTACK, 100.0, obs, legal, true);  // 高 Q
    qa.update(obs, CombatAction::WAIT, 1.0, obs, legal, true);      // 低 Q 但已学
    // 合法集只剩 WAIT → 必须返回 WAIT
    std::vector<CombatAction> only_wait{CombatAction::WAIT};
    EXPECT_EQ(qa.exploit_action(obs, only_wait), (int)CombatAction::WAIT);
}

// G32-E-B2: 合法集为空 → 不接管
TEST(QAgent, ExploitRejectsEmptyLegalSet) {
    QAgent qa;
    SimulationState s;
    s.player.alive = true;
    auto obs = Observation::from_state(s);
    qa.update(obs, CombatAction::WAIT, 5.0, obs, get_possible_actions(s), true);
    EXPECT_EQ(qa.exploit_action(obs, {}), -1);
}

// G32-E-B2: 未见过的状态仍返回 -1 (置信度感知降级的前提)
TEST(QAgent, ExploitUnknownStateReturnsMinusOne) {
    QAgent qa;
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100; s.player.alive = true;
    auto obs = Observation::from_state(s);
    EXPECT_EQ(qa.exploit_action(obs, get_possible_actions(s)), -1);
}

// G32-E-B1: bootstrap 必须被 next_legal 限制
// 若错误地把非法动作(此处 ATTACK, Q=10)算进 max, WAIT 的 Q 会变成 4.5
TEST(QAgent, BootstrapIgnoresActionsOutsideNextLegal) {
    QAgent qa(0.5, 0.9, 0.0);
    Observation o;
    o.player_hp_ratio = 0.5f; o.enemy_count = 1; o.nearest_enemy_dist = 3;
    auto legal = std::vector<CombatAction>{CombatAction::ATTACK, CombatAction::WAIT};
    qa.update(o, CombatAction::ATTACK, 10.0, o, legal, true);
    qa.update(o, CombatAction::WAIT, 0.0, o, {CombatAction::WAIT}, false);
    for (auto& d : qa.action_distribution())
        if (d.action_name == "wait") EXPECT_NEAR(d.avg_q, 0.0, 1e-6);
}

// G32-E-B3: 旧版存档必须被拒绝(to_key 键格式已变, 混读会错位)
TEST(QAgent, LoadRejectsLegacyVersion) {
    const char* path = "saves/_test_legacy_qtable.json";
    FILE* f = fopen(path, "w");
    ASSERT_TRUE(f != nullptr);
    fputs("{\"q\": {\"5:2:1:1:1:0|0\": 1.0}}", f);
    fclose(f);
    QAgent qa;
    EXPECT_FALSE(qa.load(path));
    EXPECT_EQ(qa.table_size(), 0u);
    remove(path);
}
