# 3D 建模资产全面审查报告（2026-11-17）

> 只读审查，三路并行：资产文件统计 / 代码接线 / 工作流规范。
> 结论先行：**没有任何实体类别缺 3D 模型**。本轮建模的性质是"精修质量"，不是"补缺口"。

## 1. 资产总览

- 根目录 `assets/models/`，共 **271 个 `.json`，合计 405,636 面**。
- 全部可正常解析，schema 统一为 `{name, vertices, faces}`；顶点为 `[x,y,z,r,g,b]`，面为三角形索引。`height` 不是 JSON 字段，运行时由包围盒派生（与 `src/data/mesh_asset.h` 一致）。
- 无空模型（最小 120 面，是 slime 系武器）、无 NaN/极大坐标、文件名与内部 name 全部一致、无字节级重复。

| 类别 | 件数 | 总面 | 均面 | 区间 |
|---|---:|---:|---:|---|
| player 玩家部件 | 6 | 12,032 | 2,005 | 1,040–4,236 |
| mon 怪物部件（30 怪×5 槽） | 150 | 128,236 | **855** | 120–1,236 |
| npc NPC | 4 | 35,720 | 8,930 | 8,156–9,296 |
| boss Boss 部件（6 Boss×5 槽） | 30 | 71,192 | 2,373 | 1,228–4,828 |
| item 道具 | 18 | 12,748 | 708 | 204–1,310 |
| terrain 地形 | 21 | 43,908 | 2,091 | 732–3,152 |
| room 房间 | 19 | 56,364 | 2,967 | 244–8,220 |
| arena 竞技场 | 5 | 2,200 | 440 | 196–792 |
| lm 地标 | 9 | 9,556 | 1,062 | 380–1,688 |
| guard / altar / chest / spring | 9 | 33,680 | — | — |

**质量分层（与预览图一致）**：
- 已精修：Boss（暗影骑士有披风/肩甲/武器层次）、玩家（v8 精细面部与服饰）、NPC（面部+服饰完整）。
- 未精修：30 种怪物是 4 形态族（humanoid/orb/slime/construct）参数化配色批量产出，第一版方块拼装；平均 855 面/件明显低于其他类别。这正是 CHANGELOG A17 承诺的"先铺开，再二次精修"尚未兑现的部分。
- 完成但单调：地形（石板+铆钉）、地标（倒锥体等抽象几何）、道具（简洁但统一）。

## 2. 代码接线（无缺口）

- 模型加载两套入口：整体 `ModelCache::get(name)`（`model_cache.cpp:80`）、部件 `_try_attach_lowpoly`（`avatar_director.cpp:54`）/ `PlayerAvatar`（`player_avatar.cpp:74`）。
- **代码引用的模型 100% 存在（0 缺失）**：地形 21、地标 9、部件 186（怪 150 + Boss 30 + 玩家 6）全部齐全。
- **孤儿资产 2 个**：`item_armor.json`、`item_robe.json`——8 种护甲拆分前的遗留，仅出现在注释（`hd2d_scene_builder.cpp:1101`），运行时不加载。
- **billboard 只剩回退路径**：地形缺资产→quad、道具缺贴图→fallback、NPC 加载失败→纸偶、`--no-voxel-avatar`→纸偶。没有任何实体"永久停留在纸片"。
- LAVA 是设计上刻意保留 shader/quad（`hd2d_scene_builder.cpp:442`），不是缺模型。

## 3. 数据异常（可在立项中顺手处理）

- 几何重复 2 组：`terrain_wall_prison.json` ≡ `terrain_wall_prison_v0.json`、`terrain_wall_volcano.json` ≡ `_v0`（仅内部 name 不同）。`terrain_wall_void_v0.json` 是近似变体（158 顶点不同），非完全重复。
- 68 个部件模型 y 范围为负（mon 的 arm/weapon 60 个 + boss 8 个）：这是部件局部锚点坐标（武器挂在身侧、手臂下垂），**不是错误**，建模时必须沿用该约定。

## 4. 工作流规范（建模前必读）

两套导出协议：
- **协议 A 整件场景模型**：`blender_lib.export_json`，Blender `(x,y,z)→引擎(x,z,-y)`，Blender 内 Z-up、正面 -Y、脚底 z≥0、1 单位=1 像素（TILE_SIZE=32）。用于 NPC/道具/房间/地标/地形/护甲/竞技场。
- **协议 B 角色部件**：`blender_models_player.export_part_json`，`(x,y,z)→mesh(x,-y,z)`，局部原点=骨骼锚点，Blender 内正面 +Y。怪物 ppu 0.8、Boss ppu 1.15（魔王 1.20）。

统一规范：
- 明暗烘进顶点色（引擎无实时光照）：SHADE 表 front 1.00 / back 0.45 / left·right 0.62 / top 1.55 / bottom 0.30；`COLOR_JITTER=7`；Roughness 0.72；不写 normals。
- 面拆顶点导出（每面独占 3 顶点）+ 扇形三角化，过滤零面积面。
- 命名：整件 `<name>.json`；部件 `<prefix>_part_<slot>.json`（slot: head/torso/arm/leg/weapon，玩家多 cape）。
- 预览命名：`<类别>_preview_<模型key>[_视角].png`，统一放 `art_src/preview/`。

可复用参数化脚本（`art_src/scripts/`）：
- `blender_models_monsters.py`：`MONSTERS` 30 项配色 + 4 形态族 `FAMILIES` 模板，批量生成 5 槽。
- `boss_mcp_rebuild.py`：6 Boss 的 `CONFIGS` 集中管理锚点/ppu/前缀，`rebuild()` 编排。
- `blender_models_terrain.py`、`blender_models_armors.py`、`blender_models_landmarks.py` 均为字典驱动批量导出。

Blender 环境：MCP 已连接（Blender 5.2.1 LTS，插件 1.8，协议 13 匹配，遥测关闭；资产库与 AI 生成器均未开启→走手建脚本路线）。当前 Blender 是默认空场景（只有 Cube/Light/Camera），建模脚本不依赖已打开工程。

**禁用工具**（项目文档明确）：`get_viewport_screenshot`、`get_world_state_snapshot`、`drain_human_activity`；预览走 `look()`，且 `look(image=)` 路径必须正斜杠。

## 5. 审查结论

建模缺口为 0，真正的工作是**质量精修**，优先级候选：
1. 怪物二次精修（30 种，A17 承诺的"再精修"）——工作量最大，但视觉收益最直接。
2. 清理 2 个孤儿资产 + 2 组重复墙资产——低成本卫生工作。
3. 地标/道具的进一步细节层次——中等。

具体的优先级与复杂度排序、分批方案，见立项设计文档。
