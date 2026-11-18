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
#include "world/special_room_palette.h"
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

// ── A21: 3D 世界标签投影的视口归一化 ──
// 根因: GetWorldToScreen 按 DPI 放大后的 viewport(1440x960) 返回像素, 但 HUD/
// E提示/伤害飘字画在 960x640 逻辑 RT → 提示飘到右下、与物品对不齐。
TEST(HD2DSceneBuilder, LogicalScreenCoordNormalizesViewport) {
    // 无缩放: viewport 恰为逻辑 RT → 保持原值
    Vector2 a = logical_screen_coord({480.0f, 320.0f}, 960.0f, 640.0f, 960.0f, 640.0f);
    EXPECT_FLOAT_EQ(a.x, 480.0f);
    EXPECT_FLOAT_EQ(a.y, 320.0f);
    // DPI 1.5x: proj(1381,759)@1440x960 → 逻辑(≈920,506)@960x640
    Vector2 b = logical_screen_coord({1381.0f, 759.0f}, 1440.0f, 960.0f, 960.0f, 640.0f);
    EXPECT_NEAR(b.x, 920.7f, 1.0f);
    EXPECT_NEAR(b.y, 506.0f, 1.0f);
    // 逻辑中心不因缩放改变
    Vector2 c = logical_screen_coord({720.0f, 480.0f}, 1440.0f, 960.0f, 960.0f, 640.0f);
    EXPECT_NEAR(c.x, 480.0f, 0.5f);
    EXPECT_NEAR(c.y, 320.0f, 0.5f);
}

// ── A21: 3D 掉落物必须抬到 A20 雕饰地板石板顶面之上 ──
// 实测: 地板石板 y∈[0,4.48], 掉落物模型平躺 y∈[0,3](高度≈0) → 放 y=0 会被埋住。
// 2D/无雕饰地板是平片(y≈0), 保持贴地 0。
TEST(HD2DSceneBuilder, GroundItemLiftedAboveTerrainSlab) {
    EXPECT_FLOAT_EQ(hd2d::ground_item_model_y(false), 0.0f);
    // 必须严格高于石板顶面 4.48(留余量), 但也不该飘太高
    EXPECT_GT(hd2d::ground_item_model_y(true), 4.48f);
    EXPECT_LE(hd2d::ground_item_model_y(true), 12.0f);
}

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

// ── P5: 特殊房类型色烘进 3D 模型 tint ─────────────────────────────
// 顶点色本身很暗(≈26/255), tint 与之相乘。为不比普通地板更暗且保留九色色相,
// tint 把基色色相拉满到 max=255(非简单乘系数), 再乘 triggered/visible 压暗。
TEST(HD2DSceneBuilder, SpecialRoomTintForModel) {
    SpecialRoom sr;
    sr.type = SpecialRoomType::SHOP;
    sr.triggered = false;

    Color normal = hd2d::special_room_tint_for_model(&sr, true);
    // 色相拉满: 最大通道接近 255(SHOP 基色 {62,56,24} → 最大 r=255)
    EXPECT_GE(normal.r, 250);
    EXPECT_LE(normal.r, 255);
    // 色相比例保持基色比例 g/r≈56/62, b/r≈24/62
    EXPECT_NEAR((double)normal.g / normal.r, 56.0 / 62.0, 0.06);
    EXPECT_NEAR((double)normal.b / normal.r, 24.0 / 62.0, 0.06);

    sr.triggered = true;
    Color trig = hd2d::special_room_tint_for_model(&sr, true);
    EXPECT_NEAR((double)trig.r / normal.r, 0.55, 0.05) << "triggered 压暗 45%";

    sr.triggered = false;
    Color hidden = hd2d::special_room_tint_for_model(&sr, false);
    EXPECT_NEAR((double)hidden.r / normal.r, 0.60, 0.05) << "未即时可见压暗 40%";
}

