# A6-S2：NPC 视觉身份回退（撤回批次6 骨骼化） — 设计文档

- 日期：2026-09-24
- 状态：已批准（彻底回退：删 10 个白名单键 + 删 60 个资产文件 + 删 50 条 sprites.json 注册 + 删生成器；C++ 接线保留不删）
- 关联提交：批次6 `7bbf799`（NPC 10/10 骨骼化，本批将其视觉部分撤回）、批次8 `e982d9b`（lightning_orb）
- 基线：HEAD `e982d9b`，工作树 clean

## 背景与问题

批次6 把 10 个世界 NPC 接入骨骼化。**用户实机验收反馈：NPC 容易和怪混淆，应尽量贴近原版贴图；实在不行就用原版贴图。**

调研确认反馈成立，根因不是"颜色接近"而是**造型契约共用**：

### 根因

`tools/gen_npc_parts.py:13-15` 直接从 `gen_mon_humanoid_parts.py` 导入 `BONES`/`OUTLINE`/`PART_ROWS`/`PART_SLOTS`/`SIZES`/`TIERS`/`WEAPONS`/`scale_rows`。NPC 非武器部件**全部使用与怪物/骑士完全相同的像素图** —— 同一个头盔、同一个护甲躯干、同一双手臂、同两条腿；只有色板、体型缩放（standard/runt/gaunt/bulk）和武器不同。

后果：10 个 NPC 没有任何独立脸型、发型、帽子、衣服轮廓，视觉上就是"换色板的怪物"。

最糟一处：`npc_80` 维拉（`runt + dagger`，绿色系）与 `mon_goblin_hunter` 哥布林猎人**体型档、武器像素、骨架契约完全相同，色板也都是绿系**。

### 三个加剧因素

1. **体型跳变**：旧整图世界内画 `TILE_SIZE-4 = 28×28`（`src/game/config.h:19-20`）；骨骼 NPC `anchor` 高度 62 / `pixels_per_unit` 0.8 → 主体约 50 世界单位高。NPC 明显变大，更像怪。
2. **HD2D 无 NPC 名条**：`hd2d_scene_builder.cpp:974-1003` 不画 NPC 名称，只有 2D 画（`game_scene.cpp:3176-3182`）。3D 下只能靠轮廓认人。
3. **同一 NPC 两套面孔**：对话肖像仍用旧圆脸整图（`game_scene_interaction.cpp:249-265`），世界内却是护甲分件，身份割裂。

### 关键事实：不存在 10 张原版 NPC 图

`assets/sprites/` 下旧整图只有 4 张，全部 16×16、298-337 字节的圆脸简化人类头像：`npc_1.png`/`npc_2.png`/`npc_3.png`/`npc_blacksmith.png`。楼层映射（`npc_system.cpp:150-158`）本就是 **3+3+3+1 复用**：F2/F7/F12→`npc_1`，F3/F6/F9→`npc_2`，F4/F8/F11→`npc_3`，F14→`npc_blacksmith`，其余→`npc_1`。

即：**"让 10 个 NPC 各自还原成原版角色"在素材上不可能**（从来没有 10 张独立原版图）。能精确还原的是"回到批次6 之前那 4 张共用整图的样子"。这正是用户所说"实在不行就用原版贴图"。

### 已核实的回退路径（零 C++ 改动）

当前 NPC 渲染本就是**骨骼优先、静态图回落**：

- 2D：`game_scene.cpp:3161-3174` —— `npc_avatar(id)` 存在且 `active()` → 画骨骼；否则 `npc_sprite_key(current_floor)` → `SpriteRenderer::draw_sprite`；再否则画绿点。
- HD2D：`hd2d_scene_builder.cpp:982-1001` —— 骨骼 active 且 `part_draws()` 非空 → 3D billboard 分件；否则楼层旧整图 34px billboard；再否则跳过。
- 懒建与缓存：`game_scene.cpp:3029-3065`，键为 `"npc_" + std::to_string(id)`，**成功或失败都缓存**，失败不重复尝试。

因此**只要把 10 个 `npc_*` 键从 `actor_avatars.json` 移除，NPC 即自动落回旧整图，`src/` 一行都不用改**。

