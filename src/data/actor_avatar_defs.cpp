// A6-S1: actor_avatars.json 加载器 — {"actors": {"<key>": {"skeleton": "...", "anim": "..."}}}
#include "data/actor_avatar_defs.h"

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

bool parse_entry(const std::string& key, const nlohmann::json& e,
                 ActorAvatarDef& def, std::string& err) {
    if (!e.is_object()) { err = "actor_avatars: entry '" + key + "' not object"; return false; }
    for (const char* field : {"skeleton", "anim"}) {
        if (!e.contains(field) || !e[field].is_string()
            || e[field].get<std::string>().empty()) {
            err = "actor_avatars: entry '" + key + "' missing/empty field: " + field;
            return false;
        }
    }
    def.skeleton = e["skeleton"].get<std::string>();
    def.anim = e["anim"].get<std::string>();
    return true;
}

} // namespace

std::optional<std::map<std::string, ActorAvatarDef>> parse_actor_avatars(
        const nlohmann::json& j, std::string& err) {
    std::map<std::string, ActorAvatarDef> out;
    if (!j.is_object()) { err = "actor_avatars: root not object"; return std::nullopt; }
    if (!j.contains("actors")) return out;                       // 缺省 = 全回退
    if (!j["actors"].is_object()) { err = "actor_avatars: actors not object"; return std::nullopt; }
    for (const auto& [key, e] : j["actors"].items()) {
        ActorAvatarDef def;
        if (!parse_entry(key, e, def, err)) return std::nullopt;
        out[key] = std::move(def);
    }
    return out;
}

std::optional<std::map<std::string, ActorAvatarDef>> load_actor_avatars_file(
        const std::string& path, std::string& err) {
    nlohmann::json j;
    if (!read_json_file(path, j, err)) {
        err.clear();           // 文件缺失是合法配置 (白名单空) — 全回退, 非错误
        return std::map<std::string, ActorAvatarDef>{};
    }
    return parse_actor_avatars(j, err);
}
