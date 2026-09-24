# A6-S2 批次6：10 NPC 骨骼化 — 设计文档

- 日期：2026-09-24
- 状态：已批准（方案1：镜像 mon 管线；§1–§3 用户逐节确认）
- 关联提交：批次4 `7057fe1`、批次5 `6fcdccd`（enemies 30/30）
- 基线：HEAD `6fcdccd`，工作树 clean；sim 基线 sha `b6814bdd4b44bf15e92153db5c4876552ea1d9f04ade82b68a724a155cc17d06`

## 背景与范围

A6-S2 已完成 **30/30 enemies** 骨骼化。本批把 **10 个世界 NPC** 从 4 张共享 16×16 静态图迁到 **10 套独立骨骼分件**，并在 2D + HD2D 世界路径接入 `SkeletonAvatar`（与 mon 同策略：骨骼优先、静态图回落）。

**范围内（10 NPC，按 `npc_id = floor*10+slot`）**：

| npc_id | 楼层 | 角色 | 拟定 family 主题 | weapon |
|---|---|---|---|---|
| 20 | F2 | 老囚犯埃德加 | `npc_prisoner` 锈铁囚服 | 空手（透明） |
| 30 | F3 | 猎人瑞卡 | `npc_hunter` 皮革棕 | dagger |
| 40 | F4 | 幸存者卡利安 | `npc_survivor` 灰褐布衣 | 空手 |
| 60 | F6 | 收藏家卡兹 | `npc_collector` 赭黄斗篷 | 空手 |
| 70 | F7 | 祭司泰伦斯 | `npc_priest` 白金圣袍 | staff |
| 80 | F8 | 侦察兵维拉 | `npc_scout` 苔绿轻甲 | dagger |
| 90 | F9 | 朝圣者索拉斯 | `npc_pilgrim` 尘灰长袍 | 空手 |
| 110 | F11 | 迷失灵魂 | `npc_ghost` 浅灰青（模拟半透明） | 空手 |
| 120 | F12 | 眠者 | `npc_dreamer` 梦霭紫 | 空手 |
| 140 | F14 | 守望者 | `npc_watcher` 铸光青金 | cleaver |

（family 名与 weapon 分配实现前可微调；表为 spec 定稿基线。）

**已核实现状**：

- 身份：`npc_system.cpp` 10 个硬编码 `NPCData`；`npc_sprite_key(floor)` 仅 **4 键**（`npc_1/2/3/blacksmith`）→ 10 人共享；`NPCState.id` 已是 `floor*10+slot`。
- 2D：`game_scene.cpp:3100-3133` 循环画静态图（frame 0）+ 绿点回落；名字表硬编码 `:3096`。
- HD2D：`hd2d_scene_builder.cpp:974-994` `_build_npcs` 整层共用 `npc_sprite_key`，`NpcView` 仅 `{tile_x,tile_y,finished}`。
- 对话肖像：`game_scene_interaction.cpp:257-265` 56×56 静态图；标题页 `title_scene.cpp:250` 用 `npc_blacksmith`。
- `actor_avatars.json` / `SkeletonAvatar`：**零 npc 键**；头文件已注明「未来 NPC」。
- `encounters.json` 6 个叙事 NPC：**无世界体**，范围外。

**范围外**：Boss 骨骼化；lightning_orb 运行时；encounters 叙事 NPC；走跑循环（NPC 静止 idle）；对话肖像/标题骨骼化；`npc_sprite_key` 数据化重构；新增 `resources/npcs.json`。

## §1 资源与生成（新生成器，不改 mon 三文件）

### 生成器 `tools/gen_npc_parts.py`（新建）

- **import** `gen_mon_humanoid_parts`：`BONES`、`PART_SLOTS`、`SIZES`、`TIERS`、`WEAPONS`、`OUTLINE`、`PART_ROWS`、`build_palette` 所需色板结构、`skeleton_dict` 模式（**NPC 自建** `skeleton_dict`，因 file 前缀是 `npc_` 非 `mon_`）。
- **不修改** `gen_mon_humanoid_parts.py` / `gen_mon_soft_parts.py` / `gen_mon_float_golem_parts.py`。
- 数据表：
  - `NPC_FAMILIES`：10 色板，九键契约 `. o s m l h r R p b g` 不变，描边 `(63,38,49,255)`。
  - `NPCS = {"npc_20": (family, tier, weapon_key_or_None), ...}` 10 条。
