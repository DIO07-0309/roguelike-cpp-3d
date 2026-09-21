// WeatherSystem: 3D 天气粒子系统
// 支持 5 种天气类型：雨/雪/灰烬/灰尘/孢子
#pragma once

#include <vector>
#include <random>
#include "raylib.h"

namespace Game {

// 天气类型枚举
enum class WeatherType {
    NONE = 0,
    RAIN = 1,        // 雨
    SNOW = 2,        // 雪
    ASH = 3,         // 灰烬
    DUST = 4,        // 灰尘
    SPORE = 5        // 孢子
};

// 天气粒子
struct WeatherParticle {
    Vector3 position;
    Vector3 velocity;
    float life;
    float max_life;
    float size;
    Color color;
    bool active = false;
};

// 天气系统
class WeatherSystem {
public:
    static WeatherSystem& inst();
    
    // 初始化/关闭
    bool init(int pool_size = 300);
    void shutdown();
    
    // 设置天气类型
    void set_weather(WeatherType type);
    WeatherType get_weather() const { return _current_weather; }
    
    // 根据 biome 自动设置天气
    void set_weather_from_biome(const char* biome_id);
    
    // 更新天气
    void update(float dt);
    
    // 获取粒子列表（用于渲染）
    const std::vector<WeatherParticle>& get_particles() const { return _particles; }
    
    // 获取当前天气颜色（用于地面效果）
    Color get_ground_tint() const;
    float get_ground_alpha() const { return _ground_alpha; }
    
    // 设置地面效果强度
    void set_ground_intensity(float intensity);
    
private:
    WeatherSystem() = default;
    ~WeatherSystem() = default;
    
    void spawn_particles(float dt);
    void spawn_single_particle();
    void update_particles(float dt);
    
    std::vector<WeatherParticle> _particles;
    WeatherType _current_weather = WeatherType::NONE;
    float _spawn_timer = 0.0f;
    float _ground_alpha = 0.0f;
    float _ground_intensity = 1.0f;
    
    std::mt19937 _rng;
    std::uniform_real_distribution<float> _dist;
    
    bool _ready = false;
};

} // namespace Game
