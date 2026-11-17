# 3D 怪物精修（P0+P1 样板）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 清理 4 个冗余模型资产，并用 Blender MCP 精修 orc/slime/golem/fire_imp 四族怪物共 20 件低模部件，使面数达到 1200–2000/件（上限 4000），作为后续铺开其余 26 种的样板。

**Architecture:** 精修采用 per-monster refinement：`blender_models_monsters.py` 保留 4 个共享 `FAMILIES` 模板不动，新增按怪物名索引的 `REFINEMENTS` 配置，`build_parts(name)` 在共享模板结果上叠加该怪物的额外细节。这样 P1 产物可复现、其余 26 种零影响。模型经 Blender MCP 的 `execute_blender_code` 在用户实时 Blender 中构建并导出 JSON，预览走 `look()`，绝不调用被禁用的截图类工具。

**Tech Stack:** Blender 5.2.1 + Blender MCP 插件 1.8（协议 13，已验证连接）；Python 脚本 `art_src/scripts/blender_lib.py` / `blender_models_monsters.py` / `blender_scene.py`；C++17 + raylib 5；gtest；node（资产 JSON 校验）。

**Spec:** `docs/superpowers/specs/2026-11-17-modeling-refinement-design.md`（含审查报告依赖 `docs/superpowers/specs/2026-11-17-modeling-audit.md`）

## Global Constraints

- 导出 schema 固定 `{"name", "vertices":[[x,y,z,r,g,b]...], "faces":[[i,j,k]...]}`；顶点 6 数、xyz 三位小数、rgb 0–255；不写 normals。
- 角色部件协议 B：Blender `(x,y,z)→mesh(x,-y,z)`；Blender 内正面 +Y，导出后 mesh.y 负=朝相机；局部原点=骨骼 pivot；沿用 `ANCHORS`（leg=20/arm=35/weapon=30/torso=26/head=37）。
- 明暗烘焙表固定：front 1.00 / back 0.45 / left·right 0.62 / top 1.55 / bottom 0.30；`COLOR_JITTER=7`；Roughness 0.72。
- 面数目标 1200–2000/件，硬上限 4000；小于 1200 的部件（如 slime weapon 120 面）须提升。
- 轮廓不得超出当前骨架 pivot 与部件包围盒包络；外扩部件必须 `look()` 审图 + 实机截图确认不穿模不裁切。
- 禁用 MCP 工具：`get_viewport_screenshot`、`get_world_state_snapshot`、`drain_human_activity`；`look(image=)` 路径用正斜杠。
- 每次 MCP 调用 `user_prompt` 填用户原话："先开始全面检查当前所有建模，审查完毕后立项开始按优先级和复杂度用blender-MCP建模"。
- 不改 gameplay/RNG/存档/骨架/动画/2D 素材。
- 全量门：ctest 90/90、`python tools/fn_length_scan.py` 0 超规、`python tools/world_validator.py` 通过。

## Review Focus

- **资产删除后测试清单悬空**：删 4 个模型名后 `mesh_asset_test.kModelNames` 若不同步，`AllModelsLoad` 会失败 → Task 1 的测试步骤覆盖。
- **精修脚本重跑污染其他 26 种怪物**：若误改共享 `FAMILIES` 模板，`export_all()` 会静默改掉非试点资产 → Task 2 的非试点字节不变校验覆盖。
- **部件导出朝向错误（协议 A/B 混用）**：若用错 `export_json`，正面会背朝相机 → Task 3 的 `look()` 正面审阅 + 实机截图覆盖。
- **穿模/裁切**：外扩部件超出 pivot 包络会穿模 → Task 3 的装配 `look()` + 实机截图覆盖。
- **面数不达标或超上限**：精修后过小/过大 → Task 3 的 JSON 面数校验覆盖。

---

### Task 1: 资产卫生 — 删除 4 个冗余模型并同步测试清单

**Files:**
- Delete: `assets/models/item_armor.json`
- Delete: `assets/models/item_robe.json`
- Delete: `assets/models/terrain_wall_prison_v0.json`
- Delete: `assets/models/terrain_wall_volcano_v0.json`
- Modify: `tests/rendering/mesh_asset_test.cpp:26,44-45`（从 `kModelNames` 移除这 4 个名字）
- Test: `tests/rendering/mesh_asset_test.cpp`

**Interfaces:**
- Consumes: 审查报告 §3（孤儿与几何重复结论）。
- Produces: 干净的资产基线；`mesh_asset_test.kModelNames` 与磁盘资产一一对应。

- [ ] **Step 1: 写失败测试 — 从 `kModelNames` 移除 4 个名字**

