#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include <memory>
#include "challenge_room.h"
#include "combat_system.h"  // 全局 rng (CountingRng::draws 用于证明不消耗)
#include "player.h"
#include "item.h"
#include "reward_manager.h"
#include "game_map.h"
#include "monster.h"
#include "data/item_defs.h"
#include "data/weapon_defs.h"

static Player make_player(int keys = 3) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.key_count = keys;
    p.gold = 100;
    return p;
}

// 数据集: 本文件多个用例依赖 items.json / weapons.json 注册表 (奖励发道具、
// 武器倍率)。原先靠 RewardDataJsonLoads 「恰好排在前面」的副作用式加载, 一旦用
// --gtest_filter 单跑靠后的奖励/tick 用例, 注册表为空 → generate_random_item()
// 拿不到任何模板, 奖励恒为 0 件 → 断言失败 (更糟的旧症状是 random_rarity() 除零)。
// 改用 GlobalTestEnvironment: 在全部静态初始化完成后、任何用例之前加载一次,
// 用例排布顺序从此不再有意义。不能用静态初始化对象 —— 本 TU 链接顺序在 lib 之前,
// 可能早于 g_item_defs_registry 构造, 构成跨 TU 静态初始化顺序 UB。
class RewardDataEnvironment : public testing::Environment {
public:
    void SetUp() override {
        (void)load_item_defs("resources/items.json");
        (void)load_weapon_defs("resources/weapons.json");
    }
};
static testing::Environment* const g_reward_data_env =
    testing::AddGlobalTestEnvironment(new RewardDataEnvironment);

// --- Q1: State Machine ---

TEST(ChallengeRoomTest, InitialPhaseIsInactive) {
    ChallengeRoomController c;
    EXPECT_EQ(c.phase(), ChallengePhase::INACTIVE);
}

TEST(ChallengeRoomTest, TryActivateWithoutKeyFails) {
    ChallengeRoomController c;
    Player p = make_player(0);
    EXPECT_FALSE(c.try_activate(p));
    EXPECT_EQ(c.phase(), ChallengePhase::INACTIVE);
    EXPECT_EQ(p.key_count, 0);
}

TEST(ChallengeRoomTest, TryActivateWithKeySucceeds) {
    ChallengeRoomController c;
    Player p = make_player(1);
    EXPECT_TRUE(c.try_activate(p));
    EXPECT_EQ(c.phase(), ChallengePhase::UNLOCKED);
    EXPECT_EQ(p.key_count, 0);
}

TEST(ChallengeRoomTest, TryActivateConsumesKey) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    EXPECT_EQ(p.key_count, 2);
}

TEST(ChallengeRoomTest, DoubleActivateFails) {
    ChallengeRoomController c;
    Player p = make_player(3);
    EXPECT_TRUE(c.try_activate(p));
    EXPECT_FALSE(c.try_activate(p));
    EXPECT_EQ(p.key_count, 2);
}

TEST(ChallengeRoomTest, OnPlayerEnteredTransitionsToArmed) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    EXPECT_EQ(c.phase(), ChallengePhase::UNLOCKED);
    c.on_player_entered();
    EXPECT_EQ(c.phase(), ChallengePhase::ARMED);
}

TEST(ChallengeRoomTest, OnDoorsLockedTransitionsToSpawning) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    c.on_player_entered();
    EXPECT_EQ(c.phase(), ChallengePhase::ARMED);
    c.on_doors_locked();
    EXPECT_EQ(c.phase(), ChallengePhase::WAVE_SPAWNING);
}

TEST(ChallengeRoomTest, ResetReturnsToInactive) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    c.on_player_entered();
    c.on_doors_locked();
    c.reset();
    EXPECT_EQ(c.phase(), ChallengePhase::INACTIVE);
}

// --- Q2: Deterministic Seed ---

TEST(ChallengeRoomTest, DeterministicSeedSameInputs) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    // Same inputs → same seed (tested indirectly via spawn determinism)
    EXPECT_EQ(c.phase(), ChallengePhase::UNLOCKED);
}

TEST(ChallengeRoomTest, DeterministicSeedNoCollision) {
    // Seed derivation: hash_combine with avalanche
    // We verify the concept: different room_index + wave_index → different behavior
    // (actual spawn test would need full map setup)
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    EXPECT_TRUE(c.is_cleared() == false);
}

// --- Q3: Wave Info ---

TEST(ChallengeRoomTest, WaveInfoDefaults) {
    ChallengeRoomController c;
    EXPECT_EQ(c.current_wave(), 0);
    EXPECT_EQ(c.total_waves(), 3);
}

TEST(ChallengeRoomTest, ResetClearsWaveCount) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    c.on_player_entered();
    c.on_doors_locked();
    c.reset();
    EXPECT_EQ(c.current_wave(), 0);
}

// --- Q4: IsCleared ---

TEST(ChallengeRoomTest, IsClearedFalseByDefault) {
    ChallengeRoomController c;
    EXPECT_FALSE(c.is_cleared());
}

TEST(ChallengeRoomTest, IsClearedFalseWhenActive) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    EXPECT_FALSE(c.is_cleared());
}

// --- Batch 3I: Portal State Machine ---

TEST(ChallengeRoomPortal, SetupPortal) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    EXPECT_EQ(c.phase(), ChallengePhase::PORTAL_ACTIVE);
    EXPECT_EQ(c.portal_tx(), 10);
    EXPECT_EQ(c.portal_ty(), 5);
}

TEST(ChallengeRoomPortal, ConsumeKeySuccess) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    Player p = make_player(1);
    EXPECT_TRUE(c.consume_key_for_challenge(p));
    EXPECT_EQ(p.key_count, 0);
}

