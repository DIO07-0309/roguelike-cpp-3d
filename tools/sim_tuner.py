#!/usr/bin/env python3
# P3-redesign 系统性调参 — 批量评估脚本.
# 用 SIM_TUNE_* 环境变量扫描参数组合, 每个配置跑多个 seed, 汇总 KPI 表.
# 用法: python tools/sim_tuner.py --runs N --seeds 1,2,3 --preset grid|quick
import argparse, json, os, subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
EXE = REPO / "build" / "roguelike_cpp.exe"
REPORT = REPO / "reports" / "balance_report.json"
KNOBS = [
    "SIM_TUNE_LOOT_W",
    "SIM_TUNE_ROOM_W",
    "SIM_TUNE_STUCK",
    "SIM_TUNE_WALL",
    "SIM_TUNE_ATK",
    "SIM_TUNE_HP",
    "SIM_TUNE_HUNT",
    "SIM_TUNE_BOSSHP",
    "SIM_TUNE_BOSSATK",
]

# 预设配置网格 (默认值见 sim_tuning.cpp: loot.3 room.2 stuck400 wall72000 atk6 hp30 hunt8 bosshp5 bossatk2.5)
GRID = [
    {"name": "default", "env": {}},
    {"name": "stuck240", "env": {"SIM_TUNE_STUCK": "240"}},  # A/B 基线: 旧预算 240s
    {"name": "stuck400", "env": {"SIM_TUNE_STUCK": "400"}},  # 达标值 (default 与其等价)
    {"name": "boss_3", "env": {"SIM_TUNE_BOSSHP": "3"}},
    {"name": "boss_2", "env": {"SIM_TUNE_BOSSHP": "2"}},
    {"name": "player_atk10", "env": {"SIM_TUNE_ATK": "10"}},
    {"name": "player_atk14", "env": {"SIM_TUNE_ATK": "14"}},
    {
        "name": "loot0_room0",
        "env": {"SIM_TUNE_LOOT_W": "0.05", "SIM_TUNE_ROOM_W": "0.05"},
    },
    {
        "name": "no_suppress",
        "env": {"SIM_TUNE_LOOT_W": "0.7", "SIM_TUNE_ROOM_W": "0.5"},
    },
    {"name": "hunt3", "env": {"SIM_TUNE_HUNT": "3"}},
    {
        "name": "combo_strong",
        "env": {"SIM_TUNE_BOSSHP": "3", "SIM_TUNE_ATK": "10", "SIM_TUNE_HP": "40"},
    },
    {
        "name": "combo_max",
        "env": {
            "SIM_TUNE_BOSSHP": "2",
            "SIM_TUNE_BOSSATK": "1.8",
            "SIM_TUNE_ATK": "14",
            "SIM_TUNE_HP": "50",
            "SIM_TUNE_STUCK": "400",
        },
    },
]


def run_one(env, runs, seed):
    # 隔离: 清除所有 SIM_TUNE_* 与 SIM_RD_* 残留, 防父环境污染 (假阴性/假阳性)
    clean = {
        k: v
        for k, v in os.environ.items()
        if k not in KNOBS and not k.startswith("SIM_RD_")
    }
    child = dict(clean)
    child.update(env)
    # 防脏读: 跑前删旧 report (golden test 同款保护)
    REPORT.unlink(missing_ok=True)
    try:
        proc = subprocess.run(
            [str(EXE), "--sim", str(runs), "--sim-seed", str(seed)],
            cwd=str(REPO),
            env=child,
            capture_output=True,
            text=True,
            timeout=600,
        )
    except subprocess.TimeoutExpired:
        return None
    if proc.returncode != 0 or not REPORT.exists():
        return None
    try:
        d = json.loads(REPORT.read_text(encoding="utf-8"))
        s = d["summary"]
    except Exception:
        return None
    # 校验本次报告确实由 runs 局产出, 防残留/部分写入
    if s.get("total_runs") != runs:
        return None
    return {
        "win": s["win_rate"],
        "floor": s["avg_floor"],
        "stuck": s["outcome_dist"]["STUCK_RECOVERED"],
        "boss5": s["boss_kill_counts"][0],
        "boss10": s["boss_kill_counts"][1],
        "boss15": s["boss_kill_counts"][2],
        "maxfloor": max(r["floor"] for r in d["runs"]),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=20)
    ap.add_argument("--seeds", default="4242")
    ap.add_argument("--preset", choices=["grid", "quick"], default="quick")
    args = ap.parse_args()
    seeds = [int(x) for x in args.seeds.split(",")]
    configs = GRID if args.preset == "grid" else GRID[:6]
    print(f"runs={args.runs} seeds={seeds} configs={len(configs)}")
    print(
        f"{'config':<16} {'win%':>6} {'floor':>6} {'stuck':>6} {'B5':>4} {'B10':>4} {'B15':>4} {'maxf':>5}"
    )
    for cfg in configs:
        agg = {
            "win": 0.0,
            "floor": 0.0,
            "stuck": 0,
            "boss5": 0,
            "boss10": 0,
            "boss15": 0,
            "maxfloor": 0,
        }
        ok = 0
        for seed in seeds:
            r = run_one(cfg["env"], args.runs, seed)
            if r is None:
                continue
            ok += 1
            for k in agg:
                agg[k] += r[k]
        if not ok:
            print(f"{cfg['name']:<16} FAILED")
            continue
        for k in agg:
            agg[k] /= ok
        print(
            f"{cfg['name']:<16} {agg['win'] * 100:>5.1f} {agg['floor']:>6.2f} "
            f"{agg['stuck']:>6.1f} {agg['boss5']:>4.0f} {agg['boss10']:>4.0f} "
            f"{agg['boss15']:>4.0f} {agg['maxfloor']:>5.0f}"
        )


if __name__ == "__main__":
    main()
