#include "challenge_room.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "combat_system.h"
#include "item.h"
#include "reward_manager.h"
#include "growth_curve.h"
#include "boss.h"           // B4-T3: 压轴波 boss 工厂
#include "core/logger.h"
#include "spawn_tables.h"    // A6-S2 批次9: 挑战房刷怪池
#include <algorithm>
#include <cassert>
#include <cmath>

static constexpr int MAX_CHALLENGE_MONSTERS = 12;

const char* ChallengeRoomController::_pick_monster_type(
    int floor, int wave, uint32_t rng) {

    // 批次9: 9 个池子迁至 resources/challenge_pools.json (3 群系 x 3 波, 均匀抽签)
    // rng 由调用方以 wave_seed 派生, 不触碰全局 rng() — 见设计 §6.1
    const std::string* id = pick_challenge_monster(floor, wave, rng);
    return id ? id->c_str() : "slime";
}

// Deterministic seed: avalanche hash_combine (no XOR collision)
uint32_t ChallengeRoomController::_deterministic_seed(
    uint32_t dungeon_seed, int room_index, int wave_index) const {
    uint32_t h = dungeon_seed;
    h ^= (uint32_t)(room_index + 1) * 0x9E3779B9u;
    h ^= (uint32_t)(wave_index + 1) * 0x85EBCA6Bu;
    h ^= h >> 16; h *= 0x45D9F3Bu;
    h ^= h >> 16; h *= 0x45D9F3Bu;
    h ^= h >> 16;
    return h;
}

namespace {
// B4: 保留波次槽位. 真实波用 0..2, 压轴用 99 —— 与真实波经 avalanche 后
// 属不同哈希域, 判定不改变小怪波构成, 故存档/回放可比性不被破坏.
constexpr int kBossWaveSlot = 99;
constexpr int kBossWaveChancePct = 25;
// B4-T9: 保底阈值. 本局连续空手 N 次后下次必出, 消除「整局看不到」的挫败.
// p=0.25 + 阈值 3 的分布: 第1间 25% / 第2间累计 43.75% / 第3间累计 57.8% / 第4间 100%.
constexpr int kBossWavePityMisses = 3;

// B4-T4: 奖励结算参数. 基础三项是现状契约; 压轴加成仅在 boss_wave_pending 时叠加,
// 不改写基础项, 因此未出 boss 的房间与改造前逐字节一致.
constexpr int kBaseRewardItems = 3;
constexpr int kBaseRewardRetries = 5;
constexpr int kBossBonusItems = 1;
constexpr int kBossBonusRetries = 8;
constexpr int kRewardGoldBase = 50;
constexpr int kRewardGoldPerFloor = 15;
constexpr float kBossRewardGoldMult = 1.5f;

// 复刻现状内联重试: 先抽一次, 稀有度不足且未超上限则重抽.
std::shared_ptr<Item> roll_item_at_least(int rarity_floor, int retry_cap) {
    std::shared_ptr<Item> item = generate_random_item();
    int tries = 0;
    while (item && item->rarity < static_cast<Rarity>(rarity_floor) && tries < retry_cap) {
        item = generate_random_item();
        tries++;
    }
    return item;
}
}

bool ChallengeRoomController::has_boss_wave(uint32_t dungeon_seed,
                                            int room_index, int miss_streak) const {
    if (miss_streak >= kBossWavePityMisses) return true;  // 保底优先于随机
    uint32_t boss_seed = _deterministic_seed(dungeon_seed, room_index, kBossWaveSlot);
    return (int)(boss_seed % 100u) < kBossWaveChancePct;
}

int ChallengeRoomController::boss_wave_chance_pct() { return kBossWaveChancePct; }
int ChallengeRoomController::boss_wave_pity_cap() { return kBossWavePityMisses; }

std::string ChallengeRoomController::boss_wave_hint(int pity_streak) {
    std::string base = "隐藏压轴 " + std::to_string(kBossWaveChancePct) + "% · 连空"
                       + std::to_string(kBossWavePityMisses) + "次必出";
    if (pity_streak > 0)
        return base + " (已空" + std::to_string(pity_streak) + "次)";
    return base;
}

