#pragma once
// A5-T3: 玩家骨骼形象 — A6-S1 起为 SkeletonAvatar 的玩家适配壳 (对外 API 不变)
#include <string>
#include <vector>
#include "game/animation/skeleton_avatar.h"

class Player;
struct GameMap;

class PlayerAvatar {
public:
    // 全有才 true (all-or-nothing): 两 JSON + 全部件贴图, 任一失败 → active()=false 走静帧回退
    bool try_init(const std::string& anim_dir, std::string& err);
    void update(float dt, const Player& pl);                      // hp 边沿/武器/移动 → animator
    void draw(const Player& pl, float cam_x, float cam_y, const GameMap* view_map);  // 7 件 + ghost
    bool active() const { return _core.active(); }
    const std::string& current_clip() const { return _core.current_clip(); }
    float time() const { return _core.time(); }
    float dur_scale() const { return _core.dur_scale(); }
    float pose_time() const { return _core.pose_time(); }
    std::vector<AvatarPartDraw> worldParts(const Player& player) const;

private:
    OverlayTf _overlay(const Player& pl) const;

    SkeletonAvatar _core;   // 组合优于继承: 通用骨骼核心 (数据+贴图+animator)
};
