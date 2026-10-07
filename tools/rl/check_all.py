# -*- coding: utf-8 -*-
"""RL 链路总闸门 — 依次跑四道一致性检查, 任一失败即非零退出。

四道闸门各自防一类静默错误:
  1. parity_test.py    C++ CombatEnvironment ≡ Python 移植 (逐位)
  2. verify_weights.py C++ 手写前向   ≡ PyTorch (权重布局 + ReLU)
  3. verify_policy.py  C++ DqnPolicy  ≡ Python (输入缩放 + masked argmax)
  4. dqn_net_test      C++ 单元测试 (结构校验 / 坏权重拒绝)

用法:
    python tools/rl/check_all.py
    python tools/rl/check_all.py --skip-parity     # 跳过较慢的 parity

注意 (踩过的坑): MinGW 的依赖追踪只比时间戳, 用脚本改文件内容时若保留了
原时间戳, make不会重编 → 拿着旧 exe 跑校验会得到假失败。verify_all 之前
务必确认目标确实重编了 (脚本里对每个 exe 打印构建结果)。
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
PY = sys.executable
# DQN 目标在 tests/dqn/ 子目录 (独立 CMakeLists, 避开 tests/CMakeLists.txt
# 被并行开发还原的问题), 故 exe 输出在 build/tests/dqn/ 而非 build/tests/
EXE_DIR = os.path.join(ROOT, "build", "tests", "dqn")
OBJ_ROOT = os.path.join(ROOT, "build", "tests", "dqn", "CMakeFiles")


def run(label: str, args: list, cwd: str = ROOT) -> tuple:
    print(f"\n=== {label} ===")
    try:
        r = subprocess.run(args, cwd=cwd, capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=900)
    except subprocess.TimeoutExpired:
        print("  TIMEOUT")
        return label, False
    out = (r.stdout or "") + (r.stderr or "")
    for line in out.strip().splitlines()[-6:]:
        # Windows 控制台是 GBK, 直接 print 替换字符会抛 UnicodeEncodeError
        print("  " + line.encode("ascii", "replace").decode("ascii"))
    ok = r.returncode == 0
    print(f"  --> {'PASS' if ok else 'FAIL'}")
    return label, ok


def find_cmake() -> str:
    """定位 cmake 可执行文件。

    直接 subprocess.run(["cmake", ...]) 在本机会抛 FileNotFoundError ——
    anaconda 的 cmake 位于 <prefix>/Library/bin, 只有 PowerShell 的会话
    PATH 里有, 子进程继承不到。故显式回退到几个已知位置。
    """
    found = shutil.which("cmake")
    if found:
        return found
    for cand in (r"C:\mingw64\bin\cmake.exe",
                 r"C:\Program Files\CMake\bin\cmake.exe",
                 os.path.join(sys.prefix, "Library", "bin", "cmake.exe"),
                 os.path.join(sys.prefix, "Scripts", "cmake.exe")):
        if os.path.exists(cand):
            return cand
    raise FileNotFoundError("找不到 cmake — 请手动构建四个目标后重跑")


def build_targets(names: list) -> bool:
    """强制重编 — 必须删 obj 目录, 不能只跑 cmake --build。

    踩过的坑: 用脚本改 .cpp 内容做变异测试再改回来时, 若用 Copy-Item 恢复
    备份, 文件的 mtime 会保持"备份时刻"而非"内容变化时刻"。MinGW/Make 的
    依赖追踪只比时间戳, 于是 `cmake --build` 认为目标是最新的, 根本不重编,
    拿着变异版 exe 跑校验 → 假失败 (本轮实际踩中两次: dqn_probe 的权重索引
    倒置、dqn_policy_probe 的缩放系数 50->40)。

    正确做法: 删掉 <target>.dir 整个对象目录 + 删 exe, 再 configure + build。
    """
    print("=== 强制重建校验用可执行文件 ===")
    for name in names:
        objdir = os.path.join(OBJ_ROOT, f"{name}.dir")
        if os.path.isdir(objdir):
            shutil.rmtree(objdir, ignore_errors=True)
        for exe in glob.glob(os.path.join(EXE_DIR, f"{name}.exe")):
            try:
                os.remove(exe)
            except OSError:
                pass

    cmake = find_cmake()
    cfg = subprocess.run([cmake, "-B", "build"], cwd=ROOT,
                         capture_output=True, text=True,
                         encoding="utf-8", errors="replace")
    if cfg.returncode != 0:
        print("  cmake configure FAILED")
        print((cfg.stdout or "")[-500:])
        return False

    r = subprocess.run([cmake, "--build", "build", "--target"] + names,
                       cwd=ROOT, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    ok = r.returncode == 0
    print(f"  targets={names} --> {'OK' if ok else 'BUILD FAILED'}")
    if not ok:
        print((r.stdout or "")[-500:])
    return ok


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--skip-parity", action="store_true")
    ap.add_argument("--skip-build", action="store_true")
    args = ap.parse_args()

    if not args.skip_build:
        if not build_targets(["dqn_probe", "dqn_policy_probe",
                              "parity_dump", "dqn_net_test"]):
            return 2

    # 权重产物必须存在
    weights = os.path.join(HERE, "dqn_weights.bin")
    if not os.path.exists(weights):
        print(f"缺少 {weights} — 先跑 export_weights.py")
        return 2

    results = []
    if not args.skip_parity:
        dump = os.path.join(EXE_DIR, "parity_dump.exe")
        subprocess.run([dump, "777", "12",
                        os.path.join(HERE, "parity_trace.json")],
                       cwd=ROOT, capture_output=True)
        results.append(run("1. Python 移植 parity",
                           [PY, os.path.join(HERE, "parity_test.py")]))
    results.append(run("2. C++ 前向 vs PyTorch",
                       [PY, os.path.join(HERE, "verify_weights.py")]))
    results.append(run("3. C++ DqnPolicy vs Python",
                       [PY, os.path.join(HERE, "verify_policy.py")]))
    exe = os.path.join(EXE_DIR, "dqn_net_test.exe")
    results.append(run("4. DqnNet 单元测试", [exe]))

    print("\n" + "=" * 52)
    for label, ok in results:
        print(f"  {'PASS' if ok else 'FAIL'}  {label}")
    failed = [l for l, ok in results if not ok]
    if failed:
        print(f"\n{len(failed)} 道闸门未通过: {', '.join(failed)}")
        return 1
    print("\n全部闸门通过 — C++ 推理链路与 Python 训练链路一致")
    return 0


if __name__ == "__main__":
    sys.exit(main())