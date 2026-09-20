#pragma once
#include <string>
#include <unordered_map>
#include "raylib.h"

class BGMEngine {
public:
    void init();
    void close();
    void play(const std::string& name, float vol = 0.4f);
    void stop();
    void update(float dt);  // Q4.2: 检测播放结束并循环重播
    bool is_initialized() const { return !_cache.empty(); }

private:
    Sound _compile_bgm(const std::string& name);
    std::unordered_map<std::string, Sound> _cache;
    std::string _current;   // 当前播放曲目 (stop 修复)
    float _volume = 0.4f;
    bool _playing = false;
};
