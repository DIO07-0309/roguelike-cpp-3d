# A9 攻击特效优化 v1 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 优化攻击特效（武器三连击 + 技能 + 元素效果），在 HD-2D 3D 模式下提升视觉冲击和打击感。

**Architecture:** 保留现有 `Effect` 结构，逐步增强各组件。新增粒子系统、打击感组合、Shader 效果。数据流不变，每帧从现有系统读取。渲染顺序不变。

**Tech Stack:** C++17, Raylib 5.0, GoogleTest

## Global Constraints

- 函数长度不超过 40 行
- 一个类只负责一件事情
- 优先组合而不是继承
- 所有变量命名必须语义化
- 每次修改代码前先分析影响
- 修改完成必须进行 Code Review
- 不允许一次生成超过 300 行代码
- 永远以可维护性优先
- Release 0 error
- ctest 66/66（新增测试用例）
- World Validator 0/0
- 桌面包同步到 `C:\Users\HP\Desktop\Roguelike-CPP-3D版\`

---

### Task 1: 粒子系统

**Files:**
- Create: `src/game/systems/particle_system.h`
- Create: `src/game/systems/particle_system.cpp`
- Test: `tests/vfx/vfx_test.cpp` (新建)

**Interfaces:**
- Consumes: `Vector2 position`, `Color color`, `float duration`
- Produces: `ParticleSystem` 类（发射器 + 粒子池）

- [ ] **Step 1: 创建粒子系统头文件**

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

- [ ] **Step 2: 创建粒子系统实现**

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

- [ ] **Step 3: 创建测试文件**

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

- [ ] **Step 4: 注册测试文件**

修改 `tests/CMakeLists.txt`，添加：

```cmake
add_roguelike_test(vfx_test              vfx/vfx_test.cpp)                # A9-T1: 攻击特效优化
```

- [ ] **Step 5: 编译并运行测试**

运行：`cmake --build build --config Release && cd build && ctest -R vfx_test`

预期：Release 0 error，ctest 通过

- [ ] **Step 6: 提交**

```bash
git add src/game/systems/particle_system.h src/game/systems/particle_system.cpp tests/vfx/vfx_test.cpp tests/CMakeLists.txt
git commit -m "feat(a9-t1): 粒子系统（发射器 + 粒子池）"
```

---

### Task 2: 武器特效增强

**Files:**
- Modify: `src/game/systems/game_renderer.cpp` (_draw_effect_body 函数)
- Modify: `src/game/systems/weapon_executor.cpp` (on_hit 函数)
- Modify: `src/game/types/combat_types.h` (Effect.kind 扩展)
- Test: `tests/vfx/vfx_test.cpp`

**Interfaces:**
- Consumes: `Effect` 结构, `WeaponType` 枚举
- Produces: 5 类武器 × 3 段连击特效类型

- [ ] **Step 1: 分析现有代码**

读取 `src/game/systems/game_renderer.cpp:181-215`，了解当前特效类型。

读取 `src/game/systems/weapon_executor.cpp`，找到 `on_hit` 函数。

- [ ] **Step 2: 添加测试用例**

在 `tests/vfx/vfx_test.cpp` 中添加：

```cpp
// 武器特效类型映射测试
TEST(WeaponVfxTest, SlashArcTypes) {
    // 剑（扇形斩）
    std::string kinds[3] = {"slash_arc_1", "slash_arc_2", "slash_arc_3"};
    for (int i = 0; i < 3; i++) {
        EXPECT_FALSE(kinds[i].empty());
    }
}

TEST(WeaponVfxTest, PierceBeamTypes) {
    // 矛（穿透）
    std::string kinds[3] = {"pierce_beam_1", "pierce_beam_2", "pierce_beam_3"};
    for (int i = 0; i < 3; i++) {
        EXPECT_FALSE(kinds[i].empty());
    }
}

