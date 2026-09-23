# A6-S2 批次5：影武者 3 怪 + 毒液蠕虫 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 shadow_stalker/shadow_assassin/night_stalker/poison_wyrm 生成骨骼分件并接入资源体系（enemies 30/30 全量骨骼化），零 C++ 源码改动；附带 lightning_orb 火山池**数据面**清偿。

**Architecture:** 扩展两个现有生成器（方案一）：humanoid +3 色板 +3 映射 +`SPEAR`；soft +1 色板 +`SOFT_ROWS_OVERRIDES` 虫形覆盖 +`wyrm_venom` 映射。dry-run 对旧 19 怪零 diff 回归门；注册链与批次4 同构（sprites / actor_avatars / animation_test）+ 双源火山池同改。

**Tech Stack:** Python 3 + Pillow（conda run）、C++17 + CMake + GoogleTest、JSON 资源。

## Global Constraints

- **Spec**：`docs/superpowers/specs/2026-09-23-a6s2-batch5-shadow-wyrm-design.md`（`dad136d` + 勘误：spear 本批新增、lightning_orb 降数据面）。
- **裁定 A（批次4 遗留）不适用于本批**：本批旧怪门禁 = **严格零 diff**（humanoid 旧 14 + soft 旧 5 = 19 怪 95 PNG + 19 JSON 逐字节一致），无 16 项 arm/leg 例外。
- **裁定 B4-2**：`--sim` 不触达白名单；sim A/B 用 `Out-File -Encoding utf8`；真门禁 = ctest + validator + 实机验收。
- **裁定 B-1′（2026-09-23，用户后改走推荐项）**：lightning_orb 双源池只做**数据面清偿**（`biomes.json` + `world/volcano.json` 的 `enemy_pool`/`enemy_weights` 长度耦合同步 +1）；**删除**「火山常规层池出」验收点——`BiomeDef::enemy_pool` 无 C++ 消费、encounter `risk` 无执行器、`biome_events.json` 无加载器。CHANGELOG 如实写「池已登记，运行时刷出留后续 C++ 批次」。
- **spear（用户方案1）**：humanoid 新增 `SPEAR` 像素串 + `WEAPONS["spear"]`；spec 行42/83 已勘误（计划不依赖旧文）。
- 生成器 ≤300 行、函数 ≤40 行；本批仅新增数据常量 + soft `part_image` 查覆盖表（不新增函数）。当前 humanoid 246 / soft 201。
- 全部骨架 `pixels_per_unit = 0.8`；武器/色板行字符只允许 `. o s m l h r R p b g`；humanoid 描边 `(63,38,49)`。
- 零 C++ 源码改动；JSON 改后必须 `conda run python tools/world_validator.py` → 0/0。
- Python 一律 `conda run python`；多行脚本写 `C:\Users\HP\AppData\Local\Temp\opencode\` 临时 .py；脚本只 ASCII 输出。
- **提交策略**：全批单一 commit，经用户实机验收确认后执行（覆盖 skill 默认每任务 commit）。
- 每步 Code Review；交付前同步桌面包 `C:\Users\HP\Desktop\Roguelike-CPP-3D版`（robocopy /MIR 8 目录 exit 0/1 成功；保留 `saves/`、`3D模式.exe.lnk`；根文件 + 根目录 exe）。
- sheet 产物在 `reports/`（**.gitignore**），本地评审用，不进 git status。

---

### Task 1: 扩展两生成器 + dry-run 零 diff + sheet 评审

**Files:**
- Modify: `tools/gen_mon_humanoid_parts.py`（FAMILIES/MONSTERS/WEAPONS，约 :12-118）
- Modify: `tools/gen_mon_soft_parts.py`（FAMILIES/MONSTERS/SOFT_ROWS_OVERRIDES/part_image，约 :18-147）
- Create: `C:\Users\HP\AppData\Local\Temp\opencode\b5dry\{sprites,anim}\`
- Artifact: `...\b5dry\sheet_humanoid.png`、`...\b5dry\sheet_soft.png`

**Interfaces:**
- Produces: MONSTERS 两文件各 +N 键（Task 2 正式生成依赖）；CLI 参数已存在。

- [ ] **Step 1: humanoid FAMILIES 追加 3 色板**（插在 `void_dark` 后，九键齐全 s/m/l/h/r/R/p/b/g）：

```python
    "shadow_dusk": {"s": (28, 22, 40), "m": (52, 40, 72), "l": (86, 68, 116),
                    "h": (132, 112, 168), "r": (40, 34, 56), "R": (70, 60, 96),
                    "p": (92, 214, 208), "b": (18, 14, 26), "g": (96, 88, 70)},
    "shadow_ink": {"s": (18, 18, 22), "m": (36, 38, 46), "l": (62, 66, 78),
                   "h": (108, 116, 132), "r": (30, 34, 42), "R": (54, 60, 72),
                   "p": (156, 176, 200), "b": (12, 12, 16), "g": (96, 88, 70)},
    "night_brown": {"s": (48, 36, 26), "m": (86, 64, 44), "l": (128, 98, 66),
                    "h": (176, 144, 100), "r": (70, 54, 38), "R": (104, 82, 56),
                    "p": (214, 224, 236), "b": (32, 24, 18), "g": (96, 88, 70)},
