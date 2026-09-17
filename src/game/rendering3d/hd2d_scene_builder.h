#pragma once
#include "hd2d_renderer.h"
#include "hd2d_part_geometry.h"
#include <vector>

class GameScene;

// ============================================================
// M6-HD2D: 场景构建器 — 每帧从 GameScene 只读状态提取 HD2DDrawItem
// 唯一职责: 2D 游戏状态 → 3D 绘制列表的纯翻译层
// 红线: 只读 gs; 不产生任何 gameplay 副作用; 视觉随机只吃 visual_rng
// ============================================================
namespace hd2d {

void appendAvatarParts(const std::vector<AvatarPartDraw>& parts, Vector3 feet_world,
                       float sort_y, unsigned char alpha, float blob_width,
                       std::vector<HD2DDrawItem>& out_items);

// 提取 GameScene 可见范围内的地形 + 实体 + 特效
// visible_tiles: 切片阶段用相机视锥粗裁剪 (地图过大时只建可见块)
void build_scene(GameScene& gs, std::vector<HD2DDrawItem>& out_items,
                 bool part_color_ready);

} // namespace hd2d
