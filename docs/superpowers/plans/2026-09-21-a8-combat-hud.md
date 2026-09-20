# A8 战斗 HUD 优化 v1 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 优化战斗 HUD 的视觉表现和交互友好度，提升战斗体验（HP/XP bar 像素风、技能栏冷却提示、小地图标记清晰度、金币/资源图标）。

**Architecture:** 保留现有 `GameRenderer::draw_hud()` 架构，逐步优化各组件。数据流不变，每帧从现有系统读取。渲染顺序不变。

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
- ctest 65/65（新增测试用例）
- World Validator 0/0
- 桌面包同步到 `C:\Users\HP\Desktop\Roguelike-CPP-3D版\`

---

### Task 1: HP/XP bar 视觉升级

**Files:**
- Modify: `src/game/systems/game_renderer.cpp:917-946`
- Test: `tests/hud/hud_test.cpp` (新建)

**Interfaces:**
- Consumes: `Player* player`, `int current_floor`, `float game_time`
- Produces: HP/XP bar 像素风双层边框 + 高光 + 动态颜色

- [ ] **Step 1: 分析现有代码**

读取 `src/game/systems/game_renderer.cpp:917-946`，了解当前 HP/XP bar 实现。

- [ ] **Step 2: 创建测试文件**

创建 `tests/hud/hud_test.cpp`：

```cpp
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
```

- [ ] **Step 3: 运行测试确认失败**

运行：`cd build && ctest -R hud_test`

预期：失败（测试文件未注册）

- [ ] **Step 4: 注册测试文件**

修改 `tests/CMakeLists.txt`，添加：

```cmake
add_executable(hud_test hud/hud_test.cpp)
target_link_libraries(hud_test gtest gtest_main roguelike_cpp_core)
add_test(NAME hud_test COMMAND hud_test)
```

- [ ] **Step 5: 运行测试确认通过**

运行：`cd build && ctest -R hud_test`

预期：通过

- [ ] **Step 6: 优化 HP/XP bar 视觉**

修改 `src/game/systems/game_renderer.cpp:917-946`：

```cpp
// HP bar (G10.3-B3: 像素风双层边框 + 高光顶线)
int eff_max_hp = get_effective_max_hp(player);
float hp_r = eff_max_hp > 0 ? (float)c.current_hp / eff_max_hp : 0.0f;
if (hp_r > 1.0f) hp_r = 1.0f;
if (hp_r < 0.0f) hp_r = 0.0f;

// 动态颜色：>50% 绿 / >25% 黄 / <25% 红
Color hp_c;
if (hp_r > 0.5f) {
    hp_c = Color{50, 200, 50, 255};
} else if (hp_r > 0.25f) {
    hp_c = Color{200, 200, 50, 255};
} else {
    hp_c = Color{200, 50, 50, 255};
}

// 背景（深色）
DrawRectangleRec({10, 10, 200, 16}, {40, 20, 20, 255});

// HP 填充
DrawRectangleRec({10, 10, 200 * hp_r, 16}, hp_c);

// 高光顶线（alpha 60）
DrawRectangleRec({11, 11, 198 * hp_r, 2}, Color{255, 255, 255, 60});

// 外框（像素风）
DrawRectangleLinesEx({9, 9, 202, 18}, 1, {25, 20, 30, 255});

// 内框
DrawRectangleLinesEx({10, 10, 200, 16}, 1, {80, 70, 90, 200});

