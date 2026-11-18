# P3 道具精修 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 清理 A22 遗留构建脚本残留，并把 8 件通用道具（长枪/双截棍/药水×2/匕首/剑/护符/弩）从 204–592 面精修到 ≥1000 面/件，配独立资产闸门。

**Architecture:** 8 件通用道具由 `blender_models_items.py` 的 builder 函数生成，经 `build_models_blender.py` 的 BUILDERS 调用、`blender_lib.export_json`（协议 A）导出。精修直接在各 builder 的 `parts` 列表追加细节基元（提高 cylinder segments / sphere rings + 特征件），不放大占地 bbox。新增独立闸门 `tools/check_ground_item_assets_p3.js` 校验 schema/finite/面数≥1000/bbox 不超基线。

**Tech Stack:** Blender 5.2.1 + Blender MCP；`blender_lib`（protocol A）；C++17 + raylib；gtest；node 闸门。

**Spec:** `docs/superpowers/specs/2026-11-18-p3-refinement-design.md`（审查 `docs/superpowers/specs/2026-11-18-p3-asset-audit.md`）

## Global Constraints

- 协议 A：`export_json` 做 `(x,y,z)→(x,z,-y)`，Blender 正面 -Y → 引擎 +Z 朝相机。
- schema `{name, vertices[x,y,z,r,g,b], faces[i,j,k]}`；明暗烘顶点色；WRITE_NORMALS=False。
- 面数硬门槛：8 件精修道具每件 **≥1000 三角面**（faces.length），无上限（保持低模，建议 ≤2000）。
- 占地 bbox 不得超出精修前基线（每轴 min/max 均在基线内），避免穿墙/盖过相邻格。
- 8 道具基线面数：长枪 204 / 双截棍 296 / 药水红蓝 272 / 匕首 332 / 剑 440 / 护符 496 / 弩 592。
- 不改 `kGroundItemModels` 映射、不改 mesh_asset_test 契约、不改 gameplay/RNG/存档。
- 禁用 MCP 工具：`get_viewport_screenshot` / `get_world_state_snapshot` / `drain_human_activity`；预览走 `look()`。
- 每次 MCP 调用 `user_prompt` 填："可以继续"。
- 全量门：ctest 90/90、fn_length 0 超规、world_validator 通过、mesh_asset_test 全绿。

## Review Focus

- **A22 残留生成回来**：`build_models_blender.py` 若仍有 item_armor/item_robe 条目，重跑 build_all 会重新生成已删资产 → Task 1 验收 grep 0 命中。
- **面数不达标**：长枪/药水基线极低，细节加不够 → Task 3-6 的闸门 ≥1000 逐件检查。
- **占地 bbox 放大**：细节伸出原包围盒 → Task 7 闸门 bbox 不超基线。
- **schema/name 破坏**：导出格式错 → 闸门 schema/name 检查 + mesh_asset_test。
- **预览全家福未更新**：`blender_preview_items.png` 仍是旧造型 → Task 7 重生成 + 抽样审图。

---

### Task 1: 清理 A22 构建脚本残留

**Files:**
- Modify: `art_src/scripts/build_models_blender.py:78-79`（删两条 BUILDERS）
- Modify: `art_src/scripts/blender_models_items.py:93-151`（删 build_armor / build_robe 函数）
- Test: grep 校验

**Interfaces:**
- Consumes: 审查报告 §3（残留位置）。
- Produces: 干净构建脚本，build_all 不再生成 item_armor/item_robe。

- [ ] **Step 1: 写失败校验（grep）**

写临时校验：对两个脚本源文本断言不含 `build_armor` / `build_robe` / `"item_armor":` / `"item_robe":`。
Run → Expected: FAIL（4 项命中）。

- [ ] **Step 2: 删除残留**

从 `build_models_blender.py` 删 `"item_armor": item_models.build_armor,` 与 `"item_robe": item_models.build_robe,`；从 `blender_models_items.py` 删 build_armor（93-121）与 build_robe（124-151）两函数。

- [ ] **Step 3: 校验通过**

Run 步骤 1 校验 → Expected: PASS（0 命中）。`python -c "import ast; ast.parse(open(...).read())"` 两脚本语法 OK。

- [ ] **Step 4: 提交**

Commit: `chore(art): remove stale item_armor/item_robe builders`

---

### Task 2: 建立 P3 资产闸门与基线表

**Files:**
- Create: `tools/check_ground_item_assets_p3.js`
- Create: `tools/ground_item_baselines_p3.json`
- Test: 闸门脚本自身

**Interfaces:**
- Consumes: 8 道具精修前 JSON（当前磁盘基线）。
- Produces: 闸门命令 `node tools/check_ground_item_assets_p3.js`，校验 8 件 schema/name/finite/face≥1000/bbox≤基线。

- [ ] **Step 1: 生成基线表**

写 node 脚本读 8 件 `assets/models/item_*.json`（不含 armor），输出 `tools/ground_item_baselines_p3.json`：`{name: {faces, bbox: {xmin,xmax,ymin,ymax,zmin,zmax}}}`。

- [ ] **Step 2: 写闸门脚本**

`check_ground_item_assets_p3.js`：读基线表，对 8 件校验 schema 键 `faces,name,vertices` / `d.name==文件名` / 顶点 6 数且 finite / `faces.length>=1000` / 各轴 bbox 分量不超基线（容差 0.01）。输出每件 PASS/FAIL + 总计，失败 exit 1。

