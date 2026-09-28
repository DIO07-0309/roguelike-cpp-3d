#pragma once
#include <string>
#include <vector>
#include <unordered_set>

// ============================================================
// G4.4: ModManager — Mod 生命周期管理
// scan/ list/ enable/ disable + mods/config.json 持久化
// ============================================================

struct ModInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string dir_path;
    bool enabled = false;     // 用户选择
    bool valid = false;       // mod.json 有效?
    std::string error;        // 加载失败原因
};

class ModManager {
public:
    // ── 扫描 mods/ 目录 ──
    void scan(const std::string& mods_dir = "mods");

    // ── 启用/禁用 ──
    void enable(const std::string& mod_id);
    void disable(const std::string& mod_id);
    bool is_enabled(const std::string& mod_id) const;

    // ── 持久化 ──
    bool load_config(const std::string& config_path = "mods/config.json");
    bool save_config(const std::string& config_path = "mods/config.json") const;

    // ── 查询 ──
    const std::vector<ModInfo>& all() const { return _mods; }
    std::vector<std::string> enabled_ids() const;
    int total_count() const { return (int)_mods.size(); }
    int enabled_count() const;

private:
    std::vector<ModInfo> _mods;
    std::unordered_set<std::string> _enabled_set;

    // G13: scan() 分段
    void _scan_dir(const std::string& mods_dir);
};
