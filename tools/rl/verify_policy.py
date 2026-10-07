# -*- coding: utf-8 -*-
"""C++ DqnPolicy 与 Python action_mask 的一致性校验。

校验两件容易静默出错的事:
  1. build_input 的 18 维构造与缩放系数 —— 权重算对了但输入喂错, 输出全错
  2. masked_argmax 的选动作规则 —— Q 值对但动作选错, 同样静默

做法: 造 N 条随机 (obs, legal_mask), C++ 侧输出 build_input 结果与选中动作,
Python 侧算参考值逐条比对。argmax 在 Q 值相等时可能因并列取首个 ——
构造时避开并列即可 (随机浮点几乎不会精确相等)。
"""
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
PROBE = os.path.join("build", "tests", "dqn", "dqn_policy_probe.exe")
WEIGHTS = os.path.join(HERE, "dqn_weights.bin")
TMP_IN = os.path.join(HERE, "_policy_input.txt")
TMP_OUT = os.path.join(HERE, "_policy_output.txt")


def main() -> int:
    if not os.path.exists(PROBE):
        print(f"缺少 {PROBE} — 先构建 dqn_policy_probe 目标")
        return 2
    if not os.path.exists(WEIGHTS):
        print(f"缺少 {WEIGHTS} — 先跑 export_weights.py")
        return 2

    sys.path.insert(0, HERE)
    from action_mask import build_input as py_build_input
    import torch
    from dqn_model import DQN

    rng = np.random.default_rng(20261004)
    n = 128
    obs = np.zeros((n, 8), dtype=np.float32)
    obs[:, 0] = rng.uniform(0.0, 1.0, n)
    obs[:, 1] = rng.integers(0, 40, n).astype(np.float32)   # 原始 attack 量级
    obs[:, 2] = rng.integers(0, 6, n).astype(np.float32)
    obs[:, 3] = rng.integers(0, 30, n).astype(np.float32)  # 原始距离量级
    obs[:, 4] = rng.uniform(0.0, 1.0, n)
    obs[:, 5] = rng.integers(0, 2, n).astype(np.float32)
    obs[:, 6] = rng.integers(0, 8, n).astype(np.float32)
    obs[:, 7] = rng.integers(0, 5, n).astype(np.float32)
    masks = (rng.random((n, 10)) < 0.5).astype(np.float32)
    masks[:, 9] = 1.0                                       # WAIT 恒合法

    with open(TMP_IN, "w") as f:
        for i in range(n):
            f.write(",".join(f"{v:.9g}" for v in obs[i]) + "|"
                    + ",".join(f"{v:.0f}" for v in masks[i]) + "\n")
    subprocess.run([PROBE, WEIGHTS, TMP_IN, TMP_OUT], check=True,
                   capture_output=True)

    cpp_rows = []
    cpp_actions = []
    with open(TMP_OUT, encoding="utf-8") as f:
        next(f)                      # 跳过表头
        for line in f:
            line = line.strip()
            if not line:
                continue
            vec_s, act_s = line.split(";")
            cpp_rows.append([float(x) for x in vec_s.split(",")])
            cpp_actions.append(int(act_s))
    cpp_in = np.array(cpp_rows, dtype=np.float64)
    cpp_actions = np.array(cpp_actions, dtype=np.int64)

    net = DQN()
    net.load_state_dict(torch.load(os.path.join(HERE, "dqn.pt"),
                                   map_location="cpu"))
    net.eval()

    max_in_diff = 0.0
    action_mismatch = []
    for i in range(n):
        ref_in = py_build_input(obs[i], masks[i]).astype(np.float64)
        max_in_diff = max(max_in_diff, np.abs(cpp_in[i] - ref_in).max())
        x = torch.FloatTensor(py_build_input(obs[i], masks[i])).unsqueeze(0)
        with torch.no_grad():
            q = net(x).squeeze(0).numpy()
        legal = np.flatnonzero(masks[i] > 0)
        ref_act = int(legal[np.argmax(q[legal])])
        if ref_act != cpp_actions[i]:
            action_mismatch.append((i, ref_act, int(cpp_actions[i])))

    print(f"比对 {n} 条 (obs, mask)")
    print(f"  build_input 最大绝对差 {max_in_diff:.3e}")
    print(f"  选中动作不一致 {len(action_mismatch)} 条")
    if max_in_diff > 1e-5:
        print("FAIL — build_input 不一致 (缩放系数或顺序错)")
        return 1
    if action_mismatch:
        print("FAIL — 选中动作不一致")
        for i, r, c in action_mismatch[:5]:
            print(f"  [{i}] python={r} cpp={c}")
        return 1
    print("OK — C++ DqnPolicy 与 Python 一致 (输入构造 + masked argmax)")
    os.remove(TMP_IN)
    os.remove(TMP_OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
