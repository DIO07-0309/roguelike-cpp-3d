#include "data/animation_defs.h"
#include <cmath>
#include <fstream>
#include <sstream>

namespace {

constexpr float kKeyTolerance = 1e-3f;

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

bool parse_bones(const nlohmann::json& j, SkeletonDef& sk, std::string& err) {
    if (!j.contains("bones") || !j["bones"].is_array() || j["bones"].empty()) {
        err = "skeleton: bones missing/empty";
        return false;
    }
    std::map<std::string, int> name_to_idx;
    for (const auto& b : j["bones"]) {
        if (!b.contains("name") || !b["name"].is_string()) { err = "skeleton: bone missing name"; return false; }
        BoneDef bd;
        bd.name = b["name"].get<std::string>();
        bd.x = get_float(b, "x", 0.f);
        bd.y = get_float(b, "y", 0.f);
        if (b.contains("parent") && !b["parent"].is_null()) {
            if (!b["parent"].is_string()) { err = "skeleton: parent must be string"; return false; }
            auto it = name_to_idx.find(b["parent"].get<std::string>());
            if (it == name_to_idx.end()) {   // 父未声明 = 前向引用, 链合成前提被破坏
                err = "skeleton: bone '" + bd.name + "' parent unknown/forward-ref";
                return false;
            }
            bd.parent = it->second;
        }
        name_to_idx[bd.name] = static_cast<int>(sk.bones.size());
        sk.bones.push_back(std::move(bd));
    }
    return true;
}

bool parse_parts(const nlohmann::json& j, SkeletonDef& sk, std::string& err) {
    std::map<std::string, int> name_to_idx;
    for (size_t i = 0; i < sk.bones.size(); ++i) name_to_idx[sk.bones[i].name] = static_cast<int>(i);
    if (!j.contains("parts") || !j["parts"].is_array()) { err = "skeleton: parts missing"; return false; }
    for (const auto& p : j["parts"]) {
        PartDef pd;
        if (!p.contains("bone") || !p["bone"].is_string() || !p.contains("file")) {
            err = "skeleton: part missing bone/file";
            return false;
        }
        auto it = name_to_idx.find(p["bone"].get<std::string>());
        if (it == name_to_idx.end()) { err = "skeleton: part bone unknown: " + p["bone"].get<std::string>(); return false; }
        pd.bone = it->second;
        pd.file = p["file"].get<std::string>();
        pd.dx = get_float(p, "dx", 0.f);
        pd.dy = get_float(p, "dy", 0.f);
        if (p.contains("pivot") && p["pivot"].is_array() && p["pivot"].size() >= 2) {
            pd.pivot_x = p["pivot"][0].get<float>();
            pd.pivot_y = p["pivot"][1].get<float>();
        }
        sk.parts.push_back(std::move(pd));
    }
    return true;
}

bool parse_clip(const nlohmann::json& c, const std::map<std::string, int>& bone_idx,
                AnimClipDef& clip, std::string& err) {
    clip.loop = c.contains("loop") && c["loop"].get<bool>();
    clip.dur = get_float(c, "dur", 0.f);
    if (clip.dur <= 0.f) { err = "anim: clip dur must be > 0"; return false; }
    if (!c.contains("tracks") || !c["tracks"].is_array()) { err = "anim: clip tracks missing"; return false; }
    for (const auto& t : c["tracks"]) {
        TrackDef tr;
        if (!t.contains("bone") || !t["bone"].is_string()) { err = "anim: track missing bone"; return false; }
        auto it = bone_idx.find(t["bone"].get<std::string>());
        if (it == bone_idx.end()) { err = "anim: track bone unknown"; return false; }
        tr.bone = it->second;
        if (!t.contains("keys") || !t["keys"].is_array() || t["keys"].empty()) {
            err = "anim: track keys missing/empty";
            return false;
        }
        for (const auto& k : t["keys"]) {
            KeyDef kd;
            kd.t = get_float(k, "t", 0.f);
            kd.x = get_float(k, "x", 0.f);
            kd.y = get_float(k, "y", 0.f);
            kd.rot = get_float(k, "rot", 0.f);
            kd.sx = get_float(k, "sx", 1.f);
            kd.sy = get_float(k, "sy", 1.f);
            tr.keys.push_back(kd);
        }
        clip.tracks.push_back(std::move(tr));
    }
    return true;
}

bool validate_clip_keys(const AnimClipDef& clip, const std::string& name, std::string& err) {
    for (const auto& tr : clip.tracks) {
        for (size_t i = 1; i < tr.keys.size(); ++i) {
            if (tr.keys[i].t < tr.keys[i - 1].t) { err = "anim[" + name + "]: keys not sorted"; return false; }
        }
        const float last_t = tr.keys.back().t;
        if (std::fabs(last_t - clip.dur) > kKeyTolerance) {
            err = "anim[" + name + "]: last key t != dur";
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<SkeletonDef> parse_skeleton(const nlohmann::json& j, std::string& err) {
    SkeletonDef sk;
    sk.pixels_per_unit = get_float(j, "pixels_per_unit", 0.5f);
    if (j.contains("anchor") && j["anchor"].is_array() && j["anchor"].size() >= 2) {
        sk.anchor_x = j["anchor"][0].get<float>();
        sk.anchor_y = j["anchor"][1].get<float>();
    }
    if (!parse_bones(j, sk, err)) return std::nullopt;
    if (!parse_parts(j, sk, err)) return std::nullopt;
    return sk;
}

std::optional<AnimSetDef> parse_anim(const nlohmann::json& j, const SkeletonDef& sk, std::string& err) {
    std::map<std::string, int> bone_idx;
    for (size_t i = 0; i < sk.bones.size(); ++i) bone_idx[sk.bones[i].name] = static_cast<int>(i);
    if (!j.contains("animations") || !j["animations"].is_object()) { err = "anim: animations missing"; return std::nullopt; }
    AnimSetDef set;
    for (const auto& [name, c] : j["animations"].items()) {
        AnimClipDef clip;
        if (!parse_clip(c, bone_idx, clip, err)) return std::nullopt;
        err.clear();
        if (!validate_clip_keys(clip, name, err)) return std::nullopt;
        set.clips[name] = std::move(clip);
    }
    return set;
}

std::optional<SkeletonDef> load_skeleton_file(const std::string& path, std::string& err) {
    nlohmann::json j;
    if (!read_json_file(path, j, err)) return std::nullopt;
    return parse_skeleton(j, err);
}

std::optional<AnimSetDef> load_anim_file(const std::string& path, const SkeletonDef& sk, std::string& err) {
    nlohmann::json j;
    if (!read_json_file(path, j, err)) return std::nullopt;
    return parse_anim(j, sk, err);
}
