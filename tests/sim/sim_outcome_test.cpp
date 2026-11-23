// sim 结算判定契约 — victory 必须排除超时/看门狗强制结算 (排除 F15 存活漏洞)
#include <gtest/gtest.h>

#include "core/sim/sim_outcome.h"

TEST(SimOutcome, NormalF15ClearIsVictory) {
    // 正常清完 F15 且存活, 无任何强制结算标志 → 胜利
    EXPECT_TRUE(is_sim_victory(15, true, false, false, 0));
}

TEST(SimOutcome, DeadAtF15IsNotVictory) {
    EXPECT_FALSE(is_sim_victory(15, false, false, false, 0));
}

TEST(SimOutcome, BelowF15IsNotVictory) {
    EXPECT_FALSE(is_sim_victory(14, true, false, false, 0));
}

TEST(SimOutcome, TimeoutSettlementAtF15IsNotVictory) {
    // 900s 游戏超时结算: F15 存活也不算胜利
    EXPECT_FALSE(is_sim_victory(15, true, true, false, 0));
    // 帧预算超时结算
    EXPECT_FALSE(is_sim_victory(15, true, false, true, 0));
}

TEST(SimOutcome, WatchdogSettlementAtF15IsNotVictory) {
    // 看门狗强制结算 (STUCK_RECOVERED): F15 存活不算胜利
    EXPECT_FALSE(is_sim_victory(15, true, false, false, 1));
}
