#include "dungeon_generator.h"
#include "item.h"  // for rng
#include "challenge_room.h"
#include <algorithm>
#include <queue>
#include <cfloat>

DungeonGenerator::DungeonGenerator(int w, int h, int ts, int mp, int mr, int mg)
    : _w(w), _h(h), _ts(ts), _min_part(mp), _min_room(mr), _margin(mg) {}

// ---- BSPNode 实现 ----
BSPNode::BSPNode(int x_, int y_, int w_, int h_) : x(x_), y(y_), w(w_), h(h_) {}
BSPNode::~BSPNode() { delete left; delete right; }
bool BSPNode::is_leaf() const { return !left && !right; }
std::pair<int,int> BSPNode::center() const { return {x + w/2, y + h/2}; }
std::pair<int,int> BSPNode::room_center() const { return {rx + rw/2, ry + rh/2}; }

std::shared_ptr<GameMap> DungeonGenerator::generate(uint32_t seed, int special_room_count,
                                                   int arena_density,
                                                   const std::string& biome_id) {
    _seed = seed;
    if (seed != 0) _local_rng.seed(seed);

    _rooms.clear(); _corridors.clear(); _connections.clear(); _special_rooms.clear();
    delete _root;
    _root = new BSPNode(0, 0, _w, _h);

    _partition(_root);
    _create_rooms(_root);
    _connect_rooms(_root);

    auto gm = std::make_shared<GameMap>(_w, _h, _ts);
    auto tmpl = _build_template();
    _repair_room_apertures(tmpl);   // Batch 1 (A1): DOOR 是房间唯一对外孔径 (INVARIANT seal)
    gm->load_from_template(tmpl);

    _assign_special_rooms(special_room_count, biome_id);

    // Batch 3G: Challenge Room 近出口放置 (非 Boss 层)
    if (special_room_count > 0) _place_challenge_room(gm.get());
    gm->special_rooms = _special_rooms;

    if (arena_density > 0) _assign_arena_objects(gm.get(), arena_density);

    return gm;
}

// Batch 3G: 选距出口最近且未被特殊房间占用的房间 (侧枝: 房间经走廊相连,
// Challenge Room 不物理阻挡移动, 故不阻塞主路)
void DungeonGenerator::_place_challenge_room(GameMap* gm) {
    if (_rooms.size() < 4) return;

    auto [ex, ey, ew, eh] = _rooms.back();
    const int exit_cx = ex + ew / 2, exit_cy = ey + eh / 2;

    int best = -1;
    float best_dist = FLT_MAX;
    for (int i = 1; i < (int)_rooms.size() - 1; i++) {
        auto [rx, ry, rw, rh] = _rooms[i];
        bool has_special = false;
        for (auto& sr : _special_rooms)
            if (sr.cx == rx + rw / 2 && sr.cy == ry + rh / 2) { has_special = true; break; }
        if (has_special) continue;

        const int cx = rx + rw / 2, cy = ry + rh / 2;
        const float d = (float)((cx - exit_cx) * (cx - exit_cx) + (cy - exit_cy) * (cy - exit_cy));
        if (d < best_dist) { best_dist = d; best = i; }
    }
    if (best < 0) return;
    _special_rooms.push_back(_make_challenge_room(gm, best));
}

