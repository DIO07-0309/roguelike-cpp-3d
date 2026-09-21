#pragma once
#include "raylib.h"
#include <vector>
#include <array>

// ============================================================
// ParticleSystem - 粒子系统（发射器 + 粒子池）
// ============================================================

struct Particle {
    Vector2 position;
    Vector2 velocity;
    Color color;
    Color end_color;
    float elapsed = 0.0f;
    float duration = 1.0f;
    float size = 4.0f;
    bool alive = false;
};

struct EmitterConfig {
    Vector2 position;
    Vector2 velocity_min, velocity_max;
    Color color;
    Color end_color;
    float duration_min = 0.5f, duration_max = 1.0f;
    float size_min = 2.0f, size_max = 6.0f;
    int emit_rate = 10;  // 每秒发射数
};

class ParticleSystem {
public:
    static constexpr int MAX_PARTICLES = 512;
    
    // 初始化
    static void init();
    
    // 发射粒子
    static void emit(const EmitterConfig& config, int count = 1);
    
    // 更新（每帧调用）
    static void update(float delta_time);
    
    // 绘制
    static void draw(float cam_x, float cam_y);
    
    // 清空所有粒子
    static void clear();
    
    // 获取活跃粒子数
    static int active_count();
    
private:
    static std::array<Particle, MAX_PARTICLES> s_particles;
    static bool s_initialized;
};
