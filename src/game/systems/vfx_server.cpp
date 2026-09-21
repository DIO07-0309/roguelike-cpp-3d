#include "vfx_server.h"
#include "data/vfx_recipe.h"  // G5.8.5
#include "core/service_locator.h"      // Q4.6: 间接访问 Audio/Presentation
#include "core/scene_tree.h"
#include "audio/audio_server.h"
#include "director/presentation_system_director.h"
#include "combat_system.h"             // RNG-002: visual_rng (视觉掷骰独立流)
#include <cmath>
#include <algorithm>

static Effect _ef(const char* k, float x, float y, float r, Color c, float d) {
    return {k, x, y, r, c, d, 0};
}

// ═══════════════════════════════════════════════════════════════
// G5.8.1 — 9 Composable VFX Primitives
// 注: Effect 生命周期/渲染由 game_scene.active_effects + GameRenderer 管理
// ═══════════════════════════════════════════════════════════════

void VFXServer::ring(float cx, float cy, float radius, Color c, int layers, float dur,
                     float layer_delay) {
    for (int i = 0; i < layers; i++) {
        Effect ef = _ef("ring", cx, cy, radius * (0.5f + i * 0.25f),
                        Color{c.r,c.g,c.b,(unsigned char)(c.a - (unsigned char)(i*40))}, dur);
        ef.start_delay = layer_delay * i;  // G5.8.8: 层间错峰
        effects.push_back(ef);
    }
}

void VFXServer::beam(float sx, float sy, float tx, float ty, Color c, float dur) {
    effects.push_back({"bolt", sx, sy, 0, c, dur, 0, Direction::DOWN, tx, ty, 1});
}

void VFXServer::lightning(float sx, float sy, float tx, float ty, int branches, Color c, float dur) {
    // Main bolt
    effects.push_back({"bolt", sx, sy, 0, c, dur, 0, Direction::DOWN, tx, ty, 1});
    // Jagged offset lines for electricity effect
    for (int j = 0; j < branches; j++) {
        // RNG-002: 视觉掷骰走 visual_rng 独立流, 不吃 gameplay rng
        float off = (float)((int)visual_rng() % 12 - 6);
        Color dim = {c.r, c.g, c.b, (unsigned char)(c.a / 2)};
        effects.push_back({"bolt", sx + off, sy + off, 0, dim, dur * 0.6f, 0, Direction::DOWN, tx + off, ty + off});
    }
}

void VFXServer::explosion(float cx, float cy, float radius, Color c, int count, float dur) {
    ring(cx, cy, radius * 0.6f, c, 2, dur);
    for (int i = 0; i < count; i++) {
        // RNG-002: 视觉掷骰走 visual_rng 独立流, 不吃 gameplay rng
        float a = (float)(visual_rng() % 360) * DEG2RAD;
        float d = radius * (0.3f + (float)(visual_rng() % 70) / 100.0f);
        effects.push_back(_ef("spark", cx + cosf(a) * d, cy + sinf(a) * d,
                              2.0f + (float)(visual_rng() % 4), c, dur * 0.8f));
    }
}

void VFXServer::shockwave(float cx, float cy, float radius, Color c, int layers, float dur) {
    ring(cx, cy, radius, c, layers, dur);
    effects.push_back(_ef("flash", cx, cy, radius * 0.4f,
                          Color{c.r,c.g,c.b,(unsigned char)(c.a / 3)}, dur * 0.5f));
}

void VFXServer::slash_arc(float cx, float cy, Direction dir, float radius, Color c, float dur) {
    effects.push_back({"slash_arc", cx, cy, radius, c, dur, 0, dir});
}

void VFXServer::smoke_puff(float cx, float cy, float radius, Color c, int count, float dur) {
    for (int i = 0; i < count; i++)
        // RNG-002: 视觉掷骰走 visual_rng 独立流, 不吃 gameplay rng
        effects.push_back(_ef("smoke", cx + (float)(visual_rng()%24 - 12), cy + (float)(visual_rng()%24 - 12),
                              radius * (0.5f + (float)(visual_rng()%50)/100.0f), c, dur));
}

