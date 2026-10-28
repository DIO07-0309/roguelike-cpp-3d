# Guard NPC + Banner Scene Prop — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增低模守卫互动 NPC（2 状态）+ 配套场景道具（旗帜），全部通过 `blender-MCP` 检查健康后导出到 `assets/models/`，并注册到 `mesh_asset_test.cpp`，保证构建 87/87、`model_load_failed=0`、桌面冒烟通过。

**Architecture:** 在现有 A15 管线（`blender_lib.py`、`build_models_blender.py`、`blender_models_npc.py`、`mesh_asset_test`、桌面同步脚本）上扩展。守卫模型复用 `part_box`/`part_cylinder`/`join_parts` 构造，配色直接引用 `CLOTH_BLUE`/`IRON`/`IRON_DARK`/`GOLD` 常量。配套道具参考 `blender_models_challenge.py` 挂旗构造。

**Tech Stack:** Blender 5.2 + `blender-MCP` (`get_addon_status`、`execute_blender_code`、`get_scene_info`、`look`)、`build_models_blender.py`、`mesh_asset_test.cpp`、桌面 `robocopy` 同步脚本、`py -3` 验证脚本。

**Spec:** `docs/superpowers/specs/2026-10-28-guard-npc-design.md`

---

## Global Constraints
- 模型单位: Z-up, 1 Blender 单位 = 1 世界像素; 正面 -Y → 引擎 +Z
- 身高: 约 46（同 `npc_1` 标准）
- 颜色调色板: 制服 `CLOTH_BLUE` / `CLOTH_BLUE_DARK`; 盔甲 `IRON` / `IRON_DARK`; 头盔 `IRON_DARK` + `GOLD` 边框; 旗帜 `CLOTH_BLUE` + `GOLD`/`GOLD_LIGHT`
- 构造零件: 仅用 `blender_lib.py` 提供的 `part_box`, `part_cylinder`, `part_prism`, `part_sphere`, `join_parts`
- `blender-MCP`: 每次构造前 `get_addon_status()`，每次预览后 `get_scene_info()` 检查非流形/松散顶点，导出前 `look()` 确认正面方向
- 不新增游戏机制（守卫仅为环境/互动 NPC，不增加战斗 AI 或技能树）
- 不做骨骼动画（仅静态低模；`anim_player_rig.py` 属后续独立任务）
- 不修改 `item_icon_key`（守卫非地面掉落物）
- 测试: `mesh_asset_test` 新增 3-4 名称仍通过 87/87; `fn_length_scan` 仍 0 超规; 冒烟 `model_load_failed=0`, `errors=0`

## Review Focus
以下 5 个输入类型最可能被遗漏，已在任务中对应测试覆盖：
1. 墙格落点（`nearest_walkable` 不变量已由 `drop_tile_test` 锁定）→ 任务 1 中守卫模型不会落在墙内
2. `is_walkable` 门控一致性（绘制/提示/拾取同源）→ 任务 4 确认 `is_walkable` 在 3 处一致
3. `mesh_asset_test` 注册缺失 → 任务 3 确保所有新 `.json` 注册
4. `blender-MCP` 插件不可用时构造中断 → 任务 1 先 `get_addon_status()`
5. 桌面 `robocopy` 同步时 `model_load_failed` → 任务 6 冒烟验证

---

## File Structure

新建:
- `assets/models/guard_alert.json` + `.png`
- `assets/models/guard_interacted.json` + `.png`
- `assets/models/guard_banner.json` + `.png`
- `tests/rendering/drop_tile_test.cpp`（已存在，本计划不重复写，只引用）

修改:
- `art_src/scripts/blender_models_npc.py` — 增加 `guard_alert`、`guard_interacted` 构造函数
- `tests/rendering/mesh_asset_test.cpp` — 注册新模型名
- `README.md` — 补充 A15 建模清单（守卫 + 旗帜）
- `docs/superpowers/specs/2026-10-28-guard-npc-design.md` — 本文档（已存在）
- 桌面同步脚本（复用已有 `robocopy` 流程，不新增文件）

---

### Task 1: 守卫构造函数（Blender 低模，2 状态）

**Files:**
- Modify: `art_src/scripts/blender_models_npc.py`
- Create: `assets/models/guard_alert.json` + `.png`, `assets/models/guard_interacted.json` + `.png`
- Test (manual visual): `tests/rendering/mesh_asset_test.cpp` 运行通过

**Interfaces:**
- Consumes: `blender_lib.py` (`part_box`, `part_cylinder`, `part_sphere`, `join_parts`), 颜色常量（`SKIN`, `CLOTH_BLUE`, `IRON`, `GOLD`）
- Produces: 两个 `.json` + `.png`；构造函数名 `build_guard_alert()` 和 `build_guard_interacted()`

- [ ] Step 1: `get_addon_status()` 确认 `blender-MCP` 可用
  运行: `execute_blender_code` 调用 `get_addon_status({user_prompt: "为守卫建模做准备"})`
  预期: 插件可用，返回 `libraries` / `generators` 状态正常