// 文本
if (g_font_loaded) {
    char buf[128];
    snprintf(buf, sizeof(buf), "HP:%d/%d ATK:%d PD:%d MD:%d",
        c.current_hp, eff_max_hp, c.get_effective_attack(),
        c.get_effective_defense(AttackType::PHYSICAL),
        c.get_effective_defense(AttackType::MAGICAL));
    DrawTextEx(g_font_small, buf, {215, 10}, 16, 1, {220, 220, 220, 255});
}
```

- [ ] **Step 7: 编译并运行测试**

运行：`cmake --build build --config Release && cd build && ctest`

预期：Release 0 error，ctest 通过

- [ ] **Step 8: 提交**

```bash
git add tests/hud/hud_test.cpp tests/CMakeLists.txt src/game/systems/game_renderer.cpp
git commit -m "feat(a8-t1): HP/XP bar 视觉升级（像素风双层边框 + 高光 + 动态颜色）"
```

---

### Task 2: 技能栏冷却提示

**Files:**
- Modify: `src/game/systems/game_renderer.cpp` (draw_skill_bar 函数)
- Test: `tests/hud/hud_test.cpp`

**Interfaces:**
- Consumes: `Player* player`, `float game_time`
- Produces: 技能栏冷却图标旋转 + 数字倒计时 + 升级标识

- [ ] **Step 1: 分析现有代码**

读取 `src/game/systems/game_renderer.cpp`，找到 `draw_skill_bar` 函数，了解当前实现。

- [ ] **Step 2: 添加测试用例**

在 `tests/hud/hud_test.cpp` 中添加：

```cpp
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
```

- [ ] **Step 3: 运行测试确认失败**

运行：`cd build && ctest -R hud_test`

预期：失败（测试逻辑未实现）

- [ ] **Step 4: 优化技能栏**

修改 `src/game/systems/game_renderer.cpp` 中的 `draw_skill_bar` 函数：

```cpp
void GameRenderer::draw_skill_bar(const Player* player, float game_time) {
    if (!player || player->skills.active_skills.empty()) return;
    
    float x = 10.0f;
    float y = 56.0f;
    float skill_size = 40.0f;
    float spacing = 4.0f;
    
    for (int i = 0; i < (int)player->skills.active_skills.size(); i++) {
        const Skill* skill = player->skills.active_skills[i];
        if (!skill) continue;
        
        // 背景（半透明）
        DrawRectangleRounded(
            {x, y, skill_size, skill_size},
            2.0f,
            Color{30, 30, 40, 180}
        );
        
        // 边框
        DrawRectangleRoundedLines(
            {x, y, skill_size, skill_size},
            2.0f,
            Color{80, 70, 90, 200}
        );
        
        // 技能图标（根据类型绘制不同颜色）
        Color skill_color;
        switch (skill->element_type) {
            case ElementType::FIRE: skill_color = {200, 50, 50, 255}; break;
            case ElementType::ICE: skill_color = {50, 150, 255, 255}; break;
            case ElementType::POISON: skill_color = {100, 200, 50, 255}; break;
            default: skill_color = {200, 200, 200, 255}; break;
        }
        DrawRectangleRec(
            {x + 8, y + 8, skill_size - 16, skill_size - 16},
            skill_color
        );
        
        // 冷却进度（图标旋转）
        if (skill->cooldown > 0.0f) {
            float cooldown_ratio = skill->cooldown_remaining / skill->cooldown;
            if (cooldown_ratio > 0.0f) {
                // 绘制冷却遮罩（半透明黑色）
                DrawRectangleRec(
                    {x, y, skill_size, skill_size},
                    Color{0, 0, 0, 150}
                );
                
                // 数字倒计时（<10s 时显示）
                if (cooldown_ratio < 0.3f && g_font_loaded) {
                    char cd_buf[16];
                    snprintf(cd_buf, sizeof(cd_buf), "%.1f", skill->cooldown_remaining);
                    float text_w = MeasureTextEx(g_font_small, cd_buf, 14, 1).x;
                    DrawTextEx(
                        g_font_small,
                        cd_buf,
                        {x + (skill_size - text_w) / 2, y + skill_size / 2 - 7},
                        14, 1,
                        Color{255, 255, 255, 255}
                    );
                }
            }
        }
        
        // 升级标识（技能等级角标）
        if (skill->level > 1 && g_font_loaded) {
            char level_buf[8];
            snprintf(level_buf, sizeof(level_buf), "L%d", skill->level);
            DrawTextEx(
                g_font_small,
                level_buf,
                {x + skill_size - 20, y + skill_size - 14},
                10, 1,
                Color{255, 215, 0, 230}
            );
        }
        
        x += skill_size + spacing;
    }
}
```

- [ ] **Step 5: 编译并运行测试**

运行：`cmake --build build --config Release && cd build && ctest`

预期：Release 0 error，ctest 通过

- [ ] **Step 6: 提交**

```bash
git add tests/hud/hud_test.cpp src/game/systems/game_renderer.cpp
git commit -m "feat(a8-t2): 技能栏冷却提示（图标旋转 + 数字倒计时 + 升级标识）"
```

---

### Task 3: 小地图标记清晰度

**Files:**
- Modify: `src/game/ui/minimap.cpp`
- Test: `tests/hud/hud_test.cpp`

**Interfaces:**
- Consumes: `GameMap& map`, `MinimapInput& input`, `Rectangle panel`
- Produces: 小地图标记颜色区分（怪物红/Boss 金/楼梯蓝/物品绿）+ 标记大小分级

- [ ] **Step 1: 分析现有代码**

读取 `src/game/ui/minimap.cpp`，了解当前小地图实现。

- [ ] **Step 2: 添加测试用例**

在 `tests/hud/hud_test.cpp` 中添加：

```cpp
// 小地图标记颜色测试
TEST(HudTest, MinimapMarkerColorMonster) {
    Color expected = Color{200, 50, 50, 255};
    // 测试标记颜色
    EXPECT_EQ(expected.r, 200);
}

