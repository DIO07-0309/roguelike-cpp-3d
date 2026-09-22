#include "systems/weapon_executor.h"
#include "entities/player.h"
#include "entities/monster.h"
#include "systems/weapon_component.h"
#include "systems/combat_system.h"
#include "systems/hit_detection.h"
#include "systems/vfx_server.h"
#include "game_map.h"
#include "combat_feel.h"
#include "config.h"
#include "audio_server.h"
#include "core/event_bus.h"
#include "combat/element_resolver.h"  // G10.3
#include "systems/relic_effect_processor.h"  // M1A.1
#include "systems/collision_utils.h"
#include "ai/player_behavior/player_behavior_recorder.h" // F15.1
#include <algorithm>
#include <cmath>

// ── G9.3: map weapon type + stage to attack tag ──
static AttackTag _weapon_tag(WeaponType wt, int stage, bool is_stage3) {
    switch (wt) {
    case WeaponType::DAGGER:
        return is_stage3 ? AttackTag::PIERCE : AttackTag::SLASH;
    case WeaponType::SWORD:
        return (stage == 0 || is_stage3) ? AttackTag::BLUNT : AttackTag::SLASH;
    case WeaponType::NUNCHAKU:
        return is_stage3 ? AttackTag::MULTI_HIT : AttackTag::KNOCKBACK;
    case WeaponType::CROSSBOW:
        return is_stage3 ? AttackTag::MARKED : AttackTag::RANGED;
    case WeaponType::SPEAR:
        return is_stage3 ? AttackTag::PIERCE_STACK : AttackTag::PIERCE;
    default: return AttackTag::NONE;
    }
}

// ── G9.3: write AttackContext to Player for skill synergy ──
static void _set_attack_context(Player* p, float dmg, Monster* target,
                                  float game_time)
{
    const WeaponDef* def = p->weapon.current_def();
    if (!def) return;
    bool is_s3 = (p->weapon.combo_index() == 2 && def->stage_count >= 3);
    AttackTag tag = _weapon_tag(def->type, p->weapon.combo_index(), is_s3);
    AttackContext& ctx = p->last_attack;
    ctx.weapon_type = def->type;
    ctx.primary_tag = tag;
    ctx.combo_stage = p->weapon.combo_index();
    ctx.stage3_triggered = is_s3;
    ctx.damage_dealt = dmg;
    ctx.last_target = target;
    ctx.timestamp = (float)game_time;
}

// ── Helper: compute damage for a single hit ──
static int _calc_weapon_dmg(const Player* p, const Monster* target,
                             float stage_mult, bool& is_crit,
                             AttackType atype = AttackType::PHYSICAL)
{
    int atk = get_effective_attack(p);
    int def = target ? target->combat.get_effective_defense(atype) : 0;
    int base = calculate_damage(atk, def, atype);
    int dmg = (int)(base * stage_mult);
    int combo_idx = p->weapon.combo_index();
    float crit_chance = combo_idx >= 2 ? 0.30f : combo_idx == 1 ? 0.15f : 0.05f;
    if ((rng() % 1000) < (int)(crit_chance * 1000.0f)) { dmg *= 2; is_crit = true; }
    return std::max(1, dmg);
}

// ── Build raw target pointer list ──
static std::vector<Monster*> _raw_targets(
    const std::vector<Monster*>& targets)
{
    std::vector<Monster*> rt; rt.reserve(targets.size());
    for (auto* m : targets) if (m && m->combat.is_alive) rt.push_back(m);
    return rt;
}

// ── Get player origin ──
static Vector2 _player_origin(const Player* p) {
    return { p->entity.rect.x + p->entity.rect.width / 2,
             p->entity.rect.y + p->entity.rect.height / 2 };
}

// ── G10.5-B: forward vec (mirror of hit_detection.cpp _forward_vec) ──
static Vector2 _we_forward_vec(Direction dir) {
    switch (dir) {
    case Direction::UP:    return {0.0f, -1.0f};
    case Direction::DOWN:  return {0.0f, 1.0f};
    case Direction::LEFT:  return {-1.0f, 0.0f};
    case Direction::RIGHT: return {1.0f, 0.0f};
    default: return {0.0f, 1.0f};
    }
}

