#include "team_coordinator.h"
#include "monster.h"
#include "ai.h"              // AIState enum for archer_active check
#include "player.h"
#include "game_map.h"
#include <cmath>

// ============================================================
// G2.2: TeamCoordinator — 怪物协同分析器实现
// 纯函数: 输入 monster list + player, 输出 TeamDecision
// 无副作用, 无状态, 不修改任何对象
// ============================================================

static float _dist2(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1, dy = y2 - y1;
    return dx * dx + dy * dy;
}

// ── 辅助: 找指定 TeamRole 的最近盟友 ──
Monster* TeamCoordinator::_find_nearest_ally_with_role(
    const std::vector<Monster*>& allies, const Monster* self,
    TeamRole role, float ax, float ay) {
    Monster* best = nullptr;
    float best_d2 = (8.0f * 32.0f) * (8.0f * 32.0f);
    for (auto* a : allies) {
        if (!a || a == self || !a->combat.is_alive) continue;
        if (a->team_role != role) continue;
        float ax2 = a->entity.rect.x + a->entity.rect.width / 2;
        float ay2 = a->entity.rect.y + a->entity.rect.height / 2;
        float d2 = _dist2(ax, ay, ax2, ay2);
        if (d2 < best_d2) { best_d2 = d2; best = a; }
    }
    return best;
}

// G13: 走 TeamCtx 的简写
Monster* TeamCoordinator::_nearest_ally_role(const TeamCtx& c,
                                             const std::vector<Monster*>& allies,
                                             TeamRole role) {
    return _find_nearest_ally_with_role(allies, c.self, role, c.ax, c.ay);
}

// ── 辅助: 找最近石柱 ──
bool TeamCoordinator::_find_nearest_cover(
    const GameMap* map, float ax, float ay,
    float& out_cx, float& out_cy, float max_dist) {
    if (!map) return false;
    float best_d2 = max_dist * max_dist;
    bool found = false;
    for (auto& ao : map->arena_objects) {
        if (ao.type != ArenaObjectType::ROCK || !ao.active) continue;
        float cx = ao.tile_x * 32.0f + 16;
        float cy = ao.tile_y * 32.0f + 16;
        float d2 = _dist2(ax, ay, cx, cy);
        if (d2 < best_d2) { best_d2 = d2; out_cx = cx; out_cy = cy; found = true; }
    }
    return found;
}

// ============================================================
// 核心: evaluate() — 分析单个怪物的协同上下文
// G13: 每个 TeamRole 一条分支拆成独立函数 (≤40 行红线)
// ============================================================

namespace {

// 归一化朝 (tx,ty) 移动; 过近(<1px) 不动。返回是否真的写了 dec
static bool move_toward(TeamDecision& dec, float from_x, float from_y,
                        float tx, float ty) {
    const float mx = tx - from_x, my = ty - from_y;
    const float len = sqrtf(mx * mx + my * my);
    if (len <= 1) return false;
    dec.move_x = mx / len;
    dec.move_y = my / len;
    return true;
}

static TeamCtx make_team_ctx(const Monster* self, const Player& player,
                             const GameMap* map) {
    TeamCtx c;
    c.self = self;
    c.map = map;
    c.ax = self->entity.rect.x + self->entity.rect.width / 2;
    c.ay = self->entity.rect.y + self->entity.rect.height / 2;
    c.px = player.entity.rect.x + player.entity.rect.width / 2;
    c.py = player.entity.rect.y + player.entity.rect.height / 2;
    c.dp = sqrtf(_dist2(c.ax, c.ay, c.px, c.py));
    return c;
}

// 8 tile 内存活、非 Boss 的盟友
static void collect_nearby_allies(const TeamCtx& c, const std::vector<Monster*>& all,
                                  std::vector<Monster*>& out) {
    const float range2 = (8.0f * 32.0f) * (8.0f * 32.0f);
    for (auto* o : all) {
        if (!o || o == c.self || !o->combat.is_alive || o->is_boss) continue;
        const float ox = o->entity.rect.x + o->entity.rect.width / 2;
        const float oy = o->entity.rect.y + o->entity.rect.height / 2;
        if (_dist2(c.ax, c.ay, ox, oy) < range2) out.push_back(o);
    }
}

}  // namespace

