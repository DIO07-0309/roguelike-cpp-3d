#include "special_room.h"
#include "player.h"
#include "combat_system.h"
#include "item.h"
#include "core/logger.h"
#include "build_score.h"
#include "relic_progression.h"
#include "reward_manager.h"
#include <algorithm>

SpecialRoomType special_room_from_index(int idx) {
    switch (idx % 9) {
        case 0: return SpecialRoomType::ALTAR;
        case 1: return SpecialRoomType::TREASURE;
        case 2: return SpecialRoomType::FOUNTAIN;
        case 3: return SpecialRoomType::SHOP;
        case 4: return SpecialRoomType::BLACKSMITH;
        case 5: return SpecialRoomType::LIBRARY;
        case 6: return SpecialRoomType::GAMBLER;
        case 7: return SpecialRoomType::SHRINE;
        default: return SpecialRoomType::SECRET;
    }
}
SpecialRoomType special_room_from_challenge() {
    return SpecialRoomType::CHALLENGE;
}

std::string special_room_to_string(SpecialRoomType type) {
    switch (type) {
        case SpecialRoomType::ALTAR:      return "altar";
        case SpecialRoomType::TREASURE:   return "treasure";
        case SpecialRoomType::FOUNTAIN:   return "fountain";
        case SpecialRoomType::SHOP:       return "shop";
        case SpecialRoomType::BLACKSMITH: return "blacksmith";
        case SpecialRoomType::LIBRARY:    return "library";
        case SpecialRoomType::GAMBLER:    return "gambler";
        case SpecialRoomType::SHRINE:     return "shrine";
        case SpecialRoomType::SECRET:     return "secret";
        case SpecialRoomType::CHALLENGE:  return "challenge";
    }
    return "unknown";
}

SpecialRoomType special_room_from_string(const std::string& s) {
    if (s == "altar")      return SpecialRoomType::ALTAR;
    if (s == "treasure")   return SpecialRoomType::TREASURE;
    if (s == "fountain")   return SpecialRoomType::FOUNTAIN;
    if (s == "shop")       return SpecialRoomType::SHOP;
    if (s == "blacksmith") return SpecialRoomType::BLACKSMITH;
    if (s == "library")    return SpecialRoomType::LIBRARY;
    if (s == "gambler")    return SpecialRoomType::GAMBLER;
    if (s == "shrine")     return SpecialRoomType::SHRINE;
    if (s == "secret")     return SpecialRoomType::SECRET;
    if (s == "challenge")  return SpecialRoomType::CHALLENGE;
    return SpecialRoomType::ALTAR;
}

// ============================================================
// B9 共享辅助: 净化负面 Buff (poison / slow)
// ============================================================
static void _cleanse_debuffs(Player* player) {
    auto& buffs = player->active_buffs;
    buffs.erase(std::remove_if(buffs.begin(), buffs.end(),
        [](const BuffInstance& b) {
            return b.id == "poison" || b.id == "slow";
        }), buffs.end());
}

// ============================================================
// B9.1 祭坛: 4 结果池 (等概率随机)
// ============================================================
static std::string _altar_attack_up(Player* player) {
    apply_buff(player, "attack_up", 1);
    return "祭坛赐福：你的攻击力提升了。";
}

static std::string _altar_heal(Player* player) {
    int eff_max = get_effective_max_hp(player);  // B11: blood_charm
    int amount = std::max(1, eff_max * 3 / 10);
    // B12: sage_leaf — 祭坛治疗 +10
    if (player_has_relic(player, "sage_leaf")) amount += 10;
    // D8: healing_herb — 所有治疗 +15%
    if (player_has_relic(player, "healing_herb")) amount = (int)(amount * 1.15f);
    heal_player(player, amount);  // B11: 使用 effective max clamp
    return "祭坛赐福：你的伤势恢复了。";
}

static std::string _altar_blood_power(Player* player) {
    int loss = std::max(1, player->combat.current_hp * 2 / 10);
    if (player->combat.current_hp - loss < 1)
        loss = player->combat.current_hp - 1;
    if (loss > 0) player->combat.take_damage(loss);
    apply_buff(player, "attack_up", 2);
    return "祭坛夺取了你的生命，但赐予了更强的力量。";
}

static std::string _altar_cleanse(Player* player) {
    _cleanse_debuffs(player);
    return "祭坛的光辉净化了你身上的负面状态。";
}

static std::string _exec_altar(Player* player) {
    using Fn = std::string(*)(Player*);
    static const Fn pool[] = {
        _altar_attack_up,
        _altar_heal,
        _altar_blood_power,
        _altar_cleanse,
    };
    int idx = rng() % 4;
    return pool[idx](player);
}

