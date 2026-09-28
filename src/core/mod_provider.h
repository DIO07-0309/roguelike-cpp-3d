#pragma once
#include "registry_provider.h"
#include <vector>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

// ============================================================
// G4.1: ModProvider — 单个 Mod 目录的 Provider
// 优先级来自 mod.json, 由 RegistryBuilder 管理
// ============================================================

class ModProvider : public IRegistryProvider {
public:
    // 从 mods/<dir>/ 目录创建 (读取 mod.json + 数据文件列表)
    static std::unique_ptr<ModProvider> create(const std::string& mod_dir);

    const char* name() const override { return _manifest.name.c_str(); }
    int priority() const override { return _manifest.priority; }
    bool enabled() const override { return _manifest.enabled; }

    bool has_module(const std::string& module_name) const override;
    ModuleData load_module(const std::string& module_name) override;

    // G4.2: for DependencyResolver
    struct Manifest {
        std::string id;
        std::string name;
        std::string version;
        int priority = 100;
        bool enabled = true;
        std::string dir_path;
        std::vector<std::string> provides;
        std::vector<std::string> requires_ids;
        std::vector<std::string> load_after;
    };
    const Manifest& manifest() const { return _manifest; }

private:
    ModProvider() = default;
    Manifest _manifest;

    // G13: create() 分段
    static void _fill_manifest_base(const nlohmann::json& j, const std::string& mod_dir,
                                    Manifest& m);
    static void _fill_manifest_provides(const nlohmann::json& j, Manifest& m);
    static void _fill_manifest_deps(const nlohmann::json& j, Manifest& m);
    static void _log_manifest(const Manifest& m);
};