TeamDecision TeamCoordinator::evaluate(
    const Monster* self, const Player& player,
    const std::vector<Monster*>& all, const GameMap* map) {

    TeamDecision dec;
    if (self->team_role == TeamRole::NONE) return dec;

    TeamCtx c = make_team_ctx(self, player, map);

    std::vector<Monster*> allies;
    collect_nearby_allies(c, all, allies);
    if (allies.empty()) return dec;

    c.tank = _nearest_ally_role(c, allies, TeamRole::FRONTLINE);

    switch (c.self->team_role) {
    case TeamRole::FRONTLINE: _eval_frontline(c, allies, dec); break;
    case TeamRole::BACKLINE:  _eval_backline(c, allies, dec); break;
    case TeamRole::SUPPORT:   _eval_support(c, allies, dec); break;
    case TeamRole::FLANK:     _eval_flank(c, allies, dec); break;
    case TeamRole::COMMAND:   dec.should_command = true; break;
    default: break;
    }
    return dec;
}

// ── FRONTLINE: 保护后排 + 寻找掩体 ──
void TeamCoordinator::_eval_frontline(const TeamCtx& c,
                                      const std::vector<Monster*>& allies,
                                      TeamDecision& dec) {
    if (c.dp >= 6.0f * 32.0f) return;          // 离玩家太远就不协防

    // 优先: 站在石柱和玩家之间
    float cx, cy;
    if (_find_nearest_cover(c.map, c.ax, c.ay, cx, cy, 3.0f * 32.0f)
        && move_toward(dec, c.px, c.py, cx, cy)) return;

    // 次选: 保护最近的 BACKLINE / SUPPORT 盟友
    Monster* backline = _nearest_ally_role(c, allies, TeamRole::BACKLINE);
    if (!backline) backline = _nearest_ally_role(c, allies, TeamRole::SUPPORT);
    if (!backline) return;
    const float bx = backline->entity.rect.x + backline->entity.rect.width / 2;
    const float by = backline->entity.rect.y + backline->entity.rect.height / 2;
    move_toward(dec, c.ax, c.ay, (c.px + bx) / 2, (c.py + by) / 2);
}

// ── BACKLINE: 保持距离 + 寻找掩体 ──
void TeamCoordinator::_eval_backline(const TeamCtx& c,
                                     const std::vector<Monster*>& allies,
                                     TeamDecision& dec) {
    float cx, cy;
    if (_find_nearest_cover(c.map, c.ax, c.ay, cx, cy, 3.5f * 32.0f)
        && move_toward(dec, c.ax, c.ay, cx, cy)) return;

    // Tank 已接敌则向后拉开距离
    if (c.tank && c.dp < 5.0f * 32.0f)
        move_toward(dec, c.px, c.py, c.ax, c.ay);
}

// ── SUPPORT: 找需要治疗的盟友 + 保持后方 ──
void TeamCoordinator::_eval_support(const TeamCtx& c,
                                    const std::vector<Monster*>& allies,
                                    TeamDecision& dec) {
    Monster* best = nullptr;
    float best_score = -1;
    for (auto* a : allies) {
        if (!a->combat.is_alive) continue;
        const float hp_pct = (float)a->combat.current_hp / a->combat.max_hp;
        float s = (1.0f - hp_pct) * 100;
        if (a->team_role == TeamRole::FRONTLINE) s += 40;
        else if (a->team_role == TeamRole::COMMAND) s += 25;
        if (s > best_score && hp_pct < 0.85f) { best_score = s; best = a; }
    }
    if (best) {
        dec.should_support = true;
        dec.support_target = best;
        dec.support_type = 1;  // heal (实际由 MonsterAI 决定 buff/heal 随机)
    }
    if (c.dp < 4.0f * 32.0f)
        move_toward(dec, c.px, c.py, c.ax, c.ay);
}

// ── FLANK: 等待 Tank 接敌, 然后绕侧 ──
void TeamCoordinator::_eval_flank(const TeamCtx& c,
                                  const std::vector<Monster*>& allies,
                                  TeamDecision& dec) {
    bool tank_fighting = (c.tank && hypotf(
        c.tank->entity.rect.x - c.px, c.tank->entity.rect.y - c.py) < 3.0f * 32.0f);
    bool archer_active = false;
    for (auto* a : allies) {
        if (a->team_role == TeamRole::BACKLINE && a->ai && a->ai->state == AIState::ATTACK) {
            archer_active = true;
            break;
        }
    }
    if (tank_fighting || archer_active || c.dp <= 3.0f * 32.0f) return;

    const float perp_x = -(c.py - c.ay), perp_y = (c.px - c.ax);
    const float plen = sqrtf(perp_x * perp_x + perp_y * perp_y);
    if (plen > 1) { dec.move_x = perp_x / plen; dec.move_y = perp_y / plen; }
}