// ============================================================
// B12: rarity 权重 relic 抽取 (common:100, rare:40, epic:10, legendary:3)
static int _rarity_weight(const std::string& rarity) {
    if (rarity == "common")    return 100;
    if (rarity == "rare")      return 40;
    if (rarity == "epic")      return 10;
    if (rarity == "legendary") return 3;
    return 100;
}

// B12: 宝箱房 relic 掉落 — 按 rarity 权重抽取, 支持 drop_chance 参数
// B12: 宝箱房 relic 掉落档位 — rarity 权重 + 候选 id 池
struct RelicRaritySlot { std::string rarity; int weight; std::vector<std::string> ids; };

// 收集各 rarity 档位的未持有 relic; out_total_w = 加权前总权重
// (注意: BuildScore 加权只改 slot.weight, 不回写 total_w — 与原始实现一致)
static std::vector<RelicRaritySlot> _collect_relic_slots(
        Player* player, const std::vector<std::string>& all_ids, int& out_total_w) {
    RelicRaritySlot slots[] = {
        {"common", 100, {}}, {"rare", 40, {}}, {"epic", 10, {}}, {"legendary", 3, {}}};
    out_total_w = 0;
    for (auto& slot : slots) {
        for (auto& id : all_ids) {
            if (player_has_relic(player, id)) continue;
            const RelicDef* def = get_relic_def(id);
            if (def && def->rarity == slot.rarity) slot.ids.push_back(id);
        }
        if (!slot.ids.empty()) out_total_w += slot.weight;
    }
    return {slots, slots + 4};
}

// D3 Step4: 根据 BuildScore 调整 relic 权重 (匹配 +60%)
static void _boost_build_matched_slots(Player* player, std::vector<RelicRaritySlot>& slots) {
    const BuildScore bs = calculate_build(player);
    for (auto& slot : slots) {
        std::vector<std::string> matched;
        for (auto& id : slot.ids)
            if (relic_matches_build(bs, id)) matched.push_back(id);
        if (!matched.empty()) slot.weight = slot.weight * 8 / 5;
    }
}

// 槽内抽取 — D3 Step4: 匹配 Build 的 relic 概率翻倍 (40%)
static std::string _roll_relic_in_slot(Player* player, const RelicRaritySlot& slot) {
    const std::vector<std::string>& pool = slot.ids;
    std::vector<std::string> build_ids;
    const BuildScore bs = calculate_build(player);
    for (auto& id : pool)
        if (relic_matches_build(bs, id)) build_ids.push_back(id);
    if (!build_ids.empty() && (rng() % 100) < 40)
        return build_ids[(int)(rng() % (uint32_t)build_ids.size())];
    return pool[(int)(rng() % (uint32_t)pool.size())];
}

// fallback: 因某种原因没选中, 从所有未持有候选中随机
static std::string _fallback_relic_pick(Player* player,
                                        const std::vector<std::string>& all_ids) {
    std::vector<std::string> all_candidates;
    for (auto& id : all_ids)
        if (!player_has_relic(player, id)) all_candidates.push_back(id);
    if (all_candidates.empty()) return "";
    return all_candidates[(int)(rng() % (uint32_t)all_candidates.size())];
}

// 授予 relic — C1 仪式感消息 + D4.6 永久 Archive/熟练度记录
static std::string _grant_relic(Player* player, const std::string& chosen) {
    player->add_relic(chosen, PersistenceScope::FLOOR);
    const RelicDef* def = get_relic_def(chosen);
    std::string name = def ? def->name : chosen;
    LOG_INFO("[RELIC] 获得圣物: %s (%s)", name.c_str(), def ? def->rarity.c_str() : "?");
    if (def) g_relic_archive.mark_obtained(chosen, rarity_level(def->rarity));
    return "RELIC:" + name;   // GameScene 会识别 RELIC: 前缀显示大提示
}

// B12: 宝箱房 relic 掉落 — 按 rarity 权重抽取, 支持 drop_chance 参数
static std::string _try_grant_random_relic(Player* player, float drop_chance) {
    if (!player) return "";
    if ((int)(rng() % 1000) >= (int)(drop_chance * 1000.0f)) return "";

    const auto all_ids = get_all_relic_ids();
    int total_w = 0;
    std::vector<RelicRaritySlot> slots = _collect_relic_slots(player, all_ids, total_w);
    _boost_build_matched_slots(player, slots);

    if (total_w == 0) return "";

    int roll = (int)(rng() % (uint32_t)total_w);
    std::string chosen;
    for (auto& slot : slots) {
        if (slot.ids.empty()) continue;
        if (roll < slot.weight) { chosen = _roll_relic_in_slot(player, slot); break; }
        roll -= slot.weight;
    }
    if (chosen.empty()) chosen = _fallback_relic_pick(player, all_ids);

    return _grant_relic(player, chosen);
}


