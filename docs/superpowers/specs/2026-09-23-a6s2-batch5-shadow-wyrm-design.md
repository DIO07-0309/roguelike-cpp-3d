# A6-S2 批次5：影武者 3 怪 + 毒液蠕虫骨骼接入 — 设计文档

- 日期：2026-09-23
- 状态：已批准（方案一：扩展两个现有生成器）；勘误 2026-09-23：spear 为本批新增、lightning_orb 降为数据面清偿（裁定 B-1′）
- 关联提交：批次2 `bd43a6e`、批次3 `df47069`、批次4 `7057fe1`

## 背景与范围

A6-S2 骨骼化改造已完成 26/30 只怪。本批补齐**最后 4 怪**，接入后 enemies.json 30 怪全量骨骼化（白名单 26 → 30）。

**范围内（4 怪，visual_id）**：`shadow_stalker` 暗影潜伏者、`shadow_assassin` 暗影刺客、`night_stalker` 夜行猎手、`poison_wyrm` 毒液蠕虫；**附带**：lightning_orb 死内容缺口清偿（火山生成池 +1）。

**范围外（维持批次4 划定）**：NPC、Boss 骨骼化；B 线玩法批次。

**既有事实（已核实）**：

- 4 怪静态图 `mon_*` 均已在 `resources/sprites.json` 注册 → `sprite_override = mon_<visual_id>` 链路就绪，零 C++ 改动。
- 生成位：`biomes.json`/`world/prison.json` 监狱池（shadow_stalker）、`world/abyss.json` 深渊池（shadow_stalker/shadow_assassin）、`challenge_room.cpp` 监狱波（shadow_stalker）、火山波（orc/shaman/poison_wyrm）、深渊第2波（ice_warden/blood_priest/night_stalker）、`floor_manager.cpp` case 10（shadow_assassin/night_stalker 埋伏轮换）。
- lightning_orb：有骨架 + 静态图 + enemies.json 定义，但**无任何生成池引用**（死内容，批次3 记录、批次4 划出范围）。
- `biomes.json` 与 `world/volcano.json` 是双源池，改一处 validator 会报警——须两处同改。

## §1 美术与数据（扩展两个现有生成器，方案一）

### 影武者 3 怪 → 扩展 `gen_mon_humanoid_parts.py`

+3 FAMILIES 色板（字符契约 `. o s m l h r R p b g` 不变，深紫褐描边 `(63,38,49)` 不变）：

| family | 怪 | 主题 |
|---|---|---|
| `shadow_dusk` | shadow_stalker | 暮紫黑，幽瞳青高光 |
| `shadow_ink` | shadow_assassin | 墨黑，刃口冷钢光 |
| `night_brown` | night_stalker | 夜棕毛皮，月银高光 |

MONSTERS 映射与 tier（ppu 全档 0.8，TIERS 批次3 已修）：

| id | family | tier | weapon |
|---|---|---|---|
| shadow_stalker | shadow_dusk | standard | dagger |
| shadow_assassin | shadow_ink | gaunt | dagger |
| night_stalker | night_brown | standard | spear |

**night_stalker 武器**：`dagger` 已定义；`spear` **本批新增** `SPEAR` 像素串常量（6×24，契约字符 `. o s m l h r R p b g`）并登记 `WEAPONS["spear"]`。

### 毒液蠕虫 → 扩展 `gen_mon_soft_parts.py`

+1 色板 `wyrm_venom`（腐绿体液，黄绿毒斑）+ **per-family 虫形行覆盖**：

- 现有 `SOFT_ROWS` 是全体软体怪共享的 blob 蠕动造型；蠕虫需要环节蠕虫躯干。
- 实现：`SOFT_ROWS_OVERRIDES = {"wyrm_venom": {...}}`，部件渲染查覆盖表、未命中回落共享 `SOFT_ROWS`——不破坏旧 5 怪零 diff。
- MONSTERS：`"poison_wyrm": "wyrm_venom"`。
- weapon 槽：`staff`（shaman/图腾施法者），复用现有字符。

### 零 diff 回归门

修改生成器后先 dry-run 全量旧怪到临时目录：**humanoid 旧 14 怪 + soft 旧 5 怪 = 19 怪 95 PNG + 19 JSON 必须与磁盘逐字节一致**。float/golem 生成器不修改，跳过。任何 diff 立即停止排查，不得带 diff 进正式生成。正式生成同样要求旧文件零 diff。

虫形覆盖断言：生成脚本断言 `wyrm_venom` 必须命中 `SOFT_ROWS_OVERRIDES`（防键名拼错静默回落 blob）。

## §2 注册链（与批次4 同构，零 C++ 改动）