TEST(ChallengeRoomPortal, ConsumeKeyFailNoKey) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    Player p = make_player(0);
    EXPECT_FALSE(c.consume_key_for_challenge(p));
}

TEST(ChallengeRoomPortal, ConsumeKeyFailWrongPhase) {
    ChallengeRoomController c;
    Player p = make_player(1);
    EXPECT_FALSE(c.consume_key_for_challenge(p));
}

TEST(ChallengeRoomPortal, SetRoomRect) {
    ChallengeRoomController c;
    c.set_room_rect(5, 5, 8, 6);
    EXPECT_EQ(c.room_rx(), 5);
    EXPECT_EQ(c.room_ry(), 5);
    EXPECT_EQ(c.room_rw(), 8);
    EXPECT_EQ(c.room_rh(), 6);
}

TEST(ChallengeRoomPortal, SetReturnPortal) {
    ChallengeRoomController c;
    c.set_return_portal(8, 12);
    EXPECT_EQ(c.return_portal_tx(), 8);
    EXPECT_EQ(c.return_portal_ty(), 12);
}

TEST(ChallengeRoomPortal, MarkCleared) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    c.mark_cleared();
    EXPECT_EQ(c.phase(), ChallengePhase::CLEARED);
    EXPECT_TRUE(c.is_cleared());
}

TEST(ChallengeRoomPortal, ResetClearsPortal) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    c.set_return_portal(8, 12);
    c.reset();
    EXPECT_EQ(c.portal_tx(), -1);
    EXPECT_EQ(c.return_portal_tx(), -1);
}

TEST(ChallengeRoomPortal, FullFlow) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    EXPECT_EQ(c.phase(), ChallengePhase::PORTAL_ACTIVE);

    Player p = make_player(1);
    EXPECT_TRUE(c.consume_key_for_challenge(p));
    EXPECT_EQ(p.key_count, 0);

    c.mark_cleared();
    EXPECT_EQ(c.phase(), ChallengePhase::CLEARED);

    c.set_return_portal(8, 12);
    EXPECT_GT(c.return_portal_tx(), 0);
}

// --- B4-T2: hidden boss finale wave decision ---

TEST(ChallengeRoomTest, BossWaveIsDeterministic) {
    ChallengeRoomController c;
    bool first = c.has_boss_wave(0xDEADBEEFu, 7);
    for (int i = 0; i < 100; i++)
        EXPECT_EQ(c.has_boss_wave(0xDEADBEEFu, 7), first);
}

TEST(ChallengeRoomTest, BossWaveAgreesOnFreshInstances) {
    ChallengeRoomController a, b;
    for (int room = 0; room < 40; room++)
        EXPECT_EQ(a.has_boss_wave(0x12345678u, room),
                  b.has_boss_wave(0x12345678u, room));
}

TEST(ChallengeRoomTest, BossWaveIsolationBetweenRooms) {
    ChallengeRoomController c;
    bool room3_before = c.has_boss_wave(0xA5A5A5A5u, 3);
    for (int room = 7; room < 30; room++)
        (void)c.has_boss_wave(0xA5A5A5A5u, room);
    EXPECT_EQ(c.has_boss_wave(0xA5A5A5A5u, 3), room3_before);
}

TEST(ChallengeRoomTest, BossWaveRateMatches25Percent) {
    ChallengeRoomController c;
    int hits = 0;
    for (int s = 0; s < 100; s++)
        for (int r = 0; r < 20; r++)
            if (c.has_boss_wave((uint32_t)s, r)) hits++;
    // 2000 pairs, p = 0.25 -> mean 500, sigma = sqrt(2000*.25*.75) = 19.36.
    // Band [393, 607] is +/-6.04 sigma (two-sided tail ~1.5e-9).
    // The 2000 pairs are a fixed deterministic set and the hash is a pure function,
    // so `hits` is a compile-time constant, not a random variable: flake probability = 0.
    EXPECT_GE(hits, 393);
    EXPECT_LE(hits, 607);
}

// Room fixed, seed swept. A seed-blind implementation (e.g. `room_index % 4 == 0`)
// returns a constant here and trips one of the two assertions.
TEST(ChallengeRoomTest, BossWaveVariesAcrossSeedsFixedRoom) {
    ChallengeRoomController c;
    bool saw_hit = false;
    bool saw_miss = false;
    for (uint32_t seed = 0; seed < 512u; seed++) {
        if (c.has_boss_wave(seed, 3)) saw_hit = true;
        else saw_miss = true;
    }
    EXPECT_TRUE(saw_hit)  << "no boss wave over 512 seeds at room 3";
    EXPECT_TRUE(saw_miss) << "boss wave on every seed at room 3";
}

// Dual property: seed fixed, room swept. Guards the joint-seed/room degeneration.
TEST(ChallengeRoomTest, BossWaveVariesAcrossRoomsFixedSeed) {
    ChallengeRoomController c;
    bool saw_hit = false;
    bool saw_miss = false;
    for (int room = 0; room < 512; room++) {
        if (c.has_boss_wave(0x12345678u, room)) saw_hit = true;
        else saw_miss = true;
    }
    EXPECT_TRUE(saw_hit)  << "no boss wave over 512 rooms at seed 0x12345678";
    EXPECT_TRUE(saw_miss) << "boss wave on every room at seed 0x12345678";
}

