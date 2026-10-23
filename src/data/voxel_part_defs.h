#pragma once
// A13: 部件体素化参数 (voxel_parts.json) — 2D 部件贴图 → 3D 网格的雕刻规则
// 与 art_src/scripts/voxel_lib.py 的 P/INK_SHADE/hem 参数一一对应
#include <nlohmann/json.hpp>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct VoxelHemStep {
    int rows_from_bottom = 0;   // 距下摆行数阈值 (含)
    float add_px = 0.f;         // 加成厚度 (贴图像素)
};

struct VoxelPartDef {
    float base = 4.f;      // 边缘厚度 (贴图像素)
    float growth = 0.5f;   // 每远离边缘 1px 增量
    float cap = 10.f;      // 厚度上限
    float ink_px = 6.f;    // 墨线像素固定厚度
    float y_offset = 0.f;  // 深度挂载偏移 (+ 朝角色背面), 贴图像素
    std::vector<VoxelHemStep> hem;   // 下摆加厚 (按声明序, 首个命中生效)
};

struct VoxelConfig {
    float ink_max_luminance = 0.25f;   // 亮度低于此 = 墨线像素
    float alpha_min = 0.5f;            // 低于此 alpha = 透明
    std::map<std::string, VoxelPartDef> parts;   // key = 部件槽 (arm/leg/...)
    // A13-M2: 按 parts_key (skeleton 名, 如 "mon_orc") 的槽参数覆盖 — 观感特例专用;
    // 未覆盖字段继承默认槽值 (解析时合并)
    std::map<std::string, std::map<std::string, VoxelPartDef>> overrides;

    const VoxelPartDef* find(const std::string& slot) const {
        auto it = parts.find(slot);
        return it == parts.end() ? nullptr : &it->second;
    }

    // A13-M2: 覆盖优先查找 (parts_key 无覆盖 → 退回默认槽)
    const VoxelPartDef* find_for(const std::string& parts_key, const std::string& slot) const {
        auto actor = overrides.find(parts_key);
        if (actor != overrides.end()) {
            auto over = actor->second.find(slot);
            if (over != actor->second.end()) return &over->second;
        }
        return find(slot);
    }
};

// 从 "assets/sprites/player_part_arm.png" 提取槽名 "arm"; 无 "_part_" 返回空串
std::string voxel_slot_from_path(const std::string& path);

// A13-M2: "resources/animations/mon_orc_skeleton.json" → "mon_orc"; 无 "_skeleton.json" 后缀返回空串
std::string voxel_parts_key_from_skeleton(const std::string& skeleton_path);

std::optional<VoxelConfig> parse_voxel_config(const nlohmann::json& j, std::string& err);
std::optional<VoxelConfig> load_voxel_parts_file(const std::string& path, std::string& err);
