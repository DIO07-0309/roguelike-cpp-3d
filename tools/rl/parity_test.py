# -*- coding: utf-8 -*-
"""Parity 验证 — Python CombatEnvironment vs C++ 黄金轨迹逐位比对。

黄金轨迹由 tools/rl/parity_dump.cpp (C++ 原实现) 生成:
    build/tests/parity_dump.exe <seed> <episodes> tools/rl/parity_trace.json

比对项: 每步的 8 维观测、奖励、玩家快照 (含冷却)、怪物快照 (HP/位置/存活)
判定: float32 逐位相等 (bit-exact), 不容许容差 — 目的是防 train/serve skew,
      任何浮点语义差异都会让Python 训出的策略在 C++ 推理时失效。
"""
import json
import os
import sys
import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
from combat_env_py import (CombatEnvironment, SimulationState,
                           MonsterSnapshot, PlayerSnapshot,
                           ATTACK, SKILL_1, SKILL_4, MOVE_UP, MOVE_RIGHT, WAIT)

TRACE = os.path.join(_HERE, "parity_trace.json")


def build_scenario():
    s = SimulationState()
    p = s.player
    p.hp, p.max_hp, p.x, p.y = 100.0, 100.0, 5.0, 5.0
    p.attack, p.pdef, p.mdef, p.alive = 10, 3, 1, True
    s.monsters = [
        MonsterSnapshot("slime", 20, 20, 5, 6, 3, 1, 0, [], True, False),
        MonsterSnapshot("orc", 30, 30, 7, 5, 7, 3, 1, [], True, False),
    ]
    return s


def bits(x) -> int:
    return int(np.float32(x).view(np.uint32))


def cmp(name, got, want, path, errors):
    if isinstance(want, (int, float)) and not isinstance(want, bool):
        if bits(got) != bits(want):
            errors.append(f"{path}: {name} py={got!r}(0x{bits(got):08x}) "
                          f"cpp={want!r}(0x{bits(want):08x})")
    elif got != want:
        errors.append(f"{path}: {name} py={got!r} cpp={want!r}")