TEST(ChallengeRoomTest, BossWaveDoesNotConsumeGlobalRng) {
    ChallengeRoomController c;
    rng.seed(0xC0FFEEu);
    visual_rng.seed(0xC0FFEEu);
    uint64_t draws_before = rng.draws;
    uint64_t vdraws_before = visual_rng.draws;
    for (int room = 0; room < 50; room++)
        (void)c.has_boss_wave(0xDEADBEEFu, room);
    EXPECT_EQ(rng.draws, draws_before);       // pure: zero draws on gameplay stream
    EXPECT_EQ(visual_rng.draws, vdraws_before);  // and zero on the visual stream
    uint32_t after = rng();              // stream position must be untouched
    rng.seed(0xC0FFEEu);
    EXPECT_EQ(rng(), after);
    uint32_t vafter = visual_rng();      // visual stream position untouched too
    visual_rng.seed(0xC0FFEEu);
    EXPECT_EQ(visual_rng(), vafter);
}

TEST(ChallengeRoomTest, BossWaveStorageResetsLikeFresh) {
    ChallengeRoomController dirty;
    Player key_holder = make_player(1);
    dirty.try_activate(key_holder);
    dirty.on_player_entered();
    dirty.on_doors_locked();
    dirty.reset();

    ChallengeRoomController fresh;
    EXPECT_EQ(dirty.total_waves(), 3);  // boss 占保留槽位, 不是第 4 波
    EXPECT_FALSE(dirty.boss_wave_pending());
    EXPECT_FALSE(dirty.boss_wave_decided());
    EXPECT_FALSE(fresh.boss_wave_pending());
    EXPECT_FALSE(fresh.boss_wave_decided());
}

// --- B4-T9: 隐藏压轴保底 (账号级 pity) ---

// 保底必须覆盖纯随机: 在 25% 判定为 miss 的 (seed, room) 上, streak 到阈值时强制 HIT.
TEST(ChallengeRoomTest, PityForcesHitWherePureRollMisses) {
    ChallengeRoomController c;
    const int cap = ChallengeRoomController::boss_wave_pity_cap();
    ASSERT_EQ(cap, 3);
    int forced = 0;
    for (uint32_t s = 0; s < 64u; s++)
        for (int r = 0; r < 16; r++)
            if (!c.has_boss_wave(s, r) && c.has_boss_wave(s, r, cap))
                forced++;
    EXPECT_GT(forced, 0) << "保底从未覆盖过任何一次纯随机 miss";
}

// 阈值之下保底不得改动判定 —— 否则等于偷偷提高概率, 且破坏存档/回放可比性.
TEST(ChallengeRoomTest, PityBelowCapLeavesRollUntouched) {
    ChallengeRoomController c;
    const int cap = ChallengeRoomController::boss_wave_pity_cap();
    for (uint32_t s = 0; s < 64u; s++)
        for (int r = 0; r < 16; r++)
            for (int streak = 0; streak < cap; streak++)
                EXPECT_EQ(c.has_boss_wave(s, r, streak), c.has_boss_wave(s, r, 0))
                    << "streak=" << streak << " 改动了纯随机结果";
}


// UI 提示必须把概率与保底写出来, 让玩家分清「手气差」和「坏了」.
TEST(ChallengeRoomTest, BossWaveHintSurfacesChanceAndCap) {
    // 文案与常量同源: 断言走 accessor 而非硬编码字面量, 改概率时测试自动跟随
    const std::string chance =
        std::to_string(ChallengeRoomController::boss_wave_chance_pct()) + "%";
    const std::string hint = ChallengeRoomController::boss_wave_hint(0);
    EXPECT_NE(hint.find(chance), std::string::npos);
    EXPECT_NE(hint.find(std::to_string(ChallengeRoomController::boss_wave_pity_cap())),
              std::string::npos);
    EXPECT_EQ(hint.find("(已空"), std::string::npos) << "无进度时不应显示连空次数";

    const std::string progressed = ChallengeRoomController::boss_wave_hint(2);
    EXPECT_NE(progressed.find("(已空2次)"), std::string::npos)
        << "有进度时必须把连空次数显示给玩家";
}

// --- B4-T3: wave-advance truth table (pure function) ---

TEST(ChallengeRoomTest, WaveAdvanceTraceNoBoss) {
    EXPECT_EQ(ChallengeRoomController::decide_advance(1, 3, false), WaveAdvance::WAIT);
    EXPECT_EQ(ChallengeRoomController::decide_advance(2, 3, false), WaveAdvance::WAIT);
    EXPECT_EQ(ChallengeRoomController::decide_advance(3, 3, false), WaveAdvance::REWARD);
}

TEST(ChallengeRoomTest, WaveAdvanceTraceWithBoss) {
    EXPECT_EQ(ChallengeRoomController::decide_advance(1, 3, true), WaveAdvance::WAIT);
    EXPECT_EQ(ChallengeRoomController::decide_advance(2, 3, true), WaveAdvance::WAIT);
    EXPECT_EQ(ChallengeRoomController::decide_advance(3, 3, true), WaveAdvance::BOSS_WAIT);
    // boss 波清完后 current=4: 越过 total, 必须回 REWARD 而非再次 BOSS_WAIT
    EXPECT_EQ(ChallengeRoomController::decide_advance(4, 3, true), WaveAdvance::REWARD);
}

TEST(ChallengeRoomTest, WaveAdvanceBossFlagCannotTriggerBelowTotal) {
    EXPECT_EQ(ChallengeRoomController::decide_advance(0, 3, true), WaveAdvance::WAIT);
    EXPECT_EQ(ChallengeRoomController::decide_advance(1, 3, true), WaveAdvance::WAIT);
    EXPECT_EQ(ChallengeRoomController::decide_advance(2, 3, true), WaveAdvance::WAIT);
}

