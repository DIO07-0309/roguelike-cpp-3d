#pragma once

// P3-redesign: sim 命中瞄准开关. 与纯观测诊断标志解耦 (职责单一).
// main.cpp 在 --sim 时置 true; 真实游戏恒 false.
void sim_hit_aim_set_enabled(bool enabled);
bool sim_hit_aim_enabled();
