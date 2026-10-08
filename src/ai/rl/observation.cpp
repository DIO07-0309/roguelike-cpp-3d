// G8.4: Observation implementation
#include "ai/rl/observation.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace rl {

// v0.9.34 G32-C: 网格尺寸 — 与 mcts::get_possible_actions 的边界一致
static const float kGridW = 40.0f;
static const float kGridH = 30.0f;

Observation Observation::from_state(const mcts::SimulationState& state) {
    Observation obs;
    auto& p = state.player;
    obs.player_hp_ratio = p.max_hp > 0 ? p.hp / p.max_hp : 0;
    obs.player_attack = (float)p.attack;

    int alive = 0;
    float nearest = 999;
    float strongest = 0;
    bool has_boss = false;
    float nearest_x = -1, nearest_y = -1;   // v0.9.34 G32-C
    for (auto& m : state.monsters) {
        if (!m.alive) continue;
        alive++;
        float d = std::abs(m.x - p.x) + std::abs(m.y - p.y);
        // 严格小于: 与既有 nearest 判定一致, 保证记录的是同一只怪
        if (d < nearest) { nearest = d; nearest_x = m.x; nearest_y = m.y; }
        float m_hp_ratio = m.max_hp > 0 ? m.hp / m.max_hp : 0;
        if (m_hp_ratio > strongest) strongest = m_hp_ratio;
        if (m.is_boss) has_boss = true;
    }
    obs.enemy_count = (float)alive;
    obs.nearest_enemy_dist = nearest;
    obs.strongest_hp_ratio = strongest;
    obs.boss_present = has_boss ? 1.0f : 0;
    obs.buff_count = (float)p.buffs.size();
    obs.player_style = (float)state.player_style;  // F15.4

    // v0.9.34 G32-C: 位置 + 冷却维度 (归一化到 [0,1])
    obs.player_x = (p.x - 0) / kGridW;
    obs.player_y = (p.y - 0) / kGridH;
    obs.nearest_enemy_x = (nearest_x < 0) ? 0.5f : nearest_x / kGridW;
    obs.nearest_enemy_y = (nearest_y < 0) ? 0.5f : nearest_y / kGridH;
    obs.attack_cooldown_ratio = std::min(p.attack_cooldown / 2.0f, 1.0f);
    int ready = 0;
    for (int i = 0; i < 4; i++) if (p.skill_cooldowns[i] <= 0) ready++;
    obs.skill_ready_ratio = (float)ready / 4.0f;
    return obs;
}

std::vector<float> Observation::to_vector() const {
    // v0.9.34 G32-C: 8 → 14 维。位置与冷却维度是打通 DQN 的必要条件,
    // 见observation.h 里"位置维度"注释里的死循环证据。
    // 顺序必须与 Python observation_py.to_vector14 完全一致。
    return {player_hp_ratio, player_attack, enemy_count, nearest_enemy_dist,
            strongest_hp_ratio, boss_present, buff_count, player_style,
            player_x, player_y, nearest_enemy_x, nearest_enemy_y,
            attack_cooldown_ratio, skill_ready_ratio};
}

// v0.9.36 G32-E: 补齐 strongest_hp_ratio / buff_count
// 旧实现只编码 6 个字段, 仅在这两项上不同的状态会撞进同一 Q 表条目 ——
// 例如"最强怪满血但身上挂 3 个 buff"与"最强怪满血无 buff"共用一个 Q 值。
std::string Observation::to_key() const {
    int hp_b = (int)(player_hp_ratio * 10);           // 0-10
    int atk_b = (int)(player_attack / 5.0f);          // 0-6
    int ec_b = (int)enemy_count;                       // 0-5
    int nd_b = (int)(nearest_enemy_dist / 2.0f);      // 0-10
    int sh_b = (int)(strongest_hp_ratio * 10);        // 0-10
    int boss_b = (int)boss_present;
    int bf_b = (int)buff_count;                       // 上限 9, 防键长失控
    if (bf_b > 9) bf_b = 9;
    int style_b = (int)player_style;
    std::ostringstream ss;
    ss << hp_b << ":" << atk_b << ":" << ec_b << ":" << nd_b << ":" << sh_b
       << ":" << boss_b << ":" << bf_b << ":" << style_b;
    return ss.str();
}

} // namespace rl
