# NPC 视觉身份回退（撤回批次6 骨骼化） — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 10 个世界 NPC 从"护甲骨骼分件（像怪）"回退到批次6 之前的 28×28 旧圆脸整图，并彻底清除这批已知错误的美术资产，`src/` 零改动。

**Architecture:** NPC 渲染本就是「骨骼优先、静态图回落」（2D `game_scene.cpp:3161-3174`、HD2D `hd2d_scene_builder.cpp:982-1001`），且骨骼懒建失败会缓存不重试。因此**只需从 `actor_avatars.json` 移除 10 个 `npc_*` 键**，NPC 即自动落回 `npc_sprite_key(floor)` 的旧整图。再删掉这批资产（50 PNG + 10 skeleton + 50 条注册 + 生成器）让仓库不留重现陷阱。C++ 接线全部保留为休眠状态。

**Tech Stack:** C++17 / MinGW / nlohmann::json / GoogleTest / Pillow（生成器）/ PowerShell 5.1

**设计文档：** `docs/superpowers/specs/2026-09-24-a6s2-npc-visual-revert-design.md`（已批准，提交 `8fcab88`）

## Global Constraints

- 基线：HEAD `8fcab88`，工作树 clean
- **`src/` 零改动**：`git status --short src` 必须为空。C++ 接线（`NpcView.npc_id`、`_npc_avatars_tick`、2D/HD2D 分支）一行都不动
- 4 张旧整图 `npc_1/2/3/blacksmith.png` 及其 `sprites.json` `sprites` 段注册（`:275-293`）**保留**；`npc_sprite_key(floor)` 楼层映射（`npc_system.cpp:150-158`）**保留**
- 30 怪 + 5 Boss 的分件/骨架/白名单**零改动**
- `tools/world_validator.py` **不改**（已核实只检查 `player_skeleton.json`）
- 提交：**单次提交** `revert(a6-s2): 撤回批次6 NPC骨骼化 - 恢复原版整图`
- 不触碰 `Roguelike-CPP-初代版`（已冻结）；桌面同步保留 `saves/` 与 `3D模式.exe.lnk`

### 环境陷阱

- 构建：`cmake --build build --config Release -- -j 4`（不要用 `/m`）
- 测试：`ctest --test-dir build`（不要加 `--build-nofail`）
- Validator：`conda run python tools\world_validator.py`
- **PowerShell 命令里不要写中文字面量**（PS 5.1 会编码失败并可能让整行解析崩溃）；需要中文时用 Python 脚本 + `chr(0x...)`
- `conda run python -c` **不支持多行**（换行会被当成脚本参数），复杂逻辑写成 `.py` 文件再跑
- 写临时 Python 脚本时优先用 `write` 工具写 `.py`，但**写完必须 `read` 回读确认**——工具层出现过写入回显与磁盘内容不一致的情况

---

## Task 1: 数据面回退（actor_avatars + 测试期望）

**Files:**
- Modify: `resources/animations/actor_avatars.json:123-162`（删 10 条 npc 条目，45→35 键）
- Modify: `tests/animation/animation_test.cpp`（`expected` 集合去 10 个 npc 键；`:685` 注释去 `NPC`）

**Interfaces:**
- 消费：`load_actor_avatars_file` 产出 `std::map<std::string, ActorAvatarDef>`；NPC 运行时用键 `"npc_" + std::to_string(id)` 查（`game_scene.cpp:3029-3065`），查不到即走静态回落
- 产出：`actor_avatars.json` 35 键（30 mon + 5 boss）；`expected` 集合 35 键

- [ ] **Step 1: 从 actor_avatars.json 删除 npc 块（123-162 行）**

删掉这 40 行（10 条 × 4 行），使 `mon_poison_wyrm` 的 `},`（`:122`）直接接 `boss_shadow_knight`：

```json
    "npc_20": {
      "skeleton": "resources/animations/npc_20_skeleton.json",
      "anim": "resources/animations/player_anim.json"
    },
    "npc_30": { ... },
    "npc_40": { ... },
    "npc_60": { ... },
    "npc_70": { ... },
    "npc_80": { ... },
    "npc_90": { ... },
    "npc_110": { ... },
    "npc_120": { ... },
    "npc_140": { ... },
```

- [ ] **Step 2: 校验 JSON 合法且 35 键**

```powershell
conda run python -c "import json; d=json.load(open(r'C:\Demo\roguelike_cpp\resources\animations\actor_avatars.json',encoding='utf-8')); print('keys:',len(d['actors'])); print('npc keys:',sum(1 for k in d['actors'] if k.startswith('npc_')))"
```

期望：`keys: 35`、`npc keys: 0`。

- [ ] **Step 3: 从 animation_test.cpp 删 10 个 npc 键并改注释**

