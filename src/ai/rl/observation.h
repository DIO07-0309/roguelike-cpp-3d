#pragma once
// G8.4: Observation — fixed-length feature vector from SimulationState.
// Used by: QAgent (key hashing), RL DQN/PPO (neural network input).

#include <vector>
#include <string>
#include <cstdint>
#include "ai/mcts/simulation_state.h"

namespace rl {

struct Observation {
    float player_hp_ratio = 1.0f;      // 0.0-1.0
    float player_attack = 10.0f;       // raw attack value
    float enemy_count = 0;             // number of alive enemies
    float nearest_enemy_dist = 99.0f;  // Manhattan distance to nearest
    float strongest_hp_ratio = 0;      // strongest enemy's HP ratio
    float boss_present = 0;            // 0 or 1
    float buff_count = 0;              // number of active buffs
    float player_style = 0;            // F15.4: 0=none,1=aggressive,2=defensive,3=sniper,4=mage

    // v0.9.34 G32-C: 位置维度 — 补齐G31 识别的 POMDP 缺陷
    //
    // 为什么必须加: 原 8 维里**没有任何位置信息**。DQN 实测出现死循环:
    //   网络学出 wait 68.9%, 而 WAIT 不改变任何观测量 → 状态完全冻结 →
    //   Q 值每帧完全相同 → 重复同一决策 → 卡在冷却里出不来。
    //   逐帧追踪证据: k=3 之后每步 ATK/DOWN/SK3/WAIT 的 Q 值小数点后 4 位
    //   完全一致, 而玩家 HP 每步 -4。移动能力必须可观测。
    //
    // 归一化: 网格 40x30, 除以 GRID_W / GRID_H 映射到 [0,1]
    float player_x = 0.5f;             // 玩家 x / GRID_W
    float player_y = 0.5f;             // 玩家 y / GRID_H
    float nearest_enemy_x = 0.5f;      // 最近敌人 x / GRID_W (无敌人时 0.5)
    float nearest_enemy_y = 0.5f;      // 最近敌人 y / GRID_H

    // v0.9.34: 冷却维度 — 让网络能判断"现在能不能攻击"
    // 原 8 维缺 attack_cooldown 与 skill_cooldowns, 是"合法性 POMDP"的
    // 直接来源: mask 虽已喂进网络(10维), 但网络无法从状态本身预测未来
    // 的合法性变化, 学不会"冷却将好时该准备输出"。
    float attack_cooldown_ratio = 0.0f;   // attack_cooldown / 2.0 上限截断
    float skill_ready_ratio = 1.0f;       // 就绪技能数 / 4

    // Convert to flat float vector (for neural net input / hash key)
    std::vector<float> to_vector() const;

    // Hash key for Q-table lookup
    std::string to_key() const;

    static Observation from_state(const mcts::SimulationState& state);

    // 向量长度 — 14 (原 8 + 位置 4 + 冷却 2)
    static constexpr int kVectorDim = 14;
};

} // namespace rl