1. 正式生成 **20 PNG**（4×5）+ 4 骨架 JSON（`resources/animations/mon_*_skeleton.json`，ppu 0.8）。
2. `resources/sprites.json` `skeleton_parts` **+20 键（135 → 155）**，DIMS 契约：torso(28,20) head(22,20) arm(11,16) leg(22,20) weapon(6,24)。
3. `resources/animations/actor_avatars.json` 白名单 **+4（26 → 30）**，anim 复用 `player_anim.json` → enemies 30 怪全量接入。
4. `tests/animation/animation_test.cpp` expected **26 → 30**，注释改“批次1-5”。
5. **lightning_orb 生成池（仅数据面，裁定 B-1′ 2026-09-23）**：`resources/biomes.json` 火山 `enemy_pool` +`lightning_orb` **且** `resources/world/volcano.json` 池同步 +1（双源同改）；若旁挂 `enemy_weights` 则长度同步 +1。**不声称运行时可刷出**——真接入留后续 C++ 批次。
6. `reports/mon_humanoid_sheet.png` 更新（17 行：旧14+影3）；soft sheet 若批次2 有产物则同步 +1 行（实现时核实文件名）。

## §3 验证与交付链

1. dry-run sheet → 用户过目美术 → 正式生成 + 旧 19 怪零 diff 门。
2. `cmake --build build` + ctest 全量 68 项 + `python tools/world_validator.py` 0/0。
3. **sim A/B（裁定 B4-2 新规，不走批内 UTF-16 老路）**：与批次4 基线比——`--sim 12 --sim-seed 3` 输出 sha 应等于 `7057fe1` 时点 after 输出（两侧均规范 UTF-8 磁盘状态；如需换白名单用 `Out-File -Encoding utf8`）。**火山池 +1 可能产生合法行为差**：若输出不再逐字节一致，DoD 降级为「同 seed 两次运行同 sha（确定性保持）+ validator 0/0 + 无崩溃」，CHANGELOG 如实记录。以实测为准再定稿。
4. README CHANGELOG 批次5 条目（含 lightning_orb 死内容缺口清偿说明、30/30 里程碑）。
5. 桌面测试包同步（镜像目录 + 根文件 + 根目录 exe，保留 `saves/`、`3D模式.exe.lnk`）。
6. 用户实机验收后单次 commit（master 直接执行 + 全批一提交，沿用既有裁定）。

**实机验收点**（最短路径，已删假验收）：监狱常规层（shadow_stalker 池出）、深渊常规层（shadow_stalker/shadow_assassin）、深渊挑战房第2波（night_stalker）、火山挑战房（poison_wyrm 波）。~~火山常规层 lightning_orb 池出~~ → 无运行时路径，改为 validator 双源 +1 通过。

## §4 风险与边界

- **虫形覆盖键名拼错** → 静默回落 blob：sheet 评审肉眼可查 + 生成脚本断言必命中覆盖表。
- **双源池不同步**（biomes vs world/volcano）→ validator 池引用检查，两处同改必过。
- **night_stalker spear**：`spear` 非既有键——必须先加 `SPEAR` 常量 + `WEAPONS["spear"]`，否则 `part_grid` KeyError；加后干跑自检 24×6。
- **lightning_orb 无运行时刷出路径**：`BiomeDef::enemy_pool`/`world/*.json` 不被 C++ 消费；`EncounterChoice.risk/effect` 只加载不执行；`biome_events.json` C++ 不加载。本批仅做双源池**数据面清偿**，验收点**不含**「火山常规层池出」。
- **函数/行数规范**：生成器保持 ≤300 行、函数 ≤40 行（新增均为数据常量与行覆盖 dict，不新增函数）。
- **火山池改动 vs sim 基线**：可能打破逐字节一致——DoD 已按 §3 预案降级，属合法行为差非回归。
- **runt/standard/bulk/gaunt 数值等价**：ppu 统一 0.8，tier 仅语义标签。

## 验收标准（DoD）

- 旧 19 怪生成产物零 diff；新增 20 PNG + 4 JSON 落盘且 ppu 0.8；虫形覆盖断言通过。
- sprites 155 键 / 白名单 30 键 / 测试 30 expected；**enemies 30 怪全量骨骼化**。
- ctest 68/68、validator 0/0；sim：与批次4 基线逐字节一致，或（池改动导致差异时）同 seed 确定性复现 + 无崩溃并在 CHANGELOG 记录。
- lightning_orb 火山池双源接入（biomes + world/volcano，数据面；无「池出」运行时断言）。
- CHANGELOG 已更新（含 30/30 里程碑 + lightning_orb 清偿）、桌面包已同步、用户实机验收通过、已 commit。
