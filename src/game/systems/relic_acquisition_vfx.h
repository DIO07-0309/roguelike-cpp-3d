#pragma once
#include <string>
#include <vector>
#include "types/combat_types.h"   // Effect

// Presentation-only: turns a relic acquisition into a one-shot VFX演出.
// Pure output into `active_effects`; no gameplay / RNG / save access.
class RelicAcquisitionVfx {
public:
    struct Mapping {
        std::string recipe;
        int accent[3] = {255, 255, 255};
        std::string label;
    };

    static bool load(const std::string& json_path);
    static void install();                       // load mapping + register hook (idempotent)
    static void uninstall();                     // clear hook + pending (tests / teardown)
    static void process_queue(std::vector<Effect>& out);  // drain pending → effects
    static std::size_t mapping_count();
    static std::size_t pending_count();
    static bool has_mapping(const std::string& id);
};
