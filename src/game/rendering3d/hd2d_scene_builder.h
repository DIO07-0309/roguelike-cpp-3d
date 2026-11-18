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
struct SpecialRoom;  // P5: 特殊房 tint (全局类型, 定义见 world/special_room.h)

namespace hd2d {

// A21: 3D 掉落物摆放高度。A20 雕饰地板是 y=0~4.48 的实体石板(实测),
// 而掉落物模型是平躺的(y=0~3, 高度≈0); 若仍放 y=0 会被石板完全埋住(看不见)。
// terrain_active: 该掉落格实际启用了 A20 雕饰地板。返回掉落物 world_pos.y。
float ground_item_model_y(bool terrain_active);

// P5: 特殊房类型色烘进 3D 地形模型 tint。
// tint 与模型顶点色相乘, 故基色已重标定; triggered/未探索在同一公式内压暗。
// visible_now: 当前帧是否可见 (false = 已探索但不可见, 压暗 40%)。
Color special_room_tint_for_model(const SpecialRoom* sr, bool visible_now);

// P5: 只有特殊房 FLOOR 转 3D 地形模型; 墙/门仍走 quad。
bool special_room_floor_uses_3d(bool is_floor, bool in_special_room);
// P5: rubble 装饰密度(坐标哈希 ≈20%, 确定性), 供渲染与测试共用。
bool rubble_placed(int tx, int ty);
// P5 fix: 是否仍需画 2D 类型色 tint/decal。只有"FLOOR 且真走 3D"才跳过
// (石板会盖住 decal); STAIRS_DOWN / 缺资产回退都保留。
bool special_room_tile_decal_needed(bool is_floor_tile, bool use_3d);
// P5-fix2: 贴地 arena 陷阱(尖刺/毒池)的摆放高度。有 3D 石板格抬到石板顶之上,
// 无石板格保持贴地(避免悬空)。
float arena_flat_object_y(bool terrain_active);

// G12-5: tint 默认值只在 hd2d_part_geometry.h 声明一次 (避免 TU 内重复)
void appendAvatarParts(const std::vector<AvatarPartDraw>& parts, Vector3 feet_world,
                        float sort_y, unsigned char alpha, float blob_width,
                        std::vector<HD2DDrawItem>& out_items, Color tint);

// 提取场景可见范围内的地形 + 实体 + 特效
// visible_tiles: 切片阶段用相机视锥粗裁剪 (地图过大时只建可见块)
void build_scene(const hd2d::SceneView& view, std::vector<HD2DDrawItem>& out_items,
                 bool part_color_ready);

} // namespace hd2d