```

- [ ] **Step 2: humanoid MONSTERS 追加 3 映射**（插在 `dark_mage` 后）：

```python
    "shadow_stalker": ("shadow_dusk", "standard", "dagger"),
    "shadow_assassin": ("shadow_ink", "gaunt", "dagger"),
    "night_stalker": ("night_brown", "standard", "spear"),
```

- [ ] **Step 3: humanoid 新增 SPEAR 并登记 WEAPONS**（`BLOODSTAFF` 后；**本批唯一新武器键**）：

```python
SPEAR = ("..oo..|.ohhlo|.ohhlo|ohhmmo|.orRro|.rrrr.|..rr..|..gg..|..gg..|"
         "..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|"
         "..gg..|..gg..|.oooo.|..oo..|......|......")
WEAPONS = {"sword": list(PART_ROWS["weapon"]), "cleaver": CLEAVER.split("|"),
           "bow": BOW.split("|"), "staff": STAFF.split("|"),
           "dagger": DAGGER.split("|"), "greatsword": GREATSWORD.split("|"),
           "lance": LANCE.split("|"), "scythe": SCYTHE.split("|"),
           "tome": TOME.split("|"), "bloodstaff": BLOODSTAFF.split("|"),
           "spear": SPEAR.split("|")}
```

- [ ] **Step 4: spear 尺寸自检**（写临时 `C:\Users\HP\AppData\Local\Temp\opencode\b5_spear_check.py`）：

```python
import importlib.util
spec = importlib.util.spec_from_file_location("g", "tools/gen_mon_humanoid_parts.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
rows = m.WEAPONS["spear"]
print(len(rows), {len(r) for r in rows})
assert len(rows) == 24 and {len(r) for r in rows} == {6}
print("SPEAR OK")
```

Run: `conda run python C:\Users\HP\AppData\Local\Temp\opencode\b5_spear_check.py`
Expected: `24 {6}` + `SPEAR OK`；否则回修 Step 3。

- [ ] **Step 5: soft FAMILIES 追加 `wyrm_venom` + MONSTERS 映射 + 导入 STAFF**：

Import 行改为：

```python
from gen_mon_humanoid_parts import (BONES, PART_SLOTS, SIZES, STAFF,
                                    skeleton_dict)
```

FAMILIES 追加（`leech_blood` 后）：

```python
    "wyrm_venom":  {"s": (30, 48, 24),  "m": (52, 86, 42),  "l": (92, 140, 66),
                    "h": (168, 214, 96), "r": (74, 110, 40), "R": (112, 158, 58),
                    "p": (198, 232, 92), "b": (28, 32, 24),  "g": (96, 88, 56)},
```

MONSTERS 追加：

```python
    "poison_wyrm": "wyrm_venom",
```

- [ ] **Step 6: soft 虫形覆盖表 + part_image 查表 + 断言**（`SOFT_ROWS` 定义后）：

```python
SOFT_ROWS_OVERRIDES = {
    "wyrm_venom": {
        "torso": [
            "....oooooooooooooooo....",
            "..oohhhhhhhhhhhhhhhoo...",
            ".ohlllllllllllllllllho..",
            "olmmmmllooooooommmmmmlo.",
            "ollllloooooooooollmmllo.",
            "olmmmmmmmmmmmmmmmlllllo.",
            "ollllllllmmmmmmmmmmmlo..",
            "olmmmmllooooooommmmmmlo.",
            "ollllloooooooooollmmllo.",
            "olmmmmmmmmmmmmmmmlllllo.",
            "ollllllllmmmmmmmmmmmlo..",
            "olmmmmllooooooommmmmmlo.",
            "ollllloooooooooollmmllo.",
            "olmmmmmmmmmmmmmmmlllllo.",
            "ollllllllmmmmmmmmmmmlo..",
            ".ollllllllllllllllllo...",
            "..oolllllllllllllloo....",
            "....oooooooooooooooo....",
            ".......................",
            ".......................",
        ],
        "weapon": STAFF.split("|"),
    },
}
assert "torso" in SOFT_ROWS_OVERRIDES.get("wyrm_venom", {}), \
    "wyrm_venom must hit torso override (key typo guard)"
```

`part_image` 改 1 行基行选择（函数仍 ≤40 行）：

```python
    base = SOFT_ROWS_OVERRIDES.get(family, {}).get(part_name, SOFT_ROWS[part_name])
    rows = fit_rows(base, width, height)
```

（替换原 `rows = fit_rows(SOFT_ROWS[part_name], width, height)`。）

- [ ] **Step 7: 行数门**

Run: 临时脚本计两文件行数。
Expected: humanoid ≤300、soft ≤300；`python -m py_compile` 两文件无语法错。

- [ ] **Step 8: 双生成器 dry-run 到临时目录**

Run:
```
conda run python tools\gen_mon_humanoid_parts.py --output-dir C:\Users\HP\AppData\Local\Temp\opencode\b5dry\sprites --skeleton-dir C:\Users\HP\AppData\Local\Temp\opencode\b5dry\anim --sheet C:\Users\HP\AppData\Local\Temp\opencode\b5dry\sheet_humanoid.png
conda run python tools\gen_mon_soft_parts.py --output-dir C:\Users\HP\AppData\Local\Temp\opencode\b5dry\sprites --skeleton-dir C:\Users\HP\AppData\Local\Temp\opencode\b5dry\anim --sheet C:\Users\HP\AppData\Local\Temp\opencode\b5dry\sheet_soft.png
```
Expected: humanoid 打印 17 行 `wrote mon_*`、soft 打印 6 行；无 KeyError/ValueError/断言失败；两张 sheet 落盘。

- [ ] **Step 9: 旧 19 怪严格零 diff 回归门**（写 `C:\Users\HP\AppData\Local\Temp\opencode\b5_zero_diff.py`）：

```python
import hashlib, pathlib
HUMAN = ["orc","elite_orc","archer","shaman","goblin_hunter","tank",
         "bone_soldier","skeleton_archer","charger","summoner","necromancer",
         "ice_warden","blood_priest","dark_mage"]
SOFT = ["slime","bomber","elite_slime","frost_slime","blood_leech"]
def h(p): return hashlib.sha256(p.read_bytes()).hexdigest()
dry = pathlib.Path("C:/Users/HP/AppData/Local/Temp/opencode/b5dry")
bad = []
for m in HUMAN + SOFT:
    for part in ["torso","head","arm","leg","weapon"]:
        a = pathlib.Path(f"assets/sprites/mon_{m}_part_{part}.png")
        b = dry / "sprites" / f"mon_{m}_part_{part}.png"
        if h(a) != h(b): bad.append(str(a))
    a = pathlib.Path(f"resources/animations/mon_{m}_skeleton.json")
    b = dry / "anim" / f"mon_{m}_skeleton.json"
    if h(a) != h(b): bad.append(str(a))
print("ZERO-DIFF OK" if not bad else f"DIFF count={len(bad)}")
for x in bad: print(x)
```

Run: `conda run python C:\Users\HP\AppData\Local\Temp\opencode\b5_zero_diff.py`
Expected: **`ZERO-DIFF OK`**（0 项）。任何 diff → 停止，排查 Step 1-6 数据/覆盖表污染旧怪。

- [ ] **Step 10: 双 sheet 用户评审**（Read 工具读两张 png 附给用户）
Expected: 用户确认影3怪+毒液蠕虫造型/武器/虫形可接受；不接受 → 迭代后重跑 Step 8-10。

### Task 2: 正式生成（零 diff 门）

**Files:**
- Create: `assets/sprites/mon_{shadow_stalker,shadow_assassin,night_stalker,poison_wyrm}_part_{torso,head,arm,leg,weapon}.png`（20）
- Create: `resources/animations/mon_{shadow_stalker,shadow_assassin,night_stalker,poison_wyrm}_skeleton.json`（4）
- Side-effect（gitignored）: `reports/mon_humanoid_sheet.png`、`reports/mon_soft_sheet.png`

**Interfaces:**
- Produces: 20 PNG + 4 JSON（ppu 0.8），Task 3 登记依赖文件名 `mon_<vid>_part_<slot>`。

- [ ] **Step 1: 正式运行两生成器**（默认目录）

Run:
```
conda run python tools\gen_mon_humanoid_parts.py
conda run python tools\gen_mon_soft_parts.py
```
Expected: 17 + 6 行 `wrote mon_*`；两 sheet 更新。

- [ ] **Step 2: git 新文件在位 + 旧 19 怪零改**

Run: `git status --short assets/sprites resources/animations`
Expected: **恰好 24 个 `??`**（20 PNG + 4 JSON）；**零 ` M`**（旧文件一字节未动）。出现任何 ` M` → 停止排查。

- [ ] **Step 3: 正式产物再零 diff 抽检**（对 Task 1 脚本改 dry 路径为正式路径语义：对比「正式生成前后」不可行则跳过；以 Step 2 git 零 ` M` 为门）。

- [ ] **Step 4: ppu 抽检**

Run: 临时脚本断言 4 骨架 JSON `pixels_per_unit == 0.8`。
Expected: 全 True。

### Task 3: 资源注册 + 双源池 + 构建 + 全量测试

**Files:**
- Modify: `resources/sprites.json`（skeleton_parts +20 → 155）
- Modify: `resources/animations/actor_avatars.json`（`actors` 内 +4 → 30）
- Modify: `tests/animation/animation_test.cpp:685-697`（注释批次1-5、expected +4 → 30）
- Modify: `resources/biomes.json:39-40`（火山池 +lightning_orb、weights +1）
- Modify: `resources/world/volcano.json:14-15`（同池同权，保持无空格风格）
- Create: `C:\Users\HP\AppData\Local\Temp\opencode\reg_b5.py`

**Interfaces:**
- Consumes: Task 2 的 20 PNG/4 JSON 文件名。
- Produces: 白名单 30 键、测试断言 30、双源池数据一致。

- [ ] **Step 1: 写并执行登记脚本**（`reg_b5.py`）：

```python
import json
from pathlib import Path
p = Path("resources/sprites.json")
j = json.loads(p.read_text(encoding="utf-8"))
DIMS = {"torso": (28, 20), "head": (22, 20), "arm": (11, 16),
        "leg": (22, 20), "weapon": (6, 24)}
ids = ["shadow_stalker", "shadow_assassin", "night_stalker", "poison_wyrm"]
added = 0
for mid in ids:
    for name, (w, h) in DIMS.items():
        key = f"mon_{mid}_part_{name}"
        if key not in j["skeleton_parts"]:
            j["skeleton_parts"][key] = {
                "file": f"assets/sprites/mon_{mid}_part_{name}.png",
                "w": w, "h": h}
            added += 1
p.write_text(json.dumps(j, indent=2, ensure_ascii=False) + "\n",
             encoding="utf-8")
print("added", added, "total", len(j["skeleton_parts"]))
```

Run: `conda run python C:\Users\HP\AppData\Local\Temp\opencode\reg_b5.py`
Expected: `added 20 total 155`。

- [ ] **Step 2: actor_avatars.json 白名单 +4**（`mon_dark_mage` 块后、`actors` 收尾 `}` 前；anim 全 `resources/animations/player_anim.json`）：

```json
    "mon_shadow_stalker": {
      "skeleton": "resources/animations/mon_shadow_stalker_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "mon_shadow_assassin": {
      "skeleton": "resources/animations/mon_shadow_assassin_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "mon_night_stalker": {
      "skeleton": "resources/animations/mon_night_stalker_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "mon_poison_wyrm": {
      "skeleton": "resources/animations/mon_poison_wyrm_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    }
```

- [ ] **Step 3: animation_test expected 改 30**（注释「批次1-5」，集合尾追加 4 项）：

```cpp
    // A6-S2 批次1 (人形) + 批次2 (软体) + 批次3 (浮灵/魔像) + 批次4 (人形补充) + 批次5 (影武者/毒液蠕虫)
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
                                            "mon_poison_wyrm"};
