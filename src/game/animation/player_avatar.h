#pragma once
// A5-T3: 玩家骨骼形象 — A6-S1 起为 SkeletonAvatar 的玩家适配壳 (对外 API 不变)
#include <string>
#include <vector>
#include "game/animation/skeleton_avatar.h"

class Player;
struct GameMap;

// A16-Player: 朝向角 — 绕广告牌 up 轴, 0=下(朝相机), +90=右, -90=左, 180=上(背对相机)
// 供渲染层 (worldParts) 与测试锁定映射用
enum class Direction;
float direction_yaw(Direction dir);

class PlayerAvatar {
public:
    // 全有才 true (all-or-nothing): 两 JSON + 全部件贴图, 任一失败 → active()=false 走静帧回退
    bool try_init(const std::string& anim_dir, std::string& err);
    void update(float dt, const Player& pl);                      // hp 边沿/武器/移动 → animator
    void draw(const Player& pl, float cam_x, float cam_y, const GameMap* view_map) const;  // 7 件 + ghost
    bool active() const { return _core.active(); }
    const std::string& current_clip() const { return _core.current_clip(); }
    float time() const { return _core.time(); }
    float dur_scale() const { return _core.dur_scale(); }
    float pose_time() const { return _core.pose_time(); }
    std::vector<AvatarPartDraw> worldParts(const Player& player) const;

private:
    OverlayTf _overlay(const Player& pl) const;
    void _setup_voxel_parts();   // A13: 构建各部件体素网格 (默认接入; --no-voxel-avatar 关; 失败静默回退)
    bool _setup_lowpoly_parts(); // A16-Player: 构建手工低模部件 (优先于体素; 任一失败 → false 回退体素)

    SkeletonAvatar _core;   // 组合优于继承: 通用骨骼核心 (数据+贴图+animator)
};