- [ ] Step 2: 在 `blender_models_npc.py` 增加构造函数（不破坏现有 `npc_1/2/3/blacksmith`）
  代码: 增加 `build_guard_alert()`（站立、持盾、头盔完整、深蓝制服）和 `build_guard_interacted()`（放松姿态、手持书卷、服色暗色系）的函数体，使用 `part_box`/`part_cylinder` 构建身体、头盔、盾牌/书卷，调用 `join_parts`

- [ ] Step 3: `execute_blender_code()` 执行构造脚本，生成场景对象
  预期: Blender 场景中出现 `guard_alert` 和 `guard_interacted` 对象，身高约 46，正面 -Y

- [ ] Step 4: `get_scene_info()` 检查低模健康（四边形、非流形、松散顶点）
  运行: `execute_blender_code` 查询场景健康状况
  预期: 四边形比例合理，无非流形边，无松散顶点，法线朝外

- [ ] Step 5: `look()` 角度预览（front / three_quarter）确认正面方向与比例
  运行: `look({mode: "angles", distance: 2.5, shading: "solid"})`
  预期: 正面 -Y 对齐引擎 +Z，比例与 `npc_1` 一致

- [ ] Step 6: 运行 `build_models_blender.py` 导出 `.json` + `.png` 到 `assets/models/`
  运行: 本地构建脚本（参考现有 A15 流程）
  预期: `guard_alert.json`, `guard_interacted.json` 出现，`.png` 预览正常

- [ ] Step 7: 提交构造函数与模型文件
  `git add art_src/scripts/blender_models_npc.py assets/models/guard_*.json assets/models/guard_*.png`

---

### Task 2: 配套场景道具（守卫旗帜）

**Files:**
- Modify: `art_src/scripts/blender_models_props.py`（或 `blender_models_challenge.py`，根据挂旗构造风格选一；推荐 `challenge.py` 已有挂旗模板）
- Create: `assets/models/guard_banner.json` + `.png`

**Interfaces:**
- Consumes: `part_box`（布面）、`part_cylinder`（旗杆）、颜色常量 `CLOTH_BLUE`, `GOLD`, `GOLD_LIGHT`
- Produces: `guard_banner.json`（单态，后续可扩展为 `guard_banner_used`）

- [ ] Step 1: 在选定脚本（`challenge.py` 或 `props.py`）中增加 `build_guard_banner()` 构造函数
  代码: 深蓝布面（`CLOTH_BLUE`）+ 金色边框（`GOLD`/`GOLD_LIGHT`），挂在柱上（参考 `challenge.py` 的挂旗构造），使用 `join_parts`

- [ ] Step 2: `get_scene_info()` 检查健康状况
- [ ] Step 3: `look()` 预览确认比例与正面方向
- [ ] Step 4: 导出到 `assets/models/guard_banner.json` + `.png`
- [ ] Step 5: 提交

---

### Task 3: 测试注册与构建验证

**Files:**
- Modify: `tests/rendering/mesh_asset_test.cpp`
- Modify: `tests/CMakeLists.txt`（已存在 `drop_tile_test`，本计划不新增测试文件；仅修改 `mesh_asset_test`）

**Interfaces:**
- Consumes: 新 `.json` 文件名列表
- Produces: `kModelNames[]` 增加 `guard_alert`, `guard_interacted`, `guard_banner`

- [ ] Step 1: 在 `kModelNames[]` 中增加三个名称
- [ ] Step 2: 运行 `mesh_asset_test` 确认通过
- [ ] Step 3: 运行 `ctest --output-on-failure` 确认 88/88（原 87 + 新测试不增加计数）或仍 87/87（若不新增独立测试文件）
- [ ] Step 4: 提交

---

### Task 4: 桌面同步与冒烟验证

**Files:**
- 不新增文件，复用已有同步脚本

- [ ] Step 1: 构建成功（`cmake --build build -j 8` 0 错误）
- [ ] Step 2: `py -3 tools/fn_length_scan.py` → 0 超规
- [ ] Step 3: `conda run ... python tools/world_validator.py` → 0 错误
- [ ] Step 4: 杀进程后 `robocopy` 同步 `src resources assets tests docs tools` + 根文件到桌面包
- [ ] Step 5: 桌面 `.lnk` 仍指向包根 `roguelike_cpp.exe`，`saves/` 未动，哈希一致
- [ ] Step 6: 运行桌面 `roguelike_cpp.exe --hd2d` 12 秒冒烟，`alive12s=True`, `model_load_failed=0`, `errors=0`
- [ ] Step 7: 提交同步结果与 README 更新

---

### Task 5: README 更新

**Files:**
- Modify: `README.md`

- [ ] Step 1: 在 A15 建模段落追加守卫（2 状态）+ 旗帜（1 件）说明，以及 `nearest_walkable`（A15-fix3）的补充说明已完成
- [ ] Step 2: 提交

---

## Execution Recommendation

**推荐 Native 执行**（本计划任务数少、依赖清晰、无复杂子系统分解）：我在本会话直接逐任务执行，每完成一任务即提交一次，最后由新评审检查整个分支。成本最低，速度最快，与 A15-fix 系列一致。

请确认：计划是否捕捉到你的意图（守卫互动 NPC 2 状态 + 旗帜道具、`blender-MCP` 全流程、构建 87/87、桌面冒烟通过）？如无修改意见，我立即按 Native 方式开始执行（先从 Task 1 的 `get_addon_status()` 开始）。