// 由 best 房间构造 CHALLENGE: 找房间边缘第一扇门, 传送门放室内一格
SpecialRoom DungeonGenerator::_make_challenge_room(GameMap* gm, int room_idx) {
    auto [rx, ry, rw, rh] = _rooms[room_idx];
    SpecialRoom sr;
    sr.cx = rx + rw / 2; sr.cy = ry + rh / 2;
    sr.rx = rx; sr.ry = ry; sr.rw = rw; sr.rh = rh;
    sr.type = SpecialRoomType::CHALLENGE;
    sr.triggered = false;

    for (int dy = -1; dy <= sr.rh && sr.portal_tx == 0; dy++) {
        for (int dx = -1; dx <= sr.rw && sr.portal_tx == 0; dx++) {
            const int tx = sr.rx + dx, ty = sr.ry + dy;
            if (gm->door_state_at(tx, ty) == DoorState::NONE) continue;
            if (dx < 0)                { sr.portal_tx = sr.rx;              sr.portal_ty = ty; }
            else if (dx >= sr.rw)      { sr.portal_tx = sr.rx + sr.rw - 1;  sr.portal_ty = ty; }
            else if (dy < 0)           { sr.portal_tx = tx;                 sr.portal_ty = sr.ry; }
            else                       { sr.portal_tx = tx;                 sr.portal_ty = sr.ry + sr.rh - 1; }
        }
    }
    return sr;
}


// D2 Step5: 在每个非特殊房间放置战场元素
void DungeonGenerator::_assign_arena_objects(GameMap* gm, int density) {
    static const ArenaObjectType POOL[] = {
        ArenaObjectType::EXPLOSIVE_BARREL, ArenaObjectType::HEALING_TOTEM,
        ArenaObjectType::POISON_POOL, ArenaObjectType::ROCK, ArenaObjectType::SPIKE
    };
    for (auto& [rx, ry, rw, rh] : _rooms) {
        if (_special_rooms.size() > 0) {
            // 跳过已有特殊房间
            bool is_special = false;
            for (auto& sr : _special_rooms)
                if (rx == sr.rx && ry == sr.ry) { is_special = true; break; }
            if (is_special) continue;
        }
        for (int i = 0; i < density; i++) {
            int tx = rx + 1 + _rand_int(std::max(1, rw - 2));
            int ty = ry + 1 + _rand_int(std::max(1, rh - 2));
            if (!gm->is_walkable(tx, ty)) continue;
            // 检查不在已有 Arena 上
            bool dup = false;
            for (auto& ao : gm->arena_objects)
                if (ao.tile_x == tx && ao.tile_y == ty) { dup = true; break; }
            if (dup) continue;
            ArenaObject obj;
            obj.type = POOL[_rand_int(5)];
            obj.tile_x = tx; obj.tile_y = ty; obj.active = true;
            gm->arena_objects.push_back(obj);
        }
    }
}

std::vector<std::pair<int,int>> DungeonGenerator::get_room_centers() const {
    std::vector<std::pair<int,int>> centers;
    for (auto& [rx, ry, rw, rh] : _rooms)
        centers.emplace_back(rx + rw/2, ry + rh/2);
    return centers;
}

// B8: 统一的随机入口 — seed≠0 时走本地 rng，否则走全局 rng
int DungeonGenerator::_rand_int(int max_exclusive) {
    if (max_exclusive <= 0) return 0;
    if (_seed != 0) return (int)(_local_rng() % (uint32_t)max_exclusive);
    return (int)(rng() % max_exclusive);
}

// B8/G6.2: 从 _rooms 中挑选 N 个作为特殊房间 (shuffle pool 随机分配)
void DungeonGenerator::_assign_special_rooms(int count, const std::string& biome_id) {
    if (_rooms.size() < 4) return;

    const std::vector<int> candidates = _collect_special_candidates();
    const std::vector<SpecialRoomType> pool = _build_special_pool();

    std::vector<const LandmarkDef*> landmarks;
    if (!biome_id.empty())
        landmarks = get_landmarks_for_biome(biome_id);

    // Reserve 1 candidate for challenge room placement below
    const int scount = std::min(count, (int)candidates.size() - 1);
    int placed_lm = 0;
    for (int i = 0; i < scount; i++)
        _assign_one_special_room(candidates, pool, landmarks, placed_lm, biome_id, i);

    _maybe_convert_secret(biome_id);
}