- **空手**：`weapon=None` → weapon 像素行全 `.` → 全透明 PNG；骨架仍含 weapon 槽（`PART_SLOTS` 不分叉）。
- rig/tier/ppu：与 humanoid 相同 `BONES`/`PART_SLOTS`；tier 用现有 `standard/gaunt/runt/bulk` 语义标签；**ppu 全 0.8**。
- 产出：`assets/sprites/npc_{id}_part_{torso,head,arm,leg,weapon}.png`（50 PNG）+ `resources/animations/npc_{id}_skeleton.json`（10 JSON，`file` 路径 `npc_` 前缀）。
- CLI 与 soft 同构：`--output-dir` / `--skeleton-dir` / `--sheet` → `reports/npc_sheet.png`（10 行；reports 不进 git）。
- **行数 ≤300、函数 ≤40**；Python 一律 `conda run python`；脚本只 ASCII 输出。
- 幽灵半透明：**浅灰青色板模拟**，不破二值 alpha 契约（`part_image` 断言 alpha ∈ {0,255}）。

### 注册

1. `resources/sprites.json` `skeleton_parts` **+50**（10×5）→ **155 → 205**；DIMS 与 mon 一致：torso(28,20) head(22,20) arm(11,16) leg(22,20) weapon(6,24)。
2. `resources/animations/actor_avatars.json` **+10**，键 **`npc_20`…`npc_140`**（与 `NPCState.id` 一致）；`anim` 复用 `player_anim.json`；`skeleton` 指向 `resources/animations/npc_{id}_skeleton.json` → **30 → 40**。
3. 旧 `npc_1/2/3/blacksmith` **保留**在 sprites.json（肖像/标题/回落）；**不删** `npc_sprite_key()`。

### 零 diff 门

正式生成前 dry-run 到临时目录：**旧 30 mon 分件/骨架 + 4 张 `npc_*.png` 静态图逐字节不变**；本批生成器为新文件，mon 三文件 git diff 必须为空。正式生成后同样校验 mon 侧与静态图零 diff。

## §2 C++ 接线（方案1：镜像 mon，最小触点）

### 2.1 `NpcView` 带 id

```cpp
// game_scene.h
struct NpcView {
    int tile_x = 0, tile_y = 0;
    bool finished = false;
    int npc_id = 0;   // 新增: floor*10+slot, 与 NPCState.id / quests 一致
};
```

`npc_views()`：`out.push_back({tile_x, tile_y, false, _npc_state[i].id});`（构造点仅此一处，已核实）。

### 2.2 NPC 骨骼缓存 + tick

- 成员（`game_scene.h`）：`std::map<int, std::unique_ptr<SkeletonAvatar>> _npc_avatars;`（键 = npc_id；上限 10）。
- `void _npc_avatars_tick();` 声明贴 `_monster_avatars_tick`；调用点与 mon **同两处**（HD2D 分支 `:2440` 旁、2D 分支 `:2450` 旁）。
- 逻辑（镜像 mon tick，函数 ≤40 行）：
  1. 白名单懒加载复用 `_actor_avatars_loaded` / `_actor_avatars`（与 mon 共用，不二次读文件）。
  2. 遍历 `_npc_state[0.._npc_count)`：`finished` 跳过；键 `"npc_" + std::to_string(id)`；未命中 map 且白名单无键 → 不建（回落）。
  3. `try_init` 成败都 `emplace` 缓存不重试（同 mon）。
  4. active 则 `advance(dt, idle AnimInput)`：NPC 无移动/攻击 → 默认全 false 的 `AnimInput` 即 idle；`track_facing` 可省略（NPC 不位移，默认 face=1）。
- **不改** `_monster_avatars_tick` 与玩家路径。

### 2.3 2D 世界绘制（`game_scene.cpp` NPC 循环 ~3112）

- 键：`_npc_state[i].id` → `_npc_avatars`。
- active → `avatar->draw_at(feet, 1.f, 255)`；feet 锚点：tile 中心 `(nx, ny)` 与现有 `s = TILE_SIZE-4` 中心绘制对齐——**实现时用 `ny + s/2 - k`（k 与 mon 脚底同源或 sheet 实机微调），验收不得悬空/陷地**。
- 否则 → 现有 `sprite_by_key(npc_sprite_key(floor))` + 绿点回落 **原样**。
- 名字标签 / E 提示 / `finished` 跳过 / 可见性剔除 **不动**。

### 2.4 HD2D `_build_npcs`

- 每 NPC：`npc.npc_id` → 查 GameScene 骨骼（需 const 访问器，如 `SkeletonAvatar* npc_avatar(int npc_id)` 与 mon `skeleton_avatar()` 对称）。
- active → `part_draws({}, false)` + `appendAvatarParts(parts, feet_world, …)`（feet 与 2D 同语义：tile 中心 xz，y=0；`sort_y` 仍 `tile_y * TILE_SIZE`）；`continue`。
- 否则 → 现有 `ENTITY_BILLBOARD` + `npc_sprite_key(gs.current_floor)` **原样**（整层 skey 可保留作回落）。
- `outline=true`、size=34、可见性剔除不动。

