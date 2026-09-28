#pragma once
#include <vector>
#include <string>
#include <random>
#include <memory>
#include <tuple>
#include "game_map.h"
#include "config.h"
#include "special_room.h"
#include "landmark.h"
#include "encounter.h"

// ============================================================
// BSPNode — 二分空间划分节点
// ============================================================
struct BSPNode {
    int x, y, w, h;
    BSPNode* left = nullptr;
    BSPNode* right = nullptr;
    // room: (rx, ry, rw, rh)
    int rx = 0, ry = 0, rw = 0, rh = 0;
    bool has_room = false;

    BSPNode(int x_, int y_, int w_, int h_);
    ~BSPNode();
    bool is_leaf() const;
    std::pair<int,int> center() const;
    std::pair<int,int> room_center() const;
};

// ============================================================
// DungeonGenerator — BSP 地牢生成器
// ============================================================

// Phase 2: Room ↔ Door ↔ Corridor 连接信息
struct CorridorConnection {
    std::pair<int,int> room_a_edge;   // Room A 内部边缘地板 tile
    std::pair<int,int> door_a;        // Room A 外部 Door 位置 (墙壁 tile)
    std::pair<int,int> door_b;        // Room B 外部 Door 位置 (墙壁 tile)
    std::pair<int,int> room_b_edge;   // Room B 内部边缘地板 tile
};

class DungeonGenerator {
public:
    DungeonGenerator(int w, int h, int ts,
                     int min_part = 8, int min_room = 5, int margin = 1);

    std::shared_ptr<GameMap> generate(uint32_t seed = 0, int special_room_count = 3,
                                     int arena_density = 0,
                                     const std::string& biome_id = "");
    std::vector<std::pair<int,int>> get_room_centers() const;
    std::vector<SpecialRoom> get_special_rooms() const { return _special_rooms; }
    // 诊断：暴露房间矩形与连接（供拓扑验证工具使用）
    const std::vector<std::tuple<int,int,int,int>>& get_room_rects() const { return _rooms; }
    const std::vector<CorridorConnection>& get_connections() const { return _connections; }
    // M4b: Boss 房矩形 (rooms.back()), 无房间时返回 {0,0,0,0}
    std::tuple<int,int,int,int> get_boss_room_rect() const {
        return _rooms.empty() ? std::make_tuple(0, 0, 0, 0) : _rooms.back();
    }

private:
    int _w, _h, _ts, _min_part, _min_room, _margin;
    BSPNode* _root = nullptr;
    std::vector<std::tuple<int,int,int,int>> _rooms;
    std::vector<std::tuple<int,int,int,int>> _corridors;  // 保留兼容
    std::vector<CorridorConnection> _connections;  // Phase 2: 新连接
    std::vector<SpecialRoom> _special_rooms;

    // Seed 驱动 (B8)
    uint32_t _seed = 0;
    std::mt19937 _local_rng;

    int _rand_int(int max_exclusive);
    void _assign_special_rooms(int count, const std::string& biome_id = "");
    // G13: 特殊房间放置分段
    std::vector<int> _collect_special_candidates();
    std::vector<SpecialRoomType> _build_special_pool();
    void _assign_one_special_room(const std::vector<int>& candidates,
                                  const std::vector<SpecialRoomType>& pool,
                                  const std::vector<const LandmarkDef*>& landmarks,
                                  int& placed_lm, const std::string& biome_id, int i);
    void _maybe_convert_secret(const std::string& biome_id);
    void _place_challenge_room(GameMap* gm);
    SpecialRoom _make_challenge_room(GameMap* gm, int room_idx);
    void _assign_arena_objects(GameMap* gm, int density);

    void _partition(BSPNode* node);
    void _create_child_nodes(BSPNode* node, bool vertical, int split);
    void _create_rooms(BSPNode* node);
    void _connect_rooms(BSPNode* node);
    std::pair<int,int> _pick_room(BSPNode* node);
    std::vector<std::string> _build_template();
    void _carve_rect(std::vector<std::string>& grid, int x, int y, int w, int h);
    void _carve_corridor(std::vector<std::string>& grid, int x1, int y1, int x2, int y2);
    void _carve_line(std::vector<std::string>& grid, int x1, int y1, int x2, int y2, int width);
    void _carve_diamond(std::vector<std::string>& grid, int cx, int cy, int r);

    // Phase 2: Room ↔ Door ↔ Corridor
    std::pair<int,int> _pick_room_edge(BSPNode* node, int target_x, int target_y);
    std::pair<int,int> _compute_door_pos(int edge_x, int edge_y, int room_rx, int room_ry, int room_rw, int room_rh);
    std::tuple<int,int,int,int> _get_room_rect(BSPNode* node);

    // Batch 1 (A1): 孔径完整性修复 — 环墙缺口 回墙/door 化, 保证 DOOR 是房间唯一对外孔径
    // (审计: ROOM_ENCOUNTER_DOOR_FOV_INTEGRATION_AUDIT §4; 算法已由 build/ 量化工具原型验证)
    void _repair_room_apertures(std::vector<std::string>& grid);
    std::vector<std::pair<int,int>> _collect_aperture_gaps(
        const std::vector<std::string>& grid) const;
    void _repair_aperture(std::vector<std::string>& grid, int gx, int gy,
                          int H, int W, int sx, int sy) const;
};
