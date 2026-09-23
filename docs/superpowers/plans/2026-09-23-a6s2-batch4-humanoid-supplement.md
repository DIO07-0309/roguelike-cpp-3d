# A6-S2 批次4：人形补充 6 怪 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 charger/summoner/necromancer/ice_warden/blood_priest/dark_mage 生成骨骼分件并接入资源体系，零 C++ 改动。

**Architecture:** 扩展 `tools/gen_mon_humanoid_parts.py`（方案一）：新增 6 色板 + 4 武器 + 6 映射，复用人形 rig；正式生成前以 dry-run 临时目录对旧 8 怪做零 diff 回归门；注册链与批次3 同构（sprites.json / actor_avatars / animation_test）。

**Tech Stack:** Python 3 + Pillow（conda run）、C++17 + CMake + GoogleTest、JSON 资源。

## Global Constraints

- 裁定 A（2026-09-23，用户授权）：批次1 的 16 个旧 8 怪 arm/leg PNG 为既有存量过期（磁盘 9×16/12×20、sprites.json 登记 9/12，模板已加宽为 11/22），本批 Task 2 负责重生成并同步更新 sprites.json 对应 16 处尺寸；凡涉及“旧 8 怪零 diff”的门禁，判定标准为“**恰为该 16 项 arm/leg 差异、其余 0 项**”。
- A/B 方法注记（裁定 B4-2，2026-09-23，用户采纳）：`--sim` 不触达 actor_avatars 白名单（`game_scene.h:383` 无头模式不渲染、sim 输出零 `A6:` 行）；且 PS 5.1 的 `>` 重定向产出 UTF-16 白名单（游戏 UTF-8 加载器拒绝）。未来批次 sim A/B：换文件用 `Out-File -Encoding utf8` 或 cmd 原生重定向；DoD“与上批基线一致”以两侧规范 UTF-8 的 after 输出哈希对比为准；真正的白名单门禁 = ctest（加载白名单+骨骼做 pose 断言）+ World Validator + 实机验收。
- 生成器 ≤300 行、函数 ≤40 行；本次仅新增数据常量，不新增函数。
- 全部骨架 `pixels_per_unit = 0.8`；武器/色板行字符只允许 `. o s m l h r R p b g`。
- 零 C++ 源码改动；JSON 改后必须 `python tools/world_validator.py`（conda 环境）。
- Python 一律 `conda run python`；不支持含换行 `-c`，多行脚本写临时文件再执行。
- **提交策略（用户偏好，覆盖 skill 默认的每任务 commit）**：全批单一 commit，经用户实机验收确认后执行。
- 每步完成后按 CLAUDE.md 规范做 Code Review；交付前同步桌面包 `C:\Users\HP\Desktop\Roguelike-CPP-3D版`（保留 `saves/`、`3D模式.exe.lnk`）。
- Spec：`docs/superpowers/specs/2026-09-23-a6s2-batch4-humanoid-supplement-design.md`（`2ae5d8d`）。

---

### Task 1: 扩展生成器 + dry-run 零 diff + sheet 评审