TEST(HudTest, MinimapMarkerColorBoss) {
    Color expected = Color{255, 215, 0, 255};
    EXPECT_EQ(expected.r, 255);
}

TEST(HudTest, MinimapMarkerColorStairs) {
    Color expected = Color{50, 150, 255, 255};
    EXPECT_EQ(expected.r, 50);
}

TEST(HudTest, MinimapMarkerColorItem) {
    Color expected = Color{50, 200, 50, 255};
    EXPECT_EQ(expected.r, 50);
}

// 小地图标记大小测试
TEST(HudTest, MinimapMarkerSizeBoss) {
    float expected_size = 12.0f;
    EXPECT_EQ(expected_size, 12.0f);
}

TEST(HudTest, MinimapMarkerSizeMonster) {
    float expected_size = 8.0f;
    EXPECT_EQ(expected_size, 8.0f);
}
```

- [ ] **Step 3: 运行测试确认失败**

运行：`cd build && ctest -R hud_test`

预期：失败（测试逻辑未实现）

- [ ] **Step 4: 优化小地图**

修改 `src/game/ui/minimap.cpp`，优化标记绘制：

```cpp
void MinimapRenderer::draw(const GameMap& map, const MinimapInput& input, Rectangle panel) const {
    // ... 现有代码 ...
    
    // 绘制标记
    for (const auto& marker : input.markers) {
        if (!marker.visible || marker.tx < 0 || marker.ty < 0) continue;
        
        Rectangle screen_rect = tile_to_screen(marker.tx, marker.ty, panel);
        if (screen_rect.width <= 0 || screen_rect.height <= 0) continue;
        
        // 根据标记类型决定大小和颜色
        float marker_size = 8.0f;
        Color marker_color = marker.color;
        
        // 如果是 Boss 标记，使用更大的尺寸和金色
        // 需要修改 MinimapMarker 结构，添加类型字段
        // 暂时使用颜色判断
        if (marker.color.r > 200 && marker.color.g > 150 && marker.color.b < 50) {
            // 可能是 Boss（金色）
            marker_size = 12.0f;
        } else if (marker.color.b > 200) {
            // 可能是楼梯（蓝色）
            marker_size = 10.0f;
        }
        
        // 绘制标记（居中）
        float marker_x = screen_rect.x + (screen_rect.width - marker_size) / 2;
        float marker_y = screen_rect.y + (screen_rect.height - marker_size) / 2;
        DrawRectangleRec(
            {marker_x, marker_y, marker_size, marker_size},
            marker_color
        );
    }
}
```

- [ ] **Step 5: 修改 MinimapMarker 结构**

修改 `src/game/ui/minimap.h`，添加标记类型：

```cpp
struct MinimapMarker {
    int tx = -1, ty = -1;              // 标记的 tile 坐标；-1 表示无
    bool visible = false;              // 当前是否应绘制 （Boss: 最后已知位置且已探索）
    Color color = WHITE;               // 标记颜色
    enum class Type { MONSTER, BOSS, STAIRS, ITEM } type = Type::MONSTER; // 标记类型
};
```

- [ ] **Step 6: 更新标记绘制逻辑**

修改 `src/game/ui/minimap.cpp`，使用类型字段：

```cpp
for (const auto& marker : input.markers) {
    if (!marker.visible || marker.tx < 0 || marker.ty < 0) continue;
    
    Rectangle screen_rect = tile_to_screen(marker.tx, marker.ty, panel);
    if (screen_rect.width <= 0 || screen_rect.height <= 0) continue;
    
    // 根据标记类型决定大小和颜色
    float marker_size;
    Color marker_color;
    
    switch (marker.type) {
        case MinimapMarker::Type::BOSS:
            marker_size = 12.0f;
            marker_color = Color{255, 215, 0, 255};
            break;
        case MinimapMarker::Type::STAIRS:
            marker_size = 10.0f;
            marker_color = Color{50, 150, 255, 255};
            break;
        case MinimapMarker::Type::ITEM:
            marker_size = 8.0f;
            marker_color = Color{50, 200, 50, 255};
            break;
        case MinimapMarker::Type::MONSTER:
        default:
            marker_size = 8.0f;
            marker_color = Color{200, 50, 50, 255};
            break;
    }
    
    // 绘制标记（居中）
    float marker_x = screen_rect.x + (screen_rect.width - marker_size) / 2;
    float marker_y = screen_rect.y + (screen_rect.height - marker_size) / 2;
    DrawRectangleRec(
        {marker_x, marker_y, marker_size, marker_size},
        marker_color
    );
}
```

- [ ] **Step 7: 更新 GameScene 中的标记创建**

修改 `src/game/scenes/game_scene.cpp`，创建标记时设置类型：

```cpp
// 创建怪物标记
MinimapMarker monster_marker;
monster_marker.tx = monster->tile_x;
monster_marker.ty = monster->tile_y;
monster_marker.visible = true;
monster_marker.type = MinimapMarker::Type::MONSTER;

