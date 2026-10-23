#pragma once
#include <map>
#include <memory>
#include <vector>
#include "game/animation/player_avatar.h"
#include "game/animation/voxel_mesh_library.h"
#include "data/actor_avatar_defs.h"

class Player;
class Monster;

// ============================================================
// G12-6: 骨骼形象懒建 + 驱动 —— 主游戏与教程共用
// 单一职责: 只持有玩家形象与皮肤白名单, 按帧驱动骨骼
// 此前 TutorialScene 复制了一份 _ensure_player_avatar / _monster_avatars_tick,
// 两处漂移风险; 抽出来后 game_scene.cpp 与 tutorial_scene.cpp 各自组合一个实例
// 红线: 不持任何 gameplay 状态, 被驱动的对象仍是外部的 Player/Monster
// ============================================================
class AvatarDirector {
public:
    // 玩家形象: 首次调用建, 成败都缓存不重试 (失败则走 billboard 回退)
    void ensure_player(Player* player);
    void tick_player(const Player* player);
    // 怪物形象: 白名单命中懒建一次 (成败都缓存), 与玩家同款渲染驱动
    void tick_monsters(const std::vector<std::unique_ptr<Monster>>& monsters);

    const PlayerAvatar* player_avatar() const { return _player_avatar.get(); }
    // 皮肤白名单只读出口 —— NPC 懒建共用同一份 (避免各自重复加载)
    const std::map<std::string, ActorAvatarDef>& actor_defs() const { return _actor_avatars; }

private:
    void ensure_defs();
    // A13-M2: 怪物挂体素件 (网格按类共享; 失败/无槽位 → 该件留空回退纸偶)
    void _attach_voxel(SkeletonAvatar& avatar, const std::string& skeleton_path);

    std::unique_ptr<PlayerAvatar> _player_avatar;
    std::map<std::string, ActorAvatarDef> _actor_avatars;
    bool _actor_avatars_loaded = false;
    VoxelMeshLibrary _voxel_lib;   // A13-M2: 怪物体素网格共享缓存 (玩家仍走自有路径)
};
