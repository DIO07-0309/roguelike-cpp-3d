"""Blender MCP 客户端 (直连 127.0.0.1:9876)

用途: 脚本化驱动已开启 "MCP for Blender" 插件的 Blender 实例 —
  - 执行一段 Python (bpy) 代码
  - 抓 3D 视口截图 (插件走 GPU 离屏渲染, 窗口不聚焦也能抓)
用法:
  python tools/blender_client.py --code "import bpy; print(bpy.app.version_string)"
  python tools/blender_client.py --file art_src/scripts/xxx.py
  python tools/blender_client.py --shot --out art_src/preview/blender_view.png
"""

import argparse
import json
import os
import socket
import sys

HOST = "localhost"
PORT = 9876


def send(command, host=HOST, port=PORT, timeout=180):
    """发一条 JSON 命令并等回包 (插件按 JSON 逐条解析)"""
    with socket.create_connection((host, port), timeout=timeout) as sock:
        sock.sendall(json.dumps(command).encode("utf-8"))
        buffer = b""
        while True:
            chunk = sock.recv(65536)
            if not chunk:
                break
            buffer += chunk
            try:
                return json.loads(buffer.decode("utf-8"))
            except json.JSONDecodeError:
                continue  # 半包, 继续收
    raise RuntimeError("blender closed the connection without a response")


def run_code(code):
    return send({"type": "execute_code", "params": {"code": code}})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--code", help="在 Blender 内执行的 Python 代码")
    parser.add_argument("--file", help="执行一个 .py 文件")
    parser.add_argument("--shot", action="store_true", help="抓 3D 视口截图")
    parser.add_argument("--out", default="blender_view.png", help="截图输出路径")
    parser.add_argument("--max-size", type=int, default=1400)
    parser.add_argument("--scene-info", action="store_true")
    args = parser.parse_args()

    if args.code or args.file:
        code = args.code
        if args.file:
            with open(args.file, encoding="utf-8") as handle:
                code = handle.read()
            # 插件的 exec 不提供 __file__/__name__ — 手动注入, 让脚本按普通语义运行
            code = "__file__ = r'%s'\n__name__ = '__main__'\n%s" % (
                os.path.abspath(args.file),
                code,
            )
        result = run_code(code)
        text = json.dumps(result, ensure_ascii=False)
        print(text if len(text) < 12000 else text[:12000] + " ...<truncated>")

    if args.scene_info:
        print(json.dumps(send({"type": "get_scene_info"}), ensure_ascii=False)[:4000])

    if args.shot:
        result = send(
            {
                "type": "get_viewport_screenshot",
                "params": {"max_size": args.max_size, "filepath": args.out},
            }
        )
        print(json.dumps(result, ensure_ascii=False)[:800])


if __name__ == "__main__":
    try:
        main()
    except Exception as error:  # noqa: BLE001 — CLI 需要可读错误
        print("blender_client error: %s" % error, file=sys.stderr)
        sys.exit(1)