// ============================================================
// B9.2 宝箱: 品质分层 (60% 普通 / 30% 丰厚 / 10% 祝福)
// ============================================================
// B9.2 宝箱品质分层 — 写入 msg
static void _treasure_loot(Player* player, int roll, std::string& msg) {
    if (roll < 10) {
        // 祝福宝箱 (10%)
        auto item = generate_random_item();
        if (item) player->inventory.add(item, player);
        apply_buff(player, "attack_up", 1);
        msg = "宝箱中的力量流入了你的身体。";
        return;
    }
    if (roll < 40) {
        // 丰厚宝箱 (30%: roll ∈ [10, 39])
        auto a = generate_random_item();
        auto b = generate_random_item();
        if (a) player->inventory.add(a, player);
        if (b) player->inventory.add(b, player);
        msg = "你打开了宝箱，里面装着两件战利品！";
        return;
    }
    // 普通宝箱 (60%: roll ∈ [40, 99])
    auto item = generate_random_item();
    if (!item) { msg = "宝箱里空空如也。"; return; }
    player->inventory.add(item, player);
    msg = "你打开了宝箱，获得了战利品。";
}

// B13: 品质分级 relic 掉率 (普通40% / 丰厚70% / 祝福100%) + B12: merchant_coin +15%
static void _treasure_relic(Player* player, int roll, std::string& msg) {
    float relic_chance = (roll < 10) ? 1.00f : (roll < 40) ? 0.70f : 0.40f;
    if (player_has_relic(player, "merchant_coin"))
        relic_chance = std::min(relic_chance + 0.15f, 0.95f);
    std::string relic_msg = _try_grant_random_relic(player, relic_chance);
    if (!relic_msg.empty()) msg += " " + relic_msg;
}

// B11: golden_dice / B12: merchant_coin — 各额外多给 1 件物品 (叠加)
static void _treasure_extra_items(Player* player, std::string& msg) {
    const struct { const char* relic; const char* msg; } extras[] = {
        {"golden_dice",   " 金色骰子额外赐予了一件物品。"},
        {"merchant_coin", " 商人硬币额外带来了一件物品。"},
    };
    for (auto& e : extras) {
        if (!player_has_relic(player, e.relic)) continue;
        auto extra = generate_random_item();
        if (extra && player->inventory.add(extra, player)) msg += e.msg;
    }
}

static std::string _exec_treasure(Player* player) {
    int roll = rng() % 100;   // 0-99
    std::string msg;
    _treasure_loot(player, roll, msg);
    _treasure_relic(player, roll, msg);
    _treasure_extra_items(player, msg);
    return msg;
}


// ============================================================
// B9.3 泉水: 回满 HP + 净化负面状态
// ============================================================
static std::string _exec_fountain(Player* player) {
    int eff_max = get_effective_max_hp(player);  // B11: blood_charm
    int missing = eff_max - player->combat.current_hp;
    if (missing < 0) missing = 0;
    // B12: sage_leaf — 泉水治疗 +10
    if (player_has_relic(player, "sage_leaf")) missing += 10;
    // D8: healing_herb — 所有治疗 +15%
    if (player_has_relic(player, "healing_herb")) missing = (int)(missing * 1.15f);
    if (missing > 0) heal_player(player, missing);  // B11: 使用 effective max clamp
    _cleanse_debuffs(player);
    return "泉水治愈并净化了你的身体。";
}

// ============================================================
// 统一交互入口
// ============================================================
// D8 Step5: 6 new room types
// ============================================================

// ---- SHOP (商店) — 随机提供1件稀有以上物品 ----
static std::string _exec_shop(Player* player) {
    // D9: 商店提供 2 件稀有+物品 (was 1)
    int count = 0;
    for (int t = 0; t < 2; t++) {
        auto item = generate_random_item();
        int tries = 0;
        while (item && item->rarity < Rarity::RARE && tries < 5) {
            item = generate_random_item(); tries++;
        }
        if (item && player->inventory.add(item, player)) count++;
    }
    if (count == 0) return "商店今天没有特别的货物。";
    return "商店为你提供了" + std::to_string(count) + "件货物。";
}

// ---- BLACKSMITH (铁匠) — 随机强化武器或护甲 ----
static std::string _exec_blacksmith(Player* player) {
    auto& inv = player->inventory;
    std::shared_ptr<EquipmentItem> target = inv.equipped["weapon"];
    if (!target) target = inv.equipped["armor"];
    // 若没有装备，给一件随机装备
    (void)target; // MVP: 简化实现 — 随机给一件装备
    auto eq = std::make_shared<EquipmentItem>("铁匠制品", Rarity::RARE, "weapon", 8, 2, 0);
    inv.add(eq, player);
    return "铁匠为你打造了一件新装备。";
}

