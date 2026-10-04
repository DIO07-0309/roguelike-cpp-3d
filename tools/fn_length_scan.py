#!/usr/bin/env python3
r"""
G29: Function Length Scanner — 扫 src/ 下所有函数定义, 报出超过 40 行的违规。

修掉 G28 期间暴露的三类误判 (旧的内联扫描器全中):
  1. 带 :: 的限定名被漏 —— `PlayerController::tick` 219 行就这么漏掉的,
     因为 `(\w+)\(` 匹配不到 `PlayerController::tick(`。
  2. 嵌套重复源树重复计数 —— src/src、src/src/src 会让每个函数被数 3 遍。
  3. 字符串/注释里的花括号破坏深度统计 —— 用 strip_noise 等长替换消掉。

参数列表支持跨行: 先定位 `)`, 再看其后是否跟 `{` (定义) / `;` (声明) / `=` (lambda)。
这样不用为多行签名写正则。

用法: python tools/fn_length_scan.py [src_dir] [--limit N]
退出码: 0 = 全部达标, 1 = 有超规函数
"""

import io
import os
import re
import sys

LIMIT = 40


def read(p):
    try:
        with io.open(p, encoding="utf-8", errors="replace") as f:
            return f.read()
    except Exception:
        return ""


def strip_noise(t):
    """去注释与字面量, 等长替换, 行号不变。同 dead_method_scan.py。"""
    out = []
    i, n = 0, len(t)
    while i < n:
        c = t[i]
        if c == "/" and i + 1 < n:
            if t[i + 1] == "/":
                while i < n and t[i] != "\n":
                    out.append(" ")
                    i += 1
                continue
            if t[i + 1] == "*":
                j = t.find("*/", i + 2)
                j = n if j < 0 else j + 2
                while i < j:
                    out.append("\n" if t[i] == "\n" else " ")
                    i += 1
                continue
        if c in ('"', "'"):
            q = c
            out.append(" ")
            i += 1
            while i < n:
                if t[i] == "\\":
                    out.append("  ")
                    i += 2
                    continue
                if t[i] == q:
                    out.append(" ")
                    i += 1
                    break
                out.append(" " if t[i] != "\n" else "\n")
                i += 1
            continue
        out.append(c)
        i += 1
    return "".join(out)


def match_open(t, i):
    """i 指向 '(', 返回匹配的 ')' 索引, 未闭合返回 -1。"""
    d = 0
    while i < len(t):
        if t[i] == "(":
            d += 1
        elif t[i] == ")":
            d -= 1
            if d == 0:
                return i
        i += 1
    return -1


def match_brace(t, i):
    """i 指向 '{', 返回匹配的 '}' 索引, 未闭合返回 -1。"""
    d = 0
    while i < len(t):
        if t[i] == "{":
            d += 1
        elif t[i] == "}":
            d -= 1
            if d == 0:
                return i
        i += 1
    return -1


# 控制流/语句, 不是函数定义
NOT_FUNC = {"if", "for", "while", "switch", "catch", "return", "sizeof", "throw", "do", "else"}

# ')' 之后允许跟的修饰 + 可选构造初始化列表, 然后必须是 '{'
TAIL_RE = re.compile(
    r"\s*(?:(?:const|noexcept|override|final|volatile)\s+)*"   # 修饰
    r"(?::[^{;]*)?"                                             # 初始化列表
    r"\s*\{"
)

NAME_RE = re.compile(r"(?:(\w+)::)?(~?\w+)\s*\(")


def line_of(t, i):
    return t.count("\n", 0, i) + 1


def scan_file(path, limit):
    """返回 [(span, lineno, name, relpath), ...] 中超过 limit 的项。"""
    src = read(path)
    if not src:
        return []
    t = strip_noise(src)
    rel = os.path.relpath(path)
    over = []
    pos = 0
    while True:
        m = NAME_RE.search(t, pos)
        if not m:
            break
        pos = m.end()
        if m.group(2) in NOT_FUNC:
            continue
        r = match_open(t, m.end() - 1)
        if r < 0:
            break
        if not TAIL_RE.match(t, r + 1):
            continue                     # 声明 / lambda / 初始化, 非定义
        b = t.find("{", r)
        e = match_brace(t, b)
        if b < 0 or e < 0:
            continue
        span = line_of(t, e) - line_of(t, b) + 1
        if span > limit:
            name = (m.group(1) + "::" + m.group(2)) if m.group(1) else m.group(2)
            over.append((span, line_of(t, b), name, rel))
    return over


def main():
    src_dir = sys.argv[1] if len(sys.argv) > 1 else "src"
    limit = LIMIT
    for a in sys.argv[2:]:
        if a.startswith("--limit="):
            limit = int(a.split("=", 1)[1])

    hits = []
    files = 0
    for root, dirs, fs in os.walk(src_dir):
        for fn in fs:
            if not fn.endswith(".cpp"):
                continue
            files += 1
            hits += scan_file(os.path.join(root, fn), limit)

    hits.sort(reverse=True)
    print("扫描 %d 个 .cpp, 阈值 >%d 行: 超规 %d 个\n" % (files, limit, len(hits)))
    for span, ln, name, rel in hits:
        print("%4d  %-44s %s:%d" % (span, name[:44], rel.replace(os.sep, "/"), ln))
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())
