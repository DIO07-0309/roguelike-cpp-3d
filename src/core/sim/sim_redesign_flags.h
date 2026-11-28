#pragma once

// P3-redesign 卡死系统性修复 — per-change 开关 (便于 A/B 二分定位).
// 全部默认 true (协同方案), main.cpp 在 --sim 时激活; 真实游戏恒 false.
// 每个开关对应一个改动: standstill / loot-suppress / progress-tighten / hunt-lock.
// 数值调整单独一个开关 (growth/budget/player-growth).
struct SimRedesignFlags {
    bool standstill_fix = true;     // 站桩死区交还 approach
    bool loot_suppress = true;      // 清怪阶段 loot/room 降权
    bool progress_tighten = true;   // p_move 假进展收紧
    bool hunt_lock = true;          // floor_hunting 锁存
    bool numeric = true;            // 数值+预算调整
};

const SimRedesignFlags& sim_redesign_flags();
SimRedesignFlags& sim_redesign_flags_mut();
void sim_redesign_set_active(bool active);  // false 时全部改动禁用 (真实游戏)
bool sim_redesign_active();
