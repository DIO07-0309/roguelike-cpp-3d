// A6-S1: SkeletonAvatar 实现 — 通用懒加载 + 姿态求值 + 2D 逐件绘制 (原 PlayerAvatar 核心)
#include "game/animation/skeleton_avatar.h"

#include <cmath>

SkeletonAvatar::~SkeletonAvatar() { release_textures(); }

void SkeletonAvatar::release_textures() {
    for (auto& t : _tex)
        if (t.id > 0) UnloadTexture(t);
    _tex.clear();
}

bool SkeletonAvatar::try_init(const std::string& skel_path, const std::string& anim_path,
                              std::string& err) {
    _active = false;
    auto sk = load_skeleton_file(skel_path, err);
    if (!sk) return false;
    auto set = load_anim_file(anim_path, *sk, err);
    if (!set) return false;
    std::vector<Texture2D> tex;
    for (const auto& part : sk->parts) {
        Texture2D t = LoadTexture(part.file.c_str());
        if (t.id <= 0) {
            for (auto& ok : tex) UnloadTexture(ok);
            err = "avatar: texture load failed: " + part.file;
            return false;
        }
        tex.push_back(t);
    }
    _sk = std::move(*sk);
    _set = std::move(*set);
    _tex = std::move(tex);
    _an.set_durations(_set);
    _active = true;
    return true;
}

void SkeletonAvatar::advance(float dt, const AnimInput& in) {
    if (!_active) return;
    _an.advance(dt, in);
}

bool SkeletonAvatar::hp_hit_edge(int hp) {
    const bool hit = (_last_hp >= 0 && hp < _last_hp);   // hp 下降沿 = 受击表现信号
    _last_hp = hp;
    return hit;
}

void SkeletonAvatar::track_facing(const Vector2& pos) {
    if (_has_last_pos) {
        const float dx = pos.x - _last_pos.x;
        if (dx > 1e-3f) _facing = 1.f;
        else if (dx < -1e-3f) _facing = -1.f;
    }
    _last_pos = pos;
    _has_last_pos = true;
}

std::vector<WorldBone> SkeletonAvatar::pose(const OverlayTf& overlay) const {
    return compute_pose(_sk, _an.clip(_set), _an.pose_time(), overlay);
}

void SkeletonAvatar::draw_row(const std::vector<WorldBone>& pose, Vector2 feet,
                              float face, int alpha) const {
    for (size_t index = 0; index < _sk.parts.size(); ++index) {
        const auto& definition = _sk.parts[index];
        const auto part = buildAvatarPart(definition, pose[definition.bone], _tex[index],
                                          _sk.pixels_per_unit, face < 0);
        const Rectangle source{0, 0, part.flip_x ? -float(part.tex.width) : float(part.tex.width),
                               float(part.tex.height)};
        const Rectangle destination{feet.x + part.offset.x, feet.y - part.offset.y,
                                    part.size.x, part.size.y};
        DrawTexturePro(part.tex, source, destination, part.pivot, part.rot_deg,
                       {255, 255, 255, static_cast<unsigned char>(alpha)});
    }
}

void SkeletonAvatar::draw_at(Vector2 feet, float face, int alpha) const {
    if (!_active) return;
    draw_row(pose({}), feet, face, alpha);
}

std::vector<AvatarPartDraw> SkeletonAvatar::part_draws(const OverlayTf& overlay,
                                                       bool flip_x) const {
    std::vector<AvatarPartDraw> parts;
    if (!_active) return parts;
    const auto bones = pose(overlay);
    parts.reserve(_sk.parts.size());
    for (size_t index = 0; index < _sk.parts.size(); ++index) {
        const auto& part = _sk.parts[index];
        parts.push_back(buildAvatarPart(part, bones[part.bone], _tex[index],
                                        _sk.pixels_per_unit, flip_x));
    }
    return parts;
}

AvatarPartDraw buildAvatarPart(const PartDef& part, const WorldBone& bone,
                               Texture2D texture, float pixels_per_unit, bool flip_x) {
    const float radians = bone.rot_deg * 3.14159265f / 180.f;
    const float cosine = std::cos(radians), sine = std::sin(radians);
    const float local_x = part.dx * bone.sx, local_y = part.dy * bone.sy;
    const float face = flip_x ? -1.f : 1.f;
    AvatarPartDraw result;
    result.tex = texture;
    result.offset = {face * (bone.x + local_x * cosine - local_y * sine) * pixels_per_unit,
                     (bone.y + local_x * sine + local_y * cosine) * pixels_per_unit};
    result.size = {texture.width * pixels_per_unit * bone.sx,
                   texture.height * pixels_per_unit * bone.sy};
    result.pivot = {part.pivot_x * pixels_per_unit * bone.sx,
                    (texture.height - part.pivot_y) * pixels_per_unit * bone.sy};
    if (flip_x) result.pivot.x = result.size.x - result.pivot.x;
    result.rot_deg = -face * bone.rot_deg;
    result.flip_x = flip_x;
    return result;
}
