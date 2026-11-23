#pragma once
#include <cstdint>

// P3: sim 卡死诊断聚合 — 纯值累加, 不改 gameplay / RNG / 输出日志.
// 把每局 sim_ai.cpp 的既有诊断计数器在 _collect_sim_stats 时累加, 汇总进
// balance_report.json 的 diagnostics 段, 便于 300-500 局对比无需扒 game.log.
struct SimDiagSnapshot {
    int64_t hunt_enters = 0;        // hunt 僵持检测触发次数
    int64_t hunt_frames = 0;        // hunt 期间帧数
    int64_t hunt_approach = 0;      // hunt 期间 approach 帧数
    int64_t move_rec = 0, move_loot = 0, move_room = 0, move_appr = 0, move_stand = 0;
    int64_t bfs_fail = 0;           // appr 分支 step==-1 帧数
    int64_t progress_move = 0, progress_monhp = 0, progress_php = 0, progress_alive = 0;
    int64_t step_attempts = 0, step_rollbacks = 0;
    int64_t stuck_bfs_hit = 0, stuck_bfs_fail = 0;
    int64_t runs = 0;               // 累加的局数
};

void sim_diag_accumulate(const SimDiagSnapshot& per_run);
const SimDiagSnapshot& sim_diag_totals();
void sim_diag_reset();