void VFXServer::spark_burst(float cx, float cy, int count, Color c, float dur) {
    for (int i = 0; i < count; i++) {
        // RNG-002: 视觉掷骰走 visual_rng 独立流, 不吃 gameplay rng
        float a = (float)(visual_rng() % 360) * DEG2RAD;
        float d = 5.0f + (float)(visual_rng() % 28);
        effects.push_back(_ef("spark", cx + cosf(a) * d, cy + sinf(a) * d,
                              1.5f + (float)(visual_rng() % 4), c, dur));
    }
}

void VFXServer::aura_ring(float cx, float cy, float radius, Color c, float dur) {
    effects.push_back({"shield_ring", cx, cy, radius, c, dur, 0});
    spark_burst(cx, cy, 6, Color{c.r,c.g,c.b,(unsigned char)(c.a / 2)}, dur * 0.7f);
}

void VFXServer::flash(float cx, float cy, float radius, Color c, float dur) {
    effects.push_back(_ef("flash", cx, cy, radius, c, dur));
}

// ═══════════════════════════════════════════════════════════════
// G5.8.5: Color preset + Recipe dispatcher
// ═══════════════════════════════════════════════════════════════

Color VFXServer::preset_color(const std::string& name) {
    // skill build colors
    if (name == "ice")        return {100, 200, 255, 200};
    if (name == "fire")       return {255, 120, 30, 200};
    if (name == "lightning")  return {200, 220, 255, 240};
    if (name == "blood")      return {220, 30, 50, 220};
    if (name == "shadow")     return {80, 30, 160, 200};
    if (name == "nature")     return {100, 255, 100, 200};
    if (name == "holy")       return {255, 220, 80, 200};
    if (name == "void")       return {80, 20, 120, 200};
    if (name == "gold")       return {255, 200, 50, 200};
    if (name == "red")        return {255, 80, 80, 200};
    if (name == "white")      return {200, 200, 200, 160};
    // G5.8.8-fix: vfx_recipes.json presets (poison/time/heal/bleed/summon)
    if (name == "poison")     return {100, 200, 50, 220};
    if (name == "time")       return {180, 160, 220, 220};
    if (name == "heal")       return {80, 220, 120, 220};
    if (name == "bleed")      return {200, 40, 40, 220};
    if (name == "summon")     return {180, 140, 220, 220};
    return {200, 200, 200, 200};
}

// G5.8.8: 单步分发 — 由 play_recipe 调用, 支持 JSON 全部 kind
static void _emit_step(VFXServer& vfx, const VFXStep& step, float cx, float cy,
                       Direction dir, float tx, float ty, int level) {
    Color c = step.color_preset.empty()
              ? Color{255, 200, 50, 220}
              : VFXServer::preset_color(step.color_preset);
    int cnt = step.count + level / 2; // level scales count

    if (step.type == "ring")
        vfx.ring(cx, cy, step.radius, c, step.layers + level / 2, step.duration,
                 step.layer_delay);
    else if (step.type == "beam" || step.type == "bolt") {
        float btx = tx ? tx : cx + cosf(step.direction_rad) * step.target_dist;
        float bty = ty ? ty : cy + sinf(step.direction_rad) * step.target_dist;
        // G10.5-B B5: 零长 beam/bolt 跳过 (tx=0 且 target_dist=0 时退化为不可见点)
        float blen = hypotf(btx - cx, bty - cy);
        if (blen > 1.0f) vfx.beam(cx, cy, btx, bty, c, step.duration);
    }
    else if (step.type == "lightning") {
        float ltx = tx ? tx : cx + cosf(step.direction_rad) * step.target_dist;
        float lty = ty ? ty : cy + sinf(step.direction_rad) * step.target_dist;
        float llen = hypotf(ltx - cx, lty - cy);
        if (llen > 1.0f)
            vfx.lightning(cx, cy, ltx, lty, cnt, c, step.duration);
    }
    else if (step.type == "explosion")
        vfx.explosion(cx, cy, step.radius, c, cnt, step.duration);
    else if (step.type == "shockwave")
        vfx.shockwave(cx, cy, step.radius, c, cnt, step.duration);
    else if (step.type == "slash_arc")
        vfx.slash_arc(cx, cy, dir, step.radius, c, step.duration);
    else if (step.type == "cone") {
        // G5.8.8-fix: cone 独立 kind, 使用 GameRenderer 的矩形锥形渲染
        Effect ef = {"cone", cx, cy, step.radius, c, step.duration, 0, dir};
        vfx.effects.push_back(ef);
    }
    else if (step.type == "smoke")
        vfx.smoke_puff(cx, cy, step.radius, c, cnt, step.duration);
    else if (step.type == "spark")
        vfx.spark_burst(cx, cy, cnt, c, step.duration);
    else if (step.type == "aura")
        vfx.aura_ring(cx, cy, step.radius, c, step.duration);
    else if (step.type == "flash")
        vfx.flash(cx, cy, step.radius, c, step.duration);
}

