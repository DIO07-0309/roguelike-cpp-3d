#include "data/voxel_part_defs.h"

#include <fstream>
#include <sstream>

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

float get_float(const nlohmann::json& o, const char* key, float fallback) {
    if (o.contains(key) && o[key].is_number()) return o[key].get<float>();
    return fallback;
}

bool parse_hem(const nlohmann::json& arr, VoxelPartDef& def, std::string& err) {
    if (!arr.is_array()) { err = "voxel_parts: hem must be array"; return false; }
    for (const auto& step : arr) {
        if (!step.is_array() || step.size() < 2 || !step[0].is_number() || !step[1].is_number()) {
            err = "voxel_parts: hem step must be [rows, add]";
            return false;
        }
        VoxelHemStep s;
        s.rows_from_bottom = step[0].get<int>();
        s.add_px = step[1].get<float>();
        def.hem.push_back(s);
    }
    return true;
}

bool parse_part(const nlohmann::json& o, VoxelPartDef& def, std::string& err) {
    if (!o.is_object()) { err = "voxel_parts: part must be object"; return false; }
    def.base = get_float(o, "base", def.base);
    def.growth = get_float(o, "growth", def.growth);
    def.cap = get_float(o, "cap", def.cap);
    def.ink_px = get_float(o, "ink", def.ink_px);
    def.y_offset = get_float(o, "y_offset", def.y_offset);
    if (o.contains("hem")) {
        def.hem.clear();   // A13-M2: 合并语义 — 显式 hem 整体替换默认
        return parse_hem(o["hem"], def, err);
    }
    return true;
}

// A13-M2: overrides = { "<parts_key>": { "<slot>": {部分字段…} } }, 未写字段继承默认槽
bool parse_overrides(const nlohmann::json& j, VoxelConfig& config, std::string& err) {
    if (!j.is_object()) { err = "voxel_parts: overrides must be object"; return false; }
    for (const auto& actor : j.items()) {
        if (!actor.value().is_object()) {
            err = "voxel_parts: overrides[" + actor.key() + "] must be object";
            return false;
        }
        for (const auto& item : actor.value().items()) {
            auto base = config.parts.find(item.key());
            if (base == config.parts.end()) {
                err = "voxel_parts: override for unknown slot: " + item.key();
                return false;
            }
            VoxelPartDef def = base->second;
            if (!parse_part(item.value(), def, err)) return false;
            config.overrides[actor.key()][item.key()] = std::move(def);
        }
    }
    return true;
}

} // namespace

std::string voxel_slot_from_path(const std::string& path) {
    const size_t at = path.find("_part_");
    if (at == std::string::npos) return "";
    const size_t start = at + 6;
    const size_t end = path.find('.', start);
    return path.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

std::string voxel_parts_key_from_skeleton(const std::string& skeleton_path) {
    const size_t slash = skeleton_path.find_last_of("/\\");
    const std::string name =
        slash == std::string::npos ? skeleton_path : skeleton_path.substr(slash + 1);
    const std::string suffix = "_skeleton.json";
    if (name.size() <= suffix.size()) return "";
    const size_t at = name.size() - suffix.size();
    if (name.compare(at, suffix.size(), suffix) != 0) return "";
    return name.substr(0, at);
}

std::optional<VoxelConfig> parse_voxel_config(const nlohmann::json& j, std::string& err) {
    VoxelConfig config;
    config.ink_max_luminance = get_float(j, "ink_max_luminance", config.ink_max_luminance);
    config.alpha_min = get_float(j, "alpha_min", config.alpha_min);
    if (!j.contains("parts") || !j["parts"].is_object() || j["parts"].empty()) {
        err = "voxel_parts: parts missing/empty";
        return std::nullopt;
    }
    for (const auto& item : j["parts"].items()) {
        VoxelPartDef def;
        if (!parse_part(item.value(), def, err)) return std::nullopt;
        config.parts[item.key()] = std::move(def);
    }
    if (j.contains("overrides") && !parse_overrides(j["overrides"], config, err))
        return std::nullopt;
    if (config.alpha_min <= 0.f || config.alpha_min > 1.f) {
        err = "voxel_parts: alpha_min out of (0, 1]";
        return std::nullopt;
    }
    return config;
}

std::optional<VoxelConfig> load_voxel_parts_file(const std::string& path, std::string& err) {
    nlohmann::json j;
    if (!read_json_file(path, j, err)) return std::nullopt;
    return parse_voxel_config(j, err);
}