**Files:**
- Modify: `tools/gen_mon_humanoid_parts.py`（FAMILIES/MONSTERS/WEAPONS 常量区，约 :12-80）
- Create: `C:\Users\HP\AppData\Local\Temp\opencode\b4dry\`（dry-run 输出）
- Artifact: `C:\Users\HP\AppData\Local\Temp\opencode\b4dry\sheet.png`（评审用）

**Interfaces:**
- Produces: MONSTERS 新增 6 键（后续 Task 2 正式生成依赖）；CLI `--output-dir/--skeleton-dir/--sheet` 已存在，无需改 main。

- [ ] **Step 1: FAMILIES 追加 6 个色板**（插在 `bone_white` 块后，字母键齐全 s/m/l/h/r/R/p/b/g）：

```python
    "orc_crimson": {"s": (88, 30, 34), "m": (140, 48, 46), "l": (192, 78, 62),
                    "h": (234, 132, 96), "r": (122, 60, 40), "R": (166, 86, 54),
                    "p": (236, 208, 160), "b": (42, 24, 26), "g": (124, 96, 64)},
    "goblin_amber": {"s": (70, 56, 34), "m": (120, 92, 48), "l": (170, 136, 70),
                     "h": (222, 188, 112), "r": (110, 82, 44), "R": (152, 118, 66),
                     "p": (248, 196, 84), "b": (48, 40, 32), "g": (120, 102, 66)},
    "necro_rot": {"s": (36, 48, 40), "m": (58, 82, 60), "l": (92, 126, 88),
                  "h": (144, 180, 122), "r": (104, 74, 50), "R": (146, 106, 68),
                  "p": (216, 220, 186), "b": (24, 30, 26), "g": (110, 96, 72)},
    "frost_ice": {"s": (48, 78, 110), "m": (80, 124, 164), "l": (126, 172, 206),
                  "h": (198, 230, 246), "r": (46, 60, 92), "R": (78, 98, 138),
                  "p": (236, 248, 255), "b": (30, 44, 62), "g": (96, 90, 76)},
    "blood_crimson": {"s": (58, 20, 32), "m": (98, 30, 46), "l": (148, 46, 64),
                      "h": (198, 72, 90), "r": (170, 130, 52), "R": (214, 172, 84),
                      "p": (240, 60, 70), "b": (34, 16, 24), "g": (110, 90, 70)},
    "void_dark": {"s": (34, 26, 48), "m": (56, 44, 80), "l": (86, 70, 122),
                  "h": (126, 110, 168), "r": (58, 50, 70), "R": (90, 80, 110),
                  "p": (104, 180, 110), "b": (22, 18, 32), "g": (96, 88, 70)},
```

- [ ] **Step 2: MONSTERS 追加 6 映射**（插在 `skeleton_archer` 后）：

```python
    "charger": ("orc_crimson", "standard", "lance"),
    "summoner": ("goblin_amber", "runt", "tome"),
    "necromancer": ("necro_rot", "gaunt", "scythe"),
    "ice_warden": ("frost_ice", "bulk", "greatsword"),
    "blood_priest": ("blood_crimson", "standard", "bloodstaff"),
    "dark_mage": ("void_dark", "gaunt", "staff"),
```

- [ ] **Step 3: WEAPONS 追加 4 个像素串常量**（GREATSWORD 后）并更新 WEAPONS dict：

```python
LANCE = ("..oo..|.ohhlo|ohhmmo|.ohhlo|orRrro|.rrrr.|..rr..|..gg..|..gg..|"
         "..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|.oggo.|"
         "..gg..|..gg..|..gg..|.oooo.|..oo..")
SCYTHE = ("...ooo|..ohhl|.ohhlo|ohhlo.|ohlo..|.oo...|..gg..|..gg..|..gg..|"
          "..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|"
          "..gg..|..gg..|..gg..|..gg..|.oooo.|..oo..")
TOME = (".oooo.|orrrro|orhhro|orhhro|orRRro|orhhro|orhhro|orrrro|orhhro|"
        "orhhro|orRRro|orhhro|orrrro|.oooo.|..gg..|..gg..|.oggo.|..gg..|"
        "..gg..|..gg..|..gg..|.oooo.|..oo..|......")
BLOODSTAFF = ("..oo..|.orrro|orRRro|orrrro|.oRro.|..r...|..gg..|..gg..|"
              "..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|"
              "..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|.oooo.|..oo..")
WEAPONS = {"sword": list(PART_ROWS["weapon"]), "cleaver": CLEAVER.split("|"),
           "bow": BOW.split("|"), "staff": STAFF.split("|"),
           "dagger": DAGGER.split("|"), "greatsword": GREATSWORD.split("|"),
           "lance": LANCE.split("|"), "scythe": SCYTHE.split("|"),
           "tome": TOME.split("|"), "bloodstaff": BLOODSTAFF.split("|")}
