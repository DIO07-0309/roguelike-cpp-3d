# P3 地标 / 道具精修立项设计（2026-11-18）

状态：**待用户拍板**
依赖审查：`docs/superpowers/specs/2026-11-18-p3-asset-audit.md`

## 1. 目标

按优先级和复杂度精修 P3 范围（9 地标 + 16 道具）中质量最低的部分，
并清理 A22 遗留的构建脚本残留。不改 gameplay / 坐标协议 / 资产 schema。

## 2. 目标 / 非目标 / 反目标

**目标**
- 把 8 件通用道具（面数 204–592）的细节层次提升到 600–1200 面/件。
- 清理 `build_models_blender.py` 与 `blender_models_items.py` 里已删资产的 builder 残留。
- 为地标/道具建立独立的资产契约闸门（A22 闸门只覆盖 mon_* 部件）。

**非目标**
- 8 件护甲（728–1310）和 9 件地标（380–1688）本身健康，本轮**不强制精修**；
  如需做，放到批次 2 且明确收益。
- 不改协议 A 的坐标映射、导出格式、配色来源。

**反目标（做了就算失败）**
- 破坏 `kGroundItemModels` 映射或 `mesh_asset_test` 契约。
- 让道具占地超出原 bbox 导致穿墙/盖过相邻格。
- 重跑构建脚本又把已删的 item_armor/item_robe 生成回来。

## 3. 约束（协议 A，与 A22 协议 B 不同）

- 整件模型：`export_json` 做 `(x,y,z)→(x,z,-y)`，Blender 正面 -Y → 引擎 +Z 朝相机。
- .spark 明暗烘顶点色（SHADE 表 + COLOR_JITTER=7），WRITE_NORMALS=False。
- schema `{name, vertices[x,y,z,r,g,b], faces[i,j,k]}`。
- 道具是平躺/立放整件，无骨骼 pivot：精修只能向内加细节或轻微抬高，
  **不得放大占地 bbox**（地标多为 30×22，道具 varies）。
- 契约须覆盖：schema / name==stem / finite / 面数区间 / 占地不超原 bbox。

## 4. 优先级与复杂度（建议）

| 优先级 | 工作 | 件数 | 复杂度 | 收益 |
|---|---|---:|---|---|
| P3-0 | 清理 A22 脚本残留（item_armor/item_robe builder） | 0 | 低 | 防倒退 |
| P3-1 | 8 件通用道具精修（长枪/双截棍/药水×2/匕首/剑/护符/弩） | 8 | 中 | 高（面数落差最大） |
| P3-2 | 9 地标 + 8 护甲细节微调 | 17 | 中高 | 中（本身已健康） |

**建议本轮范围：P3-0 + P3-1**（清理 + 8 道具精修），P3-2 留待后续。

## 5. 批次方案（建议范围）

### 批次 0：清理脚本残留
- `build_models_blender.py:78-79` 删除 item_armor / item_robe 两条 BUILDERS。
- `blender_models_items.py:93-151` 删除 build_armor / build_robe 遗留函数。
- 验收：`grep item_armor"/"item_robe" 在构建脚本 0 命中；build_all() 不再生成这俩。

### 批次 1：8 件通用道具精修
- 在 `blender_models_items.py` 的每个 builder 内增加细节件（用现有 part_* 基元），
  面数目标 600–1200，不放大占地 bbox。
  建议：长枪加枪头刃纹+缨+握把缠带；双截棍加端头配重+链节；药水加标签浮雕+瓶塞纹；
  匕首/剑加护手雕纹+刃脊；护符加宝石切面+链节；弩加弓臂层次+扳机。
- 经 Blender MCP `execute_blender_code` 重建这 8 件并覆盖 JSON。
- 重生成 `blender_preview_items.png`。
- 验收：新闸门 schema/finite/面数/占地 bbox 全过；mesh_asset_test 全绿；
  `look()` 审图无穿模；抽样实机截图。

### 批次 2（本轮不展开）
9 地标 / 8 护甲的细节微调，按用户节奏另行拍板。

## 6. 待拍板

1. 范围：P3-0 + P3-1（清理 + 8 道具），还是连 P3-2（地标+护甲）一起？
2. 8 道具面数目标 600–1200 是否合适（当前 204–592，护甲均值 1060）？
3. 批次 0 的脚本残留清理是否同意（低风险，防倒退）？
4. 是否需要为 P3 单独提交资产契约闸门脚本（独立工具，不复用 mon_* 闸门）？

> 以上是建议，拍板后再进入 writing-plans 出详细实施计划。
