// A6-S2 批次9: spawn tables golden oracle
// 证明 JSON 驱动路径与旧 C++ 硬编码逐位等价 — 输出序列 + 掷骰计数双重比对。
// 旧逻辑在此逐字复刻为 oracle, 不引用任何待测代码。
#include <gtest/gtest.h>
#include <cstdint>
#include <fstream>
#include <cstdio>
#include <string>
#include <vector>
#include "spawn_tables.h"
#include "floor_config.h"
#include "combat_system.h"

namespace {

// ── 旧 floor_manager.cpp 内层 12 case switch, 逐字复刻 (r 由调用方注入) ──
const char* legacy_pick_slot(int i, int floor, uint32_t r) {
    switch (i) {
        case 0:  return (r % 3 == 0) ? "orc" : "slime";
        case 1:  return "archer";
        case 2:  return "shaman";
        case 3:  return "bomber";
        case 4:  return "tank";
        case 5:  return "elite";
        case 6:  return (floor >= 6 && floor <= 10 && r % 2 == 0) ? "lightning_orb" : "charger";
        case 7:  return "summoner";
        case 8:  return (r % 2 == 0) ? "skeleton_archer" : "goblin_hunter";
        case 9:  return (r % 2 == 0) ? "dark_mage" : "void_walker";
        case 10: return (r % 2 == 0) ? "shadow_assassin" : "night_stalker";
        case 11: return (r % 2 == 0) ? "stone_guardian" : "iron_sentinel";
        default: return "slime";
    }
}

// 旧 switch 对单个 (slot, floor) 的掷骰次数 — 注意 slot 6 的 && 短路语义
int expected_draws(int i, int floor) {
    if (i == 0) return 1;
    if (i == 6) return (floor >= 6 && floor <= 10) ? 1 : 0;
    if (i == 8 || i == 9 || i == 10 || i == 11) return 1;
    return 0;
}

// ── 旧 _pick_monster_type 完整版 (外层加权抽签 + 内层 switch), 逐字复刻 ──
const char* legacy_pick_monster_type(int floor) {
    const FloorConfig* cfg = get_floor_config(floor);
    if (!cfg) return "slime";
    int w[12];
    for (int i = 0; i < 12; i++) w[i] = cfg->enemy_weights[i];
    int total = 0;
    for (int i = 0; i < 12; i++) total += w[i];
    if (total <= 0) return "slime";
    int roll = (int)(rng() % (uint32_t)total);
    int sum = 0;
    for (int i = 0; i < 12; i++) {
        if (w[i] == 0) continue;
        sum += w[i];
        if (roll < sum) {
            switch (i) {
                case 0:  return (rng() % 3 == 0) ? "orc" : "slime";
                case 1:  return "archer";
                case 2:  return "shaman";
                case 3:  return "bomber";
                case 4:  return "tank";
                case 5:  return "elite";
                case 6:  return (cfg->floor >= 6 && cfg->floor <= 10 && rng() % 2 == 0) ? "lightning_orb" : "charger";
                case 7:  return "summoner";
                case 8:  return (rng() % 2 == 0) ? "skeleton_archer" : "goblin_hunter";
                case 9:  return (rng() % 2 == 0) ? "dark_mage" : "void_walker";
                case 10: return (rng() % 2 == 0) ? "shadow_assassin" : "night_stalker";
                case 11: return (rng() % 2 == 0) ? "stone_guardian" : "iron_sentinel";
            }
        }
    }
    return "slime";
}

// ── 新路径完整版: 外层与 legacy 逐行同构, 仅内层改调 pick_slot_monster ──
const char* fresh_pick_monster_type(int floor) {
    const FloorConfig* cfg = get_floor_config(floor);
    if (!cfg) return g_spawn_default.c_str();
    int w[12];
    for (int i = 0; i < 12; i++) w[i] = cfg->enemy_weights[i];
    int total = 0;
    for (int i = 0; i < 12; i++) total += w[i];
    if (total <= 0) return g_spawn_default.c_str();
    int roll = (int)(rng() % (uint32_t)total);
    int sum = 0;
    for (int i = 0; i < 12; i++) {
        if (w[i] == 0) continue;
        sum += w[i];
        if (roll < sum) {
            const std::string* id = pick_slot_monster(i, cfg->floor);
            return id ? id->c_str() : g_spawn_default.c_str();
        }
    }
    return g_spawn_default.c_str();
}

// ── 旧 challenge_room.cpp 9 个池, 逐字复刻 ──
const char* legacy_pick_challenge(int floor, int wave, uint32_t r) {
    struct Pool { const char* types[4]; int count; };
    auto pick = [](const Pool& p, uint32_t x) -> const char* {
        return p.types[x % (uint32_t)p.count];
    };
    if (floor <= 5) {
        const Pool pools[3] = {
            {{"slime", "skeleton_archer", "bone_soldier"}, 3},
            {{"orc", "shadow_stalker", "blood_leech"}, 3},
            {{"elite_slime", "charger", "summoner", "orc"}, 4},
        };
        return pick(pools[wave], r);
    }
    if (floor <= 10) {
        const Pool pools[3] = {
            {{"fire_imp", "bomber", "frost_slime", "lightning_orb"}, 4},
            {{"orc", "shaman", "poison_wyrm"}, 3},
            {{"storm_elemental", "golem", "necromancer"}, 3},
        };
        return pick(pools[wave], r);
    }
    const Pool pools[3] = {
        {{"shadow_stalker", "void_walker", "dark_mage"}, 3},
        {{"ice_warden", "blood_priest", "night_stalker"}, 3},
        {{"stone_guardian", "iron_sentinel", "elite_orc"}, 3},
    };
    return pick(pools[wave], r);
}

}  // namespace

class SpawnTablesTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(load_spawn_slots("resources/enemy_slots.json"));
        ASSERT_TRUE(load_challenge_pools("resources/challenge_pools.json"));
    }
};

TEST_F(SpawnTablesTest, SlotPickerMatchesLegacySwitch) {
    for (int i = 0; i < 12; i++) {
        for (int f = 1; f <= 15; f++) {
            for (int n = 0; n < 256; n++) {
                const uint32_t seed = 0x5EEDu * (uint32_t)(n + 1)
                                    + (uint32_t)i * 7919u
                                    + (uint32_t)f * 104729u;
                seed_rng(seed);
                const uint32_t r = rng();          // 与旧代码看到同一个值
                seed_rng(seed);
                const std::string* id = pick_slot_monster(i, f);
                ASSERT_NE(id, nullptr) << "slot=" << i << " floor=" << f;
                // 两个 const char* 分属堆与 .rodata, 必须按字符串内容比
                EXPECT_STREQ(id->c_str(), legacy_pick_slot(i, f, r))
                    << "slot=" << i << " floor=" << f << " n=" << n;
            }
        }
    }
}

TEST_F(SpawnTablesTest, SlotPickerDrawCountMatchesLegacy) {
    for (int i = 0; i < 12; i++) {
        for (int f = 1; f <= 15; f++) {
            seed_rng(0xA11CEu + (uint32_t)i * 463u + (uint32_t)f);
            const std::string* id = pick_slot_monster(i, f);
            ASSERT_NE(id, nullptr);
            EXPECT_EQ((int)rng.draws, expected_draws(i, f))
                << "slot=" << i << " floor=" << f << " got=" << *id;
        }
    }
}

TEST_F(SpawnTablesTest, FullPickSequenceMatchesLegacy) {
    const int kIters = 3000;
    for (int f = 1; f <= 15; f++) {
        const uint32_t seed = 0xC0FFEEu + (uint32_t)f * 7919u;

        seed_rng(seed);
        std::vector<std::string> legacy;
        legacy.reserve(kIters);
        for (int n = 0; n < kIters; n++) legacy.push_back(legacy_pick_monster_type(f));
        const uint64_t legacy_draws = rng.draws;

        seed_rng(seed);
        std::vector<std::string> fresh;
        fresh.reserve(kIters);
        for (int n = 0; n < kIters; n++) fresh.push_back(fresh_pick_monster_type(f));
        const uint64_t fresh_draws = rng.draws;

        EXPECT_EQ(legacy_draws, fresh_draws) << "floor=" << f;
        EXPECT_EQ(legacy, fresh) << "floor=" << f;
    }
}