### 范围内

1. `resources/animations/actor_avatars.json` 移除 10 个 `npc_*` 条目（45 → 35 键）
2. `tests/animation/animation_test.cpp` `expected` 集合移除 10 个 `npc_*` 键（45 → 35）并更新注释
3. 删除 50 个 `assets/sprites/npc_*_part_*.png`（15.2 KB）
4. 删除 10 个 `resources/animations/npc_*_skeleton.json`（18 KB）
5. `resources/sprites.json` `skeleton_parts` 移除 50 条 `npc_*_part_*` 注册（230 → 180）
6. 删除 `tools/gen_npc_parts.py`（164 行）
7. `README.md` CHANGELOG 新增一条

### 范围外（保留不动）

- **`src/` 全部 C++ 接线**：`NpcView.npc_id`（`game_scene.h:184-191`）、`_npc_avatars_tick`（`game_scene.cpp:3029-3065`）、2D 分支（`:3161-3174`）、HD2D 分支（`hd2d_scene_builder.cpp:974-1003`）**一行都不改**。它们变为"查不到键 → 走静态回落"的休眠状态，日后用新造型写好 `npc_*_skeleton.json` + 重新登记白名单即可低成本恢复。
- 4 张旧整图 `npc_1/2/3/blacksmith.png` 及其 `sprites.json` `sprites` 段注册（`:275-293`）**保留**
- `npc_sprite_key(floor)` 楼层映射（`npc_system.cpp:150-158`）**保留**
- 对话肖像（`game_scene_interaction.cpp:249-265`）与标题页（`title_scene.cpp:244-255`）**不改**，本来就用旧整图
- HD2D 补 NPC 名条：用户已明确选择"回退到原版整图"而非"回退+补名条"，本批不做
- NPC 专属新造型（平民轮廓/布袍/圆头/缩小到 28×28）：需新写生成器，另立设计
- 30 怪 + 5 Boss 分件与骨架：零改动

## §1 数据面改动

### `resources/animations/actor_avatars.json`

删除第 **123-162** 行共 10 个条目（`npc_20`/`npc_30`/`npc_40`/`npc_60`/`npc_70`/`npc_80`/`npc_90`/`npc_110`/`npc_120`/`npc_140`），使 `mon_poison_wyrm`（`:119-122`）直接衔接 `boss_shadow_knight`（`:163`）。键数 **45 → 35**。

### `resources/sprites.json`

`skeleton_parts` 段移除 50 条 `npc_*_part_*` 注册，键数 **230 → 180**。删除后必须保证剩余键与磁盘文件一一对应。

### `tests/animation/animation_test.cpp`

`RepoDefaultWhitelistCoversA6HumanoidFamily`（`:681`）的 `expected` 集合移除 10 个 `npc_*` 键。断言为 `ASSERT_EQ(out->size(), expected.size())` 自比，**无硬编码计数**，只需改集合内容。`:685` 注释里"批次1-7: .../NPC/5 Boss"要去掉 `NPC`。

同时该循环内的解析断言（9 骨 / 7 件 / anim 可解析）继续覆盖剩余 35 个键，**不得删改**。

## §2 资产删除

| 类别 | 数量 | 体量 | 路径 |
|---|---|---|---|
| 分件 PNG | 50 | 15.2 KB | `assets/sprites/npc_*_part_*.png` |
| 骨架 JSON | 10 | 18 KB | `resources/animations/npc_*_skeleton.json` |
| 生成器 | 1 | 164 行 | `tools/gen_npc_parts.py` |

**为什么敢删生成器**：`gen_npc_parts.py` 里的 `BONES`/`OUTLINE`/`PART_ROWS`/`PART_SLOTS`/`SIZES`/`TIERS`/`WEAPONS`/`scale_rows` 全部是从 `gen_mon_humanoid_parts.py` 导入的，而后者又从 `gen_player_knight_parts.py` 导入（`gen_mon_humanoid_parts.py:8`）。**骨架契约本体在上游两个文件里，删掉 NPC 生成器不丢失任何 rig 资产。** 真正的问题是 NPC 复用了怪物的护甲像素部件（`PART_ROWS`），日后重做要写的是全新的平民轮廓，不是重跑这个脚本 —— 留着它等于留一个"一跑就重现混淆"的陷阱。

