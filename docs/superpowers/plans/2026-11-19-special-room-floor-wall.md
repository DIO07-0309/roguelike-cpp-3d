# 特殊房间地板/墙壁破碎感升级 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让特殊房间地板从纯色块变成 3D 群系地形 + 破碎碎石装饰，普通地板/墙壁也加破碎感，且特殊房类型色不丢失。

**Architecture:** 分三层——(1) C++ 渲染层放开特殊房 FLOOR tile 走 `terrain_floor_<biome>` 3D 模型路径，类型色烘进 MODEL3D.tint（合成 特殊房色×雾×未探索 公式），墙保留 quad；(2) Blender 新建 `terrain_rubble_*` 破碎装饰模型（3 群系），渲染层按坐标哈希≈20% 密度作为额外 MODEL3D 追加在石板上；(3) 2D 最小对位 + 两处色表合并为单一真相。闸门校验新资产、ctest 补 tint/密度纯函数断言、全量预览人工审图硬闸门。

**Tech Stack:** C++17 + raylib + gtest；Blender 5.2.1 + Blender MCP（协议 A）；node 闸门。

**Spec:** `docs/superpowers/specs/2026-11-19-special-room-floor-wall-design.md`（含 8 项拍板 + builder 审查修正）

## Global Constraints

- 不改 gameplay / RNG / 存档 / 房间生成分布 / 枚举；不改 `resources/*.json`；不改房间摆件 `room_*`。
- 特殊房**只地板转 3D**（WALL/DOOR 保留 quad）；墙转 3D 会丢阴影且超 bbox 约定，禁止。
- 类型色烘进 MODEL3D.tint：`tint` 与顶点色是**相乘**关系（`_draw_model` :734 `diffuse.color = item.tint`），色值须重标定避免过暗；须合成 特殊房色 × 雾(可见白/不可见153灰) × triggered(×0.55) × 未探索(×0.6) 为单一公式。
- **不动** `_draw_terrain_pass` 的 decal/model 绘制顺序（renderer.cpp:475/484）——牵动所有 FLOOR_DECAL。
- `_terrain_active_for()`(:1163-1172) 必须与 use_3d 判定同步：特殊房地板启用 3D 后，掉落物要抬到 y=6（`ground_item_model_y`），否则埋进石板。
- 新 rubble 模型走协议 A，schema 严格 `{name,vertices,faces}`，逐顶点 6 数、finite、贴地 y_min≈0。
- rubble bbox 守 1 tile=32 占地（x/z 建议 ±16 内）、高度合理（坐在石板顶 y≈4.48 之上，作为独立 MODEL3D 摆放时由 C++ 给 world_pos.y，模型自身 y_min≈0）。
- 密度哈希用 `visual_rng`/坐标哈希，确定性、不碰 gameplay RNG。
- 2D/3D 色表合并为单一真相（一处函数/常量，两路径共用）。
- 全量门：ctest（新增测试后 N/N）、P4 19/19、P3b 17/17、P3 8/8、A22 150/150、fn_length 0、world_validator 通过。
- 全量预览重生成 + 人工审图硬闸门（ctest 不覆盖 tint/特殊房观感，不能当观感验证）；桌面包同步哈希；最终整支审查。
- Blender 脚本硬编码 `REPO=C:/Demo/roguelike_cpp`，直接在 master 执行（不用 worktree）。
- 每次 MCP 调用 `user_prompt` 填用户原话："特殊房间地板墙壁破碎感升级"。

## Review Focus

- **只改 use_3d 一处**：特殊房掉落物埋进石板 → `_terrain_active_for()` 同步 + ctest `GroundItemLiftedAboveTerrainSlab` 扩展。
- **类型色丢失**：3D 石板盖住纯色 decal → tint 合成公式 + ctest 纯函数断言 + 实机审图。
- **tint 过暗/串色**：tint×顶点色相乘、材质全局共享 → 色值重标定 + 审图 + fog/explored 合成进同一公式。
- **墙被误转 3D**：丢阴影 + 高差接缝 → 只 FLOOR 转 3D 的断言 + 审图。
- **rubble z-fight/密度失控**：碎片与石板顶面重叠、密铺 → bbox 闸门 + 密度哈希断言 + 审图。
- **2D/3D 色表漂移**：两处重复色表 → 合并单一真相 + 测试。

---

### Task 1: C++ 特殊房地板转 3D + tint 合成 + 掉落物高度同步

**Files:**
- Modify: `src/game/rendering3d/hd2d_scene_builder.cpp`（`_special_room_tint` / `_emit_special_room_floor` / `_try_terrain_3d` / `_emit_terrain_tile` 的 `use_3d` / `_emit_floor_item` 特殊房分支 / `_terrain_active_for`）
- Modify: `src/game/rendering3d/hd2d_scene_builder.h`（如需暴露纯函数供测试）
- Test: `tests/rendering/hd2d_scene_builder_test.cpp`