void VFXServer::play_recipe(const char* recipe_id, float cx, float cy,
                             Direction dir, float tx, float ty, int level) {
    const VFXRecipe* recipe = get_vfx_recipe(recipe_id);
    // G9.3: if direct lookup fails, try "skill_" prefix for skill recipes
    if (!recipe) {
        std::string prefixed = "skill_" + std::string(recipe_id);
        recipe = get_vfx_recipe(prefixed.c_str());
    }
    if (!recipe) {
        // fallback: generic slash + spark
        slash_arc(cx, cy, dir, 56.0f, {255, 80, 80, 200});
        spark_burst(cx, cy, 6, {255, 120, 80, 255}, 0.30f);
        return;
    }

    // Q4.6: 消费 recipe 的 sfx / camera_shake 字段
    if (!recipe->sfx.empty()) {
        auto* tree = ServiceLocator::get<SceneTree>();
        if (tree) tree->get_audio()->play_sfx(recipe->sfx.c_str(), 0.6f);
    }
    if (recipe->camera_shake > 0) {
        auto* pres = ServiceLocator::get<PresentationSystemDirector>();
        if (pres) pres->trigger_shake(recipe->camera_shake * 0.8f);
    }

    for (auto& step : recipe->steps) {
        size_t start_idx = effects.size();
        _emit_step(*this, step, cx, cy, dir, tx, ty, level);
        // G5.8.8: 分镜延迟 — 该步新生成的特效统一挂 start_delay
        if (step.delay > 0)
            for (size_t i = start_idx; i < effects.size(); i++)
                effects[i].start_delay = step.delay;
    }
}

// ═══════════════════════════════════════════════════════════════
// Backward-compat wrappers — all rewritten as primitive compositions
// ═══════════════════════════════════════════════════════════════

void VFXServer::player_attack(float cx, float cy, float range, const AttackEvolutionState& evo) {
    Color c = (evo.level == 1) ? Color{255,200,50,200}
            : (evo.level == 2) ? Color{255,150,40,220} : Color{255,200,40,240};
    int n = (evo.level == 1) ? 4 : (evo.level == 2) ? 6 : 10;
    ring(cx, cy, range * (0.6f + evo.level * 0.3f), c, 1, 0.20f + evo.level * 0.05f);
    spark_burst(cx, cy, n, {255,220,100,255}, 0.25f);
    if (evo.level >= 3) flash(cx, cy, range * 0.8f, {255,220,60,100}, 0.20f);
}

void VFXServer::slash_skill(float cx, float cy, Direction dir, int level) {
    float r = level == 1 ? 56.0f : level == 2 ? 72.0f : 88.0f;
    slash_arc(cx, cy, dir, r, {255,80,80,200});
    if (level >= 3) spark_burst(cx, cy, 8, {255,120,80,255}, 0.35f);
}

void VFXServer::fireball(float cx, float cy, float tx, float ty, int level) {
    beam(cx, cy, tx, ty, {255,100,50,200}, 0.4f);
    for (int i = 0; i < level; i++)
        ring(tx, ty, 16.0f + i * 8.0f, {255,180,60,150}, 1, 0.35f + i * 0.1f);
}

void VFXServer::heal(float cx, float cy, int) {
    ring(cx, cy, 40, {100,255,100,180}, 1, 0.5f);
    spark_burst(cx, cy, 6, {150,255,150,255}, 0.4f);
}

void VFXServer::monster_attack(float mx, float my, float px, float py, Color c) {
    beam(mx, my, px, py, c, 0.25f);
}

