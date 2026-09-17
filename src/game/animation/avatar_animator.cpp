// A5-T3: AvatarAnimator 实现
#include "game/animation/avatar_animator.h"

#include <algorithm>

AvatarAnimator::AvatarAnimator() {
    _durs["idle"] = 2.4f;     // 兜底默认, set_durations 以 JSON 为准
    _durs["walk"] = 0.7f;
    _durs["attack"] = 0.36f;
    _durs["hit"] = 0.18f;
}

float AvatarAnimator::current_dur() const {
    auto it = _durs.find(_name);
    float d = it == _durs.end() ? 0.f : it->second;
    return d * (_name == "attack" ? _dur_scale : 1.f);
}

void AvatarAnimator::switch_to(const std::string& name) {
    _name = name;
    _t = 0.f;
}

const AnimClipDef* AvatarAnimator::clip(const AnimSetDef& set) const {
    auto it = set.clips.find(_name);
    if (it == set.clips.end()) it = set.clips.find("idle");
    return it == set.clips.end() ? nullptr : &it->second;
}

void AvatarAnimator::set_durations(const AnimSetDef& set) {
    for (const auto& [name, c] : set.clips)
        if (_durs.count(name)) _durs[name] = c.dur;
}

void AvatarAnimator::advance(float dt, const AnimInput& in) {
    bool rising_hit = in.hit_flash && !_prev_hit;
    _prev_hit = in.hit_flash;

    bool rising_attack = in.attacking && !_prev_attack;
    _prev_attack = in.attacking;

    if (rising_hit) _hit_pending = true;         // 一次性态延后到播完帧消费
    if (rising_attack) {
        _dur_scale = std::clamp(in.attack_recovery_ratio, 1.f, 2.f);
        switch_to("attack");
    } else if (rising_hit && !is_one_shot()) {   // loop 态 hit 立即接管
        _hit_pending = false;
        switch_to("hit");
    }
    _t += dt;
    if (is_one_shot() && _t >= current_dur()) {  // 播完判定在帧尾 (大 dt 可同帧越界)
        bool go_hit = _hit_pending;
        _hit_pending = false;
        switch_to(go_hit ? "hit" : (in.moving ? "walk" : "idle"));
    } else if (!is_one_shot() && !rising_attack) {
        std::string want = in.moving ? "walk" : "idle";
        if (want != _name) switch_to(want);
    }
}
