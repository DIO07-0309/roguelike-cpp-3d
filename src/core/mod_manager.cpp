#include "mod_manager.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

// ============================================================
// G4.4: ModManager — 目录扫描 + 配置持久化
// ============================================================

static ModInfo _build_mod_info(const char* dir_name, const std::string& dir, const json& j) {
    ModInfo mi;
    mi.id       = j.value("id", dir_name);
    mi.name     = j.value("name", mi.id);
    mi.version  = j.value("version", "?");
    mi.dir_path = dir;
    mi.valid    = !mi.id.empty();
    mi.enabled  = j.value("enabled", true);
    if (!mi.valid) mi.error = "missing id";
    return mi;
}

// G4.4: 扫描 mods/ 子目录 (Windows FindFirstFile; POSIX 暂为 stub)
void ModManager::_scan_dir(const std::string& mods_dir) {
#ifdef _WIN32
    std::string search = mods_dir + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;

        std::string dir = mods_dir + "/" + fd.cFileName;
        std::string mf = dir + "/mod.json";
        std::ifstream f(mf);
        if (!f.is_open()) continue;

        json j;
        try { f >> j; }
        catch (...) { continue; }

        _mods.push_back(_build_mod_info(fd.cFileName, dir, j));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    (void)mods_dir;   // stub — Windows only for now
#endif
}

void ModManager::scan(const std::string& mods_dir) {
    _mods.clear();
    _enabled_set.clear();

    _scan_dir(mods_dir);

    // ── 尝试加载优先配置 ──
    load_config();

    LOG_INFO("[ModManager] Scanned %d mods, %d enabled",
             (int)_mods.size(), enabled_count());
}


bool ModManager::load_config(const std::string& config_path) {
    std::ifstream f(config_path);
    if (!f.is_open()) return false;

    json j;
    try { f >> j; }
    catch (...) { return false; }

    if (!j.contains("enabled") || !j["enabled"].is_array()) return false;

    _enabled_set.clear();
    for (auto& id : j["enabled"])
        _enabled_set.insert(id.get<std::string>());

    // ── 应用默认启用状态 ──
    for (auto& mi : _mods) {
        // If enabled set is non-empty, mod is enabled ONLY if listed
        // If enabled set is empty (first run), use mod.json "enabled" field
        if (!_enabled_set.empty())
            mi.enabled = _enabled_set.count(mi.id) > 0;
    }

    LOG_INFO("[ModManager] Config loaded: %zu enabled mods", _enabled_set.size());
    return true;
}

bool ModManager::save_config(const std::string& config_path) const {
    json j;
    j["enabled"] = json::array();
    for (auto& mi : _mods)
        if (mi.enabled && mi.valid)
            j["enabled"].push_back(mi.id);

    std::ofstream f(config_path);
    if (!f.is_open()) return false;
    f << j.dump(2) << "\n";
    return true;
}

void ModManager::enable(const std::string& mod_id) {
    _enabled_set.insert(mod_id);
    for (auto& mi : _mods)
        if (mi.id == mod_id) mi.enabled = true;
}

void ModManager::disable(const std::string& mod_id) {
    _enabled_set.erase(mod_id);
    for (auto& mi : _mods)
        if (mi.id == mod_id) mi.enabled = false;
}

bool ModManager::is_enabled(const std::string& mod_id) const {
    return _enabled_set.count(mod_id) > 0;
}

std::vector<std::string> ModManager::enabled_ids() const {
    std::vector<std::string> ids;
    for (auto& mi : _mods)
        if (mi.enabled && mi.valid)
            ids.push_back(mi.id);
    return ids;
}

int ModManager::enabled_count() const {
    int n = 0;
    for (auto& mi : _mods) if (mi.enabled && mi.valid) n++;
    return n;
}
