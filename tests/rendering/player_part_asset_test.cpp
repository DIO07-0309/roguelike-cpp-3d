#include <gtest/gtest.h>
#include "data/mesh_asset.h"
#include "data/voxel_part_defs.h"
#include "game/animation/player_avatar.h"   // A16-Player: direction_yaw 朝向映射
#include "game/animation/skeleton_avatar.h"  // A17-Monster: monster_swing_yaw 摆动轴 / AvatarPartDraw
#include "game/entities/entity.h"            // Direction
#include "rendering3d/hd2d_renderer.h"       // A17-Monster: HD2DDrawItem 解耦契约

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

// A17-Monster: 怪物摆动轴 yaw — 由实际位移方向决定 (水平=左右摆, 垂直=前后摆)。
// 这是 v8 swing_side 通用化后怪物缺 yaw 的回归锁定。
TEST(PlayerPartAssets, MonsterSwingYawFollowsMoveDirection) {
    // 水平位移 → ±90 (side 轴转到深度, 屏幕内左右摆)
    EXPECT_FLOAT_EQ(monster_swing_yaw(1.0f, 0.0f), 90.0f);    // 向右
    EXPECT_FLOAT_EQ(monster_swing_yaw(-1.0f, 0.0f), -90.0f);  // 向左
    // 垂直位移 → 0/180 (side 轴 = 屏幕水平, 前后摆)
    EXPECT_FLOAT_EQ(monster_swing_yaw(0.0f, 1.0f), 180.0f);   // 向下(背对)
    EXPECT_FLOAT_EQ(monster_swing_yaw(0.0f, -1.0f), 0.0f);    // 向上(朝相机)
    // 斜向以较大分量为准
    EXPECT_FLOAT_EQ(monster_swing_yaw(2.0f, 0.5f), 90.0f);
    EXPECT_FLOAT_EQ(monster_swing_yaw(0.5f, 2.0f), 180.0f);
    // 静止返回 0 (调用方据此沿用上一方向, 不抖动)
    EXPECT_FLOAT_EQ(monster_swing_yaw(0.0f, 0.0f), 0.0f);
}

// A17-Monster: 摆动轴 yaw 与转身 yaw 必须是两个独立字段 (解耦契约)。
// v8 把摆动轴绑定在 facing_yaw 上, 怪物 facing_yaw 恒 0 → 恒前后摆; 修复后各自独立。
TEST(PlayerPartAssets, SwingYawIsDecoupledFromFacingYaw) {
    AvatarPartDraw part;
    EXPECT_FLOAT_EQ(part.yaw_deg, 0.0f);
    EXPECT_FLOAT_EQ(part.swing_yaw, 0.0f);
    // 两者可独立赋值, 互不影响
    part.yaw_deg = 45.0f;
    part.swing_yaw = -90.0f;
    EXPECT_FLOAT_EQ(part.yaw_deg, 45.0f);     // 转身
    EXPECT_FLOAT_EQ(part.swing_yaw, -90.0f);  // 摆动轴 = 水平移动左右摆
    HD2DDrawItem item;
    EXPECT_FLOAT_EQ(item.facing_yaw, 0.0f);
    EXPECT_FLOAT_EQ(item.swing_yaw, 0.0f);
    item.facing_yaw = 45.0f;
    item.swing_yaw = -90.0f;
    EXPECT_FLOAT_EQ(item.facing_yaw, 45.0f);
    EXPECT_FLOAT_EQ(item.swing_yaw, -90.0f);
}

TEST(PlayerPartAssets, CapeSwayScalesWithSpeedAndDash) {
    // A16-Player: 披风摆幅随移动速度/瞬步增大 — 静止=0, 移动>0, 更快更大, 瞬步最大
    EXPECT_FLOAT_EQ(cape_sway_gain(0.0f, 0.0f), 0.0f);        // 静止无额外摆幅
    const float slow = cape_sway_gain(0.4f, 0.0f);
    const float fast = cape_sway_gain(1.0f, 0.0f);
    const float dash = cape_sway_gain(1.0f, 1.0f);
    EXPECT_GT(slow, 0.0f);
    EXPECT_GT(fast, slow);        // 速度越快摆幅越大
    EXPECT_GT(dash, fast);        // 瞬步额外增大
}

TEST(PlayerPartAssets, BossShadowKnightPartsLoad) {
    // A16-Boss: 暗影骑士手建低模 (试点) — 5 件齐全可加载 (AvatarDirector 优先采用)
    const char* slots[] = {"leg", "arm", "weapon", "torso", "head"};
    for (const char* slot : slots) {
        std::string err;
        const auto a = load_mesh_asset_file(
            mesh_asset_path(std::string("boss_shadow_knight_part_") + slot), err);
        ASSERT_TRUE(a) << slot << ": " << err;
        EXPECT_GT(a->triangle_count(), 0) << slot;
        EXPECT_TRUE(no_degenerate(*a)) << slot;
    }
}