- [ ] **Step 3: 验证 RED**

Run `node tools/check_ground_item_assets_p3.js` → Expected: FAIL（8 件面数均 <1000）。

- [ ] **Step 4: 提交**

Commit: `test(art): add P3 item asset gate (face>=1000, bbox<=baseline)`

---

### Task 3: 精修长枪 + 双截棍（最低面数两件）

**Files:**
- Modify: `art_src/scripts/blender_models_items.py`（build_spear / build_nunchaku 的 parts 列表）
- Overwrite: `assets/models/item_spear.json`, `item_nunchaku.json`
- Update: `art_src/preview/blender_preview_items.png`

**Interfaces:**
- Consumes: blender_lib part_* 基元；协议 A 导出；`build_models_blender.py` BUILDERS。
- Produces: 2 件 ≥1000 面、bbox 不超基线。

- [ ] **Step 1: 追加细节**

在 build_spear 追加：枪头刃纹（part_prism ×2）、缨（part_cylinder 高段）、握把缠带（part_cylinder 多段）；提高 shaft/socket cylinder 的 vertices 8→12。在 build_nunchaku 追加：端头配重（part_cylinder vertices 12）、链节（part_torus major 12/minor 6）、棍身纹（part_cylinder 段）。**所有新增顶点必须在原 bbox 内**（参考基线表）。

- [ ] **Step 2: 经 Blender MCP 重建这 2 件**

用 execute_blender_code 执行脚本里对应 builder，经 export_json 覆盖 2 个 JSON（user_prompt="可以继续"）。

- [ ] **Step 3: 闸门校验**

Run `node tools/check_ground_item_assets_p3.js` → Expected: 这 2 件 PASS（其余仍 FAIL 因未精修）。

- [ ] **Step 4: 审图**

`look()` 看这 2 件（target 用对象名），确认正面朝相机、无穿模。

- [ ] **Step 5: 提交**

Commit: `art(item): refine spear and nunchaku to 1000+ faces`

---

### Task 4: 精修药水红/蓝 + 匕首 + 剑

**Files:**
- Modify: `art_src/scripts/blender_models_items.py`(_potion / build_dagger / build_sword)
- Overwrite: `assets/models/item_potion_red.json`, `item_potion_blue.json`, `item_dagger.json`, `item_sword.json`
- Update: `blender_preview_items.png`

**Interfaces:** 同 Task 3。
**Produces:** 4 件 ≥1000 面、bbox 不超基线。

- [ ] **Step 1: 追加细节**

药水：瓶身 segment 8→12、标签浮雕（part_box 薄片）、瓶塞纹（part_cylinder 12）；匕首/剑：护手雕纹（part_box 对称件）、刃脊（part_prism）、握把缠带（part_cylinder 多段）；提高 blade segments。所有顶点留在 bbox 内。

- [ ] **Step 2: MCP 重建 4 件**

- [ ] **Step 3: 闸门校验（4 件 PASS）**

- [ ] **Step 4: `look()` 审图**

- [ ] **Step 5: 提交** `art(item): refine potions, dagger and sword`

---

### Task 5: 精修护符 + 弩

**Files:**
- Modify: `art_src/scripts/blender_models_items.py`(build_charm / build_crossbow)
- Overwrite: `assets/models/item_charm.json`, `item_crossbow.json`
- Update: `blender_preview_items.png`

**Interfaces:** 同 Task 3。
**Produces:** 2 件 ≥1000 面、bbox 不超基线。

- [ ] **Step 1-5:** 重复 Task 3 步骤（护符加宝石切面 part_sphere 高段+链节 part_torus；弩加弓臂层次 part_box 对称件+扳机 part_cylinder）。提交 `art(item): refine charm and crossbow`

---

### Task 6: 重生成预览 + 全量验收 + 文档同步

**Files:**
- Update: `art_src/preview/blender_preview_items.png`
- Modify: `CHANGELOG.md`（P3 条目）
- Test: 全量门

**Interfaces:**
- Consumes: Task 1-5 全部产物。
- Produces: 可交付的 P3。

- [ ] **Step 1: 重生成道具全家福预览**

`build_models_blender.py` 的 preview 部分重跑，输出 `blender_preview_items.png`；抽样审图确认 8 道具造型更新。

- [ ] **Step 2: 全量闸门**

Run `node tools/check_ground_item_assets_p3.js` → Expected: 8/8 PASS。`ctest` 90/90；`node tools/check_ground_item_assets.js --all` 150/150（A22 闸门不退化）；fn_length 0；world_validator 通过。

- [ ] **Step 3: CHANGELOG + 桌面同步**

写 P3 条目（清理残留、8 道具面数前后、闸门）；同步桌面运行包并哈希校验。

- [ ] **Step 4: 提交并推送**

Commit: `docs+art: P3 item refinement complete`；`git push origin master`。

## Self-Review

- Spec coverage：§2 目标（8 道具精修+清理残留）→ Task 1-5；§3 约束（协议 A/bbox/面数）→ Global Constraints + 闸门；§4 优先级 → Task 1 / 3-5；§6 拍板 → 全任务。覆盖完整。
- Review Focus：5 条均有对应任务测试（残留 grep / 面数闸门 / bbox 闸门 / schema / 预览重生成）。
- Type consistency：闸门脚本名、基线表名、导出路径贯穿一致。
- Proportion：计划以契约和步骤为主，无大段代码。