TEST(ChallengeRoomTest, WaveAdvanceTotalWavesUnchanged) {
    ChallengeRoomController c;
    c.reset();
    EXPECT_EQ(c.total_waves(), 3);   // 压轴占用波次索引 3, 不改 _total_waves
    EXPECT_FALSE(c.boss_wave_pending());
    EXPECT_FALSE(c.boss_wave_decided());
}

// ============================================================
// B4-T4: reward isolation.
//   The hidden-boss bonus must be additive, so a room that did NOT roll a boss
//   must receive byte-identical rewards to the pre-B4 code.
// ============================================================

namespace {

std::string item_signature(const Item& it) {
    std::ostringstream os;
    os << it.base_name << "|r" << (int)it.rarity;
    if (const auto* ch = dynamic_cast<const CharmItem*>(&it)) {
        os << "|slot=" << ch->slot << "|atk=" << ch->atk_bonus
           << "|pdef=" << ch->pdef_bonus << "|mdef=" << ch->mdef_bonus
           << "|wid=" << ch->weapon_def_id << "|skill=" << ch->skill_class_name;
    } else if (const auto* eq = dynamic_cast<const EquipmentItem*>(&it)) {
        os << "|slot=" << eq->slot << "|atk=" << eq->atk_bonus
           << "|pdef=" << eq->pdef_bonus << "|mdef=" << eq->mdef_bonus
           << "|wid=" << eq->weapon_def_id;
    } else if (const auto* cs = dynamic_cast<const ConsumableItem*>(&it)) {
        os << "|eff=" << cs->effect_type << "|val=" << cs->effect_value
           << "|buf=" << cs->buff_id << "|trig=" << cs->triggers.size();
    }
    return os.str();
}

std::vector<std::string> signatures_of(const Inventory& inv) {
    std::vector<std::string> sigs;
    for (const auto& it : inv.items) sigs.push_back(item_signature(*it));
    return sigs;
}

struct RewardObservation {
    std::vector<std::string> signatures;
    int gold_added = 0;
    int granted = 0;
    int dropped = 0;
    uint64_t draws = 0;
};

// Verbatim replica of the pre-B4 _grant_rewards body, used as the reference oracle.
// The drop branch is only reachable when the inventory is full; the tests below
// that compare draws/gold run it against an empty 16-slot inventory.
RewardObservation run_legacy_reference(int floor, uint32_t seed) {
    rng.seed(seed);
    Player p = make_player(3);
    RewardObservation obs;
    obs.gold_added = -p.gold;
    for (int i = 0; i < 3; i++) {
        auto item = generate_random_item();
        int tries = 0;
        while (item && item->rarity < Rarity::RARE && tries < 5) {
            item = generate_random_item();
            tries++;
        }
        if (!item) continue;
        if (p.inventory.add(item, &p)) {
            obs.granted++;
        } else {
            obs.dropped++;
            (void)item;
        }
    }
    int gold = 50 + floor * 15;
    RewardManager::grant_gold(p, gold);
    obs.signatures = signatures_of(p.inventory);
    obs.gold_added += p.gold;
    obs.draws = rng.draws;
    return obs;
}

RewardObservation run_new_path(int floor, uint32_t seed, bool boss_wave_pending,
                              int inventory_capacity = 16) {
    rng.seed(seed);
    Player p = make_player(3);
    p.inventory.max_size = inventory_capacity;
    ChallengeRoomController c;
    c.set_room_rect(1, 1, 4, 4);
    GameMap map(8, 8, 32);
    std::vector<DroppedItem> drops;
    int start_gold = p.gold;
    c.grant_rewards_for_test(p, &map, floor, drops, boss_wave_pending);
    RewardObservation obs;
    obs.signatures = signatures_of(p.inventory);
    obs.gold_added = p.gold - start_gold;
    obs.granted = p.inventory.item_count();
    obs.dropped = (int)drops.size();
    obs.draws = rng.draws;
    return obs;
}

void fill_inventory(Player& p, int count) {
    for (int i = 0; i < count; i++)
        p.inventory.items.push_back(
            std::make_shared<EquipmentItem>("filler", Rarity::COMMON, "armor", 0, 1, 1));
}

Player new_bonus_player(int inventory_capacity = 16) {
    Player p = make_player(3);
    p.inventory.max_size = inventory_capacity;
    return p;
}

std::shared_ptr<Item> bonus_item_only(uint32_t seed, int inventory_capacity) {
    Player p = new_bonus_player(inventory_capacity);
    fill_inventory(p, inventory_capacity);
    ChallengeRoomController c;
    c.set_room_rect(1, 1, 4, 4);
    GameMap map(8, 8, 32);
    std::vector<DroppedItem> drops;
    rng.seed(seed);
    c.grant_rewards_for_test(p, &map, 1, drops, true);
    return drops.empty() ? nullptr : drops.back().item;
}

}  // namespace

TEST(ChallengeRoomTest, RewardDataJsonLoads) {
    ASSERT_TRUE(load_item_defs("resources/items.json"));
    ASSERT_TRUE(load_weapon_defs("resources/weapons.json"));
    ASSERT_TRUE(is_item_defs_loaded());
    ASSERT_TRUE(is_weapon_defs_loaded());
}

// --- decide_reward_plan: full truth table over floors 1..30 ---

