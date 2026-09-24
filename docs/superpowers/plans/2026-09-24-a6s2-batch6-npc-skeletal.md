# A6-S2 批次6：10 NPC 骨骼化 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 10 世界 NPC 生成独立骨骼分件并接入 2D+HD2D 世界渲染（骨骼优先、静态图回落），白名单 30→40；肖像/标题仍用旧静态图。

**Architecture:** 新建 `tools/gen_npc_parts.py`（import humanoid rig 常量，独立 `NPC_FAMILIES`/`NPCS`）；注册链同 mon（sprites `skeleton_parts` +50、`actor_avatars` +10 键 `npc_20`…）；C++ 镜像 mon：`NpcView.npc_id` + `_npc_avatars_tick` + 2D `draw_at` / HD2D `part_draws` 分支。

**Tech Stack:** Python 3 + Pillow（conda run）、C++17 + CMake + GoogleTest、JSON 资源。

## Global Constraints

- **Spec**：`docs/superpowers/specs/2026-09-24-a6s2-batch6-npc-skeletal-design.md`（`b38bcee`，用户已批）。
- **不修改** `gen_mon_humanoid_parts.py` / `gen_mon_soft_parts.py` / `gen_mon_float_golem_parts.py`；旧 30 mon 分件/骨架 + 4 张 `npc_*.png` 静态图 **严格零 diff**。
- **裁定 B4-2**：sim A/B 用 `Out-File -Encoding utf8`；真门禁 = ctest + validator + 实机验收；无头 sim 不实例化 SkeletonAvatar → 预期与批次5 基线 sha `b6814bdd4b44bf15e92153db5c4876552ea1d9f04ade82b68a724a155cc17d06` 逐字节一致；意外分叉 → 同 seed 两次同 sha + 无崩溃 + CHANGELOG 如实记录。
- 生成器 ≤300 行、函数 ≤40 行；ppu 全 0.8；色板/武器行字符只允许 `. o s m l h r R p b g`；描边 `(63,38,49)`；二值 alpha。
- 空手：`weapon=None` → weapon 行全 `.` 透明 PNG（**不进 `WEAPONS[...]`**，避开 `_bbox` 全透明崩溃）。
- 注册目标：`skeleton_parts` **155→205**、`actor_avatars` **30→40**、animation_test expected **30→40**；DIMS torso(28,20) head(22,20) arm(11,16) leg(22,20) weapon(6,24)。
- **不改**：对话肖像、标题页、`npc_sprite_key()`、4 静态 PNG 注册、mon 渲染路径、encounters 叙事 NPC。
- JSON/中文 README 改动用 Python 脚本 + UTF-8；PS 管道 `>` 禁用（用 `Out-File -Encoding utf8`）。
- Python 一律 `conda run python`；多行脚本写 `C:\Users\HP\AppData\Local\Temp\opencode\`；脚本只 ASCII 输出。
- **提交策略**：全批单一 commit，经用户实机验收确认后执行。
- 交付前同步桌面包 `C:\Users\HP\Desktop\Roguelike-CPP-3D版`（robocopy /MIR 8 目录 exit 0/1 成功；根文件 + **根目录 exe**；保留 `saves/`、`3D模式.exe.lnk`）。
- sheet 在 `reports/`（.gitignore），本地评审不进 git。
- 函数 ≤40 行；每步 Code Review。

---

### Task 1: 新建 `gen_npc_parts.py` + dry-run + sheet 评审

**Files:**
- Create: `tools/gen_npc_parts.py`
- Create temp: `C:\Users\HP\AppData\Local\Temp\opencode\b6dry\{sprites,anim}\`
- Artifact: `C:\Users\HP\AppData\Local\Temp\opencode\b6dry\sheet_npc.png`

**Interfaces:**
- Produces: 10 NPC 分件+骨架生成能力；CLI `--output-dir/--skeleton-dir/--sheet`；键名 `npc_{20,30,40,60,70,80,90,110,120,140}`（Task 3 依赖）。

- [ ] **Step 1: 写入完整生成器**（≤300 行；import 复用 humanoid，**本地** palette/part_grid/skeleton_dict——humanoid 的 `build_palette` 绑死其 `FAMILIES`，不可直接用于 NPC 色板）：

```python
# A6-S2 批次6: NPC 骨骼分件生成器
# 复用 humanoid rig/管道; 产出 npc_{id}_part_*.png + npc_{id}_skeleton.json
import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw

from anim_preview import checkerboard, render_pose
from gen_mon_humanoid_parts import (
    BONES, OUTLINE, PART_ROWS, PART_SLOTS, SIZES, TIERS, WEAPONS, scale_rows,
)

ROOT = Path(__file__).resolve().parents[1]

