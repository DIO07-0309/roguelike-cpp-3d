# 特殊房间地板/墙壁破碎感升级立项设计（2026-11-19）

状态：**已确认（用户拍板 4+4 项，全按推荐；builder 审查已修正 §4 前提）**
前置调查：只读调查报告（本报告 §1 现状事实）
技术栈：C++17 + raylib（渲染路径）/ Blender 5.2.1 + MCP（模型）/ node 闸门

## 0. 用户拍板结论

| 决策点 | 用户选择 |
|---|---|
| 范围 | **特殊房间 + 普通地板/墙壁都加破碎感**（全局地牢语言） |
| 技术方案 | **改 C++ 放开特殊房 3D 模型路径 + 新建破碎装饰模型**（复用 P4 群系地形） |
| 视觉形态 | **碎裂石板 + 裂缝 + 崩落碎石**（地板碎块/错缝/龟裂发光；墙壁缺角/剥落/裸石/垂挂碎屑） |
| 交付 | **沿用惯例**：独立闸门 + bbox/面数 + 全量预览审图 + 桌面同步哈希 + 最终审查 + C++ 补 ctest |

### 补充拍板（builder 审查后，4 项全按推荐）

| 决策点 | 用户选择 |
|---|---|
| 类型色共存 | **烘进 MODEL3D.tint** —— 合成 特殊房色×雾×未探索 一个公式；改动最小、不碰绘制顺序；色值需重标定 |
| 墙 | **保留 quad** —— 只把特殊房地板转 3D，墙维持现状（保留阴影/轮廓），避免丢阴影与 42.5 高差接缝 |
| 破碎装饰挂载 | **独立 rubble 装饰件额外摆** —— 新建 `terrain_rubble_*`，每 tile 按坐标哈希≈20% 密度追加 1 件坐在石板上；不撞 P4 bbox 上限 |
| 2D 路径 | **最小对位** —— `drawSpecialRoomFloor()` 加裂缝线+碎石点，色表合并为单一真相 |

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

## 4. builder 审查结论（关键事实修正 + 选定方案）

builder 只读审查推翻了设计者原 §4-1 的前提，以下为已确认事实：

- **绘制顺序**：decal(:475) → MODEL3D(:484)，`_draw_floor_decal` 关深度测试。
  放开 3D 后**石板会盖住 y=0.02 纯色 decal 与 y=0.12 inlay**（不是相反）。
  原"半透明 decal 叠在模型上"方案不可行 → 改为**烘进 MODEL3D.tint**。
- **tint 是相乘**：`_draw_model`(:734) 材质 diffuse=item.tint × 顶点色。
  直接塞 `{25,25,35}`≈10% 亮度且覆盖雾；须把 特殊房色×雾×未探索 合成一个公式、色值重标定。
- **矩形含周圈墙**（`game_map.cpp:288-304`），墙转 3D 会丢阴影（阴影器只认 WALL_BLOCK）、
  出现 42.5 vs 40 高差与 z±20 悬挑 → **墙保留 quad**。
- **2D 路径完全独立**（`game_map.cpp:617-650`），色表与 3D 重复两份 → 合并为单一真相 + 最小对位。
- **只改 `use_3d` 不够**：`_terrain_active_for()`(:1165) 也排除特殊房，决定掉落物高度；
  不同步改会让掉落物埋进石板。
- **测试盲区**：`hd2d_scene_builder_test.cpp` 全文不覆盖 tint/特殊房/3D 地形/墙，
  ctest 90/90 不能当观感验证；须补 builder 层断言 + 全量预览人工审图硬闸门。

### 选定方案（用户拍板）

1. 类型色烘进 MODEL3D.tint（合成公式 + 色值重标定），不动 decal/model 绘制顺序。
2. 墙保留 quad，仅地板转 3D。
3. 破碎装饰用独立 `terrain_rubble_*` 模型作额外 MODEL3D 按坐标哈希≈20% 密度追加，
   不烘进 P4 模型（避免撞 bbox 上限）。
4. 2D 最小对位 + 色表单一真相。

## 5. 优先级与批次（建议）

| 批次 | 工作 | 说明 |
|---|---|---|
| P5-1 | C++ 特殊房地板转 3D + tint 合成公式 + `_terrain_active_for` 同步 + ctest | 先打通显示 |
| P5-2 | 破碎 rubble 模型（地板/墙壁，按群系）+ 独立闸门 | Blender 建模 |
| P5-3 | C++ 追加 rubble MODEL3D（哈希密度）+ 普通房也挂 + 2D 最小对位 + 色表单一真相 | 全局语言 |
| P5-4 | 全量预览审图 + 文档 + 桌面同步 + 最终审查 | 交付 |

> 具体任务划分由实施计划定。