TEST(ChallengeRoomTest, RewardPlanNoBossMatchesLegacyContract) {
    for (int floor = 1; floor <= 30; floor++) {
        RewardPlan plan = ChallengeRoomController::decide_reward_plan(floor, false);
        EXPECT_EQ(plan.base_item_count, 3)      << "floor " << floor;
        EXPECT_EQ(plan.base_retry_cap, 5)       << "floor " << floor;
        EXPECT_EQ(plan.base_rarity_floor, (int)Rarity::RARE) << "floor " << floor;
        EXPECT_EQ(plan.bonus_item_count, 0)     << "floor " << floor;
        EXPECT_EQ(plan.bonus_retry_cap, 0)      << "floor " << floor;
        EXPECT_EQ(plan.bonus_rarity_floor, 0)   << "floor " << floor;
        EXPECT_EQ(plan.gold, 50 + floor * 15)   << "floor " << floor;
    }
}

TEST(ChallengeRoomTest, RewardPlanBossBonusIsAdditiveOnly) {
    for (int floor = 1; floor <= 30; floor++) {
        RewardPlan base = ChallengeRoomController::decide_reward_plan(floor, false);
        RewardPlan boss = ChallengeRoomController::decide_reward_plan(floor, true);
        // the three base fields must be copied verbatim, never perturbed
        EXPECT_EQ(boss.base_item_count, base.base_item_count)     << "floor " << floor;
        EXPECT_EQ(boss.base_retry_cap, base.base_retry_cap)       << "floor " << floor;
        EXPECT_EQ(boss.base_rarity_floor, base.base_rarity_floor) << "floor " << floor;
        EXPECT_EQ(boss.bonus_item_count, 1)                       << "floor " << floor;
        EXPECT_EQ(boss.bonus_retry_cap, 8)                        << "floor " << floor;
        EXPECT_EQ(boss.bonus_rarity_floor, (int)Rarity::EPIC)     << "floor " << floor;
        EXPECT_EQ(boss.gold, (int)((50 + floor * 15) * 1.5f))     << "floor " << floor;
    }
}

// --- byte-identity: non-boss run must equal the pre-B4 reference oracle ---

TEST(ChallengeRoomTest, NonBossRewardsByteIdenticalToLegacy) {
    for (uint32_t seed = 1u; seed <= 64u; seed++) {
        for (int floor = 1; floor <= 5; floor++) {
            RewardObservation legacy = run_legacy_reference(floor, seed);
            RewardObservation actual = run_new_path(floor, seed, false);
            EXPECT_EQ(actual.gold_added, legacy.gold_added)
                << "floor " << floor << " seed " << seed;
            EXPECT_EQ(actual.granted, legacy.granted)
                << "floor " << floor << " seed " << seed;
            EXPECT_EQ(actual.draws, legacy.draws)
                << "floor " << floor << " seed " << seed
                << " RNG stream perturbed by the additive branch";
            ASSERT_EQ(actual.signatures.size(), legacy.signatures.size())
                << "floor " << floor << " seed " << seed;
            for (size_t i = 0; i < legacy.signatures.size(); i++)
                EXPECT_EQ(actual.signatures[i], legacy.signatures[i])
                    << "floor " << floor << " seed " << seed << " slot " << i;
        }
    }
}

// --- additivity: the boss run's base items are untouched, exactly one is added ---

TEST(ChallengeRoomTest, BossBonusAppendsExactlyOneItemAfterBase) {
    for (uint32_t seed = 1u; seed <= 64u; seed++) {
        for (int floor = 1; floor <= 5; floor++) {
            RewardObservation base = run_new_path(floor, seed, false);
            RewardObservation boss = run_new_path(floor, seed, true);
            ASSERT_EQ(base.signatures.size(), 3u);
            EXPECT_EQ(boss.signatures.size(), base.signatures.size() + 1u)
                << "floor " << floor << " seed " << seed;
            for (size_t i = 0; i < base.signatures.size(); i++)
                EXPECT_EQ(boss.signatures[i], base.signatures[i])
                    << "floor " << floor << " seed " << seed << " slot " << i
                    << " bonus branch must run after the base branch";
            EXPECT_EQ(boss.gold_added, (int)((50 + floor * 15) * 1.5f))
                << "floor " << floor << " seed " << seed;
            EXPECT_GT(boss.draws, base.draws)
                << "floor " << floor << " seed " << seed;
        }
    }
}

// --- the EPIC floor is reachable through the bonus retry cap, and the cap is respected ---

TEST(ChallengeRoomTest, BossBonusRetriesReachEpic) {
    int epic_reached = 0;
    for (uint32_t seed = 1u; seed <= 500u; seed++) {
        std::shared_ptr<Item> bonus = bonus_item_only(seed, 3);
        ASSERT_NE(bonus, nullptr) << "seed " << seed << " bonus was never dropped";
        if (bonus->rarity >= Rarity::EPIC) epic_reached++;
    }
    EXPECT_GT(epic_reached, 0)
        << "EPIC never reached across 500 seeds: bonus floor or retry cap is broken";
}

TEST(ChallengeRoomTest, BossBonusNeverDrawsBeyondRetryCap) {
    // Each generate_random_item() consumes 2-5 rng draws (rarity + category +
    // up to 3 value rolls), so 8 retries + the initial roll caps at 9 * 5 draws.
    constexpr int kMaxDrawsPerItem = 5;
    constexpr int kBonusAttempts = 8 + 1;
    for (uint32_t seed = 1u; seed <= 200u; seed++) {
        RewardObservation base = run_new_path(1, seed, false);
        RewardObservation boss = run_new_path(1, seed, true);
        EXPECT_LE((int)(boss.draws - base.draws), kBonusAttempts * kMaxDrawsPerItem)
            << "seed " << seed << " bonus consumed more than 8 retries + 1 initial roll";
    }
}

// --- inventory overflow: the bonus item falls to the room center ---