### 2.5 明确不改

- 对话肖像 `game_scene_interaction.cpp:257-265`（56×56 静态图）。
- 标题页 `title_scene.cpp` `npc_blacksmith`。
- `npc_sprite_key()` 函数与 4 静态 PNG 注册。
- `NPCData` 硬编码表、名字 label 表、quests/dialogues JSON。
- encounters.json 叙事 NPC；无 `resources/npcs.json`。
- mon 三生成器与 mon 渲染路径。

**错误处理**：骨架/贴图缺失 → `try_init` 失败 → 静态图路径（与 mon 相同 all-or-nothing）；白名单空 → 零开销全回落。

## §3 测试、验证与交付

### 测试

- `tests/animation/animation_test.cpp` expected **30 → 40**：+`npc_20, npc_30, npc_40, npc_60, npc_70, npc_80, npc_90, npc_110, npc_120, npc_140`；注释「批次1-5 + 批次6 NPC」；`ASSERT_EQ(size)`；旧 30 mon 键不得删改。
- `asset_manifest_test`：自动覆盖新 50 `skeleton_parts`（文件存在 + w/h 一致）。
- mon 渲染/白名单回归：无专门 NPC 渲染单测；以 ctest 全量 + 实机为准。

### 门禁

1. dry-run `reports/npc_sheet.png` → **用户过目** 10 造型/武器/空手透明。
2. 正式生成 + 注册；**零 diff**：旧 30 mon 产物 + 4 静态 PNG。
3. `cmake --build build` + `ctest` 全量（当前 68，以实测为准）+ `conda run python tools/world_validator.py` → 0/0。
4. **sim A/B（裁定 B4-2）**：`--sim 12 --sim-seed 3`；无头不渲染 NPC、不实例化 SkeletonAvatar → 预期与批次5 基线 **逐字节一致** sha=`b6814bdd…17d06`。若意外分叉：同 seed 两次同 sha + 无崩溃 + CHANGELOG 如实记录。换文件必须 UTF-8（`Out-File -Encoding utf8`，禁 PS `>`）。
5. README CHANGELOG 批次6（10 NPC、白名单 40、`npc_id` 键、肖像/标题仍静态、世界路径 2D+HD2D 骨骼优先）。
6. 桌面包同步：robocopy /MIR 8 目录（`src/ resources/ tools/ tests/ docs/ assets/ .github/ vendor/`）+ 根文件（CMakeLists.txt README.md CLAUDE.md CMakePresets.json .gitignore）+ **根目录 exe**；保留 `saves/`、`3D模式.exe.lnk`；exit 0/1 成功。
7. **实机验收**：至少覆盖 F2 空手囚犯、F3 持刃猎人、F7 持杖祭司、F11 幽灵、F14 持 cleaver 守望者；2D 与 HD2D 各看一眼；**对话肖像仍是旧静态图**；名字/E 提示正常。
8. 用户确认 → **全批单次 commit**（master，覆盖 SDD 每 Task commit）。

### 风险

| 风险 | 缓解 |
|---|---|
| `NpcView` 加字段破坏构造 | 仅 `npc_views()` 一处 push，已核实 |
| 2D feet 锚点悬空/陷地 | 与 `TILE_SIZE-4` 中心绘制对齐；实机验收 |
| 空手 weapon 槽 KeyError | `weapon=None` → 全 `.` 行，不进 `WEAPONS[...]` 索引 |
| 幽灵真 alpha | 二值 alpha 契约不破，浅色模拟 |
| HD2D 缺 avatar 访问器 | 与 mon 对称的 `npc_avatar(int)`；只读红线内 |
| tick 双调用路径遗漏 | 紧贴 mon 两调用点各 +1 行 |
| `gen_npc_parts` 行数 | 新文件 ≤300；import 复用 humanoid 常量 |
| 白名单 40 打破 animation_test | 同步 expected 集合 |

## 验收标准（DoD）

- 10 套 `npc_*_part_*`（50 PNG）+ 10 `npc_*_skeleton.json`（ppu 0.8）落盘；空手 weapon 全透明。
- `skeleton_parts` **205** / `actor_avatars` **40** / animation_test expected **40**。
- 旧 30 mon 分件与 4 张 `npc_*.png` 零 diff；mon 三生成器无 diff。
- ctest 全量通过、validator 0/0；sim 与批次5 基线逐字节一致（或合法分叉预案）。
- 2D + HD2D 世界路径骨骼优先回落正确；肖像/标题仍静态。
- CHANGELOG 已更新、桌面包已同步、用户实机验收通过、单次 commit。
