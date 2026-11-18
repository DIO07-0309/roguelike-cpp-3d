# 特殊房间地板/墙壁破碎感升级立项设计（2026-11-19）

状态：**已确认（用户拍板 4 项，全按推荐）**
前置调查：只读调查报告（本报告 §1 现状事实）
技术栈：C++17 + raylib（渲染路径）/ Blender 5.2.1 + MCP（模型）/ node 闸门

## 0. 用户拍板结论

| 决策点 | 用户选择 |
|---|---|
| 范围 | **特殊房间 + 普通地板/墙壁都加破碎感**（全局地牢语言） |
| 技术方案 | **改 C++ 放开特殊房 3D 模型路径 + 新建破碎装饰模型**（复用 P4 群系地形） |
| 视觉形态 | **碎裂石板 + 裂缝 + 崩落碎石**（地板碎块/错缝/龟裂发光；墙壁缺角/剥落/裸石/垂挂碎屑） |
| 交付 | **沿用惯例**：独立闸门 + bbox/面数 + 全量预览审图 + 桌面同步哈希 + 最终审查 + C++ 补 ctest |

## 1. 现状事实（只读调查结论）

### 1.1 根因：特殊房被排除出 3D 模型路径

- `src/game/rendering3d/hd2d_scene_builder.cpp:494-498`：`_emit_terrain_tile()` 里
  `const bool use_3d = !map->get_special_room_at(tx, ty) && _terrain_model_name(...)`。
  **特殊房内所有 tile（含墙/地板/门）一律 `use_3d=false`**，不进 3D 模型路径。
- `:1161-1172` `_terrain_active_for()` 同样显式排除特殊房。
- 结果：特殊房地板走 `_emit_special_room_floor()`（`:300-321`，每 tile 一个 FLOOR_DECAL，
  `texture={}`、`tint=_special_room_tint(sr)`），渲染器 `hd2d_renderer.cpp:1043-1066`
  无纹理分支走 `DrawPlane(纯色)` —— **每 tile 一块不透明纯色平面，覆盖群系地板**。
- 普通地板的装饰（污渍/裂缝/苔藓）在特殊房被跳过（`:409-413`）；
  地面唯一图案是 `_emit_room_inlay_decals()`（`:1350-1373`）的 0.35 tile 小菱形。
- 9 色固定色表 `_special_room_tint()`（`:143-163`）：LANDMARK / CHALLENGE
  共用默认暗色 `{25,25,35}`，triggered 后 ×0.55。
- 特殊房墙无专用模型：`_emit_wall_item()`（`:264-276`）走通用群系墙 quad，
  渲染器无贴图时 `DrawCube` 纯色方块（`:645-651`）。

### 1.2 已有资产（P4 成果，可复用）

- `terrain_floor/stairs/door/wall_<biome>` 19 个 JSON，群系专属主题造型，1 tile=32，自带顶点色。
- 房间摆件 `room_*`（书架/赌桌/神殿柱等）精致（用户肯定），本轮不动。
- **没有** `room_floor_*` / `room_wall_*` 专用模型，全仓检索确认。

## 2. 目标 / 非目标 / 反目标

**目标**
- 特殊房地板/墙壁不再是纯色块：与走廊一样显示 3D 地形模型 + 破碎感装饰。
- 普通地板/墙壁也加破碎感装饰（全局地牢语言），地板图案丰富度对齐房间花雕。
- 三群系配色关系保留；特殊房类型区分（色表/tint）不丢失。

**非目标**
- 不改 gameplay / RNG / 存档 / 房间生成分布 / 枚举。
- 不改房间摆件 `room_*`（用户已肯定）。
- 不改资源数据文件（`resources/*.json` 只读）。

**反目标（做了就算失败）**
- 破坏 2D/3D 双路径一致性、ctest 契约、bbox 契约、贴地约定。
- 改资产名导致 `kGroundItemModels`/`_terrain_model_name` 映射失效。
- 让特殊房 tint 与 3D 模型顶点色叠加出怪异画面（需实机审图验证）。

## 3. 约束

- C++ 改动最小面：集中在 `hd2d_scene_builder.cpp` 的 `_emit_terrain_tile()` / `_emit_special_room_floor()`
  与 `hd2d_renderer.cpp` 的地板/墙绘制；必须保留 2D 路径（`game_map.cpp`）不动或同步。
- 3D 模型走协议 A（`export_json`，顶点色烘明暗），schema `{name,vertices,faces}`。
- bbox：拼贴模型守 1 tile=32 占地（x/z±16）、贴地 y_min=0；面数严格 > 基线（沿前几轮闸门模式）。
- 破碎感形态：地板碎裂石板/错缝/龟裂发光线/崩落碎石；墙壁缺角/剥落/裸石/垂挂碎屑。
- 配色用现有 per-biome palette 键，不新增颜色常量。
- 全量门：ctest 90/90（新增 C++ 测试后为 N/N）、mesh_asset_test、P4 19/19、P3b 17/17、P3 8/8、
  A22 150/150、fn_length 0、world_validator。
- 全量预览重生成 + 审图；桌面包同步哈希；最终整支审查。

## 4. 待 builder 审查确认的关键问题（设计者已给出倾向，需验证）

1. **特殊房 tint 与 3D 模型顶点色如何共存**（最高优先）：放开 use_3d 后，MODEL3D 的 tint 仅承载雾
   （白/153 灰，`_try_terrain_3d` :470-478），而 `_emit_special_room_floor` 的纯色 decal 覆盖模型。
   方案倾向：特殊房启用 3D 模型时，**不再画纯色 decal**，改用一个半透明 tint 的 FLOOR_DECAL 叠在
   模型上保留类型区分（或把 tint 烘进 MODEL3D.tint）。需实机审图选其一。
2. **墙也走 3D**：特殊房墙 tile 放开 use_3d 后，`terrain_wall_<biome>_vN` 是否合适（墙高 40=1.25 tile，
   与特殊房矩形命中无关）。
3. **破碎装饰落在哪层**：倾向新建独立 `terrain_rubble_*` / 装饰件模型，由渲染层在特殊房地板/墙 tile
   额外摆一件；普通地板/墙壁的破碎感可直接烘进现有 `terrain_*` 模型（改 P4 模型）。
4. **2D 路径同步**：`game_map.cpp:617-650` 的 2D 纯色地板是否也要加破碎 decal，还是仅 3D 路径。

## 5. 优先级与批次（建议）

| 批次 | 工作 | 说明 |
|---|---|---|
| P5-1 | C++ 放开特殊房 3D 模型路径 + tint 共存验证 + ctest | 先打通显示，spike 验证观感 |
| P5-2 | 破碎装饰模型（地板/墙壁，按群系） | Blender 建模 + bbox 闸门 |
| P5-3 | 普通地板/墙壁加破碎感（改 terrain_* 模型） | 全局语言 |
| P5-4 | 全量预览审图 + 文档 + 桌面同步 + 最终审查 | 交付 |

> 具体任务划分由实施计划定；builder 审查回答 §4 问题后再定稿。
