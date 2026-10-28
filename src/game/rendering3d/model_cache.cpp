// A14: 真 3D 模型缓存实现 — JSON 读入 → raylib Mesh (顶点色) → GPU 上传
#include "game/rendering3d/model_cache.h"

#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/logger.h"
#include "data/mesh_asset.h"

namespace {

const char* kModelDir = "assets/models/";

// A14: CPU 侧缓冲 (上传后由 raylib 接管释放)
struct MeshBuffers {
    float* vertices = nullptr;
    float* normals = nullptr;
    unsigned char* colors = nullptr;
    unsigned short* indices = nullptr;
};

MeshBuffers alloc_buffers(int vertex_count, int face_count) {
    MeshBuffers buffers;
    buffers.vertices = static_cast<float*>(MemAlloc(sizeof(float) * 3 * vertex_count));
    buffers.normals = static_cast<float*>(MemAlloc(sizeof(float) * 3 * vertex_count));
    buffers.colors = static_cast<unsigned char*>(MemAlloc(sizeof(unsigned char) * 4 * vertex_count));
    buffers.indices = static_cast<unsigned short*>(MemAlloc(sizeof(unsigned short) * 3 * face_count));
    return buffers;
}

void fill_mesh_data(const MeshAsset& asset, MeshBuffers& buffers, int face_count) {
    const int vertex_count = static_cast<int>(asset.positions.size());
    for (int i = 0; i < vertex_count; ++i) {
        buffers.vertices[i * 3 + 0] = asset.positions[i].x;
        buffers.vertices[i * 3 + 1] = asset.positions[i].y;
        buffers.vertices[i * 3 + 2] = asset.positions[i].z;
        buffers.normals[i * 3 + 0] = asset.normals[i].x;
        buffers.normals[i * 3 + 1] = asset.normals[i].y;
        buffers.normals[i * 3 + 2] = asset.normals[i].z;
        buffers.colors[i * 4 + 0] = asset.colors[i].r;
        buffers.colors[i * 4 + 1] = asset.colors[i].g;
        buffers.colors[i * 4 + 2] = asset.colors[i].b;
        buffers.colors[i * 4 + 3] = asset.colors[i].a;
    }
    // 生成器按面拆分顶点 (每面 3 顶点), 故索引即 3f/3f+1/3f+2
    for (int f = 0; f < face_count; ++f) {
        buffers.indices[f * 3 + 0] = static_cast<unsigned short>(f * 3 + 0);
        buffers.indices[f * 3 + 1] = static_cast<unsigned short>(f * 3 + 1);
        buffers.indices[f * 3 + 2] = static_cast<unsigned short>(f * 3 + 2);
    }
}

Mesh build_gpu_mesh(const MeshAsset& asset, int& triangle_count) {
    Mesh mesh{};
    const int vertex_count = static_cast<int>(asset.positions.size());
    const int face_count = static_cast<int>(asset.normals.size() / 3);
    triangle_count = face_count;
    if (vertex_count <= 0 || face_count <= 0) return mesh;
    MeshBuffers buffers = alloc_buffers(vertex_count, face_count);
    fill_mesh_data(asset, buffers, face_count);
    mesh.vertexCount = vertex_count;
    mesh.triangleCount = face_count;
    mesh.vertices = buffers.vertices;
    mesh.normals = buffers.normals;
    mesh.colors = buffers.colors;
    mesh.indices = buffers.indices;
    UploadMesh(&mesh, false);
    return mesh;
}

}  // namespace

// A16-Player: 公开入口 — 复用上方匿名命名空间的 build_gpu_mesh
Mesh build_gpu_mesh_asset(const MeshAsset& asset) {
    int triangle_count = 0;
    return build_gpu_mesh(asset, triangle_count);
}

std::shared_ptr<ModelAsset> ModelCache::get(const std::string& name) {
    auto found = _cache.find(name);
    if (found != _cache.end()) return found->second;

    std::string err;
    const std::string path = std::string(kModelDir) + name + ".json";
    auto asset = load_mesh_asset_file(path, err);
    if (!asset) {
        LOG_WARN("A14: model load failed (%s), fallback to paper sprite", err.c_str());
        _cache[name] = nullptr;
        return nullptr;
    }
    auto result = std::make_shared<ModelAsset>();
    result->height = asset->height();
    result->mesh = build_gpu_mesh(*asset, result->triangle_count);
    if (result->mesh.vertexCount <= 0) {
        LOG_WARN("A14: model empty (%s), fallback to paper sprite", path.c_str());
        _cache[name] = nullptr;
        return nullptr;
    }
    _cache[name] = result;
    return result;
}

void ModelCache::clear() {
    for (auto& item : _cache)
        if (item.second && item.second->mesh.vertexCount > 0) UnloadMesh(item.second->mesh);
    _cache.clear();
}