// G13: 候选房间序 — 优先靠近出生房 (room 1/2), 再整体洗牌且保留 room 1 不被打散
std::vector<int> DungeonGenerator::_collect_special_candidates() {
    std::vector<int> candidates;
    // G10: prioritize rooms near player spawn (room 0) for first few relics
    if ((int)_rooms.size() >= 3) candidates.push_back(1);
    if ((int)_rooms.size() >= 4) candidates.push_back(2);
    for (int i = 3; i < (int)_rooms.size() - 1; i++)
        candidates.push_back(i);

    for (int i = (int)candidates.size() - 1; i > 0; i--) {
        int j = _rand_int(i + 1);
        if (candidates[i] == 1 || candidates[j] == 1) continue;
        std::swap(candidates[i], candidates[j]);
    }
    return candidates;
}

// G13: 类型池 — 全部基础类型 (排除 LANDMARK/SECRET/CHALLENGE) 洗牌后 GAMBLER 置顶
std::vector<SpecialRoomType> DungeonGenerator::_build_special_pool() {
    std::vector<SpecialRoomType> pool = {
        SpecialRoomType::ALTAR, SpecialRoomType::TREASURE,
        SpecialRoomType::FOUNTAIN, SpecialRoomType::SHOP,
        SpecialRoomType::BLACKSMITH, SpecialRoomType::LIBRARY,
        SpecialRoomType::GAMBLER, SpecialRoomType::SHRINE
    };
    for (int i = (int)pool.size() - 1; i > 0; i--) {
        int j = _rand_int(i + 1);
        std::swap(pool[i], pool[j]);
    }
    // Batch 3G: Guarantee GAMBLER at Room 1 (spawn-side economic anchor)
    for (int i = 0; i < (int)pool.size(); i++) {
        if (pool[i] == SpecialRoomType::GAMBLER && i != 0) {
            std::swap(pool[0], pool[i]);
            break;
        }
    }
    return pool;
}

// G13: 放置单个特殊房间 — 50% 概率转群系地标, 否则取池内类型
void DungeonGenerator::_assign_one_special_room(
        const std::vector<int>& candidates, const std::vector<SpecialRoomType>& pool,
        const std::vector<const LandmarkDef*>& landmarks, int& placed_lm,
        const std::string& biome_id, int i) {
    auto [rx, ry, rw, rh] = _rooms[candidates[i]];
    SpecialRoom sr;
    sr.cx = rx + rw / 2; sr.cy = ry + rh / 2;
    sr.rx = rx; sr.ry = ry; sr.rw = rw; sr.rh = rh;

    // G6.2: ~50% chance -> biome landmark (never override GAMBLER at Room 1)
    if (!landmarks.empty() && _rand_int(2) == 0 && placed_lm < 3
        && !(i == 0 && pool[0] == SpecialRoomType::GAMBLER)) {
        const LandmarkDef* lm = landmarks[_rand_int((int)landmarks.size())];
        sr.type = SpecialRoomType::LANDMARK;
        sr.landmark_id = lm->id;
        sr.biome_id = biome_id;
        placed_lm++;
    } else {
        sr.type = pool[i % (int)pool.size()];
    }
    sr.triggered = false;
    _special_rooms.push_back(sr);
}

// G6.6: 30% chance to convert one existing room -> SECRET
void DungeonGenerator::_maybe_convert_secret(const std::string& biome_id) {
    if (biome_id.empty() || _special_rooms.empty() || _rand_int(100) >= 30) return;
    if (!pick_encounter_by_trigger(biome_id, "wall_interact")) return;
    const int idx = _rand_int((int)_special_rooms.size());
    if (_special_rooms[idx].type != SpecialRoomType::LANDMARK
        && _special_rooms[idx].type != SpecialRoomType::GAMBLER)
        _special_rooms[idx].type = SpecialRoomType::SECRET;
}


