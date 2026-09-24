# A6-S2 批次7：5 Boss 骨骼化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) 或 superpowers:executing-plans，按 task 逐个实现。Step 用 checkbox（`- [ ]`）跟踪。

**Goal:** 把 5 个 Boss（shadow_knight / necromancer / vampire / fire_demon / demon_lord）从旧 16×16 配色方块升级为大体型骨骼分件（5 套 1.4× rig，共 25 分件 + 5 骨架 + 1 共享 anim），零 C++ 改动接入 2D 与 HD2D 骨骼优先 / 静态图回落渲染。

**Architecture:** 新建独立生成器 `tools/gen_boss_parts.py`（表驱动 5 Boss，复用 humanoid `PART_ROWS` 九键造型并最近邻放大 1.4×，本地 palette/part_grid/skeleton_dict）+ 新建大 rig（BONES/pivot 全 1.4×）+ 新建 `boss_anim.json`（三剪辑与 player_anim 同值，torso y 改为新 bind）。渲染侧完全零改动：Boss 已进 `gs.monsters`，2D 骨骼分支与 HD2D `_build_entities` 均无 `is_boss` 排除，白名单命中即自动接管。

**Tech Stack:** Python 3（Pillow，`conda run python`）、C++17 + CMake + GoogleTest + Raylib 5.0、JSON 数据驱动资源。

## Global Constraints

- Spec：`docs/superpowers/specs/2026-09-24-a6s2-batch7-boss-skeletal-design.md`（提交 `db41cc4`，用户已批）。
- 提交策略（用户裁定，覆盖 SDD 逐 task commit 规则）：全批**单次提交** `feat(a6-s2): 批次7 - 5 Boss骨骼分件与接入`，Task 7 用户门通过后执行。Task 1-6 均不提交。
- 不修改既有生成器：`tools/gen_mon_humanoid_parts.py`、`gen_mon_soft_parts.py`、`gen_mon_float_golem_parts.py`、`gen_npc_parts.py`、`gen_player_knight_parts.py`、`m5_sprite_gen.py`。旧 30 mon 分件、10 npc 分件/骨架、41 份既有骨架、`boss_*.png` 旧整图一律**严格零 diff**。
- 不改 C++ 逻辑：`BossAI` / `BossType` / `_boss_type_for_floor` / 战斗数值 / arena / Phase2 / mirror 战斗逻辑 / `get_boss_visual_color` / 对话肖像 / `title_scene` / `dialogues.json`。本批 C++ 改动量为 **0 行**（已核实，见偏差 3）。
- 像素契约：ppu `0.8`；描边 `OUTLINE = (63, 38, 49, 255)`；九键字符集仅 `. o s m l h r R p b g`；alpha 仅 `{0, 255}`；空手武器像素行全 `.`（fire_demon）。
- 大 rig DIMS：head `30×26`、torso `40×28`、arm `16×22`、leg `32×28`、weapon `10×30`；BONES 与 PART_SLOTS pivot 全部 1.4×；`anchor [34, 87]`；9 骨 / 7 件契约。
- 注册目标：`sprites.json` skeleton_parts **205 → 230**；`actor_avatars.json` **40 → 45**；`animation_test` expected **40 → 45**。
- 门禁：`cmake --build build` 无 error、触及文件无新 warning；`ctest` **68/68**；`tools/world_validator.py` **0/0**。
- sim A/B：白名单 45 ↔ 40 两跑 `--sim 12 --sim-seed 3` 输出**逐字节一致**。
- 桌面同步：`C:\Users\HP\Desktop\Roguelike-CPP-3D版`（robocopy `/MIR` 8 目录 + 5 根文件 + build 下 exe/dll；保留 `saves\` 与 `3D模式.exe.lnk`）。
- 环境铁律：Python 一律 `conda run python`，禁 `-c` 多行内联（写临时 `.py` 到 `C:\Users\HP\AppData\Local\Temp\opencode\`）；脚本只输出 ASCII；PowerShell 写文件用 `Out-File -Encoding utf8`；`git show ... > file` 同样需包一层 utf8。
- 编码规范：函数 ≤40 行、类只做一件事、优先组合而非继承、命名语义化；每次改动前 grep 影响面、完成后 Code Review；不改 `CMakeLists.txt` 编译标志。
- 生成器 `tools/gen_boss_parts.py` **≤300 行**，`main()`/`make_sheet()`/`generate()` 等均 ≤40 行。

## 偏差记录（相对已批 spec，均由代码核实，非风格调整）

1. **不生成 5 张新整图。** spec 原写「5 张整图 `boss_<id>.png`」，但 `sprites.json:190-209` **已注册** `boss_shadow_knight` / `boss_necromancer` / `boss_vampire` / `boss_fire_demon`（`m5_sprite_gen.py` 产物，`assets/sprites/boss_*.png`，16×16）。重新生成会破坏零 diff 且被下次 `m5_sprite_gen.py` 覆盖。`demon_lord` 无整图键 → `boss.cpp:1215-1220` 走 `boss_self`（`player_fire.png` 玩家剪影，F15 镜像有意设计）。→ 白名单 5 键 = `boss_shadow_knight` / `boss_necromancer` / `boss_vampire` / `boss_fire_demon` / **`boss_self`**。
2. **新增 `resources/animations/boss_anim.json`**，不直接复用 `player_anim.json`。原因：anim key 值是**绝对 local**（`animation_defs.cpp:101-102` 仅在字段缺失时回落 bind；`skeleton_pose.cpp:20-22` `key_to_local` 不做加法）。`torso` track 显式写 `y: 0/1.2/1.5`，在 1.4× rig（bind y=8）上会把胸干塌 8 unit 且 attack 时回弹。新文件仅改 torso 的 y（8 / 9.2 / 9.5），其余逐字节同 `player_anim.json`。
3. **C++ 改动 0 行**（spec 原写「预期 0-2 行」）。已核实三处：`game_scene_input.cpp:133` `_s.monsters.emplace_back(boss)`；`monster.cpp:266-271` 骨骼分支无 `is_boss` 排除；`hd2d_scene_builder.cpp:459-469` `_build_entities` 遍历 `gs.monsters` 无 `is_boss` 排除。Task 4 改为纯验证。

---

### Task 1: 生成器 `tools/gen_boss_parts.py` + 大 rig + `boss_anim.json` + dry-run

**Files:**
- Create: `tools/gen_boss_parts.py`
- Create: `resources/animations/boss_anim.json`
- Temp: `C:\Users\HP\AppData\Local\Temp\opencode\b7dry\`（`sprites\`、`anim\`）
- Create: `reports/boss_sheet.png`（`.gitignore` 内，不进 git）

**Interfaces:**
- Produces：5 Boss × 5 分件（`boss_<id>_part_{head,torso,arm,leg,weapon}.png`）+ 5 骨架（`boss_<id>_skeleton.json`，`file` 字段为 `assets/sprites/boss_<id>_part_<name>.png`）+ 1 共享 `boss_anim.json`；CLI `--output-dir --skeleton-dir --sheet`。Task 2-3 依赖这些精确文件名与 DIMS。
- Consumes：`gen_mon_humanoid_parts.OUTLINE / PART_ROWS / WEAPONS`；`anim_preview.checkerboard / render_pose`。

- [ ] **Step 1: 写 `tools/gen_boss_parts.py`**（整文件如下，≤300 行）：

```python
# A6-S2 批次7: Boss 骨骼分件生成器 (5 Boss / 大 rig 1.4x / 独立骨架)
# 产出 boss_<id>_part_*.png + boss_<id>_skeleton.json; 共享 boss_anim.json
import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw

