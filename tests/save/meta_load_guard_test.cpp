// ═══════════════════════════════════════════════════════════════
// G49: MetaSystem::load 数组解析护栏
//
// 背景: meta_save.json 用的是手写 strstr 解析 (不用 JSON 库), 4 个数组
// (endings / nodes / hints / deaths) 的解析逻辑同构但各自指针递增。
// 这类代码的失败模式是**静默的** — 键名拼错、指针少推一格、边界判断错,
// 都只会让某项读不出来, 不报错也不崩。
//
// 故用真实文件直接驱动 load(), 断言每个数组都被正确读入。
// ═══════════════════════════════════════════════════════════════
#include <gtest/gtest.h>

#include "game/meta_progression.h"
#include "data/meta_node_defs.h"

#include <cstdio>
#include <fstream>
#include <string>

namespace {

// 写一份临时 meta_save.json 并加载, 返回解析结果。
// MetaSystem::load 的路径是硬编码的 saves/meta_save.json, 故先备份再替换,
// 用完还原 — 不污染玩家真实存档。
bool load_meta_from(const std::string& json, MetaSave& out) {
    const char* real = "saves/meta_save.json";
    std::ifstream src(real, std::ios::binary);
    std::string backup;
    if (src.is_open())
        backup.assign(std::istreambuf_iterator<char>(src),
                      std::istreambuf_iterator<char>());
    { std::ofstream dst(real); dst << json; }

    MetaSystem& ms = g_meta;
    ms.debug_reset_collection();
    ms._clear_death_history_for_test();
    // load_from_defs 依赖 meta_nodes.json 的 registry defs — 不加载则
    // get_meta_node_def 全返回 null, _node_count 保持 0
    load_meta_node_defs("resources/meta_nodes.json");
    ms.load_from_defs();
    const bool ok = ms.load();
    out = ms.data();

    if (!backup.empty()) {
        std::ofstream dst(real, std::ios::binary);
        dst << backup;
    }
    return ok;
}

}  // namespace

// ── META-001: 4 个数组全部非空时必须都被读出 ──
TEST(MetaSystemLoad, AllArraysParsed) {
    const std::string json = R"({
  "runs": 7,
  "soul": 12,
  "know": 3,
  "memory": 5,
  "nodes": [1,0,2,3],
  "best_floor": 6,
  "pity": 2,
  "endings": [1,3],
  "hints": ["hud_intro","element_select","combo_stage3"],
  "deaths": ["3|火焰陷阱","5|兽人"]
})";

    MetaSave s;
    ASSERT_TRUE(load_meta_from(json, s)) << "load() 返回 false — 文件未被读取";

    EXPECT_EQ(s.total_runs, 7);
    EXPECT_EQ(s.currency.soul_fragments, 12);
    EXPECT_EQ(s.currency.knowledge, 3);
    EXPECT_EQ(s.currency.ancient_memory, 5);
    EXPECT_EQ(s.best_floor, 6);
    EXPECT_EQ(s.challenge_pity_streak, 2);

    ASSERT_EQ(s.unlocked_endings.size(), 2u) << "endings 数组未读出";
    EXPECT_EQ(s.unlocked_endings[0], 1);
    EXPECT_EQ(s.unlocked_endings[1], 3);

    EXPECT_EQ(s.node_levels[0], 1) << "nodes[0] 未读出";
    EXPECT_EQ(s.node_levels[1], 0);
    EXPECT_EQ(s.node_levels[2], 2) << "nodes[2] 未读出 — 数组解析可能少推了一项";
    EXPECT_EQ(s.node_levels[3], 3);

    ASSERT_EQ(s.first_hints_shown.size(), 3u) << "hints 数组未读出 (键名拼错?)";
    EXPECT_TRUE(s.first_hints_shown["hud_intro"]);
    EXPECT_TRUE(s.first_hints_shown["element_select"]);
    EXPECT_TRUE(s.first_hints_shown["combo_stage3"]);

    ASSERT_EQ(s.death_history.size(), 2u) << "deaths 数组未读出";
    EXPECT_EQ(s.death_history[0].floor, 3);
    EXPECT_EQ(s.death_history[0].cause, "火焰陷阱");
    EXPECT_EQ(s.death_history[1].floor, 5);
    EXPECT_EQ(s.death_history[1].cause, "兽人");
}

// ── META-002: 数组缺省 / 超过 8 条上限 ──
TEST(MetaSystemLoad, MissingArraysAndDeathCap) {
    // 老档: 无 endings / hints / deaths 键
    const std::string old_json = R"({"runs":1,"soul":0,"know":0,"memory":0,"nodes":[],"best_floor":1})";
    MetaSave s;
    ASSERT_TRUE(load_meta_from(old_json, s));
    EXPECT_TRUE(s.unlocked_endings.empty()) << "无 endings 键应得空数组";
    EXPECT_TRUE(s.first_hints_shown.empty()) << "无 hints 键应得空表";
    EXPECT_TRUE(s.death_history.empty()) << "无 deaths 键应得空史 (天然兼容老档)";

    // deaths 超过 8 条: 丢最旧, 留最近 8 条 (与 record_death 环形上限一致)
    std::string many = R"({"deaths":[)";
    for (int i = 1; i <= 12; ++i) {
        if (i > 1) many += ",";
        many += "\"" + std::to_string(i) + "|c" + std::to_string(i) + "\"";
    }
    many += "]}";
    MetaSave s2;
    ASSERT_TRUE(load_meta_from(many, s2));
    ASSERT_EQ(s2.death_history.size(), 8u) << "deaths 应按 8 条上限截断";
    EXPECT_EQ(s2.death_history.front().floor, 5) << "截断应丢最旧 4 条, 保留 5..12";
    EXPECT_EQ(s2.death_history.back().floor, 12);
}

// ── META-003: 畸形项不得让 load 失败或写入脏数据 ──
TEST(MetaSystemLoad, MalformedEntriesAreSkipped) {
    const std::string json = R"({
  "endings": [1,2],
  "hints": ["ok",""],
  "deaths": ["bad_no_bar","7|死因"]
})";
    MetaSave s;
    ASSERT_TRUE(load_meta_from(json, s)) << "畸形项不应让 load 失败";
    EXPECT_EQ(s.unlocked_endings.size(), 2u);
    EXPECT_EQ(s.first_hints_shown.size(), 1u) << "空字符串 hint 应被跳过";
    ASSERT_EQ(s.death_history.size(), 1u) << "无 '|' 的项应被跳过";
    EXPECT_EQ(s.death_history[0].floor, 7);
}