// ── P5 fix: 只有"FLOOR 且真走 3D"才跳过 2D tint/decal ─────────────
// 特殊房 STAIRS_DOWN / 缺资产回退(use_3d=false) 必须保留 2D 类型色。
TEST(HD2DSceneBuilder, SpecialRoomDecalNeededForNon3dTiles) {
    EXPECT_FALSE(hd2d::special_room_tile_decal_needed(true, true))
        << "FLOOR 真走 3D → 跳过 2D decal(石板会盖住)";
    EXPECT_TRUE(hd2d::special_room_tile_decal_needed(false, true))
        << "STAIRS_DOWN(非FLOOR) → 保留 2D tint/decal";
    EXPECT_TRUE(hd2d::special_room_tile_decal_needed(true, false))
        << "缺资产回退(use_3d=false) → 保留 2D tint/decal";
}

// ── P5: 只有特殊房 FLOOR 转 3D, 墙/门仍走 quad ───────────────────
TEST(HD2DSceneBuilder, SpecialRoomFloorUsesTerrainModel) {
    EXPECT_TRUE(hd2d::special_room_floor_uses_3d(true, true))
        << "特殊房 FLOOR 必须走 3D 地形模型";
    EXPECT_FALSE(hd2d::special_room_floor_uses_3d(false, true))
        << "特殊房 WALL/DOOR 必须保留 quad(保阴影/轮廓)";
    EXPECT_TRUE(hd2d::special_room_floor_uses_3d(true, false))
        << "普通 FLOOR 本来就走 3D";
}

// ── P5: rubble 密度哈希确定性 + 单一色表真相 ──────────────────────
TEST(HD2DSceneBuilder, RubbleDensityDeterministic) {
    EXPECT_EQ(hd2d::rubble_placed(3, 7), hd2d::rubble_placed(3, 7))
        << "同坐标必须同结果";
    int placed = 0;
    for (int x = 0; x < 20; ++x)
        for (int y = 0; y < 20; ++y)
            if (hd2d::rubble_placed(x, y)) ++placed;
    // 20% 密度, 20x20=400 格 → 期望 80 (±20)
    EXPECT_GE(placed, 60);
    EXPECT_LE(placed, 100);
    // 不能所有格都相同
    bool first = hd2d::rubble_placed(0, 0);
    bool all_same = true;
    for (int x = 0; x < 20; ++x)
        for (int y = 0; y < 20; ++y)
            if (hd2d::rubble_placed(x, y) != first) all_same = false;
    EXPECT_FALSE(all_same);
}

TEST(HD2DSceneBuilder, SpecialRoomPaletteSingleSource) {
    // 2D 与 3D 都从同一函数取 SHOP 基色
    Color shop = special_room_base_color(SpecialRoomType::SHOP);
    EXPECT_EQ(shop.r, 62);
    EXPECT_EQ(shop.g, 56);
    EXPECT_EQ(shop.b, 24);
    // 3D tint 派生自同一基色(重标定后更亮)
    SpecialRoom sr;
    sr.type = SpecialRoomType::SHOP;
    EXPECT_GE(hd2d::special_room_tint_for_model(&sr, true).r, 100);
}

// ── P5-fix2: 贴地陷阱(尖刺/毒池)在有 3D 石板的格子上必须抬到石板顶 ──
// 石板顶 y≈4.48; 陷阱原本贴地 y=0.08 会被埋。无石板格保持贴地避免悬空。
TEST(HD2DSceneBuilder, ArenaFlatObjectLiftedOnTerrain) {
    EXPECT_FLOAT_EQ(hd2d::arena_flat_object_y(false), 0.08f)
        << "无 3D 石板的格子保持贴地";
    EXPECT_GT(hd2d::arena_flat_object_y(true), 4.48f)
        << "有 3D 石板时陷阱须高于石板顶 4.48";
    EXPECT_LE(hd2d::arena_flat_object_y(true), 6.0f)
        << "不应过高悬空";
}
