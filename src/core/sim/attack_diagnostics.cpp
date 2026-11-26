#include "attack_diagnostics.h"

// 单一全局聚合器 — 单线程 sim, 无锁. 纯值累加, 不消费 rng, 不改任何状态.
static AttackDiagSnapshot g_totals;

void attack_diag_record(bool can_attack, int result_count, int damage,
                        float dist_ratio, bool is_crossbow) {
    ++g_totals.attack_calls;
    if (is_crossbow) ++g_totals.crossbow_calls;
    if (can_attack) {
        ++g_totals.can_attack_pass;
        g_totals.total_results += result_count;
        g_totals.total_damage += damage;
        // 弩走 ProjectilePool, 正常返回空 results, 不算 miss.
        if (result_count == 0 && !is_crossbow) ++g_totals.whiffs;
        // dist_ratio<0 = 场上无存活怪 (哨兵), 不计入任何距离桶.
        if (dist_ratio >= 0.0f) {
            int b = 0;
            if (dist_ratio > 0.75f) b = 3;
            else if (dist_ratio > 0.5f) b = 2;
            else if (dist_ratio > 0.25f) b = 1;
            if (b >= 0 && b < 4) ++g_totals.dist_bucket[b];
        }
    }
}

const AttackDiagSnapshot& attack_diag_totals() { return g_totals; }
void attack_diag_reset() { g_totals = AttackDiagSnapshot{}; }
