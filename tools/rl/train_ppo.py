# -*- coding: utf-8 -*-
"""PPO 训练 — 复用 DQN 的环境与输入构造, 但完全不同的学习算法。

与 DQN 的关键差异:
  - 不做 bootstrap: 用 GAE 估计优势, 无 Q 值不动点 → 治好了 wait 89.7%
  - on-policy: 数据用完即弃, 样本效率低于 DQN 但策略稳定
  - clipped surrogate: 限制单次更新幅度, 防止策略突变

超参 (针对本环境调过, 理由见注释):
  - gamma=0.995 / gae_lambda=0.95 : episode 300 步, 折扣要大但不能丢长程
  - clip=0.2 : 标准 PPO 值
  - entropy_coef=0.01 : 本环境动作分布退化(wait 89.7%), 需更强的探索激励
  - lr=3e-4 : 比 DQN 的 1e-3 小, PPO 对步长更敏感
"""
import argparse
import os
import sys
import time

import numpy as np
import torch
import torch.nn as nn

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import combat_env_py
from action_mask import (INPUT_DIM, ACTION_COUNT, legal_mask, build_input,
                         masked_random, WAIT)
from combat_env_py import CombatEnvironment
from ppo_model import ActorCritic, RolloutBuffer
import train_dqn as td


GAMMA = 0.995
GAE_LAMBDA = 0.95
CLIP = 0.2
ENTROPY_COEF = 0.01
VALUE_COEF = 0.5
LR = 3e-4
HORIZON = 1024
UPDATE_EPOCHS = 4
MINI_BATCH = 64
MAX_GRAD_NORM = 0.5
EPS_START, EPS_END = 0.5, 0.05
REWARD_SCALE = 0.01


def compute_gae(rewards, values, dones, last_value, gamma=GAMMA,
                lam=GAE_LAMBDA):
    """GAE(λ) 优势估计 — 不用 bootstrap 目标, 只用 V 估计偏差"""
    n = len(rewards)
    adv = np.zeros(n, dtype=np.float32)
    last_gae = 0.0
    for t in reversed(range(n)):
        next_value = last_value if t == n - 1 else values[t + 1]
        non_terminal = 0.0 if dones[t] else 1.0
        delta = rewards[t] + gamma * next_value * non_terminal - values[t]
        last_gae = delta + gamma * lam * non_terminal * last_gae
        adv[t] = last_gae
    returns = adv + values
    return adv, returns


def collect_rollout(net, opt_env, buf, rng, epsilon, level, variant_seed):
    """填满一个 rollout buffer, 返回 (平均奖励, 平均步数, 胜率)"""
    td.combat_env_py = combat_env_py
    initial = td.make_scenario(variant_seed % 6, rng, level)
    initial.seed = int(rng.integers(0, 0xFFFFFFFF)) & 0xFFFFFFFF
    obs = opt_env.reset(initial)
    ep_r, ep_steps, ep_win = 0.0, 0, 0
    episodes = 0
    total_win = 0

    limit = td.max_steps(level)
    while not buf.full():
        mask = legal_mask(opt_env)
        x = build_input(obs, mask)
        xt = torch.FloatTensor(x).unsqueeze(0)
        mt = torch.FloatTensor(mask).unsqueeze(0)
        with torch.no_grad():
            if rng.random() < epsilon:
                a = masked_random(rng, mask)
                logits, v = net(xt)
                mlogits = torch.where(mt > 0, logits,
                                      torch.full_like(logits, -1e9))
                lp = torch.log_softmax(mlogits, dim=-1)[0, a]
            else:
                a_t, lp, v = net.act(xt, mt)
                a = int(a_t.item())
        sr = opt_env.step(a)
        done = sr.terminal
        buf.add(x, mask, a, float(lp.item()), sr.reward * REWARD_SCALE,
                float(v.item()), 1.0 if done else 0.0)
        ep_r += sr.reward
        ep_steps += 1
        obs = sr.observation

        if done or ep_steps >= limit:
            episodes += 1
            total_win += opt_env.state().victory
            ep_r, ep_steps = 0.0, 0
            obs = opt_env.reset(td.make_scenario(
                (variant_seed + episodes) % 6, rng, level))
    return ep_r, ep_steps, (total_win / max(1, episodes))


