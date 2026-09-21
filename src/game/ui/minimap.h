#pragma once
#include "game_map.h"
#include "raylib.h"
#include <vector>
#include <utility>

// ============================================================
// MinimapRenderer (Phase 3) — 小地图渲染（只读 GameMap，无第二套状态）
// ============================================================
//
// 职责：
//   1. tile → 小地图屏幕坐标换算
//   2. Tile 绘制（按 isExplored/isVisible 决定显隐与亮度）
//   3. 玩家/实体/Boss/楼梯标记绘制
//   4. 面板边框与背景
// 不维护任何 explored/visible 副本，每帧从 GameMap 只读查询。
// Boss/楼梯的"已发现"标记由 GameScene 传入（见 MinimapMarker），本类不追踪探索历史。

// 标记描述 — 由调用方 (GameScene) 计算并传入，本类仅绘制
struct MinimapMarker {
    int tx = -1, ty = -1;              // 标记的 tile 坐标；-1 表示无
    bool visible = false;              // 当前是否应绘制 （Boss: 最后已知位置且已探索）
    Color color = WHITE;               // 标记颜色
    enum class Type { MONSTER, BOSS, STAIRS, ITEM } type = Type::MONSTER; // 标记类型
};

struct MinimapInput {
    int player_tx = 0, player_ty = 0;  // 玩家 tile
    std::vector<MinimapMarker> markers;      // 实体标记（怪物/物品：仅当前可见才传入）
    MinimapMarker boss_marker;               // Boss 最后已知位置（由 GameScene 维护）
    MinimapMarker stairs_marker;             // 楼梯（发现后永久，由 GameScene 判断）
};

class MinimapRenderer {
public:
    // ---- 纯函数（无状态，可单测，不依赖 Raylib 绘制） ----
    // tile → 小地图屏幕矩形
    static Rectangle tile_to_screen(int tx, int ty, Rectangle panel);
    // tile 类型 → 小地图颜色（0=未探索不绘制）
    static Color color_for_tile(TileType t, bool visible);
    // 未探索 → 不绘制（返回 null 色）
    static bool should_draw_tile(const GameMap& map, int tx, int ty);

    // ---- 标记"是否显示"过滤（不泄露核心逻辑，可单测） ----
    // 未探索区域绝不显示任何标记
    static bool should_show_boss(const GameMap& map, int tx, int ty);
    static bool should_show_stairs(const GameMap& map, int tx, int ty);
    static bool should_show_entity(const GameMap& map, int tx, int ty);

    // ---- 主绘制 ----
    void draw(const GameMap& map, const MinimapInput& input, Rectangle panel) const;

private:
    void _draw_marker(const MinimapMarker& m, Rectangle panel) const;
};