`:700-702` 删掉这 3 行：

```cpp
                                            "npc_20", "npc_30", "npc_40", "npc_60",
                                            "npc_70", "npc_80", "npc_90", "npc_110",
                                            "npc_120", "npc_140",
```

并把 `:685` 注释

```cpp
    // A6-S2 批次1-7: 人形/软体/浮灵魔像/人形补充/影武者毒液蠕虫/NPC/5 Boss
```

改为

```cpp
    // A6-S2 批次1-7,8: 人形/软体/浮灵魔像/人形补充/影武者毒液蠕虫/5 Boss（NPC 骨骼化已回退，见 README）
```

**保留**：循环内的 9 骨/7 件/anim 解析断言（`:707-721`）一行都不动，它继续覆盖剩余 35 键。

- [ ] **Step 4: 确认 src 零改动**

```powershell
git status --short src
```

期望输出为空。

---

## Task 2: 删除资产与生成器

**Files:**
- Delete: `assets/sprites/npc_*_part_*.png`（50 个）
- Delete: `resources/animations/npc_*_skeleton.json`（10 个）
- Delete: `tools/gen_npc_parts.py`

- [ ] **Step 1: 删 50 个分件 PNG**

```powershell
Remove-Item "C:\Demo\roguelike_cpp\assets\sprites\npc_*_part_*.png" -Force
(Get-ChildItem "C:\Demo\roguelike_cpp\assets\sprites" -Filter "npc_*_part_*.png").Count
```

期望 `0`。

- [ ] **Step 2: 删 10 个骨架 JSON**

```powershell
Remove-Item "C:\Demo\roguelike_cpp\resources\animations\npc_*_skeleton.json" -Force
(Get-ChildItem "C:\Demo\roguelike_cpp\resources\animations" -Filter "npc_*_skeleton.json").Count
```

期望 `0`。

- [ ] **Step 3: 删生成器**

```powershell
Remove-Item "C:\Demo\roguelike_cpp\tools\gen_npc_parts.py" -Force
Test-Path "C:\Demo\roguelike_cpp\tools\gen_npc_parts.py"
```

期望 `False`。

- [ ] **Step 4: 确认旧整图 4 张仍在**

```powershell
Get-ChildItem "C:\Demo\roguelike_cpp\assets\sprites" -Filter "npc_*.png" | ForEach-Object { $_.Name }
```

期望恰好 4 个：`npc_1.png`、`npc_2.png`、`npc_3.png`、`npc_blacksmith.png`。

---

## Task 3: 移除 sprites.json 的 50 条 npc 分件注册

**Files:**
- Modify: `resources/sprites.json:1242-1490`（删 50 条 `npc_*_part_*`，`skeleton_parts` 230 → 180）

**Interfaces:**
- 消费：`skeleton_parts` 段供资产校验与 `parts` 尺寸；`sprites` 段（整图）**不动**
- 产出：`skeleton_parts` 180 键，与磁盘文件一一对应

- [ ] **Step 1: 删 npc 分件注册块（1242-1490，50 条 × 5 行 = 250 行）**

用脚本按行删除 `"npc_\d+_part_` 开头的键及其后 4 行（每条 5 行：`"key": {` / `"file"` / `"w"` / `"h"` / `},`），共 50 条 250 行。

- [ ] **Step 2: 校验 JSON 合法、键数 180、无 npc 残留**

```powershell
conda run python -c "import json; d=json.load(open(r'C:\Demo\roguelike_cpp\resources\sprites.json',encoding='utf-8')); sp=d['skeleton_parts']; print('skeleton_parts:',len(sp)); print('npc part keys:',sum(1 for k in sp if k.startswith('npc_'))); print('old npc sprites kept:',[k for k in d['sprites'] if k in ('npc_1','npc_2','npc_3','npc_blacksmith')])"
```

期望：`skeleton_parts: 180`、`npc part keys: 0`、旧 4 张 `['npc_1','npc_2','npc_3','npc_blacksmith']` 齐全。

---

## Task 4: 门禁验证

**Files:** 无改动，只跑检查

- [ ] **Step 1: 资产一致性自检（本次新增门禁，写临时脚本）**

在 `C:\Users\HP\AppData\Local\Temp\opencode\npc_verify.py` 写脚本，断言：
1. `sprites.json` `skeleton_parts` 每个键的 `file` 在磁盘存在
2. 磁盘上每个 `resources/animations/*_skeleton.json`（排除 `player_anim.json`/`boss_anim.json`）的每个 `parts[].file` 存在
3. `actor_avatars.json` 每个键的 `skeleton` 与 `anim` 文件存在
4. 全仓 `npc_*_skeleton.json` / `npc_*_part_*.png` 命中数为 0
5. 4 张旧整图存在且在 `sprites` 段注册

