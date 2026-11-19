# arena 5 件精修 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 5 件 arena 道具从 A14 原版（196–792 面）精修到面数 ≥1000、造型层次丰富，bbox 不超原值。

**Architecture:** 改 `blender_models_arena.py` 的 5 个 builder，在各自原 bbox 内加主题细节层（照设计文档 §3）；经 Blender MCP `export_all` 覆盖 5 JSON。新建独立闸门（照 P3 模式：schema/name/finite + 面数≥1000 + bbox≤基线），基线从当前精修前 JSON 实测生成。

**Tech Stack:** Blender 5.2.1 + MCP（协议 A）；node 闸门；gtest（mesh_asset_test 契约）。

**Spec:** `docs/superpowers/specs/2026-11-20-arena-refinement-design.md`

## Global Constraints

- 协议 A（Z-up，导出 y=高度）；bbox 各轴不超基线（容差 0.01）；贴地保持（rock y_min=-0.08）
- 面数 ≥1000（用户沿用 P3 门槛）；不改资产名/配色常量/gameplay 映射
- 用现有 palette 键，不新增颜色常量；半径敏感件保持段数
- CLAUDE.md 规范：函数≤40 行、单一职责、语义命名、改前影响分析、改后 Code Review、单次≤300 行、可维护性优先
- 全量门：ctest 91/91、fn_length 0、world_validator、P3/P3b/P4/P5 闸门不退化
- 全量预览重生成 + 审图 + 桌面包同步（exe 到根目录，禁碰初代版）+ 最终审查
- MCP 调用 user_prompt 填用户原话："继续美术/资产方向的精修"

## Review Focus

- 面数不足（rock/spike 原 196/272 需大幅补面）→ Task 1 闸门 faces≥1000
- bbox 撑破（细节外扩）→ 闸门 bbox 六向
- 贴地契约破坏（rock y_min=-0.08）→ 闸门 y_min + mesh_asset_test
- 半径敏感件提段撑破 bbox → 保持原段数，靠内部装饰补面

---

### Task 1: 闸门 + 基线（RED）

**Files:** Create `tools/check_arena_assets.js` + `tools/arena_baselines.json`
**Interfaces:** Consumes 5 件精修前 JSON 实测；Produces 闸门命令。

- [ ] Step 1: 写 node 脚本读 `assets/models/arena_*.json`（5 件），基线从当前实测生成：
  `{name:{faces,bbox:{xmin,xmax,ymin,ymax,zmin,zmax}}}`。闸门校验 schema 键 `faces,name,vertices` /
  `d.name==文件名` / 顶点 6 数且 finite（含颜色）/ `faces>=1000` / 各轴 bbox 不超基线（容差 0.01）。
  输出每件 PASS/FAIL + 总计，失败 exit 1。
- [ ] Step 2: RED — `node tools/check_arena_assets.js` → FAIL（5 件 faces<1000）
- [ ] Step 3: 提交 `test(art): add arena asset gate (face>=1000, bbox<=baseline)`

### Task 2: 5 builder 精修 + MCP 重建（GREEN）

**Files:** Modify `art_src/scripts/blender_models_arena.py`；Overwrite 5 个 `arena_*.json`
**Interfaces:** Consumes 设计文档 §3 造型方案；Produces 5 件 bbox 内、面数≥1000。

- [ ] Step 1: 按设计 §3 给 5 个 builder 各加主题细节层（木纹/符文/碎裂棱面/倒刺/涟漪），
  所有新增顶点落在原 bbox 内，半径敏感件保持段数，函数≤40 行（超了拆 helper）。
- [ ] Step 2: MCP `export_all` 覆盖 5 JSON（user_prompt 填原话）。
- [ ] Step 3: `node tools/check_arena_assets.js` → 5/5 PASS。
- [ ] Step 4: 提交 `art(arena): refine 5 arena props to 1000+ faces`

### Task 3: 全量验收 + 文档 + 桌面同步

**Files:** Modify `CHANGELOG.md`
- [ ] Step 1: 5 张 arena 预览重生成（若有）+ `look()` 审图确认造型层次、无穿模、配色区分。
- [ ] Step 2: 全量门：ctest 91/91、fn_length 0、world_validator、P3/P3b/P4/P5 闸门不退化、arena 5/5。
- [ ] Step 3: CHANGELOG 条目 + 桌面包同步（源码/资产/闸门/预览/CHANGELOG + exe 到根目录，哈希校验）。
- [ ] Step 4: 提交并推送 `docs+art: arena refinement complete`；最终整支审查（fresh reviewer）。

## Self-Review

- Spec coverage：§2 约束 → Global Constraints + 闸门；§3 造型方案 → Task 2；§4 闸门 → Task 1；§5 任务 → Task 1-3。覆盖完整。
- Review Focus：4 条均有闸门测试（面数/bbox/贴地/段数）。
- Type consistency：`check_arena_assets.js` / `arena_baselines.json` 贯穿一致。
- Proportion：计划以契约为主，造型内容由 builder 级决策。
