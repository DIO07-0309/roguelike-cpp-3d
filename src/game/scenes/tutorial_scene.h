#pragma once
#include "node.h"
#include "input_map.h"
#include "player.h"
#include "monster.h"
#include "item.h"
#include "game_map.h"
#include "tutorial_guide.h"
#include "combat_feel.h"    // G10.8-B1: HitStop 常量
#include "systems/weapon_executor.h"  // G10.8-B1: 正式战斗链
#include "systems/vfx_server.h"       // G10.11: 拾取 VFX (ring + spark_burst)
#include <memory>
#include <string>
#include <vector>

class TutorialScene : public Node {
public:
    void _ready() override;
    void _process(double delta) override;
    void _render() override;
    void _input(const InputMap& input) override;

    std::unique_ptr<Player> player;
    std::shared_ptr<GameMap> game_map;
    std::vector<std::unique_ptr<Monster>> monsters;
    std::vector<DroppedItem> ground_items;
    TutorialGuide guide;
    bool inventory_open = false;
    int inv_cursor = 0;
    bool gave_skill = false;
    float game_time = 0;
    float cam_x = 0, cam_y = 0;
    float _tutorial_hitstop = 0.0f;   // G10.8-B1: 轻量打击停顿
    int element_cursor = 0;           // G10.8-B2: 元素选择光标
    std::vector<Effect> effects;      // G10.11: 拾取/受击特效 (与主游戏 active_effects 同款)
    std::string pickup_msg;           // G10.11: 拾取飘字
    float pickup_msg_timer = 0.0f;

private:
    void _draw_ground_items();                  // G10.11: 掉落物 (可见性门控+精灵+圆角回退+E拾取提示)
    void _draw_monster_labels();                // G10.11: 怪物名条
    void _on_pickup(const std::string& name);   // G10.11: 拾取反馈 (音效+VFX+飘字)
};
