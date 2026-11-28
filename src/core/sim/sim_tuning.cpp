#include "sim_tuning.h"
#include <cstdlib>

namespace sim_tuning {

// 读环境变量: 设了且能解析则返回, 否则用默认值.
static float envf(const char* name, float def) {
    const char* v = std::getenv(name);
    if (!v || !*v) return def;
    return static_cast<float>(std::atof(v));
}
static double envd(const char* name, double def) {
    const char* v = std::getenv(name);
    if (!v || !*v) return def;
    return std::atof(v);
}
static int envi(const char* name, int def) {
    const char* v = std::getenv(name);
    if (!v || !*v) return def;
    return std::atoi(v);
}

float loot_weight()        { return envf("SIM_TUNE_LOOT_W", 0.3f); }
float room_weight()        { return envf("SIM_TUNE_ROOM_W", 0.2f); }
double stuck_budget()      { return envd("SIM_TUNE_STUCK", 400.0); }  // 150局验证: 240→400 使 win 0→19.3%
long wall_budget()         { return static_cast<long>(envd("SIM_TUNE_WALL", 72000.0)); }
int player_atk_per_level() { return envi("SIM_TUNE_ATK", 6); }
int player_hp_per_level()  { return envi("SIM_TUNE_HP", 30); }
float hunt_threshold()     { return envf("SIM_TUNE_HUNT", 8.0f); }
float boss_hp_target()     { return envf("SIM_TUNE_BOSSHP", 5.0f); }
float boss_atk_target()    { return envf("SIM_TUNE_BOSSATK", 2.5f); }

}  // namespace sim_tuning
