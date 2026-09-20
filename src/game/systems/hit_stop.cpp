#include "systems/hit_stop.h"
#include <algorithm>

namespace {
constexpr float kEpsilon = 1e-6f;
}

void HitStop::trigger(float duration) {
    if (duration <= 0.0f) return;
    _remaining = duration;
    _active = true;
}

void HitStop::update(float dt) {
    if (!_active) return;
    _remaining -= dt;
    if (_remaining <= kEpsilon) {
        _remaining = 0.0f;
        _active = false;
    }
}

bool HitStop::active() const { return _active; }
float HitStop::remaining() const { return _remaining; }
bool HitStop::is_stunned() const { return _active; }
