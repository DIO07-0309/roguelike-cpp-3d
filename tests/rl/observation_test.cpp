// G8.4: Observation tests
#include <gtest/gtest.h>
#include "ai/rl/observation.h"
#include "ai/mcts/simulation_state.h"

using namespace rl;
using namespace mcts;

TEST(Observation, FixedLengthVector) {
    SimulationState s;
    s.player.hp = 80; s.player.max_hp = 100;
    s.player.attack = 10;
    s.monsters.push_back(MonsterSnapshot{"orc", 30, 30, 5, 0, 7, 3, 1, {}, true});
    auto obs = Observation::from_state(s);
    auto vec = obs.to_vector();
    EXPECT_EQ(vec.size(), 14u); // 14 features (8 base + position/cooldown)
    EXPECT_NEAR(vec[0], 0.8f, 0.01f);  // hp_ratio
    EXPECT_FLOAT_EQ(vec[1], 10.0f);    // attack
    EXPECT_FLOAT_EQ(vec[2], 1.0f);     // enemy_count
}

TEST(Observation, KeyIsDeterministic) {
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    auto obs = Observation::from_state(s);
    auto k1 = obs.to_key();
    auto k2 = obs.to_key();
    EXPECT_EQ(k1, k2);
}

TEST(Observation, KeyChangesWithHP) {
    SimulationState s1, s2;
    s1.player.hp = 100; s1.player.max_hp = 100;
    s2.player.hp = 50;  s2.player.max_hp = 100;
    auto k1 = Observation::from_state(s1).to_key();
    auto k2 = Observation::from_state(s2).to_key();
    EXPECT_NE(k1, k2);
}

TEST(Observation, BossPresentFlag) {
    SimulationState s;
    s.player.alive = true;
    s.monsters.push_back(MonsterSnapshot{"boss", 200, 200, 3, 0, 25, 10, 8, {}, true, true});
    auto obs = Observation::from_state(s);
    EXPECT_FLOAT_EQ(obs.boss_present, 1.0f);
}

TEST(Observation, EmptyStateDefaults) {
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    auto obs = Observation::from_state(s);
    EXPECT_FLOAT_EQ(obs.enemy_count, 0);
    EXPECT_FLOAT_EQ(obs.nearest_enemy_dist, 999.0f);
}

// ═══ v0.9.36 G32-E 回归测试 ═══
// strongest_hp_ratio / buff_count 旧实现未参与 to_key,
// 仅在这两项上不同的状态会撞进同一 Q 表条目。

TEST(Observation, KeySeparatesStrongestHpRatio) {
    Observation a, b;
    a.player_hp_ratio = 0.5f; a.enemy_count = 1; a.nearest_enemy_dist = 3;
    b = a;
    EXPECT_EQ(a.to_key(), b.to_key());
    b.strongest_hp_ratio = 0.4f;
    EXPECT_NE(a.to_key(), b.to_key());
}

TEST(Observation, KeySeparatesBuffCount) {
    Observation a, b;
    a.player_hp_ratio = 0.5f; a.enemy_count = 1; a.nearest_enemy_dist = 3;
    b = a;
    b.buff_count = 2;
    EXPECT_NE(a.to_key(), b.to_key());
}

// buff_count 上限截断 — 防止键长随 buff 数量无界增长
TEST(Observation, KeyClampsBuffCount) {
    Observation a, b;
    a.player_hp_ratio = 0.5f; a.enemy_count = 1; a.nearest_enemy_dist = 3;
    a.buff_count = 9;
    b = a;
    b.buff_count = 40;
    EXPECT_EQ(a.to_key(), b.to_key());
}

// G32-C: 位置 + 冷却维度必须进 to_vector (否则 DQN 会陷"状态冻结"死循环)
TEST(Observation, VectorCarriesPositionAndCooldown) {
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.x = 20; s.player.y = 15;
    s.player.attack_cooldown = 1.0f;
    s.player.skill_cooldowns[0] = 0; s.player.skill_cooldowns[1] = 0;
    s.player.skill_cooldowns[2] = 1; s.player.skill_cooldowns[3] = 1;
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 24, 18, 3, 1, 0, {}, true});
    auto v = Observation::from_state(s).to_vector();
    ASSERT_EQ(v.size(), (size_t)Observation::kVectorDim);
    EXPECT_NEAR(v[8], 20.0f / 40.0f, 1e-5);   // player_x 归一化
    EXPECT_NEAR(v[9], 15.0f / 30.0f, 1e-5);   // player_y
    EXPECT_NEAR(v[12], 0.5f, 1e-5);           // attack_cooldown 1.0/2.0
    EXPECT_NEAR(v[13], 0.5f, 1e-5);           // 2/4 技能就绪
}