from anim_preview import checkerboard, render_pose
from gen_mon_humanoid_parts import OUTLINE, PART_ROWS, WEAPONS

ROOT = Path(__file__).resolve().parents[1]

SIZES = {"head": (30, 26), "torso": (40, 28), "arm": (16, 22),
         "leg": (32, 28), "weapon": (10, 30)}

BONES = [
    {"name": "root"},
    {"name": "hips", "parent": "root", "y": 28},
    {"name": "torso", "parent": "hips", "y": 8},
    {"name": "head", "parent": "torso", "y": 15},
    {"name": "arm_back", "parent": "torso", "x": -4, "y": 13},
    {"name": "weapon", "parent": "arm_back", "y": -7},
    {"name": "arm_front", "parent": "torso", "x": 6, "y": 13},
    {"name": "leg_front", "parent": "hips", "x": 4},
    {"name": "leg_back", "parent": "hips", "x": -4},
]

PART_SLOTS = [("leg_back", "leg", [8, 28]), ("arm_back", "arm", [6, 22]),
              ("weapon", "weapon", [4, 30]), ("torso", "torso", [18, 14]),
              ("head", "head", [15, 10]), ("arm_front", "arm", [6, 22]),
              ("leg_front", "leg", [8, 28])]

PPU = 0.8
ANCHOR = [34, 87]

PALETTES = {
    "shadow_knight": {"s": (34, 28, 48), "m": (58, 48, 80), "l": (92, 78, 122),
                      "h": (136, 120, 168), "r": (70, 52, 110),
                      "R": (104, 84, 156), "p": (150, 132, 190),
                      "b": (18, 14, 30), "g": (96, 88, 70)},
    "necro_boss": {"s": (36, 52, 42), "m": (62, 86, 66), "l": (96, 128, 98),
                   "h": (150, 182, 140), "r": (96, 72, 50),
                   "R": (136, 104, 70), "p": (214, 220, 190),
                   "b": (24, 30, 26), "g": (96, 88, 70)},
    "vampire_boss": {"s": (52, 18, 28), "m": (96, 30, 44), "l": (148, 48, 66),
                     "h": (206, 84, 104), "r": (124, 40, 52),
                     "R": (186, 66, 80), "p": (240, 214, 160),
                     "b": (34, 14, 22), "g": (110, 80, 64)},
    "fire_demon": {"s": (58, 26, 20), "m": (112, 52, 28), "l": (178, 84, 34),
                   "h": (236, 140, 52), "r": (150, 58, 26),
                   "R": (226, 92, 32), "p": (252, 214, 120),
                   "b": (40, 16, 12), "g": (150, 110, 70)},
    "mirror_void": {"s": (30, 26, 52), "m": (54, 46, 88), "l": (84, 72, 132),
                    "h": (120, 106, 176), "r": (64, 54, 96),
                    "R": (96, 84, 144), "p": (168, 152, 214),
                    "b": (16, 12, 28), "g": (96, 88, 70)},
}

RAPIER = "|".join(["...ohhl..."] * 26 + ["..oohhho..", "....go....", "....go....", "...oggo..."])

MIRROR_BLADE_TAIL = ["...oRRo...", ".oohhhhoo.", "....go....", "....go....", "....go....", "...oggo..."]
MIRROR_BLADE = "|".join(["...ohho..."] * 3 + ["..oRppho.."] * 21 + MIRROR_BLADE_TAIL)

WEAPONS_BOSS = dict(WEAPONS)
WEAPONS_BOSS["rapier"] = RAPIER.split("|")
WEAPONS_BOSS["mirror_blade"] = MIRROR_BLADE.split("|")

BOSS = {
    "shadow_knight": ("shadow_knight", "greatsword"),
    "necromancer": ("necro_boss", "staff"),
    "vampire": ("vampire_boss", "rapier"),
    "fire_demon": ("fire_demon", None),
    "demon_lord": ("mirror_void", "mirror_blade"),
}
assert len(BOSS) == 5, "BOSS must have exactly 5 entries"

DECOR = {
    ("shadow_knight", "head"): [(1, 3, 12, 18, "R")],
    ("shadow_knight", "torso"): [(2, 3, 10, 29, "h"), (4, 6, 2, 7, "l"),
                                 (4, 6, 32, 37, "l")],
    ("shadow_knight", "arm"): [(3, 6, 5, 11, "l")],
    ("necromancer", "head"): [(1, 3, 8, 21, "m")],
    ("necromancer", "torso"): [(1, 2, 12, 27, "R"), (22, 26, 4, 35, "s")],
    ("vampire", "head"): [(7, 10, 10, 19, "p")],
    ("vampire", "torso"): [(0, 2, 8, 31, "R"), (4, 7, 3, 8, "r")],
    ("fire_demon", "head"): [(0, 2, 9, 20, "R"), (1, 1, 13, 16, "p")],
    ("fire_demon", "torso"): [(6, 8, 10, 17, "R"), (13, 15, 19, 30, "R"),
                              (19, 20, 13, 23, "r")],
    ("demon_lord", "head"): [(8, 9, 9, 20, "h")],
    ("demon_lord", "torso"): [(6, 7, 6, 33, "p"), (14, 15, 10, 29, "R")],
}


