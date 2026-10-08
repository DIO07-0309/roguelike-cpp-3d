# -*- coding: utf-8 -*-
"""统一评测口径 — DQN / 固定策略 / 随机基线在**同一批种子**上比较。

为什么需要独立文件: 之前多次出现"训练脚本打印 86% 但贪心只有 0%"的口径
不一致 —— 训练胜率混了 epsilon 探索, 不可比。本脚本固定:
  - 同一批场景种子 (EVAL_SEED_BASE 起)
  - 纯贪心 (无探索)
  - 6 个变体均衡采样
  - 同时报告固定普攻与随机基线, 作为"环境可解性"与"学习增益"的参照
判定: DQN 必须显著高于两个基线才算有效。
"""
import argparse
import os
import sys

import numpy as np
import torch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import combat_env_py
from action_mask import legal_mask, masked_argmax, masked_random, build_input
from combat_env_py import CombatEnvironment, ATTACK
from dqn_model import DQN
from train_dqn import make_scenario, max_steps

EVAL_SEED_BASE = 500000
NAMES = {0: "attack", 1: "skill_1", 2: "skill_2", 3: "skill_3", 4: "skill_4",
         5: "move_up", 6: "move_down", 7: "move_left", 8: "move_right", 9: "wait"}


def evaluate(policy, per_variant=20, real_mode=True, collect_actions=False):
    combat_env_py.REAL_MODE = real_mode
    env = CombatEnvironment()
    wins, steps_sum = 0, 0
    counts = {}
    for v in range(6):
        for t in range(per_variant):
            idx = v * 1000 + t
            s = make_scenario(v, np.random.default_rng(EVAL_SEED_BASE + idx))
            s.seed = (EVAL_SEED_BASE + idx) & 0xFFFFFFFF
            env.reset(s)
            k = 0
            while not env.is_done() and k < max_steps():
                m = legal_mask(env)
                a = policy(env, m)
                if collect_actions:
                    counts[a] = counts.get(a, 0) + 1
                env.step(a)
                k += 1
            wins += env.state().victory
            steps_sum += k
    n = 6 * per_variant
    return wins * 100.0 / n, steps_sum / n, counts, n


def make_greedy(net):
    def pol(env, m):
        x = torch.FloatTensor(build_input(env.observation(), m)).unsqueeze(0)
        with torch.no_grad():
            q = net(x).squeeze(0).numpy()
        return masked_argmax(q, m)
    return pol


def pol_attack_only(env, m):
    return ATTACK if m[ATTACK] > 0 else 9


def pol_attack_skill(env, m):
    if m[ATTACK] > 0:
        return ATTACK
    for a in (1, 2, 3, 4):
        if m[a] > 0:
            return a
    return 9


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="tools/rl/dqn_real.pt")
    ap.add_argument("--per-variant", type=int, default=20)
    ap.add_argument("--toy", action="store_true", help="用玩具模式评测")
    args = ap.parse_args()

    real_mode = not args.toy
    combat_env_py.REAL_MODE = real_mode
    net = DQN()
    net.load_state_dict(torch.load(args.model, map_location="cpu"))
    net.eval()

    print(f"评测 {args.model}  模式={'REAL' if real_mode else 'TOY'}  "
          f"每变体 {args.per_variant} 局  预算 {max_steps()} 步")
    print(f"种子基址 {EVAL_SEED_BASE}\n")

    rows = []
    wr, st, cnt, n = evaluate(make_greedy(net), args.per_variant,
                              real_mode, collect_actions=True)
    rows.append(("DQN 贪心", wr, st))
    total = sum(cnt.values())
    print("  DQN 动作分布:")
    for a, c in sorted(cnt.items(), key=lambda kv: -kv[1])[:6]:
        print(f"      {NAMES.get(a,a):<11} {c*100.0/total:5.1f}%")

    for label, pol in (("固定普攻", pol_attack_only),
                       ("普攻+技能", pol_attack_skill)):
        wr2, st2, _, _ = evaluate(pol, args.per_variant, real_mode)
        rows.append((label, wr2, st2))

    print()
    for label, wr, st in rows:
        print(f"  {label:<12} 胜率 {wr:5.1f}%   平均步数 {st:5.1f}")

    best_baseline = max(w for l, w, _ in rows if l != "DQN 贪心")
    dqn = rows[0][1]
    print(f"\n  DQN vs strongest baseline({best_baseline:.1f}%): "
          f"{dqn - best_baseline:+.1f}pp")
    if dqn <= best_baseline:
        print("  [FAIL] DQN did not beat baseline - policy ineffective")
        return 1
    print("  [OK] DQN beat baseline")
    return 0


if __name__ == "__main__":
    sys.exit(main())