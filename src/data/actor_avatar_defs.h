#pragma once
// A6-S1: actor_avatars.json — 实体骨骼皮肤映射 (数据驱动白名单; 缺省/空 = 全回退旧 sprite)
#include <nlohmann/json.hpp>
#include <map>
#include <optional>
#include <string>

struct ActorAvatarDef {
    std::string skeleton;   // 骨骼 JSON 路径 (相对工作目录)
    std::string anim;       // 动画 JSON 路径 (相对工作目录)
};

std::optional<std::map<std::string, ActorAvatarDef>> parse_actor_avatars(
    const nlohmann::json& j, std::string& err);

// 文件缺失/actors 空 → 空 map (全回退, 非错误); JSON 解析/结构错误 → nullopt + err
std::optional<std::map<std::string, ActorAvatarDef>> load_actor_avatars_file(
    const std::string& path, std::string& err);
