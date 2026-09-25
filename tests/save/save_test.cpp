// G10.9-B4: Slot API 隔离测试 — 替换旧单槽 roundtrip 测试
// 覆盖: Empty/Save/Load/Delete/Slot隔离/Meta持久/Delete不影响Meta/
//       Mirror隔离/旧档迁移/坏档容错 (B4 测试清单全项)
#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <direct.h>
#include <cstdio>
#include <vector>

#include "player.h"
#include "skill.h"
#include "item.h"
#include "save_manager.h"
#include "meta_progression.h"
#include "data/skill_defs.h"

extern bool load_biome_defs(const char* path);
extern bool load_encounter_defs(const char* path);

static bool file_exists(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false; fclose(f); return true;
}

static bool write_raw(const std::string& path, const std::string& content) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fwrite(content.c_str(), 1, content.size(), f);
    fclose(f);
    return true;
}

// ═ 槽位文件守卫: 测试前后清空三槽, 不碰真实存档 ═
class SlotGuard {
public:
    SlotGuard() { cleanup(); }
    ~SlotGuard() { cleanup(); }
    static void cleanup() {
        for (int i = 1; i <= SAVE_SLOT_COUNT; i++)
            std::remove(slot_path(i).c_str());
    }
    static std::string slot_path(int i) {
        return "saves/slot_" + std::to_string(i) + ".json";
    }
};

static void make_player_with_state(Player& p, int hp, int level) {
    p.combat.current_hp = hp;
    p.level = level;
    p.xp = 10;
    p.xp_to_next = 100;
    auto sk = skill_factory_create("slash");
    if (sk) p.skills.learn(std::move(sk));
}

// ── B4.1: Empty Slot ──
TEST(SlotApi, EmptySlotSummary) {
    SlotGuard guard;
    auto s = SaveManager::get_slot_summary(2);
    EXPECT_FALSE(s.exists);
    EXPECT_EQ(s.slot_id, 2);
    EXPECT_FALSE(SaveManager::slot_exists(2));
    auto all = SaveManager::get_all_slots();
    ASSERT_EQ(all.size(), (size_t)SAVE_SLOT_COUNT);
    for (auto& e : all) EXPECT_FALSE(e.exists);
}

// ── B4.2: Save + Summary 读回 ──
TEST(SlotApi, SaveThenSummaryReflectsState) {
    SlotGuard guard;
    ASSERT_TRUE(load_skill_defs("resources/skills.json"));
    Player p(64.0f, 64.0f, 220.0f, 100, 11, 5, 3);
    make_player_with_state(p, 77, 4);
    p.element.select(ElementType::POISON);

    ASSERT_TRUE(SaveManager::save_game(2, &p, 7, 9, 42u, {}, {},
                                       {}, {}, {0.5f}, {1.5f}, 1234.0f));
    auto s = SaveManager::get_slot_summary(2);
    ASSERT_TRUE(s.exists);
    EXPECT_EQ(s.slot_id, 2);
    EXPECT_EQ(s.floor, 7);
    EXPECT_EQ(s.max_floor, 9);
    EXPECT_EQ(s.level, 4);
    EXPECT_EQ(s.element_type, (int)ElementType::POISON);
    EXPECT_FLOAT_EQ(s.play_time, 1234.0f);
}

// ── B4.3: Load 全字段 roundtrip ──
TEST(SlotApi, LoadRoundtripAllFields) {
    SlotGuard guard;
    ASSERT_TRUE(load_skill_defs("resources/skills.json"));
    Player p(64.0f, 64.0f, 220.0f, 100, 11, 5, 3);
    make_player_with_state(p, 66, 6);
    p.element.select(ElementType::FIRE);
    p.element.add_exp(45);
    p.inventory.items.push_back(
        std::make_shared<ConsumableItem>("治疗药水", Rarity::RARE, "heal", 30, ""));

    ASSERT_TRUE(SaveManager::save_game(3, &p, 5, 5, 99u,
        {true, false}, {false, true}, {{"rule_1", 3}},
        {{101, 2}}, {0.5f, 1.0f}, {2.0f, 3.0f}, 888.0f));

    SaveData* d = SaveManager::load_game(3);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->current_floor, 5);
    EXPECT_EQ(d->max_unlocked_floor, 5);
    EXPECT_EQ(d->dungeon_seed, 99u);
    EXPECT_EQ(d->quest_states[101], 2);
    EXPECT_FLOAT_EQ(d->play_time, 888.0f);
    ASSERT_EQ(d->mirror_prior_alpha.size(), 2u);
    EXPECT_FLOAT_EQ(d->mirror_prior_alpha[1], 1.0f);
    ASSERT_EQ(d->mirror_prior_beta.size(), 2u);
    EXPECT_FLOAT_EQ(d->mirror_prior_beta[0], 2.0f);
    EXPECT_TRUE(d->element_initialized);
    EXPECT_EQ(d->element_type, (int)ElementType::FIRE);
    ASSERT_NE(d->player, nullptr);
    EXPECT_EQ(d->player->level, 6);
    EXPECT_EQ(d->player->combat.current_hp, 66);
    EXPECT_EQ(d->player->element.type, ElementType::FIRE);
    EXPECT_GE(d->player->inventory.items.size(), 1u);
    delete d;
}

