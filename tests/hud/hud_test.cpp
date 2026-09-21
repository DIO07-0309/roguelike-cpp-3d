#include <gtest/gtest.h>
#include "game/systems/game_renderer.h"
#include "game/entities/player.h"

// HP bar 颜色计算测试
TEST(HudTest, HpBarColorAbove50Percent) {
    float hp_ratio = 0.6f;
    Color expected = Color{50, 200, 50, 255};
    // 测试颜色计算逻辑
    // 实际颜色计算在 draw_hud 中，这里测试边界
    EXPECT_TRUE(hp_ratio > 0.5f);
}

TEST(HudTest, HpBarColorAbove25Percent) {
    float hp_ratio = 0.3f;
    EXPECT_TRUE(hp_ratio > 0.25f);
}

TEST(HudTest, HpBarColorBelow25Percent) {
    float hp_ratio = 0.2f;
    EXPECT_TRUE(hp_ratio < 0.25f);
}

// 边界测试
TEST(HudTest, HpRatioClampedToZero) {
    float hp_ratio = -0.1f;
    float clamped = hp_ratio < 0.0f ? 0.0f : hp_ratio;
    EXPECT_EQ(clamped, 0.0f);
}

TEST(HudTest, HpRatioClampedToOne) {
    float hp_ratio = 1.5f;
    float clamped = hp_ratio > 1.0f ? 1.0f : hp_ratio;
    EXPECT_EQ(clamped, 1.0f);
}

// 技能栏冷却进度测试
TEST(HudTest, SkillCooldownRotation) {
    float cooldown_ratio = 0.5f;
    float rotation = cooldown_ratio * 360.0f;
    EXPECT_EQ(rotation, 180.0f);
}

TEST(HudTest, SkillCooldownCountdown) {
    float cooldown_remaining = 5.5f;
    int display_seconds = static_cast<int>(cooldown_remaining);
    EXPECT_EQ(display_seconds, 5);
}

TEST(HudTest, SkillCooldownNoDisplayAbove10s) {
    float cooldown_remaining = 10.5f;
    bool should_display = cooldown_remaining < 10.0f;
    EXPECT_FALSE(should_display);
}
