# Task 1: 粒子系统

**Files:**
- Create: `src/game/systems/particle_system.h`
- Create: `src/game/systems/particle_system.cpp`
- Test: `tests/vfx/vfx_test.cpp` (新建)

**Interfaces:**
- Consumes: `Vector2 position`, `Color color`, `float duration`
- Produces: `ParticleSystem` 类（发射器 + 粒子池）

## Steps

### Step 1: 创建粒子系统头文件

创建 `src/game/systems/particle_system.h`：

```cpp
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
```

### Step 2: 创建粒子系统实现

创建 `src/game/systems/particle_system.cpp`：

```cpp
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
        
        // 颜色渐变
        Color c = Fade(Blend(p.color, p.end_color, t), 1.0f - t);
        
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
```

### Step 3: 创建测试文件

创建 `tests/vfx/vfx_test.cpp`：

```cpp
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
```

### Step 4: 注册测试文件

修改 `tests/CMakeLists.txt`，添加：

```cmake
add_roguelike_test(vfx_test              vfx/vfx_test.cpp)                # A9-T1: 攻击特效优化
```

### Step 5: 编译并运行测试

运行：`cmake --build build --config Release && cd build && ctest -R vfx_test`

预期：Release 0 error，ctest 通过

### Step 6: 提交

```bash
git add src/game/systems/particle_system.h src/game/systems/particle_system.cpp tests/vfx/vfx_test.cpp tests/CMakeLists.txt
git commit -m "feat(a9-t1): 粒子系统（发射器 + 粒子池）"
```
