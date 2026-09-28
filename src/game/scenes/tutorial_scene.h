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
#include "game/animation/avatar_director.h"   // G12-6: 骨骼形象懒建 + 驱动
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace hd2d { struct SceneView; }   // G12-4: 3D 只读视图 (定义在 rendering3d/)

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

    // G12-4: 3D 只读视图 (与 GameScene 同源, 供 hd2d 渲染器消费)
    hd2d::SceneView hd2d_view() const;

private:
    bool _try_render_hd2d(int sw, int sh);   // G12-4: 3D 表现层 (成功则返回 true)
    // G12-6: 骨骼形象懒建 + 驱动 —— 渲染器建不出骨骼就回退 2D 精灵贴图,
    // 所以教程 3D 必须走同一套流程; 现在与主游戏共用 AvatarDirector
    AvatarDirector _avatars;
    void _tick_movement(float dt);              // G12-6: 玩家移动 (_process 拆出)
    void _update_camera_and_fov();              // G12-6: 摄像机跟随 + FOV (_process 拆出)
    void _render_world_2d(int sw, int sh);      // G12-6: 2D 世界绘制 (_render 拆出)
    void _draw_pickup_msg(int sw, int sh);      // G12-6: 拾取飘字 (_render 拆出)
    void _draw_inventory_panel(int sw, int sh); // G12-6: 背包面板 (_render 拆出)
    void _draw_element_select(int sw, int sh);  // G12-6: 元素三卡片选择 (_render 拆出)
    void _draw_tutorial_hints(int sw, int sh);  // G12-6: 提示框 + 底部按键 (_render 拆出)
    bool _input_quit_or_skip(const InputMap& input);   // G12-6: 退出/跳过 (_input 拆出)
    bool _input_gate_stage(const InputMap& input);     // G12-6: 阻挡型阶段 (_input 拆出)
    void _input_element_select(const InputMap& input); // G12-6: 元素卡片导航 (_input 拆出)
    void _tick_cooldown_stage();                        // G12-6: 冷却观察计时 (_input 拆出)
    void _input_inventory(const InputMap& input);       // G12-6: 背包交互 (_input 拆出)
    void _input_actions(const InputMap& input);         // G12-6: 战斗/拾取/技能 (_input 拆出)
    void _try_pickup_nearest();                         // G12-6: 就近拾取 (_input 拆出)
    void _advance_stage_if_needed();                    // G12-6: 阶段推进 (_input 拆出)
    void _draw_ground_items();                  // G10.11: 掉落物 (可见性门控+精灵+圆角回退+E拾取提示)
    void _draw_monster_labels();                // G10.11: 怪物名条 (2D 平面偏移)
    void _draw_monster_labels_3d();             // G12-6: 3D 版 (世界坐标投影, 2D 偏移会错位)
    void _on_pickup(const std::string& name);   // G10.11: 拾取反馈 (音效+VFX+飘字)
};