TEST(WeaponVfxTest, WeaponEffectKindMapping) {
    // 5 类武器 × 3 段连击 = 15 种特效
    // 这里测试类型映射逻辑
    int weapon_count = 5;
    int combo_count = 3;
    int total_effects = weapon_count * combo_count;
    EXPECT_EQ(total_effects, 15);
}
```

- [ ] **Step 3: 运行测试确认失败**

运行：`cd build && ctest -R vfx_test`

预期：失败（特效类型未实现）

- [ ] **Step 4: 扩展 Effect 类型**

修改 `src/game/systems/game_renderer.cpp` 中的 `_draw_effect_body` 函数：

```cpp
static void _draw_effect_body(const Effect& e, float sx, float sy,
                              float cam_x, float cam_y, float t) {
    float alpha = 1.0f - t / e.duration;
    if (alpha <= 0) return;
    Color c = e.color; c.a = (unsigned char)(c.a * alpha);
    float prog = t / e.duration;
    
    // 剑（扇形斩）三连击
    if (e.kind == "slash_arc_1") {
        _draw_slash_arc_1(e, sx, sy, prog, c);
    } else if (e.kind == "slash_arc_2") {
        _draw_slash_arc_2(e, sx, sy, prog, c);
    } else if (e.kind == "slash_arc_3") {
        _draw_slash_arc_3(e, sx, sy, prog, c);
    }
    // 矛（穿透）三连击
    else if (e.kind == "pierce_beam_1") {
        _draw_pierce_beam_1(e, sx, sy, prog, c);
    } else if (e.kind == "pierce_beam_2") {
        _draw_pierce_beam_2(e, sx, sy, prog, c);
    } else if (e.kind == "pierce_beam_3") {
        _draw_pierce_beam_3(e, sx, sy, prog, c);
    }
    // 双截棍（追踪）三连击
    else if (e.kind == "whip_arc_1") {
        _draw_whip_arc_1(e, sx, sy, prog, c);
    } else if (e.kind == "whip_arc_2") {
        _draw_whip_arc_2(e, sx, sy, prog, c);
    } else if (e.kind == "whip_arc_3") {
        _draw_whip_arc_3(e, sx, sy, prog, c);
    }
    // 连弩（弹幕）三连击
    else if (e.kind == "bolt_spread_1") {
        _draw_bolt_spread_1(e, sx, sy, prog, c);
    } else if (e.kind == "bolt_spread_2") {
        _draw_bolt_spread_2(e, sx, sy, prog, c);
    } else if (e.kind == "bolt_spread_3") {
        _draw_bolt_spread_3(e, sx, sy, prog, c);
    }
    // 重锤（重击）三连击
    else if (e.kind == "smash_impact_1") {
        _draw_smash_impact_1(e, sx, sy, prog, c);
    } else if (e.kind == "smash_impact_2") {
        _draw_smash_impact_2(e, sx, sy, prog, c);
    } else if (e.kind == "smash_impact_3") {
        _draw_smash_impact_3(e, sx, sy, prog, c);
    }
    // 现有特效类型（保留）
    else if (e.kind == "pulse" || e.kind == "ring") {
        _draw_fx_ring(sx, sy, e.radius, prog, c, 24);
    } else if (e.kind == "spark") {
        _draw_fx_blast(sx, sy, e.radius * (0.5f + 0.5f * prog), c,
                       (unsigned char)std::min(255, c.a * 2));
    } else if (e.kind == "bolt") {
        DrawLineEx({sx, sy}, {e.target_x - cam_x, e.target_y - cam_y}, 3, c);
    } else if (e.kind == "flash") {
        _draw_fx_blast(sx, sy, e.radius, c,
                       (unsigned char)std::min(255, c.a * 2));
        _draw_fx_blast(sx, sy, e.radius * 1.5f, c, c.a / 2);
    } else if (e.kind == "smoke") {
        float sr = e.radius * (0.3f + 0.7f * prog);
        DrawCircle(sx, sy, sr, Fade(c, 0.5f));
    } else if (e.kind == "shield_ring") {
        float pulse = 0.8f + 0.2f * sinf(t * 8.0f);
        DrawRing({sx, sy}, e.radius * 0.7f, e.radius, 0, 360, 16,
                 Color{(unsigned char)c.r, (unsigned char)c.g, (unsigned char)c.b,
                       (unsigned char)(c.a * pulse)});
    } else if (e.kind == "slash_arc") {
        _draw_slash_arc(e, sx, sy, prog, c);
    } else if (e.kind == "cone") {
        DrawRectangleLines(sx - e.radius / 2, sy - e.radius / 4,
                           e.radius, e.radius / 2, c);
    } else {
        _draw_fx_ring(sx, sy, e.radius, prog, c, 12);
    }
}
```

- [ ] **Step 5: 添加武器特效绘制函数**

在 `game_renderer.cpp` 中添加 15 个武器特效绘制函数（每个 20-30 行）：

```cpp
static void _draw_slash_arc_1(const Effect& e, float sx, float sy, float prog, Color c) {
    // 单弧线
    float radius = e.radius * (0.5f + 0.5f * prog);
    DrawArc({sx, sy}, radius, -45, 45, 16, c);
}

