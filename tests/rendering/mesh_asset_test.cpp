// A14: 真 3D 模型资产测试 — 资产合法性 + 加载器 round-trip
// 资产由 art_src/scripts/build_models.py 生成 (引擎系坐标, 1 单位 = 1 世界像素)
#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <set>
#include <string>

#include "data/mesh_asset.h"

namespace {

const char* kModelNames[] = {"npc_1", "npc_2", "npc_3", "npc_blacksmith",
                             "chest", "altar", "spring",
                             "chest_open", "altar_used", "spring_dry",
                             "room_shop", "room_shop_used", "room_forge", "room_forge_used",
                             "room_library", "room_library_used", "room_gambler",
                             "room_gambler_used", "room_shrine", "room_shrine_used",
                             "room_secret", "room_secret_used",
                             "room_challenge_pillar", "room_challenge_pillar_used",
                             "room_challenge_banner", "room_challenge_banner_used",
                             "room_challenge_gate", "room_challenge_gate_sealed",
                             "room_challenge_gate_active"};

std::string repo_path(const std::string& relative) {
    return relative;   // ctest 工作目录 = 仓库根 (tests/CMakeLists: WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
}

}  // namespace

// A14: 七个模型资产都必须存在且能被加载
TEST(MeshAsset, AllModelsLoad) {
    for (const char* name : kModelNames) {
        std::string err;
        const auto asset = load_mesh_asset_file(mesh_asset_path(name), err);
        ASSERT_TRUE(asset.has_value()) << name << ": " << err;
        EXPECT_GT(asset->triangle_count(), 0) << name;
        EXPECT_EQ(asset->positions.size(), asset->normals.size()) << name;
        EXPECT_EQ(asset->positions.size(), asset->colors.size()) << name;
        EXPECT_EQ(asset->positions.size() % 3, 0u) << name;
    }
}

// A14: 面展开 — 每面 3 顶点且索引连续 (上传层假设 3f/3f+1/3f+2)
TEST(MeshAsset, FacesAreExpandedToContiguousTriples) {
    std::string err;
    const auto asset = load_mesh_asset_file(mesh_asset_path("npc_1"), err);
    ASSERT_TRUE(asset.has_value()) << err;
    for (size_t face = 0; face * 3 < asset->positions.size(); ++face) {
        const Vector3& a = asset->positions[face * 3 + 0];
        const Vector3& b = asset->positions[face * 3 + 1];
        const Vector3& c = asset->positions[face * 3 + 2];
        const Vector3 edge1 = {b.x - a.x, b.y - a.y, b.z - a.z};
        const Vector3 edge2 = {c.x - a.x, c.y - a.y, c.z - a.z};
        const Vector3 cross = {edge1.y * edge2.z - edge1.z * edge2.y,
                               edge1.z * edge2.x - edge1.x * edge2.z,
                               edge1.x * edge2.y - edge1.y * edge2.x};
        const float cross_len = std::sqrt(cross.x * cross.x + cross.y * cross.y + cross.z * cross.z);
        // 与加载器同一判据: 叉积模 > 1e-6 (细长倒角面也算有效, 只要不退化)
        EXPECT_GT(cross_len, 1e-6f) << "degenerate face " << face;
    }
}

// A14: 法线归一 (flat shading 每面一个单位法线)
TEST(MeshAsset, NormalsAreUnitLength) {
    std::string err;
    const auto asset = load_mesh_asset_file(mesh_asset_path("chest"), err);
    ASSERT_TRUE(asset.has_value()) << err;
    for (const auto& normal : asset->normals) {
        const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        EXPECT_NEAR(length, 1.0f, 0.01f);
    }
}

// A14: 脚底贴地 — 模型最低点应为 y=0 (引擎直接按 world_pos 放置)
TEST(MeshAsset, FeetAtGroundLevel) {
    for (const char* name : kModelNames) {
        std::string err;
        const auto asset = load_mesh_asset_file(mesh_asset_path(name), err);
        ASSERT_TRUE(asset.has_value()) << name << ": " << err;
        float low = asset->positions[0].y;
        for (const auto& p : asset->positions) low = std::min(low, p.y);
        EXPECT_NEAR(low, 0.0f, 0.1f) << name;   // 鞋底/桶沿允许 <0.1 的误差
        EXPECT_GT(asset->height(), 1.0f) << name;   // 平躺件(落地挂旗)也合法
    }
}

// A14: 损坏资产必须被拒绝 (索引越界 / 缺字段 / 全部退化)
TEST(MeshAsset, RejectsBrokenAssets) {
    const std::string path = repo_path("build/a14_broken_model.json");
    {
        std::ofstream out(path);
        out << R"({"vertices":[[0,0,0,255,0,0]],"faces":[[0,1,2]]})";
    }
    std::string err;
    EXPECT_FALSE(load_mesh_asset_file(path, err).has_value());
    {
        std::ofstream out(path);
        out << R"({"vertices":[[0,0,0,255,0,0],[1,0,0,0,255,0]]})";
    }
    EXPECT_FALSE(load_mesh_asset_file(path, err).has_value());
    {
        std::ofstream out(path);
        out << R"({"vertices":[[0,0,0,255,0,0],[1,0,0,0,255,0],[2,0,0,0,0,255]],"faces":[[0,1,2]]})";
    }
    EXPECT_FALSE(load_mesh_asset_file(path, err).has_value());   // 全部退化 → 无有效面
    std::remove(path.c_str());
}

// A14: 退化面只跳过自身, 不废掉整个模型 (倒角偶发零面积面)
TEST(MeshAsset, SkipsDegenerateFacesOnly) {
    const std::string path = repo_path("build/a14_mixed_model.json");
    {
        std::ofstream out(path);
        out << R"({"vertices":[[0,0,0,255,0,0],[1,0,0,0,255,0],[2,0,0,0,0,255],)"
               R"([0,0,2,255,0,0],[0,2,0,0,255,0]],"faces":[[0,1,2],[3,4,1]]})";
    }
    std::string err;
    const auto asset = load_mesh_asset_file(path, err);
    ASSERT_TRUE(asset.has_value()) << err;
    EXPECT_EQ(asset->triangle_count(), 1);      // 3 个共线点 → 退化面被跳过
    std::remove(path.c_str());
}

// A14: 资产路径与存在性 (2D 层据此决定是否让位给 3D 模型)
TEST(MeshAsset, PathAndExistence) {
    EXPECT_EQ(mesh_asset_path("npc_1"), "assets/models/npc_1.json");
    EXPECT_TRUE(mesh_asset_exists("npc_1"));
    EXPECT_TRUE(mesh_asset_exists("chest"));
    EXPECT_FALSE(mesh_asset_exists("no_such_model_xyz"));
}
