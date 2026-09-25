#include "item.h"
#include "player.h"
#include "skill.h"
#include "combat_system.h"
#include "data/item_defs.h"    // G3.3
#include "data/weapon_defs.h"  // G9
#include <vector>

// ---- 从 item.h 移出的实现 ----

// G3.3: rarity data from registry (替代硬编码数组)
float rarity_mult(Rarity r) {
    auto& rc = get_rarity_config();
    int idx = static_cast<int>(r);
    return (idx >= 0 && idx < 4) ? rc.mults[idx] : 1.0f;
}

Color rarity_color(Rarity r) {
    // 表现层: 4 色映射 (保留 C++ — 直接引用 Raylib Color)
    Color c[] = {
        {180, 180, 180, 255},  // COMMON
        {80, 160, 255, 255},   // RARE
        {180, 80, 255, 255},   // EPIC
        {255, 140, 40, 255},   // LEGENDARY
    };
    return c[static_cast<int>(r)];
}

std::string rarity_label(Rarity r) {
    const char* l[] = {"普通", "稀有", "史诗", "传说"};
    return l[static_cast<int>(r)];
}

Item::Item(const std::string& name, Rarity r, const std::string& tc)
    : base_name(name), rarity(r), tile_char(tc), color(rarity_color(r)) {}

std::string Item::get_description() const {
    return rarity_label(rarity) + " " + base_name;
}

EquipmentItem::EquipmentItem(const std::string& name, Rarity r, const std::string& sl,
                             int atk, int pdef, int mdef, bool rarity_scaled)
    : Item(name, r, (sl == "weapon" ? "W" : sl == "armor" ? "A" : "C")),
      slot(sl) {
    float m = rarity_scaled ? rarity_mult(r) : 1.0f;
    // G9-fix: weapon ATK is pre-computed from WeaponDef base_damage (already tier-differentiated)
    // Only apply rarity_mult to armor defenses, NOT to weapon atk
    atk_bonus = (sl == "weapon") ? std::max(1, atk)
               : std::max(1, (int)(atk * m));
    pdef_bonus = std::max(1, (int)(pdef * m));
    mdef_bonus = std::max(1, (int)(mdef * m));
}

CharmItem::CharmItem(const std::string& name, Rarity r, const std::string& sk,
                     float cd, float pw)
    : EquipmentItem(name, r, "charm", 0, 0, 0),
      skill_class_name(sk), cd_bonus(cd), power_bonus(pw) {
    tile_char = "C";
}

ConsumableItem::ConsumableItem(const std::string& name, Rarity r,
                               const std::string& type, int val,
                               const std::string& buf)
    : Item(name, r, "P"), effect_type(type),
      effect_value(std::max(1, (int)(val * rarity_mult(r)))),
      buff_id(buf) {
    if (!buf.empty()) triggers = {{buf, 1, 1.0f, BuffTarget::SELF}};
}

std::string ConsumableItem::get_description() const {
    if (!triggers.empty())
        return rarity_label(rarity) + " " + base_name + " (" + get_buff_display_name(triggers[0].buff_id) + " 6s)";
    if (effect_type == "heal")
        return rarity_label(rarity) + " " + base_name + " (恢复" + std::to_string(effect_value) + " HP)";
    return Item::get_description();
}

// ---- 原有实现 ----

Rarity random_rarity() {
    // G3.3: weights from registry (替代硬编码 {60,25,12,3})
    auto& rc = get_rarity_config();
    int total = 0;
    for (int i = 0; i < 4; i++) total += rc.weights[i];
    if (total <= 0) return Rarity::COMMON;  // 配置缺失或权重全 0 时兜底, 防 rng() % 0 除零
    int roll = (int)(rng() % total);
    int sum = 0;
    for (int i = 0; i < 4; i++) {
        sum += rc.weights[i];
        if (roll < sum) return (Rarity)i;
    }
    return Rarity::COMMON;
}

void EquipmentItem::apply(Player* player) {
    auto& mod = player->combat.modifiers;
    mod["atk_flat"] = (mod.count("atk_flat") ? mod["atk_flat"] : 0) + atk_bonus;
    if (pdef_bonus > 0) mod["pdef_flat"] = (mod.count("pdef_flat") ? mod["pdef_flat"] : 0) + pdef_bonus;
    if (mdef_bonus > 0) mod["mdef_flat"] = (mod.count("mdef_flat") ? mod["mdef_flat"] : 0) + mdef_bonus;
}

