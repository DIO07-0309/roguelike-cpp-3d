// A14: 真 3D 网格资产加载 (纯数据层 — 不碰 GL, 可在无窗口单测里跑)
#include "data/mesh_asset.h"

#include <cmath>
#include <fstream>
#include <sstream>

#include "nlohmann/json.hpp"

namespace {

bool read_json_file(const std::string& path, nlohmann::json& out, std::string& err) {
    std::ifstream in(path);
    if (!in.is_open()) { err = "cannot open: " + path; return false; }
    std::stringstream buf;
    buf << in.rdbuf();
    try {
        out = nlohmann::json::parse(buf.str());
    } catch (const nlohmann::json::exception& e) {
        err = std::string("json parse: ") + e.what() + " in " + path;
        return false;
    }
    return true;
}

bool parse_vertices(const nlohmann::json& arr, std::vector<Vector3>& positions,
                    std::vector<Color>& colors, std::string& err) {
    if (!arr.is_array() || arr.empty()) { err = "mesh_asset: vertices must be non-empty array"; return false; }
    for (const auto& item : arr) {
        if (!item.is_array() || item.size() < 6) { err = "mesh_asset: vertex must be [x,y,z,r,g,b]"; return false; }
        for (int i = 0; i < 6; ++i) {
            if (!item[i].is_number()) { err = "mesh_asset: vertex component must be number"; return false; }
        }
        positions.push_back({item[0].get<float>(), item[1].get<float>(), item[2].get<float>()});
        colors.push_back({(unsigned char)item[3].get<int>(), (unsigned char)item[4].get<int>(),
                          (unsigned char)item[5].get<int>(), 255});
    }
    return true;
}

bool parse_source_normals(const nlohmann::json& arr, const std::vector<Vector3>& source_positions,
                          std::vector<Vector3>& out, std::string& err) {
    if (!arr.is_array() || static_cast<int>(arr.size()) != static_cast<int>(source_positions.size())) {
        err = "mesh_asset: normals count must match vertices";
        return false;
    }
    for (const auto& item : arr) {
        if (!item.is_array() || item.size() < 3) { err = "mesh_asset: normal must be [x,y,z]"; return false; }
        out.push_back({item[0].get<float>(), item[1].get<float>(), item[2].get<float>()});
    }
    return true;
}

// 面展开: 每面独占 3 顶点 (flat shading + 连续索引, 上传层可直接用 3f/3f+1/3f+2)
// source_normals 非空 → 用源顶点法线 (平滑着色); 否则按面绕序算平面法线
bool parse_faces(const nlohmann::json& arr, const std::vector<Vector3>& source_positions,
                 const std::vector<Color>& source_colors,
                 const std::vector<Vector3>& source_normals,
                 std::vector<Vector3>& positions, std::vector<Vector3>& normals,
                 std::vector<Color>& colors, std::string& err) {
    if (!arr.is_array() || arr.empty()) { err = "mesh_asset: faces must be non-empty array"; return false; }
    for (const auto& item : arr) {
        if (!item.is_array() || item.size() != 3) { err = "mesh_asset: face must be [i,j,k]"; return false; }
        for (int i = 0; i < 3; ++i) {
            if (!item[i].is_number_integer()) { err = "mesh_asset: face index must be integer"; return false; }
            const int index = item[i].get<int>();
            if (index < 0 || index >= static_cast<int>(source_positions.size())) {
                err = "mesh_asset: face index out of range: " + std::to_string(index);
                return false;
            }
        }
        const int base[3] = {item[0].get<int>(), item[1].get<int>(), item[2].get<int>()};
        Vector3 normal;
        if (!source_normals.empty()) {
            normal = source_normals[base[0]];
        } else {
            const Vector3 a = source_positions[base[0]];
            const Vector3 b = source_positions[base[1]];
            const Vector3 c = source_positions[base[2]];
            const Vector3 edge1 = {b.x - a.x, b.y - a.y, b.z - a.z};
            const Vector3 edge2 = {c.x - a.x, c.y - a.y, c.z - a.z};
            normal = {edge1.y * edge2.z - edge1.z * edge2.y,
                      edge1.z * edge2.x - edge1.x * edge2.z,
                      edge1.x * edge2.y - edge1.y * edge2.x};
            const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (length < 1e-6f) continue;   // 退化面 (零面积): 跳过, 不废掉整个模型
            normal.x /= length; normal.y /= length; normal.z /= length;
        }
        for (int i = 0; i < 3; ++i) {
            positions.push_back(source_positions[base[i]]);
            normals.push_back(normal);
            colors.push_back(source_colors[base[i]]);
        }
    }
    return true;
}

}  // namespace

float MeshAsset::height() const {
    if (positions.empty()) return 0.0f;
    float low = positions[0].y, high = positions[0].y;
    for (const auto& p : positions) {
        if (p.y < low) low = p.y;
        if (p.y > high) high = p.y;
    }
    return high - low;
}

std::string mesh_asset_path(const std::string& name) {
    return "assets/models/" + name + ".json";
}

bool mesh_asset_exists(const std::string& name) {
    std::ifstream in(mesh_asset_path(name));
    return in.is_open();
}

std::optional<MeshAsset> load_mesh_asset_file(const std::string& path, std::string& err) {
    nlohmann::json root;
    if (!read_json_file(path, root, err)) return std::nullopt;
    if (!root.is_object() || !root.contains("vertices") || !root.contains("faces")) {
        err = "mesh_asset: need vertices and faces in " + path;
        return std::nullopt;
    }
    MeshAsset asset;
    std::vector<Vector3> source_positions;
    std::vector<Color> source_colors;
    if (!parse_vertices(root["vertices"], source_positions, source_colors, err)) return std::nullopt;
    std::vector<Vector3> source_normals;
    if (root.contains("normals") && !parse_source_normals(root["normals"], source_positions,
                                                           source_normals, err))
        return std::nullopt;
    if (!parse_faces(root["faces"], source_positions, source_colors, source_normals,
                     asset.positions, asset.normals, asset.colors, err)) return std::nullopt;
    if (asset.triangle_count() <= 0) {
        err = "mesh_asset: no valid faces";
        return std::nullopt;
    }
    return asset;
}
