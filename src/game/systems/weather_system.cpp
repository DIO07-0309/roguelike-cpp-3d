// WeatherSystem: 3D 天气粒子系统实现
#include "weather_system.h"
#include "core/logger.h"
#include <cstring>

namespace Game {

WeatherSystem& WeatherSystem::inst() {
    static WeatherSystem system;
    return system;
}

bool WeatherSystem::init(int pool_size) {
    if (_ready) return true;
    
    _particles.reserve(pool_size);
    _particles.resize(pool_size);
    _rng.seed(42);
    _dist = std::uniform_real_distribution<float>(0.0f, 1.0f);
    
    for (auto& p : _particles) {
        p.active = false;
        p.life = 0.0f;
    }
    
    _ready = true;
    LOG_INFO("WeatherSystem: 天气系统已初始化 (%d 粒子)", pool_size);
    return true;
}

void WeatherSystem::shutdown() {
    _particles.clear();
    _ready = false;
}

void WeatherSystem::set_weather(WeatherType type) {
    _current_weather = type;
    _spawn_timer = 0.0f;
    LOG_INFO("WeatherSystem: 天气切换为 %d", (int)type);
}

void WeatherSystem::set_weather_from_biome(const char* biome_id) {
    if (!biome_id) {
        set_weather(WeatherType::NONE);
        return;
    }
    
    // biome → 天气映射
    if (strcmp(biome_id, "ash_volcano") == 0) {
        set_weather(WeatherType::ASH);
    }
    else if (strcmp(biome_id, "void_abyss") == 0) {
        set_weather(WeatherType::SPORE);
    }
    else if (strcmp(biome_id, "ice_cavern") == 0) {
        set_weather(WeatherType::SNOW);
    }
    else if (strcmp(biome_id, "forest_grove") == 0) {
        set_weather(WeatherType::RAIN);
    }
    else {
        set_weather(WeatherType::DUST);
    }
}

void WeatherSystem::update(float dt) {
    if (!_ready || _current_weather == WeatherType::NONE) {
        _ground_alpha = 0.0f;
        return;
    }
    
    // 生成粒子
    spawn_particles(dt);
    
    // 更新粒子
    update_particles(dt);
    
    // 更新地面效果
    float target_alpha = 0.0f;
    switch (_current_weather) {
        case WeatherType::RAIN:    target_alpha = 0.3f; break;  // 潮湿
        case WeatherType::SNOW:    target_alpha = 0.5f; break;  // 积雪
        case WeatherType::ASH:     target_alpha = 0.4f; break;  // 灰烬层
        case WeatherType::DUST:    target_alpha = 0.2f; break;  // 灰尘
        case WeatherType::SPORE:   target_alpha = 0.35f; break; // 孢子腐蚀
        default: break;
    }
    _ground_alpha = _ground_alpha + (target_alpha * _ground_intensity - _ground_alpha) * dt * 2.0f;
}

void WeatherSystem::spawn_particles(float dt) {
    if (_current_weather == WeatherType::NONE) return;
    
    // 生成速率（每秒粒子数）
    float spawn_rate = 0.0f;
    switch (_current_weather) {
        case WeatherType::RAIN:    spawn_rate = 150.0f; break;
        case WeatherType::SNOW:    spawn_rate = 50.0f; break;
        case WeatherType::ASH:     spawn_rate = 80.0f; break;
        case WeatherType::DUST:    spawn_rate = 30.0f; break;
        case WeatherType::SPORE:   spawn_rate = 60.0f; break;
        default: break;
    }
    
    _spawn_timer += dt;
    float spawn_interval = 1.0f / spawn_rate;
    
    while (_spawn_timer >= spawn_interval) {
        _spawn_timer -= spawn_interval;
        spawn_single_particle();
    }
}

void WeatherSystem::spawn_single_particle() {
    // 找一个不活跃的粒子
    for (auto& p : _particles) {
        if (!p.active) {
            // 初始化粒子 - 相对于相机位置生成
            p.active = true;
            p.position = {
                (_dist(_rng) - 0.5f) * 300.0f,  // x: 相机左右 ±150
                150.0f + _dist(_rng) * 50.0f,  // y: 高处
                (_dist(_rng) - 0.5f) * 300.0f   // z: 相机前后 ±150
            };
            p.size = 1.0f;
            p.max_life = 3.0f;
            p.life = 0.0f;
            
            // 根据天气类型设置颜色和速度
            switch (_current_weather) {
                case WeatherType::RAIN:
                    p.color = Color{150, 200, 255, 220};  // 雨滴蓝
                    p.velocity = {0, -180, 0};  // 快速下落
                    p.size = 0.8f;
                    p.max_life = 1.5f;
                    break;
                    
                case WeatherType::SNOW:
                    p.color = Color{255, 255, 255, 230};  // 雪白
                    p.velocity = {0, -30, 0};  // 缓慢下落
                    p.size = 1.5f;
                    p.max_life = 5.0f;
                    break;
                    
                case WeatherType::ASH:
                    p.color = Color{80, 60, 50, 210};  // 灰烬棕
                    p.velocity = {0, -20, 0};  // 缓慢下落
                    p.size = 1.2f;
                    p.max_life = 4.0f;
                    break;
                    
                case WeatherType::DUST:
                    p.color = Color{180, 160, 140, 160};  // 灰尘灰
                    p.velocity = {0, -15, 0};  // 非常缓慢
                    p.size = 0.8f;
                    p.max_life = 6.0f;
                    break;
                    
                case WeatherType::SPORE:
                    p.color = Color{120, 200, 100, 210};  // 孢子绿
                    p.velocity = {0, -25, 0};  // 缓慢下落
                    p.size = 1.3f;
                    p.max_life = 4.5f;
                    break;
                    
                default: break;
            }
            
            // 添加随机横向漂移
            p.velocity.x += (_dist(_rng) - 0.5f) * 20.0f;
            p.velocity.z += (_dist(_rng) - 0.5f) * 20.0f;
            
            return;
        }
    }
}

void WeatherSystem::update_particles(float dt) {
    for (auto& p : _particles) {
        if (!p.active) continue;
        
        p.life += dt;
        if (p.life >= p.max_life) {
            p.active = false;
            continue;
        }
        
        // 更新位置
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.position.z += p.velocity.z * dt;
        
        // 雪和孢子添加摆动
        if (_current_weather == WeatherType::SNOW || _current_weather == WeatherType::SPORE) {
            p.position.x += sinf(p.life * 2.0f) * 10.0f * dt;
            p.position.z += cosf(p.life * 2.0f) * 10.0f * dt;
        }
        
        // 灰烬和灰尘添加旋转
        if (_current_weather == WeatherType::ASH || _current_weather == WeatherType::DUST) {
            p.velocity.x += (_dist(_rng) - 0.5f) * 5.0f;
            p.velocity.z += (_dist(_rng) - 0.5f) * 5.0f;
        }
        
        // 地面碰撞
        if (p.position.y < 0.0f) {
            p.active = false;
        }
    }
}

Color WeatherSystem::get_ground_tint() const {
    switch (_current_weather) {
        case WeatherType::RAIN:    return Color{100, 120, 140, 255};  // 潮湿蓝灰
        case WeatherType::SNOW:    return Color{220, 230, 240, 255};  // 积雪白
        case WeatherType::ASH:     return Color{60, 45, 35, 255};     // 灰烬暗棕
        case WeatherType::DUST:    return Color{140, 120, 100, 255};  // 灰尘灰
        case WeatherType::SPORE:   return Color{60, 100, 50, 255};    // 孢子毒绿
        default:                   return Color{255, 255, 255, 0};     // 无效果
    }
}

void WeatherSystem::set_ground_intensity(float intensity) {
    _ground_intensity = intensity;
}

} // namespace Game
