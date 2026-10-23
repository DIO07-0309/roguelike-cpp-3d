// A13-M2: 怪物体素网格共享库实现 — 懒建 + 缓存 (每类每槽只建一次, 多实例借用)
#include "game/animation/voxel_mesh_library.h"

#include "core/logger.h"
#include "game/rendering3d/part_voxelizer.h"

namespace {

std::string cache_key(const std::string& parts_key, const std::string& slot,
                      float pivot_x, float pivot_y) {
    return parts_key + "/" + slot + "@" + std::to_string((int)pivot_x) + "," +
           std::to_string((int)pivot_y);
}

} // namespace

VoxelMeshLibrary::~VoxelMeshLibrary() { clear(); }

void VoxelMeshLibrary::clear() {
    for (auto& item : _cache)
        if (item.second.vertexCount > 0) UnloadMesh(item.second);
    _cache.clear();
}

bool VoxelMeshLibrary::_ensure_config() {
    if (_config_loaded) return _config.has_value();
    _config_loaded = true;
    std::string err;
    _config = load_voxel_parts_file("resources/voxel_parts.json", err);
    if (!_config)
        LOG_WARN("A13-M2: voxel_parts.json invalid, monsters keep paperdoll (%s)", err.c_str());
    return _config.has_value();
}

Mesh VoxelMeshLibrary::get(const std::string& parts_key, const std::string& slot,
                           const std::string& file, float pivot_x, float pivot_y) {
    const std::string key = cache_key(parts_key, slot, pivot_x, pivot_y);
    auto found = _cache.find(key);
    if (found != _cache.end()) return found->second;

    Mesh mesh{};
    const VoxelPartDef* def = _ensure_config() ? _config->find_for(parts_key, slot) : nullptr;
    if (def) {
        Image image = LoadImage(file.c_str());
        if (image.width > 0 && image.height > 0) {
            mesh = voxel::build_part_mesh(image, *_config, *def, pivot_x, pivot_y);
            UnloadImage(image);
        }
    }
    _cache[key] = mesh;   // 失败也缓存 (空网格 = 该件回退纸偶)
    return mesh;
}
