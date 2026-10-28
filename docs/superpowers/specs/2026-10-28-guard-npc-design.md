# A15-Guard: 守卫互动 NPC + 配套场景道具（设计稿）

日期: 2026-10-28  |  作者: AI + 用户确认  |  状态: 用户已批准（B 方案，直接推进，不再逐节询问）

## 1. 目标
在现有 A15 建模管线（`blender_lib.py`、`build_models_blender.py`、44 个已导出 `.json`）基础上，新增：
- 守卫互动 NPC：2 种状态（警戒 / 已交互休息），低模风格，同现有 `npc_1/2/3/blacksmith`
- 配套场景道具 1 件：守卫旗帜（`guard_banner`）或警戒火炬（`guard_torch`）
- 总计新增 3-4 个 `.json` 模型，全部通过 `blender-MCP` 检查健康状况后导出

## 2. 资产清单与命名
| 模型文件名 | 说明 | 状态映射（参考 `altar`/`altar_used`、`spring`/`spring_dry`） |
|---|---|---|
| `guard_alert.json` | 守卫警戒：站立、持矛/盾、深蓝制服 + 铁色盔甲 | 未交互 / 初始 |
| `guard_interacted.json` | 守卫已交互：放松坐姿或手持书卷、服色略暗/灰（同色系 `CLOTH_BLUE` 暗色 + `IRON_DARK`） | 交互后 |
| `guard_banner.json` | 守卫旗帜：深蓝布面 + 金色边框（`GOLD`/`GOLD_LIGHT`），挂在柱上，参考 `blender_models_challenge.py` 的挂旗构造 | 场景装饰 |

## 3. 构造参数（与现有管线一致）
- 单位: Z-up，1 Blender 单位 = 1 世界像素
- 正面: -Y（导出后引擎 +Z 对齐）
- 身高: 约 46（与 `npc_1` 同标准）
- 基础零件: `blender_lib.py` 提供 `part_box`, `part_cylinder`, `part_prism`, `part_sphere`, `join_parts`
- 颜色调色板（直接复用现有常量）：
  - 制服: `CLOTH_BLUE` (70,104,58) / `CLOTH_BLUE_DARK`
  - 盔甲/肩甲: `IRON` (150,160,172) / `IRON_DARK` (98,106,118)
  - 头盔: `IRON_DARK` + `GOLD` 边框
  - 旗帜布面: `CLOTH_BLUE`，边框: `GOLD` / `GOLD_LIGHT`

## 4. 两种状态的差异设计
- `guard_alert`: 站立、双手持盾或长矛、头盔完整、身体略前倾
- `guard_interacted`: 坐姿（或站立但放松）、手持书卷或卷轴、头盔可略移（或保持）、身体姿势放松、颜色整体降低饱和（使用 `CLOTH_BLUE_DARK` 替代部分主色）
- 旗帜 (`guard_banner`): 独立道具，无状态变化（或可加“未使用/已使用”两态，参考 `challenge_banner` / `challenge_banner_used` 模式；先做单态，后续可扩展）

## 5. Blender-MCP 使用流程（已确认插件可用）
1. `get_addon_status()` → 确认插件版本与库可用
2. `execute_blender_code()` → 执行 `blender_models_npc.py` 构造脚本（或在脚本中增加 `guard_alert` / `guard_interacted` 构造函数）
3. `get_scene_info()` → 检查低模健康状况（四边形比例、非流形边、松散顶点、法线方向）
4. `look()` → 角度预览（front / three_quarter / top）确认比例与正面方向
5. 运行 `build_models_blender.py` → 导出 `.json` + `.png` 预览到 `assets/models/`
6. 更新 `tests/rendering/mesh_asset_test.cpp` → 注册新模型名
7. 更新 `README.md` → 补充 A15 建模数量与列表

## 6. 测试与验证
- 新增 `tests/world/drop_tile_test.cpp` 已验证 `nearest_walkable` 不变量（与本修无关，但模型注册后不破坏现有 87/87 测试）
- `mesh_asset_test` 增加 3-4 个新名称后仍通过
- 构建后 `fn_length_scan.py` 仍 0 超规（建模脚本不计入 `.cpp` 行数限制）
- 桌面同步后冒烟：`model load failed = 0`、`errors = 0`

## 7. 约束与不做的事
- 不新增游戏机制（守卫仅为环境/互动 NPC，不增加战斗 AI 或新技能树节点）
- 不修改 `item_icon_key` 映射（守卫不是可拾取地面物品）
- 不做骨骼动画（仅低模静态模型；`anim_player_rig.py` 属后续独立任务）
- 不做新房间类型（仅配套场景道具，与现有挑战房/房间构造共存）

---
自查：无 `TBD`、无内部矛盾、范围明确（3-4 模型）、不含无关重构、已确认 `blender-MCP` 可用。

请审阅此设计文档。如无修改意见，我将进入 `writing-plans` 执行阶段（先写实现计划，再逐步执行建模脚本修改、导出、测试注册、同步桌面、冒烟验证）。
