# -*- coding: utf-8 -*-
"""动作 mask 工具 — 与 mcts::get_possible_actions 共用同一套判定。

单独成模块的理由: DQN 训练时 mask 既是网络输入 (第 9-18 维) 也是 argmax 约束,
两处必须来自同一次计算, 否则"喂进去的合法性"和"实际约束的合法性"会不一致。
"""
import numpy as np

from combat_env_py import (ACTION_COUNT, ATTACK, SKILL_1, SKILL_4,
                           MOVE_UP, MOVE_DOWN, MOVE_LEFT, MOVE_RIGHT, WAIT)


def legal_mask(env) -> np.ndarray:
    """返回长度10 的 float32 0/1 向量, 索引与 mcts::CombatAction 一致"""
    mask = np.zeros(ACTION_COUNT, dtype=np.float32)
    for a in env.possible_actions():
        mask[a] = 1.0
    return mask


def masked_argmax(q: np.ndarray, mask: np.ndarray) -> int:
    """在合法动作里取最大 Q; 非法动作置-inf。全非法时退回 WAIT。

    -inf 而非 0: Q 值可以是负的 (挨打掉血), 用 0 会让非法动作看起来"还不错"。
    """
    if mask.sum() == 0:
        return WAIT
    return int(np.argmax(np.where(mask > 0, q, -np.inf)))


def masked_random(rng: np.random.Generator, mask: np.ndarray) -> int:
    """在合法动作里均匀采样 (epsilon 探索用)"""
    legal = np.flatnonzero(mask > 0)
    if legal.size == 0:
        return WAIT
    return int(rng.choice(legal))


def build_input(obs8: np.ndarray, mask10: np.ndarray) -> np.ndarray:
    """网络输入 = 18 维 = [8 维观测] + [10 维合法性 mask]

    归一化在此集中处理 — C++ 推理侧必须做完全相同的缩放, 否则 serve skew。
    """
    hp = np.float32(obs8[0])
    atk = np.float32(obs8[1]) / 50.0
    cnt = np.float32(obs8[2]) / 10.0
    dist = np.float32(obs8[3]) / 20.0
    strongest = np.float32(obs8[4])
    boss = np.float32(obs8[5])
    buffs = np.float32(obs8[6]) / 10.0
    style = np.float32(obs8[7]) / 4.0
    return np.concatenate([
        np.array([hp, atk, cnt, dist, strongest, boss, buffs, style],
                 dtype=np.float32),
        mask10.astype(np.float32),
    ])


INPUT_DIM = 18
OUTPUT_DIM = ACTION_COUNT

__all__ = ["legal_mask", "masked_argmax", "masked_random", "build_input",
           "INPUT_DIM", "OUTPUT_DIM", "ACTION_COUNT",
           "ATTACK", "SKILL_1", "SKILL_4", "MOVE_UP", "MOVE_DOWN",
           "MOVE_LEFT", "MOVE_RIGHT", "WAIT"]