def build_palette(palette_key):
    palette = {".": (0, 0, 0, 0), "o": OUTLINE}
    for char, rgb in PALETTES[palette_key].items():
        palette[char] = (*rgb, 255)
    return palette


def upscale(rows, width, height):
    src_w, src_h = len(rows[0]), len(rows)
    fx, fy = width / src_w, height / src_h
    out = [["." for _ in range(width)] for _ in range(height)]
    for oy in range(height):
        row = rows[min(src_h - 1, int(round(oy / fy)))]
        for ox in range(width):
            out[oy][ox] = row[min(src_w - 1, int(round(ox / fx)))]
    return out


def fill_rect(rows, y0, y1, x0, x1, char):
    for y in range(max(0, y0), min(len(rows) - 1, y1) + 1):
        for x in range(max(0, x0), min(len(rows[0]) - 1, x1) + 1):
            rows[y][x] = char


def part_rows(boss_id, part):
    weapon = BOSS[boss_id][1]
    if part == "weapon":
        if weapon is None:
            width, height = SIZES["weapon"]
            return ["." * width for _ in range(height)]
        rows = WEAPONS_BOSS[weapon]
    else:
        rows = PART_ROWS[part]
    width, height = SIZES[part]
    rows = upscale(list(rows), width, height)
    for y0, y1, x0, x1, char in DECOR.get((boss_id, part), []):
        fill_rect(rows, y0, y1, x0, x1, char)
    return rows


def part_image(boss_id, part):
    palette = build_palette(BOSS[boss_id][0])
    rows = part_rows(boss_id, part)
    width, height = SIZES[part]
    if len(rows) != height or any(len(r) != width for r in rows):
        raise ValueError(f"Invalid pixel grid: {boss_id} {part}")
    image = Image.new("RGBA", (width, height))
    image.putdata([palette[pixel] for row in rows for pixel in row])
    if not {pixel[3] for pixel in image.getdata()} <= {0, 255}:
        raise ValueError(f"Non-binary alpha: {boss_id} {part}")
    return image


def skeleton_dict(boss_id):
    parts = [{"bone": bone,
              "file": f"assets/sprites/boss_{boss_id}_part_{name}.png",
              "pivot": list(pivot)} for bone, name, pivot in PART_SLOTS]
    return {"pixels_per_unit": PPU, "anchor": list(ANCHOR),
            "bones": BONES, "parts": parts}


def generate(output_dir, skeleton_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    skeleton_dir.mkdir(parents=True, exist_ok=True)
    for boss_id in BOSS:
        images = {name: part_image(boss_id, name) for name in SIZES}
        for name, image in images.items():
            image.save(output_dir / f"boss_{boss_id}_part_{name}.png")
        path = skeleton_dir / f"boss_{boss_id}_skeleton.json"
        path.write_text(json.dumps(skeleton_dict(boss_id), indent=2) + "\n",
                        encoding="utf-8")
        print(f"wrote boss_{boss_id} parts+skeleton")
    return {bid: {name: part_image(bid, name) for name in SIZES}
            for bid in BOSS}


def make_sheet(images):
    row_h, part_scale, canvas_w = 150, 3, 760
    sheet = checkerboard((canvas_w, row_h * len(BOSS) + 20))
    draw = ImageDraw.Draw(sheet)
    for index, boss_id in enumerate(BOSS):
        palette_key, weapon = BOSS[boss_id]
        y = index * row_h + 18
        draw.text((6, y - 12), boss_id, fill=(205, 220, 222))
        label = "empty" if weapon is None else weapon
        draw.text((6, y + 2), f"{palette_key} ppu={PPU} {label}",
                  fill=(149, 166, 183))
        x = 150
        for name in ("head", "torso", "arm", "leg", "weapon"):
            piece = images[boss_id][name]
            enlarged = piece.resize((piece.width * part_scale,
                                     piece.height * part_scale),
                                    Image.Resampling.NEAREST)
            sheet.alpha_composite(enlarged, (x, y - 8))
            x += enlarged.width + 12
        pieces = [images[boss_id][name] for _, name, _ in PART_SLOTS]
        pose = render_pose(skeleton_dict(boss_id), pieces, None, 0, 1.2)
        sheet.alpha_composite(pose, (600, y - 26))
    return sheet


def main():
    parser = argparse.ArgumentParser(description="Generate boss_* skeleton parts.")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "assets" / "sprites")
    parser.add_argument("--skeleton-dir", type=Path,
                        default=ROOT / "resources" / "animations")
    parser.add_argument("--sheet", type=Path, default=ROOT / "reports" / "boss_sheet.png")
    options = parser.parse_args()
    images = generate(options.output_dir, options.skeleton_dir)
    options.sheet.parent.mkdir(parents=True, exist_ok=True)
    make_sheet(images).save(options.sheet)
    print(f"sheet -> {options.sheet}")


if __name__ == "__main__":
    main()
