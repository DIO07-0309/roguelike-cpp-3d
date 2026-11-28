// P6: CombatFocusTracker 纯值契约测试 — 清怪僵持检测。
// 锁死: 长时间无怪掉血→进入纯追怪(hunting), 任何战斗进展/换层归零→重置。
// 纯值输入(hp_sum/alive/game_time), 无 game 依赖。
#include <gtest/gtest.h>
#include "core/sim/combat_focus_tracker.h"
#include "core/sim/sim_redesign_flags.h"
#include "core/sim/sim_tuning.h"

// P3-redesign: floor_hunting 锁存 — redesign active 时僵持触发后保持到清层,
// 不因零星怪掉血解除 (原 hunting 契约仍由上面的测试锁定, 二者并存).
TEST(CombatFocusTracker, FloorHuntingLatchesUntilCleared) {
    const float k = sim_tuning::hunt_threshold();  // 跟随 env (防外部 SIM_TUNE_HUNT 干扰)
    sim_redesign_set_active(true);
    sim_redesign_flags_mut().hunt_lock = true;
    CombatFocusTracker f;
    f.observe(100.0f, 3, 0.0f);
    for (int t = 1; t <= (int)k + 1; ++t) f.observe(100.0f, 3, (float)t);
    EXPECT_TRUE(f.floor_hunting()) << "僵持触发应锁存";
    f.observe(90.0f, 3, k + 2.0f);            // 零星掉血: 原 hunting 退出
    EXPECT_FALSE(f.hunting());
    EXPECT_TRUE(f.floor_hunting()) << "锁存不因零星掉血解除";
    f.observe(0.0f, 0, k + 3.0f);             // 清层
    EXPECT_FALSE(f.floor_hunting()) << "清空后解锁";
    sim_redesign_set_active(false);            // 复原全局标志
}

TEST(CombatFocusTracker, FloorHuntingResetsOnFloorChange) {
    const float k = sim_tuning::hunt_threshold();  // 跟随 env
    sim_redesign_set_active(true);
    sim_redesign_flags_mut().hunt_lock = true;
    CombatFocusTracker f;
    f.observe(100.0f, 3, 50.0f);
    for (int t = 51; t <= 50 + (int)k + 1; ++t) f.observe(100.0f, 3, (float)t);
    EXPECT_TRUE(f.floor_hunting());
    f.observe(100.0f, 3, 0.0f);               // game_time 回退 = 换层
    EXPECT_FALSE(f.floor_hunting()) << "换层解锁";
    sim_redesign_set_active(false);
}

TEST(SimRedesignFlags, DefaultsAllTrue) {
    // per-change 开关默认全开 (协同方案)
    const auto& fl = sim_redesign_flags();
    EXPECT_TRUE(fl.standstill_fix);
    EXPECT_TRUE(fl.loot_suppress);
    EXPECT_TRUE(fl.progress_tighten);
    EXPECT_TRUE(fl.hunt_lock);
    EXPECT_TRUE(fl.numeric);
}

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
