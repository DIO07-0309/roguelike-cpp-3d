#include "registry_builder.h"
#include "core/logger.h"
#include <algorithm>
#include <cstring>

// ── G4.1.5: 引用验证需要读取 loaded registries ──
#include "data/skill_defs.h"
#include "data/item_defs.h"
#include "data/enemy_defs.h"
#include "systems/combat_system.h"  // BuffDef / RelicDef

// ═══════════════════════════════════════════════════════════════
// G4.1: RegistryBuilder 实现
// 职责: Provider 排序 → JSON 文本分发 → 各模块 Loader 自行解析
// 不参与任何模块的 JSON 结构解析
// ═══════════════════════════════════════════════════════════════

void RegistryBuilder::add_provider(std::unique_ptr<IRegistryProvider> p) {
    if (!p || !p->enabled()) return;
    _providers.push_back(std::move(p));
}

void RegistryBuilder::register_module(const std::string& name, ModuleLoaderFn loader) {
    _modules.push_back({name, loader});
}

bool RegistryBuilder::build_all() {
    _log.clear();
    _sort_providers();

    LOG_INFO("[Registry] Building with %d providers for %zu modules",
             (int)_providers.size(), _modules.size());

    for (auto& mod : _modules) _build_module(mod);

    LOG_INFO("[Registry] Build complete: %zu records", _log.size());
    // ── G4.1.5: 加载后验证引用完整性 ──
    validate();
    return true;
}

// G13: provider 排序 — priority → name → registration order (stable)
void RegistryBuilder::_sort_providers() {
    std::stable_sort(_providers.begin(), _providers.end(),
        [](const auto& a, const auto& b) {
            if (a->priority() != b->priority())
                return a->priority() < b->priority();
            int nc = strcmp(a->name(), b->name());
            if (nc != 0) return nc < 0;
            return false;   // registration order preserved
        });
}

// G13: 单模块 provider 链式加载 (第一个 = 初始加载, 后续 = 字段合并)
void RegistryBuilder::_build_module(const ModuleEntry& mod) {
    MergeMode mode = MergeMode::Skip;
    bool has_initial = false;

    for (auto& prov : _providers) {
        if (!prov->has_module(mod.name)) continue;

        auto data = prov->load_module(mod.name);
        if (data.text.empty()) continue;

        // G4.2: namespace — Builtin=nullptr, Mod=mod_id
        const char* ns = (data.provider == "Builtin") ? nullptr
                           : data.provider.c_str();

        int count = mod.loader(data.text.c_str(), mode, ns);

        _log.push_back({
            mod.name,
            data.provider,
            data.source,
            mode == MergeMode::Replace || mode == MergeMode::MergePatch,
            mode == MergeMode::MergePatch,   // is_patch
            count,
            0                                // merged count recorded by loader
        });

        if (!has_initial) {
            has_initial = true;
            mode = MergeMode::MergePatch;   // G4.3: 后续 provider = 字段合并
        }
    }

    if (!has_initial)
        LOG_INFO("[Registry] Module '%s': no provider has data — skipped", mod.name.c_str());
}


// ═══════════════════════════════════════════════════════════════
// G4.1.5: Registry Validation — 跨模块引用检查
// ═══════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════
// G4.1.5: Registry Validation — 跨模块引用检查
// ═══════════════════════════════════════════════════════════════
void RegistryBuilder::validate() {
    _findings.clear();
    _validate_skill_buff_refs();
    _validate_item_charm_refs();
    _validate_enemy_on_hit_refs();
    _validate_required_names();
    _log_validation_summary();
}

// ── 1. Skill triggers → BuffDef ──
void RegistryBuilder::_validate_skill_buff_refs() {
    for (auto& kv : get_all_skill_defs()) {
        auto& sk = kv.second;
        for (auto& tr : sk.triggers) {
            if (!tr.buff_id.empty() && !get_buff_def(tr.buff_id))
                _findings.push_back({ValSeverity::Warning, "skill", sk.id,
                    "references unknown buff '" + tr.buff_id + "'"});
        }
    }
}

// ── 2. Item charm → SkillDef ──
void RegistryBuilder::_validate_item_charm_refs() {
    for (auto& kv : get_all_item_defs()) {
        auto& it = kv.second;
        if (it.category == "charm" && !it.skill_id.empty()
            && !get_skill_def(it.skill_id))
            _findings.push_back({ValSeverity::Warning, "item", it.id,
                "charm references unknown skill '" + it.skill_id + "'"});
    }
}

// ── 3. Enemy on_hit → BuffDef ──
void RegistryBuilder::_validate_enemy_on_hit_refs() {
    for (auto& kv : get_all_enemy_defs()) {
        auto& en = kv.second;
        for (auto& tr : en.on_hit) {
            if (!tr.buff_id.empty() && !get_buff_def(tr.buff_id))
                _findings.push_back({ValSeverity::Warning, "enemy", en.id,
                    "on_hit references unknown buff '" + tr.buff_id + "'"});
        }
    }
}

// ── 4. Required name check ──
void RegistryBuilder::_validate_required_names() {
    for (auto& kv : get_all_enemy_defs())
        if (kv.second.name.empty())
            _findings.push_back({ValSeverity::Error, "enemy", kv.first, "missing 'name'"});
    for (auto& kv : get_all_skill_defs())
        if (kv.second.name.empty())
            _findings.push_back({ValSeverity::Error, "skill", kv.first, "missing 'name'"});
    for (auto& kv : get_all_item_defs())
        if (kv.second.name.empty())
            _findings.push_back({ValSeverity::Error, "item", kv.first, "missing 'name'"});
}

// ── Log summary ──
void RegistryBuilder::_log_validation_summary() {
    int warns = 0, errs = 0;
    for (auto& f : _findings)
        if (f.severity == ValSeverity::Error) errs++; else warns++;
    if (errs > 0 || warns > 0)
        LOG_INFO("[Validator] %d errors, %d warnings", errs, warns);
    else
        LOG_INFO("[Validator] All checks passed");
}

