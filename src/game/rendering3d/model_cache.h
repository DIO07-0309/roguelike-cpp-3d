#pragma once
// A14: 真 3D 模型共享缓存 — 按名懒加载 assets/models/<name>.json 并上传 GPU
// 缺失/损坏 → 返回空指针 (调用方回退纸片); 失败结果也缓存, 不重复读盘
#include <map>
#include <memory>
#include <string>

#include "raylib.h"

struct MeshAsset;

// A16-Player: 从 MeshAsset 构建可绘制的 raylib Mesh (顶点色, 面展开, GPU 上传)
// 供玩家低模部件等需要独立 raylib Mesh 的调用方使用 (模型缓存内部也复用此逻辑)
Mesh build_gpu_mesh_asset(const MeshAsset& asset);

struct ModelAsset {
    Mesh mesh{};          // 已上传 GPU (vertexCount>0 = 有效)
    float height = 0.0f;  // 包围盒 Y 跨度 (脚底贴地用)
    int triangle_count = 0;
};

class ModelCache {
public:
    // name = "npc_1" / "chest" / "altar" / "spring"
    std::shared_ptr<ModelAsset> get(const std::string& name);
    void clear();

private:
    std::map<std::string, std::shared_ptr<ModelAsset>> _cache;
};