// ── Resolve one hit into a result (damage + kill check) ──
static WeaponAttackResult _resolve_one(Player* p, Monster* m,
    const Vector2& hp, float mult, AttackType atype = AttackType::PHYSICAL)
{
    WeaponAttackResult ar;
    ar.target = m; ar.hit_point = hp; ar.is_crit = false;
    ar.damage = _calc_weapon_dmg(p, m, mult, ar.is_crit, atype);

    // G10.3: Element combat effects (fire crit / ice slow+freeze / poison DOT)
    bool did_freeze = false;
    ElementResolver::resolve(p, m, ar.damage, ar.is_crit, did_freeze);

    // M1A.1: PRE_DAMAGE hook — 通过 DamageContext 允许遗物修改伤害
    DamageContext dctx;
    dctx.source = p;
    dctx.target = m;
    dctx.raw_damage = ar.damage;
    dctx.final_damage = ar.damage;
    dctx.damage_type = (atype == AttackType::MAGICAL) ? DamageType::MAGICAL
                     : (atype == AttackType::TRUE) ? DamageType::TRUE
                     : DamageType::PHYSICAL;
    RelicEffectProcessor::static_on_pre_damage(dctx, p);

    int hp_before = m->combat.current_hp;
    m->combat.take_damage(dctx.final_damage);
    ar.is_killing_blow = (!m->combat.is_alive && hp_before > 0);

    // M1A.1: 新遗物系统 on_hit
    RelicEffectProcessor::static_on_hit(p, m);

    // G10.3: Element EXP
    if (ar.damage > 0) {
        ElementResolver::on_hit(p, m);
        if (ar.is_killing_blow) ElementResolver::on_kill(p, m);
    }
    return ar;
}

// ═══════════════════════════════════════════════════════════════
// WeaponSpecialState methods
// ═══════════════════════════════════════════════════════════════

void WeaponSpecialState::start(int max_h, float interval, float mult, float growth) {
    active = true; timer = 0.0f; hit_count = 0;
    max_hits = max_h; hit_interval = interval;
    next_hit_at = interval; base_mult = mult; mult_growth = growth;
}

bool WeaponSpecialState::should_fire_next(float dt) {
    if (!active) return false;
    timer += dt;
    if (timer >= next_hit_at && hit_count < max_hits) {
        next_hit_at += hit_interval; hit_count++;
        // Deactivate on the final hit, but keep hit_count/tracked so the
        // caller can still read this hit's multiplier and tracked target.
        if (hit_count >= max_hits) active = false;
        return true;
    }
    return false;
}

float WeaponSpecialState::current_multiplier() const {
    float m = base_mult;
    for (int i = 1; i < hit_count; ++i) m *= mult_growth;
    return m;
}

void WeaponSpecialState::reset() { active = false; timer = 0.0f; hit_count = 0; tracked_instance = 0; }

// ═══════════════════════════════════════════════════════════════
// Forward decls for stage-3 special initiations
// ═══════════════════════════════════════════════════════════════
static bool _try_nunchaku_special(Player* p, const AttackStageDef& st,
    float rpx, float wp, const std::vector<Monster*>& rt);
static bool _try_spear_special(Player* p, const AttackStageDef& st, float rpx);
static bool _try_crossbow_power(Player* p, const AttackStageDef& st,
    Vector2 origin, Game::ObjectPool<Projectile>* projs, GameMap* map);
static void _crossbow_normal(Player* p, const AttackStageDef& st,
    Vector2 origin, int stage_idx, Game::ObjectPool<Projectile>* projs);
static std::vector<WeaponAttackResult> _melee_normal(
    Player* p, const WeaponDef* def, const AttackStageDef& st,
    Vector2 origin, float rpx, float wp, const std::vector<Monster*>& rt,
    const AttackGeometry& geo);

// ── Forward decls for execute() 编排段 (G12: 函数 ≤40 行拆分) ──
static bool _try_stage3_special(Player* p, const WeaponDef* def,
    const AttackStageDef& st, float rpx, float wp, Vector2 origin,
    const std::vector<Monster*>& rt, Game::ObjectPool<Projectile>* projs, GameMap* map);
static void _update_range_indicator(Player* p, const WeaponDef* def, float rpx);
static std::vector<WeaponAttackResult> _resolve_normal(
    Player* p, const WeaponDef* def, const AttackStageDef& st,
    float rpx, float wp, Vector2 origin, const std::vector<Monster*>& rt,
    Game::ObjectPool<Projectile>* projs);