TEST(PlayerPartAssets, BossFireDemonPartsLoad) {
    // A16-Boss: 火魔手建低模 — 5 件齐全可加载 (AvatarDirector 优先采用)
    const char* slots[] = {"leg", "arm", "weapon", "torso", "head"};
    for (const char* slot : slots) {
        std::string err;
        const auto a = load_mesh_asset_file(
            mesh_asset_path(std::string("boss_fire_demon_part_") + slot), err);
        ASSERT_TRUE(a) << slot << ": " << err;
        EXPECT_GT(a->triangle_count(), 0) << slot;
        EXPECT_TRUE(no_degenerate(*a)) << slot;
    }
}

TEST(PlayerPartAssets, BossGolemPartsLoad) {
    // A16-Boss: 魔像手建低模 — 5 件齐全可加载 (AvatarDirector 优先采用)
    const char* slots[] = {"leg", "arm", "weapon", "torso", "head"};
    for (const char* slot : slots) {
        std::string err;
        const auto a = load_mesh_asset_file(
            mesh_asset_path(std::string("boss_golem_part_") + slot), err);
        ASSERT_TRUE(a) << slot << ": " << err;
        EXPECT_GT(a->triangle_count(), 0) << slot;
        EXPECT_TRUE(no_degenerate(*a)) << slot;
    }
}

TEST(PlayerPartAssets, BossNecromancerPartsLoad) {
    // A16-Boss: 死灵法师手建低模 — 5 件齐全可加载 (AvatarDirector 优先采用)
    const char* slots[] = {"leg", "arm", "weapon", "torso", "head"};
    for (const char* slot : slots) {
        std::string err;
        const auto a = load_mesh_asset_file(
            mesh_asset_path(std::string("boss_necromancer_part_") + slot), err);
        ASSERT_TRUE(a) << slot << ": " << err;
        EXPECT_GT(a->triangle_count(), 0) << slot;
        EXPECT_TRUE(no_degenerate(*a)) << slot;
    }
}

TEST(PlayerPartAssets, BossVampirePartsLoad) {
    // A16-Boss: 吸血鬼手建低模 — 5 件齐全可加载 (AvatarDirector 优先采用)
    const char* slots[] = {"leg", "arm", "weapon", "torso", "head"};
    for (const char* slot : slots) {
        std::string err;
        const auto a = load_mesh_asset_file(
            mesh_asset_path(std::string("boss_vampire_part_") + slot), err);
        ASSERT_TRUE(a) << slot << ": " << err;
        EXPECT_GT(a->triangle_count(), 0) << slot;
        EXPECT_TRUE(no_degenerate(*a)) << slot;
    }
}

TEST(PlayerPartAssets, BossDemonLordPartsLoad) {
    // A16-Boss: 魔王手建低模 — 5 件齐全可加载 (AvatarDirector 优先采用)
    const char* slots[] = {"leg", "arm", "weapon", "torso", "head"};
    for (const char* slot : slots) {
        std::string err;
        const auto a = load_mesh_asset_file(
            mesh_asset_path(std::string("boss_demon_lord_part_") + slot), err);
        ASSERT_TRUE(a) << slot << ": " << err;
        EXPECT_GT(a->triangle_count(), 0) << slot;
        EXPECT_TRUE(no_degenerate(*a)) << slot;
    }
}

// A17-Monster: 30 种普通怪物手建低模 — 每种 5 件, 共 150 件。
// 一个参数化遍历锁住"物种×槽位"矩阵, 避免逐条重复。
namespace {
const char* kMonsterKeys[] = {
    "orc", "archer", "goblin_hunter", "shaman", "bone_soldier",
    "skeleton_archer", "dark_mage", "shadow_stalker", "shadow_assassin",
    "night_stalker", "blood_priest", "summoner", "necromancer", "tank",
    "charger", "bomber", "elite_orc", "ice_warden", "iron_sentinel",
    "fire_imp", "lightning_orb", "void_walker", "storm_elemental",
    "slime", "frost_slime", "elite_slime", "blood_leech", "poison_wyrm",
    "golem", "stone_guardian",
};
const char* kMonsterSlots[] = {"leg", "arm", "weapon", "torso", "head"};
}  // namespace

TEST(PlayerPartAssets, All30MonsterPartsLoad) {
    for (const char* key : kMonsterKeys) {
        for (const char* slot : kMonsterSlots) {
            const std::string name = std::string("mon_") + key + "_part_" + slot;
            std::string err;
            const auto a = load_mesh_asset_file(mesh_asset_path(name), err);
            ASSERT_TRUE(a) << name << ": " << err;
            EXPECT_GT(a->triangle_count(), 0) << name;
            EXPECT_TRUE(no_degenerate(*a)) << name;
        }
    }
}
