// A13: 部件体素化单测 — 纯函数层契约 + 与 art_src/scripts/voxel_lib.py 逐件面数一致性
// 面数基线来源: Blender 脚本对同一批真实 PNG 构建的 quad 数 (build_player_rig.py 输出)
#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "data/voxel_part_defs.h"
#include "game/rendering3d/part_voxelizer.h"

namespace {

VoxelConfig test_config() {
    VoxelConfig config;
    config.ink_max_luminance = 0.25f;
    config.alpha_min = 0.5f;
    return config;
}

// 全实心网格, 调色板颜色亮度 0.43 (非墨线); ink 逐项指定
voxel::GridData solid_grid(int w, int h, const std::vector<bool>& ink = {}) {
    voxel::GridData grid;
    grid.width = w;
    grid.height = h;
    grid.cell.assign((size_t)w * h, 0);
    grid.palette.push_back(Color{120, 140, 160, 255});
    grid.is_ink.push_back(!ink.empty() && ink[0]);
    return grid;
}

VoxelPartDef flat_def(float base, float growth, float cap, float ink_px) {
    VoxelPartDef def;
    def.base = base;
    def.growth = growth;
    def.cap = cap;
    def.ink_px = ink_px;
    return def;
}

float vertex_extent(const voxel::MeshArrays& arrays, int axis, bool want_max) {
    float best = want_max ? -1e9f : 1e9f;
    for (size_t i = (size_t)axis; i < arrays.vertices.size(); i += 3) {
        best = want_max ? std::max(best, arrays.vertices[i])
                        : std::min(best, arrays.vertices[i]);
    }
    return best;
}

// A13-M2: 单件校验 — 参考 quad 数 = 引擎网格 quad 数
void expect_batch_part(const VoxelConfig& config, const std::string& key, int want_quads) {
    const size_t slash = key.find('/');
    ASSERT_NE(slash, std::string::npos) << key;
    const std::string parts_key = key.substr(0, slash);
    const std::string slot = key.substr(slash + 1);
    const std::string path = "assets/sprites/" + parts_key + "_part_" + slot + ".png";
    Image image = LoadImage(path.c_str());
    ASSERT_GT(image.width, 0) << path;
    const VoxelPartDef* def = config.find_for(parts_key, slot);
    ASSERT_NE(def, nullptr) << key;
    const auto grid = voxel::extract_grid(
        (const unsigned char*)image.data, image.width, image.height, config);
    const auto depths = voxel::compute_depths(grid, *def);
    const auto arrays = voxel::build_mesh_arrays(grid, depths, *def, 0.f, 0.f);
    EXPECT_EQ((int)(arrays.indices.size() / 6), want_quads) << path;
    UnloadImage(image);
}

} // namespace

TEST(VoxelizerGrid, ExtractRowsPaletteAndInk) {
    // raylib/stb 图像数据 = 顶部行在前: 前 12 字节 = 贴图顶部行
    unsigned char pixels[3 * 2 * 4] = {};
    const unsigned char lite[4] = {200, 160, 120, 255};
    const unsigned char dark[4] = {60, 45, 50, 255};
    std::memcpy(&pixels[0], lite, 4);      // 顶行: 亮, 透明, 暗
    std::memcpy(&pixels[8], dark, 4);
    std::memcpy(&pixels[12], lite, 4);     // 底行: 亮, 亮, 透明
    std::memcpy(&pixels[16], lite, 4);
    const auto grid = voxel::extract_grid(pixels, 3, 2, test_config());
    EXPECT_EQ(grid.width, 3);
    EXPECT_EQ(grid.height, 2);
    EXPECT_EQ(grid.at(0, 0), 0);
    EXPECT_EQ(grid.at(1, 0), -1);
    EXPECT_EQ(grid.at(2, 0), 1);
    EXPECT_EQ(grid.at(0, 1), 0);
    EXPECT_EQ(grid.at(1, 1), 0);
    EXPECT_EQ(grid.at(2, 1), -1);
    ASSERT_EQ(grid.palette.size(), 2u);
    EXPECT_FALSE(grid.is_ink[0]);
    EXPECT_TRUE(grid.is_ink[1]);
}

