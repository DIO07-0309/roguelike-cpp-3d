#include "data/enemy_defs.h"
#include "core/merge_patch.h"   // G4.3
#include <nlohmann/json.hpp>
#include <fstream>
#include <unordered_map>
#include <cstdio>

using json = nlohmann::json;

// ============================================================
// G1 Step5: EnemyDef 全局注册表
// ============================================================

static std::unordered_map<std::string, EnemyDef> g_enemy_defs;
static bool g_enemy_defs_loaded = false;

// ── 辅助: JSON → EnemyAIDef ──
static EnemyAIDef _parse_ai(const json& j) {
    EnemyAIDef ai;
    if (j.contains("sight"))        ai.sight = j["sight"].get<float>();
    if (j.contains("speed"))        ai.speed = j["speed"].get<float>();
    if (j.contains("patrol"))       ai.patrol = j["patrol"].get<float>();
    if (j.contains("attack_range")) ai.attack_range = j["attack_range"].get<float>();
    return ai;
}

// ── 辅助: JSON → EnemySkillDef ──
static EnemySkillDef _parse_skill(const json& j) {
    EnemySkillDef sk;
    if (j.contains("id"))               sk.id = j["id"].get<std::string>();
    if (j.contains("max_cooldown"))     sk.max_cooldown = j["max_cooldown"].get<float>();
    if (j.contains("initial_cooldown")) sk.initial_cooldown = j["initial_cooldown"].get<float>();
    return sk;
}

// ── 辅助: JSON → BuffTrigger ──
static BuffTrigger _parse_on_hit(const json& j) {
    BuffTrigger tr;
    if (j.contains("buff"))   tr.buff_id = j["buff"].get<std::string>();
    if (j.contains("stacks")) tr.stacks = j["stacks"].get<int>();
    if (j.contains("chance")) tr.chance = j["chance"].get<float>();
    tr.target = BuffTarget::ENEMY;  // on_hit 默认对敌人(玩家)生效
    return tr;
}

// ── D2: 辅助: JSON → EnemyProjectileDef ──
static EnemyProjectileDef _parse_projectile(const json& j) {
    EnemyProjectileDef pd;
    pd.enabled       = j.value("enabled", true);
    pd.speed         = j.value("speed", 300.0f);
    pd.warning_time  = j.value("warning_time", 0.8f);
    pd.warning_level = j.value("warning_level", 0);
    return pd;
}

// ── 辅助: JSON → 单个 EnemyDef ──
// ── 辅助: EnemyDef 基础字段 ──
static void _parse_enemy_base(const json& j, EnemyDef& def) {
    def.id         = j.value("id", "");
    def.name       = j.value("name", "");
    def.visual_id  = j.value("visual_id", "slime");
    def.hp         = j.value("hp", 15);
    def.atk        = j.value("atk", 3);
    def.pdef       = j.value("pdef", 0);
    def.mdef       = j.value("mdef", 1);
    def.type_str        = j.value("type", "normal");
    def.role_str        = j.value("role", "none");
    def.attack_type_str = j.value("attack_type", "physical");
    def.ai_archetype    = j.value("ai_archetype", "default");   // G5.3
    def.attack_pattern_str = j.value("attack_pattern", "");     // G5.5
    def.attack_cooldown   = j.value("attack_cooldown", 1.5f);
}

// ── 辅助: EnemyDef 嵌套配置 (ai / skills / on_hit / projectile) ──
static void _parse_enemy_nested(const json& j, EnemyDef& def) {
    // 嵌套 ai
    if (j.contains("ai"))
        def.ai = _parse_ai(j["ai"]);

    // 技能数组
    if (j.contains("skills") && j["skills"].is_array())
        for (auto& sk : j["skills"])
            def.skills.push_back(_parse_skill(sk));

    // on_hit 触发器
    if (j.contains("on_hit") && j["on_hit"].is_array())
        for (auto& tr : j["on_hit"])
            def.on_hit.push_back(_parse_on_hit(tr));

    // D2: 弹道攻击配置
    if (j.contains("projectile"))
        def.projectile = _parse_projectile(j["projectile"]);
}