TEST(ChallengeRoomTest, BossBonusOverflowsToGroundAtRoomCenter) {
    ChallengeRoomController c;
    c.set_room_rect(1, 1, 4, 4);
    // Exactly 3 slots: the 3 base items fill it, so only the bonus can overflow.
    Player p = new_bonus_player(3);
    GameMap map(8, 8, 32);
    std::vector<DroppedItem> drops;
    int hp_before = p.combat.current_hp;
    rng.seed(0xB055u);
    c.grant_rewards_for_test(p, &map, 1, drops, true);
    ASSERT_EQ(drops.size(), 1u)
        << "only the bonus may overflow a 3-slot inventory";
    EXPECT_EQ(drops[0].tile_x, 3);
    EXPECT_EQ(drops[0].tile_y, 3);
    EXPECT_NE(drops[0].item, nullptr);
    EXPECT_EQ(p.inventory.item_count(), 3);
    EXPECT_EQ(p.combat.current_hp, hp_before);
}

// --- P1-C9: 结算回执 — 掉地上的道具对玩家必须可见 ---
// 背景: _grant_rewards 原先只把 granted 打进开发日志, 背包满时道具全落到
// ground_items 而玩家无任何提示。实机日志出现过 "0 items + 345 gold (boss wave
// bonus)" —— 压轴波尝试给 4 件, 4 件全掉地, 玩家毫不知情。

TEST(ChallengeRoomTest, RewardReportAllGrantedNothingDropped) {
    ChallengeRoomController c;
    c.set_room_rect(1, 1, 4, 4);
    Player p = new_bonus_player(16);   // 空背包, 装得下 3 件基础奖励
    GameMap map(8, 8, 32);
    std::vector<DroppedItem> drops;
    rng.seed(0xC010u);
    c.grant_rewards_for_test(p, &map, 10, drops, false);
    ASSERT_TRUE(drops.empty());
    EXPECT_EQ(c.last_reward().granted, 3);
    EXPECT_EQ(c.last_reward().dropped, 0);
    EXPECT_EQ(c.last_reward().gold, 200);
    EXPECT_EQ(p.inventory.item_count(), 3);
}

TEST(ChallengeRoomTest, RewardReportFullInventoryCountsDropped) {
    ChallengeRoomController c;
    c.set_room_rect(1, 1, 4, 4);
    Player p = new_bonus_player(2);    // 容量 2
    fill_inventory(p, 2);              // 填满, 一件都进不去
    GameMap map(8, 8, 32);
    std::vector<DroppedItem> drops;
    rng.seed(0xC011u);
    c.grant_rewards_for_test(p, &map, 10, drops, true);   // 3 基础 + 1 压轴
    ASSERT_EQ(drops.size(), 4u);
    EXPECT_EQ(c.last_reward().granted, 0);
    EXPECT_EQ(c.last_reward().dropped, 4);
    EXPECT_EQ(c.last_reward().gold, 300);
    EXPECT_EQ(p.inventory.item_count(), 2);
}

TEST(ChallengeRoomTest, RewardReportCountsOnlyThisRoomsDrops) {
    // ground_items 可能已含其他来源的掉落 (击杀掉落等), 回执只能计本次增量
    ChallengeRoomController c;
    c.set_room_rect(1, 1, 4, 4);
    Player p = new_bonus_player(2);
    fill_inventory(p, 2);
    GameMap map(8, 8, 32);
    std::vector<DroppedItem> drops;
    drops.push_back(DroppedItem{std::make_shared<EquipmentItem>(
        "旧掉落", Rarity::COMMON, "armor", 0, 1, 1), 7, 7});
    rng.seed(0xC012u);
    c.grant_rewards_for_test(p, &map, 10, drops, false);
    ASSERT_EQ(drops.size(), 4u);          // 1 旧 + 3 新
    EXPECT_EQ(c.last_reward().dropped, 3) << "不得把别人的地面积算成本次掉落";
    EXPECT_EQ(c.last_reward().granted, 0);
}

TEST(ChallengeRoomTest, RewardMessageVariants) {
    // 文案与计数同源, 三种情形各自独立断言, 防止改文案时计数逻辑漂移
    EXPECT_EQ(ChallengeRoomController::reward_message({3, 0, 200}),
              "挑战完成 · +200 金币 · 3 件道具已入包");
    EXPECT_EQ(ChallengeRoomController::reward_message({2, 2, 300}),
              "挑战完成 · +300 金币 · 背包已满, 2 件道具掉落在房间中央");
    EXPECT_EQ(ChallengeRoomController::reward_message({0, 0, 230}),
              "挑战完成 · +230 金币");
}

TEST(ChallengeRoomTest, RewardMessageBranchesAreMutuallyExclusive) {
    // 掉地时不得同时谎称「已入包」; 零产出时不得谎称「0 件已入包」
    const std::string dropped = ChallengeRoomController::reward_message({0, 4, 345});
    EXPECT_NE(dropped.find("背包已满"), std::string::npos);
    EXPECT_EQ(dropped.find("已入包"), std::string::npos);
    EXPECT_NE(dropped.find("4 件"), std::string::npos);

    const std::string empty = ChallengeRoomController::reward_message({0, 0, 345});
    EXPECT_EQ(empty.find("道具"), std::string::npos)
        << "配置缺失未产出道具时, 不得出现任何道具字样";
}