static void _finalize_attack(Player* p, const WeaponDef* def,
    const AttackStageDef& st, const std::vector<WeaponAttackResult>& results,
    double game_time, AudioServer* audio);

// ═══════════════════════════════════════════════════════════════
// WeaponExecutor — execute
// ═══════════════════════════════════════════════════════════════

std::vector<WeaponAttackResult> WeaponExecutor::execute(
    Player* player,
    const std::vector<Monster*>& targets,
    double game_time,
    AudioServer* audio,
    Game::ObjectPool<Projectile>* projectiles,
    GameMap* map)
{
    std::vector<WeaponAttackResult> results;
    if (!player || !player->combat.is_alive) return results;
    if (!player->weapon.can_attack(game_time)) return results;

    const WeaponDef* def = player->weapon.current_def();
    if (!def) return results;

    const AttackStageDef& stage = player->weapon.current_stage();
    float rpx = stage.range * TILE_SIZE;
    float wp = (stage.hit_shape == HitShape::SECTOR) ? stage.width : stage.width * TILE_SIZE;
    Vector2 origin = _player_origin(player);
    auto rt = _raw_targets(targets);

    bool is_special = _try_stage3_special(player, def, stage, rpx, wp, origin, rt,
                                          projectiles, map);
    _update_range_indicator(player, def, rpx);
    if (!is_special)
        results = _resolve_normal(player, def, stage, rpx, wp, origin, rt, projectiles);

    _finalize_attack(player, def, stage, results, game_time, audio);
    return results;
}

// ── Stage-3 special 分发: 仅 combo 第三段且武器定义有第三段时触发 ──
static bool _try_stage3_special(Player* p, const WeaponDef* def,
    const AttackStageDef& st, float rpx, float wp, Vector2 origin,
    const std::vector<Monster*>& rt, Game::ObjectPool<Projectile>* projs, GameMap* map)
{
    if (p->weapon.combo_index() != 2 || def->stage_count < 3) return false;
    if (def->type == WeaponType::NUNCHAKU)
        return _try_nunchaku_special(p, st, rpx, wp, rt);
    if (def->type == WeaponType::SPEAR)
        return _try_spear_special(p, st, rpx);
    if (def->type == WeaponType::CROSSBOW && projs)
        return _try_crossbow_power(p, st, origin, projs, map);
    return false;
}

// ── G9.3: 远程/双节棍武器刷新射程指示器 ──
static void _update_range_indicator(Player* p, const WeaponDef* def, float rpx) {
    if (def->type != WeaponType::SPEAR && def->type != WeaponType::CROSSBOW
        && def->type != WeaponType::NUNCHAKU) return;
    p->weapon.range_indicator_timer = 0.25f;
    p->weapon.range_indicator_px = rpx;
}

// ── 普通攻击结算: 弩走弹道, 其余走 SSOT 几何近战命中 ──
static std::vector<WeaponAttackResult> _resolve_normal(
    Player* p, const WeaponDef* def, const AttackStageDef& st,
    float rpx, float wp, Vector2 origin, const std::vector<Monster*>& rt,
    Game::ObjectPool<Projectile>* projs)
{
    if (def->type == WeaponType::CROSSBOW && projs) {
        _crossbow_normal(p, st, origin, p->weapon.combo_index(), projs);
        return {};
    }
    // G10.5-B: 构建 SSOT 几何 — range_boost/legendary 膨胀后的真实值
    float eff_rpx = rpx;
    if (p->weapon.combo_index() == 2 && def->stage_count >= 3) {
        if (def->affix.type == "range_boost")
            eff_rpx *= (1.0f + def->affix.value);
        if (def->legendary_effect == "sword_wave")
            eff_rpx *= 1.3f;
    }
    AttackGeometry geo;
    geo.origin = origin;
    geo.direction = _we_forward_vec(p->direction);
    geo.shape = st.hit_shape;
    geo.range_px = eff_rpx;
    geo.width_px = wp;
    return _melee_normal(p, def, st, origin, rpx, wp, rt, geo);
}

