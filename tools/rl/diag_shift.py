# -*- coding: utf-8 -*-
"""状态分布诊断 — 为什么 Boss 域的决策分布和训练域完全不同?

domain_check.py 发现: DQN 在真实 Boss 战状态上选skill_4/skill_2 达48.5%,
而训练域里 skill 只占 20%, wait 占 37%。若差异来自"Boss 战是训练域的
外推", 那是域过拟合; 若来自"mask 差异"(训练域某些状态 skill 不合法),
那只是分布偏移, 可通过补训练样本修正。

本脚本区分这两种可能: 对同一批观测分别用
  (a) 训练域常见的 mask
  (b) Boss 域的 mask
 跑一次, 看动作分布对 mask 的敏感度。
"""
import os
import sys

import numpy as np
import torch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from action_mask import legal_mask, masked_argmax, build_input
from combat_env_py import CombatEnvironment, SimulationState, MonsterSnapshot
from dqn_model import DQN
from train_dqn import make_scenario

NAMES = {0: "attack", 1: "skill_1", 2: "skill_2", 3: "skill_3", 4: "skill_4",
         5: "move_up", 6: "move_down", 7: "move_left", 8: "move_right", 9: "wait"}


def act_distribution(net, obs_list, mask_list):
    counts = {}
    for obs, mask in zip(obs_list, mask_list):
        x = torch.FloatTensor(build_input(obs, mask)).unsqueeze(0)
        with torch.no_grad():
            q = net(x).squeeze(0).numpy()
        a = masked_argmax(q, mask)
        counts[a] = counts.get(a, 0) + 1
    return counts


def show(title, counts, n):
    print(f"\n  {title}")
    for a, c in sorted(counts.items(), key=lambda kv: -kv[1])[:5]:
        print(f"    {NAMES.get(a,a):<11} {c*100.0/n:5.1f}%")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    net = DQN()
    net.load_state_dict(torch.load(os.path.join(here, "dqn.pt"),
                                   map_location="cpu"))
    net.eval()
    env = CombatEnvironment()
    rng = np.random.default_rng(1)

    # 收集 300 个 Boss 域观测, 以及各自真实 mask 和"训练域典型 mask"
    obs_list, real_masks, train_masks = [], [], []
    for i in range(300):
        px = float(rng.integers(0, 40)); py = float(rng.integers(0, 30))
        dist = float(rng.uniform(1.0, 14.0))
        st = SimulationState(); p = st.player
        p.hp = float(rng.uniform(0.15, 1.0)) * 100.0
        p.max_hp = 100.0; p.x, p.y = px, py; p.attack = 10; p.alive = True
        st.monsters = [MonsterSnapshot("boss", 800.0, 800.0, px + dist, py,
                                       25, 10, 8, [], True, True)]
        st.player_style = int(rng.integers(0, 5))
        env.reset(st)
        obs_list.append(env.observation())
        real_masks.append(legal_mask(env))
        # 训练域典型 mask: 玩家贴怪(d=1), 普攻+技能全开
        train_masks.append(np.array([1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
                                    dtype=np.float32))

    n = len(obs_list)
    print(f"样本 {n} 个 Boss 域观测")
    show("(a) 用 Boss 域真实 mask", act_distribution(net, obs_list, real_masks), n)
    show("(b) 用训练域全开 mask",
         act_distribution(net, obs_list, train_masks), n)

    # 对照: 训练域观测 + 全开 mask
    tr_obs, tr_masks = [], []
    for i in range(300):
        s = make_scenario(i % 5, np.random.default_rng(8000 + i))
        s.seed = (42 + i * 7919) & 0xFFFFFFFF
        env.reset(s)
        tr_obs.append(env.observation())
        tr_masks.append(legal_mask(env))
    show("(c) 训练域观测 + 训练域真实 mask",
         act_distribution(net, tr_obs, tr_masks), n)

    print("\n--- 诊断 ---")
    print("  若 (a) 与 (b) 分布相近 → 差异来自观测外推 (域过拟合)")
    print("  若 (a) 与 (b) 差异大   → 差异来自 mask (分布偏移, 可修正)")
    print("  若 (c) 也偏向 skill     → 网络对 skill 有系统性偏置")


if __name__ == "__main__":
    main()