TEST(VoxelizerDepth, EdgeDistanceFieldsAndInkOverride) {
    const auto def = flat_def(4.f, 1.f, 100.f, 7.f);
    const auto grid = solid_grid(5, 5);
    const auto depths = voxel::compute_depths(grid, def);
    EXPECT_FLOAT_EQ(depths[0], 4.f);                     // 角 d=0
    EXPECT_FLOAT_EQ(depths[(size_t)2 * 5 + 2], 6.f);     // 中心 d=2
    const auto ink_grid = solid_grid(5, 5, {true});
    const auto ink_depths = voxel::compute_depths(ink_grid, def);
    EXPECT_FLOAT_EQ(ink_depths[(size_t)2 * 5 + 2], 7.f); // 墨线固定厚度
}

TEST(VoxelizerDepth, CapAndHemFirstMatchWins) {
    auto def = flat_def(4.f, 10.f, 10.f, 7.f);
    auto grid = solid_grid(5, 5);
    EXPECT_FLOAT_EQ(voxel::compute_depths(grid, def)[(size_t)2 * 5 + 2], 10.f);  // cap
    def = flat_def(3.f, 1.f, 50.f, 7.f);
    def.hem.push_back(VoxelHemStep{1, 5.f});
    def.hem.push_back(VoxelHemStep{3, 2.f});
    const auto depths = voxel::compute_depths(solid_grid(1, 5), def);
    EXPECT_FLOAT_EQ(depths[4], 8.f);   // 底行: 首步 +5
    EXPECT_FLOAT_EQ(depths[3], 8.f);
    EXPECT_FLOAT_EQ(depths[2], 5.f);   // 第二档 +2
    EXPECT_FLOAT_EQ(depths[0], 3.f);
}

TEST(VoxelizerMesh, IsolatedCellEmitsSixFaces) {
    const auto def = flat_def(6.f, 1.f, 50.f, 7.f);
    const auto grid = solid_grid(1, 1);
    const auto arrays =
        voxel::build_mesh_arrays(grid, voxel::compute_depths(grid, def), def, 0.f, 0.f);
    EXPECT_EQ(arrays.indices.size() / 6, 6u);            // 6 quad
    EXPECT_EQ(arrays.vertices.size() / 3, 24u);
    EXPECT_FLOAT_EQ(vertex_extent(arrays, 1, false), -3.f);
    EXPECT_FLOAT_EQ(vertex_extent(arrays, 1, true), 3.f);
    EXPECT_FLOAT_EQ(vertex_extent(arrays, 2, false), 0.f);
    EXPECT_FLOAT_EQ(vertex_extent(arrays, 2, true), 1.f);
}

TEST(VoxelizerMesh, SameDepthNeighborsCullSharedWalls) {
    const auto def = flat_def(6.f, 1.f, 50.f, 7.f);
    const auto grid = solid_grid(2, 1);
    const auto arrays = voxel::build_mesh_arrays(
        grid, voxel::compute_depths(grid, def), def, 0.f, 0.f);
    EXPECT_EQ(arrays.indices.size() / 6, 10u);           // 12 面 - 2 内壁
    const auto stack = solid_grid(1, 2);
    const std::vector<float> split_depths = {6.f, 2.f};  // 深度不同 → 墙保留
    const auto split_arrays =
        voxel::build_mesh_arrays(stack, split_depths, def, 0.f, 0.f);
    EXPECT_EQ(split_arrays.indices.size() / 6, 12u);
}

TEST(VoxelizerMesh, YOffsetShiftsDepthRange) {
    auto def = flat_def(2.f, 1.f, 50.f, 7.f);
    def.y_offset = 4.f;
    const auto grid = solid_grid(1, 1);
    const auto arrays = voxel::build_mesh_arrays(
        grid, voxel::compute_depths(grid, def), def, 0.f, 0.f);
    EXPECT_FLOAT_EQ(vertex_extent(arrays, 1, false), 3.f);
    EXPECT_FLOAT_EQ(vertex_extent(arrays, 1, true), 5.f);
}

TEST(VoxelDefs, SlotFromPathAndRealConfig) {
    EXPECT_EQ(voxel_slot_from_path("assets/sprites/player_part_arm.png"), "arm");
    EXPECT_EQ(voxel_slot_from_path("assets/sprites/boss_golem_part_weapon.png"), "weapon");
    EXPECT_EQ(voxel_slot_from_path("assets/sprites/foo.png"), "");
    std::string err;
    const auto config = load_voxel_parts_file("resources/voxel_parts.json", err);
    ASSERT_TRUE(config) << err;
    ASSERT_NE(config->find("head"), nullptr);
    ASSERT_NE(config->find("cape"), nullptr);
    EXPECT_FLOAT_EQ(config->find("torso")->base, 9.f);
    EXPECT_FLOAT_EQ(config->find("cape")->y_offset, 9.6f);
    ASSERT_EQ(config->find("cape")->hem.size(), 2u);
    EXPECT_EQ(voxel_slot_from_path("assets/sprites/mon_archer_part_leg.png"), "leg");
}

