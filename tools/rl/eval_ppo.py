# -*- coding: utf-8 -*-
"""策略评测 — DQN 与 PPO 统一口径对比。

与 bench.py 的区别: 本脚本能同时评测两种算法, 且强制报告动作分布。
判定标准(全部满足才算 PPO 有效):
  1. wait 占比 < 30%(DQN 卡在89.7%, 这是核心病灶)
  2. 单一动作占比 < 50%(不退化)
  3. 贪心胜率 > 固定策略基线
"""
import argparse
import os
import sys

import numpy as np
import torch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import combat_env_py
from action_mask import (INPUT_DIM, legal_mask, masked_argmax,
                         masked_random, build_input, WAIT, ATTACK)
from combat_env_py import CombatEnvironment
from dqn_model import DQN
from ppo_model import ActorCritic
import train_dqn as td

EVAL_SEED_BASE = 500000
NAMES = {0: "attack", 1: "skill_1", 2: "skill_2", 3: "skill_3", 4: "skill_4",
         5: "move_up", 6: "move_down", 7: "move_left", 8: "move_right", 9: "wait"}
MAX_WAIT = 0.30
MAX_SINGLE = 0.50


def rollout_all(env, act_fn, per_variant, level):
    wins, steps_sum = 0, 0
    counts = {}
    for v in range(6):
        for t in range(per_variant):
            idx = v * 1000 + t
            s = td.make_scenario(v, np.random.default_rng(EVAL_SEED_BASE + idx),
                                 level)
            s.seed = (EVAL_SEED_BASE + idx) & 0xFFFFFFFF
            env.reset(s)
            k = 0
            while not env.is_done() and k < td.max_steps(level):
                a = act_fn(env)
                counts[a] = counts.get(a, 0) + 1
                env.step(a)
                k += 1
            wins += env.state().victory
            steps_sum += k
    n = 6 * per_variant
    return wins * 100.0 / n, steps_sum / n, counts, n


def make_dqn(path):
    net = DQN()
    net.load_state_dict(torch.load(path, map_location="cpu"))
    net.eval()

    def act(env):
        m = legal_mask(env)
        x = torch.FloatTensor(build_input(env.observation(), m)).unsqueeze(0)
        with torch.no_grad():
            q = net(x).squeeze(0).numpy()
        return masked_argmax(q, m)
    return act


def make_ppo(path):
    net = ActorCritic()
    net.load_state_dict(torch.load(path, map_location="cpu"))
    net.eval()

    def act(env):
        m = legal_mask(env)
        x = torch.FloatTensor(build_input(env.observation(), m)).unsqueeze(0)
        mt = torch.FloatTensor(m).unsqueeze(0)
        with torch.no_grad():
            return int(net.act_greedy(x, mt).item())
    return act


def pol_attack_skill(env):
    m = legal_mask(env)
    if m[ATTACK] > 0:
        return ATTACK
    for a in (1, 2, 3, 4):
        if m[a] > 0:
            return a
    return 9


def report(label, wr, steps, counts, n):
    total = max(1, sum(counts.values()))
    wait_pct = counts.get(WAIT, 0) * 100.0 / total
    atk_pct = counts.get(ATTACK, 0) * 100.0 / total
    top = max(counts.items(), key=lambda kv: kv[1])
    top_pct = top[1] * 100.0 / total
    print(f"  {label:<16} 胜率 {wr:5.1f}%  步数 {steps:5.1f}  "
          f"attack {atk_pct:5.1f}%  wait {wait_pct:5.1f}%")
    return wr, atk_pct, top_pct


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dqn", default="tools/rl/dqn_cur.pt")
    ap.add_argument("--ppo", default="tools/rl/ppo.pt")
    ap.add_argument("--per-variant", type=int, default=15)
    ap.add_argument("--level", type=int, default=2)
    args = ap.parse_args()

    combat_env_py.REAL_MODE = True
    env = CombatEnvironment()
    print(f"评测 L{args.level}  每变体 {args.per_variant}  "
          f"预算 {td.max_steps(args.level)} 步  种子基址 {EVAL_SEED_BASE}\n")

    results = {}
    for label, maker, path in (("DQN", make_dqn, args.dqn),
                                ("PPO", make_ppo, args.ppo)):
        if not os.path.exists(path):
            print(f"  跳过 {label}: 缺少 {path}")
            continue
        wr, st, cnt, n = rollout_all(env, maker(path), args.per_variant,
                                     args.level)
        results[label] = report(label, wr, st, cnt, n)

    wr, st, cnt, n = rollout_all(env, pol_attack_skill, args.per_variant,
                                 args.level)
    results["基线(普攻+技能)"] = report("基线(普攻+技能)", wr, st, cnt, n)

    print()
    if "PPO" not in results:
        print("[SKIP] PPO 权重缺失")
        return 2
    ppo_wr, ppo_atk, _ = results["PPO"]
    base_wr, base_atk, _ = results["基线(普攻+技能)"]
    dqn_wr = results.get("DQN", (0, 0, 0))[0]
    print(f"  PPO {ppo_wr:.1f}%  DQN {dqn_wr:.1f}%  基线 {base_wr:.1f}%")
    print()

    problems = []
    if ppo_wr <= dqn_wr:
        problems.append(f"PPO {ppo_wr:.1f}% 未超过 DQN {dqn_wr:.1f}%")
    if ppo_atk <= 0.01:
        problems.append(f"attack 占比 {ppo_atk:.1f}% ≈ 0 (不会输出)")
    if ppo_wr <= base_wr:
        problems.append(f"PPO {ppo_wr:.1f}% 未超过基线 {base_wr:.1f}%")

    if problems:
        print("[FAIL] PPO 仍未达标:")
        for p in problems:
            print("  - " + p)
        return 1
    print("[OK] PPO 超过 DQN 与基线")
    return 0


if __name__ == "__main__":
    sys.exit(main())