脚本用相对路径基于 `C:\Demo\roguelike_cpp`，输出 ASCII，末尾 `print("OK" if all_ok else "FAIL")`。跑：

```powershell
conda run python C:\Users\HP\AppData\Local\Temp\opencode\npc_verify.py
```

期望 `OK`。

- [ ] **Step 2: 引用残留检查**

```powershell
Select-String -Path src\game\**\*.cpp,tests\**\*.cpp,resources\animations\actor_avatars.json -Pattern 'npc_20|npc_140|npc_.*_skeleton' | ForEach-Object { $_.Path + ":" + $_.LineNumber }
```

期望：无输出（`src/` 动态构造键 `"npc_" + id`，无字面量；测试已删；白名单已删）。

- [ ] **Step 3: 构建 + ctest**

```powershell
cmake --build build --config Release -- -j 4
ctest --test-dir build
```

期望：build 0 error；ctest **68/68**（`animation_test` 白名单断言随集合缩小而通过，测试数不变）。

- [ ] **Step 4: World Validator**

```powershell
conda run python tools\world_validator.py
```

期望：`Errors: 0` / `Warnings: 0`。

- [ ] **Step 5: 确认 src 零改动 + 统计改动面**

```powershell
git status --short src
git diff --stat
```

期望：`git status --short src` 为空；`git diff --stat` 显示 4 个修改文件（actor_avatars.json、sprites.json、animation_test.cpp、README.md），删除经 `git status` 体现为 ` D`。

---

## Task 5: README CHANGELOG

**Files:**
- Modify: `README.md`（在批次8 条目之后插入新条目）

- [ ] **Step 1: 定位锚点**

```powershell
Select-String -Path README.md -Pattern "A6-S2" | ForEach-Object { $_.LineNumber }
```

批次8 条目当前在第 134 行；新条目插在它之后（`- G5.5` 之前）。

- [ ] **Step 2: 插入一行 CHANGELOG**

内容（单行，`- ` 开头，与前批次同风格）：

```markdown
- A6-S2 批次6 修正（开发版，未发布）：**撤回 NPC 骨骼化，恢复原版整图**——实机反馈「NPC 容易和怪混淆」。根因不是配色接近，而是 `gen_npc_parts.py` 从 `gen_mon_humanoid_parts.py` 复用 `PART_ROWS`/`SIZES`/`TIERS`/`WEAPONS`，NPC 非武器部件与怪物/骑士**像素图完全相同**（同头盔、同护甲躯干、同手臂、同腿），只有色板+体型缩放不同；最糟 `npc_80` 维拉与 `mon_goblin_hunter` 连体型档、武器、绿色系都一致。**不存在 10 张原版 NPC 图**——旧整图只有 `npc_1/2/3/blacksmith` 四张 16×16 圆脸头像，楼层映射本就是 3+3+3+1 复用。本批利用既有「骨骼优先、静态图回落」路径（2D `game_scene.cpp:3161-3174`、HD2D `hd2d_scene_builder.cpp:982-1001`，懒建失败会缓存不重试）**零 C++ 改动**回退：移除 10 个 `npc_*` 白名单键（45→35）+ 测试期望集合同步，删除 50 分件 PNG + 10 骨架 JSON + 50 条 `sprites.json` 注册 + 164 行生成器（骨架契约本体在上游 `gen_player_knight_parts.py`/`gen_mon_humanoid_parts.py`，删 NPC 生成器不丢 rig 资产）。NPC 恢复 28×28 圆脸整图、失去 idle 呼吸动画，换来「一眼认出是人不是怪」；C++ 接线保留为休眠，日后用新造型低成本接回。门禁：ctest 68/68、World Validator 0/0、新增资产一致性自检（`skeleton_parts`↔磁盘、skeleton→part.file、白名单→skeleton/anim 三向交叉）、`src/` 零改动。范围外：NPC 专属新造型（需新写生成器）、HD2D 补 NPC 名条。
```

- [ ] **Step 3: 确认插入成功且批次6 原条目保留**

```powershell
Select-String -Path README.md -Pattern "A6-S2" | ForEach-Object { $_.LineNumber }
```

期望批次8 之后新增一行；批次6 原条目（记录「10 世界 NPC 骨骼化」）**保留不动**（历史记述）。

---

## Task 6: 桌面同步 + 用户验收 + 单提交

- [ ] **Step 1: 镜像桌面开发包**