TEST_F(SpawnTablesTest, ChallengePoolMatchesLegacy) {
    for (int f = 1; f <= 15; f++) {
        for (int w = 0; w < 3; w++) {
            for (int n = 0; n < 256; n++) {
                const uint32_t r = 0x9E3779B9u * (uint32_t)(n + 1)
                                 + (uint32_t)f * 31u + (uint32_t)w;
                const std::string* id = pick_challenge_monster(f, w, r);
                ASSERT_NE(id, nullptr) << "floor=" << f << " wave=" << w;
                EXPECT_STREQ(id->c_str(), legacy_pick_challenge(f, w, r))
                    << "floor=" << f << " wave=" << w << " n=" << n;
            }
        }
    }
}

TEST_F(SpawnTablesTest, LightningOrbFloorGate) {
    // F6-10: lightning_orb 与 charger 各占一半, 两者都必须出现
    for (int f = 6; f <= 10; f++) {
        int orbs = 0, chargers = 0;
        for (int n = 0; n < 200; n++) {
            seed_rng(0x0F100u + (uint32_t)f * 31u + (uint32_t)n);
            const std::string* id = pick_slot_monster(6, f);
            ASSERT_NE(id, nullptr);
            if (*id == "lightning_orb") orbs++;
            else if (*id == "charger") chargers++;
            else FAIL() << "F" << f << " 意外结果: " << *id;
        }
        EXPECT_GT(orbs, 0) << "F" << f;
        EXPECT_GT(chargers, 0) << "F" << f;
    }
    // F1-5 / F11-15: 槽位 6 候选被过滤后仅剩 charger, 且不掷骰
    for (int f = 1; f <= 15; f++) {
        if (f >= 6 && f <= 10) continue;
        for (int n = 0; n < 50; n++) {
            seed_rng(0x15A5Eu + (uint32_t)f * 31u + (uint32_t)n);
            const std::string* id = pick_slot_monster(6, f);
            ASSERT_NE(id, nullptr);
            EXPECT_EQ(*id, std::string("charger")) << "F" << f << " n=" << n;
            EXPECT_EQ((int)rng.draws, 0) << "F" << f << " 单候选不得掷骰";
        }
    }
}

TEST_F(SpawnTablesTest, MissingSlotFallsBack) {
    EXPECT_EQ(get_spawn_slot(-1), nullptr);
    EXPECT_EQ(get_spawn_slot(12), nullptr);
    EXPECT_EQ(get_spawn_slot(99), nullptr);
    EXPECT_EQ(pick_slot_monster(99, 5), nullptr);
    EXPECT_EQ(pick_challenge_monster(99, 0, 0), nullptr);   // 无 biome 覆盖 floor 99
    EXPECT_EQ(pick_challenge_monster(1, 99, 0), nullptr);   // wave 越界
    EXPECT_EQ(g_spawn_default, "slime");
}

TEST(SpawnTablesLoad, MissingFileReturnsFalseAndKeepsData) {
    ASSERT_TRUE(load_spawn_slots("resources/enemy_slots.json"));
    const size_t before = g_spawn_slots.size();
    EXPECT_EQ(before, 12u);
    EXPECT_FALSE(load_spawn_slots("resources/_no_such_spawn_tables.json"));
    EXPECT_EQ(g_spawn_slots.size(), before);
    EXPECT_EQ(g_spawn_default, "slime");
    EXPECT_TRUE(load_challenge_pools("resources/challenge_pools.json"));
    const size_t before_pools = g_challenge_pools.size();
    EXPECT_EQ(before_pools, 3u);
    EXPECT_FALSE(load_challenge_pools("resources/_no_such_challenge_pools.json"));
    EXPECT_EQ(g_challenge_pools.size(), before_pools);
}

TEST(SpawnTablesLoad, MalformedJsonReturnsFalseAndKeepsData) {
    ASSERT_TRUE(load_spawn_slots("resources/enemy_slots.json"));
    const size_t before = g_spawn_slots.size();
    const std::string bad = "build/_spawn_tables_bad_test.json";
    {
        std::ofstream out(bad);
        if (!out) GTEST_SKIP() << "build/ 不可写, 跳过畸形 JSON 用例";
        out << "{ this is not json";
    }
    EXPECT_FALSE(load_spawn_slots(bad.c_str()));
    EXPECT_EQ(g_spawn_slots.size(), before);
    EXPECT_EQ(g_spawn_default, "slime");
    std::remove(bad.c_str());
}
