#pragma once
// A6-T3: CameraLanguageDirector — 摄像机语言状态机 + 插值
// 管理 Boss 战运镜 (zoom_in/out) 和击杀顿帧

#include "data/camera_defs.h"
#include "raylib.h"       // Vector2
#include "raymath.h"      // Vector2 operators

enum class CameraState { NORMAL, BOSS_WAR, KILL_STUN };

class CameraLanguageDirector {
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
    int frame_count() const;         // 帧计数器 (调试用)
    
    // 设置视野半径 (像素), 用于限制偏移范围
    void set_fov_radius(float radius_px);
    
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
    float _focus_timer = 0.0f;  // 聚焦持续时间计时器
    
    // Boss 战状态
    bool _boss_war_active = false;
    
    // 视野半径 (像素), 限制偏移范围
    float _fov_radius_px = 160.0f;  // 默认 5 tile (32px * 5)
    
    // 调试用帧计数器
    int _frame_count = 0;
};