```

- [ ] **Step 4: 每串拆分后自检行数=24、行宽=6**（防 KeyError/ValueError 的干跑前置）

Run: `conda run python -c "import re; from pathlib import Path; s=Path('tools/gen_mon_humanoid_parts.py').read_text(encoding='utf-8'); import importlib.util; spec=importlib.util.spec_from_file_location('g','tools/gen_mon_humanoid_parts.py'); m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m); [print(k,len(v),{len(r) for r in v}) for k,v in m.WEAPONS.items() if k in ('lance','scythe','tome','bloodstaff')]"`
Expected: 四行输出 `24 {6}`（该命令含多行逻辑，实际写临时 .py 执行）。

- [ ] **Step 5: dry-run 到临时目录**

Run: `conda run python tools\gen_mon_humanoid_parts.py --output-dir C:\Users\HP\AppData\Local\Temp\opencode\b4dry\sprites --skeleton-dir C:\Users\HP\AppData\Local\Temp\opencode\b4dry\anim --sheet C:\Users\HP\AppData\Local\Temp\opencode\b4dry\sheet.png`
Expected: 打印 14 行 `wrote mon_*`，无异常/KeyError。

- [ ] **Step 6: 旧 8 怪零 diff 回归门**（写临时 `C:\Users\HP\AppData\Local\Temp\opencode\b4_zero_diff.py` 执行）：

```python
import hashlib, pathlib
OLD = ["orc","elite_orc","archer","shaman","goblin_hunter","tank","bone_soldier","skeleton_archer"]
def h(p): return hashlib.sha256(p.read_bytes()).hexdigest()
bad = []
for m in OLD:
    for part in ["torso","head","arm","leg","weapon"]:
        a = pathlib.Path(f"assets/sprites/mon_{m}_part_{part}.png")
        b = pathlib.Path(f"C:/Users/HP/AppData/Local/Temp/opencode/b4dry/sprites/mon_{m}_part_{part}.png")
        if h(a) != h(b): bad.append(str(a))
    a = pathlib.Path(f"resources/animations/mon_{m}_skeleton.json")
    b = pathlib.Path(f"C:/Users/HP/AppData/Local/Temp/opencode/b4dry/anim/mon_{m}_skeleton.json")
    if h(a) != h(b): bad.append(str(a))
