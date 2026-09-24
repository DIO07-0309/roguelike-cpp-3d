# A6-S2 批次7：5 Boss 骨骼化 — 设计文档

- 日期：2026-09-24
- 状态：已批准（方案B：单生成器+每 Boss 专属部件造型；新建大 rig+独立骨架；复用三剪辑；范围/体型/动画三问用户已裁定）
- 关联提交：批次5 `6fcdccd`（enemies 30/30）、批次6 `7bbf799`（NPC 10/10）
- 基线：HEAD `7bbf799`，工作树 clean

## 背景与范围

A6-S2 已完成 **30 enemies + 10 NPC** 骨骼化（skeleton_parts 205、白名单 40）。本批把 **5 个 Boss** 从配色方块 + `boss_f5/f10/self` 降级图迁到 **5 套独立大体型骨骼分件**，沿用既有白名单路径（骨骼优先、静态图回落），复用 idle/walk/attack 三剪辑。

**范围内（5 Boss）**：

| id | 名称 | 出场 | 造型主题 | weapon |
|---|---|---|---|---|
| shadow_knight | 暗影骑士 | F5 随机1/3 | 紫黑暗铠、巨剑 | greatsword |
| necromancer | 亡灵法师 | F5 随机1/3 | 灰绿袍服、枯骨法杖 | staff |
| vampire | 血族伯爵 | F5 随机1/3 | 绯红立领、细剑 | rapier |
| fire_demon | 地狱火魔 | F10 固定 | 熔岩纹躯、焰冠 | 空手（透明 weapon） |
| demon_lord | 终焉回响 | F15 固定 | 虚空镜像（暗化玩家剪影） | mirror_blade |

**已核实现状**：

- Boss 是 `Monster`（挂 `BossAI`），与普通怪同走 `Monster::draw` → 骨骼覆盖分支（`monster.cpp:266-271`）与 `_monster_avatars_tick()` 白名单懒建（`game_scene.cpp:2991`），键 = `monster_actor_key(m)` = `sprite_override`。
- `boss_factory_create`（`boss.cpp:1213-1220`）：`sprite_override = "boss_<visual_id>"` **仅当** `sprite_by_key("boss_<visual_id>")` 命中整图注册；否则回落 `boss_f5`（F5）/`boss_f10`（F10）/`boss_self`（F15 镜像有意用玩家形象）。
- `bosses.json` 仅 5 条定义；`BossType::GOLEM` 在 `_boss_type_for_floor` 不可达（死代码，触发即降级 shadow_knight），**范围外**。
- Boss 体型 `entity.size` 48×48（部分路径 52/56），普通怪 28×28。
- `mon_necromancer`/`mon_golem` 普通怪分件已存在（批次3/4），与 Boss 版**视觉身份脱钩**：Boss 用 `boss_*` 键，互不干扰。
- F15 `demon_lord` `behavior_type=mirror`：镜像**战斗逻辑**不动，本批只换视觉（暗化玩家剪影 + 玩家同款剑）；`boss_self` 回落路径保留。
- `fire_demon` `domain_config.weak_points`（fire_core 弱点实体）是独立表现，**范围外**。

**范围外**：GOLEM 死代码清理；Phase2/技能专属骨骼剪辑（Phase2 仍用现有 shader/闪光特效）；lightning_orb 运行时；Boss 专属新骨拓扑（多肢/浮空）；boss_f5/f10/self 旧图删除。

## §1 资源与生成（新生成器，不改既有生成器）

### 生成器 `tools/gen_boss_parts.py`（新建）

- 镜像 `gen_npc_parts.py` 结构；**不修改** `gen_mon_humanoid_parts.py` / `gen_mon_soft_parts.py` / `gen_mon_float_golem_parts.py` / `gen_npc_parts.py`。
- 本地 `build_palette`/`part_grid`/`skeleton_dict`（humanoid `build_palette` 绑死其 `FAMILIES`，不可直接复用——同批次6 做法）。
- 表驱动 `BOSS_CONFIGS = {id: (palette_key, weapon_kind)}` 5 条；5 套色板（暗铠紫黑/死灵灰绿/血族绯红/熔岩橙红/镜像虚空紫），描边 `(63,38,49,255)` 不变，九键契约 `. o s m l h r R p b g` 不变。
- 每 Boss 专属部件造型函数（骑士铠甲板/法师袍摆/血族立领/火魔熔岩纹+焰冠/镜像虚边），共享 humanoid BONES/pivot 契约。
- **大 rig DIMS**：torso(40,28) head(30,26) arm(16,22) leg(32,28) weapon(10,30)（≈1.4× NPC 28/22/11/22/6 系）；**ppu 全 0.8**。
- 空手（fire_demon）：weapon 像素行全 `.` → 全透明 PNG；骨架仍含 weapon 槽。
- 另生成 5 张**整图合成预览** `boss_<id>.png`（五件合成静态图），用途：sprites.json 整图键注册 + 骨骼未命中时静态回落。
- 产出：25 分件 PNG + 5 整图 PNG → `assets/sprites/`；5 份 `boss_<id>_skeleton.json` → `resources/animations/`。
- CLI 与既有生成器同构：`--output-dir` / `--skeleton-dir` / `--sheet` → `reports/boss_sheet.png`（reports 不进 git）。
- **行数 ≤300、函数 ≤40**；`conda run python`；脚本只 ASCII 输出；alpha ∈ {0,255} 二值契约不破。