def ppo_update(net, opt, buf, rng):
    x, mask, actions, logprobs, rewards, values, dones = buf.get()
    adv, returns = compute_gae(rewards, values, dones, 0.0)
    adv = (adv - adv.mean()) / (adv.std() + 1e-8)

    xt = torch.FloatTensor(x)
    mt = torch.FloatTensor(mask)
    at = torch.LongTensor(actions)
    lpt = torch.FloatTensor(logprobs)
    at_v = torch.FloatTensor(adv)
    rt = torch.FloatTensor(returns)
    vt = torch.FloatTensor(values)

    n = len(x)
    losses = []
    for _ in range(UPDATE_EPOCHS):
        idx = torch.randint(0, n, (min(MINI_BATCH, n),))
        lp, entropy, v = net.evaluate(xt[idx], mt[idx], at[idx])
        ratio = torch.exp(lp - lpt[idx])
        s1 = ratio * at_v[idx]
        s2 = torch.clamp(ratio, 1 - CLIP, 1 + CLIP) * at_v[idx]
        actor_loss = -torch.min(s1, s2).mean()
        value_loss = ((v - rt[idx]) ** 2).mean()
        loss = actor_loss + VALUE_COEF * value_loss - ENTROPY_COEF * entropy.mean()
        opt.zero_grad()
        loss.backward()
        nn.utils.clip_grad_norm_(net.parameters(), MAX_GRAD_NORM)
        opt.step()
        losses.append(float(loss.item()))
    return float(np.mean(losses))


def train(iters: int, out_path: str, seed: int = 42, real_mode=True,
          curriculum=True, verbose=True):
    combat_env_py.REAL_MODE = real_mode
    torch.manual_seed(seed)
    rng = np.random.default_rng(seed)
    net = ActorCritic()
    opt = torch.optim.Adam(net.parameters(), lr=LR)
    env = CombatEnvironment()
    buf = RolloutBuffer(HORIZON, INPUT_DIM)

    stages = ([(0, iters // 3), (1, iters // 3),
               (2, iters - 2 * iters // 3)] if curriculum else [(2, iters)])
    t0 = time.time()
    total_wins = 0
    total_eps = 0

    for level, n_iters in stages:
        if n_iters <= 0:
            continue
        stage_win = 0
        for it in range(n_iters):
            frac = it / max(1, n_iters - 1)
            epsilon = EPS_START + (EPS_END - EPS_START) * frac
            _, _, wr = collect_rollout(net, env, buf, rng, epsilon, level,
                                       it + level * 1000)
            loss = ppo_update(net, opt, buf, rng)
            buf.reset()
            stage_win += wr
            total_wins += wr
            total_eps += 1
            if verbose and (it + 1) % max(1, n_iters // 4) == 0:
                print(f"  L{level} [{it+1}/{n_iters}] eps={epsilon:.3f} "
                      f"win={wr*100:.0f}% loss={loss:.4f} "
                      f"[{time.time()-t0:.0f}s]")
        if verbose and curriculum:
            print(f"  == L{level} 完成, 平均胜率 "
                  f"{stage_win*100/max(1,n_iters):.0f}% ==")

    torch.save(net.state_dict(), out_path)
    print(f"\n[PPO{' REAL' if real_mode else ''}] 训练完成 "
          f"({total_eps} rollouts)  平均胜率 "
          f"{total_wins*100/max(1,total_eps):.1f}%")
    print(f"保存: {out_path} [{time.time()-t0:.0f}s]")
    return out_path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iters", type=int, default=300)
    ap.add_argument("--out", default="tools/rl/ppo.pt")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--toy", action="store_true")
    ap.add_argument("--no-curriculum", action="store_true")
    args = ap.parse_args()
    train(args.iters, args.out, args.seed, not args.toy,
          not args.no_curriculum)
    return 0


if __name__ == "__main__":
    sys.exit(main())