print("DIFF:" if bad else "ZERO-DIFF OK", bad)
```

Expected（裁定 A）: `DIFF:` 恰为 16 项 = 旧 8 怪的 arm/leg PNG（既有存量过期清单，Task 2 修复），**其余 0 项**（torso/head/weapon/skeleton JSON/新怪条目出现任何 diff → 停止排查 Step 1-3 数据错误）。

- [ ] **Step 7: 查看 sheet 并提交用户评审**

Run: 读取 `C:\Users\HP\AppData\Local\Temp\opencode\b4dry\sheet.png`（Read 工具），附图给用户。
Expected: 用户确认 6 新怪造型/武器可接受；不接受 → 迭代 Step 1-3 后重跑 Step 5-7。

### Task 2: 正式生成（零 diff 门）

**Files:**
- Create: `assets/sprites/mon_*_part_*.png`（6×5=30）、`resources/animations/mon_{charger,summoner,necromancer,ice_warden,blood_priest,dark_mage}_skeleton.json`
- Modify（仅新文件出现）: `reports/mon_humanoid_sheet.png`

**Interfaces:**
- Produces: 30 PNG + 6 JSON（ppu 0.8），Task 3 登记依赖其文件名 `mon_<vid>_part_<slot>`。

- [ ] **Step 1: 正式运行生成器**（默认目录）

Run: `conda run python tools\gen_mon_humanoid_parts.py`
Expected: 14 行 `wrote mon_*`；sheet 落 `reports/mon_humanoid_sheet.png`。

- [ ] **Step 2: git 侧存量修复门 + 新文件在位**

Run: `git status --short assets/sprites resources/animations`
Expected（裁定 A）: 旧 8 怪中**恰好 16 个 arm/leg PNG 为 ` M`**（存量修复生效），旧 8 怪其余 32 文件（torso/head/weapon/skeleton JSON）无改动；新增 `??` 共 36（30 新 PNG + 6 新 JSON）。出现第 17 处旧 8 改动或 torso/head/weapon 改动 → 停止排查。

- [ ] **Step 2b: 同步 sprites.json 旧 8 arm/leg 尺寸（裁定 A 存量修复）**

写临时 `C:\Users\HP\AppData\Local\Temp\opencode\fix_b4_dims.py`：

```python
import json
from pathlib import Path
from PIL import Image
p = Path('resources/sprites.json')
j = json.loads(p.read_text(encoding='utf-8'))
OLD = ["orc","elite_orc","archer","shaman","goblin_hunter","tank","bone_soldier","skeleton_archer"]
fixed = []
for m in OLD:
    for part in ["arm", "leg"]:
        key = f"mon_{m}_part_{part}"
        entry = j["skeleton_parts"][key]
        img = Image.open(entry["file"])
        if (entry["w"], entry["h"]) != img.size:
            entry["w"], entry["h"] = img.size
            fixed.append(key)
