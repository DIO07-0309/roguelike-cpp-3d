// G14: hd2d 3D 构建层测试 — 此前零自动化覆盖 (animation_test.cpp 只 #include 未调用)。
// 两处目标:
//   1) build_scene 空视图契约 — 回归 G14 修复: _build_entities / _build_boss_skill_warnings /
//      _build_monster_overlays 三处 `*view.monsters` 曾无判空, 空视图调用直接 segfault。
//   2) GameScene::hd2d_view() 只读视图装配 — 锁死 GameScene → 3D 表现层的接线
//      (G12-4 解耦后, 这是 3D 渲染器唯一的数据入口)。
// 限制: build_scene 的地形分支会调 procedural_tile() 生成纹理, 需要 GL 上下文,
//   因此无法 headless 断言实体/billboard 产出; 该部分仍需实机验收。
#include <gtest/gtest.h>

#include "rendering3d/hd2d_scene_builder.h"
#include "rendering3d/hd2d_scene_view.h"
#include "rendering3d/hd2d_renderer.h"
#include "scenes/game_scene.h"
#include "entities/player.h"
#include "world/game_map.h"
#include "data/boss_defs.h"
#include "data/enemy_defs.h"

#include <memory>
#include <vector>

// main.cpp 中的字体全局在测试中桩化 (与 floor_lifecycle_test.cpp 一致)
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

namespace {

std::unique_ptr<GameScene> make_scene() {
    load_boss_defs("resources/bosses.json");
    load_enemy_defs("resources/enemies.json");
    auto s = std::make_unique<GameScene>();
    s->player = std::make_unique<Player>(0, 0, 200, 200, 10, 5, 3);
    // 禁升级分支 (其内部解引用 get_tree(), 测试中为 null)
    s->player->xp_to_next = 1000000;
    // 禁 _activate_stairs
    s->stairs_active = true;
    return s;
}

}  // namespace

// ── 契约 1: 全空视图必须安全返回空列表 (约定: nullptr = 子系统缺失) ──
TEST(HD2DSceneBuilder, EmptyViewProducesNoItems) {
    hd2d::SceneView view;
    std::vector<HD2DDrawItem> items;
    hd2d::build_scene(view, items, false);
    EXPECT_TRUE(items.empty());
}

// ── 契约 2: hd2d_view 必须填齐核心只读指针 (缺失则 3D 不出画面) ──
TEST(GameSceneHd2dView, PopulatesCorePointers) {
    auto s = make_scene();
    s->enter_floor(3, 4242u);

    hd2d::SceneView v = s->hd2d_view();

    EXPECT_NE(v.game_map, nullptr) << "无地形则 3D 不出画面";
    EXPECT_NE(v.player, nullptr)   << "无玩家则看不到主角";
    EXPECT_NE(v.monsters, nullptr)
        << "空指针会让 build_scene 解引用崩溃 (见契约 1 的回归)";
    EXPECT_EQ(v.monsters->size(), s->monsters.size());
    EXPECT_EQ(v.current_floor, 3);
    EXPECT_EQ(v.player, s->player.get());
    EXPECT_EQ(v.game_map, s->game_map.get());
}

// ── 契约 3: 可选子系统必须全部接线 (3D 特效/相机/氛围/挑战房) ────
TEST(GameSceneHd2dView, WiresAllOptionalSubsystems) {
    auto s = make_scene();
    s->enter_floor(3, 4242u);

    hd2d::SceneView v = s->hd2d_view();

    EXPECT_NE(v.boss_ctrl, nullptr);
    EXPECT_NE(v.camera_director, nullptr);
    EXPECT_NE(v.challenge_ctrl, nullptr);
    EXPECT_NE(v.ambient_layer, nullptr);
    EXPECT_TRUE(v.npc_avatar)      << "NPC 骨骼 callable 未接线";
    EXPECT_TRUE(v.player_avatar_fn) << "玩家骨骼 callable 未接线";
    EXPECT_FALSE(v.sim_mode);
    EXPECT_FALSE(v.in_challenge_arena);
}

// ── 契约 4: 挑战竞技场状态必须透传到 3D 视图 ─────────────────
TEST(GameSceneHd2dView, ChallengeArenaPropagatesToView) {
    auto s = make_scene();
    s->enter_floor(1, 4243u);
    EXPECT_FALSE(s->hd2d_view().in_challenge_arena);

    s->enter_challenge_arena();
    const hd2d::SceneView in_arena = s->hd2d_view();
    EXPECT_TRUE(in_arena.in_challenge_arena);
    EXPECT_EQ(in_arena.current_floor, 1);

    s->exit_challenge_arena();
    EXPECT_FALSE(s->hd2d_view().in_challenge_arena);
}
