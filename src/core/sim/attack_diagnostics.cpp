#include "attack_diagnostics.h"

// 单一全局聚合器 — 单线程 sim, 无锁. 纯值累加, 不消费 rng, 不改任何状态.
static AttackDiagSnapshot g_totals;

void attack_diag_record(bool can_attack, int result_count, int damage,
                        float dist_ratio, bool is_crossbow, int hit_shape) {
    ++g_totals.attack_calls;
    if (is_crossbow) ++g_totals.crossbow_calls;
    if (can_attack) {
        ++g_totals.can_attack_pass;
        g_totals.total_results += result_count;
        g_totals.total_damage += damage;
        // 弩走 ProjectilePool, 正常返回空 results, 不算 miss.
        if (result_count == 0 && !is_crossbow) ++g_totals.whiffs;
        // 近战按形状分桶 (CIRCLE/SECTOR/RECTANGLE/CAPSULE = 0..3)
        if (!is_crossbow && hit_shape >= 0 && hit_shape <= 3) {
            ++g_totals.shape_pass[hit_shape];
            if (result_count == 0) ++g_totals.shape_whiff[hit_shape];
        }
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

// sim 批次开关 — 仅 sim 时采样, 真实游戏零开销
static bool g_enabled = false;
void attack_diag_set_enabled(bool enabled) { g_enabled = enabled; }
bool attack_diag_enabled() { return g_enabled; }

void attack_diag_record_hit(int hit_shape, float range_px, float width_param,
                            float nearest_dist_px, float angle_deg, int hit_count) {
    (void)hit_shape; (void)width_param;  // 备用: 未来按形状细分
    if (!g_enabled) return;
    ++g_totals.hit_probe;
    if (hit_count == 0) ++g_totals.hit_probe_empty;
    float ratio = range_px > 0.0f ? nearest_dist_px / range_px : -1.0f;
    if (ratio >= 0.0f) {
        int b = ratio > 0.75f ? 3 : ratio > 0.5f ? 2 : ratio > 0.25f ? 1 : 0;
        if (b >= 0 && b < 4) ++g_totals.probe_dist_bucket[b];
    }
    int a = angle_deg >= 90.0f ? 3 : angle_deg >= 60.0f ? 2 : angle_deg >= 30.0f ? 1 : 0;
    if (a >= 0 && a < 4) ++g_totals.probe_angle_bucket[a];
}

const AttackDiagSnapshot& attack_diag_totals() { return g_totals; }
void attack_diag_reset() { g_totals = AttackDiagSnapshot{}; }
