# -*- coding: utf-8 -*-
"""域适配度测量 — DQN 在真实 Boss 战状态上的行为是否合理。

背景: DQN 训练用的是 CombatEnvironment (玩具战斗模型):
  - 40x30 网格 + 曼哈顿距离
  - 普攻伤害 = attack - pdef*0.5, 乘 0.8~1.2 随机
  - 冷却按"步"计, 每步 -0.25
  - 固定 2 只怪 (slime 15~26 HP / orc 20~36 HP)
真实 Boss 战 (MirrorBattleState) 完全不同: 真实伤害公式/ 真实冷却时长 /
多阶段 Boss。DQN 的 Q 值是在玩具模型里标定的, 喂真实状态进去得到的
Q 值在真实战斗里**没有意义**。

本脚本不改任何游戏代码, 只回答一个问题: 这个 DQN 放到真实 Boss 战里
会做出什么决策? 如果答案是"44% 时间在发呆", 那就不能直接接入。
"""
import os
import sys

import numpy as np
import torch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from action_mask import legal_mask, masked_argmax, build_input, ATTACK, WAIT
from combat_env_py import CombatEnvironment, SimulationState, MonsterSnapshot
from dqn_model import DQN

NAMES = {0: "attack", 1: "skill_1", 2: "skill_2", 3: "skill_4"[0:0] or "skill_2",
         3: "skill_3", 4: "skill_4", 5: "move_up", 6: "move_down",
         7: "move_left", 8: "move_right", 9: "wait"}


def load():
    here = os.path.dirname(os.path.abspath(__file__))
    net = DQN()
    net.load_state_dict(torch.load(os.path.join(here, "dqn.pt"),
                                   map_location="cpu"))
    net.eval()
    return net


def sweep_training_domain(net, n=400):
    """训练分布内 — 作为对照组"""
    from train_dqn import make_scenario
    env = CombatEnvironment()
    rng = np.random.default_rng(0)
    counts, qs = {}, []
    for i in range(n):
        s = make_scenario(i % 5, np.random.default_rng(7000 + i))
        s.seed = (42 + i * 7919) & 0xFFFFFFFF
        env.reset(s)
        k = 0
        while not env.is_done() and k < 60:
            m = legal_mask(env)
            x = torch.FloatTensor(build_input(env.observation(), m)).unsqueeze(0)
            with torch.no_grad():
                q = net(x).squeeze(0).numpy()
            qs.append(q)
            a = masked_argmax(q, m)
            counts[a] = counts.get(a, 0) + 1
            env.step(a)
            k += 1
    return counts, np.array(qs)


def make_real_like_state(px, py, boss_hp_pct, player_hp_pct, dist_tiles,
                         style, boss_atk_ready, player_skills_ready):
    """构造一个语义上等价于真实 Boss 战的 MirrorBattleState 对应观测

    复用 mirror_state_to_obs 的映射 (mirror_agent.cpp:13):
      player_hp_ratio ← player_hp_pct
      player_attack   ← 10.0 (硬编码常量)
      enemy_count     ← 1.0  (单 Boss)
      nearest_dist    ← dist_tiles
      strongest       ← boss_hp_pct
      boss_present    ← 1.0
      buff_count      ← 0
      player_style    ← style
    """
    s = SimulationState()
    p = s.player
    p.hp = player_hp_pct * 100.0
    p.max_hp = 100.0
    p.x, p.y = px, py
    p.attack = 10
    p.alive = True
    # 单 Boss, 放在 dist_tiles 处
    s.monsters = [MonsterSnapshot("boss", 800.0 * max(boss_hp_pct, 0.01),
                                  800.0, px + dist_tiles, py, 25, 10, 8,
                                  [], boss_hp_pct > 0, True)]
    s.player_style = style
    return s


def sweep_boss_domain(net, n=400):
    """真实 Boss 战状态分布"""
    env = CombatEnvironment()
    rng = np.random.default_rng(1)
    counts, qs = {}, []
    for i in range(n):
        px = float(rng.integers(0, 40))
        py = float(rng.integers(0, 30))
        dist = float(rng.uniform(1.0, 14.0))       # 真实 Boss 战距离跨度更大
        boss_hp = float(rng.uniform(0.05, 1.0))
        player_hp = float(rng.uniform(0.15, 1.0))
        style = int(rng.integers(0, 5))
        st = make_real_like_state(px, py, boss_hp, player_hp, dist, style,
                                  bool(rng.integers(0, 2)), 2)
        env.reset(st)
        m = legal_mask(env)
        x = torch.FloatTensor(build_input(env.observation(), m)).unsqueeze(0)
        with torch.no_grad():
            q = net(x).squeeze(0).numpy()
        qs.append(q)
        a = masked_argmax(q, m)
        counts[a] = counts.get(a, 0) + 1
    return counts, np.array(qs)


def report(title, counts, qs):
    total = sum(counts.values())
    print(f"\n--- {title} ---")
    print(f"  样本 {total}, Q 值范围 [{qs.min():.3f}, {qs.max():.3f}], "
          f"均值 {qs.mean():.3f}")
    ranked = sorted(counts.items(), key=lambda kv: -kv[1])
    for a, c in ranked:
        print(f"    {NAMES.get(a, a):<12} {c:4d}  {c*100.0/total:5.1f}%")


def main():
    net = load()
    c1, q1 = sweep_training_domain(net)
    report("训练分布内 (CombatEnvironment)", c1, q1)
    c2, q2 = sweep_boss_domain(net)
    report("真实 Boss 战分布 (MirrorBattleState 语义)", c2, q2)

    t1 = sum(c1.values())
    t2 = sum(c2.values())
    print("\n--- 关键结论 ---")
    print(f"  训练域 wait 占比   : {c1.get(9,0)*100.0/t1:5.1f}%")
    print(f"  Boss 域 wait 占比  : {c2.get(9,0)*100.0/t2:5.1f}%")
    print(f"  Boss 域 attack 占比: {c2.get(0,0)*100.0/t2:5.1f}%")
    lo = min(c2.get(0,0)*100.0/t2, c2.get(9,0)*100.0/t2)
    print(f"\n  若 Boss 域里某个动作占比超过 50%, 说明 DQN 已退化为"
          f"单一动作常量输出, 接入游戏无意义。")


if __name__ == "__main__":
    main()