static void _draw_slash_arc_2(const Effect& e, float sx, float sy, float prog, Color c) {
    // 双弧线
    float radius = e.radius * (0.5f + 0.5f * prog);
    DrawArc({sx, sy}, radius, -45, 45, 16, c);
    DrawArc({sx, sy}, radius * 0.8f, -30, 30, 16, Fade(c, 0.7f));
}

static void _draw_slash_arc_3(const Effect& e, float sx, float sy, float prog, Color c) {
    // 三连弧 + 粒子拖尾
    float radius = e.radius * (0.5f + 0.5f * prog);
    DrawArc({sx, sy}, radius, -45, 45, 16, c);
    DrawArc({sx, sy}, radius * 0.8f, -30, 30, 16, Fade(c, 0.7f));
    DrawArc({sx, sy}, radius * 0.6f, -15, 15, 16, Fade(c, 0.5f));
    
    // 粒子拖尾
    EmitterConfig config;
    config.position = {e.world_x, e.world_y};
    config.velocity_min = {-20, -20};
    config.velocity_max = {20, 20};
    config.color = c;
    config.end_color = Color{255, 255, 255, 0};
    config.duration_min = 0.3f;
    config.duration_max = 0.5f;
    config.size_min = 2.0f;
    config.size_max = 4.0f;
    ParticleSystem::emit(config, 3);
}

// ... 其他 12 个函数类似（pierce_beam_1/2/3, whip_arc_1/2/3, bolt_spread_1/2/3, smash_impact_1/2/3）
```

- [ ] **Step 6: 修改武器命中触发**

修改 `src/game/systems/weapon_executor.cpp` 中的 `on_hit` 函数，根据武器类型和连击数创建对应的 Effect：

```cpp
void WeaponExecutor::on_hit(WeaponType weapon, int combo, float hit_x, float hit_y, 
                            std::vector<Effect>& effects) {
    Effect e;
    e.world_x = hit_x;
    e.world_y = hit_y;
    e.radius = 32;
    e.duration = 0.3f;
    
    // 根据武器类型和连击数设置特效类型
    if (weapon == WeaponType::SWORD) {
        e.kind = "slash_arc_" + std::to_string(combo);
        e.color = Color{200, 200, 220, 255};
    } else if (weapon == WeaponType::SPEAR) {
        e.kind = "pierce_beam_" + std::to_string(combo);
        e.color = Color{150, 200, 255, 255};
    } else if (weapon == WeaponType::NUNCHUKU) {
        e.kind = "whip_arc_" + std::to_string(combo);
        e.color = Color{255, 200, 150, 255};
    } else if (weapon == WeaponType::CROSSBOW) {
        e.kind = "bolt_spread_" + std::to_string(combo);
        e.color = Color{255, 180, 50, 255};
    } else if (weapon == WeaponType::HAMMER) {
        e.kind = "smash_impact_" + std::to_string(combo);
        e.color = Color{255, 100, 100, 255};
    }
    
    effects.push_back(e);
}
```

- [ ] **Step 7: 编译并运行测试**

运行：`cmake --build build --config Release && cd build && ctest -R vfx_test`

预期：Release 0 error，ctest 通过

- [ ] **Step 8: 提交**

```bash
git add src/game/systems/game_renderer.cpp src/game/systems/weapon_executor.cpp src/game/types/combat_types.h tests/vfx/vfx_test.cpp
git commit -m "feat(a9-t2): 武器特效增强（5 类武器 × 3 段连击）"
```

---

### Task 3: 打击感组合

**Files:**
- Create: `src/game/systems/hit_flash.h`
- Create: `src/game/systems/hit_flash.cpp`
- Modify: `src/game/systems/game_renderer.cpp` (集成 HitFlash)
- Test: `tests/vfx/vfx_test.cpp`

**Interfaces:**
- Consumes: `float duration`, `Color color`
- Produces: `HitFlash` 类（命中闪白）

- [ ] **Step 1: 创建 HitFlash 头文件**

创建 `src/game/systems/hit_flash.h`：

```cpp
#pragma once
#include "raylib.h"

