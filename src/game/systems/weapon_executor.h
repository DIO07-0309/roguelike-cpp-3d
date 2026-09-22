#pragma once
#include <vector>
#include "types/weapon_types.h"
#include "object_pool.h"
#include "systems/hit_detection.h"

class Player;
class Monster;
class GameMap;
class AudioServer;
struct Effect;

// ============================================================
// G9: WeaponExecutor — execute weapon attacks + tick ongoing specials
// Single responsibility: WeaponRuntime → HitDetection → Damage → Results
// ============================================================

class WeaponExecutor {
public:
    // Execute one attack. projectiles may be nullptr for melee weapons.
    static std::vector<WeaponAttackResult> execute(
        Player* player,
        const std::vector<Monster*>& targets,
        double game_time,
        AudioServer* audio,
        Game::ObjectPool<Projectile>* projectiles = nullptr,
        GameMap* map = nullptr);

    // G9.1: Tick ongoing multi-hit specials (nunchaku flurry, spear rapid).
    // Call once per frame from GameScene::_process().
    static std::vector<WeaponAttackResult> tick_specials(
        Player* player,
        const std::vector<Monster*>& targets,
        float dt);

    // G9.1: Tick active projectiles (crossbow bolts).
    // Call once per frame from GameScene::_process().
    static std::vector<WeaponAttackResult> tick_projectiles(
        Game::ObjectPool<Projectile>& projectiles,
        const std::vector<Monster*>& targets,
        float dt,
        const GameMap* map = nullptr);
};
