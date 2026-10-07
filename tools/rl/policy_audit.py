# -*- coding: utf-8 -*-
"""动作分布体检 — 训练完成后检查策略是否退化。

存在的理由: domain_check.py 曾测出 DQN 在 Boss 域"48.5% 放技能、1% 普攻",
而当时训练时打印的胜率是 73% —— 胜率高完全掩盖了策略失衡。单看胜率
无法判断策略是否可用, 必须看动作分布。

判定标准 (阈值可按需调整, 但理由写在这里):
  - 单一动作占比 > 50%  → 策略退化为常量输出, 不可用
  - wait 占比 > 30%     → 大概率在"等冷却"而非主动决策
  - attack 占比 < 5%    → 输出窗口过窄, 真实战斗里会显得迟钝
  以上任一命中即返回非零退出码。
"""
import argparse
import os
import sys

import numpy as np
import torch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import combat_env_py
from action_mask import legal_mask, masked_argmax, build_input
from combat_env_py import CombatEnvironment, SimulationState, MonsterSnapshot
from dqn_model import DQN

NAMES = {0: "attack", 1: "skill_1", 2: "skill_2", 3: "skill_3", 4: "skill_4",
         5: "move_up", 6: "move_down", 7: "move_left", 8: "move_right", 9: "wait"}

MAX_SINGLE = 0.50     # 单一动作占比上限
MAX_WAIT = 0.30       # wait 占比上限
MIN_ATTACK = 0.05     # attack 占比下限


def sample_states(net, n, seed_base, real_domain):
    """收集 (obs, mask) 样本 — real_domain=True 时用 Boss 战分布"""
    env = CombatEnvironment()
    rng = np.random.default_rng(seed_base)
    out = []
    for i in range(n):
        st = SimulationState()
        p = st.player
        if real_domain:
            px = float(rng.integers(0, 40)); py = float(rng.integers(0, 30))
            dist = float(rng.uniform(1.0, 14.0))
            p.hp = float(rng.uniform(0.15, 1.0)) * 100.0
            p.max_hp = 100.0
            p.x, p.y = px, py
            st.monsters = [MonsterSnapshot("boss", 800.0, 800.0,
                                           px + dist, py, 25, 10, 8, [], True, True)]
            st.player_style = int(rng.integers(0, 5))
        else:
            from train_dqn import make_scenario
            st = make_scenario(i % 5, np.random.default_rng(seed_base + i))
            st.seed = (42 + i * 7919) & 0xFFFFFFFF
        env.reset(st)
        m = legal_mask(env)
        x = torch.FloatTensor(build_input(env.observation(), m)).unsqueeze(0)
        with torch.no_grad():
            q = net(x).squeeze(0).numpy()
        out.append((masked_argmax(q, m), q))
    return out


def check(net, label, n=600, real_domain=True):
    samples = sample_states(net, n, 900000, real_domain)
    counts = {}
    for a, _ in samples:
        counts[a] = counts.get(a, 0) + 1
    qs = np.array([q for _, q in samples])

    print(f"\n--- {label} ---")
    print(f"  样本 {n}, Q 值范围 [{qs.min():.3f}, {qs.max():.3f}] "
          f"均值 {qs.mean():.3f}")
    ranked = sorted(counts.items(), key=lambda kv: -kv[1])
    for a, c in ranked:
        pct = c * 100.0 / n
        flag = " <<<" if pct > MAX_SINGLE * 100 else ""
        print(f"    {NAMES.get(a, a):<12} {c:4d}  {pct:5.1f}%{flag}")

    wait_pct = counts.get(9, 0) * 100.0 / n
    atk_pct = counts.get(0, 0) * 100.0 / n
    top_pct = ranked[0][1] * 100.0 / n
    problems = []
    if top_pct > MAX_SINGLE * 100:
        problems.append(f"单一动作占比 {top_pct:.1f}% > {MAX_SINGLE*100:.0f}%")
    if wait_pct > MAX_WAIT * 100:
        problems.append(f"wait 占比 {wait_pct:.1f}% > {MAX_WAIT*100:.0f}%")
    if atk_pct < MIN_ATTACK * 100:
        problems.append(f"attack 占比 {atk_pct:.1f}% < {MIN_ATTACK*100:.0f}%")
    return problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="tools/rl/dqn_real.pt")
    ap.add_argument("--real-mode", action="store_true",
                    help="评估时也开REAL_MODE (模型是真实模式训练时须加)")
    args = ap.parse_args()

    combat_env_py.REAL_MODE = args.real_mode
    net = DQN()
    net.load_state_dict(torch.load(args.model, map_location="cpu"))
    net.eval()

    print(f"体检 {args.model}  REAL_MODE={args.real_mode}")
    problems = check(net, "Boss 战分布", real_domain=True)
    problems += check(net, "训练分布", real_domain=False)

    print("\n" + "=" * 50)
    if problems:
        print("策略体检未通过:")
        for p in problems:
            print(f"  - {p}")
        return 1
    print("策略体检通过 — 动作分布无退化, 可接入游戏")
    return 0


if __name__ == "__main__":
    sys.exit(main())