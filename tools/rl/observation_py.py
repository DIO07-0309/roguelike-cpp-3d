# -*- coding: utf-8 -*-
"""Observation 的 8 维特征提取 — 对齐 ai/rl/observation.cpp::from_state + to_vector。

顺序必须与 C++ 完全一致:
    player_hp_ratio, player_attack, enemy_count, nearest_enemy_dist,
    strongest_hp_ratio, boss_present, buff_count, player_style
"""
import numpy as np

F32 = np.float32


def to_vector8(state) -> np.ndarray:
    p = state.player
    hp_ratio = F32(p.hp / p.max_hp) if p.max_hp > 0 else F32(0.0)

    alive = 0
    nearest = F32(999.0)
    strongest = F32(0.0)
    has_boss = False
    for m in state.monsters:
        if not m.alive:
            continue
        alive += 1
        # C++ 用 std::abs(float),返回 float; Python 的 abs() 走 double 语义,
        # 表达式里混 F32 会被提升成 float64 → 距离差 1 格。这里全程 F32 显式转换。
        d = F32(abs(F32(m.x) - F32(p.x))) + F32(abs(F32(m.y) - F32(p.y)))
        d = F32(d)
        if d < nearest:
            nearest = d
        m_hp_ratio = F32(m.hp / m.max_hp) if m.max_hp > 0 else F32(0.0)
        if m_hp_ratio > strongest:
            strongest = m_hp_ratio
        if m.is_boss:
            has_boss = True

    return np.array([
        F32(hp_ratio),                    # 0 player_hp_ratio
        F32(p.attack),                    # 1 player_attack
        F32(alive),                       # 2 enemy_count
        F32(nearest),                     # 3 nearest_enemy_dist
        F32(strongest),                   # 4 strongest_hp_ratio
        F32(1.0 if has_boss else 0.0),    # 5 boss_present
        F32(len(p.buffs)),                # 6 buff_count
        F32(state.player_style),          # 7 player_style
    ], dtype=np.float32)