void DungeonGenerator::_partition(BSPNode* node) {
    bool vertical = (node->w > node->h) ? true
                  : (node->h > node->w) ? false
                  : (_rand_int(2) == 0);

    int region = vertical ? node->w : node->h;
    if (region < _min_part * 2) return;

    int split = _min_part + _rand_int(region - _min_part * 2 + 1);
    _create_child_nodes(node, vertical, split);
    _partition(node->left);
    _partition(node->right);
}

void DungeonGenerator::_create_child_nodes(BSPNode* node, bool vert, int split) {
    if (vert) {
        node->left  = new BSPNode(node->x, node->y, split, node->h);
        node->right = new BSPNode(node->x + split, node->y, node->w - split, node->h);
    } else {
        node->left  = new BSPNode(node->x, node->y, node->w, split);
        node->right = new BSPNode(node->x, node->y + split, node->w, node->h - split);
    }
}

void DungeonGenerator::_create_rooms(BSPNode* node) {
    if (!node->is_leaf()) {
        if (node->left)  _create_rooms(node->left);
        if (node->right) _create_rooms(node->right);
        return;
    }
    int rw = _min_room + _rand_int(std::max(1, node->w - 2 * _margin - _min_room + 1));
    int rh = _min_room + _rand_int(std::max(1, node->h - 2 * _margin - _min_room + 1));
    int rx = node->x + _margin + _rand_int(std::max(1, node->w - rw - 2 * _margin + 1));
    int ry = node->y + _margin + _rand_int(std::max(1, node->h - rh - 2 * _margin + 1));

    node->rx = rx; node->ry = ry; node->rw = rw; node->rh = rh;
    node->has_room = true;
    _rooms.emplace_back(rx, ry, rw, rh);
}

void DungeonGenerator::_connect_rooms(BSPNode* node) {
    if (node->is_leaf()) return;
    if (node->left)  _connect_rooms(node->left);
    if (node->right) _connect_rooms(node->right);
    if (!node->left || !node->right) return;

    // Phase 2: 获取两个子树的房间信息
    auto [arx, ary, arw, arh] = _get_room_rect(node->left);
    auto [brx, bry, brw, brh] = _get_room_rect(node->right);
    if (arw <= 0 || brw <= 0) return;

    // 获取两个房间的中心（用于选择最近边缘）
    int acx = arx + arw/2, acy = ary + arh/2;
    int bcx = brx + brw/2, bcy = bry + brh/2;

    // 选择边缘连接点
    auto edge_a = _pick_room_edge(node->left, bcx, bcy);
    auto edge_b = _pick_room_edge(node->right, acx, acy);
    if (edge_a.first < 0 || edge_b.first < 0) return;

    // 计算 Door 位置
    auto door_a = _compute_door_pos(edge_a.first, edge_a.second, arx, ary, arw, arh);
    auto door_b = _compute_door_pos(edge_b.first, edge_b.second, brx, bry, brw, brh);
    if (door_a.first < 0 || door_b.first < 0) return;

    // 存储连接信息
    _connections.push_back({edge_a, door_a, door_b, edge_b});

    // 保留旧 _corridors 用于兼容（走廊从 Door 到 Door）
    _corridors.emplace_back(door_a.first, door_a.second, door_b.first, door_b.second);
}

std::pair<int,int> DungeonGenerator::_pick_room(BSPNode* node) {
    if (node->is_leaf()) {
        if (node->has_room) return node->room_center();
        return {-1, -1};
    }
    auto children = {node->left, node->right};
    std::vector<BSPNode*> valid;
    for (auto* c : children) if (c) valid.push_back(c);
    if (valid.empty()) return {-1, -1};
    return _pick_room(valid[_rand_int((int)valid.size())]);
}

