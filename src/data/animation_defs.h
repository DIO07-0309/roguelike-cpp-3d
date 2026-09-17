#pragma once
// A5-T1: 骨骼/动画 JSON 数据定义加载器 (spec: 2026-09-17-a5-skeletal-animation-design.md §4)
#include <nlohmann/json.hpp>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct BoneDef {
    std::string name;
    int parent = -1;          // 声明序约束: 父必须先于子 (链合成前提, parse 时校验)
    float x = 0, y = 0;       // bind 局部偏移, Y 向上
};

struct PartDef {
    int bone = -1;
    std::string file;
    float dx = 0, dy = 0;
    float pivot_x = 0, pivot_y = 0;   // 贴图内锚点, 左下原点像素坐标
};

struct SkeletonDef {
    float pixels_per_unit = 0.5f;
    float anchor_x = 0, anchor_y = 0; // 骨骼空间脚底原点
    std::vector<BoneDef> bones;
    std::vector<PartDef> parts;       // 数组序 = 绘制序 (后→前)
};

struct KeyDef {
    float t = 0;
    float x = 0, y = 0;      // 绝对局部值 (非增量), 缺省 = bind
    float rot = 0;
    float sx = 1, sy = 1;
};

struct TrackDef {
    int bone = -1;
    std::vector<KeyDef> keys;
};

struct AnimClipDef {
    bool loop = false;
    float dur = 0;
    std::vector<TrackDef> tracks;
};

struct AnimSetDef {
    std::map<std::string, AnimClipDef> clips;
};

std::optional<SkeletonDef> parse_skeleton(const nlohmann::json& j, std::string& err);
std::optional<AnimSetDef> parse_anim(const nlohmann::json& j, const SkeletonDef& sk, std::string& err);
std::optional<SkeletonDef> load_skeleton_file(const std::string& path, std::string& err);
std::optional<AnimSetDef> load_anim_file(const std::string& path, const SkeletonDef& sk, std::string& err);