def build_scenario(variant: int, ep_idx: int):
    """场景必须与 parity_dump.cpp::run_episodes 的多样化逻辑逐位一致"""
    s = SimulationState()
    p = s.player
    p.hp, p.max_hp, p.x, p.y = 100.0, 100.0, 5.0, 5.0
    p.attack, p.pdef, p.mdef, p.alive = 10, 3, 1, True
    s.monsters = [
        MonsterSnapshot("slime", 20, 20, 5, 6, 3, 1, 0, [], True, False),
        MonsterSnapshot("orc", 30, 30, 7, 5, 7, 3, 1, [], True, False),
    ]
    p.x = 5.0 + float(ep_idx % 5)
    p.y = 5.0 + float((ep_idx // 5) % 4)
    if variant == 0:                       # 贴身: slime 可近战
        s.monsters[0].x, s.monsters[0].y = p.x, p.y + 1.0
    elif variant == 1:                     # 中距: 仅技能可命中
        s.monsters[0].x, s.monsters[0].y = p.x + 2.0, p.y
    elif variant == 2:                     # 左上角 (0,0)
        p.x, p.y = 0.0, 0.0
        s.monsters[0].x, s.monsters[0].y = 1.0, 0.0
    elif variant == 3:                     # 右下角 (39,29)
        p.x, p.y = 39.0, 29.0
        s.monsters[0].x, s.monsters[0].y = 38.0, 29.0
    elif variant == 4:                     # 右边缘中段
        p.x, p.y = 39.0, 12.0
        s.monsters[0].x, s.monsters[0].y = 38.0, 12.0
    else:                                  # 上边缘中段
        p.x, p.y = 20.0, 29.0
        s.monsters[0].x, s.monsters[0].y = 20.0, 28.0
    return s


class ActionPicker:
    """照抄 parity_dump.cpp::ActionPicker — 加权采样, 保证分支覆盖"""

    M32 = 0xFFFFFFFF

    def __init__(self, seed: int):
        self.s = seed & self.M32
        self.step = 0

    def next_u32(self) -> int:
        self.s = (self.s * 1664525 + 1013904223) & self.M32
        return self.s

    def pick(self, legal: list) -> int:
        r = self.next_u32()
        if not legal:
            return WAIT
        if (r & 3) == 0:
            for a in legal:
                if a == ATTACK:
                    return a
        if (r & 3) == 1:
            for a in legal:
                if SKILL_1 <= a <= SKILL_4:
                    return a
        if (r & 7) == 2:
            moves = [a for a in legal if MOVE_UP <= a <= MOVE_RIGHT]
            if moves:
                return moves[(r >> 8) % len(moves)]
        return legal[(r >> 16) % len(legal)]


def check_episode(jep, ep_idx, seed, errors):
    initial = build_scenario(jep["variant"], ep_idx)
    initial.seed = seed + ep_idx * 7919
    env = CombatEnvironment()
    obs0 = env.reset(initial)
    for i, v in enumerate(jep["obs0"]):
        cmp(f"obs0[{i}]", obs0[i], v, f"ep{ep_idx}.obs0", errors)

    picker = ActionPicker(seed + ep_idx * 7331)
    for jst in jep["steps"]:
        t = jst["t"]
        legal = env.possible_actions()
        cmp("legal", legal, jst["legal"], f"ep{ep_idx}.t{t}", errors)
        obs_before = env.observation()
        for i, v in enumerate(jst["obs_before"]):
            cmp(f"obs_before[{i}]", obs_before[i], v, f"ep{ep_idx}.t{t}", errors)

        picked = picker.pick(legal)
        cmp("picker", picked, jst["action"], f"ep{ep_idx}.t{t}", errors)
        sr = env.step(picked)
        path = f"ep{ep_idx}.t{t}"
        for i, v in enumerate(jst["obs_after"]):
            cmp(f"obs_after[{i}]", sr.observation[i], v, path, errors)
        cmp("reward", sr.reward, jst["reward"], path, errors)
        cmp("terminal", sr.terminal, jst["terminal"], path, errors)

        pa = jst["player_after"]
        p = env.state().player
        cmp("p.hp", p.hp, pa["hp"], path, errors)
        cmp("p.x", p.x, pa["x"], path, errors)
        cmp("p.y", p.y, pa["y"], path, errors)
        cmp("p.atk_cd", p.attack_cooldown, pa["attack_cooldown"], path, errors)
        for k in range(4):
            cmp(f"p.skill_cd[{k}]", p.skill_cooldowns[k],
                pa["skill_cooldowns"][k], path, errors)
        cmp("p.alive", p.alive, pa["alive"], path, errors)

        for k, jm in enumerate(jst["monsters_after"]):
            m = env.state().monsters[k]
            cmp(f"m{k}.hp", m.hp, jm["hp"], path, errors)
            cmp(f"m{k}.x", m.x, jm["x"], path, errors)
            cmp(f"m{k}.y", m.y, jm["y"], path, errors)
            cmp(f"m{k}.alive", m.alive, jm["alive"], path, errors)


def main() -> int:
    with open(TRACE, encoding="utf-8") as f:
        trace = json.load(f)
    seed, n_ep = trace["seed"], trace["episodes"]
    errors: list[str] = []
    for ep_idx, jep in enumerate(trace["episodes_data"]):
        check_episode(jep, ep_idx, seed, errors)

    total_steps = sum(len(e["steps"]) for e in trace["episodes_data"])
    if errors:
        print(f"PARITY FAIL — {len(errors)} mismatches "
              f"({n_ep} episodes / {total_steps} steps)")
        for e in errors[:25]:
            print("  " + e)
        if len(errors) > 25:
            print(f"  ... and {len(errors) - 25} more")
        return 1
    print(f"PARITY OK — {n_ep} episodes / {total_steps} steps bit-exact "
          f"(uint32 LCG + float32)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