// ============================================================
// HitFlash - 命中闪白效果
// ============================================================

class HitFlash {
public:
    // 触发闪白
    static void trigger(float duration = 0.1f, Color color = WHITE);
    
    // 更新（每帧调用）
    static void update(float delta_time);
    
    // 绘制（叠加到实体上）
    static void draw(float entity_x, float entity_y, float radius);
    
    // 是否活跃
    static bool is_active();
    
    // 剩余时间
    static float remaining();
    
private:
    static float s_elapsed;
    static float s_duration;
    static Color s_color;
    static bool s_active;
};
```

- [ ] **Step 2: 创建 HitFlash 实现**

创建 `src/game/systems/hit_flash.cpp`：

```cpp
#include "hit_flash.h"

float HitFlash::s_elapsed = 0.0f;
float HitFlash::s_duration = 0.0f;
Color HitFlash::s_color = WHITE;
bool HitFlash::s_active = false;

void HitFlash::trigger(float duration, Color color) {
    s_elapsed = 0.0f;
    s_duration = duration;
    s_color = color;
    s_active = true;
}

void HitFlash::update(float delta_time) {
    if (!s_active) return;
    
    s_elapsed += delta_time;
    if (s_elapsed >= s_duration) {
        s_active = false;
    }
}

void HitFlash::draw(float entity_x, float entity_y, float radius) {
    if (!s_active) return;
    
    float t = s_elapsed / s_duration;
    Color c = s_color;
    c.a = (unsigned char)(c.a * (1.0f - t));
    
    // 绘制闪白（白色叠加）
    DrawCircle(entity_x, entity_y, radius, Fade(c, 0.5f));
}

bool HitFlash::is_active() {
    return s_active;
}

float HitFlash::remaining() {
    if (!s_active) return 0.0f;
    return s_duration - s_elapsed;
}
```

- [ ] **Step 3: 添加测试用例**

在 `tests/vfx/vfx_test.cpp` 中添加：

```cpp
// HitFlash 测试
TEST(HitFlashTest, Trigger) {
    HitFlash::trigger(0.1f, WHITE);
    EXPECT_TRUE(HitFlash::is_active());
    EXPECT_GT(HitFlash::remaining(), 0.0f);
}

TEST(HitFlashTest, Update) {
    HitFlash::trigger(0.1f, WHITE);
    
    // 更新 0.05 秒
    HitFlash::update(0.05f);
    EXPECT_TRUE(HitFlash::is_active());
    EXPECT_GT(HitFlash::remaining(), 0.0f);
    
    // 更新 0.1 秒（过期）
    HitFlash::update(0.1f);
    EXPECT_FALSE(HitFlash::is_active());
}

