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

float vec_len(Vector2 a) {
    return sqrtf(a.x * a.x + a.y * a.y);
}

// 限制向量长度到 max_len
Vector2 vec_limit(Vector2 a, float max_len) {
    float len = vec_len(a);
    if (len <= max_len || len < 0.001f) return a;
    return {a.x / len * max_len, a.y / len * max_len};
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

void CameraLanguageDirector::set_fov_radius(float radius_px) {
    _fov_radius_px = radius_px;
}

void CameraLanguageDirector::enter_boss_war() {
    if (!_initialized) return;
    _state = CameraState::BOSS_WAR;
    _boss_war_active = true;
    _boss_war_timer = _def.boss_war.zoom_in.duration;
    _focus_timer = _def.boss_war.focus_duration;  // 聚焦持续时间
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
    // 平滑插值回正常状态 (快速回归, 确保聚焦结束后立即归零)
    float lerp_t = dt * (_def.boss_war.lerp_speed * 5.0f);  // 5x 速度, 快速回归
    _current_fov_scale = lerp(_current_fov_scale, _target_fov_scale, lerp_t);
    _current_focus_offset = lerp_vec(_current_focus_offset, _target_focus_offset, lerp_t);
    // 强制归零 (避免浮点残留)
    if (vec_len(_current_focus_offset) < 0.1f) {
        _current_focus_offset = {0, 0};
    }
    if (_current_fov_scale > 0.999f && _current_fov_scale < 1.001f) {
        _current_fov_scale = 1.0f;
    }
}

void CameraLanguageDirector::update_boss_war(float dt, const Vector2& player_pos, const Vector2& boss_pos) {
    // 聚焦计时器: 到期后自动回归玩家
    _focus_timer -= dt;
    if (_focus_timer <= 0) {
        exit_boss_war();
        return;
    }
    
    // Boss 战期间: 镜头直接停在 Boss 身上 (100% 偏移)
    // mark_boss_room() 已预渲染整个 Boss 房间, 无需视野半径限制
    Vector2 to_boss = vec_sub(boss_pos, player_pos);
    
    // 100% 聚焦 Boss: 镜头直接到 Boss 位置
    Vector2 target_offset = to_boss;
    
    // 注意: 不限制在视野半径内, 因为 Boss 房间已预渲染
    // 镜头可以移动到 Boss 位置, 即使超出玩家视野
    
    _target_focus_offset = target_offset;
    
    // 插值到目标 FOV (提高速度, 更快跟随)
    float lerp_t = dt * (_def.boss_war.lerp_speed * 3.0f);  // 3x 速度
    _current_fov_scale = lerp(_current_fov_scale, _target_fov_scale, lerp_t);
    _current_focus_offset = lerp_vec(_current_focus_offset, _target_focus_offset, lerp_t);
    
    // Boss 战计时器
    if (_boss_war_timer > 0) {
        _boss_war_timer -= dt;
    }
}

void CameraLanguageDirector::update_kill_stun(float dt) {
    _kill_stun_timer -= dt;
    if (_kill_stun_timer <= 0) {
        _kill_stun_timer = 0;
        // Boss 战期间击杀: 回归 BOSS_WAR 而非 NORMAL
        if (_boss_war_active) {
            _state = CameraState::BOSS_WAR;
        } else {
            _state = CameraState::NORMAL;
            _target_fov_scale = 1.0f;
            _target_focus_offset = {0, 0};
        }
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
