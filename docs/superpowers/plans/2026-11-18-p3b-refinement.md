# P3-2 地标 / 护甲细节微调 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 对 9 个地标 + 8 种护甲做"补明确缺口"的细节微调，所有新增顶点落在原 bbox 内，面数严格高于基线。

**Architecture:** 两批独立：地标由 `blender_models_landmarks.py` 的 `LANDMARK_BUILDERS`（9 builder）经 `export_landmark(key)` 导出；护甲由 `blender_models_armors.py` 的 `ARMOR_BUILDERS`（7/8 共用 `_base_chest` 骨架 + robe 独立）经 `export_armor(key)` 导出。两脚本都自带 `export_<type>(key)` / `preview_one(key)`。新增独立闸门 `tools/check_p3b_assets.js` + 基线表（17 件的 bbox + faces），校验 bbox 不超基线且 faces 严格 > 基线。

**Tech Stack:** Blender 5.2.1 + Blender MCP；`blender_lib`（协议 A）；C++17 + raylib；gtest；node 闸门。

**Spec:** `docs/superpowers/specs/2026-11-18-p3b-refinement-design.md`（审查 `docs/superpowers/specs/2026-11-18-p3b-asset-audit.md`）

## Global Constraints

- 协议 A：`export_json` 做 `(x,y,z)→(x,z,-y)`；JSON y=高度、z=纵深。
- schema `{name, vertices[x,y,z,r,g,b], faces}`；明暗烘顶点色。
- 微调强度：只补审查列出的明确缺口（docstring 承诺未建模 / 素面部位），不设面数目标、不为凑数字。
- **面数严格 > 基线**（用户拍板）：每件精修后 faces 必须大于精修前基线（证明确实加了细节）。
- **bbox 各轴不超基线**（容差 0.01）；贴地 y_min=0 不变（FeetAtGroundLevel 契约 ±0.1，robe 基线 −0.082 保持）。
- 低段数采样经验：半径敏感件（肩球/圆盘/瓶身）保持原段数，靠 bbox 内部装饰补细节。
- 不改资产名（`kGroundItemModels` / `item.cpp` 映射依赖）、不改配色常量、不改 `_base_chest` 骨架尺寸。
- 17 张预览全部重生成 + 审图；完成后同步桌面包并哈希校验。
- 禁用 MCP 工具：`get_viewport_screenshot` / `get_world_state_snapshot` / `drain_human_activity`；预览走 `look()`。
- 每次 MCP 调用 `user_prompt` 填用户原话："P3-2（9 地标 + 8 护甲细节微调）"。
- 全量门：ctest 90/90、mesh_asset_test 全绿（AllNineLandmark/AllEightArmor）、fn_length 0、world_validator 通过、P3 闸门 8/8 与 A22 闸门 150/150 不退化。

## Review Focus

- **面数未增加**：只加装饰但面数没超基线 → Task 闸门 faces>baseline（A22 front_detail 同源思路）。
- **bbox 撑破**：低段数采样经验失效 → 闸门 bbox 六向 + 审图。
- **贴地契约破坏**：新细节放到 y<−0.1 → mesh_asset_test FeetAtGroundLevel + 闸门 y_min 契约（robe 例外基线 −0.082）。
- **命名/映射失效**：改资产名 → 不改名的硬约束 + ctest 契约。
- **预览未更新**：17 张预览还是旧的 → Task 分批改生成 + 全家福审图。

---

### Task 1: 建立 P3b 闸门与基线表

**Files:**
- Create: `tools/check_p3b_assets.js`
- Create: `tools/p3b_baselines.json`
- Test: 闸门脚本自身

**Interfaces:**
- Consumes: 17 件精修前 JSON（当前磁盘基线）。
- Produces: `node tools/check_p3b_assets.js` → 校验 17 件 schema/name/finite/bbox≤基线/faces>基线。

- [ ] **Step 1: 生成基线表**

写 node 脚本读 9 个 `lm_*.json` + 8 个 `item_armor_*.json`，输出 `tools/p3b_baselines.json`：`{name: {faces, bbox:{xmin,xmax,ymin,ymax,zmin,zmax}}}`。