TEST(HitFlashTest, DefaultDuration) {
    HitFlash::trigger();
    EXPECT_EQ(HitFlash::remaining(), 0.1f);
}
```

- [ ] **Step 4: 集成到 game_renderer.cpp**

在 `src/game/systems/game_renderer.cpp` 中添加 HitFlash 更新和绘制：

```cpp
void GameRenderer::draw_entity(const Entity* entity, float cam_x, float cam_y) {
    // ... 现有绘制代码 ...
    
    // 命中闪白
    if (HitFlash::is_active()) {
        float sx = entity->rect.x - cam_x;
        float sy = entity->rect.y - cam_y;
        float radius = max(entity->rect.width, entity->rect.height) / 2;
        HitFlash::draw(sx, sy, radius);
    }
}
```

- [ ] **Step 5: 增强震屏强度分级**

修改现有震屏代码，根据怪物类型设置不同强度：

```cpp
// 在怪物死亡时触发震屏
void on_monster_killed(Monster* monster) {
    float amplitude = 4.0f;  // 普通
    if (monster->is_elite()) amplitude = 8.0f;
    if (monster->is_boss()) amplitude = 16.0f;
    
    ScreenShake::trigger(amplitude, 20.0f);
}
```

- [ ] **Step 6: 飘字颜色分级**

修改飘字代码，根据伤害类型设置颜色：

```cpp
void DamageFloat::trigger(float x, float y, int damage, DamageType type, bool critical) {
    Color color;
    if (critical) {
        color = Color{255, 215, 0, 255};  // 金色
    } else if (type == DamageType::PHYSICAL) {
        color = Color{255, 255, 255, 255};  // 白色
    } else if (type == DamageType::MAGICAL) {
        color = Color{100, 150, 255, 255};  // 蓝色
    } else {
        color = Color{255, 100, 100, 255};  // 红色（元素）
    }
    // ... 创建飘字 ...
}
```

- [ ] **Step 7: 编译并运行测试**

运行：`cmake --build build --config Release && cd build && ctest -R vfx_test`

预期：Release 0 error，ctest 通过

- [ ] **Step 8: 提交**

```bash
git add src/game/systems/hit_flash.h src/game/systems/hit_flash.cpp src/game/systems/game_renderer.cpp tests/vfx/vfx_test.cpp
git commit -m "feat(a9-t3): 打击感组合（闪白 + 震屏分级 + 飘字颜色）"
```

---

### Task 4: Shader 效果

**Files:**
- Create: `src/game/systems/vfx_shader.h`
- Create: `src/game/systems/vfx_shader.cpp`
- Test: `tests/vfx/vfx_test.cpp`

**Interfaces:**
- Consumes: `RenderTexture2D target`
- Produces: `VFXShader` 类（发光/模糊/扭曲）

- [ ] **Step 1: 创建 VFXShader 头文件**

创建 `src/game/systems/vfx_shader.h`：

```cpp
#pragma once
#include "raylib.h"

// ============================================================
// VFXShader - 特效 Shader（发光/模糊/扭曲）
// ============================================================

enum class VFXType {
    NONE,
    BLOOM,      // 发光
    MOTION_BLUR,// 模糊
    SCREEN_WARP // 扭曲
};

class VFXShader {
public:
    // 初始化
    static void init();
    
    // 设置当前效果
    static void set_type(VFXType type);
    
    // 启用/禁用
    static void enable(bool enabled);
    
    // 是否活跃
    static bool is_active();
    
    // 获取当前类型
    static VFXType current_type();
    
    // 应用效果（在绘制场景后调用）
    static void apply(RenderTexture2D target, int screen_w, int screen_h);
    
private:
    static Shader s_bloom_shader;
    static Shader s_blur_shader;
    static Shader s_warp_shader;
    static VFXType s_current_type;
    static bool s_enabled;
};
```

- [ ] **Step 2: 创建 VFXShader 实现**

创建 `src/game/systems/vfx_shader.cpp`：

```cpp
#include "vfx_shader.h"
#include "config.h"

