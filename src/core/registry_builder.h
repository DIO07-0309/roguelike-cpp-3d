#pragma once
#include <string>
#include <vector>
#include <memory>
#include "registry_provider.h"

// ============================================================
// G4.1: RegistryBuilder — Provider 排序 + JSON 文本分发
// 不解析任何模块的 JSON 结构, 只负责调度
// ============================================================

// ── 每个 Def 模块的加载器: json_text + mode + namespace → 返回加载条目数 ──
// id_namespace: nullptr=Builtin(无前缀), "hard_mode"→前缀 "hard_mode:"
using ModuleLoaderFn = int (*)(const char* json_text, MergeMode mode,
                                const char* id_namespace);

class RegistryBuilder {
public:
    // ── Provider 管理 ──
    void add_provider(std::unique_ptr<IRegistryProvider> provider);

    // ── 注册模块加载器 (各 Def 模块在启动时调用) ──
    void register_module(const std::string& name, ModuleLoaderFn loader);

    // ── 执行全部构建 ──
    bool build_all();

    // ── G4.1.5: 引用完整性验证 ──
    void validate();
    const std::vector<ValFinding>& findings() const { return _findings; }

    // ── 查询 ──
    const std::vector<BuildRecord>& log() const { return _log; }
    int provider_count() const { return (int)_providers.size(); }

private:
    std::vector<std::unique_ptr<IRegistryProvider>> _providers;
    std::vector<BuildRecord> _log;
    std::vector<ValFinding> _findings;

    // ── 每个模块: name → loader function ──
    struct ModuleEntry {
        std::string name;
        ModuleLoaderFn loader;
    };
    std::vector<ModuleEntry> _modules;

    // ── G13: build_all / validate 分段 ──
    void _sort_providers();
    void _build_module(const ModuleEntry& mod);
    void _validate_skill_buff_refs();
    void _validate_item_charm_refs();
    void _validate_enemy_on_hit_refs();
    void _validate_required_names();
    void _log_validation_summary();
};
