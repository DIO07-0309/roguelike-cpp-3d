#pragma once
// A14: 真 3D 网格资产 (JSON: 顶点+顶点色+三角面) — 纯数据层, 可无窗口单测
// 生成源: art_src/scripts/build_models.py (低模硬表面, 引擎坐标系, 1 单位 = 1 世界像素)
#include <optional>
#include <string>
#include <vector>

#include "raylib.h"

struct MeshAsset {
    std::vector<Vector3> positions;   // 每面 3 顶点 (面展开, flat/normal 源)
    std::vector<Vector3> normals;     // 每顶点法线 (JSON 带 normals → 平滑; 否则按面绕序算平面法线)
    std::vector<Color> colors;        // 每顶点色 (面内三点同色)
    int triangle_count() const { return static_cast<int>(positions.size() / 3); }
    float height() const;              // 包围盒 Y 跨度 (脚底贴地用)
};

// 读 JSON 网格: vertices=[x,y,z,r,g,b], faces=[i,j,k]
// 失败 (文件缺失/格式错/索引越界/退化面) → nullopt, err 给出原因
std::optional<MeshAsset> load_mesh_asset_file(const std::string& path, std::string& err);

// A14: 模型资产路径与存在性 (assets/models/<name>.json)
// 2D 层据此决定"是否让位给 3D 模型" (模型缺失 → 照旧画纸片)
std::string mesh_asset_path(const std::string& name);
bool mesh_asset_exists(const std::string& name);
