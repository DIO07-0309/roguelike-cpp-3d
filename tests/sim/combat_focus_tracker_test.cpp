// P6: CombatFocusTracker 纯值契约测试 — 清怪僵持检测。
// 锁死: 长时间无怪掉血→进入纯追怪(hunting), 任何战斗进展/换层归零→重置。
// 纯值输入(hp_sum/alive/game_time), 无 game 依赖。
#include <gtest/gtest.h>
#include "core/sim/combat_focus_tracker.h"

TEST(CombatFocusTracker, HuntingAfterNoDamage) {
    constexpr float k = CombatFocusTracker::kHuntThresholdSec;
    CombatFocusTracker f;
    f.observe(100.0f, 3, 0.0f);   // 锚定
    for (int t = 1; t < (int)k; ++t)
        f.observe(100.0f, 3, (float)t);
    EXPECT_FALSE(f.hunting()) << "阈值-1s 不该进入纯追怪";
    f.observe(100.0f, 3, k);       // 精确边界 t=阈值
    EXPECT_TRUE(f.hunting()) << "恰好达阈值应进入纯追怪(>=)";
}

TEST(CombatFocusTracker, ResetsOnDamage) {
    CombatFocusTracker f;
    f.observe(100.0f, 3, 0.0f);
    for (int t = 1; t <= (int)CombatFocusTracker::kHuntThresholdSec + 1; ++t)
        f.observe(100.0f, 3, (float)t);
    ASSERT_TRUE(f.hunting());
    f.observe(90.0f, 3, CombatFocusTracker::kHuntThresholdSec + 2.0f);   // 怪 HP 下降 → 有战斗进展
    EXPECT_FALSE(f.hunting()) << "怪掉血应退出纯追怪";
}

TEST(CombatFocusTracker, ResetsOnMonsterDeath) {
    CombatFocusTracker f;
    f.observe(100.0f, 3, 0.0f);
    for (int t = 1; t <= (int)CombatFocusTracker::kHuntThresholdSec + 1; ++t)
        f.observe(100.0f, 3, (float)t);
    ASSERT_TRUE(f.hunting());
    f.observe(100.0f, 2, CombatFocusTracker::kHuntThresholdSec + 2.0f);   // 存活数降 → 有战斗进展
    EXPECT_FALSE(f.hunting()) << "怪死亡应退出纯追怪";
}

TEST(CombatFocusTracker, ResetsOnTimeRollback) {
    CombatFocusTracker f;
    f.observe(100.0f, 3, 50.0f);
    f.observe(100.0f, 3, 0.0f);    // game_time 回退(换层归零) → 内部重置
    EXPECT_FALSE(f.hunting());
    f.observe(100.0f, 3, CombatFocusTracker::kHuntThresholdSec - 1.0f);
    EXPECT_FALSE(f.hunting()) << "重置后 7s 未达阈值";
    f.observe(100.0f, 3, CombatFocusTracker::kHuntThresholdSec + 1.0f);
    EXPECT_TRUE(f.hunting()) << "重置后满 8s 才进入";
}

TEST(CombatFocusTracker, NoHuntWhenNoMonsters) {
    CombatFocusTracker f;
    for (int t = 0; t <= 30; ++t)
        f.observe(0.0f, 0, (float)t);   // 无存活怪
    EXPECT_FALSE(f.hunting()) << "无怪不该进入纯追怪(应走清层/楼梯逻辑)";
}

TEST(CombatFocusTracker, ResetClearsState) {
    CombatFocusTracker f;
    f.observe(100.0f, 3, 0.0f);
    for (int t = 1; t <= (int)CombatFocusTracker::kHuntThresholdSec + 1; ++t)
        f.observe(100.0f, 3, (float)t);
    ASSERT_TRUE(f.hunting());
    f.reset();
    EXPECT_FALSE(f.hunting());
    f.observe(100.0f, 3, CombatFocusTracker::kHuntThresholdSec + 92.0f);
    EXPECT_FALSE(f.hunting()) << "reset 后锚定重新计时";
}
