// A5-T2: 骨骼姿态求值器实现
#include "game/animation/skeleton_pose.h"

#include <cmath>

namespace {

constexpr float kDeg2Rad = 3.14159265358979f / 180.f;

struct LocalTf {
    float x = 0, y = 0;
    float rot_deg = 0;
    float sx = 1, sy = 1;
};

LocalTf bind_local(const BoneDef& b) {
    return { b.x, b.y, 0.f, 1.f, 1.f };
}

LocalTf key_to_local(const KeyDef& k) {
    return { k.x, k.y, k.rot, k.sx, k.sy };
}

const TrackDef* track_for(const AnimClipDef& clip, size_t bone) {
    for (const auto& tr : clip.tracks)
        if (tr.bone == static_cast<int>(bone)) return &tr;
    return nullptr;
}

float normalize_time(const AnimClipDef& clip, float t) {
    if (clip.dur <= 0.f) return 0.f;
    if (!clip.loop) return t < 0.f ? 0.f : (t > clip.dur ? clip.dur : t);
    float w = std::fmod(t, clip.dur);
    return w < 0.f ? w + clip.dur : w;
}

float lerp(float a, float b, float k) { return a + (b - a) * k; }

LocalTf sample_local(const SkeletonDef& sk, const AnimClipDef* clip, size_t bone, float t) {
    LocalTf bind = bind_local(sk.bones[bone]);
    if (!clip) return bind;
    const TrackDef* tr = track_for(*clip, bone);
    if (!tr) return bind;
    const auto& ks = tr->keys;
    float tt = normalize_time(*clip, t);
    if (tt <= ks.front().t) return key_to_local(ks.front());
    if (tt >= ks.back().t) return key_to_local(ks.back());
    size_t i = 0;
    while (i + 2 < ks.size() && ks[i + 1].t < tt) ++i;
    const KeyDef& a = ks[i];
    const KeyDef& b = ks[i + 1];
    float k = (b.t - a.t) > 1e-6f ? (tt - a.t) / (b.t - a.t) : 0.f;
    LocalTf out;
    out.x = lerp(a.x, b.x, k); out.y = lerp(a.y, b.y, k);
    out.rot_deg = lerp(a.rot, b.rot, k);
    out.sx = lerp(a.sx, b.sx, k); out.sy = lerp(a.sy, b.sy, k);
    return out;
}

WorldBone compose(const WorldBone& parent, const LocalTf& local) {
    float rad = parent.rot_deg * kDeg2Rad;
    float c = std::cos(rad), s = std::sin(rad);
    float px = local.x * parent.sx, py = local.y * parent.sy;
    WorldBone out;
    out.x = parent.x + px * c - py * s;
    out.y = parent.y + px * s + py * c;
    out.rot_deg = parent.rot_deg + local.rot_deg;
    out.sx = parent.sx * local.sx;
    out.sy = parent.sy * local.sy;
    return out;
}

} // namespace

std::vector<WorldBone> compute_pose(const SkeletonDef& sk, const AnimClipDef* clip,
                                    float t, const OverlayTf& overlay) {
    std::vector<WorldBone> world(sk.bones.size());
    WorldBone root_tf{ overlay.x, overlay.y, overlay.rot_deg, overlay.sx, overlay.sy };
    for (size_t i = 0; i < sk.bones.size(); ++i) {
        const BoneDef& b = sk.bones[i];
        const WorldBone& parent = b.parent < 0 ? root_tf : world[b.parent];
        world[i] = compose(parent, sample_local(sk, clip, i, t));
    }
    return world;
}
