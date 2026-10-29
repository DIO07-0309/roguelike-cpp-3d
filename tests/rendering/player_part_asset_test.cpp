#include <gtest/gtest.h>
#include "data/mesh_asset.h"
#include "data/voxel_part_defs.h"
#include "game/animation/player_avatar.h"   // A16-Player: direction_yaw 朝向映射
#include "game/entities/entity.h"           // Direction

// ============================================================
// A16-Player: 玩家低模部件资产 — 6 件 (leg/torso/head/arm/weapon/cape)
//   · 广告牌局部空间: 原点=骨骼锚点(pivot), x=宽度, y=深度(负=朝相机), z=高度
//   · 与体素共用 voxel_slot_from_path 槽位键 (运行时按槽位加载到骨骼)
//   · 不进 mesh_asset_test 的 kModelNames (那里校验脚底 y≈0 贴地; 部件锚点在 pivot 不贴地)
// ============================================================

namespace {

const char* kPartSlots[] = {"leg", "torso", "head", "arm", "weapon", "cape"};

bool no_degenerate(const MeshAsset& asset) {
    const auto& p = asset.positions;
    for (int f = 0; f < asset.triangle_count(); ++f) {
        const Vector3 a = p[f * 3 + 0], b = p[f * 3 + 1], c = p[f * 3 + 2];
        const Vector3 e1 = {b.x - a.x, b.y - a.y, b.z - a.z};
        const Vector3 e2 = {c.x - a.x, c.y - a.y, c.z - a.z};
        const float cx = e1.y * e2.z - e1.z * e2.y;
        const float cy = e1.z * e2.x - e1.x * e2.z;
        const float cz = e1.x * e2.y - e1.y * e2.x;
        if (cx * cx + cy * cy + cz * cz <= 1e-12f) return false;
    }
    return true;
}

void bounds(const MeshAsset& asset, float& min_x, float& max_x,
            float& min_y, float& max_y, float& min_z, float& max_z) {
    min_x = max_x = asset.positions[0].x;
    min_y = max_y = asset.positions[0].y;
    min_z = max_z = asset.positions[0].z;
    for (const auto& p : asset.positions) {
        min_x = std::min(min_x, p.x); max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y); max_y = std::max(max_y, p.y);
        min_z = std::min(min_z, p.z); max_z = std::max(max_z, p.z);
    }
}

}  // namespace

TEST(PlayerPartAssets, AllSixSlotsLoad) {
    for (const char* slot : kPartSlots) {
        std::string err;
        const auto asset = load_mesh_asset_file(
            mesh_asset_path("player_part_" + std::string(slot)), err);
        ASSERT_TRUE(asset.has_value()) << slot << ": " << err;
        EXPECT_GT(asset->triangle_count(), 0) << slot;
        EXPECT_EQ(asset->positions.size(), asset->normals.size()) << slot;
        EXPECT_EQ(asset->positions.size(), asset->colors.size()) << slot;
        EXPECT_EQ(asset->positions.size() % 3, 0u) << slot;
        EXPECT_TRUE(no_degenerate(*asset)) << slot << " 含退化面";
    }
}