```

注意三处易错点，写盘前逐项确认：

- `RAPIER` 与 `MIRROR_BLADE` 每行**必须 10 字符**：`"...ohhl..."`/`"...ohho..."`（3+4+3）、`"..oRppho.."`/`"..oohhho.."`（2+6+2）、`"...oRRo..."`/`"...oggo..."`（3+4+3）、`"....go...."`（4+2+4）、`".oohhhhoo."`（1+8+1）。`split("|")` 必须 **30 行**：RAPIER = 26 blade + 1 guard + 2 handle + 1 pommel；MIRROR_BLADE = 3 tip + 21 blade + 1 mid + 1 guard + 3 handle + 1 pommel。`part_rows` 对 weapon 仍走 `upscale(rows, 10, 30)`（`fx = fy = 1.0` 恒等，不缩放）——所以必须按目标尺寸直书；knight 的 `greatsword`/`staff` 是 6×24 网格，由 `upscale` 放大到 10×30。
- 两把新武器行模板的 `|` 分隔**不可省略**（`"|".join([...])`）：写成纯字符串拼接会让 `split("|")` 只剩 1 行、宽 300+ 字符，`part_image` 的 `Invalid pixel grid` 断言会立刻炸。行数分解：RAPIER = 26 blade + 1 guard + 2 handle + 1 pommel；MIRROR_BLADE = 3 tip + 21 blade + 1 mid + 1 guard + 3 handle + 1 pommel。
- 五处 id 书写须统一为 `demon_lord`（BOSS / DECOR 键 / 文件名 / 骨架键），写盘后自查一遍：`grep -c demon_lard` 应为 0。

- [ ] **Step 2: 写 `resources/animations/boss_anim.json`**（`torso` 的 y 改为新 bind 8/9.2/9.5，其余逐字节同 `player_anim.json`）：

```json
{
  "animations": {
    "idle": {
      "loop": true, "dur": 2.4,
      "tracks": [
        { "bone": "torso", "keys": [
          { "t": 0.0, "rot": 0, "y": 8 },
          { "t": 1.2, "rot": 2, "y": 9.2 },
          { "t": 2.4, "rot": 0, "y": 8 } ] },
        { "bone": "head", "keys": [
          { "t": 0.0, "rot": 0 },
          { "t": 1.2, "rot": -2 },
          { "t": 2.4, "rot": 0 } ] }
      ]
    },
    "walk": {
      "loop": true, "dur": 0.7,
      "tracks": [
        { "bone": "leg_front", "keys": [
          { "t": 0.0, "rot": -22 }, { "t": 0.35, "rot": 22 }, { "t": 0.7, "rot": -22 } ] },
        { "bone": "leg_back", "keys": [
          { "t": 0.0, "rot": 22 }, { "t": 0.35, "rot": -22 }, { "t": 0.7, "rot": 22 } ] },
        { "bone": "arm_front", "keys": [
          { "t": 0.0, "rot": 14 }, { "t": 0.35, "rot": -14 }, { "t": 0.7, "rot": 14 } ] },
        { "bone": "arm_back", "keys": [
          { "t": 0.0, "rot": -14 }, { "t": 0.35, "rot": 14 }, { "t": 0.7, "rot": -14 } ] },
        { "bone": "weapon", "keys": [
          { "t": 0.0, "rot": -14 }, { "t": 0.35, "rot": 14 }, { "t": 0.7, "rot": -14 } ] },
        { "bone": "torso", "keys": [
          { "t": 0.0, "y": 8 }, { "t": 0.175, "y": 9.5 }, { "t": 0.35, "y": 8 },
          { "t": 0.525, "y": 9.5 }, { "t": 0.7, "y": 8 } ] }
      ]
    },
    "attack": {
      "loop": false, "dur": 0.36,
      "tracks": [
        { "bone": "arm_back", "keys": [
          { "t": 0.0, "rot": 0 }, { "t": 0.1, "rot": -45 },
          { "t": 0.2, "rot": 80 }, { "t": 0.36, "rot": 10 } ] },
        { "bone": "weapon", "keys": [
          { "t": 0.0, "rot": 0 }, { "t": 0.1, "rot": -20 },
          { "t": 0.2, "rot": 40 }, { "t": 0.36, "rot": 0 } ] },
        { "bone": "torso", "keys": [
          { "t": 0.0, "rot": 0 }, { "t": 0.1, "rot": -4 },
          { "t": 0.2, "rot": 6 }, { "t": 0.36, "rot": 0 } ] }
      ]
    },
    "hit": {
      "loop": false, "dur": 0.18,
      "tracks": [
        { "bone": "torso", "keys": [
          { "t": 0.0, "rot": 0 }, { "t": 0.06, "rot": -8 }, { "t": 0.18, "rot": 0 } ] },
        { "bone": "head", "keys": [
          { "t": 0.0, "rot": 0 }, { "t": 0.06, "rot": -12 }, { "t": 0.18, "rot": 0 } ] }
      ]
    }
  }
}
```

`attack` / `hit` 的 `torso` track 只写 `rot` 不写 `y` → 解析器回落 bind `y=8`（`animation_defs.cpp:102`），正好与 idle/walk 的起始 y 连续，无回弹。

- [ ] **Step 3: dry-run 到临时目录（不碰仓库资产）**

Run: `conda run python tools\gen_boss_parts.py --output-dir C:\Users\HP\AppData\Local\Temp\opencode\b7dry\sprites --skeleton-dir C:\Users\HP\AppData\Local\Temp\opencode\b7dry\anim --sheet C:\Users\HP\AppData\Local\Temp\opencode\b7dry\sheet.png`

Expected: 5 行 `wrote boss_<id> parts+skeleton` + `sheet -> ...b7dry\sheet.png`；无 `ValueError`（即无 `Invalid pixel grid` / `Non-binary alpha`）。

- [ ] **Step 4: 静态自检 + 用户目检 sheet**

Run（写临时 `.py` 到 `C:\Users\HP\AppData\Local\Temp\opencode\b7check.py` 后 `conda run python`，只输出 ASCII）：断言 25 个 PNG 尺寸 == `SIZES`；5 个骨架 JSON `pixels_per_unit == 0.8`、`len(bones) == 9`、`len(parts) == 7`、`anchor == [34, 87]`、`file` 全部匹配 `assets/sprites/boss_<id>_part_<name>.png`；`boss_anim.json` 可 `json.load` 且 clip 名集合 == `{idle, walk, attack, hit}`；`boss_demon_lord` 的 weapon 分件 100% 透明像素。

同时 `git status --short` 必须**无** `assets/sprites/mon_*` / `npc_*` / `resources/animations/mon_*` / `npc_*` 的 `M`（只有本批新文件 `??`）。

Expected: 全部断言通过。把 `b7dry\sheet.png` 交用户目检 5 个 Boss 造型与 pose；有造型问题只改 `DECOR` / `PALETTES` 后重跑 Step 3-4，不动 rig 与 pivot。

### Task 2: 生成正式资源 + 零 diff 门

**Files:**
- Create: `assets/sprites/boss_<id>_part_{head,torso,arm,leg,weapon}.png` × 25
- Create: `resources/animations/boss_<id>_skeleton.json` × 5

**Interfaces:**
- Consumes: Task 1 的生成器（已 dry-run 通过 + 用户目检 sheet 通过）。
- Produces: 与骨架 `file` 字段一一对应的 25 张 PNG + 5 份骨架 JSON。

- [ ] **Step 1: 正式生成（默认路径）**

Run: `conda run python tools\gen_boss_parts.py`

Expected: 5 行 `wrote boss_<id> parts+skeleton` + `sheet -> reports\boss_sheet.png`。

- [ ] **Step 2: 资源自检**（写临时 `.py` 到 `C:\Users\HP\AppData\Local\Temp\opencode\b7verify.py`，只输出 ASCII）：

```python
import json
from pathlib import Path
from PIL import Image

root = Path(r"C:\Demo\roguelike_cpp")
sizes = {"head": (30, 26), "torso": (40, 28), "arm": (16, 22),
         "leg": (32, 28), "weapon": (10, 30)}
