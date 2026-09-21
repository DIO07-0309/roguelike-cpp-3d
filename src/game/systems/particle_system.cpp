#include "particle_system.h"
#include <cstdlib>
#include <cmath>

std::array<Particle, ParticleSystem::MAX_PARTICLES> ParticleSystem::s_particles;
bool ParticleSystem::s_initialized = false;

void ParticleSystem::init() {
    if (s_initialized) return;
    for (auto& p : s_particles) {
        p.alive = false;
    }
    s_initialized = true;
}

void ParticleSystem::emit(const EmitterConfig& config, int count) {
    if (!s_initialized) init();
    
    for (int i = 0; i < count; i++) {
        // 找到空闲粒子
        int idx = -1;
        for (int j = 0; j < MAX_PARTICLES; j++) {
            if (!s_particles[j].alive) {
                idx = j;
                break;
            }
        }
        if (idx == -1) break;  // 粒子池耗尽
        
        Particle& p = s_particles[idx];
        p.alive = true;
        p.position = config.position;
        
        // 随机速度
        float vx = config.velocity_min.x + (config.velocity_max.x - config.velocity_min.x) * (float)rand() / RAND_MAX;
        float vy = config.velocity_min.y + (config.velocity_max.y - config.velocity_min.y) * (float)rand() / RAND_MAX;
        p.velocity = {vx, vy};
        
        // 颜色和生命周期
        p.color = config.color;
        p.end_color = config.end_color;
        p.elapsed = 0.0f;
        p.duration = config.duration_min + (config.duration_max - config.duration_min) * (float)rand() / RAND_MAX;
        p.size = config.size_min + (config.size_max - config.size_min) * (float)rand() / RAND_MAX;
    }
}

void ParticleSystem::update(float delta_time) {
    if (!s_initialized) return;
    
    for (auto& p : s_particles) {
        if (!p.alive) continue;
        
        p.elapsed += delta_time;
        if (p.elapsed >= p.duration) {
            p.alive = false;
            continue;
        }
        
        // 更新位置
        p.position.x += p.velocity.x * delta_time;
        p.position.y += p.velocity.y * delta_time;
        
        // 重力（可选）
        p.velocity.y += 50.0f * delta_time;
    }
}

void ParticleSystem::draw(float cam_x, float cam_y) {
    if (!s_initialized) return;
    
    for (const auto& p : s_particles) {
        if (!p.alive) continue;
        
        float t = p.elapsed / p.duration;
        if (t >= 1.0f) continue;
        
        // 计算屏幕位置
        float sx = p.position.x - cam_x;
        float sy = p.position.y - cam_y;
        
        // 颜色渐变（手动混合）
        unsigned char r = (unsigned char)(p.color.r + (p.end_color.r - p.color.r) * t);
        unsigned char g = (unsigned char)(p.color.g + (p.end_color.g - p.color.g) * t);
        unsigned char b = (unsigned char)(p.color.b + (p.end_color.b - p.color.b) * t);
        unsigned char a = (unsigned char)((p.color.a + (p.end_color.a - p.color.a) * t) * (1.0f - t));
        Color c = Color{r, g, b, a};
        
        // 绘制粒子（圆点）
        DrawCircle(sx, sy, p.size * (1.0f - t), c);
    }
}

void ParticleSystem::clear() {
    for (auto& p : s_particles) {
        p.alive = false;
    }
}

int ParticleSystem::active_count() {
    int count = 0;
    for (const auto& p : s_particles) {
        if (p.alive) count++;
    }
    return count;
}
