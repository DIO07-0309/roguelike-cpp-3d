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
#include "data/actor_avatar_defs.h"   // G12-4: 怪物骨骼皮肤白名单
#include "animation/player_avatar.h"  // G12-4: 玩家骨骼形象 (3D 路径)
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
    // G12-4: 骨骼形象懒建 (镜像 game_scene.cpp 同名实现) —— 渲染器建不出骨骼就
    // 会回退 2D 精灵贴图, 所以教程 3D 必须走同一套流程
    void _ensure_player_avatar();
    void _player_avatar_tick();
    void _monster_avatars_tick();
    std::unique_ptr<PlayerAvatar> _player_avatar;
    std::map<std::string, ActorAvatarDef> _actor_avatars;
    bool _actor_avatars_loaded = false;
    void _draw_ground_items();                  // G10.11: 掉落物 (可见性门控+精灵+圆角回退+E拾取提示)
    void _draw_monster_labels();                // G10.11: 怪物名条
    void _on_pickup(const std::string& name);   // G10.11: 拾取反馈 (音效+VFX+飘字)
};
