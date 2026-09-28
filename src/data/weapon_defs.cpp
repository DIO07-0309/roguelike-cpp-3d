#include "data/weapon_defs.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>
#include <algorithm>

using json = nlohmann::json;

// ============================================================
// G9: WeaponDef global registry
// ============================================================

static std::unordered_map<std::string, WeaponDef> g_weapon_defs;
static bool g_weapon_loaded = false;

// ── Parse a single AttackStageDef from JSON ──
static AttackStageDef _parse_stage(const json& j) {
    AttackStageDef s;
    s.damage_multiplier = j.value("mult", 1.0f);
    s.hit_shape = hit_shape_from_string(j.value("shape", "CIRCLE").c_str());
    // G10.2: damage_type (0=PHYSICAL, 1=MAGICAL, 2=TRUE)
    s.damage_type = j.value("damage_type", 0);
    s.range     = j.value("range", 1.0f);
    s.width     = j.value("width", 0.5f);
    s.recovery  = j.value("recovery", 0.2f);
    s.windup    = j.value("windup", 0.05f);
    s.hit_frame = j.value("hit_frame", 0.1f);
    s.cancel_window = j.value("cancel_window", 0.6f);
    s.vfx_recipe    = j.value("vfx_recipe", "");
    s.camera_shake  = j.value("camera_shake", 2.0f);
    s.sfx_name      = j.value("sfx", "");
    return s;
}

// ── Parse a single color field [r,g,b,a] ──
static Color _parse_color(const json& j) {
    if (j.is_array() && j.size() >= 3)
        return {
            (unsigned char)j[0].get<int>(),
            (unsigned char)(j.size() > 1 ? j[1].get<int>() : 255),
            (unsigned char)(j.size() > 2 ? j[2].get<int>() : 255),
            (unsigned char)(j.size() > 3 ? j[3].get<int>() : 255)
        };
    return {180, 180, 180, 255};
}

// ── Parse a full WeaponDef from JSON object ──
// ── WeaponDef 基础字段 ──
static void _parse_weapon_base(const json& j, WeaponDef& w) {
    w.id        = j.value("id", "");
    w.name      = j.value("name", "");
    w.type      = weapon_type_from_string(j.value("type", "FIST").c_str());
    w.rarity    = j.value("rarity", "common");
    w.base_damage = j.value("base_damage", 5.0f);
    w.base_range  = j.value("base_range", 1.0f);
    w.min_range   = j.value("min_range", 0.0f);
    w.max_range   = j.value("max_range", 0.0f);
    w.combo_timeout = j.value("combo_timeout", 0.80f);
}

// ── Stages ──
static void _parse_weapon_stages(const json& j, WeaponDef& w) {
    if (j.contains("stages") && j["stages"].is_array()) {
        w.stage_count = std::min((int)j["stages"].size(), 3);
        for (int i = 0; i < w.stage_count; ++i)
            w.stages[i] = _parse_stage(j["stages"][i]);
    } else {
        // Default single stage (fist)
        w.stages[0] = AttackStageDef{};
        w.stage_count = 1;
    }
}

// ── G9.2: 命名与附魔 (quality_colors / rare / epic / legendary / affix) ──
static void _parse_weapon_naming(const json& j, WeaponDef& w) {
    if (j.contains("quality_colors") && j["quality_colors"].is_array()) {
        const int n = std::min((int)j["quality_colors"].size(), 4);
        for (int i = 0; i < n; ++i)
            w.quality_colors[i] = _parse_color(j["quality_colors"][i]);
    }
    if (j.contains("rare_names") && j["rare_names"].is_array()) {
        const int n = std::min((int)j["rare_names"].size(), 2);
        for (int i = 0; i < n; ++i)
            w.rare_names[i] = j["rare_names"][i].get<std::string>();
    }
    // G9.2: Epic names (2 random-roll entries)
    if (j.contains("epic_names") && j["epic_names"].is_array()) {
        const int n = std::min((int)j["epic_names"].size(), 2);
        for (int i = 0; i < n; ++i)
            w.epic_names[i] = j["epic_names"][i].get<std::string>();
    }
    // G9.2: Legendary name (fixed)
    w.legendary_name = j.value("legendary_name", "");

    // G9.2: Affix
    if (j.contains("affix") && j["affix"].is_object()) {
        w.affix.type  = j["affix"].value("type", "");
        w.affix.value = j["affix"].value("value", 0.0f);
    }
    // G9.2: Legendary effect
    w.legendary_effect = j.value("legendary_effect", "");
}

// ── Parse a full WeaponDef from JSON object (分段解析) ──
static WeaponDef _parse_weapon(const json& j) {
    WeaponDef w;
    _parse_weapon_base(j, w);
    _parse_weapon_stages(j, w);
    _parse_weapon_naming(j, w);
    return w;
}


// ── Load from file ──
bool load_weapon_defs(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        printf("[WEAPON_DEF] ERROR: cannot open %s\n", json_path.c_str());
        return false;
    }
    json j;
    try { f >> j; } catch (const std::exception& e) {
        printf("[WEAPON_DEF] JSON error: %s\n", e.what());
        return false;
    }

    if (!j.contains("weapons") || !j["weapons"].is_array()) {
        printf("[WEAPON_DEF] root must have \"weapons\" array\n");
        return false;
    }

    int count = 0;
    for (auto& entry : j["weapons"]) {
        WeaponDef w = _parse_weapon(entry);
        if (w.id.empty()) continue;
        g_weapon_defs[w.id] = w;
        count++;
    }

    g_weapon_loaded = true;
    printf("[WEAPON_DEF] Loaded %d weapons from %s\n", count, json_path.c_str());
    return true;
}

// ── Load from in-memory JSON (mod support) ──
int load_weapon_defs_from_json(const char* json_text, MergeMode mode,
                                const char* id_namespace) {
    if (!json_text) return 0;
    int count = 0;
    try {
        json j = json::parse(json_text);
        if (!j.contains("weapons") || !j["weapons"].is_array()) return 0;
        for (auto& entry : j["weapons"]) {
            WeaponDef w = _parse_weapon(entry);
            if (w.id.empty()) continue;
            std::string key = id_namespace
                ? std::string(id_namespace) + ":" + w.id
                : w.id;
            if (mode == MergeMode::Skip && g_weapon_defs.count(key))
                continue;
            w.id = key;
            g_weapon_defs[key] = w;
            count++;
        }
    } catch (const std::exception& e) {
        printf("[WEAPON_DEF] parse error: %s\n", e.what());
    }
    g_weapon_loaded = true;
    return count;
}

// ── Lookup ──
const WeaponDef* get_weapon_def(const std::string& id) {
    auto it = g_weapon_defs.find(id);
    return (it != g_weapon_defs.end()) ? &it->second : nullptr;
}

std::vector<const WeaponDef*> get_weapon_defs_for_type(WeaponType wt) {
    std::vector<const WeaponDef*> result;
    for (auto& kv : g_weapon_defs)
        if (kv.second.type == wt)
            result.push_back(&kv.second);
    return result;
}

const std::unordered_map<std::string, WeaponDef>& get_all_weapon_defs() {
    return g_weapon_defs;
}

bool is_weapon_defs_loaded() { return g_weapon_loaded; }
