#pragma once
#include "world/game_map.h"
#include "entities/player.h"
#include "entities/monster.h"
#include "entities/item.h"
#include "types/combat_types.h"       // Effect
#include "types/weapon_types.h"       // Projectile
#include "scenes/npc_view.h"          // NpcView
#include <vector>
#include <memory>
#include <functional>

class SceneTree;
class SkeletonAvatar;
class PlayerAvatar;
class AmbientLayer;
class BossSystemDirector;
class ChallengeRoomController;
class CameraLanguageDirector;

namespace hd2d {

// G12-4: 3D 表现层的只读场景视图 (组合而非继承)
//
// 之前 hd2d 渲染器与 scene_builder 直接吃 GameScene&, 把 3D 表现层焊死在
// 唯一一个游戏场景上 —— 教程场景想 3D 只能重写一遍渲染。
// 现在任何场景只要填出这个结构就能喂给渲染器, 渲染器从此不依赖 GameScene。
//
// 红线: 只读快照, 不含可变引用, 调用方不得借此写 gameplay 状态。
// 约定: nullptr / 空 vector / 空 callable = 该场景没有这个子系统。
//       build_scene 自行判空跳过 (G14: 三处 *view.monsters 曾无守卫,
//       空视图调用直接 segfault — 见 tests/rendering/hd2d_scene_builder_test.cpp)。
struct SceneView {
    // ── 核心 (缺失则不出画面) ──
    const GameMap*  game_map = nullptr;
    const Player*   player   = nullptr;
    const std::vector<std::unique_ptr<Monster>>* monsters = nullptr;

    // ── 实体内容 (per-frame 集合一律值快照: 空 vector 即"该场景没有这类东西",
    //    使用方无需判空; 只有单例与可选子系统才用指针) ──
    std::vector<Effect>       effects;                      // 特效
    std::vector<DroppedItem>  dropped;                      // 地面掉落
    std::vector<Projectile>   projectiles;                  // 存活投射物
    std::vector<NpcView>      npc_views;                    // 在图 NPC
    std::function<SkeletonAvatar*(int)> npc_avatar;         // 空 = 无 NPC 骨骼
    std::function<const PlayerAvatar*()> player_avatar_fn;  // 空 = 无玩家骨骼形象

    // ── 可选子系统 ──
    const BossSystemDirector*       boss_ctrl       = nullptr;
    const CameraLanguageDirector*   camera_director = nullptr;
    const ChallengeRoomController*  challenge_ctrl  = nullptr;
    const AmbientLayer*             ambient_layer   = nullptr;

    // ── 标量 ──
    int  current_floor      = 1;
    bool sim_mode           = false;
    bool in_challenge_arena = false;
    bool camera_def_loaded  = false;
    SceneTree* tree = nullptr;
};

}  // namespace hd2d