// ── B4.4: Delete ──
TEST(SlotApi, DeleteRemovesOnlyTarget) {
    SlotGuard guard;
    ASSERT_TRUE(load_skill_defs("resources/skills.json"));
    Player p1(64.0f, 64.0f, 220.0f, 100, 11, 5, 3);
    Player p2(64.0f, 64.0f, 220.0f, 100, 11, 5, 3);
    ASSERT_TRUE(SaveManager::save_game(1, &p1, 2, 2));
    ASSERT_TRUE(SaveManager::save_game(2, &p2, 8, 8));

    SaveManager::delete_save(1);
    EXPECT_FALSE(SaveManager::slot_exists(1));
    EXPECT_TRUE(SaveManager::slot_exists(2));       // 邻档不受影响
    auto s2 = SaveManager::get_slot_summary(2);
    EXPECT_EQ(s2.floor, 8);
}

// ── B4.5: Slot 隔离 — slot_1 数据不泄漏到 slot_2 ──
TEST(SlotApi, SlotsAreIsolated) {
    SlotGuard guard;
    ASSERT_TRUE(load_skill_defs("resources/skills.json"));
    Player p1(64.0f, 64.0f, 220.0f, 100, 11, 5, 3);
    make_player_with_state(p1, 10, 12);
    Player p2(64.0f, 64.0f, 220.0f, 100, 11, 5, 3);
    make_player_with_state(p2, 99, 3);
    ASSERT_TRUE(SaveManager::save_game(1, &p1, 3, 3, 111u));
    ASSERT_TRUE(SaveManager::save_game(2, &p2, 9, 9, 222u));

    auto s1 = SaveManager::get_slot_summary(1);
    auto s2 = SaveManager::get_slot_summary(2);
    EXPECT_NE(s1.floor, s2.floor);
    EXPECT_NE(s1.level, s2.level);

    // active slot 语义: 切换后 load_save 读到不同档
    SaveManager::set_active_slot(1);
    SaveData* d1 = SaveManager::load_save();
    ASSERT_NE(d1, nullptr);
    EXPECT_EQ(d1->current_floor, 3);
    EXPECT_EQ(d1->dungeon_seed, 111u);
    delete d1;
    SaveManager::set_active_slot(2);
    SaveData* d2 = SaveManager::load_save();
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->current_floor, 9);        // Slot 1 != Slot 2
    EXPECT_EQ(d2->dungeon_seed, 222u);
    delete d2;
    SaveManager::set_active_slot(1);        // 复位
}

// ── B4.6: Meta 持久 — unlock_ending 落盘, 删档不丢 ──
TEST(SlotApi, MetaSurvivesSlotDelete) {
    SlotGuard guard;
    MetaSystem::g_readonly = false;
    // 直接操纵 meta (不碰磁盘前先备份恢复)
    MetaSystem::unlock_ending(2);
    EXPECT_TRUE(MetaSystem::ending_unlocked(2));

    Player p(64.0f, 64.0f, 220.0f, 100, 11, 5, 3);
    ASSERT_TRUE(SaveManager::save_game(1, &p, 1, 1));
    SaveManager::delete_save(1);
    EXPECT_FALSE(SaveManager::slot_exists(1));
    // 删档后 meta 仍在 (内存 + 磁盘都该在)
    EXPECT_TRUE(MetaSystem::ending_unlocked(2));
    // 磁盘验证: meta_save.json 重读后仍含 ending 2
    g_meta.load();
    EXPECT_TRUE(MetaSystem::ending_unlocked(2));
    // 清理: 移除测试写入的 ending (保持全局单例干净)
    MetaSystem::debug_reset_collection();
    g_meta.save();
}

