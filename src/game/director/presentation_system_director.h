#pragma once
#include <string>
#include <vector>
#include "raylib.h"
#include "entities/entity.h"  // Direction enum

enum class BuildType : int;
class VFXServer;

// ═══════════════════════════════════════════════════════════════
// G5.8.2: BuildTheme — per-Build Presentation identity
// Single Source of Truth for all Camera/Audio/VFX/UI styling
// ═══════════════════════════════════════════════════════════════

// ── VFX Modifier (theme-driven primitive scaling) ──
struct VFXModifier {
    float particle_speed  = 1.0f;   // 1.0=normal, 0.7=slow(ice), 1.4=fast(lightning)
    float explosion_scale = 1.0f;   // radius multiplier
    float beam_width      = 1.0f;   // beam/bolt thickness
    float ring_speed      = 1.0f;   // ring expansion rate
    float spark_scale     = 1.0f;   // spark size multiplier
    float lifetime_scale  = 1.0f;   // effect duration multiplier
};

// ── Camera Modifier (theme-driven camera behavior) ──
struct CameraModifier {
    float shake_scale   = 1.0f;   // 1.0=normal, 1.3=fire, 0.8=ice
    float freeze_bonus  = 0.0f;   // extra freeze duration (seconds)
    float zoom_offset   = 0.0f;   // -0.05=zoom out(shadow), +0.03=zoom in(berserker)
    float dash_offset   = 0.0f;   // camera pull during player dash
};

// ── ScreenFX Modifier (post-process / overlay hints) ──
struct ScreenFXModifier {
    Color edge_tint{0, 0, 0, 0};     // screen edge color overlay (0 alpha = off)
    Color hit_flash_tint{255, 255, 255, 200}; // hit flash color
    float vignette_strength = 0.0f;  // 0=none, 0.3=shadow, 0.15=ice
};

// ── Audio Modifier (theme-driven audio hints) ──
struct AudioModifier {
    const char* hit_sfx    = "melee";    // "melee"|"slash"|"bolt"
    const char* skill_sfx  = "bolt";     // skill cast sound
    float pitch_shift = 0.0f;            // -0.1=deeper(fire), +0.05=higher(ice)
    float reverb_mix  = 0.0f;            // 0=dry, 0.3=shadow
};

// ── Full Theme ──
struct BuildTheme {
    Color primary   {255, 200, 50, 255};
    Color secondary {255, 180, 40, 220};
    Color accent    {240, 140, 30, 200};
    const char* name       = "neutral";
    const char* vfx_preset = "";

    VFXModifier    vfx;
    CameraModifier camera;
    ScreenFXModifier screen;
    AudioModifier  audio;

    static BuildTheme from_build_type(BuildType bt);
};

// ============================================================
// D6 Step5: PresentationSystemDirector — 统一表现层状态管理
// 组合模式: 持有 Shake/Freeze/DamageFloat/RoomMsg/Intro 所有状态
// GameScene 通过此 Director 访问表现层变量
// ============================================================

class PresentationSystemDirector {
public:
    // ── Damage Float ──
    // G10.4-B Fix2: max_lifetime 记录初始寿命 — alpha 按自身生命周期计算
    // (修复暴击 0.85s 除数硬编码 0.6f 的 alpha 溢出: 361→105 前段隐形)
    struct DamageFloat { float x, y; float lifetime; float max_lifetime = 0.6f;
                         int value; Color color; const char* label = nullptr; };
    std::vector<DamageFloat> damage_floats;
    void  spawn_damage(float wx, float wy, int dmg, Color c, float lifetime = 0.6f);
    void  spawn_label(float wx, float wy, const char* text, Color c, float lifetime = 0.8f);

    // ── Camera Shake ──
    float shake_timer = 0.0f;
    float shake_intensity = 0.0f;
    void  trigger_shake(float intensity);

    // ── Frame Freeze ──
    float freeze_timer = 0.0f;
    void  trigger_freeze(float duration);
    bool  is_frozen() const { return freeze_timer > 0.0f; }

    // ── Q4.7: 玩家受击红屏 ──
    float hit_flash_timer = 0.0f;
    void  trigger_hit_flash(float duration = 0.18f);

    // ── Room Message ──
    std::string room_msg;
    float room_msg_timer = 0.0f;
    void  show_message(const char* text, float duration = 2.5f);

    // ── G5.8.2: Build Theme ──
    void  set_build_theme(BuildType bt);
    const BuildTheme& get_theme() const { return _theme; }
    Color get_build_color() const { return _theme.primary; }  // backward compat
    BuildTheme _theme;

    // ── Boss Intro Text ──
    std::string boss_intro_text;
    std::string boss_modifier_text;

    // ── Floor/Chapter Intro ──
    bool  floor_intro_active = false;
    float floor_intro_timer = 0.0f;
    float floor_intro_fade = 0.0f;
    int   floor_intro_floor = 0;
    bool  chapter_intro_active = false;
    float chapter_intro_timer = 0.0f;
    int   chapter_intro_ch = -1;

    // ── Debug panel flags (按F键切换) ──
    bool show_growth_debug = false;
    bool combat_juice_on = true;
    bool show_flow_debug = false;
    bool show_boss_behavior = false;
    bool show_boss_cmd = false;
    bool show_boss_report = false;
    int  last_combo_announced = 0;

    // ── Tick (更新所有表现层计时器) ──
    void tick(float dt);
    void init_events();  // D7 Step5: 订阅 EventBus
    void unregister_events();  // 场景析构时注销, 防止悬挂订阅
};

// C1 helper
Color dmg_color_for(int dmg, bool is_magic, bool is_poison);
