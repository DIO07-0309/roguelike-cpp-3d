#include <algorithm>
#include <gtest/gtest.h>
#include "boss_defs.h"

namespace {
class BossDefsTest : public ::testing::Test {
protected:
    void SetUp() override { load_boss_defs("resources/bosses.json"); }
};

TEST_F(BossDefsTest, GolemDefLoads) {
    const BossDef* g = get_boss_def("golem");
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->id, "golem");
    EXPECT_EQ(g->visual_id, "golem");
}

TEST_F(BossDefsTest, GolemIsDefenderWithActiveShield) {
    const BossDef* g = get_boss_def("golem");
    ASSERT_NE(g, nullptr);
    EXPECT_TRUE(g->is_defender);
    EXPECT_GT(g->shield_pct, 0.0f);
    EXPECT_FALSE(g->is_summoner);
    EXPECT_TRUE(g->skill_overrides.size() == 3u);
}

static bool has_summon_skill(const BossDef* g) {
    for (const auto& s : g->skill_overrides) if (s.id == "summon") return true;
    return false;
}

// Skill overrides are applied by ID-MATCH at boss.cpp:1240-1257
// (`sk.id == "charge"` -> ai->_charge cooldown/damage/windup/range).
// Array position is irrelevant; a missing entry leaves that skill's
// params at compile-time defaults.
TEST_F(BossDefsTest, GolemHasChargeAndShockwaveSkillEntries) {
    const BossDef* g = get_boss_def("golem");
    ASSERT_NE(g, nullptr);
    auto has = [&](const std::string& id) {
        for (const auto& s : g->skill_overrides) if (s.id == id) return true;
        return false;
    };
    EXPECT_TRUE(has("charge"));
    EXPECT_TRUE(has("shockwave"));
    EXPECT_TRUE(has("barrage"));
}

// Whether the boss summons is decided SOLELY by skill_cycle_bias:
// _next_cycle_skill (boss.cpp:425-435) returns Summon only when
// cycle_len==4 (idx3) or cycle_len==6 (idx4). `is_summoner` is used
// only for display strings (boss.cpp:1145, 1278) and never gates it.
TEST_F(BossDefsTest, GolemCycleBiasNeverSummons) {
    const BossDef* g = get_boss_def("golem");
    ASSERT_NE(g, nullptr);
    EXPECT_NE(g->skill_cycle_bias, 4);   // idx3 -> Summon
    EXPECT_NE(g->skill_cycle_bias, 6);   // idx4 -> Summon
    EXPECT_FALSE(has_summon_skill(g));   // belt-and-braces
}

TEST_F(BossDefsTest, GolemComboCommandsAreLegal) {
    const BossDef* g = get_boss_def("golem");
    ASSERT_NE(g, nullptr);
    ASSERT_FALSE(g->combos.empty());
    const std::vector<std::string> ok = {"normal", "charge", "shockwave", "summon",
                                         "defend", "barrage", "cone", "blink", "whirlwind"};
    for (const auto& c : g->combos)
        for (const auto& cmd : c.commands) {
            EXPECT_NE(cmd, "") << "empty command in combo " << c.id;
            EXPECT_TRUE(std::find(ok.begin(), ok.end(), cmd) != ok.end())
                << "illegal combo command: " << cmd;
        }
}

TEST_F(BossDefsTest, BossTypeGolemResolves) {
    const BossDef* d = get_boss_def_for_type(4);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->id, "golem");
}

TEST_F(BossDefsTest, FloorTenStillMapsToFireDemon) {
    const BossDef* d = get_boss_def_for_floor(10);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->id, "fire_demon");
}
}  // namespace
