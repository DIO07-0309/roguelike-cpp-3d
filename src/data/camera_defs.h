#pragma once
// A6-T1: 摄像机语言 JSON 数据定义加载器 (spec: 2026-09-20-a6-camera-language-design.md §3)
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

struct ZoomDef {
    float fov_scale = 1.0f;   // FOV 缩放 (0.75 = 拉近, 1.0 = 原始)
    float duration = 0.0f;    // 插值时长 (秒)
};

struct BossWarDef {
    ZoomDef zoom_in;          // Boss 出场时拉近
    ZoomDef zoom_out;         // 玩家靠近时拉远
    float lerp_speed = 2.0f;  // 插值速度
};

struct KillStunDef {
    float duration = 0.08f;           // hit-stop 时长 (秒)
    float shake_amplitude = 3.0f;     // 震动幅度 (像素)
    float shake_frequency = 20.0f;    // 震动频率 (Hz)
};

struct CameraDef {
    BossWarDef boss_war;
    KillStunDef kill_stun;
};

std::optional<CameraDef> load_camera_file(const std::string& path, std::string& err);