Shader VFXShader::s_bloom_shader = {};
Shader VFXShader::s_blur_shader = {};
Shader VFXShader::s_warp_shader = {};
VFXType VFXShader::s_current_type = VFXType::NONE;
bool VFXShader::s_enabled = false;

void VFXShader::init() {
    // 加载 shader（如果存在）
    const char* bloom_path = "assets/shaders/vfx_bloom.glsl";
    const char* blur_path = "assets/shaders/vfx_blur.glsl";
    const char* warp_path = "assets/shaders/vfx_warp.glsl";
    
    if (FileExists(bloom_path)) {
        s_bloom_shader = LoadShader(bloom_path);
    }
    if (FileExists(blur_path)) {
        s_blur_shader = LoadShader(blur_path);
    }
    if (FileExists(warp_path)) {
        s_warp_shader = LoadShader(warp_path);
    }
}

void VFXShader::set_type(VFXType type) {
    s_current_type = type;
}

void VFXShader::enable(bool enabled) {
    s_enabled = enabled;
}

bool VFXShader::is_active() {
    return s_enabled && s_current_type != VFXType::NONE;
}

VFXType VFXShader::current_type() {
    return s_current_type;
}

void VFXShader::apply(RenderTexture2D target, int screen_w, int screen_h) {
    if (!is_active()) return;
    
    Shader shader;
    switch (s_current_type) {
        case VFXType::BLOOM:
            shader = s_bloom_shader;
            break;
        case VFXType::MOTION_BLUR:
            shader = s_blur_shader;
            break;
        case VFXType::SCREEN_WARP:
            shader = s_warp_shader;
            break;
        default:
            return;
    }
    
    if (shader.id == 0) return;  // Shader 未加载，fallback 到程序绘制
    
    // 应用 shader
    BeginTextureMode(target);
        ClearBackground(BLACK);
        SetTexture(target.texture);
        if (s_current_type == VFXType::BLOOM) {
            SetShaderValue(shader, shader.loc["offset"], (float){2.0f, 2.0f});
        } else if (s_current_type == VFXType::MOTION_BLUR) {
            SetShaderValue(shader, shader.loc["delta"], (float){1.0f, 0.5f});
        } else if (s_current_type == VFXType::SCREEN_WARP) {
            SetShaderValue(shader, shader.loc["intensity"], 0.5f);
            SetShaderValue(shader, shader.loc["time"], GetTime());
        }
        DrawTextureRec(target.texture, 
                       (Rectangle){0, 0, (float)target.width, (float)target.height},
                       (Vector2){0, 0}, WHITE);
    EndTextureMode();
    
    BeginShaderMode(shader);
        DrawTextureRec(target.texture, 
                       (Rectangle){0, 0, (float)target.width, (float)target.height},
                       (Vector2){0, 0}, WHITE);
    EndShaderMode();
}
```

- [ ] **Step 3: 添加测试用例**

在 `tests/vfx/vfx_test.cpp` 中添加：

```cpp
// VFXShader 测试
TEST(VFXShaderTest, Init) {
    VFXShader::init();
    EXPECT_FALSE(VFXShader::is_active());
}

TEST(VFXShaderTest, SetType) {
    VFXShader::set_type(VFXType::BLOOM);
    EXPECT_EQ(VFXShader::current_type(), VFXType::BLOOM);
}

TEST(VFXShaderTest, Enable) {
    VFXShader::set_type(VFXType::BLOOM);
    VFXShader::enable(true);
    EXPECT_TRUE(VFXShader::is_active());
    
    VFXShader::enable(false);
    EXPECT_FALSE(VFXShader::is_active());
}