bones = ["root", "hips", "torso", "head", "arm_back", "weapon",
         "arm_front", "leg_front", "leg_back"]
ids = ["shadow_knight", "necromancer", "vampire", "fire_demon", "demon_lord"]
count = 0
for boss_id in ids:
    for part, dim in sizes.items():
        png = root / "assets" / "sprites" / f"boss_{boss_id}_part_{part}.png"
        image = Image.open(png)
        assert image.size == dim, (png.name, image.size)
        assert set(p[3] for p in image.getdata()) <= {0, 255}, png.name
        count += 1
    with open(root / "resources" / "animations" / f"boss_{boss_id}_skeleton.json",
              encoding="utf-8") as fh:
        skel = json.load(fh)
    assert skel["pixels_per_unit"] == 0.8, boss_id
    assert skel["anchor"] == [34, 87], boss_id
    assert [b["name"] for b in skel["bones"]] == bones, boss_id
    assert len(skel["parts"]) == 7, boss_id
    for part in skel["parts"]:
        stem = part["file"].rsplit("_part_", 1)[1][:-4]
        assert (root / part["file"]).exists(), part["file"]
print("ok", count, len(ids))
```

Expected: `ok 25 5`。

- [ ] **Step 3: 零 diff 门**

Run: `git status --short`

Expected: 仅 `tools/gen_boss_parts.py`、`resources/animations/boss_anim.json`、`resources/animations/boss_*_skeleton.json`（5 个）为 `??`；`assets/sprites/` 仅新增 25 个 `boss_*` PNG；`reports/` 不出现；**无任何** `M`（尤其 `mon_*` / `npc_*` / 既有生成器 / `m5_sprite_gen.py` / `boss_f5.png` / `boss_*.png` 旧整图）。

### Task 3: 注册（25 分件 / 5 白名单 / 测试期望）+ 门禁

**Files:**
- Modify: `resources/sprites.json:1487-1491`（skeleton_parts 末尾，205 → 230）
- Modify: `resources/animations/actor_avatars.json:156-159`（白名单末尾，40 → 45）
- Modify: `tests/animation/animation_test.cpp:681-711`（expected 40 → 45 + skeleton/anim 解析门禁）

**Interfaces:**
- Consumes: Task 2 的 25 PNG + 5 骨架；`boss_anim.json`（Task 1）。
- Produces: 白名单 5 键 `boss_shadow_knight` / `boss_necromancer` / `boss_vampire` / `boss_fire_demon` / `boss_self`（后者对应 `demon_lord` 骨架，键名沿用回落链的 `sprite_override = "boss_self"`）。

- [ ] **Step 1: `sprites.json` 插入 25 键**

以 `"npc_140_part_weapon"` 块的结尾 `"h": 24\n    }` 为锚点，替换为下方整块（**追加 25 个键后补回逗号**）。键顺序 = Boss 出场顺序 × 部件顺序（torso/head/arm/leg/weapon），与 `npc_140_*` 既有排列一致：

```json
      "h": 24
    },
    "boss_shadow_knight_part_torso": {
      "file": "assets/sprites/boss_shadow_knight_part_torso.png",
      "w": 40,
      "h": 28
    },
    "boss_shadow_knight_part_head": {
      "file": "assets/sprites/boss_shadow_knight_part_head.png",
      "w": 30,
      "h": 26
    },
    "boss_shadow_knight_part_arm": {
      "file": "assets/sprites/boss_shadow_knight_part_arm.png",
      "w": 16,
      "h": 22
    },
    "boss_shadow_knight_part_leg": {
      "file": "assets/sprites/boss_shadow_knight_part_leg.png",
      "w": 32,
      "h": 28
    },
    "boss_shadow_knight_part_weapon": {
      "file": "assets/sprites/boss_shadow_knight_part_weapon.png",
      "w": 10,
      "h": 30
    },
    "boss_necromancer_part_torso": {
      "file": "assets/sprites/boss_necromancer_part_torso.png",
      "w": 40,
      "h": 28
    },
    "boss_necromancer_part_head": {
      "file": "assets/sprites/boss_necromancer_part_head.png",
      "w": 30,
      "h": 26
    },
    "boss_necromancer_part_arm": {
      "file": "assets/sprites/boss_necromancer_part_arm.png",
      "w": 16,
      "h": 22
    },
    "boss_necromancer_part_leg": {
      "file": "assets/sprites/boss_necromancer_part_leg.png",
      "w": 32,
      "h": 28
    },
    "boss_necromancer_part_weapon": {
      "file": "assets/sprites/boss_necromancer_part_weapon.png",
      "w": 10,
      "h": 30
    },
    "boss_vampire_part_torso": {
      "file": "assets/sprites/boss_vampire_part_torso.png",
      "w": 40,
      "h": 28
    },
    "boss_vampire_part_head": {
      "file": "assets/sprites/boss_vampire_part_head.png",
      "w": 30,
      "h": 26
    },
    "boss_vampire_part_arm": {
      "file": "assets/sprites/boss_vampire_part_arm.png",
      "w": 16,
      "h": 22
    },
    "boss_vampire_part_leg": {
      "file": "assets/sprites/boss_vampire_part_leg.png",
      "w": 32,
      "h": 28
    },
    "boss_vampire_part_weapon": {
      "file": "assets/sprites/boss_vampire_part_weapon.png",
      "w": 10,
      "h": 30
    },
    "boss_fire_demon_part_torso": {
      "file": "assets/sprites/boss_fire_demon_part_torso.png",
      "w": 40,
      "h": 28
    },
    "boss_fire_demon_part_head": {
      "file": "assets/sprites/boss_fire_demon_part_head.png",
      "w": 30,
      "h": 26
    },
    "boss_fire_demon_part_arm": {
      "file": "assets/sprites/boss_fire_demon_part_arm.png",
      "w": 16,
      "h": 22
    },
    "boss_fire_demon_part_leg": {
      "file": "assets/sprites/boss_fire_demon_part_leg.png",
      "w": 32,
      "h": 28
    },
    "boss_fire_demon_part_weapon": {
      "file": "assets/sprites/boss_fire_demon_part_weapon.png",
      "w": 10,
      "h": 30
    },
    "boss_demon_lord_part_torso": {
      "file": "assets/sprites/boss_demon_lord_part_torso.png",
      "w": 40,
      "h": 28
    },
    "boss_demon_lord_part_head": {
      "file": "assets/sprites/boss_demon_lord_part_head.png",
      "w": 30,
      "h": 26
    },
    "boss_demon_lord_part_arm": {
      "file": "assets/sprites/boss_demon_lord_part_arm.png",
      "w": 16,
      "h": 22
    },
    "boss_demon_lord_part_leg": {
      "file": "assets/sprites/boss_demon_lord_part_leg.png",
      "w": 32,
      "h": 28
    },
    "boss_demon_lord_part_weapon": {
      "file": "assets/sprites/boss_demon_lord_part_weapon.png",
      "w": 10,
      "h": 30
    }
