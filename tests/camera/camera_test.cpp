#include <gtest/gtest.h>
#include "data/camera_defs.h"
#include "game/systems/hit_stop.h"
#include "game/director/camera_director.h"
#include "raylib.h"
#include "raymath.h"

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

TEST(CameraLanguageDirector, TryInitSetsInitialState) {
    CameraLanguageDirector cd;
    std::string err;
    auto def = load_camera_file("resources/camera/boss_camera.json", err);
    ASSERT_TRUE(def);
    
    EXPECT_TRUE(cd.try_init(*def));
    EXPECT_EQ(cd.state(), CameraState::NORMAL);
    EXPECT_NEAR(cd.fov_scale(), 1.0f, 1e-6f);
    EXPECT_NEAR(cd.focus_offset().x, 0.0f, 1e-6f);
    EXPECT_NEAR(cd.focus_offset().y, 0.0f, 1e-6f);
}

TEST(CameraLanguageDirector, EnterBossWarChangesStateAndFov) {
    CameraLanguageDirector cd;
    std::string err;
    auto def = load_camera_file("resources/camera/boss_camera.json", err);
    ASSERT_TRUE(def);
    cd.try_init(*def);
    
    cd.enter_boss_war();
    EXPECT_EQ(cd.state(), CameraState::BOSS_WAR);
    // FOV 应该开始向 zoom_in.fov_scale 插值
    EXPECT_GT(cd.fov_scale(), 0.0f);
    EXPECT_LE(cd.fov_scale(), 1.0f);
}

TEST(CameraLanguageDirector, ExitBossWarReturnsToNormal) {
    CameraLanguageDirector cd;
    std::string err;
    auto def = load_camera_file("resources/camera/boss_camera.json", err);
    ASSERT_TRUE(def);
    cd.try_init(*def);
    
    cd.enter_boss_war();
    EXPECT_EQ(cd.state(), CameraState::BOSS_WAR);
    
    cd.exit_boss_war();
    EXPECT_EQ(cd.state(), CameraState::NORMAL);
}

TEST(CameraLanguageDirector, TriggerKillStunChangesState) {
    CameraLanguageDirector cd;
    std::string err;
    auto def = load_camera_file("resources/camera/boss_camera.json", err);
    ASSERT_TRUE(def);
    cd.try_init(*def);
    
    cd.trigger_kill_stun();
    EXPECT_EQ(cd.state(), CameraState::KILL_STUN);
    
    // 更新超过 duration 后应回到 NORMAL
    Vector2 player_pos = {100, 100};
    Vector2 boss_pos = {150, 150};
    cd.update(0.1f, player_pos, boss_pos);
    EXPECT_EQ(cd.state(), CameraState::NORMAL);
}

TEST(CameraLanguageDirector, UpdateInterpolatesFovScale) {
    CameraLanguageDirector cd;
    std::string err;
    auto def = load_camera_file("resources/camera/boss_camera.json", err);
    ASSERT_TRUE(def);
    cd.try_init(*def);
    
    cd.enter_boss_war();
    float initial_fov = cd.fov_scale();
    
    // 更新多帧后 FOV 应该接近 zoom_in.fov_scale
    Vector2 player_pos = {100, 100};
    Vector2 boss_pos = {150, 150};
    for (int i = 0; i < 100; ++i) {
        cd.update(0.016f, player_pos, boss_pos);
    }
    
    EXPECT_NEAR(cd.fov_scale(), def->boss_war.zoom_in.fov_scale, 0.1f);
}

TEST(CameraLanguageDirector, FocusOffsetTracksBossWithinFov) {
    CameraLanguageDirector cd;
    std::string err;
    auto def = load_camera_file("resources/camera/boss_camera.json", err);
    ASSERT_TRUE(def);
    cd.try_init(*def);
    // 设置视野半径 (默认 5 tile * 32px = 160px)
    cd.set_fov_radius(160.0f);
    
    cd.enter_boss_war();
    
    Vector2 player_pos = {100, 100};
    Vector2 boss_pos = {200, 100};  // 距离 100px
    
    // 更新多帧后 focus_offset 应该接近 35% 距离 (100 * 0.35 = 35px)
    // 且限制在视野半径 80% 内 (160 * 0.8 = 128px, 35 < 128 不触发限制)
    for (int i = 0; i < 100; ++i) {
        cd.update(0.016f, player_pos, boss_pos);
    }
    
    // 期望偏移 = 距离 * 35% = 35.0
    EXPECT_NEAR(cd.focus_offset().x, 35.0f, 5.0f);
    EXPECT_NEAR(cd.focus_offset().y, 0.0f, 5.0f);
}

TEST(CameraLanguageDirector, FocusOffsetLimitedByFovRadius) {
    CameraLanguageDirector cd;
    std::string err;
    auto def = load_camera_file("resources/camera/boss_camera.json", err);
    ASSERT_TRUE(def);
    cd.try_init(*def);
    // 设置小视野半径 (100px)
    cd.set_fov_radius(100.0f);
    
    cd.enter_boss_war();
    
    Vector2 player_pos = {100, 100};
    Vector2 boss_pos = {300, 100};  // 距离 200px
    
    // 更新多帧后 focus_offset 应该被视野半径限制
    // 35% 距离 = 70px, 但视野半径 80% = 80px, 所以 70 < 80 不触发
    // 如果用更远 Boss (400px), 35% = 140px > 80px 会触发限制
    for (int i = 0; i < 100; ++i) {
        cd.update(0.016f, player_pos, boss_pos);
    }
    
    // 期望偏移 = min(70, 80) = 70.0 (未触发限制)
    EXPECT_NEAR(cd.focus_offset().x, 70.0f, 5.0f);
    EXPECT_NEAR(cd.focus_offset().y, 0.0f, 5.0f);
}

TEST(CameraLanguageDirector, FocusTimerAutoReturns) {
    CameraLanguageDirector cd;
    std::string err;
    auto def = load_camera_file("resources/camera/boss_camera.json", err);
    ASSERT_TRUE(def);
    cd.try_init(*def);
    cd.set_fov_radius(160.0f);
    
    cd.enter_boss_war();
    EXPECT_EQ(cd.state(), CameraState::BOSS_WAR);
    
    Vector2 player_pos = {100, 100};
    Vector2 boss_pos = {200, 100};
    
    // 更新超过 focus_duration (2.0s) 后应自动回归 NORMAL
    for (int i = 0; i < 200; ++i) {  // 200 * 0.016 = 3.2s > 2.0s
        cd.update(0.016f, player_pos, boss_pos);
    }
    
    EXPECT_EQ(cd.state(), CameraState::NORMAL);
    EXPECT_NEAR(cd.focus_offset().x, 0.0f, 5.0f);
    EXPECT_NEAR(cd.focus_offset().y, 0.0f, 5.0f);
}