void VFXServer::boss_cone(float cx, float cy)   { ring(cx, cy, 96, {200,40,40,200}, 1, 0.5f); }
void VFXServer::boss_circle(float cx, float cy)  { ring(cx, cy, 80, {220,50,50,200}, 1, 0.5f); }
void VFXServer::boss_summon(float cx, float cy)  { ring(cx, cy, 64, {150,50,200,200}, 1, 0.5f); }
void VFXServer::time_stop(float cx, float cy)    { ring(cx, cy, 200, {180,180,200,150}, 1, 0.8f); }
void VFXServer::hit_flash(float x, float y, float s) { flash(x, y, s*0.8f, {255,255,255,200}, 0.15f); }

// ── G5.8 Signature Skills ──

void VFXServer::ice_nova(float cx, float cy, float radius, int level) {
    shockwave(cx, cy, radius, {100,200,255,200}, 3, 0.50f);
    spark_burst(cx, cy, 12 + level * 3, {200,230,255,255}, 0.35f);
    explosion(cx, cy, radius * 0.6f, {180,240,255,220}, 6, 0.40f);
}

void VFXServer::chain_lightning(float sx, float sy, float tx, float ty, int bounces) {
    lightning(sx, sy, tx, ty, 3, {200,220,255,240}, 0.25f);
    flash(tx, ty, 20.0f, {200,220,255,180}, 0.15f);
    spark_burst(tx, ty, 4, {180,200,255,255}, 0.20f);
}

void VFXServer::shadow_strike(float fx, float fy, float tx, float ty, int level) {
    smoke_puff(tx, ty, 14.0f, {60,20,120,180}, 3 + level, 0.30f);
    slash_arc(tx, ty, Direction::DOWN, 48.0f, {140,40,220,200}, 0.35f);
    flash(tx, ty, 28.0f, {180,60,255,150}, 0.20f);
    spark_burst(tx, ty, 6, {120,40,200,200}, 0.30f);
}

void VFXServer::blood_frenzy(float cx, float cy, float radius, int hit_count) {
    ring(cx, cy, radius, {200,30,40,180}, 1, 0.40f);
    for (int i = 0; i < 15 + hit_count * 3; i++) {
        // RNG-002: 视觉掷骰走 visual_rng 独立流, 不吃 gameplay rng
        float a = (float)(visual_rng() % 360) * DEG2RAD;
        float d = radius * (float)(visual_rng() % 100) / 100.0f;
        effects.push_back(_ef("spark", cx + cosf(a) * d, cy + sinf(a) * d, 2.5f, {220,30,50,220}, 0.35f));
    }
    for (int i = 0; i < hit_count; i++)
        effects.push_back(_ef("spark", cx - 20 + (float)(visual_rng()%40), cy - 30, 4.0f, {100,255,100,220}, 0.45f));
}

void VFXServer::summon_spirit(float cx, float cy, int count) {
    ring(cx, cy, 60.0f, {120,180,255,180}, 2, 0.60f);
    ring(cx, cy, 40.0f, {150,200,255,160}, 1, 0.50f);
    spark_burst(cx, cy, count * 6, {180,200,255,200}, 0.50f);
}

// ── G5.8 Enemy Archetype VFX ──

void VFXServer::sniper_line(float sx, float sy, float tx, float ty) {
    beam(sx, sy, tx, ty, {255,60,30,200}, 0.8f);
    ring(tx, ty, 16.0f, {255,40,20,180}, 1, 0.8f);
}

void VFXServer::controller_zone(float x, float y, float radius) {
    ring(x, y, radius, {180,50,200,160}, 3, 0.70f);
    flash(x, y, radius * 0.3f, {200,60,200,120}, 0.50f);
}

void VFXServer::ambush_smoke(float x, float y) {
    smoke_puff(x, y, 16.0f, {40,20,60,140}, 5, 0.60f);
}

void VFXServer::guardian_aura_enemy(float cx, float cy, float radius) {
    aura_ring(cx, cy, radius, {60,140,255,200}, 0.80f);
}

// ── G5.8 Boss Phase2 VFX ──

void VFXServer::boss_phase2_flash(float cx, float cy, Color tint) {
    flash(cx, cy, 300.0f, {tint.r,tint.g,tint.b,180}, 0.60f);
    shockwave(cx, cy, 100.0f, {tint.r,tint.g,tint.b,200}, 4, 0.55f);
    explosion(cx, cy, 80.0f, {tint.r,tint.g,tint.b,240}, 20, 0.50f);
}