p.write_text(json.dumps(j, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print("fixed", len(fixed), fixed)
```

Run: `conda run python C:\Users\HP\AppData\Local\Temp\opencode\fix_b4_dims.py`
Expected: `fixed 16 [...]`（arm w:9→11、leg w:12→22；与 PNG 实际尺寸逐一对齐）。

- [ ] **Step 3: 抽检新骨架 ppu**

Run: `conda run python -c "import json; print({m: json.load(open(f'resources/animations/mon_{m}_skeleton.json'))['pixels_per_unit'] for m in ['charger','summoner','necromancer','ice_warden','blood_priest','dark_mage']})"`（单行可过则用；否则临时文件）
Expected: 六项全 `0.8`。

### Task 3: 资源注册 + 构建 + 全量测试

**Files:**
- Modify: `resources/sprites.json`（skeleton_parts +30 → 135）
- Modify: `resources/animations/actor_avatars.json`（+6 → 26）
- Modify: `tests/animation/animation_test.cpp:681-694`（expected 20→26、注释）
- Create: `C:\Users\HP\AppData\Local\Temp\opencode\reg_b4.py`

**Interfaces:**
- Consumes: Task 2 的 30 PNG/6 JSON 文件名。
- Produces: 白名单 26 键（Task 4 sim A/B 的“新”侧）、测试断言 26。

- [ ] **Step 1: 写并执行登记脚本**（内容写入 `C:\Users\HP\AppData\Local\Temp\opencode\reg_b4.py`）：

```python
import json
from pathlib import Path
p = Path('resources/sprites.json')
j = json.loads(p.read_text(encoding='utf-8'))
DIMS = {'torso': (28, 20), 'head': (22, 20), 'arm': (11, 16), 'leg': (22, 20), 'weapon': (6, 24)}
ids = ['charger', 'summoner', 'necromancer', 'ice_warden', 'blood_priest', 'dark_mage']
added = 0
for mid in ids:
    for name, (w, h) in DIMS.items():
        key = f'mon_{mid}_part_{name}'
        if key not in j['skeleton_parts']:
            j['skeleton_parts'][key] = {'file': f'assets/sprites/mon_{mid}_part_{name}.png', 'w': w, 'h': h}
            added += 1
p.write_text(json.dumps(j, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
print('added', added, 'total', len(j['skeleton_parts']))
```

Run: `conda run python C:\Users\HP\AppData\Local\Temp\opencode\reg_b4.py`
Expected: `added 30 total 135`。

- [ ] **Step 2: actor_avatars.json 白名单 +6**（在 `mon_void_walker` 块后、收尾 `}` 前插入；每键 4 行，anim 全部 `resources/animations/player_anim.json`）：

```json
    "mon_charger": {
      "skeleton": "resources/animations/mon_charger_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "mon_summoner": {
      "skeleton": "resources/animations/mon_summoner_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "mon_necromancer": {
      "skeleton": "resources/animations/mon_necromancer_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "mon_ice_warden": {
      "skeleton": "resources/animations/mon_ice_warden_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "mon_blood_priest": {
      "skeleton": "resources/animations/mon_blood_priest_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "mon_dark_mage": {
      "skeleton": "resources/animations/mon_dark_mage_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    }
```

- [ ] **Step 3: animation_test expected 改 26**（注释行改“批次1-4”，集合尾部追加 6 项，字符串按 id 字典序无关、集合无序）：

```cpp
    // A6-S2 批次1 (人形) + 批次2 (软体) + 批次3 (浮灵/魔像) + 批次4 (人形补充)
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
                                            "mon_dark_mage"};
```

- [ ] **Step 4: 构建**

Run: `cmake --build build`
Expected: `Built target animation_test`，无 error。

- [ ] **Step 5: 全量测试**

Run: `cd build; ctest`
Expected: `100% tests passed, 0 tests failed out of 68`。

- [ ] **Step 6: World Validator**

Run: `conda run python tools\world_validator.py`
Expected: `Errors: 0 / Warnings: 0 / All checks passed`。

- [ ] **Step 7: 本任务 Code Review**（核对：135/26/26 三数字、JSON 可解析、无 C++ 源改动）

Run: `git status --short src`（应无输出）；`conda run python -c "import json; ...count..."` 三计数断言。

### Task 4: sim A/B 逐字节一致

**Files:**
- Temp: `av_new.json / av_old.json / sim_after4.txt / sim_before4.txt`（均在 `C:\Users\HP\AppData\Local\Temp\opencode\`）

**Interfaces:**
- Consumes: Task 3 的 26 键白名单；HEAD 的 20 键白名单（`git show HEAD:...`，spec commit 不含白名单改动）。

- [ ] **Step 1: 跑“新”侧**

Run: `Copy-Item resources\animations\actor_avatars.json $env:TEMP\..\..\..\..\..\Users\HP\AppData\Local\Temp\opencode\av_new.json -Force; .\build\roguelike_cpp.exe --sim 12 --sim-seed 3 2>&1 | Out-File -Encoding utf8 C:\Users\HP\AppData\Local\Temp\opencode\sim_after4.txt`（Copy 用全路径 `C:\Users\HP\AppData\Local\Temp\opencode\av_new.json`）
Expected: sim 正常退出。

- [ ] **Step 2: 换旧白名单跑“前”侧并还原**

Run: `git show HEAD:resources/animations/actor_avatars.json > C:\Users\HP\AppData\Local\Temp\opencode\av_old.json; Copy-Item C:\Users\HP\AppData\Local\Temp\opencode\av_old.json resources\animations\actor_avatars.json -Force; .\build\roguelike_cpp.exe --sim 12 --sim-seed 3 2>&1 | Out-File -Encoding utf8 C:\Users\HP\AppData\Local\Temp\opencode\sim_before4.txt; Copy-Item C:\Users\HP\AppData\Local\Temp\opencode\av_new.json resources\animations\actor_avatars.json -Force`
Expected: 还原后白名单仍 26 键。

- [ ] **Step 3: 逐字节比较**

Run: `$a=Get-Content C:\Users\HP\AppData\Local\Temp\opencode\sim_after4.txt -Raw; $b=Get-Content C:\Users\HP\AppData\Local\Temp\opencode\sim_before4.txt -Raw; if($a -ceq $b){'SIM BYTE-IDENTICAL'}else{'SIM DIFFERS'}`
Expected: `SIM BYTE-IDENTICAL`；否则排查白名单误改运行时行为，回退异常项。

### Task 5: CHANGELOG + 桌面同步

**Files:**
- Modify: `README.md`（CHANGELOG，插在批次3 条目后）
- Mirror → `C:\Users\HP\Desktop\Roguelike-CPP-3D版`

**Interfaces:**
- Consumes: Task 1-4 全部通过。

- [ ] **Step 1: CHANGELOG 追加批次4 条目**（批次3 条目整行之后新起一行）：

```markdown
- A6-S2 批次4（开发版，未发布）：人形补充 6 怪（冲锋兽人/哥布林召唤师/亡语者/冰狱守卫/血祭司/暗术师）接入骨骼——扩展 `gen_mon_humanoid_parts.py`（+6 色板：猩红/琥珀/腐绿/冰蓝/血红/暗紫；+4 新武器：长矛/镰刀/典籍/血杖；冰卫巨剑、暗术师法杖复用），重跑全量 14 怪旧 8 怪零 diff 回归通过；`sprites.json` skeleton_parts 登记 30 键（总 135），白名单扩至 26 键。68 项 CTest、World Validator 0/0 通过；`--sim 12 --sim-seed 3` 与批次3 基线逐字节一致。necromancer 双身份（Boss 版走 boss.cpp）不受影响。实机验收待完成。后续批次：影武者、毒液蠕虫、NPC、Boss。
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

Run: 断言 `mon_charger_part_torso.png` 存在、白名单 `mon_dark_mage` 计数=26、`saves\` 与 `3D模式.exe.lnk` 仍在、README.md 为新时间戳。
Expected: 全部 True/26。

### Task 6: 实机验收 + 提交（用户门）

**Files:**
- Modify: `README.md`（CHANGELOG“实机验收待完成”→“实机运行确认 6 怪渲染正常”）
- Commit: 全批单次提交

**Interfaces:**
- Consumes: Task 1-5 全绿 + 用户实机反馈。

- [ ] **Step 1: 向用户报告验收点**

报告：监狱 1-5 层挑战房第3波（charger+summoner）、火山 6-10 层挑战房第3波（necromancer）、深渊 11-15 层挑战房第1波（dark_mage）/第2波（ice_warden+blood_priest）。

- [ ] **Step 2: 等待用户实机确认**（阻塞门；有问题 → 截图迭代美术/注册）

- [ ] **Step 3: 更新 CHANGELOG 验收句** 并同步 README.md 到桌面包根目录（Step 同 Task 5 Step 2 的根文件复制）。

- [ ] **Step 4: Code Review + 提交**

Run: `git status --short`（应仅本批 5 改动 + 36 新文件）、`git diff --stat`、`git log --oneline -3`。
Run: `git add -A; git commit -m "feat(a6-s2): 人形补充批次4 - 6 怪骨骼分件生成与接入"; git log --oneline -2; git status --short`
Expected: 41+ 文件入库，工作区干净。

---

## Self-Review

1. **Spec coverage**：§1 色板/武器/tier/零 diff 门 → Task1-2；§2 注册五步 → Task2-3；§3 验证链+验收点+同步+commit → Task3-6；§4 风险 → Task1 Step6（KeyError）、Task2 Step2（零 diff）、Task6 Step1（双身份报告）。DoD 全覆盖。
2. **Placeholder**：无 TBD/“类似 Task N”；所有代码/脚本/命令/文案已给出实际内容。
3. **Type/命名一致性**：6 id 拼写与 enemies.json visual_id 一致；`mon_<vid>_part_<slot>` 命名与 sprites.json/DIMS/animation_test 集合一致；白名单 20→26、skeleton_parts 105→135 各处一致；`runt` 等 tier 键存在于 TIERS。