NPC_FAMILIES = {
    "npc_prisoner": {"s": (52, 48, 44), "m": (92, 86, 76), "l": (134, 126, 112),
                     "h": (182, 172, 154), "r": (88, 70, 52), "R": (128, 102, 74),
                     "p": (170, 150, 120), "b": (36, 32, 28), "g": (96, 88, 70)},
    "npc_hunter": {"s": (58, 42, 28), "m": (104, 74, 48), "l": (152, 112, 72),
                   "h": (206, 160, 108), "r": (78, 56, 36), "R": (118, 86, 54),
                   "p": (196, 152, 100), "b": (34, 26, 18), "g": (96, 88, 70)},
    "npc_survivor": {"s": (48, 46, 42), "m": (88, 84, 76), "l": (128, 122, 110),
                     "h": (174, 166, 150), "r": (72, 68, 60), "R": (108, 102, 90),
                     "p": (160, 152, 136), "b": (30, 28, 26), "g": (96, 88, 70)},
    "npc_collector": {"s": (72, 48, 24), "m": (128, 86, 40), "l": (186, 132, 60),
                      "h": (236, 186, 104), "r": (110, 72, 32), "R": (156, 106, 48),
                      "p": (224, 168, 88), "b": (40, 28, 16), "g": (96, 88, 70)},
    "npc_priest": {"s": (120, 116, 96), "m": (188, 184, 160), "l": (232, 228, 204),
                   "h": (252, 250, 236), "r": (168, 140, 60), "R": (220, 188, 90),
                   "p": (248, 236, 180), "b": (54, 50, 42), "g": (96, 88, 70)},
    "npc_scout": {"s": (40, 58, 36), "m": (70, 104, 54), "l": (110, 152, 80),
                  "h": (156, 198, 118), "r": (54, 78, 44), "R": (86, 120, 64),
                  "p": (140, 184, 104), "b": (28, 36, 24), "g": (96, 88, 70)},
    "npc_pilgrim": {"s": (66, 62, 56), "m": (112, 106, 96), "l": (160, 152, 138),
                    "h": (210, 202, 186), "r": (90, 82, 70), "R": (132, 122, 106),
                    "p": (188, 178, 160), "b": (40, 36, 32), "g": (96, 88, 70)},
    "npc_ghost": {"s": (92, 118, 128), "m": (148, 186, 198), "l": (200, 230, 240),
                  "h": (238, 252, 255), "r": (120, 150, 162), "R": (170, 206, 220),
                  "p": (220, 244, 252), "b": (48, 62, 70), "g": (96, 88, 70)},
    "npc_dreamer": {"s": (52, 36, 72), "m": (92, 64, 126), "l": (140, 104, 180),
                    "h": (194, 158, 230), "r": (76, 52, 104), "R": (118, 84, 156),
                    "p": (176, 136, 220), "b": (28, 20, 40), "g": (96, 88, 70)},
    "npc_watcher": {"s": (36, 56, 72), "m": (58, 98, 124), "l": (96, 148, 178),
                    "h": (150, 204, 232), "r": (70, 120, 90), "R": (100, 166, 128),
                    "p": (180, 230, 210), "b": (28, 38, 48), "g": (96, 88, 70)},
}

NPCS = {
    "npc_20": ("npc_prisoner", "standard", None),
    "npc_30": ("npc_hunter", "standard", "dagger"),
    "npc_40": ("npc_survivor", "standard", None),
    "npc_60": ("npc_collector", "standard", None),
    "npc_70": ("npc_priest", "standard", "staff"),
    "npc_80": ("npc_scout", "runt", "dagger"),
    "npc_90": ("npc_pilgrim", "gaunt", None),
    "npc_110": ("npc_ghost", "gaunt", None),
    "npc_120": ("npc_dreamer", "standard", None),
    "npc_140": ("npc_watcher", "bulk", "cleaver"),
}
assert len(NPCS) == 10, "NPCS must have exactly 10 entries"


def build_palette(family):
    palette = {".": (0, 0, 0, 0), "o": OUTLINE}
    for char, rgb in NPC_FAMILIES[family].items():
        palette[char] = (*rgb, 255)
    return palette


def part_grid(npc_id, part_name):
    family, tier, weapon = NPCS[npc_id]
    if part_name == "weapon":
        if weapon is None:
            w, h = SIZES["weapon"]
            return ["." * w for _ in range(h)]
        rows = list(WEAPONS[weapon])
    else:
        rows = list(PART_ROWS[part_name])
    factor_x, factor_y, _ = TIERS[tier]
    return scale_rows(rows, factor_x, factor_y)


def part_image(npc_id, part_name):
    family = NPCS[npc_id][0]
    palette = build_palette(family)
    rows = part_grid(npc_id, part_name)
    width, height = SIZES[part_name]
    if len(rows) != height or any(len(r) != width for r in rows):
        raise ValueError(f"Invalid pixel grid: {npc_id} {part_name}")
    image = Image.new("RGBA", (width, height))
    image.putdata([palette[pixel] for row in rows for pixel in row])
    if not {p[3] for p in image.getdata()} <= {0, 255}:
        raise ValueError(f"Non-binary alpha: {npc_id} {part_name}")
    return image