- [ ] **Step 2: 写闸门脚本**

`check_p3b_assets.js`：读基线表，对 17 件校验 schema 键 `faces,name,vertices` / `d.name==文件名` / 顶点 6 数且 finite（含颜色 v[3..5]）/ `faces > baseline.faces`（严格）/ 各轴 bbox 分量不超基线（容差 0.01；robe 的 y_min 基线 −0.082 按基线本身判定）。输出每件 PASS/FAIL + 总计，失败 exit 1。

- [ ] **Step 3: 验证 RED**

Run `node tools/check_p3b_assets.js` → Expected: FAIL（17 件 faces 均 == 基线，不满足严格 >）。

- [ ] **Step 4: 提交** `test(art): add P3b gate (bbox<=baseline, faces>baseline)`

---

### Task 2: 地标缺口补齐 — 志愿服务群（fire_pillar / forge_ruins / lava_rift）

**Files:**
- Modify: `art_src/scripts/blender_models_landmarks.py`（build_fire_pillar / build_forge_ruins / build_lava_rift）
- Overwrite: `assets/models/lm_{fire_pillar,forge_ruins,lava_rift}.json`
- Update: 对应 3 张 `landmark_preview_*.png`

**Interfaces:**
- Consumes: `LANDMARK_BUILDERS` / `export_landmark(key)` / `preview_one(key)`。
- Produces: 3 件 bbox 内、faces>基线。

- [ ] **Step 1: 补缺口**

fire_pillar：岩座加分层岩块、火焰锥间加火舌小锥（保持 16×16 占地、顶 36.5）。
forge_ruins：炉体加烟囱/炉口框、铁砧加侧面纹、加散落工具小件（x±15/z±11 内、顶 13.5）。
lava_rift：地面加龟裂薄片、熔岩加发光细脉、加小泡（x±15/z±11 内、顶 15）。
半径敏感件保持原段数，细节用 bbox 内部小件。

- [ ] **Step 2: MCP 重建 3 件**

`execute_blender_code` 调 `export_landmark(key)` 覆盖 JSON（user_prompt 填原话）。确保 reload 最新脚本。

- [ ] **Step 3: 闸门校验**

`node tools/check_p3b_assets.js` → Expected: 这 3 件 PASS（faces>基线、bbox 内）。

- [ ] **Step 4: 审图**

`preview_one(key)` 重渲染 3 张 + `look()` 确认正面/无穿模/未超包络。

- [ ] **Step 5: 提交** `art(landmark): refine fire_pillar, forge_ruins, lava_rift`

---

### Task 3: 地标缺口补齐 — 监牢群（broken_cell / torture_chamber / collapsed_tunnel）

**Files:** 同 Task 2，3 件监牢地标 + 各自预览。
**Interfaces:** 同 Task 2。
**Produces:** 3 件 bbox 内、faces>基线。

- [ ] **Step 1: 补缺口**

broken_cell：补墙上抓痕（多条斜细条）、铁栏锈环/端帽。
torture_chamber：球串链改/补链环（torus）、加第二件刑具。
collapsed_tunnel：门楣做断裂（分成两段错位）、加碎石小块、支撑木绑绳。注意 x 最小 −16.41 是斜梁既有边界，勿再向外扩。

- [ ] **Step 2-5:** 同 Task 2（MCP 重建 → 闸门 → 审图 → 提交 `art(landmark): refine prison landmarks`）

---

### Task 4: 地标缺口补齐 — 虚空群（floating_altar / void_crack / ancient_gate）

**Files:** 同 Task 2，3 件虚空地标 + 各自预览。
**Interfaces:** 同 Task 2。
**Produces:** 3 件 bbox 内、faces>基线。

- [ ] **Step 1: 补缺口**

floating_altar：补幽光符文（小发光条）、空间扭曲环（torus，半径守 19×19 边界）。
void_crack：裂口加锯齿边（小斜片）、多层内辉片。
ancient_gate：符文加刻纹（小发光条叠在现有 rune box 上）、门楣浮雕。守 26×11 占地（z 非对称）。

- [ ] **Step 2-5:** 同 Task 2（提交 `art(landmark): refine void landmarks`）

