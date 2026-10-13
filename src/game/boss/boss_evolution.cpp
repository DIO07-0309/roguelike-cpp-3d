#include "boss_evolution.h"
#include "relationship_system.h"

// ============================================================
// D5 Step2: Boss Evolution — 技能变体 + LastStand + Build反制
// ============================================================

// Build反制描述
const char* get_build_counter_desc(int boss_floor, BuildType bt) {
    switch (bt) {
        case BuildType::POISON_MASTER: return "周期性净化";
        case BuildType::FIRE_MAGE:     return boss_floor==10?"fire_resist":"火抗+";
        case BuildType::TIME_MASTER:   return "时间加速";
        case BuildType::BERSERKER:     return "格挡反击";
        case BuildType::SUPPORT:       return "治疗打断";
        default: return nullptr;
    }
}

// LastStand效果文字
static const char* last_stand_name(int floor) {
    if (floor == 5)  return "暗影狂怒";
    if (floor == 10) return "炼狱爆发";
    if (floor == 15) return "终焉降临";
    return "Last Stand";
}

// ── G52: 子步骤 (原 calc_boss_evolution 96 行拆 5 段) ──

// Build 反制: 6 种 BuildType 的 Boss 对应强化 (idx=NONE 之后 6 个)
static void boss_evo_apply_build_counter(BossEvolutionData& ev, BuildType bt) {
    if (bt == BuildType::NONE) return;
    int idx = (int)bt - 1;  // skip NONE=0
    if (idx >= 0 && idx < 6) ev.build_counter_active[idx] = true;
    switch (bt) {
    case BuildType::POISON_MASTER:
        // Boss 周期净化(poison tick 减半) + 冲锋伤害+
        ev.charge_mod.damage_scale = 1.15f;
        break;
    case BuildType::BERSERKER:
        // Boss 格挡: charge cd 延长 + shockwave 范围小减
        ev.charge_mod.cooldown_scale = 0.85f;
        ev.shockwave_mod.area_scale = 0.90f;
        break;
    case BuildType::TIME_MASTER:
        // Boss 时间加速: 全体 CD -15%
        ev.charge_mod.cooldown_scale = 0.85f;
        ev.shockwave_mod.cooldown_scale = 0.85f;
        ev.summon_mod.cooldown_scale = 0.85f;
        break;
    case BuildType::FIRE_MAGE:
        // Boss 火抗: HP 倍率已在外层处理 (via calc_modifier)
        ev.shockwave_mod.damage_scale = 1.10f;
        break;
    default:
        break;
    }
}

// F5 暗影骑士 — WorldFlag 技能变体
static void boss_evo_apply_f5(BossEvolutionData& ev, const WorldState& ws) {
    if (ws.has(WorldFlag::Blood_Ritual)) {
        ev.charge_mod.variant_name = "Blood Dash";
        ev.charge_mod.damage_scale += 0.15f;
    }
    if (ws.has(WorldFlag::Saved_Prisoner))
        ev.summon_mod.cooldown_scale = 1.30f;   // 召唤 CD 延长(囚犯祝福)
    if (ws.has(WorldFlag::Accepted_Curse))
        ev.shockwave_mod.damage_scale += 0.10f;
}

// F10 地狱火魔 — WorldFlag 技能变体
static void boss_evo_apply_f10(BossEvolutionData& ev, const WorldState& ws) {
    if (ws.has(WorldFlag::Blood_Ritual)) {
        ev.shockwave_mod.variant_name = "Inferno";
        ev.shockwave_mod.area_scale += 0.25f;
        ev.shockwave_mod.damage_scale += 0.10f;
    }
    if (ws.has(WorldFlag::Saved_Priest)) {
        ev.charge_mod.damage_scale = 0.85f;      // 神官祝福: 冲锋减伤
        ev.evolution_name = "Blessed Flame";
    }
    if (ws.has(WorldFlag::Accepted_Curse)) {
        ev.summon_mod.extra_projectiles = 1;     // 多召 1 只
        ev.evolution_name = "Cursed Fire";
    }
}

// F15 深渊之主 — WorldFlag + NPC 关系 + Fear 高阶联动
static void boss_evo_apply_f15(BossEvolutionData& ev, const WorldState& ws,
                               const RelationshipSystem& rels) {
    if (ws.has(WorldFlag::Blood_Ritual)) {
        ev.charge_mod.variant_name = "Abyssal Charge";
        ev.charge_mod.damage_scale += 0.15f;
        ev.shockwave_mod.area_scale += 0.15f;
        ev.evolution_name = "Blood Avatar";
    }
    if (ws.has(WorldFlag::Boss1_Defeated) && ws.has(WorldFlag::Boss2_Defeated)) {
        ev.shockwave_mod.variant_name = "Tri-Pulse";
        ev.shockwave_mod.damage_scale += 0.12f;
    }
    if (ws.has(WorldFlag::Merchant_Killed)) {
        ev.summon_mod.double_strike = true;      // 召唤 2x
        ev.evolution_name = ev.evolution_name ? ev.evolution_name : "Dark Overseer";
    }
    if (ws.has(WorldFlag::Saved_Priest))
        ev.charge_mod.cooldown_scale = 1.20f;    // CD 延长
    if (rels.check_relation(140, RelationType::RESPECT, 3))
        ev.shockwave_mod.damage_scale -= 0.10f;  // 守望者尊敬: 冲击波减伤
    // Fear 高: 额外弹幕
    bool high_fear = false;
    for (auto& r : rels.all()) {
        if (r.fear >= 4) { high_fear = true; break; }
    }
    if (high_fear) ev.summon_mod.extra_projectiles += 1;
}

// 数值收敛 — 防止多重 flag 叠加后超出设计区间
static void boss_evo_clamp(BossEvolutionData& ev) {
    if (ev.charge_mod.cooldown_scale < 0.60f)    ev.charge_mod.cooldown_scale = 0.60f;
    if (ev.shockwave_mod.cooldown_scale < 0.60f) ev.shockwave_mod.cooldown_scale = 0.60f;
    if (ev.summon_mod.cooldown_scale < 0.60f)    ev.summon_mod.cooldown_scale = 0.60f;
    if (ev.charge_mod.damage_scale > 1.50f)      ev.charge_mod.damage_scale = 1.50f;
    if (ev.shockwave_mod.damage_scale > 1.50f)   ev.shockwave_mod.damage_scale = 1.50f;
}

BossEvolutionData calc_boss_evolution(int boss_floor,
    const WorldState& ws, BuildType bt,
    const RelationshipSystem& rels) {
    BossEvolutionData ev;
    ev.boss_floor = boss_floor;

    boss_evo_apply_build_counter(ev, bt);

    // WorldFlag 技能变体 (按 Boss 层分派)
    if (boss_floor == 5) {
        boss_evo_apply_f5(ev, ws);
    } else if (boss_floor == 10) {
        boss_evo_apply_f10(ev, ws);
    } else if (boss_floor == 15) {
        boss_evo_apply_f15(ev, ws, rels);
    }

    // LastStand 名称 (未设置则取楼层默认)
    ev.evolution_name = ev.evolution_name ? ev.evolution_name
                                         : last_stand_name(boss_floor);

    boss_evo_clamp(ev);
    return ev;
}
