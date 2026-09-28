// G14: GameScene headless smoke — _process (1075 行) 主循环此前零自动化覆盖。
// 背景: 74 个既有测试无一调用 _process; 3D 表现层改动 (game_scene / player_controller /
//   hd2d_scene_builder) 只能靠实机手感验收。
// headless 约定沿用 floor_lifecycle_test.cpp / boss_floor_guard_test.cpp:
//   raylib 窗口调用已有 IsWindowReady 守卫; 字体全局在测试中桩化。
#include <gtest/gtest.h>

#include "scenes/game_scene.h"
#include "entities/player.h"
#include "world/game_map.h"
#include "data/boss_defs.h"
#include "data/enemy_defs.h"
#include "data/item_defs.h"
#include "data/weapon_defs.h"

#include <memory>

// main.cpp 中的字体全局在测试中桩化 (与 floor_lifecycle_test.cpp 一致)
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

namespace {

void load_registry_defs() {
    load_boss_defs("resources/bosses.json");
    load_enemy_defs("resources/enemies.json");
    load_item_defs("resources/items.json");
    load_weapon_defs("resources/weapons.json");
}

std::unique_ptr<GameScene> make_scene() {
    load_registry_defs();
    auto s = std::make_unique<GameScene>();
    s->player = std::make_unique<Player>(0, 0, 200, 200, 10, 5, 3);
    // 禁升级分支 (其内部解引用 get_tree(), 测试中为 null)
    s->player->xp_to_next = 1000000;
    // 禁 _activate_stairs
    s->stairs_active = true;
    return s;
}

}  // namespace

// ── SMOKE-001: 主循环连续 tick 不崩, 游戏时钟真实推进, 楼层不变量成立 ─
TEST(GameSceneSmoke, ProcessAdvancesGameTimeWithoutCrash) {
    const int floor = 1;
    const double dt = 1.0 / 60.0;

    auto s = make_scene();
    s->enter_floor(floor, 4242u);
    ASSERT_NE(s->player, nullptr);
    ASSERT_NE(s->game_map, nullptr);
    EXPECT_EQ(s->current_floor, floor);
    EXPECT_FLOAT_EQ(s->game_time, 0.0f);

    for (int i = 0; i < 60; ++i)
        s->_process(dt);

    EXPECT_NEAR(s->game_time, 60.0 * dt, 0.02)
        << "_process 未推进 game_time — 主循环静默空转或提前返回";
    EXPECT_NE(s->player, nullptr);
    EXPECT_NE(s->game_map, nullptr);
    EXPECT_EQ(s->current_floor, floor)
        << "60 tick 内不得发生楼层切换 (无显式转换入口)";
}

// ── SMOKE-002: Boss 层主循环同样存活 (is_boss_floor 专属分支) ──
TEST(GameSceneSmoke, ProcessSurvivesOnBossFloor) {
    const int floor = 5;

    auto s = make_scene();
    s->enter_floor(floor, 4243u);
    ASSERT_NE(s->player, nullptr);
    ASSERT_NE(s->game_map, nullptr);
    EXPECT_EQ(s->current_floor, floor);

    for (int i = 0; i < 120; ++i)
        s->_process(1.0 / 60.0);

    EXPECT_EQ(s->current_floor, floor);
    EXPECT_NE(s->player, nullptr);
    EXPECT_NE(s->game_map, nullptr);
}
