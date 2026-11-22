#pragma once
#include <string>

// Presentation-only hook fired when a relic is acquired via Player::add_relic.
// Gameplay-agnostic: no-op until a visual layer registers a callback.
// Sim builds never register, so firing has zero effect on determinism.
using RelicVfxHook = void (*)(const std::string& relic_id, float px, float py);

void set_relic_vfx_hook(RelicVfxHook hook);
void fire_relic_vfx_hook(const std::string& relic_id, float px, float py);
void clear_relic_vfx_hook();