WaveAdvance ChallengeRoomController::decide_advance(
    int wave, int total, bool boss_pending) {
    if (wave == total && boss_pending) return WaveAdvance::BOSS_WAIT;
    if (wave >= total) return WaveAdvance::REWARD;
    return WaveAdvance::WAIT;
}

bool ChallengeRoomController::_room_contains(int tx, int ty) const {
    return tx >= _room_rx && tx < _room_rx + _room_rw &&
           ty >= _room_ry && ty < _room_ry + _room_rh;
}

void ChallengeRoomController::reset() {
    _phase = ChallengePhase::INACTIVE;
    _current_wave = 0;
    _wave_timer = 0.0f;
    _monsters_alive_this_wave = 0;
    _portal_tx = _portal_ty = -1;
    _return_portal_tx = _return_portal_ty = -1;
    _room_rx = _room_ry = _room_rw = _room_rh = 0;
    _boss_wave_decided = false;
    _boss_wave_pending = false;
}

void ChallengeRoomController::on_player_entered() {
    if (_phase == ChallengePhase::UNLOCKED) {
        _phase = ChallengePhase::ARMED;
    }
}

void ChallengeRoomController::on_doors_locked() {
    if (_phase == ChallengePhase::ARMED) {
        _current_wave = 0;
        // 按房间清判定标记: reset() 是按层粒度, CHALLENGE_ARENA 重入路径
        // (game_scene.cpp:1294) 直接置 ARMED 而不 reset, 不清则第二间房跳过判定
        _boss_wave_decided = false;
        _boss_wave_pending = false;
        _phase = ChallengePhase::WAVE_SPAWNING;
    }
}

bool ChallengeRoomController::try_activate(Player& player) {
    if (_phase != ChallengePhase::INACTIVE) return false;
    if (player.key_count <= 0) return false;

    player.spend_key(1);
    _phase = ChallengePhase::UNLOCKED;
    LOG_INFO("[CHALLENGE] Key consumed, challenge activated");
    return true;
}

// Batch 3I: Portal state machine methods (GameScene 消费 Key, Controller 只记录状态)
void ChallengeRoomController::setup_portal(int tx, int ty) {
    _portal_tx = tx;
    _portal_ty = ty;
    _phase = ChallengePhase::PORTAL_ACTIVE;
}

bool ChallengeRoomController::consume_key_for_challenge(Player& player) {
    if (_phase != ChallengePhase::PORTAL_ACTIVE) return false;
    if (player.key_count <= 0) return false;
    player.spend_key(1);
    return true;
}

void ChallengeRoomController::set_room_rect(int rx, int ry, int rw, int rh) {
    _room_rx = rx; _room_ry = ry; _room_rw = rw; _room_rh = rh;
}

void ChallengeRoomController::set_return_portal(int tx, int ty) {
    _return_portal_tx = tx;
    _return_portal_ty = ty;
}

void ChallengeRoomController::mark_cleared() {
    _phase = ChallengePhase::CLEARED;
}

