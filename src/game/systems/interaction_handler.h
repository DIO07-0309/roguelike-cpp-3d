#pragma once
#include <vector>
#include <string>
#include <memory>
#include "item.h"

class Player;
class GameMap;
class SpecialRoom;

// ============================================================
// InteractionHandler — E 键交互 (拾取 + 特殊房间)
// ============================================================
class InteractionHandler {
public:
    // 按 E 键: 优先特殊房间, 否则拾取物品
    // 返回操作描述字符串 (空串 = 无操作)
    std::string try_interact(Player* player, GameMap* map,
                             std::vector<DroppedItem>& ground_items);

    // 拾取最近的物品 (map 非空时只捡可见格 — 与绘制门控对齐, 防隔墙/暗处捡)
    std::string pickup_item(Player* player, GameMap* map,
                            std::vector<DroppedItem>& ground_items);

    // 检测玩家是否首次步入特殊房间 (返回发现提示, 空串=无)
    std::string check_special_discovery(Player* player, GameMap* map);

    // B12.5: 追踪首次 relic 获得提示
    bool shown_relic_hint = false;

private:
    bool _is_pickup_available(Player* player, const std::vector<DroppedItem>& items) const;
};