在 `tests/rendering/mesh_asset_test.cpp:26` 把 `"item_armor", "item_robe",` 从数组删除；在 `:44-45` 把 `"terrain_wall_prison_v0",`、`"terrain_wall_volcano_v0",` 删除（保留 `_v1/_v2` 和 void 的 `_v0`）。

- [ ] **Step 2: 删除 4 个资产文件**

用 `Remove-Item` 删除上述 4 个 JSON。注意 `terrain_wall_void_v0.json` 是近似变体，**不删**。

- [ ] **Step 3: 运行 mesh_asset_test 验证通过**

Run: `cmake --build build --target mesh_asset_test -j 8` 然后 `build\tests\mesh_asset_test.exe`（工作目录 = 仓库根）
Expected: 全绿；尤其 `AllModelsLoad` 通过（清单已同步）。

- [ ] **Step 4: 全量验证 + 提交**

Run: `ctest --test-dir build --output-on-failure`（90/90）；`git status --short` 确认只有预期改动。
Commit: `chore: remove 4 redundant model assets and sync mesh_asset_test`

---

### Task 2: 建立 per-monster refinement 脚本架构（不改共享模板）

**Files:**
- Modify: `art_src/scripts/blender_models_monsters.py`（在 `MONSTERS` 后、`FAMILIES` 前新增 `REFINEMENTS = {}` 与 `_refine(name, parts)`；改 `build_parts(name):999-1004` 末尾调用 `_refine`；改 `preview_one(name):1027` 同样调用）
- Test: 资产 JSON 一致性校验（node 脚本，见步骤）

**Interfaces:**
- Consumes: `build_parts(name)` 返回 `{slot: bpy.Object}`，slot ∈ SLOTS（head/torso/arm/leg/weapon）。
- Produces: `REFINEMENTS[name]`（dict，键为 slot，值为追加细节的描述结构）、`_refine(name, parts)`（就地修改 parts 字典）；`build_parts` 与 `preview_one` 在返回/装配前调用 `_refine`。

- [ ] **Step 1: 加空 refinement 挂点（失败测试先行）**

写临时校验脚本（node，stdin 传入）：对 `blender_models_monsters.py` 源文本断言存在 `REFINEMENTS`、`_refine`、`_refine(name, parts)` 在 `build_parts` 内被调用。
Run 校验 → Expected: FAIL（符号不存在）。

- [ ] **Step 2: 实现挂点**

在 `blender_models_monsters.py` 加：
```python
REFINEMENTS = {}   # name -> {slot: [extra_part_spec, ...]}

def _refine(name, parts):
    spec = REFINEMENTS.get(name)
    if not spec:
        return parts
    # 对 spec 中每个 slot 的追加件调用 blender_lib.part_* 生成对象并 join 进 parts[slot]
    ...
    return parts
```
`build_parts(name)` 末尾由 `return {slot: builders[slot](name, c) for slot in SLOTS}` 改为先构造 `out = {...}` 再 `return _refine(name, out)`。`preview_one` 的装配循环在取 `builders[slot](name, c)` 处同步走 `_refine`（可复用 `build_parts(name)` 的结果按 slot 取对象，注意 ANCHORS 里 leg/arm 各两条骨需新建对象，沿用 `preview_one` 现有逻辑）。
关键：4 个 `FAMILIES` 模板与 `MONSTERS` 配色表**完全不动**。

- [ ] **Step 3: 验证挂点 + 非试点字节不变**

Run 步骤 1 的校验脚本 → Expected: PASS。
再用 Blender MCP `execute_blender_code` 加载脚本并调用 `build_parts("orc")`（此时 REFINEMENTS 为空）→ 导出 5 件到临时路径，与 `assets/models/mon_orc_part_*.json` 逐字节比对 → Expected: 完全一致（证明挂点不改变现有产物）。

- [ ] **Step 4: 提交**

Commit: `refactor(art): add per-monster refinement hook without touching family templates`

---

### Task 3: 精修 orc 族（humanoid 代表，5 件）

**Files:**
- Modify: `art_src/scripts/blender_models_monsters.py`（`REFINEMENTS["orc"]` 配置）
- Overwrite: `assets/models/mon_orc_part_{head,torso,arm,leg,weapon}.json`
- Update: `art_src/preview/monster_preview_orc.png`
- Test: 资产 JSON 面数/schema 校验

**Interfaces:**
- Consumes: Task 2 的 `REFINEMENTS` / `_refine`；`blender_lib.part_*` 基元；`MONSTERS["orc"]` 配色。
- Produces: orc 精修后的 5 件 JSON + 预览图；面数 1200–2000/件（≤4000）。

- [ ] **Step 1: 定义 orc 精修配置并生成**

