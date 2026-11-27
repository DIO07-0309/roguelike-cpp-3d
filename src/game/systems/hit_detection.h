#pragma once
#include <vector>
#include "raylib.h"
#include "entities/entity.h"  // Direction enum

class Monster;

// ============================================================
// G9: hit_detection — shape-based attack detection
// Pure library: no dependency on CombatSystem or Player
// ============================================================

struct HitResult {
    Monster* target = nullptr;
    float distance = 0.0f;  // from origin to target center
    float angle = 0.0f;     // radians from player facing direction
    Vector2 hit_point{};    // world position of hit
};

// ── Circle: all targets within radius of origin ──
std::vector<HitResult> hit_detect_circle(
    Vector2 origin, float radius_px,
    const std::vector<Monster*>& targets);

// aim_angle_rad: >=0 时用 (cos,sin) 作朝向向量 (sim 精确瞄准); -1=用 dir 四向 (默认, 真实游戏不变)
// ── Sector: targets within radius AND within half_angle_deg of direction ──
std::vector<HitResult> hit_detect_sector(
    Vector2 origin, Direction dir, float radius_px, float half_angle_deg,
    const std::vector<Monster*>& targets, float aim_angle_rad = -1.0f);

// ── Rectangle: targets overlapping a forward-extending rectangle ──
std::vector<HitResult> hit_detect_rectangle(
    Vector2 origin, Direction dir, float length_px, float width_px,
    const std::vector<Monster*>& targets, float aim_angle_rad = -1.0f);

// ── Capsule: line segment from origin to length, with radius ──
std::vector<HitResult> hit_detect_capsule(
    Vector2 origin, Direction dir, float length_px, float radius_px,
    const std::vector<Monster*>& targets, float aim_angle_rad = -1.0f);

// ── Dispatcher: pick shape based on HitShape enum ──
std::vector<HitResult> hit_detect_by_shape(
    int hit_shape, Vector2 origin, Direction dir,
    float range_px, float width_px,
    const std::vector<Monster*>& targets, float aim_angle_rad = -1.0f);
