# A6-S2 批次4：人形补充 6 怪骨骼接入 — 设计文档

- 日期：2026-09-23
- 状态：已批准（方案一：扩展现有生成器）
- 关联提交：批次2 `bd43a6e`、批次3 `df47069`

## 背景与范围

A6-S2 骨骼化改造已完成 20/26 只人形系+软体+浮灵魔像怪。本批为**人形补充 6 怪**接入骨骼分件，全部复用人形 rig（与批次1 同一身体模板）。

**范围内（6 怪，visual_id）**：`charger` 冲锋兽人、`summoner` 哥布林召唤师、`necromancer` 亡语者、`ice_warden` 冰狱守卫、`blood_priest` 血祭司、`dark_mage` 暗术师。

**范围外**：lightning_orb 生成池修复（死内容缺口）、影武者 3 怪、毒液蠕虫、NPC、Boss 骨骼化。

**既有事实（已核实）**：

- 6 怪静态图 `mon_*` 均已在 `resources/sprites.json` 注册 → `sprite_override = mon_<visual_id>` 链路（`monster.cpp`）就绪，零 C++ 改动。
- 生成位：`challenge_room.cpp` 监狱第3波（charger/summoner）、火山第3波（necromancer）、深渊第1波（dark_mage）、深渊第2波（ice_warden/blood_priest）；`biomes/volcano/abyss` 敌人池（charger/dark_mage）；`floor_manager.cpp` 权重位6/7/9。
- `title_scene.cpp` 预览条使用静态 `mon_summoner` 等，不受本批影响。

## §1 美术与数据（扩展 `tools/gen_mon_humanoid_parts.py`，方案一）

**6 个新 FAMILIES 色板**（字符契约 `. o s m l h r R p b g`，深紫褐描边 `(63,38,49)` 不变）：

| family | 怪 | 主题 |
|---|---|---|
| `orc_crimson` | charger | 猩红战狂，骨白角饰 |
| `goblin_amber` | summoner | 琥珀金袍（与 shaman 紫区分） |
| `necro_rot` | necromancer | 腐绿死灵袍，骸白饰 |
| `frost_ice` | ice_warden | 冰蓝铠，雪白高光 |
| `blood_crimson` | blood_priest | 暗血教袍，金边，血珠红 |
| `void_dark` | dark_mage | 暗紫术袍，幽绿魔光 |

**4 把新武器**（6×24 竖条像素串，仅允许已定义字符）：`lance` 长矛（旗缨）、`scythe` 镰刀、`tome` 竖持典籍、`bloodstaff` 血杖（红珠滴血）。冰卫复用 `greatsword`、暗术师复用 `staff`。

**MONSTERS 映射与 tier**（ppu 全档 0.8，TIERS 已于批次3 修复）：

| id | family | tier | weapon |
|---|---|---|---|
| charger | orc_crimson | standard | lance |
| summoner | goblin_amber | runt | tome |
| necromancer | necro_rot | gaunt（瘦高） | scythe |
| ice_warden | frost_ice | bulk（厚甲） | greatsword |
| blood_priest | blood_crimson | standard | bloodstaff |
| dark_mage | void_dark | gaunt（瘦法师） | staff |

**零 diff 回归门**：修改生成器后先 dry-run 全量 14 怪到临时目录，旧 8 怪的 40 PNG + 8 JSON 必须与磁盘**逐字节一致**（验证 TIERS 0.8 修复下确定性输出未漂移）；任何 diff 立即停止并排查，不得带 diff 进入正式生成。正式生成同样要求旧文件零 diff。

## §2 注册链（与批次3 同构，零 C++ 改动）

1. 正式生成 30 PNG（6×5）+ 6 骨架 JSON（`resources/animations/mon_*_skeleton.json`，ppu 0.8）。
2. `resources/sprites.json` `skeleton_parts` +30 键（105 → 135），DIMS 与批次1 一致：torso(28,20) head(22,20) arm(11,16) leg(22,20) weapon(6,24)。
3. `resources/animations/actor_avatars.json` 白名单 +6（20 → 26），anim 复用 `player_anim.json`。
4. `tests/animation/animation_test.cpp` expected 集合 20 → 26，注释改“批次1-4”。
5. `reports/mon_humanoid_sheet.png` 更新（14 行含旧 8 怪，供评审）。

## §3 验证与交付链

1. dry-run sheet 评审（用户过目美术）→ 正式生成 + 零 diff 门。
2. `cmake --build build` + ctest 全量 68 项 + `python tools/world_validator.py` 0/0。
3. sim A/B：备份新白名单 → `git show HEAD` 换旧（20 键）→ `--sim 12 --sim-seed 3` → 还原 → 输出逐字节一致。
4. README CHANGELOG 批次4 条目（含验收点、necromancer 双身份说明）。
5. 桌面测试包同步（镜像目录 + 根文件 + 根目录 exe，保留 `saves/`、`3D模式.exe.lnk`）。
6. 用户实机验收后 `git commit`（不擅自提交）。

**实机验收点**（最短路径）：监狱 1-5 层挑战房第3波（charger+summoner）、火山 6-10 层挑战房第3波（necromancer）、深渊 11-15 层挑战房第1/2波（dark_mage、ice_warden+blood_priest）。

## §4 风险与边界

- **necromancer 双身份**：Boss 版走 `boss.cpp`/`boss_necromancer` 静态图，本批 `mon_necromancer` 仅影响挑战房小怪版（同批次3 golem 先例），无冲突。
- **字符 KeyError**：武器/色板行含未定义字符会在 `part_image` 抛 KeyError（批次2 教训），dry-run 先暴露。
- **函数/行数规范**：生成器保持 ≤300 行、函数 ≤40 行（新增均为数据常量，不新增函数）。
- **runt 与 standard 数值等价**：ppu 统一 0.8 后 `runt` 仅语义标签，不产生尺寸差；如未来恢复尺寸分层需另行设计。

## 验收标准（DoD）

- 旧 8 怪生成产物零 diff；新增 30 PNG + 6 JSON 落盘且 ppu 0.8。
- sprites 135 键 / 白名单 26 键 / 测试 26 expected；ctest 68/68、validator 0/0、sim 逐字节一致。
- CHANGELOG 已更新、桌面包已同步、用户实机验收通过、已 commit。