```

- [ ] **Step 4: 双源火山池数据面清偿**（精确替换，保持各文件空格风格；weights 同步长度）：

`resources/biomes.json`（带空格风格）:
- L39: `"enemy_pool": ["fire_imp", "elite_orc", "orc", "charger", "lightning_orb"],`
- L40: `"enemy_weights": [30, 25, 25, 20, 15],`

`resources/world/volcano.json`（无空格风格）:
- L14: `"enemy_pool": ["fire_imp","elite_orc","orc","charger","lightning_orb"],`
- L15: `"enemy_weights": [30,25,25,20,15],`

Run: 临时脚本断言两文件 `enemy_pool == ["fire_imp","elite_orc","orc","charger","lightning_orb"]` 且 `len(enemy_pool)==len(enemy_weights)==5`。
Expected: 两文件均 True。

- [ ] **Step 5: 构建**

Run: `cmake --build build`
Expected: `Built target animation_test`，无 error。

- [ ] **Step 6: 全量测试**

Run: `cd build; ctest`
Expected: `100% tests passed, 0 tests failed out of 68`。

- [ ] **Step 7: World Validator**

Run: `conda run python tools\world_validator.py`
Expected: `Errors: 0 / Warnings: 0 / All checks passed`（`lightning_orb` 在 enemies.id 白名单内须通过 check_ref）。

- [ ] **Step 8: 本任务 Code Review**

Run: `git status --short src`（应无输出）；三计数断言 `skeleton_parts==155`、`len(actors)==30`、animation_test 集合 size==30 语义核对；`git diff resources/biomes.json resources/world/volcano.json` 仅 4 行。

### Task 4: sim A/B

**Files:**
- Temp: `av_new.json / av_old.json / sim_after5.txt / sim_before5.txt`（`C:\Users\HP\AppData\Local\Temp\opencode\`）

**Interfaces:**
- Consumes: Task 3 的 30 键白名单；HEAD（`dad136d`）的 26 键白名单。
- 基线：批次4 after sha `b6814bdd4b44bf15e92153db5c4876552ea1d9f04ade82b68a724a155cc17d06`。

- [ ] **Step 1: 跑“新”侧**

Run: `Copy-Item resources\animations\actor_avatars.json C:\Users\HP\AppData\Local\Temp\opencode\av_new.json -Force; .\build\roguelike_cpp.exe --sim 12 --sim-seed 3 2>&1 | Out-File -Encoding utf8 C:\Users\HP\AppData\Local\Temp\opencode\sim_after5.txt`
Expected: sim 正常退出。

- [ ] **Step 2: 换旧白名单跑“前”侧并还原**

Run: `git show HEAD:resources/animations/actor_avatars.json > C:\Users\HP\AppData\Local\Temp\opencode\av_old.json; Copy-Item C:\Users\HP\AppData\Local\Temp\opencode\av_old.json resources\animations\actor_avatars.json -Force; .\build\roguelike_cpp.exe --sim 12 --sim-seed 3 2>&1 | Out-File -Encoding utf8 C:\Users\HP\AppData\Local\Temp\opencode\sim_before5.txt; Copy-Item C:\Users\HP\AppData\Local\Temp\opencode\av_new.json resources\animations\actor_avatars.json -Force`
Expected: 还原后白名单 30 键。

- [ ] **Step 3: 逐字节比较 + 与批次4 基线 sha**

Run: 比较 after5 vs before5；并对 after5 算 sha256。
Expected（主）: `SIM BYTE-IDENTICAL` 且 sha == 批次4 基线——双源池为死数据、白名单不触 sim，理应一致。
Expected（降级，裁定 DoD）: 若因未预期路径产生差异 → 同 seed 连跑两次 after5 同 sha（确定性）+ validator 0/0 + 无崩溃，CHANGELOG 如实记录；**不得**为凑一致回退 Task 3 池改动。

### Task 5: CHANGELOG + 桌面同步

**Files:**
- Modify: `README.md`（CHANGELOG 批次4 条目后）
- Mirror → `C:\Users\HP\Desktop\Roguelike-CPP-3D版`

**Interfaces:**
- Consumes: Task 1-4 全部通过。

- [ ] **Step 1: CHANGELOG 追加批次5 条目**：

```markdown
- A6-S2 批次5（开发版，未发布）：影武者+毒液蠕虫 4 怪（暗影潜伏者/暗影刺客/夜行猎手/毒液蠕虫）接入骨骼——humanoid 扩展（+3 色板：暮紫/墨黑/夜棕；+SPEAR 武器；夜猎持矛）、soft 扩展（+wyrm_venom 色板与环节虫形行覆盖），旧 19 怪严格零 diff 回归通过；`sprites.json` skeleton_parts +20（总 155），白名单 26→30，**enemies 30 怪全量骨骼化里程碑**。附带 lightning_orb 火山池**数据面清偿**（biomes+world/volcano 双源 +1，运行时刷出路径仍无、留后续 C++ 批次，本批不验收池出）。68 项 CTest、World Validator 0/0 通过；sim A/B 见 Task4 实测记录。实机验收待完成。后续批次：NPC、Boss、lightning_orb 运行时接入。
```

- [ ] **Step 2: 同步桌面包目录**（workdir=项目根；exit 0/1 均为成功）：

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

Run: `mon_shadow_stalker_part_torso.png` 存在、白名单 `mon_poison_wyrm` 计数=30、`saves\` 与 `3D模式.exe.lnk` 仍在、README.md 新时间戳、根目录 `roguelike_cpp.exe` 更新。
Expected: 全部 True/30。

### Task 6: 实机验收 + 提交（用户门）

**Files:**
- Modify: `README.md`（验收句）
- Commit: 全批单次提交

**Interfaces:**
- Consumes: Task 1-5 全绿 + 用户实机反馈。

- [ ] **Step 1: 向用户报告验收点**（已删假验收）

报告：监狱常规层（shadow_stalker）、深渊常规层（shadow_stalker/shadow_assassin）、深渊挑战房第2波（night_stalker）、火山挑战房（poison_wyrm 波）。**不含** lightning_orb 池出。

- [ ] **Step 2: 等待用户实机确认**（阻塞门；有问题 → 截图迭代美术/注册）

- [ ] **Step 3: 更新 CHANGELOG 验收句** 并 Copy-Item README.md 到桌面包根目录。

- [ ] **Step 4: Code Review + 提交**

Run: `git status --short`（src 无改动；JSON/测试/README + 24 新文件）、`git diff --stat`、`git log --oneline -3`。
Run: `git add -A; git commit -m "feat(a6-s2): 影武者毒液蠕虫批次5 - 4 怪骨骼分件与30/30全量接入"; git log --oneline -2; git status --short`
Expected: 约 30 文件入库，工作区干净（`reports/` 仍 ignore）。

---

## Self-Review

1. **Spec coverage**：§1 双生成器+零diff+虫形断言 → Task1-2；§2 注册五步+双源池 → Task2-3；§3 验证链+验收点（已删池出）+同步+commit → Task3-6；§4 spear KeyError/双源/行数/sim 降级 → Task1 Step4/7、Task3 Step4、Task4 Step3。DoD 全覆盖且与勘误后 spec 一致。
2. **Placeholder**：无 TBD；代码/命令/文案/池 JSON 片段已给实值；sheet 文件名已核实存在（`reports/mon_humanoid_sheet.png`、`mon_soft_sheet.png`）。
3. **Type/命名一致性**：4 id 与 enemies.json visual_id 一致；`mon_<vid>_part_<slot>` 与 DIMS/白名单/expected 一致；155/30/30/19 怪各处一致；`spear` 键进 WEAPONS；`wyrm_venom` 键进 FAMILIES/MONSTERS/OVERRIDES 且断言必中；池 5 槽与 weights 5 权长度耦合、双文件集合相等。
4. **发现闭环**：spear 缺失（用户方案1）、encounter/biome_events 死路（B-1′）、裁定 A 不继承、B4-2 sim 路径——全部进 Global Constraints，无静默假设。