// Phase 2: 返回 Room 内部紧邻墙壁的地板 tile（边缘中点）
std::pair<int,int> DungeonGenerator::_pick_room_edge(BSPNode* node, int target_x, int target_y) {
    auto [rx, ry, rw, rh] = _get_room_rect(node);
    if (rw <= 0 || rh <= 0) return {-1, -1};

    // 4 个边缘中点（房间内部地板 tile）
    struct Edge { int x, y; int door_x, door_y; };
    Edge edges[4] = {
        {rx + rw/2, ry,         rx + rw/2, ry - 1},      // 上 → Door 向上
        {rx + rw/2, ry+rh-1,    rx + rw/2, ry + rh},     // 下 → Door 向下
        {rx,        ry + rh/2,  rx - 1,    ry + rh/2},   // 左 → Door 向左
        {rx+rw-1,   ry + rh/2,  rx + rw,   ry + rh/2},   // 右 → Door 向右
    };

    // 过滤 Door 越界的边缘
    int best_idx = -1;
    double best_dist = 1e9;
    for (int i = 0; i < 4; i++) {
        if (edges[i].door_x < 0 || edges[i].door_x >= _w) continue;
        if (edges[i].door_y < 0 || edges[i].door_y >= _h) continue;
        double dist = (double)(edges[i].x - target_x) * (edges[i].x - target_x)
                    + (double)(edges[i].y - target_y) * (edges[i].y - target_y);
        if (dist < best_dist) { best_dist = dist; best_idx = i; }
    }
    if (best_idx < 0) return {-1, -1};
    return {edges[best_idx].x, edges[best_idx].y};
}

// Phase 2: 从 edge point 计算 Door 位置
std::pair<int,int> DungeonGenerator::_compute_door_pos(int edge_x, int edge_y,
                                                        int room_rx, int room_ry,
                                                        int room_rw, int room_rh) {
    // edge 是房间内部地板 tile，Door 是其向外 1 格的墙壁 tile
    if (edge_y == room_ry && edge_x >= room_rx && edge_x < room_rx + room_rw)
        return {edge_x, room_ry - 1};           // 上边缘 → Door 向上
    if (edge_y == room_ry + room_rh - 1 && edge_x >= room_rx && edge_x < room_rx + room_rw)
        return {edge_x, room_ry + room_rh};     // 下边缘 → Door 向下
    if (edge_x == room_rx && edge_y >= room_ry && edge_y < room_ry + room_rh)
        return {room_rx - 1, edge_y};           // 左边缘 → Door 向左
    if (edge_x == room_rx + room_rw - 1 && edge_y >= room_ry && edge_y < room_ry + room_rh)
        return {room_rx + room_rw, edge_y};     // 右边缘 → Door 向右
    return {-1, -1};  // 不应该到这里
}

// Phase 2: 获取 BSP 叶子节点的 Room 矩形
std::tuple<int,int,int,int> DungeonGenerator::_get_room_rect(BSPNode* node) {
    if (node->is_leaf() && node->has_room)
        return {node->rx, node->ry, node->rw, node->rh};
    // 非叶子：递归找任意一个子房间
    if (node->left) {
        auto r = _get_room_rect(node->left);
        if (std::get<2>(r) > 0) return r;
    }
    if (node->right) {
        auto r = _get_room_rect(node->right);
        if (std::get<2>(r) > 0) return r;
    }
    return {0, 0, 0, 0};
}

std::vector<std::string> DungeonGenerator::_build_template() {
    std::vector<std::string> grid(_h, std::string(_w, '#'));
    for (auto& [rx, ry, rw, rh] : _rooms)
        _carve_rect(grid, rx, ry, rw, rh);
    for (auto& [x1, y1, x2, y2] : _corridors)
        _carve_corridor(grid, x1, y1, x2, y2);
    // Phase 2: 放置 Door tiles
    for (auto& conn : _connections) {
        auto [dx, dy] = conn.door_a;
        if (dx >= 0 && dx < _w && dy >= 0 && dy < _h)
            grid[dy][dx] = 'D';
        auto [dx2, dy2] = conn.door_b;
        if (dx2 >= 0 && dx2 < _w && dy2 >= 0 && dy2 < _h)
            grid[dy2][dx2] = 'D';
    }
    return grid;
}

