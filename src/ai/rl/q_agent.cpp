// G8.4: QAgent — tabular Q-learning implementation
#include "ai/rl/q_agent.h"
#include <limits>
#include <algorithm>
#include <cstdio>
#include "nlohmann/json.hpp"

#ifdef _WIN32
#include <direct.h>
#define rl_mkdir_impl(p) _mkdir(p)
#else
#include <sys/stat.h>
#define rl_mkdir_impl(p) mkdir(p, 0755)
#endif

namespace rl {

// v0.9.36 G32-E: Q 表 key schema 版本。
// to_key() 曾两次扩字段(6→8), 旧存档的键格式与新格式不同, 混读会取到
// 别的状态的 Q 值。显式版本号让 load() 能明确拒绝而不是静默读脏。
static const int kQTableVersion = 2;

std::string QAgent::_make_key(const std::string& obs_key, int action_id) const {
    return obs_key + "|" + std::to_string(action_id);
}

double QAgent::_q_value(const std::string& obs_key, int action_id) const {
    auto it = _q.find(_make_key(obs_key, action_id));
    return it != _q.end() ? it->second : 0.0;
}

// v0.9.36 G32-E: 只在合法动作里取最优 (G32-E-B2)
// 旧实现遍历全部 COUNT 个动作, 会选中冷却中的技能 / 越界移动。
mcts::CombatAction QAgent::_best_action(
    const std::string& obs_key,
    const std::vector<mcts::CombatAction>& legal) const {
    mcts::CombatAction best = mcts::CombatAction::WAIT;
    double best_q = -std::numeric_limits<double>::max();
    for (auto a : legal) {
        double qv = _q_value(obs_key, (int)a);
        if (qv > best_q) { best_q = qv; best = a; }
    }
    return best;
}

// v0.9.36 G32-E: bootstrap 上界, 只遍历 next_legal (G32-E-B1)
// 未见过的 (s',a') 由 _q_value 返回 0, 故下界取 0(与旧实现语义一致)。
double QAgent::_bootstrap_max(
    const std::string& next_key,
    const std::vector<mcts::CombatAction>& next_legal) const {
    if (next_legal.empty()) return 0.0;
    double max_next = 0.0;
    for (auto a : next_legal)
        max_next = std::max(max_next, _q_value(next_key, (int)a));
    return max_next;
}

int QAgent::exploit_action(const Observation& obs,
                           const std::vector<mcts::CombatAction>& legal) const {
    if (legal.empty()) return -1;          // 无合法动作 → 不接管
    std::string key = obs.to_key();
    bool known = false;
    for (auto a : legal) {
        if (_q.find(_make_key(key, (int)a)) != _q.end()) { known = true; break; }
    }
    if (!known) return -1;   // 未见过该状态 → 不下接管 (镜像仲裁链落下级层)
    return (int)_best_action(key, legal);
}

mcts::CombatAction QAgent::select(const mcts::SimulationState& state, uint32_t& seed) {
    auto actions = mcts::get_possible_actions(state);
    if (actions.empty()) return mcts::CombatAction::WAIT;

    std::string obs_key = Observation::from_state(state).to_key();

    // Epsilon-greedy
    seed = seed * 1664525u + 1013904223u;
    double roll = (double)(seed & 0x7FFFFFFF) / 2147483648.0;
    if (roll < _epsilon) {
        // Explore: random action
        return actions[seed % actions.size()];
    }

    // Exploit: best known Q-value among possible actions
    mcts::CombatAction best = actions[0];
    double best_q = -std::numeric_limits<double>::max();
    for (auto a : actions) {
        double qv = _q_value(obs_key, (int)a);
        if (qv > best_q) { best_q = qv; best = a; }
    }
    return best;
}

void QAgent::update(const Observation& obs, mcts::CombatAction action,
                     double reward, const Observation& next_obs,
                     const std::vector<mcts::CombatAction>& next_legal,
                     bool done) {
    std::string s_key = obs.to_key();
    std::string sn_key = next_obs.to_key();

    double old_q = _q_value(s_key, (int)action);

    // Q3.15 (B1 fix): 终局转移不得自举 — target = reward
    // v0.9.36 G32-E: 非终局时 max 只在合法动作里取(旧实现遍历全部动作)
    double max_next = done ? 0.0 : _bootstrap_max(sn_key, next_legal);

    // Q3.15 (B2 fix): 学习率按访问次数衰减 — 表格 Q 收敛的必要条件
    std::string sa_key = _make_key(s_key, (int)action);
    int visits = _sa_visits[sa_key]++;
    double alpha_eff = _alpha / (1.0 + 0.05 * visits);

    double new_q = old_q + alpha_eff * (reward + _gamma * max_next - old_q);
    _q[sa_key] = new_q;
}

std::vector<QActionStats> QAgent::action_distribution() const {
    std::vector<QActionStats> stats;
    for (int i = 0; i < (int)mcts::CombatAction::COUNT; i++) {
        QActionStats s;
        s.action_name = mcts::action_name((mcts::CombatAction)i);
        s.count = 0; s.avg_q = 0;
        double sum_q = 0;
        for (auto& [key, qv] : _q) {
            if (key.find("|" + std::to_string(i)) != std::string::npos) {
                s.count++; sum_q += qv;
            }
        }
        if (s.count > 0) s.avg_q = sum_q / s.count;
        stats.push_back(s);
    }
    return stats;
}

bool QAgent::save(const std::string& path) const {
    std::string dir = path;
    size_t pos = dir.find_last_of("/\\");
    if (pos != std::string::npos) {
        dir = dir.substr(0, pos);
        if (!dir.empty()) rl_mkdir_impl(dir.c_str());
    }
    nlohmann::json j;
    j["v"] = kQTableVersion;
    for (auto& [key, qv] : _q) j["q"][key] = qv;
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fputs(j.dump(1).c_str(), f);
    fclose(f);
    return true;
}

bool QAgent::load(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    fclose(f);
    try {
        auto j = nlohmann::json::parse(text);
        if (!j.contains("q") || !j["q"].is_object()) return false;
        // v0.9.36 G32-E: key schema 已变(to_key 增 strongest/buff 两字段),
        // 旧存档的键与新键格式不同, 混读会错位取到别的状态的 Q 值 →
        // 明确拒绝, 让调用方回落到下级策略层而非带着错数据运行
        if (j.value("v", 1) != kQTableVersion) return false;
        _q.clear();
        for (auto it = j["q"].begin(); it != j["q"].end(); ++it)
            _q[it.key()] = it.value().get<double>();
    } catch (...) { return false; }
    return true;
}

} // namespace rl
