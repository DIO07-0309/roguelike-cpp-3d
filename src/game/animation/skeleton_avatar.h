#pragma once
// A6-S1: 通用骨骼形象核心 — 从 PlayerAvatar 抽出 (数据+贴图+animator 聚合)
// 纯渲染无 gameplay; 渲染路径懒建 (无头 sim 不实例化); 玩家/怪物/未来 NPC 共用
#include <string>
#include <vector>
#include "raylib.h"
#include "data/animation_defs.h"
#include "game/animation/avatar_animator.h"
#include "game/animation/skeleton_pose.h"

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

class SkeletonAvatar {
public:
    // 全有才 true (all-or-nothing): 两 JSON + 全部件贴图, 任一失败 → active()=false 回退
    bool try_init(const std::string& skel_path, const std::string& anim_path, std::string& err);
    void advance(float dt, const AnimInput& in);
    bool active() const { return _active; }
    const std::string& current_clip() const { return _an.current_name(); }
    float time() const { return _an.time(); }
    float dur_scale() const { return _an.dur_scale(); }
    float pose_time() const { return _an.pose_time(); }

    bool hp_hit_edge(int hp);        // hp 下降沿 = 受击表现信号 (内部缓存, -1=未初始化)
    int& hp_state() { return _last_hp; }
    void track_facing(const Vector2& pos);   // 渲染层朝向: 位移 x 分量镜像
    float facing() const { return _facing; }

    std::vector<WorldBone> pose(const OverlayTf& overlay) const;
    std::vector<AvatarPartDraw> part_draws(const OverlayTf& overlay, bool flip_x) const;
    void draw_row(const std::vector<WorldBone>& pose, Vector2 feet,
                  float face, int alpha) const;
    void draw_at(Vector2 feet, float face, int alpha) const;  // identity overlay 单排绘制

    ~SkeletonAvatar();

private:
    void release_textures();
    SkeletonDef _sk;
    AnimSetDef _set;
    AvatarAnimator _an;
    std::vector<Texture2D> _tex;   // 与 _sk.parts 一一对应
    bool _active = false;
    int _last_hp = -1;
    float _facing = 1.f;
    Vector2 _last_pos = {};
    bool _has_last_pos = false;
};