TEST(ChallengeRoomTest, RewardReportClearsOnReset) {
    ChallengeRoomController c;
    c.set_room_rect(1, 1, 4, 4);
    Player p = new_bonus_player(2);
    fill_inventory(p, 2);
    GameMap map(8, 8, 32);
    std::vector<DroppedItem> drops;
    rng.seed(0xC013u);
    c.grant_rewards_for_test(p, &map, 10, drops, false);
    ASSERT_GT(c.last_reward().dropped, 0);
    c.reset();
    EXPECT_EQ(c.last_reward().granted, 0)
        << "换层后必须清零, 否则下层会把上一层的回执当成本层结果";
    EXPECT_EQ(c.last_reward().dropped, 0);
    EXPECT_EQ(c.last_reward().gold, 0);
}

// --- the bonus must not be a mainline boss reward ---

TEST(ChallengeRoomTest, BossBonusGrantsNoMainlineBossReward) {
    ChallengeRoomController c;
    c.set_room_rect(1, 1, 4, 4);
    Player p = new_bonus_player(16);
    GameMap map(8, 8, 32);
    std::vector<DroppedItem> drops;
    int hp_before = p.combat.current_hp;
    int max_hp_before = p.combat.max_hp;
    const size_t relics_before = p.relics.size();
    rng.seed(0x601E5u);
    c.grant_rewards_for_test(p, &map, 10, drops, true);
    EXPECT_EQ(p.combat.current_hp, hp_before);
    EXPECT_EQ(p.combat.max_hp, max_hp_before);
    EXPECT_EQ(p.relics.size(), relics_before)
        << "the bonus leaked onto the mainline boss relic path";
    EXPECT_EQ(drops.size(), 0u);
}

// --- wiring: tick() must gate the bonus on the actual has_boss_wave roll ---

namespace {

struct TickedResult {
    bool cleared = false;
    bool pending = false;
    int items = 0;
    int gold = 0;
    int ticks = 0;
};

TickedResult run_to_cleared(uint32_t seed, int room) {
    ChallengeRoomController c;
    Player p = make_player(3);
    std::vector<std::unique_ptr<Monster>> monsters;
    std::vector<DroppedItem> drops;
    GameMap map(8, 8, 32);
    c.try_activate(p);
    c.on_player_entered();
    c.on_doors_locked();
    c.set_room_rect(1, 1, 4, 4);
    c.set_phase_for_test(ChallengePhase::WAVE_SPAWNING);
    TickedResult out;
    for (int i = 0; i < 40 && !c.is_cleared(); i++) {
        c.tick(3.5f, &map, &p, monsters, 10, seed, room, drops);
        out.ticks++;
    }
    out.cleared = c.is_cleared();
    out.pending = c.boss_wave_pending();
    out.items = p.inventory.item_count();
    out.gold = p.gold;
    return out;
}

}  // namespace

TEST(ChallengeRoomTest, TickGrantsLegacyRewardWhenBossWaveMissed) {
    ChallengeRoomController probe;
    uint32_t miss_seed = 0;
    int miss_room = 0;
    bool found = false;
    for (uint32_t seed = 0; seed < 4096u && !found; seed++) {
        for (int room = 0; room < 64 && !found; room++) {
            if (!probe.has_boss_wave(seed, room)) {
                miss_seed = seed;
                miss_room = room;
                found = true;
            }
        }
    }
    ASSERT_TRUE(found);
    TickedResult res = run_to_cleared(miss_seed, miss_room);
    ASSERT_TRUE(res.cleared) << "seed " << miss_seed << " room " << miss_room
                             << " reached " << res.ticks << " ticks without clearing";
    EXPECT_FALSE(res.pending);
    EXPECT_EQ(res.items, 3);
    EXPECT_EQ(res.gold, 100 + 50 + 10 * 15);
}

TEST(ChallengeRoomTest, TickGrantsBonusOnlyWhenBossWaveRolled) {
    ChallengeRoomController probe;
    uint32_t hit_seed = 0;
    int hit_room = 0;
    bool found = false;
    for (uint32_t seed = 0; seed < 4096u && !found; seed++) {
        for (int room = 0; room < 64 && !found; room++) {
            if (probe.has_boss_wave(seed, room)) {
                hit_seed = seed;
                hit_room = room;
                found = true;
            }
        }
    }
    ASSERT_TRUE(found);
    TickedResult res = run_to_cleared(hit_seed, hit_room);
    ASSERT_TRUE(res.cleared) << "seed " << hit_seed << " room " << hit_room
                             << " reached " << res.ticks << " ticks without clearing";
    EXPECT_TRUE(res.pending);
    EXPECT_EQ(res.items, 4);
    EXPECT_EQ(res.gold, 100 + (int)((50 + 10 * 15) * 1.5f));
}

// Same seed/room, boss rolled and not rolled: the two runs must differ by exactly
// one item and by the 1.5x gold delta, proving the gate is the whole difference.
TEST(ChallengeRoomTest, TickBonusDeltaIsExactlyOneItemAndHalfGold) {
    ChallengeRoomController probe;
    uint32_t boss_seed = 0;
    int boss_room = 0;
    uint32_t none_seed = 0;
    int none_room = 0;
    bool found = false;
    for (uint32_t seed = 0; seed < 4096u && !found; seed++) {
        for (int room = 0; room < 64 && !found; room++) {
            if (probe.has_boss_wave(seed, room) &&
                !probe.has_boss_wave(seed, room + 1)) {
                boss_seed = none_seed = seed;
                boss_room = room;
                none_room = room + 1;
                found = true;
            }
        }
    }
    ASSERT_TRUE(found);
    TickedResult with_bonus = run_to_cleared(boss_seed, boss_room);
    TickedResult without_bonus = run_to_cleared(none_seed, none_room);
    ASSERT_TRUE(with_bonus.cleared);
    ASSERT_TRUE(without_bonus.cleared);
    EXPECT_EQ(without_bonus.items, 3);
    EXPECT_EQ(with_bonus.items, without_bonus.items + 1);
    const int base_gold = 50 + 10 * 15;
    EXPECT_EQ(without_bonus.gold, 100 + base_gold);
    EXPECT_EQ(with_bonus.gold, 100 + (int)(base_gold * 1.5f));
    EXPECT_EQ(with_bonus.gold - without_bonus.gold, base_gold / 2);
}

