#pragma once

// ============================================================
// D4.6 Step2: CombatFeelSystem — 统一打击感常数
// 所有HitStop/Shake/Crit/DamageNumber从此读取
// ============================================================
struct CombatFeelSystem {
    // ── G5.7: HitStop tuning (juicier) ──
    static constexpr float LIGHT_HIT     = 0.030f;
    static constexpr float HEAVY_HIT     = 0.075f;
    static constexpr float BOSS_HIT      = 0.10f;
    static constexpr float CRITICAL_HIT  = 0.07f;
    static constexpr float KILL_SLOWMO   = 0.14f;
    static constexpr float RELIC_PICKUP  = 0.20f;
    static constexpr float BUILD_COMPLETE = 0.18f;
    static constexpr float SKILL_EVOLVED  = 0.12f;  // G5.7

    // ── G5.7: CameraShake tuning (more impact) ──
    static constexpr float SHAKE_LIGHT   = 3.0f;
    static constexpr float SHAKE_MEDIUM  = 8.0f;
    static constexpr float SHAKE_HEAVY   = 15.0f;
    static constexpr float SHAKE_BOSS    = 20.0f;
    static constexpr float SHAKE_CRIT    = 5.0f;   // G5.7
    static constexpr float SHAKE_COMBO   = 4.0f;   // G5.7

    // ── Damage float tuning ──
    static constexpr float DMG_FLOAT_LIFE_LIGHT  = 0.55f;
    static constexpr float DMG_FLOAT_LIFE_HEAVY  = 0.75f;
    static constexpr float DMG_FLOAT_LIFE_CRIT   = 0.90f;
    static constexpr float DMG_FLOAT_SCALE_MIN   = 1.0f;
    static constexpr float DMG_FLOAT_SCALE_CRIT  = 1.6f;  // crit numbers are bigger

    // Combo里程碑 (hit数)
    static constexpr int COMBO_5   = 5;
    static constexpr int COMBO_10  = 10;
    static constexpr int COMBO_20  = 20;
    static constexpr int COMBO_MAX = 30;

    // 暴击 (combo驱动: 段数越高暴击率越高)
    static float crit_chance(int combo_count) {
        if (combo_count >= 4) return 0.30f;  // 重击 30%
        if (combo_count == 3) return 0.15f;
        if (combo_count == 2) return 0.08f;
        return 0.03f; // combo 1: 3%
    }

    static int crit_multiplier() { return 2; }  // crit = ×2

    // ── G39: 命中分级 → (顿帧秒数, 屏震幅度) ──
    // 聚合取最高等级 (KILL > CRITICAL > HEAVY > LIGHT), 单次命中只触发一档不叠加。
    // ★ G39 之前屏震恒为 0 — SHAKE_LIGHT/COMBO/CRIT/MEDIUM 四个常量从未被读取,
    // 普攻只有顿帧没有震。本函数是这两个值的唯一真源, 故放这里而非 PlayerController:
    // 手感等级表属于"打击感设计", 与谁调用无关; 放公共头也让它可以被单元测试钉住。
    static void hit_feel_for(bool has_kill, bool has_crit, int combo_stage,
                             float& freeze_seconds, float& shake_amount) {
        if (has_kill)       { shake_amount = SHAKE_MEDIUM; freeze_seconds = KILL_SLOWMO;  return; }
        if (has_crit)       { shake_amount = SHAKE_CRIT;   freeze_seconds = CRITICAL_HIT; return; }
        if (combo_stage == 2){ shake_amount = SHAKE_COMBO; freeze_seconds = HEAVY_HIT;   return; }
        shake_amount = SHAKE_LIGHT;
        freeze_seconds = LIGHT_HIT;
    }

    // Combo消息
    static const char* combo_message(int count) {
        if (count >= 30) return "PERFECT COMBO!";
        if (count >= 20) return "COMBO ×20!";
        if (count >= 10) return "COMBO ×10!";
        if (count >= 5)  return "5 HIT!";
        return nullptr;
    }
};