```

- [ ] **Step 2: `actor_avatars.json` 追加 5 键**

以 `npc_140` 块结尾为锚点，替换为：

```json
    "npc_140": {
      "skeleton": "resources/animations/npc_140_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "boss_shadow_knight": {
      "skeleton": "resources/animations/boss_shadow_knight_skeleton.json",
      "anim": "resources/animations/boss_anim.json"
    },
    "boss_necromancer": {
      "skeleton": "resources/animations/boss_necromancer_skeleton.json",
      "anim": "resources/animations/boss_anim.json"
    },
    "boss_vampire": {
      "skeleton": "resources/animations/boss_vampire_skeleton.json",
      "anim": "resources/animations/boss_anim.json"
    },
    "boss_fire_demon": {
      "skeleton": "resources/animations/boss_fire_demon_skeleton.json",
      "anim": "resources/animations/boss_anim.json"
    },
    "boss_self": {
      "skeleton": "resources/animations/boss_demon_lord_skeleton.json",
      "anim": "resources/animations/boss_anim.json"
    }
```

注意 `boss_self` 的值指向 **`boss_demon_lord`** 骨架（有意为之，见偏差 1）：F15 唯一 Boss 是 demon_lord，`boss.cpp:1215-1220` 未命中 `boss_demon_lord` 整图键时把 `sprite_override` 设为 `"boss_self"`，白名单按键名查表。

- [ ] **Step 3: `animation_test.cpp` expected 40 → 45 + 骨架/anim 解析门禁**

把 `:681-711` 的 `TEST(ActorAvatarDefs, RepoDefaultWhitelistCoversA6HumanoidFamily)` 末尾改为：

```cpp
                                            "npc_120", "npc_140",
                                            "boss_shadow_knight", "boss_necromancer",
                                            "boss_vampire", "boss_fire_demon",
                                            "boss_self"};
    ASSERT_EQ(out->size(), expected.size());
    for (const auto& key : expected) {
        auto it = out->find(key);
        ASSERT_NE(it, out->end()) << key;
        EXPECT_FALSE(it->second.skeleton.empty());
        EXPECT_FALSE(it->second.anim.empty());
        EXPECT_TRUE(std::filesystem::exists(it->second.skeleton)) << key;
        EXPECT_TRUE(std::filesystem::exists(it->second.anim)) << key;
        std::string parse_err;
        auto sk = load_skeleton_file(it->second.skeleton, parse_err);
        ASSERT_TRUE(sk.has_value()) << key << " " << parse_err;
        EXPECT_EQ(sk->bones.size(), 9u) << key;
        EXPECT_EQ(sk->parts.size(), 7u) << key;
        auto anim = load_anim_file(it->second.anim, *sk, parse_err);
        ASSERT_TRUE(anim.has_value()) << key << " " << parse_err;
    }
```

同时删掉原 `:711` 的 `EXPECT_TRUE(std::filesystem::exists(out->begin()->second.anim));`（已被循环内 per-entry 检查覆盖）。注释行 `:685` 改为「批次1 (人形) + 批次2 (软体) + 批次3 (浮灵/魔像) + 批次4 (人形补充) + 批次5 (影武者/毒液蠕虫) + 批次6 (NPC) + 批次7 (Boss)」。

新增 `load_skeleton_file` / `load_anim_file` 已由既有 `#include "data/animation_defs.h"`（`:2`）提供（声明见 `animation_defs.h:53-54`），无需新增 include。既有 41 份骨架已验证全部为 9 骨 / 7 件 / ppu 0.8，断言不会误伤。

- [ ] **Step 4: 构建**

Run: `cmake --build build`

Expected: 无 error；`animation_test` 触及行无新 warning。

- [ ] **Step 5: 全量测试 + validator**

Run: `ctest --test-dir build`；`conda run python tools\world_validator.py`

Expected: **68/68** 全过（测试数不增加，只改期望集合与断言）；validator **0/0**。重点看 `ActorAvatarDefs.RepoDefaultWhitelistCoversA6HumanoidFamily` 全绿——它现在同时校验 45 份骨架 + 5 份 anim 能被真实解析器解析（Task 1 的 rig/anim schema 门禁）。

- [ ] **Step 6: Code Review**

Run: `git diff --stat`；`git diff -- resources/sprites.json resources/animations/actor_avatars.json | Measure-Object -Line`

Expected: 3 个文件被改；`sprites.json` diff 纯新增（无既有键被删改）、230 键；白名单 45 键且键序 mon 30 → npc 10 → boss 5；`animation_test.cpp` 未删既有 mon/npc 键。

### Task 4: C++ 0 行改动核验（纯验证，无代码修改）

**Files:** 无（本 Task 不产生 diff）

**Interfaces:**
- Consumes: Task 3 的 45 键白名单。
- Produces: 确认「零改动即接管」假设成立，Task 5/7 才可依赖。

- [ ] **Step 1: 三处承重路径 grep 核验**

Run:
```powershell
Select-String -Path "src\game\scene\game_scene_input.cpp" -Pattern "monsters\.emplace_back" | ForEach-Object { $_.LineNumber }
Select-String -Path "src\game\entities\monster.cpp" -Pattern "skeleton_avatar|is_boss" | ForEach-Object { $_.LineNumber }
Select-String -Path "src\game\rendering3d\hd2d_scene_builder.cpp" -Pattern "_build_entities|is_boss" | ForEach-Object { $_.LineNumber }
```

Expected:
- `game_scene_input.cpp` 命中 `:133` `_s.monsters.emplace_back(boss)`——Boss 与怪同在 `gs.monsters`，`_monster_avatars_tick()`（`game_scene.cpp:2991`）遍历同一容器，键 = `monster_actor_key(m)` = 非空 `sprite_override` 否则 `name`。
- `monster.cpp` 骨骼分支在 `:266-271`，函数体内**不得**出现 `is_boss` 条件排除（`is_boss` 仅用于血条抑制 `:286` 与旧 sprite 配色）。
- `hd2d_scene_builder.cpp` `_build_entities` 在 `:459`，内部**不得**出现 `is_boss`；`:462-469` 为 `skeleton_avatar()` active → `appendAvatarParts` 优先、否则旧 billboard 回落。

