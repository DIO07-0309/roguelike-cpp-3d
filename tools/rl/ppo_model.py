# -*- coding: utf-8 -*-
"""PPO 网络定义 — Actor-Critic, 共享 trunk。

为什么换 PPO: DQN 在本环境出现 Q 值自举不动点 —— 网络学会"永远 WAIT"
(实测 wait 89.7%), 因为 Q(s,WAIT) = r + γ·max Q(s',·) 而 WAIT 不改变状态,
可以构造自我循环的 Q 不动点。这是 value-based 方法的结构性弱点,
调参无法消除。PPO 直接优化策略 π(a|s), **不做 bootstrap**, 不存在该不动点。

网络结构 (输入沿用 action_mask.INPUT_DIM = 24 = 14 观测 + 10 mask):
  trunk:    24 → 64 → 64 (共享)
  actor:    64 → 10          (动作 logits)
  critic:   64 → 1           (状态价值 V(s))

★ 动作合法性: 与 DQN 一样靠 mask (第 15~24 维)。非法动作的 logit 置
  -1e9 后再 softmax, 保证策略不会给非法动作分配概率。
"""
import numpy as np
import torch
import torch.nn as nn

from action_mask import INPUT_DIM, OUTPUT_DIM, ACTION_COUNT


class ActorCritic(nn.Module):
    def __init__(self, input_dim: int = INPUT_DIM, output_dim: int = OUTPUT_DIM,
                 hidden: int = 64):
        super().__init__()
        self.trunk = nn.Sequential(
            nn.Linear(input_dim, hidden), nn.Tanh(),
            nn.Linear(hidden, hidden), nn.Tanh(),
        )
        self.actor = nn.Linear(hidden, output_dim)
        self.critic = nn.Linear(hidden, 1)

    def forward(self, x):
        h = self.trunk(x)
        return self.actor(h), self.critic(h).squeeze(-1)

    def evaluate(self, x, mask, actions):
        """给定动作, 返回 (log_prob, entropy, value)

        mask 非法动作的 logit 置 -1e9 —— 与 DqnPolicy.masked_argmax 的
        "只在合法里选"语义一致, 但这里是在概率层面保证。
        """
        logits, value = self.forward(x)
        masked_logits = torch.where(mask > 0, logits,
                                    torch.full_like(logits, -1e9))
        logp_all = torch.log_softmax(masked_logits, dim=-1)
        logp = logp_all.gather(1, actions.unsqueeze(1)).squeeze(1)
        probs = torch.softmax(masked_logits, dim=-1)
        entropy = -(probs * logp_all.clamp(min=-30.0)).sum(-1)
        return logp, entropy, value

    def act(self, x, mask):
        """采样动作 (训练用) + 返回 log_prob / value"""
        logits, value = self.forward(x)
        masked_logits = torch.where(mask > 0, logits,
                                    torch.full_like(logits, -1e9))
        dist = torch.distributions.Categorical(logits=masked_logits)
        a = dist.sample()
        return a, dist.log_prob(a), value

    def act_greedy(self, x, mask):
        """贪心动作 (评测用) — argmax over legal"""
        logits, _ = self.forward(x)
        masked_logits = torch.where(mask > 0, logits,
                                    torch.full_like(logits, -1e9))
        return masked_logits.argmax(dim=-1)


class RolloutBuffer:
    """固定长度的 rollout 缓冲 — PPO 用 on-policy 数据, 采满即丢弃"""

    def __init__(self, horizon: int, input_dim: int = INPUT_DIM):
        self.horizon = horizon
        self.x = np.zeros((horizon, input_dim), dtype=np.float32)
        self.mask = np.zeros((horizon, ACTION_COUNT), dtype=np.float32)
        self.actions = np.zeros(horizon, dtype=np.int64)
        self.logprobs = np.zeros(horizon, dtype=np.float32)
        self.rewards = np.zeros(horizon, dtype=np.float32)
        self.values = np.zeros(horizon, dtype=np.float32)
        self.dones = np.zeros(horizon, dtype=np.float32)
        self.ptr = 0

    def add(self, x, mask, action, logprob, reward, value, done):
        i = self.ptr
        self.x[i] = x
        self.mask[i] = mask
        self.actions[i] = action
        self.logprobs[i] = logprob
        self.rewards[i] = reward
        self.values[i] = value
        self.dones[i] = done
        self.ptr = i + 1

    def full(self) -> bool:
        return self.ptr >= self.horizon

    def reset(self):
        self.ptr = 0

    def get(self):
        return (self.x[:self.ptr], self.mask[:self.ptr],
                self.actions[:self.ptr], self.logprobs[:self.ptr],
                self.rewards[:self.ptr], self.values[:self.ptr],
                self.dones[:self.ptr])