### 注册

1. `resources/sprites.json` `skeleton_parts` **+25** → **205 → 230**（DIMS 见上）；`sprites` 整图区 **+5** 键 `boss_<id>`（指向合成预览 PNG，w/h 取合成画布）。
2. `resources/animations/actor_avatars.json` **+5**，键 `boss_<id>`，`skeleton` 指向 `boss_<id>_skeleton.json`，`anim` 复用 `player_anim.json` → **40 → 45**。
3. `tests/animation/animation_test.cpp` `expected` 集合 **40 → 45**（`:686-702` 定义，末尾追加 5 个 `boss_*` 键，`:685` 注释改「批次1-7」）。断言为 `ASSERT_EQ(out->size(), expected.size())`（`:703`）自比集合大小，**无硬编码计数，无需改断言表达式**；不得删改既有 30 mon + 10 npc 键。
4. 旧 `boss_f5/boss_f10/boss_self` 整图**保留**（回落链与标题/演出可能引用）；`mon_*` 既有 30 怪 + `npc_*` 10 NPC 分件/骨架**严格零 diff**。

## §2 C++ 接线（预期 0-2 行）

- `boss.cpp:1213-1220` 探测逻辑**不改**：注册 `boss_<id>` 整图后 `sprite_by_key` 自动命中 → `sprite_override = "boss_<id>"` → 白名单键自然成立。
- `_monster_avatars_tick()` / `Monster::draw` 骨骼分支 / HD2D `_build_monsters` appendAvatarParts：**预期全部既有路径自动接管**；HD2D 需验证 Boss 是否在 monsters 迭代内（若有 Boss 专属绘制旁路，仅此处小改，仍骨骼优先静态回落）。
- 不改：BossAI/Phase2/arena/mirror 逻辑、`get_boss_visual_color`（回落底色仍用）、boss.cpp 其余部分、肖像/标题场景。

## §3 门禁与验收

- 生成后零 diff 门：`git status --short` 中既有 mon/npc 资源与生成器无 `M`。
- 注册后：Release build 0 err、ctest 68/68、`world_validator.py` 0/0。
- sim A/B：白名单 45↔40 两跑逐字节一致（sha 法）；与批次6 基线差异容忍 `__DATE__/__TIME__` 启动行（批次6 已定性，CHANGELOG 沿用该结论写法）。
- 桌面包同步（robocopy /MIR 8 目录 + 5 根文件 + exe/dll 到根目录；保留 `saves\`、`3D模式.exe.lnk`）。
- 实机验收：F5 三 Boss（重进刷随机）/ F10 火魔 / F15 镜像——2D+HD2D 各看、脚贴地、Phase2 变色特效正常、白名单移除后旧图回落正常。
- CHANGELOG 追加批次7 条目（批次6 行之后）；用户验收通过后单次提交 `feat(a6-s2): 批次7 - 5 Boss骨骼分件与接入`。

## 任务拆分（SDD 沿用 7 任务骨架）

1. 生成器 + dry-run sheet 目检（用户收图）
2. 生成 25 分件 + 5 整图 + 5 骨架 JSON
3. 注册（sprites/actor_avatars/animation_test）+ 门禁
4. C++ 接线验证（HD2D 旁路排查）+ 门禁
5. sim A/B
6. CHANGELOG + 桌面同步
7. 实机验收（用户门）+ 单次提交

## Self-Review

1. **Spec coverage**：5 Boss 全列；范围外清单明确（GOLEM 死代码/fire_core/Phase2 剪辑/旧图删除）。
2. **Placeholder**：无 TBD；DIMS、键数（25/5/45/230）、文件路径、色板主题均为实值。
3. **一致性**：键 `boss_<id>` 贯穿整图注册/sprite_override/白名单/骨架文件；205+25=230、40+5=45、animation_test `expected` 集合 40→45（断言自比 `expected.size()`）相互吻合；`_boss_type_to_id` 5 可达 id 与表一致。
4. **风险闭环**：sprite_override 依赖整图注册 → §1 生成器产合成整图；mirror Boss 只动视觉；零 diff 门护住既有 205 件；二值 alpha 契约延续。