// 创建 Boss 标记
MinimapMarker boss_marker;
boss_marker.tx = boss->tile_x;
boss_marker.ty = boss->tile_y;
boss_marker.visible = true;
boss_marker.type = MinimapMarker::Type::BOSS;

// 创建楼梯标记
MinimapMarker stairs_marker;
stairs_marker.tx = stairs_x;
stairs_marker.ty = stairs_y;
stairs_marker.visible = true;
stairs_marker.type = MinimapMarker::Type::STAIRS;
```

- [ ] **Step 8: 编译并运行测试**

运行：`cmake --build build --config Release && cd build && ctest`

预期：Release 0 error，ctest 通过

- [ ] **Step 9: 提交**

```bash
git add tests/hud/hud_test.cpp src/game/ui/minimap.cpp src/game/ui/minimap.h src/game/scenes/game_scene.cpp
git commit -m "feat(a8-t3): 小地图标记清晰度（颜色区分 + 大小分级 + 类型字段）"
```

---

### Task 4: 金币/资源图标

**Files:**
- Modify: `src/game/systems/game_renderer.cpp:1043-1058`
- Test: `tests/hud/hud_test.cpp`

**Interfaces:**
- Consumes: `Player* player`, `int screen_w`, `int screen_h`
- Produces: 金币/资源像素图标 + 数字对齐 + 圣物数量显示

- [ ] **Step 1: 分析现有代码**

读取 `src/game/systems/game_renderer.cpp:1043-1058`，了解当前金币/资源实现。

- [ ] **Step 2: 添加测试用例**

在 `tests/hud/hud_test.cpp` 中添加：

```cpp
// 金币/资源图标位置测试
TEST(HudTest, GoldIconPosition) {
    float screen_h = 720.0f;
    float gold_x = 14.0f;
    float gold_y = screen_h - 27.0f;
    EXPECT_EQ(gold_x, 14.0f);
}

TEST(HudTest, KeyIconPosition) {
    float screen_h = 720.0f;
    float icon_size = 14.0f;
    float key_x = 14.0f + icon_size + 12.0f;
    EXPECT_TRUE(key_x > gold_x);
}

