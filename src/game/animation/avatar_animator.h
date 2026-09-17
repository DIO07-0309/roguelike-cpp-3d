#pragma once
// A5-T3: 动画选择器 — 状态→clip + 计时 (纯逻辑, 时间由外部喂 dt, 无 GetTime)
#include <map>
#include <string>
#include "data/animation_defs.h"

struct AnimInput {
    bool attacking = false;
    bool hit_flash = false;             // hp 下降沿镜像 (PlayerAvatar 内维护)
    bool moving = false;
    float attack_recovery_ratio = 1.f;  // 实际 recovery/0.36, attack 上升沿采样
};

// 优先级 attack > hit > walk/idle; attack/hit 一次性播完不中断; hit 边沿触发+锁定
class AvatarAnimator {
public:
    AvatarAnimator();
    void advance(float dt, const AnimInput& in);
    const std::string& current_name() const { return _name; }
    float dur_scale() const { return _name == "attack" ? _dur_scale : 1.f; }
    float time() const { return _t; }
    float pose_time() const { return time() / dur_scale(); }
    const AnimClipDef* clip(const AnimSetDef& set) const;
    void set_durations(const AnimSetDef& set);   // try_init 用 JSON 真值同步
private:
    bool is_one_shot() const { return _name == "attack" || _name == "hit"; }
    float current_dur() const;
    void switch_to(const std::string& name);
    std::string _name = "idle";
    float _t = 0.f;
    float _dur_scale = 1.f;
    bool _prev_hit = false;
    bool _prev_attack = false;
    bool _hit_pending = false;
    std::map<std::string, float> _durs;
};
