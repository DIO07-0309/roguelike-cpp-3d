#pragma once
// A5-T3: 玩家骨骼形象 — 数据+贴图+animator 聚合, 渲染路径懒建 (无头 sim 不实例化)
#include <string>
#include <vector>
#include "raylib.h"
#include "data/animation_defs.h"
#include "game/animation/avatar_animator.h"
#include "game/animation/skeleton_pose.h"

class Player;
struct GameMap;

struct AvatarPartDraw {
    Texture2D tex = {};
    Vector2 offset = {};
    Vector2 pivot = {};
    Vector2 size = {};
    float rot_deg = 0;
    bool flip_x = false;
};

AvatarPartDraw buildAvatarPart(const PartDef& part, const WorldBone& bone,
                               Texture2D texture, float pixels_per_unit, bool flip_x);

class PlayerAvatar {
public:
    // 全有才 true (all-or-nothing): 两 JSON + 全部件贴图, 任一失败 → active()=false 走静帧回退
    bool try_init(const std::string& anim_dir, std::string& err);
    void update(float dt, const Player& pl);                      // hp 边沿/武器/移动 → animator
    void draw(const Player& pl, float cam_x, float cam_y, const GameMap* view_map);  // 7 件 + ghost
    bool active() const { return _active; }
    const std::string& current_clip() const { return _an.current_name(); }
    std::vector<AvatarPartDraw> worldParts(const Player& player) const;
    ~PlayerAvatar();

private:
    void release_textures();
    OverlayTf _overlay(const Player& pl) const;
    void _draw_row(const std::vector<WorldBone>& pose, Vector2 feet, float face, int alpha) const;
    SkeletonDef _sk;
    AnimSetDef _set;
    AvatarAnimator _an;
    std::vector<Texture2D> _tex;   // 与 _sk.parts 一一对应
    bool _active = false;
    int _last_hp = -1;
};
