# -*- coding: utf-8 -*-
"""权重导出 — 把训练好的 DQN 转成 C++ 可直接读取的二进制。

为什么不走 ONNX Runtime: 这个网络只有 3 层全连接 (18→64→64→10, 2946 个权重),
手写前向约 80 行即可, 零第三方库、零 dll 依赖。引入 ONNX Runtime 要往 vendor/
加约 15MB 二进制, 违反项目约束 (vendor/ 仅允许 raylib + nlohmann/json)。

格式 (小端, 无padding):
    magic   char[4]  "RLDQ"
    version uint32   1
    n_layers uint32  3
    然后每层: in_dim u32, out_dim u32, bias[out_dim] f32, weight[out_dim*in_dim] f32
    权重布局与 torch.nn.Linear 一致: weight[o*in_dim + i], 即 out_dim 行 × in_dim 列

导出后必须跑verify_weights.py 反向校验: C++ 侧算出的 Q 值要能与 PyTorch 对上。
"""
import argparse
import os
import struct
import sys

import torch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dqn_model import DQN

MAGIC = b"RLDQ"
VERSION = 1


def export(model_path: str, out_path: str) -> int:
    net = DQN()
    net.load_state_dict(torch.load(model_path, map_location="cpu"))
    net.eval()

    layers = [(m.in_features, m.out_features, m.bias.data, m.weight.data)
              for m in net.net if isinstance(m, torch.nn.Linear)]

    buf = bytearray()
    buf += MAGIC
    buf += struct.pack("<III", VERSION, len(layers), 0)  # 末位保留
    for in_dim, out_dim, bias, weight in layers:
        buf += struct.pack("<II", in_dim, out_dim)
        for v in bias.tolist():
            buf += struct.pack("<f", v)
        # weight: torch 是 [out_dim, in_dim], 展平成行优先即 o*in_dim + i
        for v in weight.reshape(-1).tolist():
            buf += struct.pack("<f", v)

    with open(out_path, "wb") as f:
        f.write(buf)

    n_params = sum(b.numel() for _, _, b, w in layers)
    print(f"导出 {out_path}: {len(buf)} 字节, {len(layers)} 层, {n_params} 参数")
    for in_dim, out_dim, _, _ in layers:
        print(f"  Linear({in_dim} -> {out_dim})")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="tools/rl/dqn.pt")
    ap.add_argument("--out", default="tools/rl/dqn_weights.bin")
    args = ap.parse_args()
    return export(args.model, args.out)


if __name__ == "__main__":
    sys.exit(main())