## §3 运行时行为变化

| 场景 | 批次6 之后（当前） | 本批之后 |
|---|---|---|
| 2D 世界内 NPC | 50px 高护甲骨骼分件（idle 呼吸） | **28×28 旧圆脸整图**（静态，无动画） |
| HD2D 世界内 NPC | 3D billboard 骨骼分件 | **34px 旧整图 billboard** |
| 对话肖像 | 旧整图 56×56 | 不变 |
| 标题页 | `npc_blacksmith` | 不变 |
| NPC 头顶名字（2D） | 有 | 不变 |
| NPC 名条（HD2D） | 无 | 不变（仍无，靠圆脸轮廓+体型与怪区分） |
| 骨骼接线代码 | 活跃 | 休眠（查不到键即回落） |

**接受的代价**：NPC 失去 idle 呼吸动画。这本来也是 10 个 NPC 共用 4 张图、无法个体化动画的必然结果，且换来的是"一眼能认出这是人不是怪"。

## §4 门禁

1. **构建**：`cmake --build build --config Release -- -j 4` → 0 error，且**确认 0 个源文件被修改**（`git status --short src` 为空）
2. **ctest**：`ctest --test-dir build` → **68/68**（`animation_test` 白名单断言随集合缩小而通过，测试数不变）
3. **World Validator**：`conda run python tools\world_validator.py` → **0 错误 0 警告**。已核实校验器只检查 `player_skeleton.json` 的部件文件（`:287-300`），不扫 `sprites.json skeleton_parts`、不扫 NPC 骨架，因此**无需改校验器**；但仍要跑，确认删资产没牵连其它交叉引用
4. **资产一致性自检**（本次新增门禁）：写临时脚本断言
   - `sprites.json` `skeleton_parts` 每个键的 `file` 在磁盘存在
   - 磁盘上每个 `*_skeleton.json`（非 `player_anim`/`boss_anim` 等 anim）引用的 `part.file` 存在
   - `actor_avatars.json` 每个键的 `skeleton` 与 `anim` 文件存在
   - 全仓 `npc_*_skeleton.json` / `npc_*_part_*.png` 命中数为 0
5. **引用残留检查**：`grep -r "npc_20\|npc_140\|gen_npc_parts\|npc_.*_skeleton"` 在 `src/`、`tests/`、`resources/`、`tools/`、`docs/` 中，除设计文档与 CHANGELOG 的历史记述外**无命中**
6. **实机验收（用户门禁）**：F2/3/4/6/7/8/9/11/12/14 各找一名 NPC，确认恢复为 28×28 圆脸小人、**一眼能与怪区分**、对话功能正常、2D 头顶名字仍在

## §5 提交策略

单次提交：

```
revert(a6-s2): 撤回批次6 NPC骨骼化 - 恢复原版整图
```

保留批次6 的 C++ 接线（不 revert 提交，而是新增一条修正提交），以保留 git 历史里"做过→发现视觉问题→撤回"的完整决策链。

## §6 改动量预估

| 文件 | 改动 |
|---|---|
| `resources/animations/actor_avatars.json` | −40 行（10 条 × 4 行） |
| `resources/sprites.json` | −250 行（50 条 × 5 行） |
| `tests/animation/animation_test.cpp` | −3 行、注释改 1 行 |
| `assets/sprites/npc_*_part_*.png` ×50 | 删除 |
| `resources/animations/npc_*_skeleton.json` ×10 | 删除 |
| `tools/gen_npc_parts.py` | 删除 |
| `README.md` | +1 行 CHANGELOG |
| `docs/superpowers/specs/…-npc-visual-revert-design.md` | 本文档（新增） |
| `docs/superpowers/plans/…-npc-visual-revert.md` | 实现计划（新增） |

**总计：修改 4 文件、删除 61 文件、新增 2 文档。`src/` 零改动。**
