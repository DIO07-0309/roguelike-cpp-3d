// G6.5/G6.7: Encounter loader
#include "world/encounter.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>
#include <cstdlib>

using json = nlohmann::json;
std::vector<EncounterDef> g_encounters;
std::unordered_map<std::string, EncounterDef*> g_encounter_by_id;
std::unordered_map<std::string, std::vector<EncounterDef*>> g_encounters_by_biome;

static std::vector<EncounterChoice> _parse_encounter_choices(const json& n) {
    std::vector<EncounterChoice> choices;
    if (!n.contains("choices")) return choices;
    for (auto& c : n["choices"]) {
        EncounterChoice ch;
        ch.text = c["text"].get<std::string>();
        ch.next = c.value("next", "end");
        ch.effect = c.value("effect", "none");
        ch.risk = c.value("risk", "none");
        choices.push_back(ch);
    }
    return choices;
}

static EncounterNode _parse_encounter_node(const json& n) {
    EncounterNode node;
    node.id = n["id"].get<std::string>();
    node.text = n.value("text", "");
    node.type = n.value("type", "dialogue");
    node.trade_cost = n.value("trade_cost", "");
    if (n.contains("trade_items"))
        for (auto& ti : n["trade_items"]) node.trade_items.push_back(ti.get<std::string>());
    node.choices = _parse_encounter_choices(n);
    return node;
}

static EncounterDef _parse_encounter(const json& obj) {
    EncounterDef enc;
    enc.id = obj["id"].get<std::string>();
    enc.type = obj["type"].get<std::string>();
    enc.name = obj["name"].get<std::string>();
    enc.biome = obj["biome"].get<std::string>();
    enc.trigger = obj.value("trigger", "floor_enter");
    enc.room = obj.value("room", "");
    enc.repeatable = obj.value("repeatable", true);
    enc.narrative = obj.value("narrative", "");
    if (obj.contains("conditions"))
        for (auto& c : obj["conditions"]) enc.conditions.push_back(c.get<std::string>());
    if (obj.contains("dialogue"))
        for (auto& n : obj["dialogue"]) enc.dialogue.push_back(_parse_encounter_node(n));
    return enc;
}

// 建索引 — id 查找 + biome 分组
static void _index_encounters() {
    for (size_t i = 0; i < g_encounters.size(); ++i) {
        EncounterDef* ep = &g_encounters[i];
        g_encounter_by_id[ep->id] = ep;
        g_encounters_by_biome[ep->biome].push_back(ep);
    }
}

bool load_encounter_defs(const char* json_path) {
    try {
        std::ifstream f(json_path);
        if (!f.is_open()) { printf("[Encounter] Cannot open %s\n", json_path); return false; }
        json data = json::parse(f);
        g_encounters.clear(); g_encounter_by_id.clear(); g_encounters_by_biome.clear();
        for (auto& obj : data) g_encounters.push_back(_parse_encounter(obj));
        _index_encounters();
        printf("[Encounter] Loaded %zu encounters across %zu biomes\n",
               g_encounters.size(), g_encounters_by_biome.size());
        return true;
    } catch (const std::exception& e) {
        printf("[Encounter] Error: %s\n", e.what()); return false;
    }
}


EncounterDef* get_encounter(const std::string& eid) {
    auto it = g_encounter_by_id.find(eid);
    return it != g_encounter_by_id.end() ? it->second : nullptr;
}

EncounterDef* pick_encounter_for_biome(const std::string& biome_id) {
    auto it = g_encounters_by_biome.find(biome_id);
    if (it == g_encounters_by_biome.end() || it->second.empty()) return nullptr;
    int idx = rand() % (int)it->second.size();
    return it->second[idx];
}

EncounterDef* pick_encounter_by_trigger(const std::string& biome_id, const std::string& trigger) {
    auto it = g_encounters_by_biome.find(biome_id);
    if (it == g_encounters_by_biome.end()) return nullptr;
    std::vector<EncounterDef*> pool;
    for (auto* e : it->second) if (e->trigger == trigger) pool.push_back(e);
    if (pool.empty()) return nullptr;
    return pool[rand() % (int)pool.size()];
}