void VFXServer::boss_gravity_pull(float cx, float cy, float px, float py) {
    ring(cx, cy, 60.0f, {80,20,120,200}, 1, 0.70f);
    beam(px, py, cx, cy, {120,40,180,160}, 0.40f);
    spark_burst(cx, cy, 12, {140,60,200,200}, 0.45f);
}

void VFXServer::portal_entry(float cx, float cy) {
    aura_ring(cx, cy, 18.0f, {80, 180, 255, 200}, 1.2f);
    spark_burst(cx, cy, 6, {120, 200, 255, 180}, 0.4f);
}

void VFXServer::portal_return(float cx, float cy) {
    aura_ring(cx, cy, 18.0f, {100, 255, 150, 200}, 1.2f);
    spark_burst(cx, cy, 6, {150, 255, 180, 180}, 0.4f);
}

// ═══════════════════════════════════════════════════════════
// A9: 武器特效增强 (15 种新特效)
// ═══════════════════════════════════════════════════════════

void VFXServer::slash_arc_1(float cx, float cy, Direction dir, float radius, Color c, float dur) {
    effects.push_back({"slash_arc_1", cx, cy, radius, c, dur, 0, dir});
}

void VFXServer::slash_arc_2(float cx, float cy, Direction dir, float radius, Color c, float dur) {
    effects.push_back({"slash_arc_2", cx, cy, radius, c, dur, 0, dir});
}

void VFXServer::slash_arc_3(float cx, float cy, Direction dir, float radius, Color c, float dur) {
    effects.push_back({"slash_arc_3", cx, cy, radius, c, dur, 0, dir});
}

void VFXServer::pierce_beam_1(float sx, float sy, float tx, float ty, Color c, float dur) {
    effects.push_back({"pierce_beam_1", sx, sy, 0, c, dur, 0, Direction::DOWN, tx, ty, 1});
}

void VFXServer::pierce_beam_2(float sx, float sy, float tx, float ty, Color c, float dur) {
    effects.push_back({"pierce_beam_2", sx, sy, 0, c, dur, 0, Direction::DOWN, tx, ty, 1});
}

void VFXServer::pierce_beam_3(float sx, float sy, float tx, float ty, Color c, float dur) {
    effects.push_back({"pierce_beam_3", sx, sy, 0, c, dur, 0, Direction::DOWN, tx, ty, 1});
}

void VFXServer::whip_arc_1(float cx, float cy, Direction dir, float radius, Color c, float dur) {
    effects.push_back({"whip_arc_1", cx, cy, radius, c, dur, 0, dir});
}

void VFXServer::whip_arc_2(float cx, float cy, Direction dir, float radius, Color c, float dur) {
    effects.push_back({"whip_arc_2", cx, cy, radius, c, dur, 0, dir});
}

void VFXServer::whip_arc_3(float cx, float cy, Direction dir, float radius, Color c, float dur) {
    effects.push_back({"whip_arc_3", cx, cy, radius, c, dur, 0, dir});
}

void VFXServer::bolt_spread_1(float cx, float cy, float tx, float ty, Color c, float dur) {
    effects.push_back({"bolt_spread_1", cx, cy, 0, c, dur, 0, Direction::DOWN, tx, ty, 1});
}

void VFXServer::bolt_spread_2(float cx, float cy, float tx, float ty, Color c, float dur) {
    effects.push_back({"bolt_spread_2", cx, cy, 0, c, dur, 0, Direction::DOWN, tx, ty, 1});
}

void VFXServer::bolt_spread_3(float cx, float cy, float tx, float ty, Color c, float dur) {
    effects.push_back({"bolt_spread_3", cx, cy, 0, c, dur, 0, Direction::DOWN, tx, ty, 1});
}

void VFXServer::smash_impact_1(float cx, float cy, float radius, Color c, float dur) {
    effects.push_back({"smash_impact_1", cx, cy, radius, c, dur, 0});
}

void VFXServer::smash_impact_2(float cx, float cy, float radius, Color c, float dur) {
    effects.push_back({"smash_impact_2", cx, cy, radius, c, dur, 0});
}

void VFXServer::smash_impact_3(float cx, float cy, float radius, Color c, float dur) {
    effects.push_back({"smash_impact_3", cx, cy, radius, c, dur, 0});
}
