#pragma once
#include <string>
#include <unordered_map>
#include "raylib.h"
#include "bgm_engine.h"

class AudioServer {
public:
    void init();
    void close();
    void play_bgm(const std::string& name, float vol = 0.4f);
    void stop_bgm(float = 0.4f);
    void play_sfx(const std::string& name, float vol = 0.6f);
    void update(float dt);

    // Q3.1: --sim 静音模式 (跳过音频合成与播放)
    static bool g_muted;

private:
    BGMEngine _bgm;
    std::unordered_map<std::string, Sound> _sfx;
    // A7-T3: 音效路径 (T4 后补)
    std::string _sfx_path = "assets/sfx/";
};
