// v0.9.31 G31-P: Parity 轨迹导出器 — 为 Python 端 CombatEnvironment 移植提供黄金数据
// 用法: roguelike_cpp.exe --rl-parity <seed> <episodes> <out.json>
// 轨迹含每步的完整状态快照 + 观测 + 奖励, Python 端逐位比对。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "ai/rl/environment.h"
#include "ai/rl/observation.h"
#include "ai/rl/q_agent.h"
#include "ai/mcts/simulation_state.h"
#include "nlohmann/json.hpp"

using namespace rl;
using namespace mcts;

static SimulationState make_scenario() {
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.x = 5; s.player.y = 5;
    s.player.attack = 10; s.player.pdef = 3; s.player.mdef = 1;
    s.player.alive = true;
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 5, 6, 3, 1, 0, {}, true});
    s.monsters.push_back(MonsterSnapshot{"orc", 30, 30, 7, 5, 7, 3, 1, {}, true});
    return s;
}

static void dump_player(nlohmann::json& j, const PlayerSnapshot& p) {
    j["hp"] = p.hp; j["max_hp"] = p.max_hp;
    j["x"] = p.x;   j["y"] = p.y;
    j["attack"] = p.attack; j["pdef"] = p.pdef; j["mdef"] = p.mdef;
    j["attack_cooldown"] = p.attack_cooldown;
    j["skill_cooldowns"] = {p.skill_cooldowns[0], p.skill_cooldowns[1],
                            p.skill_cooldowns[2], p.skill_cooldowns[3]};
    j["alive"] = p.alive; j["buff_count"] = (int)p.buffs.size();
}

static void dump_monster(nlohmann::json& j, const MonsterSnapshot& m) {
    j["type"] = m.type; j["hp"] = m.hp; j["max_hp"] = m.max_hp;
    j["x"] = m.x; j["y"] = m.y;
    j["attack"] = m.attack; j["pdef"] = m.pdef; j["mdef"] = m.mdef;
    j["alive"] = m.alive; j["is_boss"] = m.is_boss;
}

// 动作序列发生器 — 每个 episode 用不同 seed 探索不同分支。
// 早期版本每步用同一个 s (seed+ep*7331+step*104729) 选动作, 前几步轨迹高度雷同,
// 覆盖面不足; 改为每步推进 LCG, 并对 ATTACK/技能/边界移动做定向加权,
// 确保轨迹穿过 d=1 (近战)、d=2 (仅技能)、技能冷却中、网格边界等分支。
struct ActionPicker {
    uint32_t s;
    int step = 0;
    explicit ActionPicker(uint32_t seed) : s(seed) {}
    uint32_t next_u32() { s = s * 1664525u + 1013904223u; return s; }
    // 加权采样: 偏向最能触发分支的动作 (按 seed 位决定权重)
    int pick(const std::vector<mcts::CombatAction>& legal, const SimulationState& st) {
        uint32_t r = next_u32();
        if (legal.empty()) return (int)mcts::CombatAction::WAIT;
        // 25% 概率强制尝试 ATTACK (触发近战伤害分支), 若不合法则退回均匀采样
        if ((r & 3u) == 0u) {
            for (auto a : legal)
                if (a == mcts::CombatAction::ATTACK) return (int)a;
        }
        // 25% 概率强制尝试技能 (触发 AoE + 冷却分支)
        if ((r & 3u) == 1u) {
            for (auto a : legal)
                if (a >= mcts::CombatAction::SKILL_1 && a <= mcts::CombatAction::SKILL_4)
                    return (int)a;
        }
        // 20% 概率强制移动 (逼近/远离, 改变 d 触发不同分支)
        if ((r & 7u) == 2u) {
            std::vector<mcts::CombatAction> moves;
            for (auto a : legal)
                if (a >= mcts::CombatAction::MOVE_UP && a <= mcts::CombatAction::MOVE_RIGHT)
                    moves.push_back(a);
            if (!moves.empty()) return (int)moves[(r >> 8) % moves.size()];
        }
        (void)st;
        return (int)legal[(r >> 16) % legal.size()];
    }
};