// ── 攻击收尾: 技能协同上下文 + 推进连段 + 事件 + 音频 ──
static void _finalize_attack(Player* p, const WeaponDef* def,
    const AttackStageDef& st, const std::vector<WeaponAttackResult>& results,
    double game_time, AudioServer* audio)
{
    // ── G9.3: set attack context for skill synergy ──
    float total_dmg = 0.0f;
    Monster* prime_target = nullptr;
    for (auto& r : results) { total_dmg += r.damage; if (!prime_target) prime_target = r.target; }
    _set_attack_context(p, total_dmg, prime_target, (float)game_time);

    // ── Advance + emit + audio ──
    int stage_before = p->weapon.combo_index();
    p->weapon.execute_attack(game_time);
    GameEventType gev = stage_before == 0 ? GameEventType::WEAPON_STAGE_1
                      : stage_before == 1 ? GameEventType::WEAPON_STAGE_2
                      : GameEventType::WEAPON_SPECIAL;
    EventBus::inst().emit(gev, p, stage_before, st.damage_multiplier, def->name.c_str());
    EventBus::inst().emit(GameEventType::WEAPON_ATTACK_COMPLETE, p,
        (int)st.damage_multiplier * 100, total_dmg, def->name.c_str());
    // F15.2: record weapon usage with full context
    g_behavior.on_weapon_attack(weapon_type_name(def->type), (float)game_time, 0,  // floor set by game_scene
        p->entity.rect.x + p->entity.rect.width / 2,
        p->entity.rect.y + p->entity.rect.height / 2,
        (int)def->type, stage_before);
    if (audio) {
        const char* sfx = st.sfx_name.empty() ? "melee" : st.sfx_name.c_str();
        audio->play_sfx(sfx);
    }
}

// ── Stage-3 special: nunchaku 5-hit auto-track flurry ──
static bool _try_nunchaku_special(Player* p, const AttackStageDef& st,
    float rpx, float wp, const std::vector<Monster*>& rt)
{
    Vector2 origin = _player_origin(p);
    auto hits = hit_detect_by_shape((int)st.hit_shape, origin,
        p->direction, rpx, wp, rt);
    auto& sp = p->weapon.runtime().special;
    const WeaponDef* def = p->weapon.current_def();
    int total_hits = 5;
    // Legendary: +2 hits (李小龙)
    if (def && def->legendary_effect == "nunchaku_hits") total_hits = 7;
    // Affix: damage_ramp — extra growth per hit
    float growth = 1.20f + (def ? def->affix.value : 0.0f);
    sp.start(total_hits, 0.08f, 0.80f, growth);
    sp.tracked_instance = hits.empty() ? 0 : hits[0].target->instance_id;
    sp.range_px = rpx * 1.5f;
    sp.hit_shape = (int)HitShape::CIRCLE;
    sp.direction = p->direction;
    return true;
}

// ── Stage-3 special: spear 10-hit rapid pierce ──
static bool _try_spear_special(Player* p, const AttackStageDef& st, float rpx)
{
    auto& sp = p->weapon.runtime().special;
    const WeaponDef* def = p->weapon.current_def();
    int total_hits = 10;
    // Legendary: +2 hits (惊破天)
    if (def && def->legendary_effect == "spear_count") total_hits = 12;
    // Affix: pierce_bonus — extra multiplier per hit
    float mult = 1.10f + (def ? def->affix.value : 0.0f);
    sp.start(total_hits, 0.10f, mult, 1.0f);
    sp.range_px = rpx;
    sp.width_param = 30.0f;
    sp.hit_shape = (int)HitShape::SECTOR;
    sp.direction = p->direction;
    return true;
}

// ── Stage-3 special: crossbow power shot (piercing projectile + recoil) ──
static bool _try_crossbow_power(Player* p, const AttackStageDef& st,
    Vector2 origin, Game::ObjectPool<Projectile>* projs, GameMap* map)
{
    Vector2 fwd = {0, 1};
    switch (p->direction) {
    case Direction::UP: fwd = {0, -1}; break;
    case Direction::LEFT: fwd = {-1, 0}; break;
    case Direction::RIGHT: fwd = {1, 0}; break;
    default: break;
    }
    const WeaponDef* def = p->weapon.current_def();
    float legendary_bonus = (def && def->legendary_effect == "crossbow_power") ? 1.5f : 1.0f;
    Projectile proj;
    proj.pos = origin;
    proj.vel = { fwd.x * 800.0f, fwd.y * 800.0f };
    bool dummy_crit = false;
    proj.damage = _calc_weapon_dmg(p, nullptr, st.damage_multiplier * legendary_bonus, dummy_crit);
    proj.piercing = true;
    proj.pierce_walls = true;  // 弩箭蓄力穿透墙体
    proj.lifetime = 1.5f;
    proj.owner = (int)ProjectileOwner::PLAYER; projs->insert(proj);
    // 后坐力
    clamp_displacement(p->entity, -fwd.x * TILE_SIZE, -fwd.y * TILE_SIZE, map);
    return true;
}

