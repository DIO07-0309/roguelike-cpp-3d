// A13: 部件体素化实现 — 规则与 art_src/scripts/voxel_lib.py 逐条对应
#include "game/rendering3d/part_voxelizer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace voxel {

bool g_voxel_avatar = true;    // A13: 默认接入游戏 (--no-voxel-avatar 关闭 = 纸偶回退)

namespace {

enum FaceKey { FACE_FRONT, FACE_BACK, FACE_LEFT, FACE_RIGHT, FACE_TOP, FACE_BOTTOM };

const float kShade[6] = {1.00f, 0.40f, 0.60f, 0.60f, 1.75f, 0.28f};
const float kInkShade[6] = {1.00f, 0.80f, 1.08f, 1.08f, 1.25f, 0.60f};

float srgb_to_linear(float c) {
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

float linear_to_srgb(float c) {
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.f / 2.4f) - 0.055f;
}

unsigned char shaded_byte(unsigned char v, float shade) {
    const float lin = std::min(1.f, srgb_to_linear((float)v / 255.f) * shade);
    return (unsigned char)std::lround(linear_to_srgb(lin) * 255.f);
}

float luminance8(const Color& c) {
    return (0.2126f * (float)c.r + 0.7152f * (float)c.g + 0.0722f * (float)c.b) / 255.f;
}

struct CellMeta {
    int palette = -1;
    float y0 = 0.f;
    float y1 = 0.f;
};

bool same_meta(const CellMeta* a, const CellMeta* b) {
    return a && b && a->palette == b->palette && a->y0 == b->y0 && a->y1 == b->y1;
}

const CellMeta* meta_at(const std::vector<CellMeta>& meta, int width, int x, int y) {
    if (x < 0 || y < 0 || x >= width) return nullptr;
    const size_t index = (size_t)y * width + x;
    if (index >= meta.size() || meta[index].palette < 0) return nullptr;
    return &meta[index];
}

int edge_dist(const GridData& grid, int x, int y) {
    int xl = x, xr = x, yt = y, yb = y;
    while (xl > 0 && grid.at(xl - 1, y) >= 0) --xl;
    while (xr < grid.width - 1 && grid.at(xr + 1, y) >= 0) ++xr;
    while (yt > 0 && grid.at(x, yt - 1) >= 0) --yt;
    while (yb < grid.height - 1 && grid.at(x, yb + 1) >= 0) ++yb;
    return std::min(std::min(x - xl, xr - x), std::min(y - yt, yb - y));
}

void push_quad(MeshArrays& out, const Vector3 q[4], const Color& c) {
    const unsigned short base = (unsigned short)(out.vertices.size() / 3);
    for (int i = 0; i < 4; ++i) {
        out.vertices.push_back(q[i].x);
        out.vertices.push_back(q[i].y);
        out.vertices.push_back(q[i].z);
        out.colors.push_back(c.r);
        out.colors.push_back(c.g);
        out.colors.push_back(c.b);
        out.colors.push_back(c.a);
    }
    static const unsigned short kCornerIndex[6] = {0, 1, 2, 0, 2, 3};
    for (unsigned short corner : kCornerIndex) out.indices.push_back(base + corner);
}

Color face_color(const GridData& grid, int palette, int face) {
    const float shade = grid.is_ink[palette] ? kInkShade[face] : kShade[face];
    const Color& p = grid.palette[palette];
    return {shaded_byte(p.r, shade), shaded_byte(p.g, shade), shaded_byte(p.b, shade), 255};
}

void emit_face(MeshArrays& out, const GridData& grid, int palette, int face,
               const Vector3 q[4]) {
    push_quad(out, q, face_color(grid, palette, face));
}

void emit_cell(MeshArrays& out, const GridData& grid, const std::vector<CellMeta>& meta,
               int x, int y, float pivot_x, float pivot_y) {
    const CellMeta* self = meta_at(meta, grid.width, x, y);
    if (!self) return;
    const float xa = (float)x - pivot_x, xb = xa + 1.f;
    const float zb = (float)(grid.height - y) - pivot_y, za = zb - 1.f;
    const float y0 = self->y0, y1 = self->y1;
    const Vector3 front[4] = {{xa, y0, za}, {xb, y0, za}, {xb, y0, zb}, {xa, y0, zb}};
    const Vector3 back[4] = {{xa, y1, za}, {xa, y1, zb}, {xb, y1, zb}, {xb, y1, za}};
    emit_face(out, grid, self->palette, FACE_FRONT, front);
    emit_face(out, grid, self->palette, FACE_BACK, back);
    const CellMeta* left = meta_at(meta, grid.width, x - 1, y);
    const CellMeta* right = meta_at(meta, grid.width, x + 1, y);
    const CellMeta* up = meta_at(meta, grid.width, x, y - 1);
    const CellMeta* down = meta_at(meta, grid.width, x, y + 1);
    if (!same_meta(left, self)) {
        const Vector3 q[4] = {{xa, y0, za}, {xa, y0, zb}, {xa, y1, zb}, {xa, y1, za}};
        emit_face(out, grid, self->palette, FACE_LEFT, q);
    }
    if (!same_meta(right, self)) {
        const Vector3 q[4] = {{xb, y0, za}, {xb, y1, za}, {xb, y1, zb}, {xb, y0, zb}};
        emit_face(out, grid, self->palette, FACE_RIGHT, q);
    }
    if (!same_meta(up, self)) {
        const Vector3 q[4] = {{xa, y0, zb}, {xb, y0, zb}, {xb, y1, zb}, {xa, y1, zb}};
        emit_face(out, grid, self->palette, FACE_TOP, q);
    }
    if (!same_meta(down, self)) {
        const Vector3 q[4] = {{xa, y0, za}, {xa, y1, za}, {xb, y1, za}, {xb, y0, za}};
        emit_face(out, grid, self->palette, FACE_BOTTOM, q);
    }
}

} // namespace

