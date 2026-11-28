#pragma once

// P3-redesign 系统性调参 — 环境变量旋钮 (仅 sim 生效, 真实游戏零变化).
// 每个旋钮有默认值, 可由 SIM_TUNE_* 环境变量覆盖, 便于批量脚本网格搜索.
// 门控由调用方负责 (sim_redesign_active() && flag) — 本模块只做 env→数值映射,
// 真实游戏调用方不读这些 getter.
namespace sim_tuning {

float loot_weight();        // 清怪阶段 loot 分 (默认 0.3)
float room_weight();        // 清怪阶段 room 分 (默认 0.2)
double stuck_budget();      // 累计卡死预算秒 (默认 240)
long wall_budget();         // 帧预算 (默认 72000)
int player_atk_per_level(); // 每级 ATK (默认 6)
int player_hp_per_level();  // 每级 HP (默认 30)
float hunt_threshold();     // 僵持秒数 (默认 8)
float boss_hp_target();     // 全局 Boss HP 倍率目标 (默认 5.0 = SIM_CURVES F15 值 → 系数 1.0)
float boss_atk_target();    // 全局 Boss ATK 倍率目标 (默认 2.5 → 系数 1.0)

}  // namespace sim_tuning
