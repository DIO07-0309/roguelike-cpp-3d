#include <gtest/gtest.h>
#include "game/systems/particle_system.h"

// 粒子系统初始化测试
TEST(ParticleSystemTest, Init) {
    ParticleSystem::init();
    EXPECT_EQ(ParticleSystem::active_count(), 0);
}

// 粒子发射测试
TEST(ParticleSystemTest, Emit) {
    ParticleSystem::init();
    ParticleSystem::clear();
    
    EmitterConfig config;
    config.position = {100, 100};
    config.velocity_min = {-50, -50};
    config.velocity_max = {50, 50};
    config.color = Color{255, 255, 255, 255};
    config.end_color = Color{0, 0, 0, 0};
    config.duration_min = 0.5f;
    config.duration_max = 1.0f;
    config.size_min = 2.0f;
    config.size_max = 6.0f;
    config.emit_rate = 10;
    
    ParticleSystem::emit(config, 10);
    EXPECT_EQ(ParticleSystem::active_count(), 10);
}

// 粒子池耗尽测试
TEST(ParticleSystemTest, ParticlePoolLimit) {
    ParticleSystem::init();
    ParticleSystem::clear();
    
    EmitterConfig config;
    config.position = {100, 100};
    config.velocity_min = {-50, -50};
    config.velocity_max = {50, 50};
    config.color = Color{255, 255, 255, 255};
    config.end_color = Color{0, 0, 0, 0};
    
    // 发射超过最大粒子数
    ParticleSystem::emit(config, 600);
    EXPECT_EQ(ParticleSystem::active_count(), ParticleSystem::MAX_PARTICLES);
}

// 粒子更新测试
TEST(ParticleSystemTest, Update) {
    ParticleSystem::init();
    ParticleSystem::clear();
    
    EmitterConfig config;
    config.position = {100, 100};
    config.velocity_min = {0, 0};
    config.velocity_max = {0, 0};
    config.color = Color{255, 255, 255, 255};
    config.end_color = Color{0, 0, 0, 0};
    config.duration_min = 1.0f;
    config.duration_max = 1.0f;
    
    ParticleSystem::emit(config, 1);
    
    // 更新 0.5 秒
    ParticleSystem::update(0.5f);
    EXPECT_EQ(ParticleSystem::active_count(), 1);
    
    // 更新 0.6 秒（过期）
    ParticleSystem::update(0.6f);
    EXPECT_EQ(ParticleSystem::active_count(), 0);
}
