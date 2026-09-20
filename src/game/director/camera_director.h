#pragma once
// A6-T3: CameraDirector — 摄像机语言状态机 + 插值
// 管理 Boss 战运镜 (zoom_in/out) 和击杀顿帧

#include "data/camera_defs.h"
#include "raylib.h"       // Vector2
#include "raymath.h"      // Vector2 operators

enum class CameraState { NORMAL, BOSS_WAR, KILL_STUN };

class CameraDirector {
public:
    bool try_init(const CameraDef& def);
    
    // 状态切换
    void enter_boss_war();
    void exit_boss_war();
    void trigger_kill_stun();
    
    // 每帧更新 (dt 为 wall clock)
    void update(float dt, const Vector2& player_pos, const Vector2& boss_pos);
    
    // 查询接口
    Vector2 focus_offset() const;    // 相对玩家的位置偏移
    float fov_scale() const;         // 当前 FOV 缩放
    CameraState state() const;
    
private:
    void update_normal(float dt, const Vector2& player_pos);
    void update_boss_war(float dt, const Vector2& player_pos, const Vector2& boss_pos);
    void update_kill_stun(float dt);
    
    CameraDef _def;
    bool _initialized = false;
    CameraState _state = CameraState::NORMAL;
    
    // 当前插值状态
    float _current_fov_scale = 1.0f;
    Vector2 _current_focus_offset = {0, 0};
    
    // 目标值
    float _target_fov_scale = 1.0f;
    Vector2 _target_focus_offset = {0, 0};
    
    // 计时器
    float _boss_war_timer = 0.0f;
    float _kill_stun_timer = 0.0f;
    
    // Boss 战状态
    bool _boss_war_active = false;
};
