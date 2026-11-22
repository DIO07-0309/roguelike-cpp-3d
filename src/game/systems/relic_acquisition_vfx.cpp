#include "relic_acquisition_vfx.h"
#include "relic_vfx_hook.h"
#include "vfx_server.h"
#include "director/presentation_system_director.h"
#include "core/service_locator.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <fstream>

using json = nlohmann::json;

namespace {

struct Pending { std::string id; float px = 0, py = 0; };

std::unordered_map<std::string, RelicAcquisitionVfx::Mapping> g_map;
std::vector<Pending> g_pending;

void _on_acquired(const std::string& id, float px, float py) {
    g_pending.push_back({id, px, py});
}

// Blend an effect's color toward the relic accent (75% original / 25% accent).
void _tint(Effect& fx, const int accent[3]) {
    auto mix = [](unsigned char base, int acc) -> unsigned char {
        return static_cast<unsigned char>(base * 0.75 + acc * 0.25);
    };
    fx.color.r = mix(fx.color.r, accent[0]);
    fx.color.g = mix(fx.color.g, accent[1]);
    fx.color.b = mix(fx.color.b, accent[2]);
}

}  // namespace

bool RelicAcquisitionVfx::load(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        LOG_WARN("[RELIC_VFX] cannot open %s", json_path.c_str());
        return false;
    }
    json j;
    try { f >> j; } catch (const std::exception& e) {
        LOG_WARN("[RELIC_VFX] JSON error: %s", e.what());
        return false;
    }
    g_map.clear();
    for (auto& [id, entry] : j.items()) {
        if (id.rfind("_", 0) == 0) continue;  // skip _comment / _count
        Mapping m;
        m.recipe = entry.value("recipe", "");
        if (entry.contains("accent") && entry["accent"].is_array()) {
            auto a = entry["accent"];
            for (int i = 0; i < 3 && i < static_cast<int>(a.size()); i++)
                m.accent[i] = a[i].get<int>();
        }
        m.label = entry.value("label", "");
        g_map[id] = m;
    }
    return !g_map.empty();
}

void RelicAcquisitionVfx::install() {
    load("resources/relic_acquisition_vfx.json");
    set_relic_vfx_hook(&_on_acquired);
}

void RelicAcquisitionVfx::uninstall() {
    clear_relic_vfx_hook();
    g_pending.clear();
}

void RelicAcquisitionVfx::process_queue(std::vector<Effect>& out) {
    if (g_pending.empty()) return;
    for (const auto& p : g_pending) {
        auto it = g_map.find(p.id);
        if (it == g_map.end()) continue;  // un-mapped relic → no演出
        const Mapping& m = it->second;
        VFXServer vfx;
        std::size_t before = vfx.effects.size();
        vfx.play_recipe(m.recipe.c_str(), p.px, p.py);
        for (std::size_t i = before; i < vfx.effects.size(); i++)
            _tint(vfx.effects[i], m.accent);
        for (auto& fx : vfx.effects) out.push_back(fx);
        auto* pres = ServiceLocator::get<PresentationSystemDirector>();
        if (pres && !m.label.empty())
            pres->spawn_label(p.px, p.py - 20, m.label.c_str(),
                              {255, 230, 160, 255}, 1.1f);
    }
    g_pending.clear();
}

std::size_t RelicAcquisitionVfx::mapping_count() { return g_map.size(); }
std::size_t RelicAcquisitionVfx::pending_count() { return g_pending.size(); }

bool RelicAcquisitionVfx::has_mapping(const std::string& id) {
    return g_map.find(id) != g_map.end();
}
