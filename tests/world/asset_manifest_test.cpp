// G10.2-B1: Asset Manifest Foundation — 清单完整性校验 (分级红线)
// 事实源: G10_2_PIPELINE_DESIGN.md §4/§7 + STYLE_GUIDE.md 裁决记录
//
// 分级 (Review Gate 裁决 4):
//   ERROR   → 失败测试: 缺失资源 / ID 重复 / 非法路径 / 门态资源缺失 / 缩放违规
//   WARNING → 仅报告:  Vendor 未使用 / Candidate 未引用 / 共享文件
//   INFO    → 仅统计:  资产计数 / 尺寸分布
//
// 生命周期分类 (裁决: "零孤儿"不是绝对规则):
//   Managed Runtime Asset → 必须可引用 (assets/sprites/ 等)
//   Vendor Candidate      → 允许未引用 (lifecycle.vendor_candidate 前缀)
//   Deprecated            → 不允许被 Runtime 引用 (lifecycle.deprecated)
#include <gtest/gtest.h>

#include "resources/resource_manager.h"
#include "config.h"

#include <nlohmann/json.hpp>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace fs = std;

// 与其他测试一致: main.cpp 的字体全局桩化
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

namespace {

const char* kManifestPath = "resources/sprites.json";

nlohmann::json load_manifest() {
    std::ifstream in(kManifestPath);
    if (!in) return {};
    return nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/true);
}

// PNG IHDR (无 raylib 依赖)
bool png_dims(const std::string& path, int& w, int& h) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    unsigned char b[24] = {};
    in.read(reinterpret_cast<char*>(b), 24);
    if (!in || b[0] != 0x89 || b[1] != 'P') return false;
    w = b[16] * 16777216 + b[17] * 65536 + b[18] * 256 + b[19];
    h = b[20] * 16777216 + b[21] * 65536 + b[22] * 256 + b[23];
    return true;
}

bool valid_id(const std::string& id) {
    if (id.empty()) return false;
    size_t dot = id.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= id.size()) return false;
    auto ok_part = [](const std::string& s) {
        if (s.empty() || s[0] < 'a' || s[0] > 'z') return false;
        for (char c : s)
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
                return false;
        return true;
    };
    return ok_part(id.substr(0, dot)) && ok_part(id.substr(dot + 1));
}

// 收集 (id → file) 平面表: sprites 段 (legacy ID) + assets 段 (域化 ID)
struct Entry { std::string id, file, category; };
std::vector<Entry> collect(const nlohmann::json& m) {
    std::vector<Entry> out;
    if (m.contains("sprites"))
        for (auto& [k, v] : m["sprites"].items())
            out.push_back({k, v.value("file", ""), "sprites"});
    if (m.contains("assets"))
        for (auto& [cat, entries] : m["assets"].items())
            for (auto& [k, v] : entries.items())
                out.push_back({cat + "." + k, v.value("file", ""), cat});
    if (m.contains("skeleton_parts"))   // A5: 骨骼分件 (非 16px tile, 豁免 D2 红线)
        for (auto& [k, v] : m["skeleton_parts"].items())
            out.push_back({"skeleton_part." + k, v.value("file", ""), "skeleton_parts"});
    return out;
}

}  // namespace

// ── ERROR-1: 清单存在且为 schema v2 ──────────────────────────
TEST(AssetManifest, V2Parse) {
    auto m = load_manifest();
    ASSERT_FALSE(m.is_null()) << "manifest 不可读: " << kManifestPath;
    ASSERT_EQ(m.value("version", 1), 2) << "schema 版本必须 >= 2";
    ASSERT_TRUE(m.contains("sprites"));
    ASSERT_TRUE(m.contains("assets")) << "schema v2 需要 assets 类别化段";
    ASSERT_TRUE(m.contains("lifecycle")) << "schema v2 需要生命周期段";
}

// ── ERROR-2: 每个 file 都存在 + 路径卫生 ─────────────────────
TEST(AssetManifest, NoMissingFiles) {
    auto m = load_manifest();
    ASSERT_FALSE(m.is_null());
    for (auto& e : collect(m)) {
        ASSERT_FALSE(e.file.empty()) << e.id << ": 缺 file 字段";
        EXPECT_EQ(e.file.substr(0, 7), "assets/") << e.id << ": 路径必须以 assets/ 开头";
        EXPECT_EQ(e.file.find('\\'), std::string::npos) << e.id << ": 禁止反斜杠";
        EXPECT_EQ(e.file.find(".."), std::string::npos) << e.id << ": 禁止 ..";
        std::ifstream f(e.file);
        EXPECT_TRUE(f.good()) << e.id << " → 缺失资源: " << e.file;
    }
}

// ── ERROR-3: ID 规范 + 跨类别唯一 ────────────────────────────
TEST(AssetManifest, IdsValidAndUnique) {
    auto m = load_manifest();
    ASSERT_FALSE(m.is_null());
    std::set<std::string> seen;
    for (auto& e : collect(m)) {
        if (e.category != "sprites")   // legacy 键保持旧命名, assets 段强制域化规范
            EXPECT_TRUE(valid_id(e.id)) << "非法 ID: " << e.id;
        EXPECT_TRUE(seen.insert(e.id).second) << "ID 重复: " << e.id;
    }
}

