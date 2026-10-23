#pragma once
// A13-M2: 怪物体素网格共享库 — 按 (parts_key, slot, pivot) 懒建缓存, 同类怪多实例借用同一份
// 生命周期归持有者 (AvatarDirector); 借用方 (SkeletonAvatar) 不释放网格数据
#include <map>
#include <optional>
#include <string>
#include "raylib.h"
#include "data/voxel_part_defs.h"

class VoxelMeshLibrary {
public:
    VoxelMeshLibrary() = default;
    ~VoxelMeshLibrary();
    VoxelMeshLibrary(const VoxelMeshLibrary&) = delete;
    VoxelMeshLibrary& operator=(const VoxelMeshLibrary&) = delete;

    // 命中缓存直接返回; 未命中加载贴图建网格。失败 (贴图/参数缺失) 也入缓存为空网格, 不重试
    Mesh get(const std::string& parts_key, const std::string& slot,
             const std::string& file, float pivot_x, float pivot_y);
    void clear();

private:
    bool _ensure_config();

    std::optional<VoxelConfig> _config;
    bool _config_loaded = false;
    std::map<std::string, Mesh> _cache;   // key = parts_key/slot@pivot
};
