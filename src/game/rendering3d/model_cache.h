#pragma once
// A14: 真 3D 模型共享缓存 — 按名懒加载 assets/models/<name>.json 并上传 GPU
// 缺失/损坏 → 返回空指针 (调用方回退纸片); 失败结果也缓存, 不重复读盘
#include <map>
#include <memory>
#include <string>

#include "raylib.h"

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
