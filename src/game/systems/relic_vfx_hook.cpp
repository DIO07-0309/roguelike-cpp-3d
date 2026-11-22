#include "relic_vfx_hook.h"

// Single global hook slot. Default null → fire is a no-op (sim / tests).
static RelicVfxHook s_relic_vfx_hook = nullptr;

void set_relic_vfx_hook(RelicVfxHook hook) { s_relic_vfx_hook = hook; }
void clear_relic_vfx_hook() { s_relic_vfx_hook = nullptr; }

void fire_relic_vfx_hook(const std::string& relic_id, float px, float py) {
    if (s_relic_vfx_hook) s_relic_vfx_hook(relic_id, px, py);
}
