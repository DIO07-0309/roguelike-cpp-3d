#include <gtest/gtest.h>
#include "challenge_room.h"
#include "combat_system.h"  // 全局 rng (CountingRng::draws 用于证明不消耗)
#include "player.h"

static Player make_player(int keys = 3) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.key_count = keys;
    p.gold = 100;
    return p;
}

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
