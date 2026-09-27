#pragma once
#include "hd2d_renderer.h"
#include "hd2d_part_geometry.h"
#include <vector>

// ============================================================
// M6-HD2D: 场景构建器 — 每帧从只读场景视图提取 HD2DDrawItem
// 唯一职责: 游戏状态 → 3D 绘制列表的纯翻译层
// G12-4: 解耦后不再依赖 GameScene —— 任何能填出 SceneView 的场景都能走 3D
// 红线: 只读视图; 不产生任何 gameplay 副作用; 视觉随机只吃 visual_rng
// ============================================================
namespace hd2d {

// G12-5: tint 默认值只在 hd2d_part_geometry.h 声明一次 (避免 TU 内重复)
void appendAvatarParts(const std::vector<AvatarPartDraw>& parts, Vector3 feet_world,
                        float sort_y, unsigned char alpha, float blob_width,
                        std::vector<HD2DDrawItem>& out_items, Color tint);

// 提取场景可见范围内的地形 + 实体 + 特效
// visible_tiles: 切片阶段用相机视锥粗裁剪 (地图过大时只建可见块)
void build_scene(const hd2d::SceneView& view, std::vector<HD2DDrawItem>& out_items,
                 bool part_color_ready);

} // namespace hd2d
