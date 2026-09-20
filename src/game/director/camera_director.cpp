#include "game/director/camera_director.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kEpsilon = 1e-4f;

float lerp(float a, float b, float t) {
    return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
}

Vector2 lerp_vec(Vector2 a, Vector2 b, float t) {
    return {lerp(a.x, b.x, t), lerp(a.y, b.y, t)};
}

Vector2 vec_add(Vector2 a, Vector2 b) {
    return {a.x + b.x, a.y + b.y};
}

Vector2 vec_sub(Vector2 a, Vector2 b) {
    return {a.x - b.x, a.y - b.y};
}

Vector2 vec_mul(Vector2 a, float s) {
    return {a.x * s, a.y * s};
}
}

bool CameraLanguageDirector::try_init(const CameraDef& def) {
    _def = def;
    _initialized = true;
    _state = CameraState::NORMAL;
    _current_fov_scale = 1.0f;
    _target_fov_scale = 1.0f;
    _current_focus_offset = {0, 0};
    _target_focus_offset = {0, 0};
    return true;
}

void CameraLanguageDirector::enter_boss_war() {
    if (!_initialized) return;
    _state = CameraState::BOSS_WAR;
    _boss_war_active = true;
    _boss_war_timer = _def.boss_war.zoom_in.duration;
    _target_fov_scale = _def.boss_war.zoom_in.fov_scale;
}

void CameraLanguageDirector::exit_boss_war() {
    if (!_initialized || !_boss_war_active) return;
    _boss_war_active = false;
    _state = CameraState::NORMAL;
    _target_fov_scale = 1.0f;
    _target_focus_offset = {0, 0};
}

void CameraLanguageDirector::trigger_kill_stun() {
    if (!_initialized) return;
    _state = CameraState::KILL_STUN;
    _kill_stun_timer = _def.kill_stun.duration;
}

void CameraLanguageDirector::update(float dt, const Vector2& player_pos, const Vector2& boss_pos) {
    if (!_initialized) return;
    
    switch (_state) {
        case CameraState::NORMAL:
            update_normal(dt, player_pos);
            break;
        case CameraState::BOSS_WAR:
            update_boss_war(dt, player_pos, boss_pos);
            break;
        case CameraState::KILL_STUN:
            update_kill_stun(dt);
            break;
    }
}

void CameraLanguageDirector::update_normal(float dt, const Vector2& player_pos) {
    (void)player_pos;
    // 平滑插值回正常状态
    float lerp_t = dt * _def.boss_war.lerp_speed;
    _current_fov_scale = lerp(_current_fov_scale, _target_fov_scale, lerp_t);
    _current_focus_offset = lerp_vec(_current_focus_offset, _target_focus_offset, lerp_t);
}

void CameraLanguageDirector::update_boss_war(float dt, const Vector2& player_pos, const Vector2& boss_pos) {
    // Boss 战期间: 聚焦玩家和 Boss 的中点
    Vector2 sum = vec_add(player_pos, boss_pos);
    Vector2 mid_point = vec_mul(sum, 0.5f);
    _target_focus_offset = vec_sub(mid_point, player_pos);
    
    // 插值到目标 FOV
    float lerp_t = dt * _def.boss_war.lerp_speed;
    _current_fov_scale = lerp(_current_fov_scale, _target_fov_scale, lerp_t);
    _current_focus_offset = lerp_vec(_current_focus_offset, _target_focus_offset, lerp_t);
    
    // Boss 战计时器 (用于可能的淡出)
    if (_boss_war_timer > 0) {
        _boss_war_timer -= dt;
    }
}

void CameraLanguageDirector::update_kill_stun(float dt) {
    _kill_stun_timer -= dt;
    if (_kill_stun_timer <= 0) {
        _kill_stun_timer = 0;
        _state = CameraState::NORMAL;
        _target_fov_scale = 1.0f;
        _target_focus_offset = {0, 0};
    }
}

Vector2 CameraLanguageDirector::focus_offset() const {
    return _current_focus_offset;
}

float CameraLanguageDirector::fov_scale() const {
    return _current_fov_scale;
}

CameraState CameraLanguageDirector::state() const {
    return _state;
}