void EquipmentItem::remove(Player* player) {
    auto& mod = player->combat.modifiers;
    mod["atk_flat"] = mod["atk_flat"] - atk_bonus;
    if (pdef_bonus > 0) mod["pdef_flat"] = mod["pdef_flat"] - pdef_bonus;
    if (mdef_bonus > 0) mod["mdef_flat"] = mod["mdef_flat"] - mdef_bonus;
}

std::string EquipmentItem::get_description() const {
    std::string d = rarity_label(rarity) + " " + base_name + " (";
    if (atk_bonus) d += "ATK+" + std::to_string(atk_bonus) + " ";
    if (pdef_bonus) d += "PD+" + std::to_string(pdef_bonus) + " ";
    if (mdef_bonus) d += "MD+" + std::to_string(mdef_bonus) + " ";
    d += ")";
    return d;
}

void CharmItem::apply(Player* player) {
    for (auto& s : player->skills.active_skills) {
        // G3.2: _skill_id 匹配 (G3.3 完整迁移时清理 || 1 hack)
        if (s->_skill_id == skill_class_name || 1) {
            s->apply_charm(cd_bonus, power_bonus);
            break;
        }
    }
}

void CharmItem::remove(Player* player) {
    for (auto& s : player->skills.active_skills) {
        s->remove_charm();
        break;
    }
}

std::string CharmItem::get_description() const {
    std::string d = rarity_label(rarity) + " " + base_name + " (";
    if (cd_bonus > 0) d += "冷却-" + std::to_string((int)(cd_bonus*100)) + "% ";
    if (power_bonus > 0) d += "威力+" + std::to_string((int)(power_bonus*100)) + "%";
    d += ")";
    return d;
}

std::string ConsumableItem::use(Player* player) {
    if (!triggers.empty()) {
        apply_triggers_self(triggers, player);
        return "使用 " + get_description() + "，属性提升！";
    }
    if (effect_type == "heal") {
        int recovered = player->combat.heal(effect_value);
        return "使用 " + get_description() + "，恢复了 " + std::to_string(recovered) + " HP";
    }
    return "使用了 " + get_description();
}

// ---- G9: pick random WeaponDef by rarity ──
static const WeaponDef* _random_weapon_def(Rarity r, int roll) {
    static const char* names[] = {
        "dagger", "sword", "nunchaku", "crossbow", "spear"
    };
    int idx = roll % 5;
    const char* tier = (r == Rarity::LEGENDARY) ? "legendary"
                     : (r == Rarity::EPIC) ? "epic"
                     : (r == Rarity::RARE) ? "rare" : "common";
    char buf[64];
    snprintf(buf, sizeof(buf), "%s_%s", names[idx], tier);
    const WeaponDef* def = get_weapon_def(buf);
    return def ? def : get_weapon_def("fist_basic");
}

// ---- G3.3: ItemFactory — 从 registry 随机生成 ----
std::shared_ptr<Item> generate_random_item() {
    Rarity r = random_rarity();
    // 收集 registry 中所有模板, 按 category 分组
    std::vector<const ItemDef*> weapons, armors, potions, charms;
    for (auto& kv : get_all_item_defs()) {
        auto& d = kv.second;
        if (d.category == "weapon") weapons.push_back(&d);
        else if (d.category == "armor") armors.push_back(&d);
        else if (d.category == "potion") potions.push_back(&d);
        else if (d.category == "charm") charms.push_back(&d);
    }
    // Q3.3 平衡: 装备权重 71% (武/甲各35.7%), 药水/护符各14.3% — 装备为主但保留续航 (原四类等权)
    std::vector<int> cats;
    if (!weapons.empty()) cats.insert(cats.end(), 5, 0);
    if (!armors.empty())  cats.insert(cats.end(), 5, 1);
    if (!potions.empty()) cats.insert(cats.end(), 2, 2);
    if (!charms.empty())  cats.insert(cats.end(), 2, 3);
    if (cats.empty()) return nullptr;
    int cat = cats[rng() % cats.size()];

    if (cat == 0) {
        // G9: WeaponDef base_damage already encodes tier scaling — no rarity_mult double-dip
        int wroll = rng() % 100;
        const WeaponDef* wdef = _random_weapon_def(r, wroll);
        int tier_idx = (int)r;
        const char* display_name = pick_weapon_name(wdef, tier_idx);
        int atk = (int)(wdef->base_damage);
        if (atk < 1) atk = 1;
        auto item = std::make_shared<EquipmentItem>(display_name, r, "weapon", atk);
        item->weapon_def_id = wdef->id;
        return item;
    }
    if (cat == 1) {
        auto& t = armors[rng() % armors.size()];
        int pd = t->pdef_min + (int)(rng() % (t->pdef_max - t->pdef_min + 1));
        int md = t->mdef_min + (int)(rng() % (t->mdef_max - t->mdef_min + 1));
        return std::make_shared<EquipmentItem>(t->name, r, "armor", 0, pd, md);
    }
    if (cat == 2) {
        auto& t = potions[rng() % potions.size()];
        if (t->effect_type == "buff" && !t->buff_id.empty())
            return std::make_shared<ConsumableItem>(t->name, r, "buff", 1, t->buff_id);
        int heal = t->heal_min + (int)(rng() % (t->heal_max - t->heal_min + 1));
        return std::make_shared<ConsumableItem>(t->name, r, "heal", heal);
    }
    // charm
    auto& t = charms[rng() % charms.size()];
    float m = rarity_mult(r);
    return std::make_shared<CharmItem>(t->name, r, t->skill_id,
                                       t->cd_bonus * m, t->power_bonus * m);
}

