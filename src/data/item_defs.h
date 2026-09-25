#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "core/registry_provider.h"  // G4.1: MergeMode

// ============================================================
// G3.3: ItemDef — 道具配置数据 (纯数据, 不含 apply/use 行为)
// EquipmentItem / ConsumableItem / CharmItem 类保留 C++
// 属于 Data 层, 位于 src/data/
// ============================================================

// ── 稀有度配置 ──
// 必须带默认值: 零初始化的 weights 会让 random_rarity() 的 total 为 0 → rng() % 0 除零 SIGFPE。
// JSON 的 rarity 块只写「数组里出现的下标」, 数组不足 4 项时未列出的槽位必须保有默认值兜底。
struct RarityConfig {
    float mults[4]   = {1.0f, 1.2f, 1.5f, 2.0f};
    int   weights[4] = {60, 25, 12, 3};
};

// ── 道具模板 ──
struct ItemDef {
    std::string id;              // "short_sword" | "leather_armor" | ...
    std::string category;        // "weapon" | "armor" | "potion" | "charm"
    std::string name;            // 显示名 "短剑"

    // weapon/armor: 随机范围
    int atk_min = 5, atk_max = 12;
    int pdef_min = 0, pdef_max = 3;
    int mdef_min = 0, mdef_max = 4;

    // potion
    std::string effect_type;     // "heal" | "buff"
    int heal_min = 15, heal_max = 40;
    std::string buff_id;         // "attack_up" | ...

    // charm
    std::string skill_id;        // "slash" | "fireball" | ...
    float cd_bonus = 0.15f;
    float power_bonus = 0.40f;
};

// ── 全局注册表 ──
bool load_item_defs(const std::string& json_path);
int  load_item_defs_from_json(const char* json_text, MergeMode mode,
                               const char* id_namespace = nullptr);  // G4.2: namespace
const RarityConfig& get_rarity_config();
const ItemDef* get_item_def(const std::string& id);
const std::unordered_map<std::string, ItemDef>& get_all_item_defs();
bool is_item_defs_loaded();