- [ ] **Step 2: 确认无 C++ diff**

Run: `git status --short src`

Expected: **空输出**（本批 C++ 改动 0 行）。

- [ ] **Step 3: 键链闭环核对**（写临时 `.py` 校验，只输出 ASCII）

按 `boss.cpp:_boss_type_to_id` → `visual_id` → `sprite_by_key("boss_<visual_id>")` → `sprite_override` → 白名单键，逐 Boss 打印预期键并断言在白名单中：

| Boss | visual_id | `boss_<visual_id>` 整图已注册? | 实际 `sprite_override` | 白名单键 | 骨架 |
|---|---|---|---|---|---|
| shadow_knight | shadow_knight | 是（`sprites.json:190`） | `boss_shadow_knight` | `boss_shadow_knight` | `boss_shadow_knight` |
| necromancer | necromancer | 是（`:195`） | `boss_necromancer` | `boss_necromancer` | `boss_necromancer` |
| vampire | vampire | 是（`:200`） | `boss_vampire` | `boss_vampire` | `boss_vampire` |
| fire_demon | fire_demon | 是（`:205`） | `boss_fire_demon` | `boss_fire_demon` | `boss_fire_demon` |
| demon_lord | demon_lord | **否** | 回落 `boss_self`（F15） | `boss_self` | `boss_demon_lord` |

Expected: 5 行全部「键在白名单内 + 骨架文件存在」。第 5 行是偏差 1 的关键——`boss_self` 键同时是「旧静态图回落键」与「demon_lord 骨架白名单键」，两者互不冲突（前者走 `sprite_by_key`，后者走 `_actor_avatars`）。

### Task 5: sim A/B 确定性核验