// G3.3: generate_charm_for_skill — registry 查询 (替代硬编码 4 条 if-else)
std::shared_ptr<CharmItem> generate_charm_for_skill(const std::string& skill, Rarity r) {
    // Search registry for a charm template matching this skill_id
    for (auto& kv : get_all_item_defs()) {
        auto& d = kv.second;
        if (d.category != "charm") continue;
        if (d.skill_id == skill || d.skill_id == skill) {
            float m = rarity_mult(r);
            return std::make_shared<CharmItem>(d.name, r, d.skill_id,
                                               d.cd_bonus * m, d.power_bonus * m);
        }
    }
    return nullptr;
}

std::shared_ptr<CharmItem> generate_random_charm() {
    // Collect all charm templates from registry
    std::vector<const ItemDef*> charms;
    for (auto& kv : get_all_item_defs()) {
        if (kv.second.category == "charm") charms.push_back(&kv.second);
    }
    if (charms.empty()) return nullptr;
    auto& t = charms[rng() % charms.size()];
    float m = rarity_mult(Rarity::COMMON);
    return std::make_shared<CharmItem>(t->name, Rarity::COMMON, t->skill_id,
                                       t->cd_bonus * m, t->power_bonus * m);
}

// M4f.13: 物品图标派生 — 武器走 weapon_sprite_key, 装甲按名字, 药水按效果, 护符通用
const char* item_icon_key(const Item* item) {
    if (!item) return nullptr;
    auto* eq = dynamic_cast<const EquipmentItem*>(item);
    if (eq) {
        if (eq->slot == "weapon") {
            const WeaponDef* wdef = get_weapon_def(eq->weapon_def_id);
            return wdef ? weapon_sprite_key(wdef->type) : nullptr;
        }
        if (eq->slot == "charm") return "item_charm";
        if (eq->slot == "armor") {
            static const char* MAP[][2] = {
                {"皮甲", "item_armor_leather"}, {"锁子", "item_armor_chain"},
                {"铁铠", "item_armor_iron"},    {"鳞甲", "item_armor_scale"},
                {"板甲", "item_armor_plate"},   {"符文", "item_armor_rune"},
                {"冰霜", "item_armor_ice"},     {"暗影", "item_armor_robe"},
            };
            for (auto& kv : MAP)
                if (item->base_name.find(kv[0]) != std::string::npos)
                    return kv[1];
            return "item_armor_leather";
        }
    }
    auto* c = dynamic_cast<const ConsumableItem*>(item);
    if (c) return (c->effect_type == "buff") ? "item_potion_blue" : "item_potion_red";
    return nullptr;
}

// Batch 3A: 出售价格计算
int get_sell_value(const Item* item) {
    if (!item) return 0;
    int rarity_idx = static_cast<int>(item->rarity);
    if (auto* eq = dynamic_cast<const EquipmentItem*>(item)) {
        (void)eq;
        return 10 + rarity_idx * 8;  // COMMON=10, RARE=18, EPIC=26, LEGENDARY=34
    }
    if (auto* cn = dynamic_cast<const ConsumableItem*>(item)) {
        (void)cn;
        return 5 + rarity_idx * 4;   // COMMON=5, RARE=9, EPIC=13, LEGENDARY=17
    }
    return 0;
}
