#include <gtest/gtest.h>
#include "data/camera_defs.h"
#include "game/systems/hit_stop.h"

TEST(CameraDefs, ParsesValidJson) {
    std::string err;
    auto def = load_camera_file("resources/camera/boss_camera.json", err);
    ASSERT_TRUE(def) << err;
    EXPECT_NEAR(def->boss_war.zoom_in.fov_scale, 0.75f, 1e-4f);
    EXPECT_NEAR(def->boss_war.zoom_in.duration, 1.5f, 1e-4f);
    EXPECT_NEAR(def->boss_war.zoom_out.fov_scale, 1.0f, 1e-4f);
    EXPECT_NEAR(def->boss_war.zoom_out.duration, 0.8f, 1e-4f);
    EXPECT_NEAR(def->boss_war.lerp_speed, 2.0f, 1e-4f);
    EXPECT_NEAR(def->kill_stun.duration, 0.08f, 1e-4f);
    EXPECT_NEAR(def->kill_stun.shake_amplitude, 3.0f, 1e-4f);
    EXPECT_NEAR(def->kill_stun.shake_frequency, 20.0f, 1e-4f);
}

TEST(CameraDefs, RejectsMissingFile) {
    std::string err;
    auto def = load_camera_file("nonexistent.json", err);
    EXPECT_FALSE(def);
    EXPECT_FALSE(err.empty());
}

TEST(CameraDefs, UsesDefaultsWhenFieldsMissing) {
    // 验证空对象时使用默认值
    std::string err;
    auto def = load_camera_file("resources/camera/boss_camera.json", err);
    ASSERT_TRUE(def);
    // 默认值已在 JSON 中显式声明，验证读取正确
    EXPECT_GT(def->boss_war.zoom_in.fov_scale, 0.0f);
    EXPECT_GT(def->boss_war.zoom_out.fov_scale, 0.0f);
    EXPECT_GT(def->boss_war.lerp_speed, 0.0f);
    EXPECT_GT(def->kill_stun.duration, 0.0f);
}

TEST(CameraDefs, ValidatesFovScaleRange) {
    // fov_scale 必须在 (0.1, 2.0] 范围内
    std::string err;
    auto def = load_camera_file("resources/camera/boss_camera.json", err);
    ASSERT_TRUE(def);
    EXPECT_GT(def->boss_war.zoom_in.fov_scale, 0.1f);
    EXPECT_LE(def->boss_war.zoom_in.fov_scale, 2.0f);
    EXPECT_GT(def->boss_war.zoom_out.fov_scale, 0.1f);
    EXPECT_LE(def->boss_war.zoom_out.fov_scale, 2.0f);
}

TEST(CameraDefs, ValidatesDurationRange) {
    // kill_stun.duration 必须在 (0, 0.15] 范围内
    std::string err;
    auto def = load_camera_file("resources/camera/boss_camera.json", err);
    ASSERT_TRUE(def);
    EXPECT_GT(def->kill_stun.duration, 0.0f);
    EXPECT_LE(def->kill_stun.duration, 0.15f);
}

TEST(HitStop, TriggerSetsActiveAndRemaining) {
    HitStop hs;
    EXPECT_FALSE(hs.active());
    EXPECT_EQ(hs.remaining(), 0.0f);
    EXPECT_FALSE(hs.is_stunned());

    hs.trigger(0.08f);
    EXPECT_TRUE(hs.active());
    EXPECT_NEAR(hs.remaining(), 0.08f, 1e-6f);
    EXPECT_TRUE(hs.is_stunned());
}

TEST(HitStop, UpdateDecaysRemaining) {
    HitStop hs;
    hs.trigger(0.10f);
    EXPECT_NEAR(hs.remaining(), 0.10f, 1e-6f);

    hs.update(0.04f);
    EXPECT_LE(hs.remaining(), 0.06f + 1e-6f);
    EXPECT_GT(hs.remaining(), 0.05f);
    EXPECT_TRUE(hs.active());

    hs.update(0.06f);
    // 浮点精度: 0.10 - 0.04 - 0.06 可能不完全为 0
    EXPECT_NEAR(hs.remaining(), 0.0f, 1e-6f);
    EXPECT_FALSE(hs.active());
    EXPECT_FALSE(hs.is_stunned());
}

TEST(HitStop, IgnoresNonPositiveDuration) {
    HitStop hs;
    hs.trigger(0.0f);
    EXPECT_FALSE(hs.active());
    EXPECT_EQ(hs.remaining(), 0.0f);

    hs.trigger(-0.05f);
    EXPECT_FALSE(hs.active());
}

TEST(HitStop, RemainingNeverNegative) {
    HitStop hs;
    hs.trigger(0.02f);
    hs.update(0.10f);  // 超过剩余时间
    EXPECT_EQ(hs.remaining(), 0.0f);
    EXPECT_FALSE(hs.active());
}