**Files:** 临时：`C:\Users\HP\AppData\Local\Temp\opencode\`（`av_new.json`、`av_old.json`、`sim_after7.txt`、`sim_before7.txt`）

**Interfaces:**
- Consumes: Task 3 的白名单 45 键；`build\roguelike_cpp.exe`（Task 3 已重建）。
- Produces: 白名单改动不改变战斗确定性的证据（预期一致，因 sim 无头不实例化 SkeletonAvatar）。

- [ ] **Step 1: 新白名单跑基准**

Run:
```powershell
Copy-Item resources\animations\actor_avatars.json C:\Users\HP\AppData\Local\Temp\opencode\av_new.json -Force
.\build\roguelike_cpp.exe --sim 12 --sim-seed 3 2>&1 | Out-File -Encoding utf8 C:\Users\HP\AppData\Local\Temp\opencode\sim_after7.txt
```

- [ ] **Step 2: 切 40 键旧白名单跑对照，随后还原**

Run:
```powershell
git show HEAD:resources/animations/actor_avatars.json | Out-File -Encoding utf8 C:\Users\HP\AppData\Local\Temp\opencode\av_old.json
Copy-Item C:\Users\HP\AppData\Local\Temp\opencode\av_old.json resources\animations\actor_avatars.json -Force
.\build\roguelike_cpp.exe --sim 12 --sim-seed 3 2>&1 | Out-File -Encoding utf8 C:\Users\HP\AppData\Local\Temp\opencode\sim_before7.txt
Copy-Item C:\Users\HP\AppData\Local\Temp\opencode\av_new.json resources\animations\actor_avatars.json -Force
git status --short resources\animations\actor_avatars.json
```

Expected: 最后一条为 ` M resources\animations\actor_avatars.json`（说明已还原为 45 键版，未被 HEAD 版覆盖）。

- [ ] **Step 3: 逐字节比较**

Run:
```powershell
(Get-FileHash C:\Users\HP\AppData\Local\Temp\opencode\sim_after7.txt).Hash
(Get-FileHash C:\Users\HP\AppData\Local\Temp\opencode\sim_before7.txt).Hash
```

Expected: 两个 sha256 **相同**（逐字节一致）。若不一致，用 `Compare-Object` 定位差异行：仅 `__DATE__/__TIME__` 编译启动行差异可容忍（批次6 已定性），其余任何战斗结果差异都说明白名单改动泄漏进逻辑路径 → 回到 Task 4 排查。

### Task 6: README CHANGELOG + 桌面包同步

**Files:**
- Modify: `README.md`（CHANGELOG 批次6 条目之后追加批次7 条目）
- Sync: `C:\Users\HP\Desktop\Roguelike-CPP-3D版`

**Interfaces:**
- Consumes: Task 1-5 全绿。
- Produces: 可交付的桌面包（含 exe）。

- [ ] **Step 1: CHANGELOG 追加批次7 条目**（置于批次6 条目之后，整段）：

```markdown
- A6-S2 批次7（开发版，未发布）：5 Boss 骨骼化（暗影骑士/亡灵法师/血族伯爵/地狱火魔/终焉回响）——新建 `tools/gen_boss_parts.py`（大 rig 1.4×：head 30×26 / torso 40×28 / arm 16×22 / leg 32×28 / weapon 10×30，BONES/pivot 同步 1.4×，5 套色板 + RAPIER/MIRROR_BLADE 新武器 + 每 Boss DECOR 造型差，fire_demon 空手全透明）与 `resources/animations/boss_anim.json`（三剪辑与 player_anim 同值，torso y 对齐新 bind 8）；`sprites.json` skeleton_parts +25（总 230），白名单 40→45（4 个 `boss_*` 键 + `boss_self`→`boss_demon_lord` 骨架）。**C++ 0 行改动**：Boss 与怪同在 `gs.monsters`，2D 骨骼分支与 HD2D `_build_entities` 无 `is_boss` 排除，白名单命中即自动接管。animation_test 升级为同时校验 45 份骨架/anim 可被真实解析器解析。旧 30 mon + 10 npc 分件与 `boss_f5/f10/self` 旧整图零 diff。68 项 CTest、World Validator 0/0 通过；sim A/B 逐字节一致。实机验收待完成。
```

- [ ] **Step 2: 桌面包同步**（robocopy `/MIR` 8 目录；保留 `saves\` 与 `3D模式.exe.lnk`）

Run:
```powershell
$dst = "C:\Users\HP\Desktop\Roguelike-CPP-3D版"
foreach ($d in @('src','resources','tools','tests','docs','assets','.github','vendor')) {
  robocopy "$PWD\$d" "$dst\$d" /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
  Write-Host "$d -> $LASTEXITCODE"
}
foreach ($f in @('CMakeLists.txt','README.md','CLAUDE.md','CMakePresets.json','.gitignore')) {
  Copy-Item "$PWD\$f" "$dst\$f" -Force
}
Copy-Item build\roguelike_cpp.exe "$dst\roguelike_cpp.exe" -Force
Copy-Item build\raylib.dll "$dst\raylib.dll" -Force
```

Expected: 8 行 exit code 均 0/1（1 = 有复制成功，正常）；无 2+。

- [ ] **Step 3: 桌面包校验**

Run:
```powershell
$dst = "C:\Users\HP\Desktop\Roguelike-CPP-3D版"
(Get-ChildItem "$dst\assets\sprites\boss_*_part_*.png").Count
(Get-ChildItem "$dst\resources\animations\boss_*_skeleton.json").Count
Test-Path "$dst\resources\animations\boss_anim.json"
Test-Path "$dst\saves"
Test-Path "$dst\3D模式.exe.lnk"
(Get-Item "$dst\roguelike_cpp.exe").LastWriteTime
```

Expected: `25`、`5`、`True`、`True`、`True`、exe 时间为刚刚。**不得改动** `C:\Users\HP\Desktop\Roguelike-CPP-初代版`（2D 冻结包）。

### Task 7: 实机验收（用户门）+ 单次提交

**Files:** 无新增；Commit 全批单次提交

**Interfaces:**
- Consumes: Task 1-6 全绿 + 桌面包已同步。
- Produces: 提交 `feat(a6-s2): 批次7 - 5 Boss骨骼分件与接入`。

- [ ] **Step 1: 向用户报告验收清单**

用户点桌面包根目录 `roguelike_cpp.exe`，逐项确认：

- **F5** 三个 Boss 逐一出现（seed 决定；重进几次才能凑齐 shadow_knight / necromancer / vampire）：不再是 16×16 配色方块，应为骨骼分件、明显大于普通怪（1.4×）。
- **F10** fire_demon：空手无武器部件，躯干有熔岩纹路。
- **F15** demon_lord：镜像虚空紫调，持 mirror_blade。
- 三态：站定 idle 呼吸、移动 walk 步频、攻击 attack 挥击，无抽搐/回弹（重点看 attack 起手是否有 y 跳动——这正是 `boss_anim.json` 存在的理由）。
- Phase2 血红特效仍正常（骨骼路径不影响 Phase2 shader/闪光）。
- 2D 与 HD2D 两种渲染模式各看一遍。
- 脚不悬空、不陷地（feet 锚点在 entity 底边；大 rig 若偏高可在 `PART_SLOTS` 微调，但**不应**改 rig DIMS）。
- 回落：临时删掉一个白名单键重跑，对应 Boss 应退回旧 16×16 静态图（验证回落链），验完还原。

- [ ] **Step 2: 等用户实机反馈**（阻塞点，用户通过后才进入 Step 3）

- [ ] **Step 3: 全量复检**

Run: `ctest --test-dir build`；`conda run python tools\world_validator.py`；`git status --short`

Expected: 68/68、validator 0/0、工作区仅有预期文件（25 PNG + 5 骨架 + 1 anim + 1 生成器 + 3 注册/测试文件 + README + 本计划文档）；`reports/` 不入 git。

- [ ] **Step 4: Code Review + 单次提交**

Run: `git diff --stat`，然后：
```powershell
git add -A
git commit -m "feat(a6-s2): 批次7 - 5 Boss骨骼分件与接入"
```

Expected: 单个 commit 涵盖全批；`.superpowers/sdd/` 等临时工作区（若存在）不得入库。

---

## Self-Review

1. **Spec coverage**：§1 生成器（5 Boss 表驱动 / 大 rig / 每 Boss DECOR / fire_demon 空手 / ≤300 行）→ Task 1；§1 注册 230 / 45 → Task 3；§2 C++ 0 行（已核实，Task 4 从「改」降为「验」）→ Task 4；§3 零 diff 门 / ctest 68 / validator 0-0 / sim A/B / CHANGELOG / 桌面同步 / 实机验收 / 单 commit → Task 2、3、5、6、7。范围外项（GOLEM 死代码 / Phase2 专属剪辑 / lightning_orb 运行时 / 新骨拓扑 / 旧图删除）全程未触碰。
2. **Placeholder**：无 TBD / TODO / 「类似 Task N」。生成器全文、`boss_anim.json` 全文、25 键 sprites.json 块、5 键白名单块、测试补丁、桌面同步脚本均为可直接粘贴的实值。
3. **Type/命名一致性**：5 个 id 全库统一 `shadow_knight` / `necromancer` / `vampire` / `fire_demon` / `demon_lord`（无 `demon_lard`）；文件名 `boss_<id>_part_<name>.png` 在生成器、骨架 `file` 字段、sprites.json 三处逐字一致；DIM S `SIZES` 与 sprites.json `w/h` 五个部件逐一对应；`BOSS[boss_id] = (palette_key, weapon)` 索引 0/1 与 `part_rows` 用法一致；白名单 `boss_self` 键名与 `boss.cpp:1215-1220` 回落串逐字一致。
4. **计数**：`skeleton_parts` 205 + 25 = 230；白名单 40 + 5 = 45；动画测试 `expected` 集合 40 + 5 = 45；生成器 5 Boss × 5 部件 = 25 PNG、5 骨架；9 骨 / 7 件沿用既有 41 份骨架的统一契约。
5. **风险闭环**：anim key 为绝对 local（非增量）→ 新增 `boss_anim.json` 而非复用 player_anim；humanoid `build_palette` 绑死其 `FAMILIES` → 生成器本地 `build_palette`；武器按目标尺寸直书会撞 `upscale` → `part_rows` 对 weapon 仍走 `upscale(rows, 10, 30)`（fx=fy=1.0 恒等），故 RAPIER/MIRROR_BLADE 必须严格 10 列 30 行；空手全透明件绕过 upscale 与 DECOR 避免 `_bbox` 全透明崩溃；`boss_demon_lord` 无整图键 → 用 `boss_self` 白名单键兜住 F15 镜像；`git show > file` 走 PS 会写 UTF-16 → 全程 `Out-File -Encoding utf8`；桌面包同步用 robocopy `/MIR` 会删除桌面独有文件 → 已限定 8 目录且排除 `saves\` / `3D模式.exe.lnk`。


