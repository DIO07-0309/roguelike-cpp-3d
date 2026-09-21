#pragma once
#include <string>
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include "entity.h"
#include "item.h"
#include "types/combat_types.h"
#include "attack_evolution_state.h"  // G1

class Player;

// ============================================================
// VFXServer — Unified VFX Primitives (G5.8.1)
//
// All skill/archetype/boss VFX are compositions of ~9 primitives:
//   ring | beam | lightning | explosion | slash | smoke | spark | aura | flash
//
// New skills can compose primitives without adding monolithic methods.
// Future: JSON recipes → primitive dispatch (G5.8.5).
// ============================================================

class VFXServer {
public:
    std::vector<Effect> effects;

    // ═══════════════════════════════════════════════════════════
    // G5.8.1 — Composable VFX Primitives (9 methods)
    // ═══════════════════════════════════════════════════════════

    // Expanding ring(s). layers=1-4, each offset by radius*0.2
    void ring(float cx, float cy, float radius, Color c, int layers = 1, float dur = 0.45f,
              float layer_delay = 0.0f);

    // Line from (sx,sy) to (tx,ty). branches=1-5 adds offset jitter lines
    void beam(float sx, float sy, float tx, float ty, Color c, float dur = 0.30f);

    // Zigzag lightning bolt with offset jitter (thickness=2-4 pixels)
    void lightning(float sx, float sy, float tx, float ty, int branches = 3, Color c = {200,220,255,240}, float dur = 0.22f);

    // Burst of sparks outward from center
    void explosion(float cx, float cy, float radius, Color c, int count = 12, float dur = 0.40f);

    // Expanding pulse rings (shockwave)
    void shockwave(float cx, float cy, float radius, Color c, int layers = 3, float dur = 0.50f);

    // Directional slash arc
    void slash_arc(float cx, float cy, Direction dir, float radius, Color c = {255,80,80,200}, float dur = 0.35f);

    // Expanding smoke puffs (random scattered circles)
    void smoke_puff(float cx, float cy, float radius, Color c, int count = 5, float dur = 0.55f);

    // Scattered spark dots
    void spark_burst(float cx, float cy, int count, Color c, float dur = 0.30f);

    // Pulsing shield ring (aura)
    void aura_ring(float cx, float cy, float radius, Color c, float dur = 0.70f);

    // Flash (full-screen or local)
    void flash(float cx, float cy, float radius, Color c = {255,255,255,200}, float dur = 0.15f);

    // ── G5.8.5: Recipe dispatcher ──
    // play a VFXRecipe by id. skill_id → recipe → primitives
    void play_recipe(const char* recipe_id, float cx, float cy, Direction dir = Direction::DOWN,
                     float tx = 0, float ty = 0, int level = 1);

    // ── G5.8: Color preset → Color lookup ──
    static Color preset_color(const std::string& name);

    // ═══════════════════════════════════════════════════════════
    // Backward-compat wrappers (thin compositions of primitives)
    // ═══════════════════════════════════════════════════════════

    void player_attack(float cx, float cy, float range,
                       const AttackEvolutionState& evo = AttackEvolutionState{});

    // Legacy: internally delegates to slash_arc + spark_burst
    void slash_skill(float cx, float cy, Direction dir, int level);
    // Legacy: internally delegates to beam + ring
    void fireball(float cx, float cy, float tx, float ty, int level);
    // Legacy: internally delegates to ring + spark_burst
    void heal(float cx, float cy, int level);
    // Legacy
    void monster_attack(float mx, float my, float px, float py, Color c);
    void boss_cone(float cx, float cy);
    void boss_circle(float cx, float cy);
    void boss_summon(float cx, float cy);
    void time_stop(float cx, float cy);
    void hit_flash(float x, float y, float size);

    // ── G5.8: Signature skill VFX (thin wrappers) ──
    void ice_nova(float cx, float cy, float radius, int level);
    void chain_lightning(float sx, float sy, float tx, float ty, int bounces);
    void shadow_strike(float from_x, float from_y, float to_x, float to_y, int level);
    void blood_frenzy(float cx, float cy, float radius, int hit_count);
    void summon_spirit(float cx, float cy, int count);

    // ── G5.8: Enemy archetype VFX (thin wrappers) ──
    void sniper_line(float sx, float sy, float tx, float ty);
    void controller_zone(float x, float y, float radius);
    void ambush_smoke(float x, float y);
    void guardian_aura_enemy(float cx, float cy, float radius);

    // ── G5.8: Boss Phase2 transition VFX ──
    void boss_phase2_flash(float cx, float cy, Color tint);
    void boss_gravity_pull(float cx, float cy, float px, float py);

    // ── A9: 武器特效增强 (15 种新特效) ──
    void slash_arc_1(float cx, float cy, Direction dir, float radius, Color c, float dur);
    void slash_arc_2(float cx, float cy, Direction dir, float radius, Color c, float dur);
    void slash_arc_3(float cx, float cy, Direction dir, float radius, Color c, float dur);
    void pierce_beam_1(float sx, float sy, float tx, float ty, Color c, float dur);
    void pierce_beam_2(float sx, float sy, float tx, float ty, Color c, float dur);
    void pierce_beam_3(float sx, float sy, float tx, float ty, Color c, float dur);
    void whip_arc_1(float cx, float cy, Direction dir, float radius, Color c, float dur);
    void whip_arc_2(float cx, float cy, Direction dir, float radius, Color c, float dur);
    void whip_arc_3(float cx, float cy, Direction dir, float radius, Color c, float dur);
    void bolt_spread_1(float cx, float cy, float tx, float ty, Color c, float dur);
    void bolt_spread_2(float cx, float cy, float tx, float ty, Color c, float dur);
    void bolt_spread_3(float cx, float cy, float tx, float ty, Color c, float dur);
    void smash_impact_1(float cx, float cy, float radius, Color c, float dur);
    void smash_impact_2(float cx, float cy, float radius, Color c, float dur);
    void smash_impact_3(float cx, float cy, float radius, Color c, float dur);

    void portal_entry(float cx, float cy);
    void portal_return(float cx, float cy);
};