// ── ERROR-4: 门语义 — 四态齐备, LOCKED 可与 CLOSED 区分 (D3) ──
TEST(AssetManifest, DoorSemantics) {
    auto m = load_manifest();
    ASSERT_FALSE(m.is_null());
    ASSERT_TRUE(m.contains("assets") && m["assets"].contains("door"))
        << "assets.door 类别缺失";
    auto& doors = m["assets"]["door"];
    for (const char* s : {"open", "closed", "locked", "sealed"})
        EXPECT_TRUE(doors.contains(s)) << "door." << s << " 缺失";
    ASSERT_TRUE(doors.contains("locked") && doors.contains("closed"));
    ASSERT_TRUE(doors["locked"].contains("render") &&
                doors["locked"]["render"].contains("overlay"))
        << "door.locked 必须带 overlay 渲染规则 (D3: 与 CLOSED 可区分)";
}

// ── ERROR-5: Managed 资产零孤儿 (sprites/ 全接线) ────────────
TEST(AssetManifest, ManagedNoOrphans) {
    auto m = load_manifest();
    ASSERT_FALSE(m.is_null());
    std::set<std::string> files;
    for (auto& e : collect(m)) files.insert(e.file);
    for (auto& p : std::filesystem::directory_iterator("assets/sprites")) {
        if (!p.is_regular_file() || p.path().extension() != ".png") continue;
        std::string rel = "assets/sprites/" + p.path().filename().string();
        EXPECT_TRUE(files.count(rel)) << "Managed 孤儿 (未接线): " << rel;
    }
}

// ── ERROR-6: Deprecated 资产禁止 Runtime 引用 ────────────────
TEST(AssetManifest, DeprecatedNotReferenced) {
    auto m = load_manifest();
    ASSERT_FALSE(m.is_null());
    std::vector<std::string> dep;
    if (m.contains("lifecycle") && m["lifecycle"].contains("deprecated"))
        for (auto& p : m["lifecycle"]["deprecated"])
            dep.push_back(p.get<std::string>());
    for (auto& e : collect(m))
        for (auto& d : dep)
            EXPECT_NE(e.file.find(d), 0u)
                << "Deprecated 路径被引用: " << e.file << " (" << e.id << ")";
}

// ── ERROR-7: 帧尺寸与 PNG 实际一致 + 缩放红线 (D2) ───────────
TEST(AssetManifest, SpriteFramesMatchPngAndScaleRedLine) {
    auto m = load_manifest();
    ASSERT_FALSE(m.is_null());
    ASSERT_TRUE(m.contains("sprites"));
    for (auto& [k, v] : m["sprites"].items()) {
        const int fw = v.value("frame_w", 16), fh = v.value("frame_h", 16);
        EXPECT_EQ(fw * 2, TILE_SIZE) << k << ": 非整数像素缩放 (D2 红线)";
        int w = 0, h = 0;
        ASSERT_TRUE(png_dims(v.value("file", ""), w, h)) << k << ": PNG 不可读";
        EXPECT_EQ(w, fw) << k << ": PNG 宽 " << w << " != frame_w " << fw;
        EXPECT_EQ(h, fh) << k << ": PNG 高 " << h << " != frame_h " << fh;
    }
    if (m.contains("skeleton_parts")) {   // A5: 分件尺寸一致性 (无 16px 红线)
        for (auto& [k, v] : m["skeleton_parts"].items()) {
            int w = 0, h = 0;
            ASSERT_TRUE(png_dims(v.value("file", ""), w, h)) << k << ": PNG 不可读";
            EXPECT_EQ(w, v.value("w", 0)) << k << ": PNG 宽 != 声明 w";
            EXPECT_EQ(h, v.value("h", 0)) << k << ": PNG 高 != 声明 h";
        }
    }
}

// ── ERROR-8: ID 查询 API (B1 新管线入口, 纯解析层) ───────────
TEST(AssetManifest, IdResolutionApi) {
    auto& rm = ResourceManager::inst();
    EXPECT_TRUE(rm.load_assets_config());
    const AssetDef* closed = rm.asset_by_id("door.closed");
    ASSERT_NE(closed, nullptr) << "door.closed 未装载";
    EXPECT_NE(closed->path.find("tile_0022.png"), std::string::npos);
    const AssetDef* locked = rm.asset_by_id("door.locked");
    ASSERT_NE(locked, nullptr);
    EXPECT_FALSE(locked->overlay.empty()) << "door.locked overlay 规则未装载";
    const AssetDef* opened = rm.asset_by_id("door.open");
    ASSERT_NE(opened, nullptr);
    EXPECT_TRUE(opened->overlay.empty()) << "door.open 不应有 overlay";
    EXPECT_EQ(rm.asset_by_id("door.nope"), nullptr);
}

// ── WARNING / INFO: 仅报告, 不失败 ───────────────────────────
TEST(AssetManifest, Report) {
    auto m = load_manifest();
    ASSERT_FALSE(m.is_null());
    auto entries = collect(m);
    std::set<std::string> files;
    for (auto& e : entries) files.insert(e.file);
    std::cout << "[INFO] manifest entries=" << entries.size()
              << " unique_files=" << files.size() << "\n";
    // WARNING: Vendor Candidate 未引用数 (允许, 仅报告)
    if (m.contains("lifecycle") && m["lifecycle"].contains("vendor_candidate"))
        for (auto& p : m["lifecycle"]["vendor_candidate"]) {
            std::string prefix = p.get<std::string>();
            int unused = 0;
            for (auto& f : std::filesystem::directory_iterator(prefix)) {
                if (!f.is_regular_file() || f.path().extension() != ".png") continue;
                std::string rel = prefix + "/" + f.path().filename().string();
                if (!files.count(rel)) unused++;
            }
            std::cout << "[WARNING] " << prefix << " unreferenced_png=" << unused
                      << " (Vendor Candidate, 允许)\n";
        }
    SUCCEED();
}