GridData extract_grid(const unsigned char* pixels, int width, int height,
                      const VoxelConfig& config) {
    GridData grid;
    grid.width = width;
    grid.height = height;
    if (width <= 0 || height <= 0) return grid;
    grid.cell.assign((size_t)width * height, -1);
    if (!pixels) return grid;
    const unsigned char alpha_min =
        (unsigned char)std::lround(std::min(1.f, config.alpha_min) * 255.f);
    std::unordered_map<unsigned int, int> color_index;
    for (int y = 0; y < height; ++y) {
        const int src_row = y;   // raylib/stb 图像数据 = 顶部行在前 → 行 0 即贴图顶部
        for (int x = 0; x < width; ++x) {
            const unsigned char* p = &pixels[((size_t)src_row * width + x) * 4];
            if (p[3] < alpha_min) continue;
            const unsigned int key =
                ((unsigned int)p[0] << 16) | ((unsigned int)p[1] << 8) | p[2];
            auto it = color_index.find(key);
            if (it == color_index.end()) {
                const Color c = {p[0], p[1], p[2], 255};
                it = color_index.emplace(key, (int)grid.palette.size()).first;
                grid.palette.push_back(c);
                grid.is_ink.push_back(luminance8(c) < config.ink_max_luminance);
            }
            grid.cell[(size_t)y * width + x] = it->second;
        }
    }
    return grid;
}

std::vector<float> compute_depths(const GridData& grid, const VoxelPartDef& def) {
    std::vector<float> depths(grid.cell.size(), -1.f);
    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            const int palette = grid.at(x, y);
            if (palette < 0) continue;
            float dep = 0.f;
            if (grid.is_ink[palette]) {
                dep = def.ink_px;
            } else {
                dep = def.base + def.growth * edge_dist(grid, x, y);
                const int from_bottom = grid.height - 1 - y;
                for (const auto& step : def.hem) {
                    if (from_bottom <= step.rows_from_bottom) { dep += step.add_px; break; }
                }
                dep = std::min(dep, def.cap);
            }
            depths[(size_t)y * grid.width + x] = dep;
        }
    }
    return depths;
}

MeshArrays build_mesh_arrays(const GridData& grid, const std::vector<float>& depths,
                             const VoxelPartDef& def, float pivot_x, float pivot_y) {
    MeshArrays out;
    std::vector<CellMeta> meta(grid.cell.size());
    for (size_t i = 0; i < grid.cell.size(); ++i) {
        if (grid.cell[i] < 0) continue;
        const float dep = std::max(0.f, depths[i]);
        meta[i].palette = grid.cell[i];
        meta[i].y0 = -dep * 0.5f + def.y_offset;
        meta[i].y1 = dep * 0.5f + def.y_offset;
    }
    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            emit_cell(out, grid, meta, x, y, pivot_x, pivot_y);
        }
    }
    return out;
}

Mesh upload_mesh(const MeshArrays& arrays) {
    Mesh mesh{};
    if (arrays.vertices.empty() || arrays.indices.empty()) return mesh;
    mesh.vertexCount = (int)(arrays.vertices.size() / 3);
    mesh.triangleCount = (int)(arrays.indices.size() / 3);
    const size_t vbytes = arrays.vertices.size() * sizeof(float);
    const size_t cbytes = arrays.colors.size() * sizeof(unsigned char);
    const size_t ibytes = arrays.indices.size() * sizeof(unsigned short);
    mesh.vertices = (float*)MemAlloc(vbytes);
    mesh.colors = (unsigned char*)MemAlloc(cbytes);
    mesh.indices = (unsigned short*)MemAlloc(ibytes);
    std::memcpy(mesh.vertices, arrays.vertices.data(), vbytes);
    std::memcpy(mesh.colors, arrays.colors.data(), cbytes);
    std::memcpy(mesh.indices, arrays.indices.data(), ibytes);
    UploadMesh(&mesh, false);
    return mesh;
}

Mesh build_part_mesh(const Image& image, const VoxelConfig& config,
                     const std::string& slot, float pivot_x, float pivot_y) {
    const VoxelPartDef* def = config.find(slot);
    if (!def) return Mesh{};
    return build_part_mesh(image, config, *def, pivot_x, pivot_y);
}

Mesh build_part_mesh(const Image& image, const VoxelConfig& config, const VoxelPartDef& def,
                     float pivot_x, float pivot_y) {
    if (!image.data || image.width <= 0 || image.height <= 0) return Mesh{};
    const GridData grid = extract_grid(
        (const unsigned char*)image.data, image.width, image.height, config);
    const std::vector<float> depths = compute_depths(grid, def);
    return upload_mesh(build_mesh_arrays(grid, depths, def, pivot_x, pivot_y));
}

} // namespace voxel
