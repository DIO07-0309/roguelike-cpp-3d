#include "data/camera_defs.h"
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

bool parse_zoom(const nlohmann::json& j, ZoomDef& out, std::string& err) {
    out.fov_scale = get_float(j, "fov_scale", 1.0f);
    out.duration = get_float(j, "duration", 0.0f);
    if (out.fov_scale <= 0.1f || out.fov_scale > 2.0f) {
        err = "camera: fov_scale out of range [0.1, 2.0]";
        return false;
    }
    if (out.duration < 0.0f) {
        err = "camera: duration must be >= 0";
        return false;
    }
    return true;
}

bool parse_boss_war(const nlohmann::json& j, BossWarDef& out, std::string& err) {
    if (!j.contains("zoom_in") || !j["zoom_in"].is_object()) {
        err = "camera: boss_war.zoom_in missing";
        return false;
    }
    if (!parse_zoom(j["zoom_in"], out.zoom_in, err)) return false;

    if (!j.contains("zoom_out") || !j["zoom_out"].is_object()) {
        err = "camera: boss_war.zoom_out missing";
        return false;
    }
    if (!parse_zoom(j["zoom_out"], out.zoom_out, err)) return false;

    out.lerp_speed = get_float(j, "lerp_speed", 2.0f);
    if (out.lerp_speed <= 0.0f) {
        err = "camera: lerp_speed must be > 0";
        return false;
    }
    return true;
}

bool parse_kill_stun(const nlohmann::json& j, KillStunDef& out, std::string& err) {
    out.duration = get_float(j, "duration", 0.08f);
    out.shake_amplitude = get_float(j, "shake_amplitude", 3.0f);
    out.shake_frequency = get_float(j, "shake_frequency", 20.0f);

    if (out.duration <= 0.0f || out.duration > 0.15f) {
        err = "camera: kill_stun.duration must be in (0, 0.15]";
        return false;
    }
    if (out.shake_amplitude < 0.0f) {
        err = "camera: shake_amplitude must be >= 0";
        return false;
    }
    if (out.shake_frequency <= 0.0f) {
        err = "camera: shake_frequency must be > 0";
        return false;
    }
    return true;
}

} // namespace

std::optional<CameraDef> load_camera_file(const std::string& path, std::string& err) {
    nlohmann::json j;
    if (!read_json_file(path, j, err)) return std::nullopt;

    CameraDef def;
    if (j.contains("boss_war") && j["boss_war"].is_object()) {
        if (!parse_boss_war(j["boss_war"], def.boss_war, err)) return std::nullopt;
    }
    if (j.contains("kill_stun") && j["kill_stun"].is_object()) {
        if (!parse_kill_stun(j["kill_stun"], def.kill_stun, err)) return std::nullopt;
    }
    return def;
}
