#include <gtest/gtest.h>
#include "game_map.h"
#include "config.h"
#include <algorithm>
#include <cstdlib>

// ============================================================
// A15-fix3: 掉落点归一 (nearest_walkable)
//   背景: 掉落点用 tx+2 / btx+3 盲偏移, 落进墙格后模型被墙体包住
//   看不见, 但 FOV 认为墙格可见 → 出现「空地上有 E 拾取」且能捡走。
//   不变量: 所有落地点必须可走 → 画得出 = 捡得到 = 有提示。
// ============================================================

namespace {

// 10x10 全地板, 中央 (5,5) 为墙
GameMap build_map() {
    GameMap m(10, 10, 32);
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++)
            m.set_tile(x, y, TileType::FLOOR);
    m.set_tile(5, 5, TileType::WALL);
    return m;
}

}  // namespace

TEST(DropTile, WalkableTargetKeepsPosition) {
    GameMap m = build_map();
    auto [tx, ty] = m.nearest_walkable(3, 7);
    EXPECT_EQ(tx, 3);
    EXPECT_EQ(ty, 7);
}

TEST(DropTile, WallTargetSnapsToOneRing) {
    GameMap m = build_map();
    auto [tx, ty] = m.nearest_walkable(5, 5);
    ASSERT_TRUE(m.is_walkable(tx, ty));
    // 环扫半径 1 内必命中
    EXPECT_LE(std::max(std::abs(tx - 5), std::abs(ty - 5)), 1);
}

TEST(DropTile, WallRunSnapsOutOfWall) {
    GameMap m = build_map();
    for (int x = 0; x < 10; x++) m.set_tile(x, 4, TileType::WALL);   // 整行墙
    auto [tx, ty] = m.nearest_walkable(5, 4);
    ASSERT_TRUE(m.is_walkable(tx, ty));
    EXPECT_NE(ty, 4);
}

TEST(DropTile, OutOfBoundsSnapsIntoMap) {
    GameMap m = build_map();
    auto [tx, ty] = m.nearest_walkable(30, 30);
    ASSERT_TRUE(m.is_walkable(tx, ty));
    EXPECT_LT(tx, 10);
    EXPECT_LT(ty, 10);
}

TEST(DropTile, DropsAlwaysLandOnWalkableTile) {
    // 复刻调用点写法: 盲偏移 → 归一 → 断言不变量
    GameMap m = build_map();
    const int blind[][2] = {{5, 5}, {8, 9}, {9, 9}, {0, 0}};
    for (auto& b : blind) {
        auto [tx, ty] = m.nearest_walkable(b[0], b[1]);
        EXPECT_TRUE(m.is_walkable(tx, ty)) << "drop at " << b[0] << "," << b[1];
    }
}