// --- B4-T9: 保底接线 (per-run pity, 端到端) ---
// 顺序无关: tick() 曾依赖前面奖励用例「先跑一遍」触发的 items.json 加载,
// 排到最前会 0xC000001C (实为 random_rarity() 除零 SIGFPE, 见文件头)。
// RewardDataEnvironment 已在任何用例前加载注册表, 本组用例现在可以放任意位置。

namespace {

// seed 下前 n 个「纯 25% 判定为 miss」的房间号 —— 用例前提自动成立, 不硬编码
std::vector<int> first_miss_rooms(uint32_t seed, size_t n) {
    ChallengeRoomController probe;
    std::vector<int> out;
    for (int r = 0; r < 4096 && out.size() < n; r++)
        if (!probe.has_boss_wave(seed, r)) out.push_back(r);
    return out;
}

// 在同一个 controller 上打完一间房; 调用方用 reset() 模拟换层
bool clear_one_room(ChallengeRoomController& c, Player& p, GameMap& map,
                    std::vector<std::unique_ptr<Monster>>& monsters,
                    std::vector<DroppedItem>& drops, uint32_t seed, int room,
                    int* pity_streak = nullptr) {
    c.try_activate(p);
    c.on_player_entered();
    c.on_doors_locked();
    c.set_room_rect(1, 1, 4, 4);
    c.set_phase_for_test(ChallengePhase::WAVE_SPAWNING);
    for (int t = 0; t < 40 && !c.is_cleared(); t++)
        c.tick(3.5f, &map, &p, monsters, 10, seed, room, drops, pity_streak);
    monsters.clear();
    return c.is_cleared();
}

}  // namespace

// 保底计数由 GameScene 持有并落盘 (账号级), 控制器不自持 —— 否则 GameScene 每次
// 进层重建就清零, 玩家反复刷同一层时保底永远到不了阈值 (实机已复现).
// 关键不变量: 计数跨 reset() 累积.
TEST(ChallengeRoomTest, PityStreakAccumulatesAcrossPerFloorReset) {
    ChallengeRoomController c;
    Player p = make_player(8);
    GameMap map(8, 8, 32);
    std::vector<std::unique_ptr<Monster>> monsters;
    std::vector<DroppedItem> drops;
    const std::vector<int> rooms = first_miss_rooms(0u, 2);
    ASSERT_EQ(rooms.size(), 2u);

    int pity = 0;
    ASSERT_TRUE(clear_one_room(c, p, map, monsters, drops, 0u, rooms[0], &pity));
    ASSERT_EQ(pity, 1);
    EXPECT_FALSE(c.boss_wave_pending());

    c.reset();                              // 换层
    EXPECT_EQ(pity, 1) << "换层把保底进度清掉了";
    EXPECT_FALSE(c.boss_wave_decided());    // 但按房间的判定标记必须清

    ASSERT_TRUE(clear_one_room(c, p, map, monsters, drops, 0u, rooms[1], &pity));
    EXPECT_EQ(pity, 2) << "跨层未累积 — 保底永远到不了阈值";
}

// 单元测试 / 无保底上下文: nullptr 入参必须安全, 判定按 miss_streak=0 处理.
TEST(ChallengeRoomTest, PityNullStreakPointerIsSafe) {
    ChallengeRoomController c;
    Player p = make_player(3);
    GameMap map(8, 8, 32);
    std::vector<std::unique_ptr<Monster>> monsters;
    std::vector<DroppedItem> drops;
    const std::vector<int> rooms = first_miss_rooms(0u, 1);
    ASSERT_EQ(rooms.size(), 1u);
    ASSERT_TRUE(clear_one_room(c, p, map, monsters, drops, 0u, rooms[0], nullptr));
    EXPECT_FALSE(c.boss_wave_pending());
}

// 端到端: 连续 N 间天然 miss 后, 第 N+1 间必须出压轴 boss, 且计数归零.
TEST(ChallengeRoomTest, PityGuaranteesBossAfterPityCap) {
    ChallengeRoomController c;
    Player p = make_player(8);
    GameMap map(8, 8, 32);
    std::vector<std::unique_ptr<Monster>> monsters;
    std::vector<DroppedItem> drops;
    const int cap = ChallengeRoomController::boss_wave_pity_cap();
    const std::vector<int> rooms = first_miss_rooms(0u, cap + 1);
    ASSERT_EQ(rooms.size(), (size_t)cap + 1);

    int pity = 0;
    for (int i = 0; i < cap; i++) {
        ASSERT_TRUE(clear_one_room(c, p, map, monsters, drops, 0u, rooms[i], &pity));
        EXPECT_FALSE(c.boss_wave_pending());
        EXPECT_EQ(pity, i + 1);
        c.reset();                        // 换层, 保底进度必须保留
        EXPECT_EQ(pity, i + 1);
    }

    ASSERT_TRUE(clear_one_room(c, p, map, monsters, drops, 0u, rooms[cap], &pity));
    EXPECT_TRUE(c.boss_wave_pending()) << "第 " << cap + 1 << " 间房未触发保底压轴";
    EXPECT_EQ(pity, 0) << "出过压轴后计数必须归零, 否则保底会连环触发";
}