// ── Crossbow normal stages: fire projectiles ──
static void _crossbow_normal(Player* p, const AttackStageDef& st,
    Vector2 origin, int stage_idx, Game::ObjectPool<Projectile>* projs)
{
    Vector2 fwd = {0, 1};
    switch (p->direction) {
    case Direction::UP: fwd = {0, -1}; break;
    case Direction::LEFT: fwd = {-1, 0}; break;
    case Direction::RIGHT: fwd = {1, 0}; break;
    default: break;
    }
    bool dc = false;
    if (stage_idx == 1) {
        for (float spread = -15.0f; spread <= 15.0f; spread += 15.0f) {
            float rad = spread * 3.14159f / 180.0f;
            float ca = cosf(rad), sa = sinf(rad);
            Projectile p2;
            p2.pos = origin;
            p2.vel = { (fwd.x * ca - fwd.y * sa) * 700.0f,
                       (fwd.x * sa + fwd.y * ca) * 700.0f };
            p2.damage = _calc_weapon_dmg(p, nullptr, st.damage_multiplier, dc);
            p2.lifetime = 1.2f;
            projs->insert(p2);
        }
    } else {
        Projectile proj;
        proj.pos = origin;
        proj.vel = { fwd.x * 700.0f, fwd.y * 700.0f };
        proj.damage = _calc_weapon_dmg(p, nullptr, st.damage_multiplier, dc);
        proj.lifetime = 1.2f;
    proj.owner = (int)ProjectileOwner::PLAYER; projs->insert(proj);
    }
}

// ── Melee normal: instant hit detection + affix + legendary effects ──
// G10.5-B: geo 为本段 SSOT 几何, 命中结果携带供 VFX 消费
static std::vector<WeaponAttackResult> _melee_normal(
    Player* p, const WeaponDef* def, const AttackStageDef& st,
    Vector2 origin, float rpx, float wp, const std::vector<Monster*>& rt,
    const AttackGeometry& geo)
{
    std::vector<WeaponAttackResult> results;
    // ── Affix: sword range_boost on stage-3 ──
    float effective_rpx = rpx;
    bool is_stage3 = (p->weapon.combo_index() == 2 && def->stage_count >= 3);
    if (is_stage3 && def->affix.type == "range_boost")
        effective_rpx *= (1.0f + def->affix.value);
    // ── Legendary: 倚天剑 wave expands range further ──
    if (is_stage3 && def->legendary_effect == "sword_wave")
        effective_rpx *= 1.3f;

    auto hits = hit_detect_by_shape(
        (int)st.hit_shape, origin, p->direction, effective_rpx, wp, rt);
    for (auto& h : hits) {
        float mult = st.damage_multiplier;
        // ── Affix: dagger bleed on stage-3 thrust ──
        bool apply_bleed = is_stage3 && def->affix.type == "bleed"
            && ((rng() % 100) < (int)def->affix.value);
        // ── Legendary: 恶魔之爪 always bleeds ──
        if (is_stage3 && def->legendary_effect == "dagger_bleed")
            apply_bleed = true;
        if (apply_bleed && h.target && h.target->combat.is_alive)
            apply_buff(h.target, "poison", 3);

        results.push_back(_resolve_one(p, h.target, h.hit_point, mult,
            (AttackType)st.damage_type));
        results.back().geometry = geo;   // G10.5-B: 命中结果携带判定几何真相
    }
    // Sword stage-3 stun
    if (is_stage3 && def->type == WeaponType::SWORD)
        for (auto& h : hits)
            if (h.target && h.target->combat.is_alive)
                apply_buff(h.target, "slow", 3);
    return results;
}

