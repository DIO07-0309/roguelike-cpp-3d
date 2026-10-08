# -*- coding: utf-8 -*-
"""Observation 的特征提取 — 对齐 ai/rl/observation.cpp::from_state + to_vector。

★ 维度 8 → 14 (v0.9.34 G32-C)。顺序必须与 C++ 完全一致:
    0 hp_ratio    1 attack       2 enemy_count  3 nearest_dist
    4 strongest   5 boss         6 buff_count   7 style
    8 player_x    9 player_y    10 enemy_x     11 enemy_y
   12 atk_cd_ratio 13 skill_ready_ratio

为什么加位置与冷却维度: DQN 实测死循环 — 网络学出 wait 68.9%, 而 WAIT
不改变任何观测量 → 状态冻结 → Q 值每帧完全相同 → 重复同一决策 → 卡在冷却
里出不来 (逐帧证据: k=3 之后各动作 Q 值小数点后 4 位一致, 而玩家 HP 每步-4)。
移动能力必须可观测, 这是打通 DQN 的必要条件。
"""
import numpy as np

F32 = np.float32

GRID_W, GRID_H = 40.0, 30.0     # 必须与 C++ observation.cpp 的 kGridW/kGridH 一致


def to_vector14(state) -> np.ndarray:
    """14 维 float32 特征 — 与 C++ Observation::to_vector 逐位对应"""
    p = state.player
    hp_ratio = F32(p.hp / p.max_hp) if p.max_hp > 0 else F32(0.0)

    alive = 0
    nearest = F32(999.0)
    strongest = F32(0.0)
    has_boss = False
    nearest_x = F32(-1.0)
    nearest_y = F32(-1.0)
    for m in state.monsters:
        if not m.alive:
            continue
        alive += 1
        # C++ 用 std::abs(float), 返回 float; Python abs() 走 double 语义,
        # 表达式里混 F32 会被提升成 float64 → 距离差 1 格。全程显式转换。
        dx = F32(abs(F32(m.x) - F32(p.x)))
        dy = F32(abs(F32(m.y) - F32(p.y)))
        d = F32(dx + dy)
        if d < nearest:
            nearest = d
            nearest_x = F32(m.x)
            nearest_y = F32(m.y)
        m_hp_ratio = F32(m.hp / m.max_hp) if m.max_hp > 0 else F32(0.0)
        if m_hp_ratio > strongest:
            strongest = m_hp_ratio
        if m.is_boss:
            has_boss = True

    # 位置与冷却 (归一化规则与 C++ 完全一致)
    player_x = F32(F32(p.x) / F32(GRID_W))
    player_y = F32(F32(p.y) / F32(GRID_H))
    enemy_x = F32(0.5) if nearest_x < 0 else F32(nearest_x / F32(GRID_W))
    enemy_y = F32(0.5) if nearest_y < 0 else F32(nearest_y / F32(GRID_H))
    atk_cd = F32(min(F32(p.attack_cooldown) / F32(2.0), F32(1.0)))
    ready = 0
    for i in range(4):
        if p.skill_cooldowns[i] <= 0:
            ready += 1
    skill_ready = F32(ready / 4.0)

    return np.array([
        F32(hp_ratio),                    # 0
        F32(p.attack),                    # 1
        F32(alive),                       # 2
        F32(nearest),                     # 3
        F32(strongest),                   # 4
        F32(1.0 if has_boss else 0.0),    # 5
        F32(len(p.buffs)),                # 6
        F32(state.player_style),          # 7
        player_x,                         # 8
        player_y,                         # 9
        enemy_x,                          # 10
        enemy_y,                          # 11
        atk_cd,                           # 12
        skill_ready,                      # 13
    ], dtype=np.float32)


# 向后兼容别名 — 旧调用点用to_vector8
def to_vector8(state) -> np.ndarray:
    return to_vector14(state)[:8]