// ── 辅助: EnemyDef 精英配置 ──
static void _parse_enemy_elite(const json& j, EnemyDef& def) {
    def.is_elite = j.value("is_elite", false);
    if (!j.contains("elite_buffs") || !j["elite_buffs"].is_array()) return;
    for (auto& eb : j["elite_buffs"]) {
        EliteBuffEntry entry;
        entry.buff_id = eb.value("buff", "");
        entry.stacks  = eb.value("stacks", 1);
        def.elite_buff_pool.push_back(entry);
    }
}

// ── 辅助: JSON → 单个 EnemyDef (分段解析) ──
static EnemyDef _parse_enemy(const json& j) {
    EnemyDef def;
    _parse_enemy_base(j, def);
    _parse_enemy_nested(j, def);
    _parse_enemy_elite(j, def);
    return def;
}


static std::unordered_map<std::string, std::string> _enemy_src; // G4.3: source JSON per entry

// ── G4.2: from JSON text + MergeMode + namespace ──
// G4.3: __patch 合并 — 解析已存 JSON, 合并, 重新解析入库
static void _apply_patch_entry(const json& entry, const std::string& raw_id,
                               int& over, int& merged) {
    json stored;
    try { stored = json::parse(_enemy_src[raw_id]); }
    catch (...) { over++; return; }
    json merged_json = _merge_patch(stored, entry);
    auto def = _parse_enemy(merged_json);
    def.id = raw_id;
    g_enemy_defs[raw_id] = def;
    _enemy_src[raw_id] = merged_json.dump();
    merged++;
}

static void _report_enemy_load(int count, int over, int merged) {
    printf("[ENEMY_DEFS] +%d", count);
    if (over > 0) printf(" ~%d", over);
    if (merged > 0) printf(" %% %d", merged);
    printf("\n");
}

// ── G4.2: from JSON text + MergeMode + namespace ──
int load_enemy_defs_from_json(const char* json_text, MergeMode mode,
                               const char* id_namespace) {
    if (!json_text || !json_text[0]) return 0;
    json j;
    try { j = json::parse(json_text); }
    catch (const std::exception& e) {
        printf("[ENEMY_DEFS] JSON parse: %s\n", e.what());
        return 0;
    }
    if (!j.is_array()) return 0;

    const std::string ns_prefix = id_namespace ? (std::string(id_namespace) + ":") : "";
    int count = 0, over = 0, merged = 0;
    for (auto& entry : j) {
        // G4.3: __patch merge
        const bool is_patch = (mode == MergeMode::MergePatch && entry.value("__patch", false));
        std::string raw_id = entry.value("id", "");
        if (!ns_prefix.empty()) raw_id = ns_prefix + raw_id;
        const bool exists = g_enemy_defs.count(raw_id) > 0;

        if (mode == MergeMode::Skip && exists) continue;
        if (is_patch && exists) { _apply_patch_entry(entry, raw_id, over, merged); continue; }

        auto def = _parse_enemy(entry);
        if (def.id.empty()) continue;
        if (!ns_prefix.empty()) def.id = ns_prefix + def.id;
        if (exists) {
            if (mode == MergeMode::Replace || mode == MergeMode::MergePatch) {
                g_enemy_defs[def.id] = def; over++;
                _enemy_src[def.id] = entry.dump();
            }
            continue;
        }
        g_enemy_defs[def.id] = def; count++;
        _enemy_src[def.id] = entry.dump();
    }
    g_enemy_defs_loaded = true;
    _report_enemy_load(count, over, merged);
    return count + over + merged;
}


// ── 公开接口 ──

bool load_enemy_defs(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        printf("[ENEMY_DEFS] ERROR: cannot open %s\n", json_path.c_str());
        return false;
    }

    std::string text((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    return load_enemy_defs_from_json(text.c_str(), MergeMode::Skip) > 0 || is_enemy_defs_loaded();
}

const EnemyDef* get_enemy_def(const std::string& id) {
    auto it = g_enemy_defs.find(id);
    return (it != g_enemy_defs.end()) ? &it->second : nullptr;
}

const std::unordered_map<std::string, EnemyDef>& get_all_enemy_defs() {
    return g_enemy_defs;
}

bool is_enemy_defs_loaded() { return g_enemy_defs_loaded; }
