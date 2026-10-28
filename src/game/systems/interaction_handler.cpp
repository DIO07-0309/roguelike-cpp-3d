#include "interaction_handler.h"
#include "player.h"
#include "game_map.h"
#include "special_room.h"
#include "combat_system.h"
#include "config.h"
#include <cmath>
#include <algorithm>

static bool is_repeatable_special_room(SpecialRoomType type) {
    return type == SpecialRoomType::GAMBLER;
}

std::string InteractionHandler::try_interact(Player* player, GameMap* map,
                                              std::vector<DroppedItem>& ground_items) {
    if (!player || !map) return "";

    // 先检查特殊房间
    auto [tx, ty] = map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width / 2,
        player->entity.rect.y + player->entity.rect.height / 2);

    SpecialRoom* room = map->get_special_room_at(tx, ty);
    if (room && (is_repeatable_special_room(room->type) || !room->triggered)) {
        std::string msg = execute_special_room(room->type, player);
        if (!is_repeatable_special_room(room->type))
            room->triggered = true;
        return msg;
    }

    // 否则拾取物品
    return pickup_item(player, map, ground_items);
}

// A15-fix2: 同格掉落物在世界坐标里完全重合, 只捡一件看起来就是「捡了还在」→ 整格一次捡走
static void _collect_same_tile(const std::vector<DroppedItem>& items, GameMap* map,
                               int tx, int ty, std::vector<int>& out) {
    for (int i = static_cast<int>(items.size()) - 1; i >= 0; --i) {
        const auto& d = items[i];
        if (!d.item || d.tile_x != tx || d.tile_y != ty) continue;
        if (map && !map->isVisible(d.tile_x, d.tile_y)) continue;
        if (map && !map->is_walkable(d.tile_x, d.tile_y)) continue;   // A15-fix3
        out.push_back(i);
    }
}

std::string InteractionHandler::pickup_item(Player* player, GameMap* map,
                                             std::vector<DroppedItem>& ground_items) {
    DroppedItem* best = nullptr;
    float best_dist = PICKUP_RANGE * TILE_SIZE;
    Rectangle pr = player->entity.rect;

    for (auto& d : ground_items) {
        // A15-fix: 视野外物品不参与拾取 (绘制有 isVisible 门控, 否则按 E 隔墙/暗处捡)
        if (map && !map->isVisible(d.tile_x, d.tile_y)) continue;
        if (map && !map->is_walkable(d.tile_x, d.tile_y)) continue;   // A15-fix3: 墙格不参与
        if (!d.item) continue;                 // 空掉落 (生成器可返回 null) 不可捡
        float px = d.tile_x * TILE_SIZE + TILE_SIZE / 2;
        float py = d.tile_y * TILE_SIZE + TILE_SIZE / 2;
        float dist = sqrtf(powf(pr.x + pr.width / 2 - px, 2)
                         + powf(pr.y + pr.height / 2 - py, 2));
        if (dist < best_dist) { best_dist = dist; best = &d; }
    }

    if (!best) return "";

    std::vector<int> stack;                    // 下标自高到低 (helper 保证), erase 安全
    _collect_same_tile(ground_items, map, best->tile_x, best->tile_y, stack);
    std::string msg;
    for (int idx : stack) {
        auto& d = ground_items[idx];
        if (!player->inventory.add(d.item, player)) break;   // 背包满
        if (!msg.empty()) msg += "、";
        msg += d.item->get_description();
        ground_items.erase(ground_items.begin() + idx);
    }
    return msg.empty() ? "" : "拾取: " + msg;
}

std::string InteractionHandler::check_special_discovery(Player* player, GameMap* map) {
    if (!map || !player) return "";

    auto [tx, ty] = map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width / 2,
        player->entity.rect.y + player->entity.rect.height / 2);

    SpecialRoom* room = map->get_special_room_at(tx, ty);
    if (!room || room->discovered) return "";

    room->discovered = true;
    switch (room->type) {
        case SpecialRoomType::ALTAR:      return "你发现了一座古老祭坛。";
        case SpecialRoomType::TREASURE:   return "你发现了一个隐藏宝箱房。";
        case SpecialRoomType::FOUNTAIN:   return "你发现了一处治愈泉水。";
        case SpecialRoomType::SHOP:       return "你发现了一间神秘商店。";
        case SpecialRoomType::BLACKSMITH: return "你发现了一间铁匠铺。";
        case SpecialRoomType::LIBRARY:    return "你发现了一间古老的图书馆。";
        case SpecialRoomType::GAMBLER:    return "你发现了一个赌徒的房间。";
        case SpecialRoomType::SHRINE:     return "你发现了一座神圣的神殿。";
        case SpecialRoomType::SECRET:     return "你发现了一间隐藏密室！";
        case SpecialRoomType::CHALLENGE:  return "你发现了一间挑战房。";
    }
    return "";
}