void DungeonGenerator::_carve_rect(std::vector<std::string>& g, int x, int y, int w, int h) {
    for (int row = y; row < std::min(y + h, _h); row++)
        for (int col = x; col < std::min(x + w, _w); col++)
            g[row][col] = '.';
}

void DungeonGenerator::_carve_corridor(std::vector<std::string>& g, int x1, int y1, int x2, int y2) {
    int width = DUNGEON_CORRIDOR_MIN + _rand_int(DUNGEON_CORRIDOR_MAX - DUNGEON_CORRIDOR_MIN + 1);
    if (_rand_int(2) == 0) {
        _carve_line(g, x1, y1, x2, y1, width);
        _carve_line(g, x2, y1, x2, y2, width);
    } else {
        _carve_line(g, x1, y1, x1, y2, width);
        _carve_line(g, x1, y2, x2, y2, width);
    }
}

void DungeonGenerator::_carve_line(std::vector<std::string>& g, int x1, int y1, int x2, int y2, int w) {
    if (x1 == x2) {
        int step = (y2 >= y1) ? 1 : -1;
        for (int y = y1; y != y2 + step; y += step)
            _carve_diamond(g, x1, y, w);
    } else {
        int step = (x2 >= x1) ? 1 : -1;
        for (int x = x1; x != x2 + step; x += step)
            _carve_diamond(g, x, y1, w);
    }
}

void DungeonGenerator::_carve_diamond(std::vector<std::string>& g, int cx, int cy, int r) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            if (abs(dx) + abs(dy) > r) continue;
            int tx = cx + dx, ty = cy + dy;
            if (tx >= 0 && tx < _w && ty >= 0 && ty < _h)
                if (g[ty][tx] == '#')  // Phase 2: 只雕刻墙壁，保护 Room Interior
                    g[ty][tx] = '.';
        }
}
// ── Batch 1 (A1): Room Aperture Integrity ────────────────────────────
// 目标: DOOR 是房间唯一对外孔径。走廊雕凿 (_carve_diamond) 会在环墙上留
//       FLOOR 缺口 (39+ 随机测图), 这些是"隐形门"。本修复在 char 域处理:
//       - 缺口试墙后发现仍全图连通 → 回填 WALL (死凹槽, 占绝大多数)
//       - 回填会断开连通 -> 升级为 DOOR (活页孔径, 走廊穿行点)
//       零 RNG 消耗, 保持种子确定性 (扫描序 + 固定决策规则, 无随机)。
// 算法原型的成功率已由 build/tmp_exploration_audit2 实测 (7 seeds):
//   467/495 回墙, 28/495 转门; 修复后 gap=0 / interior_leak=0 / dead=0。

static int _grid_walkable_count(const std::vector<std::string>& g) {
    int n = 0;
    for (auto& row : g)
        for (char c : row)
            if (c != '#') n++;
    return n;
}

static int _grid_flood(const std::vector<std::string>& g, int sx, int sy, bool door_mode) {
    int H = (int)g.size(), W = H ? (int)g[0].size() : 0;
    if (sx < 0 || sy < 0 || sx >= W || sy >= H) return 0;
    if (g[sy][sx] == '#') return 0;
    if (door_mode && g[sy][sx] == 'D') return 0;
    std::vector<char> vis((size_t)H * W, 0);
    std::queue<std::pair<int,int> > q;
    q.push(std::make_pair(sx, sy));
    vis[sy * W + sx] = 1;
    int n = 1;
    const int DX[4] = {1,-1,0,0}, DY[4] = {0,0,1,-1};
    while (!q.empty()) {
        std::pair<int,int> c = q.front(); q.pop();
        for (int d = 0; d < 4; d++) {
            int nx = c.first + DX[d], ny = c.second + DY[d];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            int idx = ny * W + nx;
            if (vis[idx]) continue;
            char ch = g[ny][nx];
            if (ch == '#') continue;
            if (door_mode && ch == 'D') continue;
            vis[idx] = 1;
            n++;
            q.push(std::make_pair(nx, ny));
        }
    }
    return n;
}

