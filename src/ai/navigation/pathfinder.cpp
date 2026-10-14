// G8.2: A* grid pathfinder implementation
#include "ai/navigation/pathfinder.h"
#include <queue>
#include <vector>
#include <limits>
#include <cstdlib>

namespace ai {

// ── Internal node ──────────────────────────────────────────
struct AStarNode {
    int idx;       // x*height + y
    float g, f;
    bool operator>(const AStarNode& o) const { return f > o.f; }
};

// Manhattan distance on grid
static inline float heuristic(Vec2i a, Vec2i b) {
    return (float)(std::abs(a.x - b.x) + std::abs(a.y - b.y));
}

static inline int pack(int x, int y, int h) { return x * h + y; }
static inline Vec2i unpack(int idx, int h) { return {idx / h, idx % h}; }

static const int DX[4] = {1, -1, 0,  0};
static const int DY[4] = {0,  0, 1, -1};

// ── G53: A* 搜索状态 (flat 数组是性能关键, 不能用嵌套容器) ──
// 把散在 find_path 里的 5 个局部状态收成一个 struct, 两个 hot step
// (expand / reconstruct) 就成为可独立测试/独立修改的方法。
struct AStarState {
    std::vector<float> g_cost;      // idx -> 已知最优 g
    std::vector<int> parent;        // idx -> 前驱 idx (-1 = 无)
    std::vector<bool> closed;       // idx -> 已定案
    // 小顶堆: std::greater 让 priority_queue 以 f 最小者为 top
    std::priority_queue<AStarNode, std::vector<AStarNode>,
                        std::greater<AStarNode>> open;
    int nodes_visited = 0;

    void init(int size, int start_idx, float h) {
        g_cost.assign(size, std::numeric_limits<float>::max());
        parent.assign(size, -1);
        closed.assign(size, false);
        g_cost[start_idx] = 0.0f;
        open.push({start_idx, 0.0f, h});
    }
};

// 把 ci 的 4 邻域压入 open (无界/已定案/不可走的一律跳过)
static void astar_expand(AStarState& st, int ci, Vec2i goal,
                         const WalkableFn& is_walkable, int map_w, int map_h) {
    Vec2i cpos = unpack(ci, map_h);
    for (int d = 0; d < 4; d++) {
        int nx = cpos.x + DX[d], ny = cpos.y + DY[d];
        if (nx < 0 || nx >= map_w || ny < 0 || ny >= map_h) continue;
        int ni = pack(nx, ny, map_h);
        if (st.closed[ni]) continue;
        if (!is_walkable(nx, ny)) continue;

        float ng = st.g_cost[ci] + 1.0f;
        if (ng < st.g_cost[ni]) {
            st.g_cost[ni] = ng;
            st.parent[ni] = ci;
            st.open.push({ni, ng, ng + heuristic({nx, ny}, goal)});
        }
    }
}

// 从 goal_idx 顺 parent 走回 start (堆是倒序的, 需要反转)
static void astar_reconstruct(PathResult& result, AStarState& st,
                              int goal_idx, int map_h) {
    result.reachable = true;
    int p = goal_idx;
    while (p != -1) {
        result.path.push_back(unpack(p, map_h));
        p = st.parent[p];
    }
    // Reverse so path is start→goal
    for (int i = 0; i < (int)result.path.size() / 2; i++)
        std::swap(result.path[i], result.path[result.path.size() - 1 - i]);
}

PathResult find_path(Vec2i start, Vec2i goal,
                     WalkableFn is_walkable,
                     int map_w, int map_h,
                     int max_steps) {
    PathResult result;
    const int size = map_w * map_h;

    AStarState st;
    st.init(size, pack(start.x, start.y, map_h), heuristic(start, goal));

    const int goal_idx = pack(goal.x, goal.y, map_h);
    while (!st.open.empty() && st.nodes_visited < max_steps) {
        AStarNode cur = st.open.top(); st.open.pop();
        const int ci = cur.idx;
        if (st.closed[ci]) continue;
        st.closed[ci] = true;
        st.nodes_visited++;

        if (ci == goal_idx) {
            result.nodes_visited = st.nodes_visited;
            astar_reconstruct(result, st, ci, map_h);
            return result;
        }
        astar_expand(st, ci, goal, is_walkable, map_w, map_h);
    }

    result.reachable = false;
    result.nodes_visited = st.nodes_visited;
    return result;
}

} // namespace ai
