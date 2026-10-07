# -*- coding: utf-8 -*-
"""反向校验— C++ 手写前向 算出的 Q 值 必须与 PyTorch 一致。

流程:
  1. 随机造若干条 18 维输入 (含真实 mask 结构)
  2. 写进临时文件, 让 tools/rl/dqn_probe.exe (C++) 读入并前向
  3. Python 侧算 PyTorch 参考值, 逐条比对

容差说明: 累加顺序不同会产生float32 舍入级差异, 用 1e-4 相对容差。
若C++ 侧改了权重布局 (行/列颠倒) 或漏了 ReLU, 差异会是数量级级别, 必被抓。
"""
import os
import subprocess
import sys

import numpy as np
import torch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dqn_model import DQN

HERE = os.path.dirname(os.path.abspath(__file__))
PROBE = os.path.join("build", "tests", "dqn", "dqn_probe.exe")
WEIGHTS = os.path.join(HERE, "dqn_weights.bin")
TMP_INPUT = os.path.join(HERE, "_probe_input.txt")
TMP_OUTPUT = os.path.join(HERE, "_probe_output.txt")
TOL = 1e-4


def make_inputs(n: int = 64) -> np.ndarray:
    """造贴近真实的输入: 前 8 维是观测特征, 后 10 维是 mask (至少含 WAIT)"""
    rng = np.random.default_rng(4242)
    out = np.zeros((n, 18), dtype=np.float32)
    out[:, 0] = rng.uniform(0.0, 1.0, n)          # hp_ratio
    out[:, 1] = rng.uniform(0.14, 0.30, n)       # attack/50
    out[:, 2] = rng.uniform(0.0, 0.2, n)         # enemy_count/10
    out[:, 3] = rng.uniform(0.0, 0.5, n)         # dist/20
    out[:, 4] = rng.uniform(0.0, 1.0, n)         # strongest
    out[:, 5] = rng.integers(0, 2, n)            # boss
    out[:, 6] = rng.uniform(0.0, 0.2, n)         # buffs/10
    out[:, 7] = rng.uniform(0.0, 1.0, n)         # style/4
    out[:, 8:18] = (rng.random((n, 10)) < 0.5).astype(np.float32)
    out[:, 17] = 1.0                             # WAIT恒合法
    return out


def main() -> int:
    if not os.path.exists(PROBE):
        print(f"缺少 {PROBE} — 先构建 dqn_probe 目标")
        return 2
    if not os.path.exists(WEIGHTS):
        print(f"缺少 {WEIGHTS} — 先跑 export_weights.py")
        return 2

    x = make_inputs()
    np.savetxt(TMP_INPUT, x, fmt="%.9g", delimiter=",")
    subprocess.run([PROBE, WEIGHTS, TMP_INPUT, TMP_OUTPUT], check=True,
                   capture_output=True)
    cpp = np.loadtxt(TMP_OUTPUT, delimiter=",", dtype=np.float64)

    net = DQN()
    net.load_state_dict(torch.load(os.path.join(HERE, "dqn.pt"),
                                   map_location="cpu"))
    net.eval()
    with torch.no_grad():
        ref = net(torch.FloatTensor(x)).numpy().astype(np.float64)

    if cpp.shape != ref.shape:
        print(f"形状不匹配: C++ {cpp.shape} vs PyTorch {ref.shape}")
        return 1
    diff = np.abs(cpp - ref)
    rel = diff / np.maximum(np.abs(ref), 1e-6)
    worst = int(np.argmax(diff.max(axis=1)))
    print(f"比对 {len(x)} 条输入")
    print(f"  最大绝对差 {diff.max():.3e}  (第 {worst} 条)")
    print(f"  最大相对差 {rel.max():.3e}")
    if diff.max() > TOL:
        print("FAIL — C++ 前向与 PyTorch 不一致")
        for i in range(min(5, len(x))):
            print(f"  [{i}] cpp={np.round(cpp[i],5)}")
            print(f"      ref={np.round(ref[i],5)}")
        return 1
    print("OK — C++ 手写前向与 PyTorch 一致 (权重布局 + ReLU 均正确)")
    os.remove(TMP_INPUT)
    os.remove(TMP_OUTPUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
