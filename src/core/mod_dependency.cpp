#include "mod_dependency.h"
#include "core/logger.h"
#include <algorithm>
#include <queue>

// ============================================================
// G4.2: DependencyResolver — Kahn 拓扑排序 + 循环检测
// ============================================================

// G13: 缺依赖检查 — 硬依赖不在表内则该 Mod 整体跳过 (只报一次)
static void _collect_missing_deps(const std::vector<ModDepInfo>& mods,
                                  const std::unordered_map<std::string, int>& id_to_idx,
                                  std::vector<bool>& skipped, DepResult& result) {
    for (size_t i = 0; i < mods.size(); i++) {
        for (auto& req : mods[i].requires_ids) {
            if (id_to_idx.count(req)) continue;
            if (skipped[i]) continue;
            LOG_INFO("[Dependency] Skipping '%s': requires '%s' not found",
                     mods[i].id.c_str(), req.c_str());
            result.skipped.push_back(mods[i].id);
            skipped[i] = true;
        }
    }
}

// G13: 建边 — 边方向 A load_after B / A requires B → B → A (B 必须先于 A)
static void _build_edges(const std::vector<ModDepInfo>& mods,
                         const std::unordered_map<std::string, int>& id_to_idx,
                         const std::vector<bool>& skipped,
                         std::vector<std::vector<int>>& adj, std::vector<int>& indegree) {
    const int n = (int)mods.size();
    for (int i = 0; i < n; i++) {
        if (skipped[i]) continue;
        for (auto& req : mods[i].requires_ids) {
            auto it = id_to_idx.find(req);
            if (it == id_to_idx.end() || skipped[it->second]) continue;
            adj[it->second].push_back(i);
            indegree[i]++;
        }
        for (auto& la : mods[i].load_after) {
            auto it = id_to_idx.find(la);
            if (it == id_to_idx.end() || skipped[it->second]) continue;
            adj[it->second].push_back(i);
            indegree[i]++;
        }
    }
}

// G13: Kahn 拓扑排序 (入度为 0 者入队, 出队即定序)
static void _topo_sort(const std::vector<ModDepInfo>& mods,
                       const std::vector<bool>& skipped,
                       std::vector<std::vector<int>>& adj, std::vector<int>& indegree,
                       DepResult& result) {
    const int n = (int)mods.size();
    std::queue<int> q;
    for (int i = 0; i < n; i++)
        if (!skipped[i] && indegree[i] == 0)
            q.push(i);

    while (!q.empty()) {
        const int u = q.front(); q.pop();
        result.ordered_ids.push_back(mods[u].id);
        for (int v : adj[u])
            if (--indegree[v] == 0) q.push(v);
    }
}

// G13: 剩余节点 = 循环依赖
static void _report_cycles(const std::vector<ModDepInfo>& mods,
                           const std::vector<bool>& skipped,
                           const std::vector<int>& indegree, DepResult& result) {
    for (int i = 0; i < (int)mods.size(); i++) {
        if (skipped[i] || indegree[i] <= 0) continue;
        LOG_INFO("[Dependency] Skipping '%s': cyclic dependency detected",
                 mods[i].id.c_str());
        result.cycle_info.push_back(mods[i].id);
    }
}

DepResult DependencyResolver::resolve(const std::vector<ModDepInfo>& mods) {
    DepResult result;
    if (mods.empty()) return result;

    // ── 构建 ID → index 映射 ──
    std::unordered_map<std::string, int> id_to_idx;
    for (size_t i = 0; i < mods.size(); i++)
        id_to_idx[mods[i].id] = (int)i;

    std::vector<bool> skipped(mods.size(), false);
    _collect_missing_deps(mods, id_to_idx, skipped, result);

    std::vector<std::vector<int>> adj(mods.size());
    std::vector<int> indegree(mods.size(), 0);
    _build_edges(mods, id_to_idx, skipped, adj, indegree);

    _topo_sort(mods, skipped, adj, indegree, result);
    _report_cycles(mods, skipped, indegree, result);

    if (!result.ordered_ids.empty())
        LOG_INFO("[Dependency] Load order: %zu mods", result.ordered_ids.size());
    return result;
}