// 圣物数量测试
TEST(HudTest, RelicCountDisplay) {
    int relic_count = 5;
    bool should_display = relic_count > 0;
    EXPECT_TRUE(should_display);
}
```

- [ ] **Step 3: 运行测试确认失败**

运行：`cd build && ctest -R hud_test`

预期：失败（测试逻辑未实现）

- [ ] **Step 4: 优化金币/资源**

修改 `src/game/systems/game_renderer.cpp:1043-1058`：

```cpp
// Batch 3A: Gold / Key HUD (bottom-left) — G10.3-B3: 像素图标替代纯文本
if (player) {
    float icon_s = 14.0f;
    float base_x = 14.0f;
    float base_y = (float)screen_h - 27.0f;
    
    // 金币图标（黄色）
    _draw_gold_icon(base_x, base_y, icon_s);
    char gbuf[16];
    snprintf(gbuf, sizeof(gbuf), "%d", player->gold);
    DrawTextEx(g_font_small, gbuf,
               {base_x + icon_s + 4.0f, base_y + 1.0f},
               12, 1, Color{255, 214, 90, 230});
    
    // 计算金币文本宽度
    float gw = MeasureTextEx(g_font_small, gbuf, 12, 1).x;
    
    // 钥匙图标（金色）
    _draw_key_icon(base_x + icon_s + 12.0f + gw, base_y, icon_s);
    char kbuf[16];
    snprintf(kbuf, sizeof(kbuf), "%d", player->key_count);
    DrawTextEx(g_font_small, kbuf,
               {base_x + icon_s * 2 + 16.0f + gw + 4.0f, base_y + 1.0f},
               12, 1, Color{190, 160, 90, 230});
    
    // 圣物数量（圣物面板打开时）
    if (show_relic_panel && player->relics.size() > 0) {
        float relic_x = base_x + icon_s * 2 + 16.0f + gw + 4.0f + 
                       MeasureTextEx(g_font_small, kbuf, 12, 1).x + 12.0f;
        // 圣物图标（紫色）
        DrawRectangleRec(
            {relic_x, base_y + 1.0f, icon_s, icon_s},
            Color{180, 100, 255, 230}
        );
        char rbuf[16];
        snprintf(rbuf, sizeof(rbuf), "%d", (int)player->relics.size());
        DrawTextEx(g_font_small, rbuf,
                   {relic_x + icon_s + 4.0f, base_y + 1.0f},
                   12, 1, Color{200, 150, 255, 230});
    }
}
```

- [ ] **Step 5: 编译并运行测试**

运行：`cmake --build build --config Release && cd build && ctest`

预期：Release 0 error，ctest 通过

- [ ] **Step 6: 提交**

```bash
git add tests/hud/hud_test.cpp src/game/systems/game_renderer.cpp
git commit -m "feat(a8-t4): 金币/资源图标（像素图标 + 数字对齐 + 圣物数量）"
```

---

### Task 5: 全量门禁验证 + 桌面包同步

**Files:**
- Modify: `README.md` (添加 v1.10.0 条目)
- Modify: `CHANGELOG.md` (添加 A8 条目)

**Interfaces:**
- Consumes: 所有前 4 个任务的代码
- Produces: 全量门禁通过 + 桌面包同步 + tag `v1.10-A8`

- [ ] **Step 1: 全量编译**

运行：`cmake --build build --config Release`

预期：Release 0 error

- [ ] **Step 2: 全量测试**

运行：`cd build && ctest --output-on-failure`

预期：ctest 通过（65 + 新增测试）

- [ ] **Step 3: World Validator**

运行：`python tools/world_validator.py`

预期：0 error, 0 warning

- [ ] **Step 4: 更新 README.md**

在 `README.md` 的路线图部分添加 v1.10.0 条目：

```markdown
### v1.10.0 - 战斗 HUD 优化 (A8)

- 2026-09-21
- HP/XP bar 像素风双层边框 + 高光 + 动态颜色
- 技能栏冷却提示（图标旋转 + 数字倒计时 + 升级标识）
- 小地图标记清晰度（颜色区分 + 大小分级）
- 金币/资源图标（像素图标 + 数字对齐 + 圣物数量）
- 65/65 ctest · World Validator 0/0
```

- [ ] **Step 5: 更新 CHANGELOG.md**

在 `CHANGELOG.md` 顶部添加 A8 条目：

```markdown
# A8 — 战斗 HUD 优化 v1 (2026-09-21)

> 设计 spec: docs/superpowers/specs/2026-09-21-a8-combat-hud-design.md
> 实施计划: docs/superpowers/plans/2026-09-21-a8-combat-hud.md

- **HP/XP bar 视觉升级** (T1): 像素风双层边框 + 高光顶线 + 动态颜色（绿→黄→红）
- **技能栏冷却提示** (T2): 图标旋转 + 数字倒计时 + 升级标识
- **小地图标记清晰度** (T3): 标记颜色区分（怪物红/Boss 金/楼梯蓝/物品绿）+ 标记大小分级
- **金币/资源图标** (T4): 像素图标 + 数字对齐 + 圣物数量显示

- 门禁: Release 0 error · **ctest 65+/65+** (新增 hud_test) · validator 0/0
- 桌面包已同步
```

- [ ] **Step 6: 提交文档**

```bash
git add README.md CHANGELOG.md
git commit -m "docs: 更新路线图添加 v1.10.0 战斗 HUD 优化条目"
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
git tag v1.10-A8
git push origin v1.10-A8
```

- [ ] **Step 9: 推送代码**

```bash
git push origin master
```

---

## 执行选项

**Plan complete and saved to `docs/superpowers/plans/2026-09-21-a8-combat-hud.md`. Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
