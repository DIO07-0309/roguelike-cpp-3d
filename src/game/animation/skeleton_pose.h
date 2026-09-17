#pragma once
// A5-T2: 骨骼姿态求值器 — 纯数学, 无渲染依赖 (spec §5)
#include <vector>
#include "data/animation_defs.h"

struct WorldBone {
    float x = 0, y = 0;       // 骨空间世界姿态 (Y 向上, 未翻转; 翻转只在渲染层做一次)
    float rot_deg = 0;
    float sx = 1, sy = 1;
};

struct OverlayTf {            // B3 tilt/squash + 重击时长缩放等外部叠加 (作用 root, 传全链)
    float x = 0, y = 0, rot_deg = 0, sx = 1, sy = 1;
};

// clip==nullptr 或某骨无 track → 该骨 = bind; root 先乘 overlay 再按声明序链合成 (父先于子, parse 已校验)
std::vector<WorldBone> compute_pose(const SkeletonDef& sk, const AnimClipDef* clip,
                                    float t, const OverlayTf& overlay = {});
