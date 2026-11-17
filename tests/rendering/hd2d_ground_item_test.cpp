// A21-fix 回归: 3D 掉落物两处根因
//  1) 世界变换顺序 — raylib 行向量约定下 MatrixMultiply(A,B) = 先 A 后 B。
//     旧写法 MatrixMultiply(translate, rotate) 会先平移、再绕世界原点旋转:
//     yaw=90 时 z 变成 -208 (地图外), yaw=180 时坐标整体取反。
//     而 yaw 来自 tile 坐标哈希散摆, 所以几乎每件掉落物都不在正确的格子上,
//     提示仍按 tile 坐标投影 → 玩家看到「E 提示在脚下, 地上却没有物品」。
//  2) 背包满 — pickup_item 曾静默返回空串, 按 E 毫无反馈, 表现为"捡不到"。
#include <gtest/gtest.h>

#include "rendering3d/hd2d_renderer.h"
#include "systems/interaction_handler.h"
#include "entities/player.h"
#include "entities/item.h"
#include "world/game_map.h"
#include "config.h"
#include "data/item_defs.h"
#include "data/weapon_defs.h"

#include "raymath.h"

#include <vector>

namespace {

GameMap make_open_map() {
    GameMap m(12, 12, TILE_SIZE);
    for (int y = 0; y < m.height; y++)
        for (int x = 0; x < m.width; x++)
            m.set_tile(x, y, TileType::FLOOR);
    return m;
}

}  // namespace

// ── 1) 模型局部原点必须落在 world_pos 上 (与 yaw 无关) ──
TEST(HD2DModelTransform, LocalOriginLandsOnWorldPosition) {
    const Vector3 origin = {0.0f, 0.0f, 0.0f};
    const Vector3 world = {208.0f, 6.0f, 176.0f};

    for (float yaw = 0.0f; yaw < 360.0f; yaw += 15.0f) {
        const Matrix m = model_world_transform(world, yaw);
        const Vector3 got = Vector3Transform(origin, m);
        EXPECT_NEAR(got.x, world.x, 0.001f) << "yaw=" << yaw;
        EXPECT_NEAR(got.y, world.y, 0.001f) << "yaw=" << yaw;
        EXPECT_NEAR(got.z, world.z, 0.001f) << "yaw=" << yaw;
    }
}

// 模型局部脚点绕自身 Y 轴转, 不能被甩到负数 z (地图外)
TEST(HD2DModelTransform, RotatedFootPointStaysNearTile) {
    const Vector3 world = {208.0f, 6.0f, 176.0f};
    const Vector3 foot = {0.0f, 1.0f, 0.0f};
    for (float yaw = 0.0f; yaw < 360.0f; yaw += 15.0f) {
        const Vector3 got = Vector3Transform(foot, model_world_transform(world, yaw));
        EXPECT_NEAR(got.x, world.x, 2.0f) << "yaw=" << yaw;
        EXPECT_NEAR(got.z, world.z, 2.0f) << "yaw=" << yaw;
        EXPECT_NEAR(got.y, world.y + 1.0f, 0.001f) << "yaw=" << yaw;
    }
}

// 记录旧顺序的破坏性: 同样的 yaw 下模型被转到地图外 (防止有人"简化"回去)
TEST(HD2DModelTransform, OldTranslateRotateOrderOrbitsWorldOrigin) {
    const Vector3 world = {208.0f, 6.0f, 176.0f};
    const Vector3 foot = {0.0f, 1.0f, 0.0f};
    const Matrix bad = MatrixMultiply(MatrixTranslate(world.x, world.y, world.z),
                                      MatrixRotateY(90.0f * DEG2RAD));
    const Vector3 bad_foot = Vector3Transform(foot, bad);
    EXPECT_LT(bad_foot.z, 0.0f);
    EXPECT_NEAR(bad_foot.x, 176.0f, 0.001f);
    EXPECT_NEAR(bad_foot.z, -208.0f, 0.001f);
}

// ── 2) 背包满: 必须有可见反馈, 且物品留在原地 ──
TEST(InteractionPickup, FullInventoryReportsMessageAndKeepsItem) {
    ASSERT_TRUE(load_item_defs("resources/items.json"));
    ASSERT_TRUE(load_weapon_defs("resources/weapons.json"));
    GameMap map = make_open_map();
    map.update_fov(5, 5, 6);

    Player player(5 * TILE_SIZE, 5 * TILE_SIZE, 200, 200, 10, 5, 3);
    while (!player.inventory.is_full()) {
        ASSERT_TRUE(player.inventory.add(
            std::make_shared<ConsumableItem>("filler", Rarity::COMMON, "heal", 1),
            &player));
    }
    ASSERT_TRUE(player.inventory.is_full());

    auto item = generate_random_item();
    ASSERT_TRUE(item != nullptr);
    std::vector<DroppedItem> ground;
    ground.push_back({item, 5, 5});

    InteractionHandler handler;
    const std::string msg = handler.pickup_item(&player, &map, ground);

    EXPECT_FALSE(msg.empty()) << "背包满时拾取必须给可见反馈";
    EXPECT_NE(msg.find("拾取: "), 0u) << "不能谎报拾取成功";
    EXPECT_EQ(ground.size(), 1u) << "拾取失败, 物品必须留在原地";
}

// 对照: 有空位时同一路径仍正常拾取
TEST(InteractionPickup, EmptySlotStillPicksUp) {
    GameMap map = make_open_map();
    map.update_fov(5, 5, 6);

    Player player(5 * TILE_SIZE, 5 * TILE_SIZE, 200, 200, 10, 5, 3);
    ASSERT_FALSE(player.inventory.is_full());

    auto item = generate_random_item();
    ASSERT_TRUE(item != nullptr);
    std::vector<DroppedItem> ground;
    ground.push_back({item, 5, 5});

    InteractionHandler handler;
    const std::string msg = handler.pickup_item(&player, &map, ground);

    EXPECT_EQ(msg.rfind("拾取: ", 0), 0u) << "成功路径文案回归";
    EXPECT_TRUE(ground.empty()) << "拾取成功后物品必须消失";
}
