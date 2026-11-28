#pragma once

// ============================================================
// D4.6 Step1: GrowthCurve — 整个游戏统一数值来源
// 所有 Monster/Boss/Elite/Arena/Quest/Relic 倍率统一从这里读取
// ============================================================

struct GrowthCurve {
    float monster_hp;     // 怪物 HP 倍率
    float monster_atk;    // 怪物 ATK 倍率
    float boss_hp;        // Boss HP 倍率
    float boss_atk;       // Boss ATK 倍率
    float elite_scale;    // Elite 乘数
    float exp_scale;      // 经验倍率
    float gold_scale;     // 掉落倍率
    float relic_scale;    // 圣物稀有度
    float arena_scale;    // Arena伤害/收益
    float player_hp;      // 玩家等级HP成长
    float player_atk;     // 玩家等级ATK成长
};

class GrowthCurveSystem {
public:
    // P3-redesign: sim 缓和曲线开关 (由 sim_redesign_flags.numeric 驱动, main.cpp 置位)
    void set_sim_numeric(bool enabled) const { _sim_numeric = enabled; }

    // 核心查询
    const GrowthCurve& curve(int floor) const;

    // 快捷查询
    float hp_scale(int floor) const;       // 怪物HP倍率
    float atk_scale(int floor) const;      // 怪物ATK倍率
    float boss_hp_scale(int floor) const;   // Boss HP
    float boss_atk_scale(int floor) const;  // Boss ATK
    float elite_scale(int floor) const;     // Elite乘数
    float exp_scale(int floor) const;       // 经验
    float gold_scale(int floor) const;      // 掉落
    float relic_scale(int floor) const;     // 圣物稀有度
    float arena_scale(int floor) const;     // Arena

private:
    mutable bool _sim_numeric = false;  // P3-redesign
};

// 全局单实例
extern GrowthCurveSystem g_growth;
