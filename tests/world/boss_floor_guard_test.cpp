// B4-T4: 奖励隔离守卫 — is_boss_floor 真值表契约 + 非 boss 层 Boss 击杀回归
// 守卫点: game_scene_combat.cpp on_monster_killed
//   if (m->is_boss && is_boss_floor(_s.current_floor))
// 挑战房压轴 GOLEM 以 is_boss=true 刷出 (取 director/AI 行为), 死亡必须落回常规掉落分支:
//   否则非 5/10/15 层杀 Boss 白拿 sword_legendary(倚天剑, F15 终章武器) + 圣遗物 + 30% 回血,
//   并在 F5/F10/F15 误写 Boss1/2/3_Defeated → True_Ending_Ready。
#include <gtest/gtest.h>

#include "config.h"
#include "scenes/game_scene.h"
#include "entities/boss.h"
#include "entities/monster.h"
#include "entities/player.h"
#include "entities/item.h"
#include "world/game_map.h"
#include "data/boss_defs.h"
#include "data/item_defs.h"
#include "data/weapon_defs.h"

#include <memory>

// main.cpp 中的字体全局在测试中桩化 (与 floor_lifecycle_test.cpp 一致)
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

namespace {

void load_registry() {
    // 必须装载 item+weapon: 守卫生效后走常规掉落分支会调 generate_random_item(),
    // 未装载时 random_rarity() 权重和为 0 → rng() % 0 触发 SIGFPE (生产由 main.cpp 装载)
    load_boss_defs("resources/bosses.json");
    load_item_defs("resources/items.json");
    load_weapon_defs("resources/weapons.json");
}

std::unique_ptr<GameScene> make_bare_scene(int floor) {
    auto s = std::make_unique<GameScene>();
    s->player = std::make_unique<Player>(0, 0, 200, 200, 10, 5, 3);
    s->player->combat.current_hp = 10;      // 使 30% Boss 回血可观测 (10 → 70)
    s->player->xp_to_next = 1000000;        // 禁升级分支 (其内部解引用 get_tree())
    s->stairs_active = true;                // 禁 _activate_stairs
    s->game_map = std::make_shared<GameMap>(32, 32, TILE_SIZE);
    s->current_floor = floor;
    return s;
}

}  // namespace

// ── 契约 1: boss 层仅 5/10/15 ─────────────────────────────────
TEST(BossFloorGuardTest, BossFloorsOnlyFiveTenFifteen) {
    EXPECT_TRUE(is_boss_floor(5));
    EXPECT_TRUE(is_boss_floor(10));
    EXPECT_TRUE(is_boss_floor(15));
}

// ── 契约 2: 1..30 全枚举, 其余层一律 false ────────────────────
TEST(BossFloorGuardTest, AllOtherFloorsReturnFalse) {
    for (int f = 1; f <= 30; f++) {
        bool expected = (f == 5 || f == 10 || f == 15);
        EXPECT_EQ(is_boss_floor(f), expected) << "floor " << f;
    }
}

// ── 契约 3: 压轴波只可能在非 boss 层触发, 守卫必须为假 ─────────
TEST(BossFloorGuardTest, ChallengeRoomFloorsAreNeverBossFloors) {
    EXPECT_FALSE(is_boss_floor(1));
    EXPECT_FALSE(is_boss_floor(6));
    EXPECT_FALSE(is_boss_floor(9));
    EXPECT_FALSE(is_boss_floor(11));
    EXPECT_FALSE(is_boss_floor(14));
}

// ── 回归: 非 boss 层杀 is_boss 怪物不得发放主线 Boss 奖励 ──────
// 走真实生产函数 on_monster_killed (GameSceneCombat 公开构造 + friend 访问),
// 怪物经真实工厂 boss_factory_create 创建 (与 challenge_room.cpp:293 同一路径)。
// 三个断言均为主线 Boss 奖励块专属, 常规掉落分支不可能产生:
//   30% 回血 / 圣遗物入包 / 固定 2 件掉落 (legendary 武器 + 神谕药剂)。
TEST(BossFloorGuardTest, BossKillOnNormalFloorGetsNoMainlineReward) {
    load_registry();

    auto s = make_bare_scene(6);
    const int hp_before = s->player->combat.current_hp;

    std::unique_ptr<Monster> boss(boss_factory_create(BossType::GOLEM, 10, 10, 6));
    ASSERT_NE(boss, nullptr);
    ASSERT_TRUE(boss->is_boss) << "factory 必须保留 is_boss=true 以取 director 行为";
    boss->combat.is_alive = false;

    GameSceneCombat handler(*s);
    handler.on_monster_killed(boss.get());

    EXPECT_EQ(s->player->combat.current_hp, hp_before)
        << "非 boss 层不得发放 30% Boss 回血";
    EXPECT_TRUE(s->player->relics.empty())
        << "非 boss 层不得授予主线 Boss 圣遗物";
    EXPECT_LE(s->ground_items.size(), 1)
        << "非 boss 层不得发放主线 Boss 的固定 2 件掉落";
}