TEST(PlayerPartAssets, CoordinateConventionLocked) {
    // 原点=骨骼锚点(pivot): 向下延伸部件 (leg/arm/weapon/cape) 顶部≈锚点、底部深入;
    // 躯干上下对称; 头向上; 披风在背后 (y>0, 离相机)。对照设计 §2.5。
    std::string err;

    // leg: 髋顶锚点, 向下到靴 (z≈-21), 顶部不越过锚点
    {
        auto a = load_mesh_asset_file(mesh_asset_path("player_part_leg"), err);
        ASSERT_TRUE(a); float mnx, mxx, mny, mxy, mnz, mxz;
        bounds(*a, mnx, mxx, mny, mxy, mnz, mxz);
        EXPECT_LE(mnz, -18.0f) << "leg 应向下到靴";
        EXPECT_LE(mxz, 1.5f) << "leg 顶部不应越过锚点";
        EXPECT_LE(mxx, 12.0f);
    }
    // arm: 肩顶锚点, 向下到手 (z≈-18)
    {
        auto a = load_mesh_asset_file(mesh_asset_path("player_part_arm"), err);
        ASSERT_TRUE(a); float mnx, mxx, mny, mxy, mnz, mxz;
        bounds(*a, mnx, mxx, mny, mxy, mnz, mxz);
        EXPECT_LE(mnz, -16.0f) << "arm 应向下到手";
        EXPECT_LE(mxz, 1.5f) << "arm 顶部不应越过锚点";
    }
    // weapon: 握柄顶锚点, 向下到剑尖 (z≈-23)
    {
        auto a = load_mesh_asset_file(mesh_asset_path("player_part_weapon"), err);
        ASSERT_TRUE(a); float mnx, mxx, mny, mxy, mnz, mxz;
        bounds(*a, mnx, mxx, mny, mxy, mnz, mxz);
        EXPECT_LE(mnz, -21.0f) << "weapon 应向下到剑尖";
        EXPECT_LE(mxz, 1.5f) << "weapon 顶部不应越过锚点";
    }
    // cape: 顶锚点, 向下到下摆 (z≈-25), 位于背后 y>0 (离相机)
    {
        auto a = load_mesh_asset_file(mesh_asset_path("player_part_cape"), err);
        ASSERT_TRUE(a); float mnx, mxx, mny, mxy, mnz, mxz;
        bounds(*a, mnx, mxx, mny, mxy, mnz, mxz);
        EXPECT_LE(mnz, -23.0f) << "cape 应向下到下摆";
        EXPECT_LE(mxz, 2.5f) << "cape 顶部不应越过锚点";
        EXPECT_GE(mny, 0.0f) << "cape 应在背后 (y>0)";
    }
    // torso: 锚点在中间, 上下对称
    {
        auto a = load_mesh_asset_file(mesh_asset_path("player_part_torso"), err);
        ASSERT_TRUE(a); float mnx, mxx, mny, mxy, mnz, mxz;
        bounds(*a, mnx, mxx, mny, mxy, mnz, mxz);
        EXPECT_LE(mnz, -8.0f) << "torso 应向下到腰带";
        EXPECT_GE(mxz, 8.0f) << "torso 应向上到胸甲";
    }
    // head: 颈锚点, 向上到头顶 (z≈13.6)
    {
        auto a = load_mesh_asset_file(mesh_asset_path("player_part_head"), err);
        ASSERT_TRUE(a); float mnx, mxx, mny, mxy, mnz, mxz;
        bounds(*a, mnx, mxx, mny, mxy, mnz, mxz);
        EXPECT_GE(mxz, 11.0f) << "head 应向上到头顶";
    }
}

TEST(PlayerPartAssets, SlotKeyMatchesRuntimeLookup) {
    // 运行时 _setup_lowpoly_parts 用 voxel_slot_from_path(part.file) 定位低模文件,
    // 必须与体素槽位键一致 (否则加载不到对应部件)
    EXPECT_EQ(voxel_slot_from_path("assets/sprites/player_part_leg.png"), "leg");
    EXPECT_EQ(voxel_slot_from_path("assets/sprites/player_part_cape.png"), "cape");
    EXPECT_EQ(voxel_slot_from_path("assets/sprites/player_part_arm.png"), "arm");
    EXPECT_EQ(voxel_slot_from_path("assets/sprites/player_part_weapon.png"), "weapon");
    EXPECT_EQ(voxel_slot_from_path("assets/sprites/player_part_torso.png"), "torso");
    EXPECT_EQ(voxel_slot_from_path("assets/sprites/player_part_head.png"), "head");
}

TEST(PlayerPartAssets, DirectionYawFollowsWASD) {
    // A16-Player: 朝向随 WASD — 绕广告牌 up 轴转向 (0=下/朝相机, 90=右, -90=左, 180=上)
    EXPECT_FLOAT_EQ(direction_yaw(Direction::DOWN), 0.0f);    // S
    EXPECT_FLOAT_EQ(direction_yaw(Direction::RIGHT), 90.0f);  // D
    EXPECT_FLOAT_EQ(direction_yaw(Direction::LEFT), -90.0f);  // A
    EXPECT_FLOAT_EQ(direction_yaw(Direction::UP), 180.0f);    // W
}