def skeleton_dict(npc_id):
    _, tier, _ = NPCS[npc_id]
    ppu = TIERS[tier][2]
    parts = [{"bone": bone,
              "file": f"assets/sprites/{npc_id}_part_{name}.png",
              "pivot": list(pivot)} for bone, name, pivot in PART_SLOTS]
    return {"pixels_per_unit": ppu, "anchor": [24, 62],
            "bones": BONES, "parts": parts}


def generate(output_dir, skeleton_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    skeleton_dir.mkdir(parents=True, exist_ok=True)
    for npc_id in NPCS:
        images = {name: part_image(npc_id, name) for name in SIZES}
        for name, image in images.items():
            image.save(output_dir / f"{npc_id}_part_{name}.png")
        path = skeleton_dir / f"{npc_id}_skeleton.json"
        path.write_text(json.dumps(skeleton_dict(npc_id), indent=2) + "\n",
                        encoding="utf-8")
        print(f"wrote {npc_id} parts+skeleton")
    return {nid: {name: part_image(nid, name) for name in SIZES}
            for nid in NPCS}


def make_sheet(images):
    row_h, part_scale, canvas_w = 132, 3, 640
    sheet = checkerboard((canvas_w, row_h * len(NPCS) + 20))
    draw = ImageDraw.Draw(sheet)
    for index, npc_id in enumerate(NPCS):
        family, tier, weapon = NPCS[npc_id]
        y = index * row_h + 14
        draw.text((6, y - 10), npc_id, fill=(205, 220, 222))
        label = "empty" if weapon is None else weapon
        draw.text((6, y + 2), f"{tier} ppu={TIERS[tier][2]} {label}",
                  fill=(149, 166, 183))
        x = 178
        for name in ("head", "torso", "arm", "leg", "weapon"):
            piece = images[npc_id][name]
            enlarged = piece.resize((piece.width * part_scale,
                                     piece.height * part_scale),
                                    Image.Resampling.NEAREST)
            sheet.alpha_composite(enlarged, (x, y - 6))
            x += enlarged.width + 12
        pieces = [images[npc_id][name] for _, name, _ in PART_SLOTS]
        pose = render_pose(skeleton_dict(npc_id), pieces, None, 0, 1.6)
        sheet.alpha_composite(pose, (500, y - 16))
    return sheet


def main():
    parser = argparse.ArgumentParser(description="Generate npc_* skeleton parts.")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "assets" / "sprites")
    parser.add_argument("--skeleton-dir", type=Path,
                        default=ROOT / "resources" / "animations")
    parser.add_argument("--sheet", type=Path, default=ROOT / "reports" / "npc_sheet.png")
    args = parser.parse_args()
    images = generate(args.output_dir, args.skeleton_dir)
    args.sheet.parent.mkdir(parents=True, exist_ok=True)
    make_sheet(images).save(args.sheet)
    print(f"sheet -> {args.sheet}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: 行数门 + py_compile**

Run: 临时脚本 `len(open(...).readlines())` + `conda run python -m py_compile tools/gen_npc_parts.py`
Expected: 行数 ≤300；py_compile 无输出错误。

- [ ] **Step 3: 空手/武器尺寸自检**（临时 `b6_npc_check.py`）：

```python
import importlib.util
spec = importlib.util.spec_from_file_location("g", "tools/gen_npc_parts.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
assert len(m.NPCS) == 10
empty = m.part_grid("npc_20", "weapon")
assert len(empty) == 24 and all(set(r) == {"."} for r in empty)
armed = m.part_grid("npc_30", "weapon")
assert len(armed) == 24 and any(c != "." for r in armed for c in r)
assert m.skeleton_dict("npc_20")["pixels_per_unit"] == 0.8
print("CHECK OK")
```

Run: `conda run python C:\Users\HP\AppData\Local\Temp\opencode\b6_npc_check.py`
Expected: `CHECK OK`；否则回修 Step 1。

- [ ] **Step 4: mon 生成器零改动断言**

Run: `git status --short tools/gen_mon_humanoid_parts.py tools/gen_mon_soft_parts.py tools/gen_mon_float_golem_parts.py`
Expected: 无输出（三文件未改）。

- [ ] **Step 5: dry-run 到临时目录**

Run:
```
conda run python tools\gen_npc_parts.py --output-dir C:\Users\HP\AppData\Local\Temp\opencode\b6dry\sprites --skeleton-dir C:\Users\HP\AppData\Local\Temp\opencode\b6dry\anim --sheet C:\Users\HP\AppData\Local\Temp\opencode\b6dry\sheet_npc.png
```
Expected: 10 行 `wrote npc_*`；sheet 落盘；无 KeyError/ValueError/断言失败。

- [ ] **Step 6: mon 30 怪 + 4 静态图零 diff**（临时 `b6_zero_diff.py`）：

```python
import hashlib, pathlib
MON = ["orc","elite_orc","archer","shaman","goblin_hunter","tank",
       "bone_soldier","skeleton_archer","charger","summoner","necromancer",
       "ice_warden","blood_priest","dark_mage","shadow_stalker",
       "shadow_assassin","night_stalker","poison_wyrm","slime","bomber",
       "elite_slime","frost_slime","blood_leech","golem","stone_guardian",
       "iron_sentinel","lightning_orb","fire_imp","storm_elemental","void_walker"]
def h(p): return hashlib.sha256(p.read_bytes()).hexdigest()
bad = []
for m in MON:
    for part in ["torso","head","arm","leg","weapon"]:
        p = pathlib.Path(f"assets/sprites/mon_{m}_part_{part}.png")
        if not p.exists() or p.stat().st_size == 0: bad.append(str(p))
    p = pathlib.Path(f"resources/animations/mon_{m}_skeleton.json")
    if not p.exists(): bad.append(str(p))
for n in ["npc_1","npc_2","npc_3","npc_blacksmith"]:
    p = pathlib.Path(f"assets/sprites/{n}.png")
    if not p.exists(): bad.append(str(p))
# dry-run 不得写入仓库 mon/静态图: 与 git 可比对象存在即可
print("ZERO-DIFF OK" if not bad else f"MISSING {len(bad)}")
for x in bad: print(x)
```

Run: `conda run python C:\Users\HP\AppData\Local\Temp\opencode\b6_zero_diff.py`
Expected: **`ZERO-DIFF OK`**；且 `git status --short assets/sprites resources/animations` 对 mon/npc_*.png **无 ` M`**（dry-run 只写临时目录）。

- [ ] **Step 7: sheet 用户评审**（Read 附 `sheet_npc.png` 给用户）
Expected: 用户确认 10 造型/武器/空手透明可接受；不接受 → 迭代 Step 1 后重跑 5-7。

### Task 2: 正式生成（零 diff 门）

**Files:**
- Create: `assets/sprites/npc_{20,30,40,60,70,80,90,110,120,140}_part_{torso,head,arm,leg,weapon}.png`（50）
- Create: `resources/animations/npc_{20,...,140}_skeleton.json`（10）
- Side-effect（gitignored）: `reports/npc_sheet.png`

**Interfaces:**
- Produces: 50 PNG + 10 JSON（ppu 0.8）；文件名 `{npc_id}_part_{slot}` / `{npc_id}_skeleton.json`（Task 3 依赖）。

- [ ] **Step 1: 正式运行**

Run: `conda run python tools\gen_npc_parts.py`
Expected: 10 行 `wrote npc_*`；`reports/npc_sheet.png` 更新。

- [ ] **Step 2: git 新文件在位 + 旧文件零改**

Run: `git status --short assets/sprites resources/animations tools`
Expected: **恰好 60 个 `??`**（50 PNG + 10 JSON + `tools/gen_npc_parts.py` 若未先 add）；**零 ` M`** 于 mon 分件/骨架/`npc_{1,2,3,blacksmith}.png`/三 mon 生成器。任何 ` M` → 停止。

- [ ] **Step 3: ppu 抽检**

临时脚本：10 JSON `pixels_per_unit == 0.8` 且 `parts` 均含 `file` 以 `assets/sprites/npc_` 开头。
Expected: 全 True。

### Task 3: 资源注册 + animation_test + 构建 + 全量测试

**Files:**
- Modify: `resources/sprites.json`（skeleton_parts +50 → 205）
- Modify: `resources/animations/actor_avatars.json`（actors +10 → 40）
- Modify: `tests/animation/animation_test.cpp:685-699`（注释批次1-6、expected +10 → 40）
- Create: `C:\Users\HP\AppData\Local\Temp\opencode\reg_b6.py`

**Interfaces:**
- Consumes: Task 2 的 50 PNG/10 JSON 文件名。
- Produces: 白名单 40 键、测试断言 40；C++ 接线（Task 4）查 `"npc_" + id` 命中此白名单。

- [ ] **Step 1: 写并执行登记脚本**（`reg_b6.py`）：

```python
import json
from pathlib import Path
p = Path("resources/sprites.json")
j = json.loads(p.read_text(encoding="utf-8"))
DIMS = {"torso": (28, 20), "head": (22, 20), "arm": (11, 16),
        "leg": (22, 20), "weapon": (6, 24)}
ids = ["npc_20", "npc_30", "npc_40", "npc_60", "npc_70",
       "npc_80", "npc_90", "npc_110", "npc_120", "npc_140"]
added = 0
for nid in ids:
    for name, (w, h) in DIMS.items():
        key = f"{nid}_part_{name}"
        if key not in j["skeleton_parts"]:
            j["skeleton_parts"][key] = {
                "file": f"assets/sprites/{nid}_part_{name}.png",
                "w": w, "h": h}
            added += 1
p.write_text(json.dumps(j, indent=2, ensure_ascii=False) + "\n",
             encoding="utf-8")
print("added", added, "total", len(j["skeleton_parts"]))
```

Run: `conda run python C:\Users\HP\AppData\Local\Temp\opencode\reg_b6.py`
Expected: `added 50 total 205`。

- [ ] **Step 2: actor_avatars.json 白名单 +10**（`mon_poison_wyrm` 块后、`actors` 收尾 `}` 前；anim 全 `resources/animations/player_anim.json`）：

```json
    "npc_20": {
      "skeleton": "resources/animations/npc_20_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "npc_30": {
      "skeleton": "resources/animations/npc_30_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "npc_40": {
      "skeleton": "resources/animations/npc_40_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "npc_60": {
      "skeleton": "resources/animations/npc_60_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "npc_70": {
      "skeleton": "resources/animations/npc_70_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "npc_80": {
      "skeleton": "resources/animations/npc_80_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "npc_90": {
      "skeleton": "resources/animations/npc_90_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "npc_110": {
      "skeleton": "resources/animations/npc_110_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "npc_120": {
      "skeleton": "resources/animations/npc_120_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "npc_140": {
      "skeleton": "resources/animations/npc_140_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    }
```

- [ ] **Step 3: animation_test expected 改 40**（注释「批次1-5 + 批次6 NPC」，集合尾追加 10 项；旧 30 mon 键不动）：

```cpp
    // A6-S2 批次1 (人形) + 批次2 (软体) + 批次3 (浮灵/魔像) + 批次4 (人形补充) + 批次5 (影武者/毒液蠕虫) + 批次6 (NPC)
    const std::set<std::string> expected = {"mon_orc", "mon_elite_orc", "mon_archer",
                                            "mon_shaman", "mon_goblin_hunter", "mon_tank",
                                            "mon_bone_soldier", "mon_skeleton_archer",
                                            "mon_slime", "mon_bomber", "mon_elite_slime",
                                            "mon_frost_slime", "mon_blood_leech",
                                            "mon_golem", "mon_stone_guardian",
                                            "mon_iron_sentinel", "mon_lightning_orb",
                                            "mon_fire_imp", "mon_storm_elemental",
                                            "mon_void_walker", "mon_charger",
                                            "mon_summoner", "mon_necromancer",
                                            "mon_ice_warden", "mon_blood_priest",
                                            "mon_dark_mage", "mon_shadow_stalker",
                                            "mon_shadow_assassin", "mon_night_stalker",
                                            "mon_poison_wyrm",
                                            "npc_20", "npc_30", "npc_40", "npc_60",
                                            "npc_70", "npc_80", "npc_90", "npc_110",
                                            "npc_120", "npc_140"};
```

- [ ] **Step 4: 计数断言**

临时脚本：`len(skeleton_parts)==205`、`len(actors)==40`、JSON 可解析。
Expected: 全 True。

- [ ] **Step 5: 构建**

Run: `cmake --build build`
Expected: 无 error（本步无 C++ 行为变化，仅测试文件改动 → `animation_test` 重编）。

- [ ] **Step 6: 全量测试**

Run: `cd build; ctest`
Expected: `100% tests passed, 0 tests failed out of 68`（以实测为准）。

- [ ] **Step 7: World Validator**

Run: `conda run python tools\world_validator.py`
Expected: `Errors: 0 / Warnings: 0`。

- [ ] **Step 8: Code Review**

Run: `git status --short src`（应无输出）；三计数 205/40/40；`git diff --stat` 仅 sprites/actor_avatars/animation_test。

### Task 4: C++ 接线（NpcView + tick + 2D + HD2D）+ 构建测试

**Files:**
- Modify: `src/game/scenes/game_scene.h`（NpcView、成员、tick 声明、`npc_avatar` 访问器）
- Modify: `src/game/scenes/game_scene.cpp`（npc_views、tick 定义与两调用点、2D 绘制分支）
- Modify: `src/game/rendering3d/hd2d_scene_builder.cpp`（`_build_npcs` 骨骼分支）

**Interfaces:**
- Consumes: Task 3 白名单键 `npc_{id}`；`SkeletonAvatar::try_init/active/advance/draw_at/part_draws`；`appendAvatarParts`。
- Produces: `NpcView.npc_id`；`GameScene::npc_avatar(int)`；`_npc_avatars_tick()`。

- [ ] **Step 1: `game_scene.h` — NpcView + 声明**（约 :185-189 与 :383 旁、成员区）：

```cpp
    struct NpcView {                       // NPC 快照 (坐标 tile + 是否完成对话 + id)
        int tile_x = 0, tile_y = 0;
        bool finished = false;
        int npc_id = 0;                    // A6-S2 批次6: floor*10+slot
    };
```

公开访问器（贴 `playerAvatar()` 模式，:123 附近或 `npc_views` 旁）：

```cpp
    SkeletonAvatar* npc_avatar(int npc_id);   // A6-S2 批次6: HD2D/2D 只读查骨骼
```

（`SkeletonAvatar` 已由 `monster.h` 前向声明；`.h` 不需新 include。）

私有区（`_monster_avatars_tick` 旁 :383）：

```cpp
    void _npc_avatars_tick();   // A6-S2 批次6: NPC 骨骼懒建+idle 驱动
```

成员（`_actor_avatars` 旁 :429-431）：

```cpp
    // A6-S2 批次6: NPC 骨骼缓存 (键 npc_id; 未入 map=未尝试, nullptr=失败已缓存)
    std::map<int, std::unique_ptr<SkeletonAvatar>> _npc_avatars;
```

- [ ] **Step 2: `npc_views()` 带 id**（`game_scene.cpp:3369-3375`）：

```cpp
        out.push_back({_npc_tile_x[i], _npc_tile_y[i], false, _npc_state[i].id});
```

- [ ] **Step 3: 定义 `npc_avatar` + `_npc_avatars_tick`**（贴 `_monster_avatars_tick` 后，`game_scene.cpp` ~:3019）：

```cpp
SkeletonAvatar* GameScene::npc_avatar(int npc_id) {
    auto it = _npc_avatars.find(npc_id);
    return it == _npc_avatars.end() ? nullptr : it->second.get();
}

// A6-S2 批次6: NPC 骨骼 — 白名单命中懒建一次 (成败都缓存), idle-only
void GameScene::_npc_avatars_tick() {
    if (!_actor_avatars_loaded) {
        _actor_avatars_loaded = true;
        std::string conf_err;
        auto conf = load_actor_avatars_file("resources/animations/actor_avatars.json",
                                            conf_err);
        if (conf) _actor_avatars = std::move(*conf);
        else LOG_WARN("A6: actor_avatars.json invalid, all fallback (%s)",
                      conf_err.c_str());
    }
    if (_actor_avatars.empty()) return;
    const float dt = GetFrameTime();
    for (int i = 0; i < _npc_count; i++) {
        if (_npc_state[i].finished) continue;
        const int id = _npc_state[i].id;
        auto found = _npc_avatars.find(id);
        if (found == _npc_avatars.end()) {
            const std::string key = "npc_" + std::to_string(id);
            auto it = _actor_avatars.find(key);
            std::unique_ptr<SkeletonAvatar> avatar;
            if (it != _actor_avatars.end()) {
                avatar = std::make_unique<SkeletonAvatar>();
                std::string avatar_err;
                if (avatar->try_init(it->second.skeleton, it->second.anim, avatar_err))
                    LOG_INFO("A6: npc avatar active (%s)", key.c_str());
                else {
                    LOG_WARN("A6: npc avatar inactive (%s): %s",
                             key.c_str(), avatar_err.c_str());
                    avatar.reset();
                }
            }
            found = _npc_avatars.emplace(id, std::move(avatar)).first;
        }
        auto* avatar = found->second.get();
        if (!avatar || !avatar->active()) continue;
        avatar->advance(dt, AnimInput{});   // NPC 静止 → 默认全 false = idle
    }
}
```

注意：`game_scene.cpp` 需能见 `SkeletonAvatar` 完整类型（`monster.cpp` 已 include，本文件若未 include 则加 `#include "game/animation/skeleton_avatar.h"`——以编译为准）。`_monster_avatars_tick` 原样不动；白名单懒载逻辑允许与 mon **重复**（都写同一 `_actor_avatars_loaded`，后到者跳过）——若 Step 1 发现 mon 已保证加载，可只在 `_npc_avatars_tick` 开头依赖 mon 先跑；**稳妥做法：tick 内自带懒载**（如上）。

- [ ] **Step 4: 两处调用点**（`game_scene.cpp:2440` 与 `:2450` 各 +1 行）：

```cpp
            _monster_avatars_tick();
            _npc_avatars_tick();
```

```cpp
    _monster_avatars_tick();
    _npc_avatars_tick();
```

- [ ] **Step 5: 2D NPC 绘制分支**（`game_scene.cpp` ~:3112-3124，在取 `stex` 前插入判断；空手静态回落原样）：

```cpp
        float s = TILE_SIZE - 4;
        float sx = nx - s/2, sy = ny - s/2;
        SkeletonAvatar* npc_sk = npc_avatar(_npc_state[i].id);
        if (npc_sk && npc_sk->active()) {
            npc_sk->draw_at({nx, ny + s * 0.5f}, 1.f, 255);
        } else {
            SpriteDef sdef;
            Texture2D stex = ResourceManager::inst().sprite_by_key(
                npc_sprite_key(current_floor), sdef);
            if (stex.id > 0) {
                SpriteRenderer::draw_sprite(stex, sdef, 0, {sx, sy, s, s});
            } else {
                float pulse = 4 + sinf((float)GetTime() * 4) * 2;
                DrawCircle(nx, ny - 10, pulse, {100, 220, 140, 180});
                DrawCircle(nx, ny - 10, 3, {60, 180, 80, 255});
            }
        }
```

（替换原「取 sdef/stex → 画/绿点」块；名字标签与 E 提示循环体后续 **不动**。feet=`ny + s*0.5f` 与旧 sprite 底边对齐；实机若悬空/陷地再微调 k。）

- [ ] **Step 6: HD2D `_build_npcs` 骨骼分支**（`hd2d_scene_builder.cpp:974-994`）：

```cpp
static void _build_npcs(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    auto& res = ResourceManager::inst();
    const char* skey = npc_sprite_key(gs.current_floor);
    for (const auto& npc : gs.npc_views()) {
        if (gs.game_map && !gs.game_map->isVisible(npc.tile_x, npc.tile_y)) continue;
        const float wx = (float)npc.tile_x * TILE_SIZE + TILE_SIZE * 0.5f;
        const float wz = (float)npc.tile_y * TILE_SIZE + TILE_SIZE * 0.5f;
        if (auto* skav = gs.npc_avatar(npc.npc_id); skav && skav->active()) {
            const auto parts = skav->part_draws({}, false);
            if (!parts.empty()) {
                appendAvatarParts(parts, {wx, 0, wz},
                                  (float)npc.tile_y * TILE_SIZE, 255, 0.f, out);
                continue;
            }
        }
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        item.world_pos = {wx, 0, wz};
        item.size = 34.0f;
        item.sort_y = (float)npc.tile_y * TILE_SIZE;
        item.outline = true;
        SpriteDef def;
        item.texture = res.sprite_by_key(skey, def);
        if (item.texture.id <= 0) continue;
        item.tex_src = SpriteRenderer::frame_rect(def, 0);
        item.tint = WHITE;
        out.push_back(item);
    }
}
```

（`appendAvatarParts` 本文件已用；`npc_avatar` 经 `gs` 调用。`part_draws({}, false)` 与 mon 无翻转 NPC 默认朝向一致。）

- [ ] **Step 7: 构建**

Run: `cmake --build build`
Expected: 无 error；无新 warning 于触及文件。

- [ ] **Step 8: 全量测试 + validator**

Run: `cd build; ctest`；`conda run python tools\world_validator.py`
Expected: ctest 全过；validator 0/0。

- [ ] **Step 9: Code Review**

Run: `git diff --stat src`；核对：仅 3 个 C++ 文件；`_monster_avatars_tick` 函数体无 diff；肖像/`title_scene`/`npc_sprite_key` 无 diff；两调用点各 +1 行；函数长度均 ≤40。

### Task 5: sim A/B

**Files:**
- Temp: `av_new.json / av_old.json / sim_after6.txt / sim_before6.txt`（`C:\Users\HP\AppData\Local\Temp\opencode\`）

**Interfaces:**
- Consumes: Task 3-4 完成后的可执行文件；HEAD（`6fcdccd`）白名单 30 键。
- 基线：批次5 after sha `b6814bdd4b44bf15e92153db5c4876552ea1d9f04ade82b68a724a155cc17d06`。

- [ ] **Step 1: 跑“新”侧**

Run: `Copy-Item resources\animations\actor_avatars.json C:\Users\HP\AppData\Local\Temp\opencode\av_new.json -Force; .\build\roguelike_cpp.exe --sim 12 --sim-seed 3 2>&1 | Out-File -Encoding utf8 C:\Users\HP\AppData\Local\Temp\opencode\sim_after6.txt`
Expected: sim 正常退出。

- [ ] **Step 2: 换旧白名单跑“前”侧并还原**

Run: `git show HEAD:resources/animations/actor_avatars.json > C:\Users\HP\AppData\Local\Temp\opencode\av_old.json; Copy-Item C:\Users\HP\AppData\Local\Temp\opencode\av_old.json resources\animations\actor_avatars.json -Force; .\build\roguelike_cpp.exe --sim 12 --sim-seed 3 2>&1 | Out-File -Encoding utf8 C:\Users\HP\AppData\Local\Temp\opencode\sim_before6.txt; Copy-Item C:\Users\HP\AppData\Local\Temp\opencode\av_new.json resources\animations\actor_avatars.json -Force`
Expected: 还原后白名单 40 键（临时脚本断言）。

注意：`git show ... > file` 在 PS 可能 UTF-16——应用 `Out-File -Encoding utf8` 包一层，或 `git show ... | Out-File -Encoding utf8 av_old.json`。

- [ ] **Step 3: 逐字节比较 + 与批次5 基线 sha**

Run: 比较 after6 vs before6；对 after6 算 sha256。
Expected（主）: `SIM BYTE-IDENTICAL` 且 sha == `b6814bdd…17d06`——无头不触 NPC 渲染/白名单 sim 路径。
Expected（降级）: 若差异 → 同 seed 连跑两次 after6 同 sha + validator 0/0 + 无崩溃，CHANGELOG 如实记录；**不得**回退 Task 3-4 为凑一致。

### Task 6: CHANGELOG + 桌面同步

**Files:**
- Modify: `README.md`（CHANGELOG 批次5 条目后）
- Mirror → `C:\Users\HP\Desktop\Roguelike-CPP-3D版`

**Interfaces:**
- Consumes: Task 1-5 全绿。

- [ ] **Step 1: CHANGELOG 追加批次6 条目**（Python UTF-8 插行，批次5 条目整行之后）：

```markdown
- A6-S2 批次6（开发版，未发布）：10 世界 NPC 骨骼化——新建 `tools/gen_npc_parts.py`（复用 humanoid rig，10 色板；空手 weapon 全透明；键 `npc_20`…`npc_140`），`sprites.json` skeleton_parts +50（总 205），白名单 30→40；C++ 接线：`NpcView.npc_id` + `_npc_avatars_tick` + 2D/HD2D 骨骼优先静态图回落。对话肖像与标题页仍用旧 `npc_*.png`；旧 30 mon 分件零 diff。68 项 CTest、World Validator 0/0 通过；sim A/B 见 Task5 实测记录。实机验收待完成。后续批次：Boss、lightning_orb 运行时接入。
```

- [ ] **Step 2: 同步桌面包目录**（workdir=项目根；exit 0/1 均成功）：

```powershell
$dst = "C:\Users\HP\Desktop\Roguelike-CPP-3D版"
foreach ($d in @('src','resources','tools','tests','docs','assets','.github','vendor')) {
  robocopy "$d" "$dst\$d" /MIR /NFL /NDL /NJH /NJS /NP | Out-Null; Write-Host "$d -> $LASTEXITCODE" }
foreach ($f in @('CMakeLists.txt','README.md','CLAUDE.md','CMakePresets.json','.gitignore')) {
  Copy-Item $f "$dst\$f" -Force }
Copy-Item build\roguelike_cpp.exe "$dst\roguelike_cpp.exe" -Force
Copy-Item build\raylib.dll "$dst\raylib.dll" -Force
```

- [ ] **Step 3: 桌面侧断言**

Run: `npc_20_part_torso.png` 存在、白名单 `npc_140` 计数=40、`saves\` 与 `3D模式.exe.lnk` 仍在、README.md 新时间戳、根目录 `roguelike_cpp.exe` 更新。
Expected: 全部 True/40。

### Task 7: 实机验收 + 提交（用户门）

**Files:**
- Modify: `README.md`（验收句）
- Commit: 全批单次提交

**Interfaces:**
- Consumes: Task 1-6 全绿 + 用户实机反馈。

- [ ] **Step 1: 向用户报告验收点**

报告：F2 空手囚犯埃德加、F3 持刃猎人瑞卡、F7 持杖祭司泰伦斯、F11 幽灵（浅灰青）、F14 持 cleaver 守望者；2D 与 HD2D 各看一眼；**对话肖像仍是旧静态图**；名字/E 提示正常；脚不悬空/不陷地。

- [ ] **Step 2: 等待用户实机确认**（阻塞门；有问题 → 截图迭代美术/feet 锚点/注册）

- [ ] **Step 3: 更新 CHANGELOG 验收句** 并 Copy-Item README.md 到桌面包根目录。

- [ ] **Step 4: Code Review + 提交**

Run: `git status --short`（src 三 C++ 文件有 diff；JSON/测试/README + 新生成器 + 60 资源文件）、`git diff --stat`、`git log --oneline -3`。
Run: `git add -A; git commit -m "feat(a6-s2): 批次6 - 10 NPC骨骼分件与2D/HD2D接入"; git log --oneline -2; git status --short`
Expected: 约 70+ 文件入库，工作区干净（`reports/` 仍 ignore）。

---

## Self-Review

1. **Spec coverage**：§1 生成器/空手/注册/零diff → Task1-3；§2 NpcView/tick/2D/HD2D/不改清单 → Task4；§3 测试40/门禁/sim/CHANGELOG/桌面/实机/单commit → Task3-7。DoD 全覆盖。
2. **Placeholder**：无 TBD；生成器全文、JSON 块、C++ 块、命令、CHANGELOG 文案均为实值；sheet 路径 `reports/npc_sheet.png` 与 CLI 默认一致。
3. **Type/命名一致性**：`npc_id` 与 `NPCState.id`/`npc_{id}_part_*`/白名单键一致；155→205、30→40、60 新文件、10 行 wrote 一致；`npc_avatar(int)` 与 `NpcView.npc_id`/`_npc_avatars` 键一致；`appendAvatarParts` 签名与 mon 调用一致；feet 2D/HD2D 语义同 tile 中心。
4. **风险闭环**：空手 `_bbox` → part_grid 早退全 `.`；humanoid `build_palette` 绑 FAMILIES → 本地 palette；白名单懒载重复 → tick 自带懒载；PS UTF-16 → Out-File -Encoding utf8；mon 零 diff → dry-run 只写临时 + git 无 ` M`。