**Interfaces:**
- Consumes: 现有 `_special_room_tint(sr)`（:143-163）、`ground_item_model_y(bool)`（h:17）、`_terrain_active_for()`（:1163）。
- Produces: 纯函数 `hd2d::special_room_tint_for_model(const SpecialRoom*, bool visible_now)` → Color（合成公式，供 MODEL3D.tint 与测试）；特殊房 FLOOR tile 启用 3D 模型、掉落物抬升同步。

- [ ] **Step 1: 写失败测试（tint 合成）**

在 `hd2d_scene_builder_test.cpp` 加 `TEST(HD2DSceneBuilder, SpecialRoomTintForModel)`：
构造 `SpecialRoom{type=SHOP, triggered=false}`，断言 `special_room_tint_for_model(&sr, true)` 各通道
在合理区间（比原 `{62,56,24}` 重标定后更亮，例如 r∈[100,255]）；
`triggered=true` 时比 false 暗约 45%；`visible_now=false` 时比 true 暗约 40%。
另加 `TEST(..., SpecialRoomFloorUsesTerrainModel)`（helper 级，能测的部分：use_3d 判定函数对
特殊房 FLOOR 返回 true、对 WALL 返回 false）。先写测试。

- [ ] **Step 2: 验证 RED**

Run: `ctest --test-dir build --output-on-failure -R SpecialRoom`
Expected: FAIL（函数未定义 / 断言不符）。

- [ ] **Step 3: 实现 C++**

新建纯函数 `special_room_tint_for_model(const SpecialRoom* sr, bool visible)`：
以 `_special_room_tint` 的 9 色（去掉原 triggered 压暗，改在公式里统一乘）为基色，
乘 triggered(0.55) × visible(1.0 或 0.6)，并对顶点色相乘做重标定（基色调亮到乘后仍可辨，
具体系数由审图微调，起始用约 ×2.2 并 clamp 255）。
`_try_terrain_3d` 增加可选参数 `const Color* tint_override`，非 null 时用合成色替代雾白/灰。
`_emit_terrain_tile`：把 `use_3d` 的特殊房排除改为——仅 `t==FLOOR` 时允许特殊房走 3D
（墙/门仍排除）；计算 model_name 对特殊房 FLOOR 用 `_terrain_model_name`。
`_emit_floor_item`：特殊房启用 3D 时不再 `_emit_special_room_floor`（石板会盖住它）。
`_terrain_active_for`：`get_special_room_at` 排除改为——特殊房 FLOOR 且模型存在时返回 true（掉落物抬高）。

- [ ] **Step 4: 验证 GREEN**

Run: `ctest --test-dir build --output-on-failure`
Expected: 全绿（含新测试），`GroundItemLiftedAboveTerrainSlab` 仍 PASS。

- [ ] **Step 5: 提交** `feat(render): special-room floor uses 3D terrain with baked type tint`

---

### Task 2: 破碎 rubble 模型（3 群系）+ 独立闸门

**Files:**
- Create: `art_src/scripts/blender_models_rubble.py`（3 builder：prison/volcano/void）
- Create: `tools/check_p5_assets.js` + `tools/p5_baselines.json`
- Overwrite: `assets/models/terrain_rubble_{prison,volcano,void}.json`
- Update: 3 张 `terrain_preview_terrain_rubble_*.png`

**Interfaces:**
- Consumes: `blender_lib`（part_box/sphere/prism/spike 等）、`export_json`、`PALETTES` 配色（从 terrain 脚本复制三群系 palette 键）。
- Produces: 3 件 rubble 模型（贴地 y_min≈0、x/z±16 内、面数>基线），供 Task 3 渲染层追加。

- [ ] **Step 1: 建独立闸门 + 基线（RED）**

写 `tools/check_p5_assets.js`（沿 P4 闸门结构：schema/name/finite/faces>基线/bbox≤基线）。
基线 `p5_baselines.json`：本轮 rubble 是全新资产，无"精修前"基线——用**面数下限门槛**代替：
闸门对 rubble 校验 `faces >= 300`（足够表现碎裂）且 bbox 守 x/z±16、y 合理，schema/finite/name。
先跑 → Expected: FAIL（JSON 尚不存在）。

- [ ] **Step 2: 建 rubble builder**

`blender_models_rubble.py`：3 个 builder，各用 palette 色做**碎裂石板（几块错缝扁 box）+ 龟裂发光线（glow 细条）+ 崩落碎石（小 sphere/prism 簇）**。
地面装饰贴地（模型自身 y_min≈0、高度建议 ≤5，渲染时由 C++ 抬到石板顶）。
半径敏感件低段数；面数目标 ≥300。

- [ ] **Step 3: MCP 导出 3 件**

`execute_blender_code` import 脚本，`export_one(key)` 覆盖 3 JSON（user_prompt 填原话）。

- [ ] **Step 4: 闸门校验 + 审图**

`node tools/check_p5_assets.js` → Expected: 3 件 PASS。`preview_one` 渲染 3 张，`look()` 确认碎裂形态清晰、三群系配色区分。
提交 `art(terrain): add biome rubble decoration models`。

---

### Task 3: C++ 追加 rubble MODEL3D + 普通房也挂 + 2D 色表单一真相