// 与 Blender 脚本 (voxel_lib.py + build_player_rig.py) 的逐件面数一致性:
// 同一批真实 PNG、同一套参数 → 引擎侧 quad 数必须逐字节一致 (漂移即回归)
TEST(VoxelizerParity, PlayerPartsMatchBlenderFaceCounts) {
    std::string err;
    const auto config = load_voxel_parts_file("resources/voxel_parts.json", err);
    ASSERT_TRUE(config) << err;
    struct PartCase { const char* path; const char* slot; int quads; };
    const PartCase cases[] = {
        {"assets/sprites/player_part_leg.png", "leg", 1054},
        {"assets/sprites/player_part_arm.png", "arm", 386},
        {"assets/sprites/player_part_weapon.png", "weapon", 424},
        {"assets/sprites/player_part_torso.png", "torso", 2288},
        {"assets/sprites/player_part_head.png", "head", 1602},
        {"assets/sprites/player_part_cape.png", "cape", 2230},
    };
    for (const auto& item : cases) {
        Image image = LoadImage(item.path);
        ASSERT_GT(image.width, 0) << item.path;
        const VoxelPartDef* def = config->find(item.slot);
        ASSERT_NE(def, nullptr) << item.slot;
        const auto grid = voxel::extract_grid(
            (const unsigned char*)image.data, image.width, image.height, *config);
        const auto depths = voxel::compute_depths(grid, *def);
        const auto arrays = voxel::build_mesh_arrays(grid, depths, *def, 0.f, 0.f);
        EXPECT_EQ((int)(arrays.indices.size() / 6), item.quads) << item.path;
        UnloadImage(image);
    }
}

// A13-M2: parts_key 推导 + per-actor 参数覆盖 (未覆盖字段继承默认槽)
TEST(VoxelDefs, PartsKeyAndPerActorOverrides) {
    EXPECT_EQ(voxel_parts_key_from_skeleton("resources/animations/mon_orc_skeleton.json"),
              "mon_orc");
    EXPECT_EQ(voxel_parts_key_from_skeleton(
                  "resources/animations/boss_demon_lord_skeleton.json"),
              "boss_demon_lord");
    EXPECT_EQ(voxel_parts_key_from_skeleton("resources/animations/foo.json"), "");
    const auto j = nlohmann::json::parse(R"({
        "parts": {"head": {"base": 9, "cap": 22}},
        "overrides": {"mon_slime": {"head": {"cap": 4}}}
    })");
    std::string err;
    const auto config = parse_voxel_config(j, err);
    ASSERT_TRUE(config) << err;
    EXPECT_FLOAT_EQ(config->find_for("mon_slime", "head")->cap, 4.f);
    EXPECT_FLOAT_EQ(config->find_for("mon_slime", "head")->base, 9.f);   // 未覆盖字段继承
    EXPECT_FLOAT_EQ(config->find_for("mon_orc", "head")->cap, 22.f);     // 无覆盖 = 默认
    const auto bad = nlohmann::json::parse(
        R"({"parts": {"head": {}}, "overrides": {"x": {"tail": {}}}})");
    EXPECT_FALSE(parse_voxel_config(bad, err));   // 覆盖未知槽 → 拒绝
}

// A13-M2: 全量部件 (30 怪 + 6 boss + 玩家) 与 Blender 批量参考面数一致。
// 参考文件由 art_src/scripts/voxel_batch_counts.py 生成 (voxel_lib.py 规则 = 真理源),
// 该脚本对 player/* 六件自检与硬编码基线一致后才输出。
TEST(VoxelizerParity, AllPartsMatchBlenderBatchCounts) {
    std::ifstream in("art_src/preview/voxel_faces_ref.json");
    ASSERT_TRUE(in.is_open())
        << "regenerate: blender -b -P art_src/scripts/voxel_batch_counts.py";
    const auto ref = nlohmann::json::parse(in, nullptr, false);
    ASSERT_FALSE(ref.is_discarded());
    ASSERT_GT(ref.size(), 100u);
    std::string err;
    const auto config = load_voxel_parts_file("resources/voxel_parts.json", err);
    ASSERT_TRUE(config) << err;
    for (const auto& item : ref.items())
        expect_batch_part(*config, item.key(), item.value().get<int>());
}
