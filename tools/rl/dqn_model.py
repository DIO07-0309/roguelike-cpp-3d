# -*- coding: utf-8 -*-
"""DQN 网络定义 — 18 维输入 / 10 维输出。

结构照抄 docs/AI_LEARNING_GUIDE.md:602 的 DQN, 但 state_dim 从 8 改成 18
(8 观测 + 10 mask)。为什么加 mask 见设计说明:
  Observation 对动作合法性是 POMDP —— 8 个字段里没有任何一个能推出
  attack_cooldown / skill_cooldowns / player 坐标, 网络物理上不可能知道
  现在能不能攻击。mask 作为输入让网络学到"能开技能且血量低 → 该风筝"这类
  交互, 而不是把容量浪费在永远非法的动作上。

导出注意: onnx 导出时 batch 维固定为 1, C++ 推理侧每次喂单帧。
"""
import torch
import torch.nn as nn

from action_mask import INPUT_DIM, OUTPUT_DIM


class DQN(nn.Module):
    def __init__(self, input_dim: int = INPUT_DIM, output_dim: int = OUTPUT_DIM):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(input_dim, 64), nn.ReLU(),
            nn.Linear(64, 64), nn.ReLU(),
            nn.Linear(64, output_dim),
        )

    def forward(self, x):
        return self.net(x)


class ReplayBuffer:
    def __init__(self, capacity: int = 50000):
        self.buffer: list = []
        self.capacity = capacity

    def push(self, *transition):
        if len(self.buffer) >= self.capacity:
            self.buffer.pop(0)
        self.buffer.append(transition)

    def sample(self, batch: int = 64):
        idx = torch.randint(0, len(self.buffer), (batch,))
        return [self.buffer[int(i)] for i in idx]

    def __len__(self):
        return len(self.buffer)
