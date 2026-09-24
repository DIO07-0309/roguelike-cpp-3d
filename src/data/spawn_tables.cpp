// A6-S2 批次9: Spawn tables loader
#include "spawn_tables.h"
#include "combat_system.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

std::vector<SpawnSlot> g_spawn_slots;
std::vector<std::pair<std::string, std::vector<std::string>>> g_spawn_aliases;
std::string g_spawn_default = "slime";
std::vector<ChallengeBiomePool> g_challenge_pools;

static SpawnCandidate _parse_candidate(const json& obj) {
    SpawnCandidate c;
    c.id = obj["id"].get<std::string>();
    c.weight = obj.value("weight", 1);
    if (obj.contains("floors")) {
        c.floor_start = obj["floors"][0].get<int>();
        c.floor_end = obj["floors"][1].get<int>();
    }
    return c;
}

bool load_spawn_slots(const char* json_path) {
    try {
        std::ifstream f(json_path);
        if (!f.is_open()) { printf("[SpawnSlots] Cannot open %s\n", json_path); return false; }
        json data = json::parse(f);
        g_spawn_slots.clear();
        g_spawn_aliases.clear();
        g_spawn_default = data.value("default", std::string("slime"));
        if (data.contains("aliases")) {
            for (auto it = data["aliases"].begin(); it != data["aliases"].end(); ++it) {
                std::vector<std::string> targets;
                for (auto& t : it.value()) targets.push_back(t.get<std::string>());
                g_spawn_aliases.emplace_back(it.key(), std::move(targets));
            }
        }
        if (!data.contains("slots")) return false;
        for (auto& obj : data["slots"]) {
            SpawnSlot s;
            s.archetype = obj.value("archetype", std::string());
            if (!obj.contains("candidates")) continue;
            for (auto& c : obj["candidates"]) s.candidates.push_back(_parse_candidate(c));
            g_spawn_slots.push_back(std::move(s));
        }
        printf("[SpawnSlots] Loaded %zu slots, %zu aliases\n",
               g_spawn_slots.size(), g_spawn_aliases.size());
        return true;
    } catch (const std::exception& e) {
        printf("[SpawnSlots] Error: %s\n", e.what());
        return false;
    }
}

bool load_challenge_pools(const char* json_path) {
    try {
        std::ifstream f(json_path);
        if (!f.is_open()) { printf("[ChallengePools] Cannot open %s\n", json_path); return false; }
        json data = json::parse(f);
        g_challenge_pools.clear();
        if (!data.contains("biomes")) return false;
        for (auto& obj : data["biomes"]) {
            ChallengeBiomePool p;
            if (obj.contains("floors")) {
                p.floor_start = obj["floors"][0].get<int>();
                p.floor_end = obj["floors"][1].get<int>();
            }
            if (obj.contains("waves")) {
                for (auto& w : obj["waves"]) {
                    std::vector<std::string> list;
                    for (auto& t : w) list.push_back(t.get<std::string>());
                    p.waves.push_back(std::move(list));
                }
            }
            g_challenge_pools.push_back(std::move(p));
        }
        printf("[ChallengePools] Loaded %zu biome pools\n", g_challenge_pools.size());
        return true;
    } catch (const std::exception& e) {
        printf("[ChallengePools] Error: %s\n", e.what());
        return false;
    }
}

const SpawnSlot* get_spawn_slot(int slot_index) {
    if (slot_index < 0 || slot_index >= (int)g_spawn_slots.size()) return nullptr;
    return &g_spawn_slots[(size_t)slot_index];
}

const std::string* pick_slot_monster(int slot_index, int floor) {
    const SpawnSlot* s = get_spawn_slot(slot_index);
    if (!s || s->candidates.empty()) return nullptr;
    std::vector<const SpawnCandidate*> elig;
    uint64_t total = 0;
    for (const auto& c : s->candidates) {
        if (floor < c.floor_start || floor > c.floor_end) continue;
        elig.push_back(&c);
        total += (uint64_t)c.weight;
    }
    if (elig.empty() || total == 0) return nullptr;
    if (elig.size() == 1) return &elig[0]->id;   // 单候选不掷骰, 逐位等价关键
    uint64_t roll = rng() % (uint64_t)total;
    uint64_t acc = 0;
    for (const auto* c : elig) {
        acc += (uint64_t)c->weight;
        if (roll < acc) return &c->id;
    }
    return &elig.back()->id;
}

const std::string* pick_challenge_monster(int floor, int wave, uint32_t rng) {
    for (const auto& p : g_challenge_pools) {
        if (floor < p.floor_start || floor > p.floor_end) continue;
        if (wave < 0 || wave >= (int)p.waves.size()) return nullptr;
        const auto& list = p.waves[(size_t)wave];
        if (list.empty()) return nullptr;
        return &list[rng % list.size()];
    }
    return nullptr;
}
