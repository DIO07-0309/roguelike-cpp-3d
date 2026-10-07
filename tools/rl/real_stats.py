# -*- coding: utf-8 -*-
"""真实战斗参数加载器 — 从 resources/*.json 读武器/技能/敌人配置。

为什么需要: RL 的玩具 env 里"普攻"和"技能"用的是同一个虚拟 attack 值,
技能硬编码 1.5x 系数, 于是技能客观上碾压普攻, DQN 学出 attack 占比 0%。
但真实数据恰好相反 —— resources/weapons.json 里 21 把武器的 base_damage
是 4~13(均值 7.2), base_range 1~12, 普攻 DPS 本就是战斗主力; 技能
(resources/skills.json) 的冷却 2~20 秒, 属爆发手段而非持续输出。

本模块把真实 JSON 抽成 RL env 可用的参数, 消除"拍脑袋数字"这一根源。
只读 JSON, 不改游戏逻辑。
"""
import json
import os
from typing import Dict, List, Optional

_HERE = os.path.dirname(os.path.abspath(__file__))
_RES = os.path.abspath(os.path.join(_HERE, "..", "..", "resources"))


def _load(name: str) -> dict:
    path = os.path.join(_RES, name)
    with open(path, encoding="utf-8") as f:
        return json.load(f)


# ── 武器 ──
def load_weapons() -> List[dict]:
    return _load("weapons.json")["weapons"]


def weapon_stats() -> Dict[str, dict]:
    """武器类型 → {dmg_min, dmg_max, dmg_mean, range_max, combo_timeout}"""
    by_type: Dict[str, List[dict]] = {}
    for w in load_weapons():
        by_type.setdefault(w["type"], []).append(w)
    out = {}
    for t, group in by_type.items():
        dmg = [g["base_damage"] for g in group]
        rng = [g["base_range"] for g in group]
        out[t] = {
            "dmg_min": min(dmg),
            "dmg_max": max(dmg),
            "dmg_mean": sum(dmg) / len(dmg),
            "range_max": max(rng),
            "range_min": min(rng),
            "combo_timeout": min(g.get("combo_timeout", 0.9) for g in group),
        }
    return out


# ── 技能 ──
def load_skills() -> List[dict]:
    data = _load("skills.json")
    return data["skills"] if isinstance(data, dict) else data


def active_skills() -> List[dict]:
    """cooldown > 0 的技能 (modifier 类技能冷却为 0, 不是主动动作)"""
    return [s for s in load_skills() if float(s.get("cooldown", 0)) > 0]


def skill_cooldowns() -> List[float]:
    """按 skills.json 顺序返回主动技能的冷却秒数"""
    return [float(s["cooldown"]) for s in active_skills()]


# ── 敌人 (取Boss 血量/攻击量级做场景参数化) ──
def load_enemies() -> List[dict]:
    return _load("enemies.json").get("enemies", [])


def summary() -> None:
    w = weapon_stats()
    print("=== 武器 (按类型) ===")
    for t, s in w.items():
        print(f"  {t:<9} dmg {s['dmg_min']}-{s['dmg_max']} "
              f"(均值{s['dmg_mean']:.1f})  range {s['range_min']}-{s['range_max']}  "
              f"combo_timeout {s['combo_timeout']}")

    print("\n=== 主动技能 (冷却秒) ===")
    for s in active_skills():
        print(f"  {s['id']:<14} cd={s['cooldown']:>5}  "
              f"tags={','.join(s.get('tags', []))}")

    # DPS 对比: 普攻 vs 技能 (假设 combo_timeout 内可接连击)
    print("\n=== 粗略 DPS 对比 (秒) ===")
    best = max(w.values(), key=lambda s: s["dmg_mean"] / max(s["combo_timeout"], 0.1))
    atk_dps = best["dmg_mean"] / best["combo_timeout"]
    print(f"  最优普攻({best['dmg_mean']:.1f} dmg / {best['combo_timeout']}s)"
          f" = {atk_dps:.1f} DPS")
    for s in active_skills():
        cd = float(s["cooldown"])
        if cd <= 0:
            continue
        print(f"  技能 {s['id']:<14} 1次/ {cd:>5}s  (每击伤害未知, 需 dqn env 自行建模)")


if __name__ == "__main__":
    summary()