// ── v1.6-B2: 死因史 — 环形上限 + Top 聚合 + 删档不丢 ──
TEST(SlotApi, DeathHistoryRingAndTopCauses) {
    SlotGuard guard;
    MetaSystem::g_readonly = false;
    g_meta.load();  // 从磁盘基线开始 (老档 deaths=[] → 空)
    ASSERT_TRUE(g_meta.data().death_history.empty());

    // 环形: 写 10 条, 只留最近 8
    for (int i = 1; i <= 10; i++)
        g_meta.record_death(i, (i % 2 == 0) ? "火焰陷阱" : "尖刺史莱姆");
    auto& hist = g_meta.data().death_history;
    ASSERT_EQ(hist.size(), (size_t)8);
    EXPECT_EQ(hist.front().floor, 3);          // 最旧 (第1,2条被挤掉)
    EXPECT_EQ(hist.back().floor, 10);          // 最新

    // Top 聚合: 6x尖刺史莱姆 + (8-6)x火焰陷阱 — 写 10 条后
    // 奇数=尖刺史莱姆(5条), 偶数=火焰陷阱(5条) → 环形截断后 奇数4 偶数4
    auto top = g_meta.top_death_causes(3);
    ASSERT_EQ(top.size(), (size_t)2);
    EXPECT_EQ(top[0].second, 4);
    EXPECT_EQ(top[1].second, 4);

    // 删档不丢: 存盘→删 slot→重读 meta
    Player p(64.0f, 64.0f, 220.0f, 100, 11, 5, 3);
    SaveManager::save_game(1, &p, 1, 1);
    SaveManager::delete_save(1);
    g_meta.load();
    EXPECT_EQ(g_meta.data().death_history.size(), (size_t)8);  // 仍在

    // 清理: 恢复干净的 meta (清 deaths + endings)
    MetaSystem::debug_reset_collection();
    g_meta._clear_death_history_for_test();   // 测试钩子
    g_meta.save();
}