---

### Task 5: 护甲素面补齐 — 共用骨架 7 件（leather/chain/iron/scale/plate/rune/ice）

**Files:**
- Modify: `art_src/scripts/blender_models_armors.py`（7 个 build_armor_* 的特征件部分，**不动 `_base_chest`**）
- Overwrite: 7 个 `item_armor_*.json`
- Update: 7 张 `armor_preview_*.png`

**Interfaces:**
- Consumes: `_base_chest` / `ARMOR_BUILDERS` / `export_armor(key)` / `preview_one(key)`。
- Produces: 7 件 bbox 内、faces>基线。

- [ ] **Step 1: 补素面**

各 builder 在专属特征件后追加 bbox 内部装饰：leather 领下/肩素面加缝线短条；chain 上胸/腹部加铆环（沿用环阵样式）；iron 胸板加刻槽（细长 box）；scale 领下/肋侧加鳞片；plate 胸甲加分块刻槽 + 腹部装饰；rune 符文加次级笔画 + 核心环；ice 胸前加垂冰/霜纹。所有装饰落在各件 bbox 内（多数 x±10~12、高 16.2、深[-3.8,4.3]；ice 顶 20、plate 19），不碰肩球半径段数。

- [ ] **Step 2: MCP 重建 7 件**

`execute_blender_code` 调 `export_armor(key)`（护甲脚本自带 REPO），reload 最新脚本。

- [ ] **Step 3: 闸门校验**

`node tools/check_p3b_assets.js` → Expected: 7 件 PASS。

- [ ] **Step 4: 审图**

`preview_one(key)` 重渲染 7 张 + `look()` 审图。

- [ ] **Step 5: 提交** `art(armor): refine 7 shared-skeleton armors`

---

### Task 6: 护甲素面补齐 — robe（独立骨架）

**Files:** `blender_models_armors.py`(build_armor_robe) / `item_armor_robe.json` / `armor_preview_robe.png`
**Interfaces:** 同 Task 5。
**Produces:** robe bbox 内、faces>基线（基线 y_min=−0.082 保持）。

- [ ] **Step 1-5:** 补兜帽内衬/面影、前襟次级发光纹、袖/下摆装饰、腰饰；MCP 重建 → 闸门（robe y_min 守 −0.082 基线）→ 审图 → 提交 `art(armor): refine robe`

---

### Task 7: 全量验收 + 文档 + 桌面同步

**Files:** Modify `CHANGELOG.md`（P3b 条目）；17 张预览（若分批量已生成则核对）。
**Interfaces:** Consumes Task 1-6 全部产物；Produces 可交付 P3b。

- [ ] **Step 1: 全量闸门**

`node tools/check_p3b_assets.js` → Expected: 17/17 PASS。`ctest` 90/90；`node tools/check_ground_item_assets_p3.js` 8/8；`node tools/check_ground_item_assets.js --all` 150/150；fn_length 0；world_validator 通过。

- [ ] **Step 2: 预览核对**

17 张预览均已重生成（Task 2-6 完成），抽查全家福无穿模。

- [ ] **Step 3: CHANGELOG + 桌面同步**

写 P3b 条目（17 件缺口清单、面数前后、bbox 契约、闸门）；同步桌面包并哈希校验。

- [ ] **Step 4: 提交并推送** `docs+art: P3b landmark and armor refinement complete`；`git push origin master`。

## Self-Review

- Spec coverage：§2 目标（17 件补缺口）→ Task 2-6；§3 约束（协议 A/bbox/低段数经验）→ Global Constraints + 闸门；§4 批次 → Task 2-4（地标）/ 5-6（护甲）；§0 拍板 5 项 → 全部任务。覆盖完整。
- Review Focus：5 条均有对应任务测试（faces>baseline 闸门 / bbox 闸门 / FeetAtGroundLevel / 不改名 / 预览重生成）。
- Type consistency：`export_landmark` / `export_armor` / `preview_one` / 闸门脚本名 / 基线表名贯穿一致。
- Proportion：计划以契约和步骤为主，细节内容由 builder 级决策，无大段代码。