static void run_episodes(uint32_t seed, int episodes, nlohmann::json& trace) {
    CombatEnvironment env;
    for (int ep = 0; ep < episodes; ep++) {
        auto initial = make_scenario();
        // 场景多样化: 玩家/怪物坐标按 episode 变化, 覆盖贴身/中距/边界
        // 6 种变体 — 必须覆盖网格的全部 4 条边, 否则边界 mask 的
        // 上/下界常量改错parity_test 抓不到 (变异 GRID_W 40->39 曾漏网)
        int variant = ep % 6;
        initial.player.x = 5.0f + (float)(ep % 5);
        initial.player.y = 5.0f + (float)((ep / 5) % 4);
        switch (variant) {
        case 0:   // 贴身: slime 可近战 (ATTACK 伤害分支)
            initial.monsters[0].x = initial.player.x;
            initial.monsters[0].y = initial.player.y + 1;
            break;
        case 1:   // 中距: 仅技能可命中
            initial.monsters[0].x = initial.player.x + 2;
            initial.monsters[0].y = initial.player.y;
            break;
        case 2:   // 左上角 (0,0): 触发 x 下界 + y 下界
            initial.player.x = 0.0f; initial.player.y = 0.0f;
            initial.monsters[0].x = 1.0f; initial.monsters[0].y = 0.0f;
            break;
        case 3:   // 右下角 (39,29): 触发 x 上界 + y 上界
            initial.player.x = 39.0f; initial.player.y = 29.0f;
            initial.monsters[0].x = 38.0f; initial.monsters[0].y = 29.0f;
            break;
        case 4:   // 右边缘中段: 只触发 x 上界
            initial.player.x = 39.0f; initial.player.y = 12.0f;
            initial.monsters[0].x = 38.0f; initial.monsters[0].y = 12.0f;
            break;
        default:  // 上边缘中段: 只触发 y 上界
            initial.player.x = 20.0f; initial.player.y = 29.0f;
            initial.monsters[0].x = 20.0f; initial.monsters[0].y = 28.0f;
            break;
        }
        initial.rng.seed = seed + ep * 7919u;

        auto obs0 = env.reset(initial);
        nlohmann::json jep;
        jep["variant"] = variant;
        jep["obs0"] = obs0.to_vector();
        jep["steps"] = nlohmann::json::array();

        ActionPicker picker(seed + ep * 7331u);
        for (int step = 0; step < 60 && !env.is_done(); step++) {
            auto legal = get_possible_actions(env.state());
            if (legal.empty()) break;
            int action = picker.pick(legal, env.state());

            nlohmann::json jst;
            jst["t"] = step;
            jst["action"] = action;
            jst["action_name"] = action_name((mcts::CombatAction)action);
            jst["legal"] = std::vector<int>();
            for (auto a : legal) jst["legal"].push_back((int)a);
            jst["obs_before"] = Observation::from_state(env.state()).to_vector();

            auto sr = env.step((mcts::CombatAction)action);
            jst["reward"] = sr.reward;
            jst["terminal"] = sr.terminal;
            jst["obs_after"] = sr.observation.to_vector();
            dump_player(jst["player_after"], env.state().player);
            jst["monsters_after"] = nlohmann::json::array();
            for (auto& m : env.state().monsters) {
                nlohmann::json jm;
                dump_monster(jm, m);
                jst["monsters_after"].push_back(jm);
            }
            jep["steps"].push_back(jst);
        }
        jep["victory"] = env.state().victory;
        trace["episodes_data"].push_back(jep);
    }
}

int main(int argc, char** argv) {
    uint32_t seed = argc > 1 ? (uint32_t)atoi(argv[1]) : 42;
    int episodes = argc > 2 ? atoi(argv[2]) : 3;
    const char* out_path = argc > 3 ? argv[3] : "tools/rl/parity_trace.json";

    nlohmann::json trace;
    trace["seed"] = seed;
    trace["episodes"] = episodes;
    trace["contract"] = "uint32 LCG + float32; compare bit-exact";

    run_episodes(seed, episodes, trace);

    FILE* f = fopen(out_path, "w");
    if (!f) { printf("FAIL: cannot write %s\n", out_path); return 1; }
    std::string text = trace.dump(1);
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    int total = 0;
    for (auto& e : trace["episodes_data"]) total += (int)e["steps"].size();
    printf("parity trace written: %s (%d episodes / %d steps)\n",
           out_path, episodes, total);
    return 0;
}