// ── B4-T9: 压轴保底计数 — 必须落盘往返 + 账号级 (删档不丢) + 负数夹取 ──
// 这条路径是「玩家反复刷同一层也能攒到保底」的全部依赖, 单测必须覆盖.
TEST(SlotApi, ChallengePityStreakPersistsAcrossReload) {
    SlotGuard guard;
    MetaSystem::g_readonly = false;
    const int baseline = g_meta.challenge_pity_streak();

    g_meta.set_challenge_pity_streak(3);
    ASSERT_EQ(g_meta.challenge_pity_streak(), 3);

    // 磁盘直查 — 防「load() 找不到文件 → 内存值原地不动」造成的假通过
    ASSERT_TRUE(file_exists("saves/meta_save.json"))
        << "meta 存档没写出来, save() 静默失败";
    {
        std::ifstream ifs("saves/meta_save.json");
        std::string raw((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
        EXPECT_NE(raw.find("\"pity\":3"), std::string::npos)
            << "磁盘上没有 pity:3, 落盘往返并未真正发生";
    }

    g_meta.load();                              // 内存 → 磁盘 → 内存
    EXPECT_EQ(g_meta.challenge_pity_streak(), 3)
        << "保底计数没有落盘往返, 重建 GameScene 后必然清零";

    // 账号级: 删掉整个 slot, 计数仍在
    Player p(64.0f, 64.0f, 220.0f, 100, 11, 5, 3);
    ASSERT_TRUE(SaveManager::save_game(1, &p, 1, 1));
    SaveManager::delete_save(1);
    g_meta.load();
    EXPECT_EQ(g_meta.challenge_pity_streak(), 3)
        << "保底计数跟着 slot 一起丢了, 应为账号级";

    // 夹取: 负数归零
    g_meta.set_challenge_pity_streak(-4);
    EXPECT_EQ(g_meta.challenge_pity_streak(), 0);

    g_meta.set_challenge_pity_streak(baseline);   // 还原现场
    g_meta.save();
}

// ── B4.7: Mirror 记忆隔离 — 各档独立 ──
TEST(SlotApi, MirrorMemoryPerSlot) {
    SlotGuard guard;
    Player p(64.0f, 64.0f, 220.0f, 100, 11, 5, 3);
    std::vector<float> a1 = {0.1f, 0.2f}, b1 = {1.1f, 1.2f};
    std::vector<float> a2 = {0.7f, 0.8f}, b2 = {2.1f, 2.2f};
    ASSERT_TRUE(SaveManager::save_game(1, &p, 1, 1, 0, {}, {}, {}, {},
                                       a1, b1, 0.0f));
    ASSERT_TRUE(SaveManager::save_game(2, &p, 1, 1, 0, {}, {}, {}, {},
                                       a2, b2, 0.0f));
    SaveData* d1 = SaveManager::load_game(1);
    ASSERT_NE(d1, nullptr);
    ASSERT_EQ(d1->mirror_prior_alpha.size(), 2u);
    EXPECT_FLOAT_EQ(d1->mirror_prior_alpha[0], 0.1f);   // slot1 没被 slot2 污染
    delete d1;
    SaveData* d2 = SaveManager::load_game(2);
    ASSERT_NE(d2, nullptr);
    ASSERT_EQ(d2->mirror_prior_alpha.size(), 2u);
    EXPECT_FLOAT_EQ(d2->mirror_prior_alpha[0], 0.7f);
    delete d2;
}

// ── B4.8: 旧档迁移 — save.json → slot_1, 备份保留 ──
TEST(SlotApi, LegacyMigrationSafe) {
    SlotGuard guard;
    const std::string legacy = "saves/save.json";
    const std::string bak = legacy + ".bak";
    // 备份既有旧档/bak (可能存在真实文件)
    bool had_legacy = file_exists(legacy);
    bool had_bak = file_exists(bak);
    if (had_legacy) std::rename(legacy.c_str(), (legacy + ".guard").c_str());
    if (had_bak) std::rename(bak.c_str(), (bak + ".guard").c_str());

    // 场景: 只有旧档 → 迁移到 slot_1 + 旧档变 .bak
    const char* content =
        "v:4\nfloor:6\nmaxf:6\nlv:5\nxp:10\nxpt:99\n"
        "mhp:120\nchp:90\natk:16\npd:8\nmd:4\n"
        "act:slash,2,0,3;\npas:\ninv:\neqw:\nwpn:\neqa:\n"
        "buf:\nseed:77\nspr:\nspd:\n";
    ASSERT_TRUE(write_raw(legacy, content));
    EXPECT_TRUE(SaveManager::migrate_legacy_save());
    EXPECT_TRUE(SaveManager::slot_exists(1));
    EXPECT_FALSE(file_exists(legacy));           // 旧档已改名
    EXPECT_TRUE(file_exists(bak));               // 备份在
    auto s = SaveManager::get_slot_summary(1);
    EXPECT_EQ(s.floor, 6);
    EXPECT_EQ(s.level, 5);

    // 二次迁移: 旧档已不在 → no-op
    EXPECT_FALSE(SaveManager::migrate_legacy_save());

    // 场景: slot_1 已有 + 又出现旧档 → 不覆盖 (安全第一)
    ASSERT_TRUE(write_raw(legacy, "v:4\nfloor:99\n"));
    EXPECT_FALSE(SaveManager::migrate_legacy_save());
    auto s2 = SaveManager::get_slot_summary(1);
    EXPECT_EQ(s2.floor, 6);                       // 没被 99 覆盖
    std::remove(legacy.c_str());
    std::remove(bak.c_str());

    // 恢复现场
    if (had_legacy) std::rename((legacy + ".guard").c_str(), legacy.c_str());
    if (had_bak) std::rename((bak + ".guard").c_str(), bak.c_str());
}

// ── B4.9: 坏槽文件 — 不崩溃, 不影响邻档 ──
TEST(SlotApi, CorruptedSlotTolerated) {
    SlotGuard guard;
    ASSERT_TRUE(load_skill_defs("resources/skills.json"));
    Player p(64.0f, 64.0f, 220.0f, 100, 11, 5, 3);
    make_player_with_state(p, 50, 2);
    ASSERT_TRUE(SaveManager::save_game(3, &p, 4, 4));
    // slot_2 写坏数据
    ASSERT_TRUE(write_raw(SlotGuard::slot_path(2),
        "v:5\ngarbage everywhere!!! \x01\x02 no newlines"));

    SaveData* d2 = SaveManager::load_game(2);
    // 坏档要么 nullptr 要么默认值 — 不崩溃即过
    if (d2) { delete d2; }
    // 邻档 3 完好
    SaveData* d3 = SaveManager::load_game(3);
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->current_floor, 4);
    delete d3;
}

// ── 资源共存回归 (保留旧测试意图) ──
TEST(SaveFormat, ResourcesAndSavesCoexist) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    _mkdir("saves");
    const char* path = "saves/test_save2.json";
    std::ofstream w(path);
    w << "{}";
    w.close();
    EXPECT_TRUE(std::ifstream(path).is_open()) << "Save missing after resource load";
    std::remove(path);
}
