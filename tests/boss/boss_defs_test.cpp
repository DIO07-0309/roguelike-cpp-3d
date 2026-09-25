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

TEST_F(BossDefsTest, GolemShockwaveAtSkillIndexOne) {
    const BossDef* g = get_boss_def("golem");
    ASSERT_NE(g, nullptr);
    ASSERT_EQ(g->skill_overrides.size(), 3u);
    EXPECT_EQ(g->skill_overrides[0].id, "charge");
    EXPECT_EQ(g->skill_overrides[1].id, "shockwave");
    EXPECT_EQ(g->skill_overrides[2].id, "barrage");
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