// ---- LIBRARY (图书馆) — 随机获得1级技能升级 ----
static std::string _exec_library(Player* player) {
    auto& active = player->skills.active_skills;
    if (active.empty()) return "图书馆里空无一人——你还没有技能可以研习。";
    int idx = rng() % (int)active.size();
    if (active[idx]->can_upgrade()) {
        active[idx]->upgrade();
        return active[idx]->name + " 升级了！";
    }
    return "你研习了古籍，但没有获得新的领悟。";
}

// ---- GAMBLER (赌徒) — Gold-based gamble: 75% 装备 / 20% 钥匙 / 5% RUN 遗物 ----
int get_gamble_cost(int floor) {
    return 40 + floor * 10;
}

static std::string _exec_gambler(Player* player) {
    if (!player) return "";

    int cost = get_gamble_cost(player->current_floor);
    if (player->gold < cost) {
        return "金币不足。需要 " + std::to_string(cost) + " 金币。";
    }

    player->spend_gold(cost);

    int roll = rng() % 100;
    if (roll < 75) {
        // 75% Equipment
        auto item = generate_random_item();
        if (item && RewardManager::grant_item(*player, item))
            return "赌徒咧嘴一笑：运气不错！获得了 " + item->get_description();
        return "赌徒摊手：今天没货了。";
    } else if (roll < 95) {
        // 20% Key
        RewardManager::grant_key(*player, 1);
        return "获得了一把钥匙！";
    } else {
        // 5% RUN Relic (rarity-weighted, exhaustion → Key fallback)
        auto all_ids = get_all_relic_ids();
        std::vector<std::string> candidates;
        for (auto& id : all_ids)
            if (!player_has_relic(player, id))
                candidates.push_back(id);
        if (!candidates.empty()) {
            std::string chosen = candidates[rng() % candidates.size()];
            if (RewardManager::grant_relic(*player, chosen, PersistenceScope::RUN)) {
                const RelicDef* def = get_relic_def(chosen);
                return def ? ("RELIC:" + def->name) : "获得了一件圣物！";
            }
        }
        // Fallback: all relics owned → give key
        RewardManager::grant_key(*player, 1);
        return "圣物库已满，获得了一把钥匙作为替代。";
    }
}

// ---- SHRINE (神殿) — 随机 Blessing / 回血 / Rare Relic ----
static std::string _exec_shrine(Player* player) {
    int roll = rng() % 3;
    if (roll == 0) {
        apply_buff(player, "blessing", 2);
        return "神殿的光辉笼罩着你——你获得了祝福。";
    } else if (roll == 1) {
        int heal = get_effective_max_hp(player) / 2;
        heal_player(player, heal);
        return "神殿的力量治愈了你。";
    } else {
        // 概率获得 relic (高权重rare+)
        std::string relic_msg = _try_grant_random_relic(player, 0.50f);
        return relic_msg.empty() ? "神殿默默凝视着你。" : "神殿赐予了你一件圣物！" + relic_msg;
    }
}

// ---- SECRET (隐藏房) — 必定给 1 relic + 1 传说物品 ----
static std::string _exec_secret(Player* player) {
    // D9: 必定1 relic, 概率给传说物品(50%)
    std::string relic_msg = _try_grant_random_relic(player, 1.0f);
    std::string extra;
    if (rng() % 2 == 0) {
        auto item = std::make_shared<EquipmentItem>("秘宝", Rarity::LEGENDARY, "weapon", 20, 5, 3);
        if (player->inventory.add(item, player)) extra = " 而且找到了一件传说秘宝。";
    }
    return "你发现了一间隐藏房间！" + relic_msg + extra;
}

// ============================================================
std::string execute_special_room(SpecialRoomType type, Player* player) {
    switch (type) {
        case SpecialRoomType::ALTAR:      return _exec_altar(player);
        case SpecialRoomType::TREASURE:   return _exec_treasure(player);
        case SpecialRoomType::FOUNTAIN:   return _exec_fountain(player);
        case SpecialRoomType::SHOP:       return _exec_shop(player);
        case SpecialRoomType::BLACKSMITH: return _exec_blacksmith(player);
        case SpecialRoomType::LIBRARY:    return _exec_library(player);
        case SpecialRoomType::GAMBLER:    return _exec_gambler(player);
        case SpecialRoomType::SHRINE:     return _exec_shrine(player);
        case SpecialRoomType::SECRET:     return _exec_secret(player);
        case SpecialRoomType::CHALLENGE:  return "挑战房——按 [E] 消耗钥匙开启挑战。";
    }
    return "未知房间。";
}