```powershell
$dst = "C:\Users\HP\Desktop\Roguelike-CPP-3D版"
foreach ($d in "src","resources","tools","tests","docs","assets",".github","vendor") {
    robocopy "C:\Demo\roguelike_cpp\$d" "$dst\$d" /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
    "  $d exit $LASTEXITCODE"
}
foreach ($f in "CMakeLists.txt","README.md","CLAUDE.md","CMakePresets.json",".gitignore") {
    Copy-Item "C:\Demo\roguelike_cpp\$f" "$dst\$f" -Force
}
Copy-Item build\roguelike_cpp.exe "$dst\roguelike_cpp.exe" -Force
Copy-Item build\raylib.dll "$dst\raylib.dll" -Force
```

`/MIR` 会同步删除桌面包里的 60 个 npc 资产。校验：exe hash 一致；`saves/` 与 `3D模式.exe.lnk` 保留。

- [ ] **Step 2: 提请用户实机验收**

告知用户跑 `C:\Users\HP\Desktop\Roguelike-CPP-3D版\roguelike_cpp.exe`，验收点（F2/3/4/6/7/8/9/11/12/14 各找一名 NPC）：

1. **恢复圆脸小人**：NPC 是 28×28 圆脸，不再是护甲分件
2. **一眼区分人 vs 怪**：NPC 体型更小、圆脸无护甲，与怪明显不同
3. **对话正常**：靠近按 E 出对话框，肖像仍是旧整图
4. **2D 头顶名字仍在**（HD2D 仍无 NPC 名条，本批不补）
5. 30 怪 + 5 Boss 外观无变化

**必须等用户回复「通过」才能提交。**

- [ ] **Step 3: Code Review 自查（提交前）**

```powershell
git status --short src
git diff --stat
git status --short | Select-String "^\s*D" | Measure-Object | ForEach-Object { "deleted files: " + $_.Count }
```

期望：`src` 空；4 个修改 + 61 个删除（50 PNG + 10 skeleton + 1 generator）+ 1 个新增 plan 文档。

- [ ] **Step 4: 单提交**

```powershell
git add -A
git commit -m "revert(a6-s2): 撤回批次6 NPC骨骼化 - 恢复原版整图" -m "实机反馈 NPC 容易和怪混淆。根因：gen_npc_parts.py 从 gen_mon_humanoid_parts.py 复用 PART_ROWS/SIZES/TIERS/WEAPONS，NPC 非武器部件与怪物/骑士像素图完全相同（同头盔/护甲躯干/手臂/腿），只有色板+体型缩放不同；npc_80 维拉与 mon_goblin_hunter 连体型档+武器+绿色系都一致。不存在 10 张原版 NPC 图——旧整图只有 npc_1/2/3/blacksmith 四张 16x16 圆脸头像，楼层映射本就 3+3+3+1 复用。利用既有『骨骼优先、静态图回落』路径（2D game_scene.cpp:3161-3174、HD2D hd2d_scene_builder.cpp:982-1001，懒建失败会缓存不重试）零 C++ 改动回退：移除 10 个 npc_* 白名单键（45->35）+ animation_test 期望集合同步；删除 50 分件 PNG + 10 骨架 JSON + 50 条 sprites.json skeleton_parts 注册（230->180）+ 164 行 gen_npc_parts.py（骨架契约本体在上游 gen_player_knight_parts.py/gen_mon_humanoid_parts.py，删 NPC 生成器不丢 rig 资产）。NPC 恢复 28x28 圆脸整图、失去 idle 呼吸动画，换来一眼认出是人不是怪；C++ 接线保留为休眠，日后用新造型低成本接回。门禁：ctest 68/68、World Validator 0/0、新增资产一致性自检、src 零改动。范围外：NPC 专属新造型、HD2D 补 NPC 名条。实机验收通过。"
git log --oneline -2
```

- [ ] **Step 5: 收尾确认**

```powershell
git status --short
```

期望输出为空（工作树 clean）。

---

## 自检记录

- **spec 覆盖**：范围内 7 项 → Task 1（白名单+测试）、Task 2（删资产+生成器）、Task 3（删注册）、Task 4（门禁）、Task 5（CHANGELOG）、Task 6（同步+验收+提交）。§3 运行时行为表为对照说明，无需独立任务。范围外项（保留项）已写入 Global Constraints 与 Task 1 Step 4/Task 2 Step 4/Task 5 Step 3 的校验步骤。
- **无占位符**：所有代码块为可逐字执行的完整内容；Task 4 Step 1 的自检脚本按 5 条断言逐条列出（脚本本体在执行时写，属实现细节非设计占位）
- **类型/签名一致**：`load_actor_avatars_file` 键构造 `"npc_" + std::to_string(id)`（`game_scene.cpp:3029-3065`）与 spec 一致；`expected` 集合自比断言（`ASSERT_EQ(out->size(), expected.size())`）无需改表达式
- **与已批准 spec 无偏差**：spec §6 预估「修改 4 文件、删除 61 文件、新增 2 文档」与本计划 Task 1/2/3/5/6 一致（plan 文档本身是第 2 个新增文档）
