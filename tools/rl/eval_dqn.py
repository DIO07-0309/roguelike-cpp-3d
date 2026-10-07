# -*- coding: utf-8 -*-
"""DQN 评估 — 贪心策略 vs 随机基线, 在独立 seed 集上跑。

用途:
  1. 训练脚本打印的胜率混了 epsilon 探索, 不能反映部署时的贪心策略水平
  2. 必须与随机基线对比 —— 胜率 60% 在"随机就能赢"的环境里毫无意义
     (第一版环境就是这样: DQN 12.7% 反而远低于随机的 58.3%)
  3. 用**未参与训练**的 seed, 避免自评自证

    python tools/rl/eval_dqn.py --model tools/rl/dqn.onnx --episodes 600
"""
import argparse
import os
import sys

import numpy as np
import torch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from action_mask import legal_mask, masked_argmax, masked_random, build_input
from combat_env_py import CombatEnvironment
from dqn_model import DQN
import train_dqn
from train_dqn import REWARD_SCALE, make_scenario

EVAL_SEED_BASE = 900000   # 与训练 seed (42 + ep*7919) 不重叠


def load_model(path: str):
    net = DQN()
    if path.endswith(".onnx"):
        # onnx 包未安装时退回 .pt; 导出优先用 onnx, 但评估用 pt 更省依赖
        pt = path.replace(".onnx", ".pt")
        if not os.path.exists(pt):
            raise FileNotFoundError(f"需要 {path} 或 {pt}")
        path = pt
    net.load_state_dict(torch.load(path, map_location="cpu"))
    net.eval()
    return net


def greedy_action(net, obs, mask) -> int:
    with torch.no_grad():
        x = build_input(obs, mask)
        q = net(torch.FloatTensor(x).unsqueeze(0)).squeeze(0).numpy()
    return masked_argmax(q, mask)


def rollout(env, net, variant, seed, epsilon=0.0, rng=None):
    s = make_scenario(variant, np.random.default_rng(seed))
    s.seed = (seed * 7919 + 13) & 0xFFFFFFFF
    env.reset(s)
    steps = 0
    while not env.is_done() and steps < train_dqn.max_steps():
        mask = legal_mask(env)
        if epsilon > 0 and rng.random() < epsilon:
            a = masked_random(rng, mask)
        else:
            a = greedy_action(net, env.observation(), mask)
        env.step(a)
        steps += 1
    return env.state().victory, steps


def evaluate(net, episodes: int, label: str):
    env = CombatEnvironment()
    rng = np.random.default_rng(12345)
    wins, steps_sum = 0, 0
    for i in range(episodes):
        w, st = rollout(env, net, i % 5, EVAL_SEED_BASE + i * 31)
        wins += w
        steps_sum += st
    wr = wins * 100.0 / episodes
    print(f"  {label:<18} 胜率 {wr:5.1f}%  ({wins}/{episodes})  "
          f"平均步数 {steps_sum/episodes:.1f}")
    return wr


def evaluate_random(episodes: int):
    env = CombatEnvironment()
    rng = np.random.default_rng(12345)
    wins, steps_sum = 0, 0
    for i in range(episodes):
        w, st = rollout(env, None, i % 5, EVAL_SEED_BASE + i * 31,
                        epsilon=1.0, rng=rng)
        wins += w
        steps_sum += st
    wr = wins * 100.0 / episodes
    print(f"  {'随机基线':<18} 胜率 {wr:5.1f}%  ({wins}/{episodes})  "
          f"平均步数 {steps_sum/episodes:.1f}")
    return wr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="tools/rl/dqn.onnx")
    ap.add_argument("--episodes", type=int, default=600)
    args = ap.parse_args()

    net = load_model(args.model)
    print(f"评估 {args.model} (eval seed base={EVAL_SEED_BASE}, "
          f"reward_scale={REWARD_SCALE})\n")
    base = evaluate_random(args.episodes)
    dqn = evaluate(net, args.episodes, "DQN 贪心")
    delta = dqn - base
    print(f"\n  DQN 相对随机: {delta:+.1f} 个百分点")
    if delta < 2.0:
        print("  ⚠️ DQN 未显著优于随机 — 检查场景是否可赢 / reward 设计")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