// G13: 收集环墙缺口 — walkable('.') 且非门('D'), 去重后按坐标排序 (确定性)
std::vector<std::pair<int,int>> DungeonGenerator::_collect_aperture_gaps(
        const std::vector<std::string>& grid) const {
    const int H = (int)grid.size();
    const int W = (int)grid[0].size();
    std::vector<std::pair<int,int> > gaps;
    for (auto& [rx, ry, rw, rh] : _rooms) {
        for (int x = rx; x < rx + rw; x++) {
            if (ry - 1 >= 0 && grid[ry-1][x] != '#' && grid[ry-1][x] != 'D') gaps.emplace_back(x, ry-1);
            if (ry + rh < H && grid[ry+rh][x] != '#' && grid[ry+rh][x] != 'D') gaps.emplace_back(x, ry+rh);
        }
        for (int y = ry; y < ry + rh; y++) {
            if (rx - 1 >= 0 && grid[y][rx-1] != '#' && grid[y][rx-1] != 'D') gaps.emplace_back(rx-1, y);
            if (rx + rw < W && grid[y][rx+rw] != '#' && grid[y][rx+rw] != 'D') gaps.emplace_back(rx+rw, y);
        }
    }
    std::sort(gaps.begin(), gaps.end());
    gaps.erase(std::unique(gaps.begin(), gaps.end()), gaps.end());
    return gaps;
}

// G13: 单缺口决策 — 默认回墙; 仅当 >1 开放邻居且回墙会切断连通时升级为门。
// 先过滤"死胡同缺口" (开放邻居<=1): 它不连接任何其它开放区, 仅是一格走廊死端。
// 若对这类缺口 door 化会产生孤立门 (违反 Door_NoFloating: 门需 >=1 开放邻居)。
// 死胡同回墙不会破坏任何 rooms 之间的连通 (其开放侧已与主区连通, 闭合侧全墙)。
void DungeonGenerator::_repair_aperture(std::vector<std::string>& grid, int gx, int gy,
                                        int H, int W, int sx, int sy) const {
    const char cell = grid[gy][gx];
    if (cell == '#' || cell == 'D') return;

    int open_nb = 0;
    if (gy - 1 >= 0 && grid[gy-1][gx] != '#') open_nb++;
    if (gy + 1 < H  && grid[gy+1][gx] != '#') open_nb++;
    if (gx - 1 >= 0 && grid[gy][gx-1] != '#') open_nb++;
    if (gx + 1 < W  && grid[gy][gx+1] != '#') open_nb++;

    grid[gy][gx] = '#';                          // 尝试回墙
    if (open_nb > 1) {
        const int total = _grid_walkable_count(grid);
        const int reach = _grid_flood(grid, sx, sy, false);
        if (reach != total)
            grid[gy][gx] = 'D';                  // 活孔径, 升级门 (有 >1 邻居, 非孤立)
    }
    // open_nb<=1 的死胡同保持 '#' (回墙) — 不产生孤立门
}

void DungeonGenerator::_repair_room_apertures(std::vector<std::string>& grid) {
    if (_rooms.empty() || grid.empty()) return;
    const int H = (int)grid.size();
    const int W = (int)grid[0].size();

    // 出生房中心作为连通性锚点 (房间内必有 '.' 可用)
    auto [r0x, r0y, r0w, r0h] = _rooms.front();
    const int sx = r0x + r0w / 2;
    const int sy = r0y + r0h / 2;

    const std::vector<std::pair<int,int> > gaps = _collect_aperture_gaps(grid);
    for (auto& [gx, gy] : gaps)
        _repair_aperture(grid, gx, gy, H, W, sx, sy);

    // 3. 内建自检与 INVARIANT(seal) 同语义 — 由 door_seal_test 外置永久回归 (T1~T4)
}

