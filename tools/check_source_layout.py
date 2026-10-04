#!/usr/bin/env python3
"""
G29: Source Layout Guard — 拦住 G28 之前潜伏的「目录自复制」脏数据。

背景: 仓库曾出现 src/src、src/src/src、assets/assets 等嵌套自复制树
(约 70 MB, 全部 git 未跟踪)。因为 CMakeLists.txt 用
`file(GLOB_RECURSE SOURCES src/*.cpp)`, 这些副本会被递归全收进来 ——
main.cpp 一度存在三份, 任何一次干净 reconfigure 都会 LNK2005 多定义
直接编译失败。而增量构建完全无感, 只有清干净重跑才暴露。

.gitignore 挡不住这类问题 (那些文件本来就没被跟踪), 所以只有校验脚本能防。

检查项:
  1. 嵌套自复制目录 —— 相对路径中同一段名重复出现 (src/src, assets/assets)
  2. 源目录下的构建产物 —— src/、vendor/ 下出现 *.exe / *.dll
  3. 仓库根文档误入 vendor/ —— CMakeLists.txt / README.md / CLAUDE.md / CHANGELOG.md

用法: python tools/check_source_layout.py [repo_dir]
退出码: 0 = 干净, 1 = 有问题
"""

import os
import sys

ROOT_DOCS = {"CMakeLists.txt", "README.md", "CLAUDE.md", "CHANGELOG.md", "CMakePresets.json"}
ARTIFACT_EXT = (".exe", ".dll", ".pyd", ".obj", ".o")
SRC_ROOTS = ("src", "vendor")


def find_nested_dupes(base):
    """T/T/... 式自复制: 顶层名在自身子路径里再次出现。

    注意必须从仓库根往下算, 不能以顶层目录为 base —— 否则 base 本身被
    relpath 剥掉, `src/src/game` 变成 `src/game` 就检测不到了。
    """
    bad = []
    top = base.split(os.sep)[-1]
    for root, dirs, _ in os.walk(base):
        rel = os.path.relpath(root, os.path.dirname(base)).replace(os.sep, "/")
        parts = rel.split("/")
        if top in parts[1:]:
            bad.append(rel)
            dirs[:] = []          # 已判定为脏树, 不再往下走
    return sorted(bad)


def find_artifacts(base):
    """源目录下的构建产物。vendor 的 lib/ include/ 子树是合法第三方库。"""
    bad = []
    for root, _, fs in os.walk(base):
        rel = os.path.relpath(root, base).replace(os.sep, "/")
        parts = rel.split("/") if rel != "." else []
        if parts and parts[-1] in ("lib", "include"):
            continue              # vendor/raylib/lib/raylib.dll 是合法的
        for fn in fs:
            if fn.lower().endswith(ARTIFACT_EXT):
                bad.append(os.path.relpath(os.path.join(root, fn), base)
                           .replace(os.sep, "/"))
    return sorted(bad)


def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else "."
    problems = []

    print("=== 1. 嵌套自复制目录 ===")
    for top in sorted(os.listdir(repo)):
        p = os.path.join(repo, top)
        if not os.path.isdir(p) or top.startswith("build") or top in ("build-debug",
                "build_mingw", "build_test", "dist", "Testing", "node_modules"):
            continue
        for rel in find_nested_dupes(p):
            problems.append(("nested-dup", "%s/%s" % (top, rel)))
    print("    干净" if not problems else "    %d 处" % len(problems))

    print("=== 2. 源目录下的构建产物 ===")
    n0 = len(problems)
    for top in SRC_ROOTS:
        p = os.path.join(repo, top)
        if not os.path.isdir(p):
            continue
        for rel in find_artifacts(p):
            problems.append(("artifact", "%s/%s" % (top, rel)))
    print("    干净" if len(problems) == n0 else "    %d 处" % (len(problems) - n0))

    print("=== 3. 仓库根文档误入 vendor/ ===")
    n0 = len(problems)
    v = os.path.join(repo, "vendor")
    if os.path.isdir(v):
        for fn in os.listdir(v):
            if fn in ROOT_DOCS:
                problems.append(("vendor-doc", "vendor/%s" % fn))
    print("    干净" if len(problems) == n0 else "    %d 处" % (len(problems) - n0))

    cpp = 0
    s = os.path.join(repo, "src")
    if os.path.isdir(s):
        cpp = sum(1 for _, _, fs in os.walk(s) for fn in fs if fn.endswith(".cpp"))

    print("\nsrc 下 .cpp 总数: %d" % cpp)
    if problems:
        print("\n!! 发现 %d 处问题:" % len(problems))
        for kind, rel in problems[:40]:
            print("   [%-11s] %s" % (kind, rel))
        if len(problems) > 40:
            print("   ... 另有 %d 处" % (len(problems) - 40))
        return 1
    print("\n布局干净: GLOB_RECURSE 不会收到重复源文件。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