**Files:**
- Modify: `src/game/rendering3d/hd2d_scene_builder.cpp`（`_emit_terrain_tile` / 新 helper `_rubble_density` / `_append_rubble`）
- Modify: `src/game/world/game_map.cpp`（色表合并、`drawSpecialRoomFloor` 加最小破碎线/碎石点）
- Create/Modify: 色表真相源（如 `special_room_palette.h` 或在 scene_builder 暴露 getter）
- Test: `tests/rendering/hd2d_scene_builder_test.cpp`

**Interfaces:**
- Consumes: Task 1 的 `special_room_tint_for_model`、Task 2 的 `terrain_rubble_<biome>` 模型、`ground_item_model_y`。
- Produces: 纯函数 `hd2d::rubble_placed(tx,ty,biome)` → bool（≈20% 坐标哈希密度，确定性）；单一色表函数供 2D/3D 共用。

- [ ] **Step 1: 写失败测试**

`TEST(..., RubbleDensityDeterministic)`：断言 `rubble_placed(tx,ty)` 同输入同输出、统计 20×20 格约 20%（±5%）、
不同格不全相同。`TEST(..., SpecialRoomPaletteSingleSource)`：断言 2D 与 3D 取同一 SHOP 色值相等。先写。

- [ ] **Step 2: 验证 RED** → FAIL（函数未定义）。

- [ ] **Step 3: 实现**

`rubble_placed(tx,ty)`：用坐标哈希（如 `(tx*73856093 ^ ty*19349663) % 100 < 20`），确定性。
`_emit_terrain_tile`：对 FLOOR tile（特殊房与普通房都算），若 `rubble_placed(tx,ty)` 且 rubble 模型存在，
在石板顶 `world_pos.y = 5.0`（略高于 4.48 石板顶）追加一个 MODEL3D（tint 同地形雾色）。
色表：把 9 色提取为单一函数 `special_room_base_color(type)`（放共享头/或 game_map 暴露），
`_special_room_tint` 与 game_map `drawSpecialRoomFloor` 都调用它；后者加几条裂缝线（`DrawLine`）+ 2-3 个碎石点（`DrawRectangle` 小方块），仅非 triggered。

- [ ] **Step 4: 验证 GREEN**

`ctest --test-dir build --output-on-failure` → 全绿（N/N）。

- [ ] **Step 5: 提交** `feat(render): scatter rubble models + unify special-room palette`

---

### Task 4: 全量验收 + 阻塞性审图 + 文档 + 桌面同步

**Files:** Modify `CHANGELOG.md`（P5 条目）；全量预览核对。

- [ ] **Step 1: 全量闸门**

`ctest --test-dir build --output-on-failure` → N/N PASS；`node tools/check_p5_assets.js` → 3/3；
`node tools/check_p4_assets.js` 19/19；`node tools/check_p3b_assets.js` 17/17；
`node tools/check_ground_item_assets_p3.js` 8/8；`node tools/check_ground_item_assets.js --all` 150/150；
fn_length 0；world_validator 通过。

- [ ] **Step 2: 阻塞性实机审图**

重点验收（ctest 不覆盖）：特殊房地板不再是纯色块、类型色可辨且不过暗、掉落物不埋石板、
墙仍是 quad 有阴影、rubble 不 z-fight、2D 路径颜色一致。用 `look()` 看构建层预览 + 对比 2D/3D。
任一项不达标 → 回对应 Task 迭代重做后重验。

- [ ] **Step 3: CHANGELOG + 桌面同步**

写 P5 条目（根因、三层方案、tint 公式、rubble 密度、2D 对位、ctest 新增数）。
桌面包是完整源码 + 预编译 `roguelike_cpp.exe`：本轮改了 C++，须同步源码（src/）+ 资产（assets/）+
闸门/preview/CHANGELOG，并**重新编译桌面版 exe**（在桌面包目录用其 CMakePresets 构建）使改动生效，
然后哈希校验。若桌面包重编译环境不可用，则同步源码 + 资产并明确记录"exe 未重编，需用户自行构建"。

- [ ] **Step 4: 提交并推送** `docs+feat: special-room floor/wall refinement complete`；`git push origin master`。

## Self-Review

- Spec coverage：§0 8 项拍板 → Task 1/2/3/4 全覆盖；§1 根因 → Task 1（3D 路径+tint）/ Task 3（rubble+2D）；§2 目标反目标 → Global Constraints；§3 约束 → 闸门 + ctest + 审图；§4 builder 结论 → Task 1（顺序/tint 相乘/墙保留/`_terrain_active_for`）/ Task 2（独立 rubble）/ Task 3（2D 单一真相）；§5 批次 → Task 1-4。覆盖完整。
- Review Focus：6 条均有对应任务测试（掉落物高度/tint 合成/色表单一源/墙 quad/rubble 哈希/2D 色表）。
- Type consistency：`special_room_tint_for_model` / `rubble_placed` / `ground_item_model_y` / `terrain_rubble_<biome>` / 闸门脚本名贯穿一致。
- Proportion：计划以契约和步骤为主，C++ 实现细节由 builder 级决策，无大段代码。
