#pragma once
// A13: 部件体素化 — 2D 部件贴图 → 3D 网格 (G12 骨骼纸偶的 3D 升级, 渲染层专属)
// 规则与 art_src/scripts/voxel_lib.py 严格一致: 边缘距离深度场 + 墨线薄壁 + 下摆加厚
#include "raylib.h"
#include "data/voxel_part_defs.h"
#include <string>
#include <vector>

namespace voxel {

extern bool g_voxel_avatar;   // A13: 体素化身开关 (默认开; --no-voxel-avatar 关 = 纸偶回退)

struct GridData {
    int width = 0;
    int height = 0;
    std::vector<int> cell;         // 调色板索引; -1 = 透明; 行 0 = 贴图顶部
    std::vector<Color> palette;
    std::vector<bool> is_ink;

    int at(int x, int y) const { return cell[(size_t)y * width + x]; }
};

struct MeshArrays {
    std::vector<float> vertices;          // xyz (贴图像素单位, pivot 为原点)
    std::vector<unsigned char> colors;    // rgba
    std::vector<unsigned short> indices;
};

// 纯函数层 (无 GL 依赖, 单测直接覆盖)
GridData extract_grid(const unsigned char* pixels, int width, int height,
                      const VoxelConfig& config);
std::vector<float> compute_depths(const GridData& grid, const VoxelPartDef& def);
MeshArrays build_mesh_arrays(const GridData& grid, const std::vector<float>& depths,
                             const VoxelPartDef& def, float pivot_x, float pivot_y);

// GL 层
Mesh upload_mesh(const MeshArrays& arrays);
Mesh build_part_mesh(const Image& image, const VoxelConfig& config,
                     const std::string& slot, float pivot_x, float pivot_y);
// A13-M2: 显式参数版 — 调用方已解析覆盖 (find_for), 供共享网格库走 per-actor 参数
Mesh build_part_mesh(const Image& image, const VoxelConfig& config, const VoxelPartDef& def,
                     float pivot_x, float pivot_y);

} // namespace voxel
