#include "systems/hit_detection.h"
#include "entities/monster.h"
#include "types/weapon_types.h"
#include <cmath>
#include <algorithm>

// ── Helper: get forward direction vector from Direction enum ──
static Vector2 _forward_vec(Direction dir) {
    switch (dir) {
    case Direction::UP:    return {0.0f, -1.0f};
    case Direction::DOWN:  return {0.0f, 1.0f};
    case Direction::LEFT:  return {-1.0f, 0.0f};
    case Direction::RIGHT: return {1.0f, 0.0f};
    default: return {0.0f, 1.0f};
    }
}

// ── Helper: center of a monster's rect ──
static Vector2 _monster_center(const Monster* m) {
    return {
        m->entity.rect.x + m->entity.rect.width / 2,
        m->entity.rect.y + m->entity.rect.height / 2
    };
}

// ── Helper: squared distance ──
static float _dist_sq(Vector2 a, Vector2 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// ── Helper: angle between forward direction and target (radians) ──
static float _angle_to(Vector2 origin, Vector2 target, Vector2 forward) {
    float dx = target.x - origin.x;
    float dy = target.y - origin.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return 0.0f;
    float dot = (dx * forward.x + dy * forward.y) / len;
    return std::acos(std::max(-1.0f, std::min(1.0f, dot)));
}

// ── Circle detection ──
std::vector<HitResult> hit_detect_circle(
    Vector2 origin, float radius_px,
    const std::vector<Monster*>& targets)
{
    std::vector<HitResult> results;
    float r2 = radius_px * radius_px;
    for (auto* m : targets) {
        if (!m || !m->combat.is_alive) continue;
        Vector2 mc = _monster_center(m);
        float d2 = _dist_sq(origin, mc);
        if (d2 <= r2) {
            HitResult hr;
            hr.target = m;
            hr.distance = std::sqrt(d2);
            hr.angle = 0.0f;
            hr.hit_point = mc;
            results.push_back(hr);
        }
    }
    // Sort by distance (closest first)
    std::sort(results.begin(), results.end(),
        [](const HitResult& a, const HitResult& b) { return a.distance < b.distance; });
    return results;
}

// ── Sector detection ──
std::vector<HitResult> hit_detect_sector(
    Vector2 origin, Direction dir, float radius_px, float half_angle_deg,
    const std::vector<Monster*>& targets)
{
    std::vector<HitResult> results;
    Vector2 fwd = _forward_vec(dir);
    float half_rad = half_angle_deg * 3.14159265f / 180.0f;
    float r2 = radius_px * radius_px;
    for (auto* m : targets) {
        if (!m || !m->combat.is_alive) continue;
        Vector2 mc = _monster_center(m);
        float d2 = _dist_sq(origin, mc);
        if (d2 > r2) continue;
        float ang = _angle_to(origin, mc, fwd);
        if (ang <= half_rad + 0.01f) {
            HitResult hr;
            hr.target = m;
            hr.distance = std::sqrt(d2);
            hr.angle = ang;
            hr.hit_point = mc;
            results.push_back(hr);
        }
    }
    std::sort(results.begin(), results.end(),
        [](const HitResult& a, const HitResult& b) { return a.distance < b.distance; });
    return results;
}

// ── Rectangle detection ──
std::vector<HitResult> hit_detect_rectangle(
    Vector2 origin, Direction dir, float length_px, float width_px,
    const std::vector<Monster*>& targets)
{
    std::vector<HitResult> results;
    Vector2 fwd = _forward_vec(dir);
    // Perpendicular direction for width
    Vector2 perp = { -fwd.y, fwd.x };
    // Rectangle center is origin + forward * (length/2)
    Vector2 rect_center = { origin.x + fwd.x * length_px / 2,
                            origin.y + fwd.y * length_px / 2 };
    float half_w = width_px / 2;
    for (auto* m : targets) {
        if (!m || !m->combat.is_alive) continue;
        Vector2 mc = _monster_center(m);
        // Transform target to rectangle's local space
        float local_x = (mc.x - rect_center.x) * fwd.x + (mc.y - rect_center.y) * fwd.y;
        float local_y = (mc.x - rect_center.x) * perp.x + (mc.y - rect_center.y) * perp.y;
        // G10.5-B B4: fudge +16 -> MONSTER_HALF(14) — 判定框按怪物半宽扩
        // (原 +16 使玩家身后 16px 也被前向挥砍命中, 且两侧膨胀 32px)
        if (std::abs(local_x) <= length_px / 2 + 14 &&
            std::abs(local_y) <= half_w + 14) {
            HitResult hr;
            hr.target = m;
            hr.distance = std::sqrt(_dist_sq(origin, mc));
            hr.angle = _angle_to(origin, mc, fwd);
            hr.hit_point = mc;
            results.push_back(hr);
        }
    }
    std::sort(results.begin(), results.end(),
        [](const HitResult& a, const HitResult& b) { return a.distance < b.distance; });
    return results;
}

// ── Capsule detection (line segment + radius) ──
static float _point_seg_dist_sq(Vector2 p, Vector2 a, Vector2 b) {
    float abx = b.x - a.x, aby = b.y - a.y;
    float apx = p.x - a.x, apy = p.y - a.y;
    float ab2 = abx * abx + aby * aby;
    float t = (ab2 > 0.001f) ? std::max(0.0f, std::min(1.0f, (apx * abx + apy * aby) / ab2)) : 0.0f;
    float cx = a.x + t * abx, cy = a.y + t * aby;
    float dx = p.x - cx, dy = p.y - cy;
    return dx * dx + dy * dy;
}

std::vector<HitResult> hit_detect_capsule(
    Vector2 origin, Direction dir, float length_px, float radius_px,
    const std::vector<Monster*>& targets)
{
    std::vector<HitResult> results;
    Vector2 fwd = _forward_vec(dir);
    Vector2 tip = { origin.x + fwd.x * length_px, origin.y + fwd.y * length_px };
    float r2 = radius_px * radius_px;
    for (auto* m : targets) {
        if (!m || !m->combat.is_alive) continue;
        Vector2 mc = _monster_center(m);
        float d2 = _point_seg_dist_sq(mc, origin, tip);
        if (d2 <= r2) {
            HitResult hr;
            hr.target = m;
            hr.distance = std::sqrt(_dist_sq(origin, mc));
            hr.angle = _angle_to(origin, mc, fwd);
            hr.hit_point = mc;
            results.push_back(hr);
        }
    }
    std::sort(results.begin(), results.end(),
        [](const HitResult& a, const HitResult& b) { return a.distance < b.distance; });
    return results;
}

// ── Dispatcher ──
std::vector<HitResult> hit_detect_by_shape(
    int hit_shape, Vector2 origin, Direction dir,
    float range_px, float width_px,
    const std::vector<Monster*>& targets)
{
    switch (static_cast<HitShape>(hit_shape)) {
    case HitShape::SECTOR:
        return hit_detect_sector(origin, dir, range_px, width_px, targets);
    case HitShape::RECTANGLE:
        return hit_detect_rectangle(origin, dir, range_px, width_px, targets);
    case HitShape::CAPSULE:
        return hit_detect_capsule(origin, dir, range_px, width_px, targets);
    case HitShape::PROJECTILE: {
        // 约定: PROJECTILE 形状数据仅用于弩 (weapons.json type=CROSSBOW),
        // 弩在 weapon_executor 入口被拦截走真实 ProjectilePool; 此分支是兼容兜底,
        // 正常游戏路径不可达. 保留 capsule 退化为安全 fallback, 勿误删枚举.
        return hit_detect_capsule(origin, dir, range_px, width_px, targets);
    }
    case HitShape::CIRCLE:
    default:
        return hit_detect_circle(origin, range_px, targets);
    }
}
