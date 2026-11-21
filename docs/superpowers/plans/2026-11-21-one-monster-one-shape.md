# 一怪一形 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让第一批 13 种怪从"同族 cloning"变成 head/weapon/torso 有独立特征件（包络内），一眼可辨。

**Architecture:** 在 blender_models_monsters.py 的族拷贝循环前，新增 `SIGNATURE_DETAILS`（13 种签名配方）+ merge 助手（逐槽深拷贝 pilot list、签名槽整槽替换），利用 `:1529` continue 跳过族拷贝。细节用 part_* 基元放正面 +Y（head 3~5/torso 4~6.5/weapon 5~8），被 _refine clamp 进 bbox。闸门加 --unique（顶点坐标多重集 sha256，head+签名槽，排除 arm/leg）。

**Tech Stack:** Blender MCP（协议 B 部件）；node 闸门；gtest（player_part_asset_test）。

**Spec:** `docs/superpowers/specs/2026-11-21-one-monster-one-shape-design.md`（含 builder 审查修正）

## Global Constraints

- CLAUDE.md：函数≤40行、单一职责、语义命名、改前影响分析（已 grep REFINEMENTS/_refine/build_parts/export_all）、改后 Code Review、单次≤300行、可维护性优先。
- 不改机制：_refine/build_parts 签名、FAMILIES/ANCHORS、clamp、协议 B、JSON schema、ground_item_baselines.json、resources、引擎、2D 初代版包。
- 包络内：细节放 base bbox 内留余量；正面 Blender +Y（导出 mesh y<0）；face 数 ≤4000。
- merge 逐槽 list() 深拷贝，签名槽整槽替换，绝不共享可变 list。
- gate --unique 直接读 JSON、head+签名槽、排除 arm/leg、先断言顶点数相等。
- 全量门：ctest 91/91、fn_length 0、world_validator、P3/P3b/P4/P5/arena/guard/props 闸门不退化。
- 范围诚实：只 13 种，其余 17 只仍共享几何，验收说明写清。
- MCP user_prompt 填："一怪一形（A22 深化）"。

## Review Focus

- gate 槽位范围错误（全槽比→永远 RED）→ Task 1 限定 head+签名槽
- merge 共享可变 list 污染 pilot 全族 → Task 2 深拷贝测试
- 坐标压平/哈希碰撞（细节被 clamp 压成贴片）→ Task 3 审图 + 坐标预算
- 执行顺序陷阱（注入在 _REFINE_FAMILY 前/族拷贝后）→ Task 2 Code Review
- night_stalker 兽形预期 → Task 3 审图按"兽首+利爪"

---

### Task 1: --unique 闸门 + RED 契约

**Files:** Modify `tools/check_ground_item_assets.js`（加 --unique 分支）
**Interfaces:** Consumes 30×5 JSON；Produces `--unique` 命令。

- [ ] Step 1: 写 --unique 逻辑：解析 `--unique` 参数；只对 13 种签名怪（hardcode id 列表）的 head（+按签名的 weapon/torso）槽，读 `assets/models/mon_<id>_part_<slot>.json`；对每个槽算顶点坐标多重集 sha256（坐标已 round(...,3)，排序拼接），比较前断言同族内顶点数相等；同族内两两哈希必须不同。不改 --all 既有逻辑、不动 baselines.json。
- [ ] Step 2: RED — `node tools/check_ground_item_assets.js --unique` → FAIL（当前 26 只克隆 head 哈希与 pilot 相同）。
- [ ] Step 3: 提交 `test(sim): add --unique gate for monster identity (RED)`

### Task 2: SIGNATURE_DETAILS 骨架 + merge 助手 + 前 6 种配方

**Files:** Modify `art_src/scripts/blender_models_monsters.py`（在 :1527 后、:1528 前插入）
**Interfaces:** Consumes 4 pilot REFINEMENTS；Produces 6 种怪的独立配方。

- [ ] Step 1: 加 `SIGNATURE_DETAILS` 字典 + `_signature_refinements(id)` 助手（逐槽 `list()` 深拷贝 pilot，签名槽整槽替换）+ 注入循环（在 :1527 之后 :1528 之前）。
- [ ] Step 2: 写前 6 种精简配方（night_stalker / skeleton_archer / bone_soldier / shadow_assassin / shadow_stalker / storm_elemental），每种 head 必备 + weapon/torso 按 spec §2 表，每签名槽 2-3 个基元，坐标按槽预算（head y3~5/torso 4~6.5/weapon 5~8），分段数与 pilot 不同以制造哈希差异。
- [ ] Step 3: 本地 import 脚本验证 6 种 head 哈希互不相同（可写临时 py 调 export 到临时目录算 hash，或直接看 Gate）。
- [ ] Step 4: 提交 `art(mon): add signature refinements batch 1 (6 monsters)`

### Task 3: 剩余 7 种配方 + MCP 重导 + 审图

**Files:** Modify blender_models_monsters.py；Overwrite 13×5 JSON
- [ ] Step 1: 写剩余 7 种（lightning_orb / void_walker / blood_leech / poison_wyrm / stone_guardian / ice_warden / iron_sentinel）配方。
- [ ] Step 2: MCP `export_all()` 重导全部 30 种（user_prompt 填原话），13 种签名生效。
- [ ] Step 3: `node tools/check_ground_item_assets.js --unique` → PASS；`--all` → CHECKED=150 FAILED=0。
- [ ] Step 4: Blender MCP 分批装配 13 种审图——造型差异可辨（night_stalker 按"兽首+利爪"标准）、正面细节朝相机、无穿模。
- [ ] Step 5: 提交 `art(mon): add signature refinements batch 2 + export (13 monsters)`

### Task 4: 全量验收 + CHANGELOG + 桌面同步 + 最终审查

**Files:** Modify CHANGELOG.md
- [ ] Step 1: 全量门：ctest 91/91、fn_length 0、world_validator、各闸门（P3/P3b/P4/P5/arena/guard/props）不退化、--all 150、--unique PASS。
- [ ] Step 2: CHANGELOG 条目（根因、13 种签名、包络内方案、--unique 哈希、范围诚实声明）。
- [ ] Step 3: 同步桌面包（源码+资产+闸门+CHANGELOG+exe 到根目录，哈希校验，禁碰初代版）。
- [ ] Step 4: 提交推送 `docs+art: one monster one shape complete`；最终整支审查（fresh reviewer，Range = 本 plan base..HEAD）。

## Self-Review

- Spec coverage：§0 拍板 → Task 1-3；§1 根因 → Task 2（merge 跳过克隆）；§2 13 种 → Task 2/3；§3 约束 → Global Constraints + 各任务；§4 --unique → Task 1；§5 验收 → Task 4。覆盖完整。
- Review Focus：5 条均有任务测试（gate 槽位/深拷贝/坐标预算/执行顺序/兽形预期）。
- Type consistency：`SIGNATURE_DETAILS` / `_signature_refinements` / `--unique` / `mon_<id>_part_<slot>` 贯穿一致。
- Proportion：计划以契约和接线为主，13 种配方内容由 builder 级决策，单次≤300行分批。