在 `REFINEMENTS["orc"]` 为 5 槽各写追加细节（建议：torso 加胸甲分层+肩扣；head 加更清晰的面部/头盔层次；arm 加护腕；leg 加护膝；weapon 加斧刃层级与握柄细节）。细节对象用 `blender_lib.part_box/cylinder/prism` 构建，坐标须落在 ANCHORS 包络内。

- [ ] **Step 2: 经 Blender MCP 导出**

用 `execute_blender_code` 在实时 Blender 执行脚本的 `export_monster("orc")`（user_prompt 填用户原话）。导出后用 node 校验 5 个 JSON：schema 键、面数区间、顶点无 NaN、name 与文件名一致。
Expected: 5 件全部 1200–2000 面（≤4000）。

- [ ] **Step 3: 审图**

用 `look(mode="angles", target=[5 件对象名], shading="material", max_size=768)` 看正面/3q/侧面。确认：正面朝相机、无穿模、轮廓未超包络。不满意则改 REFINEMENTS 重导出。

- [ ] **Step 4: 实机验收**

`cmake --build build -j 8` 后跑 `--hd2d` 截图（参照 A21 流程：`--autocontinue --goto-floor 1 --autoshot 120 --hidwin`），确认 orc 在游戏中可见、风格与 Boss/玩家统一。

- [ ] **Step 5: 提交**

Commit: `art(mon): refine orc lowpoly (humanoid pilot)`

---

### Task 4: 精修 slime 族（slime 代表，5 件）

**Files / Interfaces:** 同 Task 3，怪物名换 `slime`。
特别注意：`mon_slime_part_weapon.json` 当前仅 120 面，必须提升到 ≥1200。

- [ ] **Step 1-5:** 严格重复 Task 3 的 5 个步骤（定义 REFINEMENTS["slime"] → MCP 导出 → node 校验 → `look()` 审图 → 实机截图 → 提交 `art(mon): refine slime lowpoly (slime pilot)`）。

---

### Task 5: 精修 golem 族（construct 代表，5 件）

**Files / Interfaces:** 同 Task 3，怪物名换 `golem`。

- [ ] **Step 1-5:** 重复 Task 3 步骤（golem 是石构造体，建议强化岩块分层/接缝/发光眼）。提交 `art(mon): refine golem lowpoly (construct pilot)`。

---

### Task 6: 精修 fire_imp 族（orb 代表，5 件）

**Files / Interfaces:** 同 Task 3，怪物名换 `fire_imp`。

- [ ] **Step 1-5:** 重复 Task 3 步骤（fire_imp 是 orb 族，建议强化火焰造型层次）。提交 `art(mon): refine fire_imp lowpoly (orb pilot)`。

---

### Task 7: 样板总验收与桌面包同步

**Files:**
- Modify: `CHANGELOG.md`（新增 P0+P1 条目，含审查结论、4 族面数前后对比、实机证据）
- Test: 全量验证

**Interfaces:**
- Consumes: Task 1–6 的全部产物。
- Produces: 可交付的 P1 样板 + 同步的桌面运行包。

- [ ] **Step 1: 全量资产校验**

Run: node 脚本遍历 `assets/models/*.json`，断言全部 schema 合法、无 NaN、name=文件名、面数 ≤4000；并再次跑 Task 2 的非试点字节不变校验（确认只有 4 个试点怪物的 JSON 变了）。
Expected: PASS；git diff 只含 4 个试点怪物的 20 个 JSON + 4 预览 + 脚本 + 测试清单改动。

- [ ] **Step 2: 全量测试门**

Run: `ctest --test-dir build --output-on-failure`（90/90）；`python tools/fn_length_scan.py`（0 超规）；`python tools/world_validator.py`（通过）。

- [ ] **Step 3: 写 CHANGELOG + 同步桌面包**

按 A21 流程写条目；把新 exe + 改动资产 + CHANGELOG 同步到 `C:\Users\HP\Desktop\Roguelike-CPP-3D版`，哈希校验一致。

- [ ] **Step 4: 提交并推送**

Commit: `docs+art: A22 model refinement pilot (P0+P1) complete`；`git push origin master`。

## Self-Review

- **Spec coverage:** §0 决策（范围/族群/面数/清理）→ Task 1–6；§3.5 逐怪覆盖架构 → Task 2；§3.6 包络边界 → Task 3–6 Step 3；§5 批次方案 → Task 1（批次 0）/ Task 3–6（批次 1）；§6 验收门 → Task 7。全部覆盖。
- **Review Focus:** 5 条均有对应任务测试（见 Review Focus 行尾标注）。
- **Type consistency:** `REFINEMENTS` / `_refine` / `build_parts` / `export_monster` 名称贯穿 Task 2–6 一致。
- **Proportion:** 计划主要为测试与步骤约束，无大段代码，符合比例。