// ═══════════════════════════════════════════════════════════════
// tick_specials — called each frame by GameScene
// ═══════════════════════════════════════════════════════════════

std::vector<WeaponAttackResult> WeaponExecutor::tick_specials(
    Player* player,
    const std::vector<Monster*>& targets,
    float dt)
{
    std::vector<WeaponAttackResult> results;
    if (!player) return results;

    auto& sp = player->weapon.runtime().special;
    if (!sp.active) return results;

    if (!sp.should_fire_next(dt)) return results;

    WeaponType wt = player->weapon.weapon_type();
    auto rt = _raw_targets(targets);
    Vector2 origin = _player_origin(player);
    float mult = sp.current_multiplier();

    if (wt == WeaponType::NUNCHAKU) {
        // Auto-track: hit the tracked target with auto-aim
        // Q3.13: 校验 tracked 仍在本帧存活怪物列表中 (instance_id 查找, 跨层残留安全)
        Monster* trg = nullptr;
        if (sp.tracked_instance != 0) {
            auto it = std::find_if(targets.begin(), targets.end(),
                [id = sp.tracked_instance](Monster* m) {
                    return m && m->instance_id == id;
                });
            if (it != targets.end() && (*it)->combat.is_alive) trg = *it;
        }
        if (trg) {
            Vector2 hp = { trg->entity.rect.x + trg->entity.rect.width / 2,
                           trg->entity.rect.y + trg->entity.rect.height / 2 };
            auto ar = _resolve_one(player, trg, hp, mult);
            ar.from_special = true;
            results.push_back(ar);
        } else if (!rt.empty()) {
            // Re-acquire nearest target
            auto hits = hit_detect_circle(origin, sp.range_px, rt);
            if (!hits.empty()) {
                sp.tracked_instance = hits[0].target->instance_id;
                auto ar = _resolve_one(player, hits[0].target, hits[0].hit_point, mult);
                ar.from_special = true;
                results.push_back(ar);
            }
        }
    }
    else if (wt == WeaponType::SPEAR) {
        // Sector rapid hits: magic-typed lightning-enhanced strikes
        auto hits = hit_detect_sector(origin, sp.direction,
            sp.range_px, sp.width_param, rt);
        for (auto& h : hits) {
            auto ar = _resolve_one(player, h.target, h.hit_point, mult,
                AttackType::MAGICAL);
            ar.from_special = true;
            results.push_back(ar);
        }
    }

    return results;
}

// ═══════════════════════════════════════════════════════════════
// tick_projectiles — called each frame by GameScene
// ═══════════════════════════════════════════════════════════════

std::vector<WeaponAttackResult> WeaponExecutor::tick_projectiles(
    Game::ObjectPool<Projectile>& projectiles,
    const std::vector<Monster*>& targets,
    float dt,
    const GameMap* map)
{
    std::vector<WeaponAttackResult> results;
    projectiles.for_each([&](Projectile& p, int) {
        if (!p.alive) return;
        if (p.owner != (int)ProjectileOwner::PLAYER) return;
        p.elapsed += dt;
        if (p.elapsed >= p.lifetime) { p.alive = false; return; }
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        // 墙体碰撞 — pierce_walls=false 的弹幕碰墙销毁
        if (!p.pierce_walls && map) {
            auto [wtx, wty] = map->pixel_to_tile(p.pos.x, p.pos.y);
            if (!map->is_walkable(wtx, wty)) { p.alive = false; return; }
        }
        for (auto* m : targets) {
            if (!m || !m->combat.is_alive) continue;
            Rectangle mr = m->entity.rect;
            if (CheckCollisionCircleRec(p.pos, 8.0f, mr)) {
                WeaponAttackResult ar;
                ar.target = m; ar.hit_point = p.pos;
                ar.damage = p.damage; ar.is_crit = false;
                int hp_before = m->combat.current_hp;
                m->combat.take_damage(p.damage);
                ar.is_killing_blow = (!m->combat.is_alive && hp_before > 0);
                ar.from_special = true;
                results.push_back(ar);
                if (!p.piercing) { p.alive = false; break; }
            }
        }
    });
    // D2: cleanup handled centrally by game_scene after both PLAYER + MONSTER ticks
    return results;
}
