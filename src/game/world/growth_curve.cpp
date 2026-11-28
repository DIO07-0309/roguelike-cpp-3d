#include "growth_curve.h"
#include <cmath>

// ============================================================
// G9.4 数值平衡: monster_hp ×1.25/floor, monster_atk ×1.18/floor
// Boss HP 大幅提升 (×1.5~2.0 原值)，DEF 有节制增长
// F1 保持不变 (教学层), F6+/F11+ 加速
// ============================================================

static const GrowthCurve CURVES[15] = {
    // hp   atk  boss_hp boss_atk elite exp gold relic arena ph_sr
    // F1: 教学层
    {1.00f,1.00f, 2.50f, 1.50f, 2.00f, 1.00f,1.00f,1.00f,1.00f, 1.00f,1.00f},
    // F2
    {1.20f,1.15f, 2.80f, 1.60f, 2.10f, 1.05f,1.05f,1.05f,1.05f, 1.02f,1.02f},
    // F3
    {1.44f,1.32f, 3.15f, 1.75f, 2.20f, 1.10f,1.10f,1.10f,1.10f, 1.04f,1.04f},
    // F4
    {1.73f,1.52f, 3.55f, 1.95f, 2.30f, 1.18f,1.18f,1.20f,1.15f, 1.06f,1.06f},
    // F5: Boss1 — M4b: 普攻低伤 (atk 15*2.3≈35), 技能靠 multiplier 有威胁
    {2.20f,2.00f, 4.50f, 2.30f, 2.40f, 1.30f,1.30f,1.40f,1.20f, 1.08f,1.08f},
    // F6: 第二章
    {2.35f,2.10f, 5.00f, 2.60f, 2.50f, 1.45f,1.40f,1.55f,1.35f, 1.12f,1.12f},
    // F7
    {2.80f,2.40f, 5.60f, 2.90f, 2.60f, 1.55f,1.50f,1.65f,1.45f, 1.16f,1.16f},
    // F8
    {3.30f,2.75f, 6.30f, 3.20f, 2.70f, 1.65f,1.60f,1.75f,1.55f, 1.20f,1.20f},
    // F9
    {3.90f,3.15f, 7.00f, 3.60f, 2.80f, 1.75f,1.70f,1.85f,1.65f, 1.24f,1.24f},
    // F10: Boss2
    {4.50f,3.60f, 8.00f, 4.00f, 2.90f, 1.90f,1.85f,2.00f,1.80f, 1.28f,1.28f},
    // F11: 第三章
    {5.00f,4.00f, 9.00f, 4.50f, 3.00f, 2.10f,2.00f,2.20f,2.00f, 1.32f,1.32f},
    // F12
    {5.80f,4.50f, 10.0f, 5.00f, 3.10f, 2.25f,2.15f,2.35f,2.15f, 1.36f,1.36f},
    // F13
    {6.70f,5.10f, 11.5f, 5.60f, 3.20f, 2.40f,2.30f,2.50f,2.30f, 1.40f,1.40f},
    // F14
    {7.70f,5.80f, 13.0f, 6.20f, 3.30f, 2.55f,2.45f,2.65f,2.45f, 1.44f,1.44f},
    // F15: 深渊之主
    {9.00f,6.50f, 15.0f, 7.00f, 3.50f, 2.70f,2.60f,2.80f,2.60f, 1.48f,1.48f},
};

// P3-redesign: sim 专用缓和成长曲线 (单调; 仅前4列敌人/boss hp/atk 下调, 第5列起 elite/exp/gold/relic/arena/player 与原表一致).
static const GrowthCurve SIM_CURVES[15] = {
    {1.00f, 1.00f, 2.50f, 1.50f, 2.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f},
    {1.18f, 1.13f, 2.68f, 1.57f, 2.10f, 1.05f, 1.05f, 1.05f, 1.05f, 1.02f, 1.02f},
    {1.36f, 1.26f, 2.86f, 1.64f, 2.20f, 1.10f, 1.10f, 1.10f, 1.10f, 1.04f, 1.04f},
    {1.54f, 1.39f, 3.04f, 1.71f, 2.30f, 1.18f, 1.18f, 1.20f, 1.15f, 1.06f, 1.06f},
    {1.71f, 1.51f, 3.21f, 1.79f, 2.40f, 1.30f, 1.30f, 1.40f, 1.20f, 1.08f, 1.08f},
    {1.89f, 1.64f, 3.39f, 1.86f, 2.50f, 1.45f, 1.40f, 1.55f, 1.35f, 1.12f, 1.12f},
    {2.07f, 1.77f, 3.57f, 1.93f, 2.60f, 1.55f, 1.50f, 1.65f, 1.45f, 1.16f, 1.16f},
    {2.25f, 1.90f, 3.75f, 2.00f, 2.70f, 1.65f, 1.60f, 1.75f, 1.55f, 1.20f, 1.20f},
    {2.43f, 2.03f, 3.93f, 2.07f, 2.80f, 1.75f, 1.70f, 1.85f, 1.65f, 1.24f, 1.24f},
    {2.61f, 2.16f, 4.11f, 2.14f, 2.90f, 1.90f, 1.85f, 2.00f, 1.80f, 1.28f, 1.28f},
    {2.79f, 2.29f, 4.29f, 2.21f, 3.00f, 2.10f, 2.00f, 2.20f, 2.00f, 1.32f, 1.32f},
    {2.96f, 2.41f, 4.46f, 2.29f, 3.10f, 2.25f, 2.15f, 2.35f, 2.15f, 1.36f, 1.36f},
    {3.14f, 2.54f, 4.64f, 2.36f, 3.20f, 2.40f, 2.30f, 2.50f, 2.30f, 1.40f, 1.40f},
    {3.32f, 2.67f, 4.82f, 2.43f, 3.30f, 2.55f, 2.45f, 2.65f, 2.45f, 1.44f, 1.44f},
    {3.50f, 2.80f, 5.00f, 2.50f, 3.50f, 2.70f, 2.60f, 2.80f, 2.60f, 1.48f, 1.48f},
};

GrowthCurveSystem g_growth;

const GrowthCurve& GrowthCurveSystem::curve(int floor) const {
    const GrowthCurve* table = _sim_numeric ? SIM_CURVES : CURVES;
    if (floor < 1) return table[0];
    if (floor > 15) return table[14];
    return table[floor - 1];
}

float GrowthCurveSystem::hp_scale(int floor) const       { return curve(floor).monster_hp; }
float GrowthCurveSystem::atk_scale(int floor) const      { return curve(floor).monster_atk; }
float GrowthCurveSystem::boss_hp_scale(int floor) const   { return curve(floor).boss_hp; }
float GrowthCurveSystem::boss_atk_scale(int floor) const  { return curve(floor).boss_atk; }
float GrowthCurveSystem::elite_scale(int floor) const     { return curve(floor).elite_scale; }
float GrowthCurveSystem::exp_scale(int floor) const       { return curve(floor).exp_scale; }
float GrowthCurveSystem::gold_scale(int floor) const      { return curve(floor).gold_scale; }
float GrowthCurveSystem::relic_scale(int floor) const     { return curve(floor).relic_scale; }
float GrowthCurveSystem::arena_scale(int floor) const     { return curve(floor).arena_scale; }