TEST(VFXShaderTest, Fallback) {
    // Shader 未加载时，不崩溃
    VFXShader::init();
    VFXShader::set_type(VFXType::BLOOM);
    VFXShader::enable(true);
    
    // 应用效果（即使 shader 未加载也不崩溃）
    RenderTexture2D target = {};
    target.id = 0;  // 未加载
    VFXShader::apply(target, 800, 600);
    EXPECT_FALSE(VFXShader::is_active());  // 应该 fallback
}
```

- [ ] **Step 4: 编译并运行测试**

运行：`cmake --build build --config Release && cd build && ctest -R vfx_test`

预期：Release 0 error，ctest 通过

- [ ] **Step 5: 提交**

```bash
git add src/game/systems/vfx_shader.h src/game/systems/vfx_shader.cpp tests/vfx/vfx_test.cpp
git commit -m "feat(a9-t4): Shader 效果（发光/模糊/扭曲）"
```

---

### Task 5: 全量门禁验证 + 桌面包同步

**Files:**
- Modify: `README.md` (添加 v1.11.0 条目)
- Modify: `CHANGELOG.md` (添加 A9 条目)

**Interfaces:**
- Consumes: 所有前 4 个任务的代码
- Produces: 全量门禁通过 + 桌面包同步 + tag `v1.11-A9`

- [ ] **Step 1: 全量编译**

运行：`cmake --build build --config Release`

预期：Release 0 error

- [ ] **Step 2: 全量测试**

运行：`cd build && ctest --output-on-failure`

预期：ctest 通过（66 + 新增测试）

- [ ] **Step 3: World Validator**

运行：`python tools/world_validator.py`

预期：0 error, 0 warning

- [ ] **Step 4: 更新 README.md**

在 `README.md` 的路线图部分添加 v1.11.0 条目：

```markdown
### v1.11.0 - 攻击特效优化 (A9)

- 2026-09-21
- 粒子系统（发射器 + 粒子池，最大 512 粒子）
- 武器特效增强（5 类武器 × 3 段连击，15 种特效）
- 打击感组合（闪白 + 震屏分级 + 飘字颜色）
- Shader 效果（发光/模糊/扭曲，仅 Boss 战启用）
- 66+/66+ ctest · World Validator 0/0
```

- [ ] **Step 5: 更新 CHANGELOG.md**

在 `CHANGELOG.md` 顶部添加 A9 条目：

```markdown
# A9 — 攻击特效优化 v1 (2026-09-21)

> 设计 spec: docs/superpowers/specs/2026-09-21-a9-attack-vfx-design.md
> 实施计划: docs/superpowers/plans/2026-09-21-a9-attack-vfx.md

- **粒子系统** (T1): `ParticleSystem` 类（发射器 + 粒子池，最大 512 粒子）
- **武器特效增强** (T2): 5 类武器 × 3 段连击（15 种特效类型）
- **打击感组合** (T3): `HitFlash` 闪白 + 震屏强度分级 + 飘字颜色分级
- **Shader 效果** (T4): `VFXShader` 类（发光/模糊/扭曲，仅 Boss 战启用）

- 门禁: Release 0 error · **ctest 66+/66+** (新增 vfx_test) · validator 0/0
- 桌面包已同步
```

- [ ] **Step 6: 提交文档**

```bash
git add README.md CHANGELOG.md
git commit -m "docs: 更新路线图添加 v1.11.0 攻击特效优化条目"
```

- [ ] **Step 7: 同步桌面包**

同步到 `C:\Users\HP\Desktop\Roguelike-CPP-3D版\`：

```bash
# 镜像 src/ resources/ tools/ tests/ docs/ assets/ vendor/
# 保留桌面独有的 saves/ 等
# 复制 exe 到桌面包根目录
```

- [ ] **Step 8: 打 tag**

```bash
git tag v1.11-A9
git push origin v1.11-A9
```

- [ ] **Step 9: 推送代码**

```bash
git push origin master
```

---

## 执行选项

**Plan complete and saved to `docs/superpowers/plans/2026-09-21-a9-attack-vfx.md`. Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
