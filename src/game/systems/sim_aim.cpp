#include "sim_aim.h"

static bool g_sim_hit_aim = false;

void sim_hit_aim_set_enabled(bool enabled) { g_sim_hit_aim = enabled; }
bool sim_hit_aim_enabled() { return g_sim_hit_aim; }
