#include "sim_redesign_flags.h"
#include <cstdlib>

static SimRedesignFlags g_flags;
static bool g_active = false;

// 二分定位: SIM_RD_<NAME>=0 关闭单个改动 (无需重编译).
static bool env_flag(const char* name, bool def) {
    const char* v = std::getenv(name);
    return v ? (v[0] != '0') : def;
}
static struct FlagsInit {
    FlagsInit() {
        g_flags.standstill_fix = env_flag("SIM_RD_STANDSTILL", true);
        g_flags.loot_suppress = env_flag("SIM_RD_LOOT", true);
        g_flags.progress_tighten = env_flag("SIM_RD_PROGRESS", true);
        g_flags.hunt_lock = env_flag("SIM_RD_HUNTLOCK", true);
        g_flags.numeric = env_flag("SIM_RD_NUMERIC", true);
    }
} g_flags_init;

const SimRedesignFlags& sim_redesign_flags() { return g_flags; }
SimRedesignFlags& sim_redesign_flags_mut() { return g_flags; }
bool sim_redesign_active() { return g_active; }
void sim_redesign_set_active(bool active) { g_active = active; }
