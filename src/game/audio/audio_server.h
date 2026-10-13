#pragma once
#include <string>
#include <unordered_map>
#include <vector>
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

    // G48: init 的子步骤 (需访问 _sfx, 故为私有静态 + 传 map)
    static void _load_sfx_from_paths(std::unordered_map<std::string, Sound>& sfx,
                                     const std::string& key,
                                     const std::vector<const char*>& paths);
    static int _load_kenney_sfx(std::unordered_map<std::string, Sound>& sfx,
                                const std::string& dir);
};
