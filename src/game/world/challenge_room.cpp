#include "challenge_room.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "combat_system.h"
#include "item.h"
#include "reward_manager.h"
#include "growth_curve.h"
#include "core/logger.h"
#include "spawn_tables.h"    // A6-S2 批次9: 挑战房刷怪池
#include <algorithm>
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
}

bool ChallengeRoomController::has_boss_wave(uint32_t dungeon_seed,
                                            int room_index) const {
    uint32_t boss_seed = _deterministic_seed(dungeon_seed, room_index, kBossWaveSlot);
    return (int)(boss_seed % 100u) < kBossWaveChancePct;
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
    std::vector<DroppedItem>& ground_items) {

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
            if (_current_wave >= _total_waves) {
                _phase = ChallengePhase::REWARD;
                _grant_rewards(*player, map, floor, ground_items);
                _return_portal_tx = _room_rx + _room_rw / 2;
                _return_portal_ty = _room_ry + _room_rh / 2;
                _phase = ChallengePhase::CLEARED;
                LOG_INFO("[CHALLENGE] All waves cleared!");
            } else {
                _wave_timer = 3.0f;
                _phase = ChallengePhase::WAIT_NEXT_WAVE;
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

void ChallengeRoomController::_grant_rewards(Player& player, GameMap* map, int floor,
                                              std::vector<DroppedItem>& ground_items) {
    int granted = 0;
    for (int i = 0; i < 3; i++) {
        auto item = generate_random_item();
        int tries = 0;
        while (item && item->rarity < Rarity::RARE && tries < 5) {
            item = generate_random_item();
            tries++;
        }
        if (!item) continue;

        if (player.inventory.add(item, &player)) {
            granted++;
        } else {
            int cx = _room_rx + _room_rw / 2;
            int cy = _room_ry + _room_rh / 2;
            DroppedItem di;
            di.item = std::move(item);
            di.tile_x = cx;
            di.tile_y = cy;
            ground_items.push_back(std::move(di));
        }
    }

    int gold = 50 + floor * 15;
    RewardManager::grant_gold(player, gold);
    LOG_INFO("[CHALLENGE] Rewards: %d items + %d gold", granted, gold);
}
