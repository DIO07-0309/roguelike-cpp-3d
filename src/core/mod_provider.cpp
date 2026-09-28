#include "mod_provider.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#undef ERROR  // windows.h defines ERROR as macro, conflicts with LogLevel::ERROR
#endif

using json = nlohmann::json;

// ═══════════════════════════════════════════════════════════════
// G4.1: ModProvider — 单个 Mod 目录
// ═══════════════════════════════════════════════════════════════

// G13: mod.json 基础字段
void ModProvider::_fill_manifest_base(const nlohmann::json& j, const std::string& mod_dir,
                                      Manifest& m) {
    m.id       = j.value("id", "");
    m.name     = j.value("name", m.id);
    m.version  = j.value("version", "0.1.0");
    m.priority = j.value("priority", 100);
    m.enabled  = j.value("enabled", true);
    m.dir_path = mod_dir;
}

// G13: provides 模块清单
void ModProvider::_fill_manifest_provides(const nlohmann::json& j, Manifest& m) {
    if (j.contains("provides") && j["provides"].is_array())
        for (auto& p : j["provides"])
            m.provides.push_back(p.get<std::string>());
}

// G13: G4.2 dependency 字段 (requires 支持对象/字符串两种形态)
void ModProvider::_fill_manifest_deps(const nlohmann::json& j, Manifest& m) {
    if (j.contains("requires") && j["requires"].is_array()) {
        for (auto& r : j["requires"]) {
            if (r.is_object() && r.contains("id"))
                m.requires_ids.push_back(r["id"].get<std::string>());
            else if (r.is_string())
                m.requires_ids.push_back(r.get<std::string>());
        }
    }
    if (j.contains("load_after") && j["load_after"].is_array())
        for (auto& la : j["load_after"])
            m.load_after.push_back(la.get<std::string>());
}

// G13: manifest 摘要日志
void ModProvider::_log_manifest(const Manifest& m) {
    LOG_INFO("[ModProvider] + %s (v%s) pri=%d provides=%zu",
        m.id.c_str(), m.version.c_str(), m.priority, m.provides.size());
}

std::unique_ptr<ModProvider> ModProvider::create(const std::string& mod_dir) {
    // 读取 mod.json
    std::string manifest_path = mod_dir + "/mod.json";
    std::ifstream f(manifest_path);
    if (!f.is_open()) {
        LOG_INFO("[ModProvider] Skipping %s: no mod.json", mod_dir.c_str());
        return nullptr;
    }

    json j;
    try { f >> j; }
    catch (const std::exception& e) {
        LOG_ERROR("[ModProvider] JSON error in %s: %s", manifest_path.c_str(), e.what());
        return nullptr;
    }

    auto mp = std::unique_ptr<ModProvider>(new ModProvider());
    _fill_manifest_base(j, mod_dir, mp->_manifest);

    if (mp->_manifest.id.empty()) {
        LOG_ERROR("[ModProvider] Missing 'id' in %s — skipping", manifest_path.c_str());
        return nullptr;
    }

    _fill_manifest_provides(j, mp->_manifest);
    _fill_manifest_deps(j, mp->_manifest);
    _log_manifest(mp->_manifest);
    return mp;
}


bool ModProvider::has_module(const std::string& module_name) const {
    return std::find(_manifest.provides.begin(), _manifest.provides.end(), module_name)
           != _manifest.provides.end();
}

ModuleData ModProvider::load_module(const std::string& module_name) {
    ModuleData md;
    md.provider = _manifest.id;  // G4.2: use mod id (not display name) for namespace
    md.source = _manifest.dir_path + "/" + module_name + "s.json";
    // Note: module names are singular (enemy, boss), file names are plural (enemies.json, bosses.json)
    // We try with the standard plural form
    std::ifstream f(md.source);
    if (!f.is_open()) {
        // Try alternative: <module>.json (no plural)
        md.source = _manifest.dir_path + "/" + module_name + ".json";
        f.open(md.source);
        if (!f.is_open()) {
            md.text.clear();
            return md;
        }
    }
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    md.text = std::move(content);
    return md;
}
