#include "sim_diagnostics.h"

// 单一全局聚合器 — 单线程 sim, 无锁.
static SimDiagSnapshot g_totals;

void sim_diag_accumulate(const SimDiagSnapshot& per_run) {
    g_totals.hunt_enters += per_run.hunt_enters;
    g_totals.hunt_frames += per_run.hunt_frames;
    g_totals.hunt_approach += per_run.hunt_approach;
    g_totals.move_rec += per_run.move_rec;
    g_totals.move_loot += per_run.move_loot;
    g_totals.move_room += per_run.move_room;
    g_totals.move_appr += per_run.move_appr;
    g_totals.move_stand += per_run.move_stand;
    g_totals.bfs_fail += per_run.bfs_fail;
    g_totals.progress_move += per_run.progress_move;
    g_totals.progress_monhp += per_run.progress_monhp;
    g_totals.progress_php += per_run.progress_php;
    g_totals.progress_alive += per_run.progress_alive;
    g_totals.step_attempts += per_run.step_attempts;
    g_totals.step_rollbacks += per_run.step_rollbacks;
    g_totals.target_switch += per_run.target_switch;
    g_totals.stuck_bfs_hit += per_run.stuck_bfs_hit;
    g_totals.stuck_bfs_fail += per_run.stuck_bfs_fail;
    g_totals.runs += per_run.runs;
}

const SimDiagSnapshot& sim_diag_totals() { return g_totals; }
void sim_diag_reset() { g_totals = SimDiagSnapshot{}; }