void ChallengeRoomController::tick(
    float dt, GameMap* map, Player* player,
    std::vector<std::unique_ptr<Monster>>& monsters,
    int floor, uint32_t dungeon_seed, int room_index,
    std::vector<DroppedItem>& ground_items, int* pity_streak) {

    if (_phase == ChallengePhase::INACTIVE ||
        _phase == ChallengePhase::UNLOCKED ||
        _phase == ChallengePhase::PORTAL_ACTIVE ||
        _phase == ChallengePhase::CLEARED) return;

    // ARMED: waiting for RoomManager to lock doors
    if (_phase == ChallengePhase::ARMED) return;

    // WAVE_SPAWNING: spawn current wave
    if (_phase == ChallengePhase::WAVE_SPAWNING) {
        _spawn_wave(_current_wave, map, monsters, floor, dungeon_seed, room_index);
        _phase = ChallengePhase::COMBAT;
        return;
    }

    // COMBAT: count alive monsters from current wave
    if (_phase == ChallengePhase::COMBAT) {
        int alive = 0;
        for (auto& m : monsters) {
            if (m && m->combat.is_alive && _room_contains(
                (int)(m->entity.rect.x / 32), (int)(m->entity.rect.y / 32))) {
                alive++;
            }
        }
        _monsters_alive_this_wave = alive;

        if (alive <= 0) {
            _current_wave++;
            if (_current_wave == _total_waves && !_boss_wave_decided) {
                _boss_wave_decided = true;
                int streak = pity_streak ? *pity_streak : 0;
                _boss_wave_pending =
                    has_boss_wave(dungeon_seed, room_index, streak);
                int next = _boss_wave_pending ? 0 : streak + 1;
                if (pity_streak) *pity_streak = next;   // 回写由 GameScene 落盘
                LOG_INFO("[CHALLENGE] Boss wave roll: %s (pity %d/%d)",
                         _boss_wave_pending ? "HIT" : "miss",
                         next, kBossWavePityMisses);
            }
            WaveAdvance adv =
                decide_advance(_current_wave, _total_waves, _boss_wave_pending);
            if (adv == WaveAdvance::WAIT || adv == WaveAdvance::BOSS_WAIT) {
                _wave_timer = 3.0f;
                _phase = ChallengePhase::WAIT_NEXT_WAVE;
            } else {
                _phase = ChallengePhase::REWARD;
                _grant_rewards(*player, map, floor, ground_items, _boss_wave_pending);
                _return_portal_tx = _room_rx + _room_rw / 2;
                _return_portal_ty = _room_ry + _room_rh / 2;
                _phase = ChallengePhase::CLEARED;
                LOG_INFO("[CHALLENGE] All waves cleared!");
            }
        }
        return;
    }

    // WAIT_NEXT_WAVE: countdown timer
    if (_phase == ChallengePhase::WAIT_NEXT_WAVE) {
        _wave_timer -= dt;
        if (_wave_timer <= 0.0f) {
            _phase = ChallengePhase::WAVE_SPAWNING;
        }
    }
}

void ChallengeRoomController::_spawn_wave(
    int wave_index, GameMap* map,
    std::vector<std::unique_ptr<Monster>>& monsters,
    int floor, uint32_t seed, int room_idx) {

    assert(wave_index != kBossWaveSlot);  // 压轴波必须走 _spawn_boss_wave, 禁止静默降级成史莱姆
    if (wave_index >= _total_waves) {
        _spawn_boss_wave(map, monsters, floor);
        return;
    }

    int count = 4;  // monsters per wave
    ChallengeModifier mod;
    const GrowthCurve& gc = g_growth.curve(floor);

    uint32_t wave_seed = _deterministic_seed(seed, room_idx, wave_index);

    for (int i = 0; i < count; i++) {
        if ((int)monsters.size() >= MAX_CHALLENGE_MONSTERS) break;

        // Deterministic position attempt
        for (int attempt = 0; attempt < 50; attempt++) {
            uint32_t pos_hash = wave_seed ^ (uint32_t)(i * 50 + attempt);
            int rx = _room_rx + 1 + (int)(pos_hash % (uint32_t)(_room_rw - 2));
            int ry = _room_ry + 1 + (int)((pos_hash >> 8) % (uint32_t)(_room_rh - 2));

            if (!map->is_walkable(rx, ry)) continue;

            // No spawn on doors
            if (map->is_door(rx, ry)) continue;

            auto [px, py] = map->tile_to_pixel(rx, ry);
            uint32_t type_rng = wave_seed ^ (uint32_t)(i * 7 + 13);
            const char* type = _pick_monster_type(floor, wave_index, type_rng);
            Monster* m = spawn_monster((float)px, (float)py, type);
            if (!m) continue;

            // Apply floor scaling + challenge modifier
            m->combat.max_hp = (int)(m->combat.max_hp * gc.monster_hp * mod.hp_multiplier);
            m->combat.current_hp = m->combat.max_hp;
            m->combat.attack = (int)(m->combat.attack * gc.monster_atk * mod.attack_multiplier);

            monsters.emplace_back(m);
            _monsters_alive_this_wave++;
            break;
        }
    }

    LOG_INFO("[CHALLENGE] Wave %d spawned, %d monsters alive",
             wave_index + 1, _monsters_alive_this_wave);
}

