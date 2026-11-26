#pragma once
#include <cstdint>

// P3-redesign 攻击链路诊断 — 纯观测, 不改 gameplay.
// 回答: AI 选 attack 后到底断在哪一步?
//   attack_calls    : _weapon_attack 被调用次数 (AI 选了 attack)
//   can_attack_pass : 其中 can_attack() 放行次数 (未放行 = 冷却门控)
//   total_results   : execute 返回的命中总数 (放行但为0 = 命中判定 miss)
//   total_damage    : 命中伤害总和
//   whiffs          : 放行但 results 为空的次数
struct AttackDiagSnapshot {
    int64_t attack_calls = 0;
    int64_t can_attack_pass = 0;
    int64_t total_results = 0;
    int64_t total_damage = 0;
    int64_t whiffs = 0;              // 仅近战形状的空 results (弩弹体不计)
    // 第二层: 放行攻击时的最近怪距离桶 (按 reach 归一: 0=<1/4, 1=<1/2, 2=<3/4, 3=>3/4)
    int64_t dist_bucket[4] = {0, 0, 0, 0};
    int64_t crossbow_calls = 0;  // 弩(弹体)攻击次数 — 走 ProjectilePool 不走 hit_detect
};

void attack_diag_record(bool can_attack, int result_count, int damage,
                        float dist_ratio, bool is_crossbow);
const AttackDiagSnapshot& attack_diag_totals();
void attack_diag_reset();