void ChallengeRoomController::_spawn_boss_wave(
    GameMap* map,
    std::vector<std::unique_ptr<Monster>>& monsters,
    int floor) {

    int cx = _room_rx + _room_rw / 2;
    int cy = _room_ry + _room_rh / 2;

    bool ok = map->is_walkable(cx, cy);
    for (int r = 1; !ok && r <= 4; r++)
        for (int dy = -r; !ok && dy <= r; dy++)
            for (int dx = -r; !ok && dx <= r; dx++) {
                if (map->is_walkable(cx + dx, cy + dy)) {
                    cx += dx;
                    cy += dy;
                    ok = true;
                }
            }

    if (!ok) {
        LOG_WARN("[CHALLENGE] No walkable tile near center, skip boss wave");
        return;
    }

    Monster* boss = boss_factory_create(BossType::GOLEM, cx, cy, floor);
    if (!boss) {
        LOG_WARN("[CHALLENGE] boss_factory_create returned null");
        return;
    }

    monsters.emplace_back(boss);
    _monsters_alive_this_wave++;
    LOG_INFO("[CHALLENGE] Boss wave spawned at tile %d,%d (floor %d)", cx, cy, floor);
}

RewardPlan ChallengeRoomController::decide_reward_plan(int floor, bool boss_wave_pending) {
    RewardPlan plan;
    plan.base_item_count = kBaseRewardItems;
    plan.base_retry_cap = kBaseRewardRetries;
    plan.base_rarity_floor = static_cast<int>(Rarity::RARE);
    plan.gold = kRewardGoldBase + floor * kRewardGoldPerFloor;
    if (boss_wave_pending) {
        plan.bonus_item_count = kBossBonusItems;
        plan.bonus_retry_cap = kBossBonusRetries;
        plan.bonus_rarity_floor = static_cast<int>(Rarity::EPIC);
        plan.gold = static_cast<int>(plan.gold * kBossRewardGoldMult);
    }
    return plan;
}

void ChallengeRoomController::grant_rewards_for_test(Player& player, GameMap* map, int floor,
                                                      std::vector<DroppedItem>& ground_items,
                                                      bool boss_wave_pending) {
    _grant_rewards(player, map, floor, ground_items, boss_wave_pending);
}

int ChallengeRoomController::_grant_items(Player& player,
                                          std::vector<DroppedItem>& ground_items,
                                          int count, int rarity_floor, int retry_cap) {
    int granted = 0;
    for (int i = 0; i < count; i++) {
        std::shared_ptr<Item> item = roll_item_at_least(rarity_floor, retry_cap);
        if (!item) continue;
        if (player.inventory.add(item, &player)) {
            granted++;
            continue;
        }
        DroppedItem di;
        di.item = std::move(item);
        di.tile_x = _room_rx + _room_rw / 2;
        di.tile_y = _room_ry + _room_rh / 2;
        ground_items.push_back(std::move(di));
    }
    return granted;
}

void ChallengeRoomController::_grant_rewards(Player& player, GameMap* map, int floor,
                                              std::vector<DroppedItem>& ground_items,
                                              bool boss_wave_pending) {
    RewardPlan plan = decide_reward_plan(floor, boss_wave_pending);
    int granted = _grant_items(player, ground_items, plan.base_item_count,
                               plan.base_rarity_floor, plan.base_retry_cap);
    if (boss_wave_pending)
        granted += _grant_items(player, ground_items, plan.bonus_item_count,
                                plan.bonus_rarity_floor, plan.bonus_retry_cap);
    RewardManager::grant_gold(player, plan.gold);
    LOG_INFO("[CHALLENGE] Rewards: %d items + %d gold%s",
              granted, plan.gold, boss_wave_pending ? " (boss wave bonus)" : "");
}
