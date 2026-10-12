<p align="center"><img src="assets/brand/roguelike.png" width="160" alt="回响深渊 Abyssal Echo 剑与门图标"></p>

<h1 align="center">回响深渊 Abyssal Echo</h1>

<p align="center">
  <b>一个会学习你的 Roguelike — C++17 + Raylib 5.0</b><br>
  15 层随机地牢 · 30 种怪物 · 5 场 Boss 战 · 最终 Boss 会复制你的武器、预判你的套路
</p>

<p align="center">
  <!-- TODO(GIF): 首图位 — F15 镜像 Boss 战斗 GIF (10s, 960x640), 录制清单见 docs/RELEASE_CHECKLIST.md 附录A -->
  <img src="docs/screenshots/v1.5.0_F6_volcano.png" width="480" alt="F6 火山群系 — 暖棕地牢与点燃的桶"><br>
  <sub>F6 火山群系（HD-2D 3D 表现层）· 更多: <a href="docs/screenshots/v1.5.0_F1_prison.png">F1 石灰监狱</a> / <a href="docs/screenshots/v1.5.0_F11_abyss.png">F11 深渊</a></sub>
</p>

---

## 这是什么游戏？

**随机地牢 + 动作战斗 + 跨局成长** 的 Roguelike：

- **每局都不一样** — BSP 随机生成 15 层地牢：遗忘监牢 → 灰烬火山 → 虚空深渊，三章各有专属贴图、怪物池与 BGM
- **动作战斗** — 5 类武器三段连击、命中形状各不相同（扇形斩/穿透矛/追踪双截棍/真弹幕连弩），震屏+顿帧+飘字打击感三件套
- **构筑成长** — 火/冰/毒元素三选一、22 个技能（每个 3 级进化）、60 圣物、21 武器、局外 10 节点永久成长，12 流派 Build
- **招牌玩法** — F15 终焉回响：**它观察你、学习你，然后用你的方式击败你**。它会复制你的武器技能、记住你的走位习惯、预判你的下一招；你的习惯越固定，它越强

> 死亡即重开。三存档槽独立记录镜像记忆 — 三个档位 = 三种玩法 = 三套"它对你了如指掌"。

## 快速开始

**方式一 · 即玩（推荐）**：GitHub [Release 页](https://github.com/DIO07-0309/roguelike-cpp-3d/releases) 下载 zip → 解压 → 双击 `roguelike_cpp.exe`。exe 自包含运行库，无需任何环境。

**方式二 · 源码构建**（第三方库已入库，clone 即编译）：

```bash
git clone https://github.com/DIO07-0309/roguelike-cpp-3d.git
cd roguelike-cpp-3d
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
build/roguelike_cpp.exe
```

需要 CMake 3.16+ 与 MinGW-w64；`vendor/`（raylib 5.0 + nlohmann/json）随仓库提供。

> 第一次进入游戏会有 **11 步新手教程**（捡/用/穿/打真实闭环）和 6 条首遇提示，跟着走即可上手。

## 操作

| 按键 | 功能 |
|------|------|
| **WASD / 方向键** | 移动（八方向） |
| **空格** | 普攻（武器三段连击） |
| **Shift** | 翻滚闪避（按住方向键定翻滚方向，0.7s 冷却） |
| **1~4** | 主动技能 |
| **E** | 拾取 / 开门 / 触发房间 / 下楼 |
| **B** | 背包（X 装备 · T 出售 · U 使用） |
| **R** | 圣物面板 |
| **N / C / F / T** | 新游戏 / 继续 / 选关 / 教程 |
| **M / G / F1** | 小地图 / 全屏 / 事件日志 |

## 游戏循环一图流

```
进入随机层 → 探索(FOV 战争迷雾) → 战斗/拾取/特殊房 → 清层下楼
     ↑                                                    ↓
     └── 死亡 → 局外成长(10 节点) + 结局收集 ←── F5/F10/F15 Boss ←── 强化(Build 成型)
```

- **11 类特殊房间**：祭坛/宝箱/泉水/商店/铁匠/图书馆/赌徒/神殿/隐藏密室/地标/挑战房（钥匙开启 3 波奖励战斗）
- **进房锁门**：踏入有怪房间门会封锁，清完自动开 — 别慌，这是特性不是 bug
- **门有四种状态**：开拱门 / 木门（E 开）/ 红锁（战斗封锁）/ 紫封印

## 内容规模

| | | | |
|------|------|------|------|
| 敌人 **30 种** | 一怪一图 | Boss **5 场** | F5 三选一随机 |
| 技能 **22** | 16 主动 + 6 被动 | 圣物 **60** | 含跨局收藏 |
| 武器 **21** | 5 类命中形状 | 结局 **5** | 条件触发 |
| 楼层 **15** | 3 章 × 5 层 | 群系 **3** | 贴图/BGM/怪池全套 |

## 常见问题

**Q: 死了就什么都没了？**
局内装备/等级清空（肉鸽本份），但结局收集、历史最高层、镜像 Boss 对你的记忆都是**跨局持久**的。

**Q: Boss 怎么会正好克制我？**
F15 镜像 Boss 读你的行为画像（攻防倾向/走位偏好/技能习惯）调整策略，且**跨局记忆随存档**。换个打法，它就得重新学。

**Q: 支持 Mac/Linux 吗？**
代码层跨平台，但仅 Windows 实机验证（见 Limitations）。

**Q: 我想改数值/加怪物？**
全部内容数据驱动：`resources/` 20+ JSON 即全部配置（enemies/skills/relics/bosses/…），支持 `mods/` 热插拔加载。改完跑 `python tools/world_validator.py` 校验。

## 技术细节（开发者看这）

- **72 个 ctest 全绿** · 确定性模拟器（同种子字节级复现，`--sim N` 批量平衡评估）
- **AI 研究平台**：行为树 / MCTS / Q-Learning / 镜像学习（行为克隆 + n-gram 战术链 + Thompson 采样五层仲裁）
- **架构**：组合优于继承（5 Director 组合）、EventBus 45 事件、数据驱动 Registry、函数 ≤40 行规范

→ 完整技术文档：**[docs/TECH.md](https://github.com/DIO07-0309/roguelike-cpp-3d/blob/master/docs/TECH.md)** · 架构权威：[docs/ARCHITECTURE.md](https://github.com/DIO07-0309/roguelike-cpp-3d/blob/master/docs/ARCHITECTURE.md) · 开发流水：[CHANGELOG.md](https://github.com/DIO07-0309/roguelike-cpp-3d/blob/master/CHANGELOG.md)

## Current Limitations

诚实清单：

- **美术** — 程序化像素 + Kenney CC0 素材；v1.4.14 起 30 怪全一怪一图 + 三群系专属墙地贴图，但均为生成/占位级，非商业美术
- **手感** — 实时动作（攻击间隔 0.5s），无翻滚/无锁定；打击感三件套就位但数值以研究平衡为主（胜率区间 6-10%）
- **平台** — Windows 实机验证；macOS/Linux 构建规范见 `docs/G4_PLATFORM_BIBLE.md`，未实机验证
- **输入** — 键盘 only
- **确定性已知项** — sim 批量间歇分岔已于 v1.4.15 修复（P1-C8/RNG-002：VFX 视觉掷骰误吃 gameplay rng 流，32 批并行验证零分岔）；`combat_coordinator.cpp` 时停 pre_hp 裸指针快照列清理候选
- **截图/GIF** — README 已有 HD-2D 实机截图（F1/F6/F11 三群系）；GIF/Trailer 仍为占位，录制清单见 `docs/RELEASE_CHECKLIST.md` 附录 A（v1.5.x 补）

## License

- 代码：MIT
- 素材：Kenney CC0（[kenney.nl](https://kenney.nl)）+ 程序生成（`tools/m5_sprite_gen.py`）
- 字体：Noto Sans CJK SC（OFL-1.1）

## CHANGELOG

- G42（开发版，未发布）：**渲染路径整治 + 绘制分发优化** —— 绘制路径 20 趟全量遍历收敛为 1 趟，6 个渲染函数拆分达标，全仓超规 **91→87**。

  - **★ 主要性能问题：绘制路径 20 趟全量遍历**。实测 F3 每帧 274 个绘制项，被完整遍历 20 次 = **5480 次迭代/帧**（约 1.1 万次 kind 判断），而每个项只命中一个 kind——**99% 的判断是纯浪费**。分布：`_draw_scene` 逐 kind 13 趟 + `_draw_terrain_pass` 3 趟 + `_draw_fx_pass` 逐 FX kind 4 趟。
  - **改法**：单趟 `switch` 分类到 18 个指针桶（`_classify_draw_items()`），各 pass 直接吃桶，自身遍历归零。绘制顺序逐一保持原样（顺序 = 深度/混合顺序，不能乱）。收益：每帧少 5200+ 次无用判断；桶容量复用，无堆分配抖动。
  - **★ 重构中踩到并修正一个自己引入的顺序陷阱**：第一版把分类留在 `_draw_scene` 内部、位于 `_draw_terrain_pass()` **之后**，导致 terrain pass 读到空桶——**地形会整个消失**。故抽出独立 `_classify_draw_items()` 并在所有 pass 之前显式调用，函数注释标明该前置条件。**教训：重构时"调用顺序"和"代码顺序"是两件事，抽取顺序错位不会报错、只会静默丢画面。**
  - **函数长度整治（6 个渲染函数）**：`_build_terrain` 166→40（拆 8 个单一职责助手：视野/贴图/墙/门/地板/特殊房间/楼梯/Boss红雾，另抽 `_unexplored_dim` 供三处共用）、`_draw_door_panel` 65→30（4 助手，两个 axis 的重复 `rlBegin/rlEnd` 序列合并）、`render_frame` 58→38（拆出 `_debug_cycle_weather`，发布版直接 return false）、`_draw_scene` 41→30、`_build_boss_skill_warnings` 86→24（4 个按技能分的助手 + 公共角度函数）。
  - **★ 拆分 `_build_boss_skill_warnings` 时差点引入行为变更**：原代码两个扇形预警都带 `&& view.player` 条件，初版把角度函数设计成「无玩家返回 0 度」会让玩家丢失时**仍创建预警项**。已改为返回 `bool`、调用方据此跳过，保留原语义。
  - **验证**：ctest **82/82**；全仓函数长度违规 **91→87**；3D 实机截图逐张比对，墙/地板/门/楼梯/特殊房/传送门/氛围粒子渲染与整治前一致；探针实测 274 项分类无遗漏无重复；Boss 技能预警经用户实机验收「预警圈正常」。
  - ⚠️ **逐字节截图对比不可行**：实测同构建两次运行哈希不同（`visual_rng` 种子跨运行不一致，粒子/天气导致），故改用「探针验证分类正确性 + diff 审查保序 + 视觉核查」三件套。**若日后要让渲染可回归比对，需先让 `visual_rng` 支持固定种子。**
  - 剩余同类超规（未拆）：`fx_bolt`(57)、`fx_generic`(54)、`_build_special_rooms`(51)、`DoorRenderer::draw_door`(51)、`fx_poison_shadow`(50)、`fx_fire_ice`(49)、`_draw_weather_particles`(47)。

- G41（开发版，未发布）：**流派主题预览工具** `--theme-preview` —— 一条命令产出 12 个流派的对比预览图，终结"验证视觉改动要进游戏找怪触发"。

  - **用法**：`roguelike_cpp.exe --theme-preview`（2D）或 `--theme-preview --hd2d`（3D）；配 `--hidwin` 后台跑不弹窗。产出 `saves/preview_NN_<build>.png` 共 12 张，**拍完进程自动退出**。每个流派的全部手感数值同时写进 `game.log`（`[PREVIEW]` 行）——静态图看不出顿帧/震幅/音效，靠日志对照。
  - **场景构造只让主题当唯一变量**：固定 5 组装饰特效沿玩家横线一字排开（爆炸 / 火花 / 闪光 / 烟雾 / 震波），间距 90 世界单位。布局刻意不上下散开——3D 相机俯角下靠边的组容易被墙挡住（实测过）。
  - **为什么需要这个**：G39/G40 改的都是"看不出数值差异"的参数（屏震幅度 / FOV / 特效半径 / 音效），逐项实机验证要进游戏找对应怪物触发，成本极高且无法横向对比。现在 12 个流派同场景同特效各拍一张，直接可对比。
  - 新增 `ThemePreviewController`（`src/game/director/theme_preview.{h,cpp}`，单一职责：只管何时切换/是否拍完，不含渲染与场景知识）+ `GameScene::_stage_theme_preview_fx()`（造场景 + 求截图）。
  - **`autoshot` 输出名改为可覆盖**（`g_autoshot_path`，空 = 原 `saves/screenshot.png`，向后兼容），主题预览复用它触发截图，不新增导出路径。
  - **踩坑记录（4 个，都是实测出来的）**：
    1. **开场字幕吃掉全部截图**：`--theme-preview` 从楼层进入开始拍，12 张全拍在"第一章 Chapter 1"满屏字幕上，特效被遮住。已加门槛：等 `floor_intro_active`/`chapter_intro_active` 结束才开拍。
    2. **全局量不能定义在 `main.cpp`**：测试目标 `roguelike_lib` 用 GLOB 收 `src/**/*.cpp` 但**显式排除 `main.cpp`**，定义在那儿会让所有测试链接报 undefined reference（实测 `hd2d_scene_builder_test` 挂掉）。已移到 `theme_preview.cpp`。
    3. **新增 .cpp 必须重新 `cmake -B build`**：`file(GLOB_RECURSE)` 在配置期求值，只 `--build` 不 configure 会链接 undefined。
    4. **`_process` 已 41 行，不能再加一行**：预览调用最初挂在 `_process` 里，直接把函数顶到 42 行违规。改为挂到 `_detect_build_fusion()` 内部（它必须跑在 build 检测之后才能覆盖主题），并让控制器自取 `GetFrameTime()` 免传 dt。
  - 门禁：Release 0 error · ctest **82/82** · 函数长度扫描违规 **91→91**（未新增，`_process` 保持 41→40）· 2D 与 3D 两种模式均实测产出 12 张且进程自动退出。

- G40-3（开发版，未发布）：**VFX 尺寸接线 + 找回丢失的 `freeze_bonus`** —— `explosion_scale` / `spark_scale` 首次生效；同时修复第一批一个字段被误回滚的问题。

  - **`explosion_scale` / `spark_scale` 接线**：新增 `Effect::themed` 标记（默认 `false` = 不缩放），仅 5 个纯装饰 primitive 置 `true`：`explosion` 的火花、`spark_burst`、`smoke_puff`、`flash`、`shockwave` 的内层 flash。在 `VFXServer` 的 `_ef()` 统一出口按 `spark_scale`（spark 类）或 `explosion_scale`（其余）缩放半径。
  - **★ 刻意不动 `ring()`**：它同时服务 Boss 电报（`boss_cone/circle/summon`）和玩家攻击范围环（`ctx.geo.range_px`），缩放假环会让玩家误判危险范围。AI 预警圈、Boss 震波走裸 `push_back`，天然不受影响。
  - **未接线的剩两个 + 两个 Screen 字段**（`beam_width` / `ring_speed` / `edge_tint` / `vignette_strength`）：前两者无对应 `Effect` 字段，需改绘制路径；后两者需改 HUD/overlay 路径。`UnwiredVfxFieldsAreStillDocumented` 测试显式标记，防止误以为已接。
  - **★ 本批自查发现 G40 第一批的 `freeze_bonus` 早已丢失**：当时做变异测试后用 `git checkout -- presentation_system_director.cpp` 恢复变异，把**尚未提交**的 `freeze_bonus` 改动一起回滚了。而我此前的"已入 HEAD"校验搜的是 `_fill_theme` 里的**写入行**（`t.camera.freeze_bonus=fb`，G5.8.2 就有），是**假阳性**——搜写入行不能证明已接线。已补回，并把校验改成搜**消费语句**（`freeze_timer = duration + ...`）。**教训与 G40-2 的 `git checkout` 事故同源：未提交的改动 + `git checkout` = 静默丢失；而验证必须搜消费点、不能搜字段名。**
  - 本批还因 PowerShell 写文件损坏过一次编码（中文注释变乱码致编译失败）和误删过一次 G40-2 CHANGELOG 条目，均已用 git/Edit 工具修复。**教训：批量改文件内容一律用 Edit 工具，不要用 PowerShell `Set-Content` 重写整个 .cpp。**
  - 真值表 `ExplosionAndSparkTruthTable` 锁 11 个流派的两个字段，值由**运行时探针导出**（照源码猜错过两次：猜的 ICE_MAGE 1.15/1.00 实际是 BERSERKER 的）。变异测试反证：SHADOW 的 `explosion_scale` 0.90→1.00 被抓。
  - 门禁：Release 0 error · ctest **82/82** · 函数长度扫描违规 **91→91**（未新增）。

- G40-2（开发版，未发布）：**VFX 速率/时长接线 + 特效 tick 重构** —— `particle_speed` / `lifetime_scale` 首次生效，并把逐字复制的特效生命周期循环收敛为单一入口。

  - **先做可 Maintenance 重构**：`_tick_arena_tail` 与 `_tick_player_combat` 里各有一份**逐字相同**的 4 行特效 tick（`elapsed += dt` + erase）。两份必须同步改，漏一处就出"竞技场特效永远粘住"。已抽为 `GameScene::_tick_active_effects(dt)` 唯一入口，`fx.elapsed += dt` 全仓仅剩 1 处。
  - `particle_speed` → 乘进 `elapsed` 推进速率。冰系 0.70（最慢）/ 雷系 1.40（最快）/ 默认狂战 1.10。
  - `lifetime_scale` → 乘进"何时移除"这一单一判据（`elapsed >= duration * life_scale + start_delay`）。暗影 1.15（残影拖尾更久）/ 雷系 0.85（最短）。
  - **刻意只接这两个、暂不接另外四个**：`explosion_scale` / `beam_width` / `ring_speed` / `spark_scale` 会缩放特效的**半径/尺寸**，但同一个 `active_effects` 列表里混着 AI 预警圈、Boss 震波半径、弹道轨迹线等 **gameplay 电报**——缩放后者会让玩家误判危险范围。这四个要么给 `Effect` 加"是否电报"标记，要么在绘制端维护排除名单，属独立设计决策，未讨论清楚前不动。
  - 两个字段都加了防呆：非正值回落 1.0（否则特效会永久卡住或时间倒流）。
  - **`UnwiredVfxFieldsAreStillDocumented` 测试** 把这四个 + 两个 Screen 字段显式标记为"仍是死数据"，防止后人误以为已接线。
  - 真值表测试 `VfxRateTruthTable` 锁全部 12 流派的 `particle_speed`/`lifetime_scale`。**它当场抓住我自己的两处转录错误**（JUGGERNAUT 误写 0.75 实为 0.85、SUPPORT 误写 0.85 实为 1.00）——这正是真值表该干的事。
  - 变异测试两次反证：ICE 的 `particle_speed` 0.70→1.00 被抓；SHADOW 的 `lifetime_scale` 1.15→1.05 被抓。
  - 运行时取证：sim 实跑 12 次采样，`speed=1.10` 时 elapsed 推进明显快于 duration；`speed=1.00/life=1.05`（未成型前的默认主题路径）表现不同，证明两个乘数都真实参与计算。
  - ⚠️ **操作失误记录**：取证后用 `git checkout --` 移除探针，把该文件里尚未提交的 G40 重构**一起回滚**了（`git checkout` 不区分探针与正式改动）。已按历史编辑重做并验证。教训：**探针必须与正式改动分次提交，或用 sed 精确删除**，绝不用 `git checkout` 清探针。
  - 门禁：Release 0 error · ctest **82/82** · 函数长度扫描违规 **91→91**（未新增）。

- G40（开发版，未发布）：**BuildTheme 打击感接线第一批** —— `freeze_bonus` / `zoom_offset` / `hit_sfx` / `skill_sfx` 四个字段首次生效。

  - **这四个字段自 G5.8.2 起就是死数据**（`_fill_theme` 里只写，全仓零读取）。G39 已救活 `shake_scale` 与 `dash_offset`，本批救活剩下四个，BuildTheme 的 17 个字段至此从"设计好了但没接"收敛到 11 个已活。
  - ⚠️ **机制澄清（写错后由用户纠正，记录以免再犯）**：BuildTheme 跟的是 `BuildType`，而 `BuildType` **不是玩家开局选定的**——玩家开局只能选元素核心三选一（`FIRE/ICE/POISON`），流派是由 `calculate_build()` 按「技能 tags + 被动 + 圣物 favorite_tags + 当前 Buff」实时累加算出的，**成型前主题固定为默认的 BERSERKER**（`_new_game()` 里 `set_build_theme(BuildType::BERSERKER)`，且主题仅在 `bt != NONE` 时更新），成型/变化时才会切主题并弹「BUILD COMPLETE!」/「BUILD CHANGED」。**所以本批的效果不是"选一个流派获得一套手感"，而是"打击感/听感跟随当前推算出的流派身份演变"。**
  - ⚠️ **`hit_sfx` / `skill_sfx` 跟随可变主题是一个主观取舍**：流派一变音效就跟着变（变化点有「BUILD CHANGED」文字 + 震屏 + 顿帧陪伴，不是无故突变），但相比 `shake_scale`/`zoom_offset` 的渐进视觉变化，**音频突变更显眼**。若实机觉得违和，可考虑把这俩改为仅在流派成型时锁一次，或退回硬编码音。
  - `freeze_bonus` → `PresentationSystemDirector::trigger_freeze()` 内部统一加成。**刻意放在这一个点而不是各调用方**：字段语义是"该流派额外顿帧时长"，与触发原因无关；单点接入也让 14 个 `trigger_freeze` 调用点自动全部生效（时停系 +0.06s / 暗影 +0.04s / 冰系 +0.02s / 其余 0）。
  - `zoom_offset` → `HD2DRenderer::set_camera_zoom()`，在 `_apply_camera_fov` 里乘进基准 FOV（暗影 −0.05 拉远 / 狂战 +0.03 拉近）。沿用 `set_camera_dash` 的同款"调用方每帧刷新"模式，未给 SceneView 增加字段。
  - `hit_sfx` → `PlayerController::_apply_post_hit` 的普攻命中音。原硬编码 `"hit"`；四个音效名（melee/hit/slash/bolt）都在 `AudioServer::init` 里程序化注册过，不会静默回默认。
  - `skill_sfx` → `CombatCoordinator::use_skill` 的技能音。**这是一次有意的语义重定义**：原来按 `_skill_id` 硬编码 slash/bolt 两类（近战技→slash、法师技→bolt），现改为"跟着当前流派走"，`self_heal` 保留治疗专属音不随流派漂移。原分类的意图（近战/法师区别）恰好与流派 signature 一致，但现在是**同一技能在不同流派下听感不同**。
  - **测试 8→12 个**（`combat_feel_test`）。新增 `FreezeBonusDifferentiatesBuilds` / `ZoomOffsetDifferentiatesBuilds` / `AudioSignaturesAreValidAndDistinct` / `AudioSignaturesDifferAcrossBuilds`。注意：测试里枚举 12 流派只用于**锁数值表与代码路径覆盖**，不代表玩家会逐个经历这些流派。
  - **★ 变异测试逼出一次测试设计缺陷（值得记住）**：初版 `AudioSignaturesDifferAcrossBuilds` 只断言"存在 2 种以上 signature"，注入"把 2/6 个流派的音效抹平成常量"的变异后**测试依然通过**——断言太弱，等于没锁。已改为**显式真值表**（12 流派逐一钉死 `hit_sfx`/`skill_sfx`），并要求三类 signature 各出现一次。改后同一变异立即被抓。**教训：只验证"有多种取值"的断言锁不住退化，要锁真值表。**
  - 真值表之外另做了两个变异的反证：抹平 `zoom_offset`（全改 0）→ 被抓；`combat_feel.h` 的 shake 分级（G39）也复验过两次。
  - 运行时取证：临时探针导出 12 流派全部 `freeze_bonus`/`zoom_offset`/`hit_sfx`/`skill_sfx` 实际取值，与真值表逐项吻合；探针已移除且确认不在 HEAD。
  - 门禁：Release 0 error · ctest **82/82** · 函数长度扫描违规 **91→91**（未新增）。

- G39（开发版，未发布）：**3D 手感打磨第一批** —— 相机跟随/屏震/FOV 冲击三件套，并把打击感接上流派主题。

  - **3D 相机改为平滑跟随**（`HD2DRenderer::_update_camera_focus`，从 97 行的 `render_frame` 抽出）：原先 `_camera_focus` 每帧**直接赋玩家坐标**，玩家像素级抖动会 1:1 传进镜头。现在按 `dt` 插值跟随，手感从"用钉子固定"变成"有阻尼地跟"。
  - **★ 位移过大必须吸附，不能一律插值**（`kFocusSnapDistance = 512` = 16 tile）：首帧 `_camera_focus` 是 `{0,0,0}`、换层/传送时玩家坐标跳变几十格，无脑插值会从世界原点或上一处**拉一条长拖影飞过来**，看起来像画面故障。阈值内才插值，这是第一版实机截图时识别的（`--hd2d --hidwin --autoshot 250 --autocontinue` 取证）。
  - **3D 屏震从"帧内消费即清零"改为目标值平滑逼近**：`set_camera_shake` 原先直接覆盖 `_shake_offset`，每帧幅度随机跳变 → 画面像信号干扰；现在写 `_shake_target`，渲染值每帧 lerp 靠近，震动有起有落而非高频闪烁。
  - **3D FOV 冲击**（`_apply_fov_pulse`）：震幅 → FOV 短暂收拢（每单位偏移 0.0022，上限 6%），打击越重镜头越"怼脸"，震动结束自动归位。**刻意只吃本帧已算好的 shake，不新增 gameplay 输入**——表现层不需要知道"为什么震"，只需要知道"震多大"。
  - **★ 普攻此前只有顿帧、没有屏震**（`PlayerController::_apply_post_hit`）：命中分级只调 `trigger_freeze`，`CombatFeelSystem` 里成对的 `SHAKE_LIGHT/COMBO/CRIT/MEDIUM` 四个常量**从未被调用**（死数据）。本批按 轻→重→暴击→击杀 四级补上，并让 `BuildTheme::camera.shake_scale` 首次真正生效（火 1.3 / 雷 1.15 / 冰 0.8 / 影 0.7），**Build 之间终于有手感差异**——此前该字段同样是死数据。
  - **★ 翻滚 FOV 拉伸接上 `BuildTheme::camera.dash_offset`**（`GameScene::_dash_fov_degrees` → `HD2DRenderer::set_camera_dash`）：该字段自 G5.8.2 起**只写不读**，是第三块死数据。翻滚只有 0.16s，故 FOV 冲击也设计成快起快落的短促拉伸（响应 30/s，全程约 0.26s），读起来是一次"视野弹开"而非持续广角。
  - **dash_offset 原值不能直接当角度用**：4~14 直接映射最大 = 28% 视野变化，会晕。乘 `kDashFovPerTheme = 0.35` 再 clamp 到 6°：火 2.8° / 雷 4.2° / 影 4.9° / 狂 2.1° / 血 1.4° / 冰·辅助·召唤 0°。**映射后流派差异完整保留**，且"稳"流恒为 0 是合理的——它们本来就没有冲刺感。原值从未校准过，此处按安全区间重新标定并注释了依据。
  - 分级聚合抽成 `_hit_feel_seconds(ctx, shake)`（输出参数约定与 `ElementResolver::resolve` 一致），`_apply_post_hit` 31→28 行。
  - Boss 运镜数据微调（`resources/camera/boss_camera.json`，`camera_test` 断言同步）：`lerp_speed` 2.0→2.6（运镜更跟手）、`focus_duration` 2.0→1.6（聚焦 Boss 不抢玩家太多时间）、`kill_stun.duration` 0.08→0.10 / `amplitude` 3.0→3.6 / `frequency` 20→22（击杀更响）。
  - 门禁：Release 0 error · ctest **81/81** · 函数长度扫描违规 **91→91**（未新增；`render_frame` 97→58 行，新增四个 helper 均 ≤40 行）· 3D 实机截图确认渲染正常、相机无飞影。
  - ⚠️ **三项手感均未经实机主观验收**：平滑跟随/FOV 冲击/翻滚拉伸都能用自动化证明"不崩、不改变逻辑"，但"跟手还是拖沓""怼脸还是过头"只能人玩。建议实机重点感受：普攻连段（三段震幅递增）、不同 Build 的翻滚弹开幅度、Boss 战运镜是否还抢时间。

- G32-G（开发版，未发布）：**AI 路线结项** —— 文档与代码状态对齐，RL/神经网络路线正式封板。

  - **新增** `docs/AI_RL_ROUTE_CLOSEOUT.md`：G32 系列结项报告。记录最终胜率对比、6 项有效改进（课程学习 / Double DQN / Observation 8→14 维 / bootstrap 过 mask / parity 闸门 / 手写推理替代 ONNX Runtime）、**3 个被证伪的假设**、关键 bug 与工具链陷阱、以及重开此线的前置条件。
  - **修订** `docs/AI_LEARNING_GUIDE.md`：第 14 章原写「Q 表 → DQN」是待办升级方向，现已实测证明为**负收益**，故在文首、目录、第 13 章路线图 17/18 项、第 14 章开头四处加警示，标注**保留为教学材料但勿照此改造项目**。
  - **修正了一个误诊的教训**：「`wait` 占比高 = 策略退化」是错的——基线策略 `wait` 占 91.6% 却赢 60%（普攻冷却 3.4 步、技能冷却 32~320 步，冷却期本来就只能等）。这个误诊导致四轮迭代方向完全错误。**教训：诊断指标必须先与基线对照。**
  - 本批仅改文档，**零代码改动**，不影响任何运行时行为。

- G32-F（开发版，未发布）：`TacticalPolicy` 接入镜像 Boss，**默认关闭**（命令行 `--tactical-mirror` 开启）。

  - **接入位置**：`boss_system_director.cpp:_init_mirror_boss()`，在 RL 层注入之后调用 `mirror::install_tactical_predictor_if_enabled(*_mirror_agent)`。装入的是 **`MlPredictor` 槽（仲裁链最高优先级）**，故会盖过战术链 / RL / 克隆 / Thompson 所有层——这是刻意的：G32-D 实测规则策略 60% 优于学习型策略（DQN 36.7% / PPO 18.9%），故让它是 ML 槽的替代实现而非叠加层。
  - **全局开关** `mirror::tactical_mirror_enabled()`，由命令行 `--tactical-mirror` 置位。**默认 false**，此时镜像 Boss 行为与接入前**逐位一致**——保证这不是一次未经验收的行为变更。新增测试 `GlobalSwitchDefaultsOff` / `GlobalSwitchEnables` 锁定该契约。
  - ⚠️ **自动化无法验证**：sim 模式只打到 2 层（`summary.avg_floor = 2.0`，`boss_kill_counts` 全 0），**从未触发 F15 镜像 Boss**，故 `game.log` 里没有任何 `[MIRROR]` 记录。这是 sim 的既有局限（G30 CHANGELOG 已记录「BOSS_CINEMATIC 路径未被确定性校验覆盖」）。**需人工打一场 F15 镜像 Boss 验收**：`roguelike_cpp.exe --tactical-mirror`，进 F15 后确认 `game.log` 出现 `[MIRROR] TacticalPolicy active`。
  - **回退方式**：去掉 `--tactical-mirror` 参数即可，零残留（该开关不写存档、不改任何持久化数据）。
  - 测试 14/14，ctest **81/81**。

- G32-E（开发版，未发布）：RL 合法性约束修复重做 —— 上一批修复曾被 `git checkout` 回退（并行开发所致），本批重做并加版本门。

  - **背景**：检查发现 G31-B1~B5 五个修复只剩 1 个（`exploit_action` 的 mask），其余 4 个被回退。其中 **G31-B3（`to_key` 补字段）被回退的后果最直接** —— 键格式从 8 字段变回 6 字段，旧 Q 表存档再次失效，`load()` 无版本检查会静默读到错位的值。
  - **重做的五项**：
    | 编号 | 缺陷 | 修法 |
    |---|---|---|
    | B1 | `update()` 的 `max_next` 遍历全部 10 个动作，把非法动作价值算进 bootstrap | 新增 `_bootstrap_max()` 只遍历 `next_legal`；参数**必传**，让"忘记传 mask"变成编译错误 |
    | B2 | `exploit_action()` 游戏内路径无 mask，会选中冷却中的技能 | 新增 `mirror_legal_actions()`，只 gate `boss_can_attack` |
    | B3 | `to_key()` 只编码 6 字段，`strongest_hp_ratio`/`buff_count` 未参与，不同状态撞同一 Q 表条目 | 补齐 8 字段（buff 上限截断 9）；`save()` 写 `"v": 2`，`load()` **明确拒绝**旧存档 |
    | B4 | 训练场景怪物距玩家 7/5 格，而 ATTACK 需 `d<2`、技能伤害需 `d<3` → 近战与技能伤害永远触发不了 | slime `d=1` / orc `d=2`，mirror Boss `d=1` |
    | B5 | `exploit_action` 的 -1 被 `combat_action_to_arm(-1)` 吞成 `RETREAT`（=1，**非负**），使 `arm >= 0` 恒真 → RL 层对未见过状态也强行接管且固定后撤 | 先拦 `-1` 再映射；`AI_LEARNING_GUIDE.md:304` 描述的「置信度感知降级」至此才真正生效 |
  - **⚠️ `boss_can_attack` 语义澄清**（写测试时抓到）：它表示「**Boss 能否反击**」（安全性信号），**不是**「玩家能否攻击」。第一版误把它当普攻前置条件，导致 Boss 无法反击时反而不敢输出。修正后它只影响「要不要贪刀」的激进程度。
  - **训练产物改为不入库**（`.gitignore` 新增 7 条）：权重与 `Observation` 维度强绑定（8→14 维即失效），入库会造成「陈旧权重 + 新代码」的静默不匹配。清理了 12 个失效产物（`*.pt`/`*.onnx`/`dqn_weights.bin`/`parity_trace.json`/临时 txt）。
  - **依赖权重的测试改为 `GTEST_SKIP` 而非失败**（`dqn_bridge_test` ×2、`dqn_net_test` ×1）：训练产物不在版本库里，干净检出时 ctest 必须能全绿。
  - **新增回归测试 9 个**，逐条对旧实现反向验证会失败：`ExploitStaysInsideLegalSet`（旧实现无视 legal 返回 ATTACK）、`BootstrapIgnoresActionsOutsideNextLegal`（旧实现 WAIT 的 Q 是 4.5 而非 0）、`LoadRejectsLegacyVersion`、`KeySeparatesStrongestHpRatio`、`KeySeparatesBuffCount`、`KeyClampsBuffCount`、`VectorCarriesPositionAndCooldown` 等。
  - 验证：Release **0 error** · ctest **81/81**（`q_test` 11 + `obs_test` 9 + `env_test` 6 + 其余 55）。

- G32-D（开发版，未发布）：RL 路线的**诚实收尾** —— 三个方向全部做完，结论是「规则策略优于学习算法」，故把规则策略落成可交付代码。

  - **三个方向的实测结果**（全部完成，但未达成目标）：
    | 方向 | 效果 |
    |---|---|
    | 课程学习（3 档难度递进） | 训练 21.7% → 48.4% |
    | Double DQN（治 max 过估计） | 档内胜率 33% → 47% |
    | Observation 8 → 14 维（位置+冷却） | 训练 48% → **69.4%** |
    | 场景随机化 + 技能槽分化 | 暴露出规则基线优势 |
    | PPO（Actor-Critic + GAE + clip） | **18.9%，比 DQN 更差** |
    最终统一口径（`eval_ppo.py`，L2 场景、15 局/变体、独立种子）：
    ```
    DQN        36.7%   attack 2.3%   wait 89.7%
    PPO        18.9%   attack 0.0%   wait 83.9%
    规则基线    60.0%   attack 1.2%   wait 91.6%
    ```
  - **★ 我犯的最大错误：把 `wait 89.7%` 误诊为病灶，花了四轮去消灭它。**
    基线的 wait 同样占 91.6% 却赢 60%——因为普攻冷却 3.4 步、技能冷却 32~320 步，**冷却期间本来就只能 WAIT**。`wait` 占比高是环境的正常形态，不是学习失败。**教训：诊断指标必须先与基线对照**，`eval_ppo.py` 与 `bench.py` 都是为此而生。
  - **PPO 的引入也没解决问题**（`tools/rl/ppo_model.py` + `train_ppo.py`，Actor-Critic 共享 trunk 24→64→64、GAE(λ=0.95)、clip=0.2、entropy_coef=0.01）。原先判断「Q 值 bootstrap 不动点」是 wait 主导的根因，实测该假设不成立——PPO 的 wait 仍占 83.9%。
  - **结论：这个 toy env 不适合用学习算法。** 理由：① 10 个动作里只有「普攻/技能/等冷却」三种有意义，移动价值极低；② 最优策略接近确定性规则（"冷却好了就打，否则等"），10 行规则就能写对；③ 奖励稀疏（胜负在 60~300 步后才出现）。**任务本身接近被规则解决，学习算法无增量价值。**
  - **交付 `TacticalPolicy`（`src/ai/mirror/tactical_policy.{h,cpp}`）** —— 把 RL 训练中确认有效的东西（优先级结构：低血量走位 > 贴身普攻 > 技能 > 靠近 > 等待）用规则实现。可解释（每个决策带中文 `reason`）、可调（`Config` 结构）、**零推理开销、零第三方依赖**。零侵入接入：`mirror::install_tactical_predictor(agent, cfg)`，与 `install_dqn_predictor` 同模式，默认不参与仲裁。
  - **语义澄清（写测试时抓到的）**：`MirrorBattleState::boss_can_attack` 表示「**Boss 能否反击**」（安全性信号），**不是**「玩家能否攻击」。初版误把它当普攻的前置条件，导致 Boss 无法反击时反而不敢输出。修正后它只影响「要不要贪刀」的激进程度，且 Boss 无法反击时置信度更高（安全输出窗口）。
  - **测试 12/12**，ctest **81/81**。含关键契约：优先级正确（贴身→普攻、低血量→闪避、超射程→靠近、冷却中不乱放技能）、`Config` 开关生效、极端输入不崩、**每个分支都给出非空 `reason`**（可解释性）、**默认关闭时仲裁链不变**。
  - **DQN/PPO 的 RL 权重当前失效**（Observation 8→14 维后未重训），`tools/rl/` 下全部脚本保留作为**学习成果与踩坑记录**，不接入生产路径。若日后要真用 RL，需先做真实游戏环境的 Python 镜像（含命中判定、Buff 触发、走位收益），而非继续调这个 toy env。

- G32-B（开发版，未发布）：DQN 接入管道（**默认关闭**）+ REAL_MODE 训练模式 —— 打通到 `MirrorAgent` 的最后一段，零侵入。

  - **★ 零侵入接入是刻意设计，不是偷懒**。第一版写成 `MirrorAgent::set_dqn_policy()`，结果 `mirror_agent.{h,cpp}` 被并行开发的 `git checkout` 还原（实测 5 次，本轮刚加的 `set_dqn_policy` 与 G31-B5 修复一并被冲掉）。改为 `rl::install_dqn_predictor(agent, weights, style)` —— 本模块**不改 `MirrorAgent` 任何一行**，只产出一个 `MlPredictor` 由调用方自己传进已有的 `set_ml_predictor()`。加载失败返回 `nullptr` 且不碰 agent，仲裁链保持原状。**教训：并行分支上不要改对方也在动的文件。**
  - **补齐 G31 遗留的语义鸿沟**：`MlPredictor` 签名是 `MirrorBattleState → PlayerActionType`（玩家**意图类型**），DQN 输出 `CombatAction`（玩家**按键级战斗动作**，含 4 技能槽 + 4 移动方向）。桥接层做两件事：`mirror_state_to_obs()` 复用 `mirror_agent.cpp:13` 的同字段映射；`combat_action_to_player_action()` 把 10 个 `CombatAction` 折叠成 5 个 `PlayerActionType`（4 技能→SKILL、4 移动→MOVE、WAIT→NONE 不算决策动作）。
  - **`WAIT → PlayerActionType::NONE` 是有意的**：NONE 不进 `is_decision_action()`，让 Boss 走常规应对而非被"玩家在发呆"误导。
  - **测试 9/9 通过**（`dqn_bridge_test`），含三项关键保证：默认关闭时仲裁链不变、安装失败零副作用、极端输入（距离 0 / 血量 0 / Boss 死亡）不崩。日志可见默认路径确实落在 `[MIRROR] 在线决策`（Thompson 兜底）而非 ML 层。
  - **REAL_MODE：按 `resources/skills.json` 的真实冷却/射程训练**。玩具环境里 4 个技能冷却全是 3.0（=12 步）且射程 3，而真实是 `slash 2.0s / fireball 5.0s / self_heal 8.0s / the_world 20.0s`——差 10 倍，导致 DQN 学出"48.5% 放技能、1% 普攻"。
  - **REAL_MODE 调试中踩到的三个坑**（都是自己引入的）：① **冷却换算方向写反**——`sec * COOLDOWN_STEP` 应为 `sec / SECONDS_PER_STEP`，写反后 slash 2 秒只给 2 步冷却，技能近乎无冷却，DQN 必然锁死单一技能；② **普攻射程未同步放大**——技能射程 5.0 而普攻仍 2.0 时场景里怪常在 `d>2`，ATTACK 从不出现在合法集里，占比恒为 0%；③ **奖励缺空闲惩罚**——冷却中只能 WAIT 且无代价，于是"放技能然后等"成为最优解，加 `IDLE_PENALTY` 后策略才分散。
  - **⚠️ 策略体检仍未通过，DQN 不可用于生产路径**。`policy_audit.py` 实测：REAL_MODE 下 Boss 域动作分布已分散（单一动作 48.7% < 50%、wait 0.5%），但 **attack 占比仍为 0%**。诊断结论是 ATTACK 在 47% 的状态里合法、却只被选中 14.6% —— **不是 mask 问题也不是网络缺陷，而是技能（1.5x 伤害 + AoE + 射程 5）客观上优于普攻**。已连续三轮调参（技能冷却 / 普攻射程 / 伤害系数 / 空闲惩罚）未能让 attack 上榜，故停止盲调。
  - **体检脚本本身是有价值的产物**：`policy_audit.py` 在单一动作 >50% / wait >30% / attack <5% 时返回非零退出码。训练时打印的胜率（86%）完全掩盖了策略失衡（skill_1 占 74%）——**胜率高不等于策略可用**，这是本轮最该记住的教训。
  - ⚠️ 遗留：① **attack 占比 0% 未解决**，根因是玩具战斗模型里技能强度不合理；正解是把真实战斗公式（武器伤害系数、命中判定、Buff 触发）搬进 Python env，而非继续调参；② `MirrorBattleState` 仍缺技能冷却与坐标，桥接层只能用 `boss_can_attack` 单一门控近似，已在代码注释里标明；③ 权重文件需随游戏分发（当前仅在 `tools/rl/`），接入时应放到 `assets/` 或 `saves/`。

- G32（开发版，未发布）：DQN 训练链路 + C++ 手写推理 —— 路线 A 全线落地。**Python 侧 DQN 贪心 81.5% vs 随机基线 59.8%（+21.7pp）；C++ 手写前向与 PyTorch 差 1.03e-06；四道跨语言闸门全通过。**
  - **注**：此处记录的是**玩具模式**（`CombatEnvironment` 原样）下的成绩，DQN 胜率 81.5%。后续 G32-D 用真实参数重训时结论反转——见 G32-D。

  - **推理不用 ONNX Runtime，改手写前向**（决策依据：网络只有 3 层全连接 18→64→64→10、**138 个参数**，手写约 80 行；ONNX Runtime 要往 `vendor/` 加约 15MB dll，违反项目约束「vendor/ 仅允许 raylib + nlohmann/json」）。新增 `src/ai/rl/dqn_net.{h,cpp}`（权重文件解析 + 逐层前向）与 `src/ai/rl/dqn_policy.{h,cpp}`（18 维输入构造 + masked argmax）。**零第三方依赖、零运行时开销。**
  - **权重格式自定义**（`tools/rl/export_weights.py` 生成）：`"RLDQ" | version | n_layers | pad`，每层 `in_dim | out_dim | bias[out] | weight[out*in]`，权重索引 `o*in_dim + i` 与 `torch.nn.Linear` 的 `[out_dim, in_dim]` 行优先一致。加载时校验 magic / version / 层间维度首尾相接 / 维度上限，坏文件一律拒绝而非静默读脏。
  - **四道跨语言闸门**（`tools/rl/check_all.py` 串起来，依次跑）：
    | 闸门 | 防的错误 |
    |---|---|
    | `parity_test.py` | C++ `CombatEnvironment` ≡ Python 移植的数值漂移 |
    | `verify_weights.py` | C++ 前向 ≡ PyTorch（权重布局倒置 / 漏 ReLU） |
    | `verify_policy.py` | C++ 输入缩放 + masked argmax ≡ Python（**Q 值对但动作选错**这类静默错误） |
    | `dqn_net_test` | 结构校验 / 坏权重拒绝 |
    全部 PASS。当前数据：parity 12 局 615 步 bit-exact；前向最大绝对差 1.03e-06；`build_input` 最大绝对差 4.98e-08、选中动作 **0 处不一致**。
  - **`DqnPolicy` 把 mask 规则集中在一处**，理由：网络只输出 Q 值、**不做动作选择**，而 mask + argmax 必须与 Python `action_mask.py` 完全一致。这套规则散落到调用方就会出现「Q 算对了但动作选错」的静默 bug。
  - **★ 闸门有效性用变异测试反证过**（不是"跑通了"就算数）：缩放系数 `50→40` → `build_input` 差 1.95e-01、12 条动作不一致；权重索引漏乘 `in_dim` → Q 值整体错位。两次都被当场抓住。
  - **★ 本轮踩到的最深的一个坑：MinGW 依赖追踪只比时间戳**。用脚本改 `.cpp` 内容做变异测试、再用 `Copy-Item` 恢复备份时，文件 mtime 保持"备份时刻"而非"内容变化时刻"，`cmake --build` 便认为目标最新、**根本不重编**，于是拿着变异版 exe 跑校验得到**假失败**。实际踩中两次（权重索引倒置、缩放系数 50→40），两次都误判成"实现有 bug"，浪费一轮排查。正解是删 `<target>.dir` 对象目录后再 configure + build，已固化进 `check_all.py` 并注释原因。
  - **`find_cmake()` 回退**：anaconda 的 cmake 在 `<prefix>/Library/bin`，只有 PowerShell 会话 PATH 里有，`subprocess.run(["cmake", ...])` 会抛 `FileNotFoundError`。脚本内显式回退到 `C:\mingw64\bin` 等已知位置。
  - **`tests/dqn/` 独立 CMake 目录**（`add_subdirectory(tests/dqn)` 引入）：`tests/CMakeLists.txt` 在并行开发中被反复还原（实测 4 次），DQN 验证目标改在那里会被冲掉，故拆到独立子目录；exe 输出随之改到 `build/tests/dqn/`。**教训**：并行改同一个文件时，改动会被对方的 `git checkout` 静默冲掉，且 `git status` 只显示"未修改"——容易误判成"改动丢了"，实际是自己看错了行数。遇到反复丢失应换文件而不是反复重试。
  - ⚠️ 遗留：① **`DqnPolicy` 尚未接入 `MirrorAgent`**——`mirror_agent.h:112` 的 `MlPredictor` 插槽签名是 `MirrorBattleState → PlayerActionType`（**玩家意图**），而 DQN 输出的是 `CombatAction`（**玩家最优战斗动作**），两者语义不同，**不能直接对接**，需先定义中间的观测适配与动作映射；② 学习到的策略 `wait` 占 43.6%，说明仍有冗余动作；③ 权重文件需随游戏分发（当前仅在 `tools/rl/`）。

  - **训练侧五个模块**（`tools/rl/`）：`action_mask.py`（合法性 mask + 18 维输入构造）、`dqn_model.py`（网络 18→64→64→10 + 经验回放）、`train_dqn.py`（训练 + 导出）、`eval_dqn.py`（贪心 vs 随机基线，独立 seed 集）、`export_weights.py`（权重转 C++ 二进制）。
  - **网络结构照 `AI_LEARNING_GUIDE.md:602`，但 `state_dim` 8 → 18**（8 观测 + 10 mask）。mask 既是输入又是 argmax 约束，**两处来自同一次 `get_possible_actions` 计算**，不会出现"喂进去的合法性"与"实际约束的合法性"不一致。
  - **bootstrap 过 mask —— G31-B1 同一个 bug 必须在 DQN 侧再堵一次**：`max_a' Q(s',a')` 若不限合法动作，会把"当前不可选的动作"当未来价值上界。`compute_loss` 用 `next_mask` 把非法动作置 -1e9，全非法行（理论上不会发生，WAIT 恒合法）用 0 兜底。
  - **★ 第一版场景设计是必输的，DQN 12.7% 远低于随机 58.3%**。根因：`make_scenario` 把 orc 放在 `p.x+2` / `(7,5)`，技能范围 d<3 勉强够到但普攻 d<2 永远够不到，"走向 orc"既无收益又有代价；而 `victory` 要求**全员**死亡 → 场景不可赢。症状迷惑性极强：loss 单调上升（67→124），看起来像网络/超参问题。**是环境问题，不是学习问题**——与 G31-B4 同类错误的放大版。修法：`place_monsters` 保证两怪都在可交战距离（slime d=1 可普攻、orc d=2 仅技能），5 种变体覆盖贴身/交换角色/双怪同侧/三个角落。
  - **★ reward 缩放是必需项而非调参偏好**。C++ reward 量级：击杀 +50、胜利 ±200、受击 -(1..12)，Q 目标量级 ≈ 296。实测未缩放时 loss=572 且下降极慢，网络输出层要从 0 学到几百，小初始化下梯度被巨大 target 淹没。`REWARD_SCALE=0.01` 后 loss 降到 ~0.1，同样时间可学有效步数提升一个数量级。正仿射变换不改变最优策略。
  - **★ 「一局只更新一次梯度」是隐蔽的训练 bug**：60 局仅 20 次梯度更新，网络基本还是随机初始化，而 epsilon 已退火到 0.05 → 确定性策略立刻塌缩到单一动作（通常是 WAIT，因为未训练网络对所有动作输出近似常数，argmax 恒取第一个）。改为**每个 env step 更新一次**（标准 DQN），并加 `GRAD_CLIP=10.0`。
  - **训练脚本打印的胜率不能当验收标准** —— 它混了 epsilon 探索。新增 `eval_dqn.py` 用**未参与训练的 seed 集**（base=900000，与训练 `42+ep*7919` 不重叠）跑纯贪心，并强制与随机基线对比。第一版环境正是靠这个基线才暴露了"随机 58.3% 就能赢"——若只看 DQN 自己的 66%，完全看不出环境有问题。脚本在 `delta < 2.0pp` 时返回非零退出码。
  - **ONNX 路线已验证但最终不采用**：`onnx.checker` 通过（opset 13），onnxruntime 1.28.0 与 PyTorch 最大绝对差 3.6e-07。改用手写前向的理由见上方「推理不用 ONNX Runtime」。
  - 学到的策略特征（300 局动作分布）：`wait` 43.6% + `move_right` 18.9% 占多数——网络学会了**先拉开距离再放技能**，而非贴身对砍。这与 reward 结构一致（胜利 +200 > 击杀 2×50），是有意义的发现：**当前 reward 让"稳赢"优于"速通"**。若想要速通风格，需重调 reward 配比，属独立议题。

- G31（开发版，未发布）：RL 层合法性约束修复 + Python 移植 parity 闸门 —— 为「Q 表 → DQN」升级路线铺路。共修 5 个 C++ 缺陷 + 建立跨语言一致性验证。

  - **G31-P Python `CombatEnvironment` 移植 + parity 测试**：`combat_env_py.py` / `observation_py.py` 逐行移植 C++ 的 `CombatEnvironment` 与 `Observation`，作为 DQN 训练环境。黄金轨迹由 `tools/rl/parity_dump.cpp`（独立 CMake 目标 `parity_dump`，只依赖 RL 三源文件）生成，`parity_test.py` 用**同 seed 双跑逐位比对**（float32 比 `0x` 位模式，不容许容差）。当前 **12 episodes / 615 steps bit-exact**，另用 seed 1 / 2026 / 999983 各 8 局交叉复验通过。
  - **parity 抓到的三类真实移植陷阱**（都是自己踩的）：① **Python `abs()` 是 float64 语义**，`abs(m.x-p.x) + abs(m.y-p.y)` 表达式里只裹一个 `F32()` 会被整体提升回 float64，导致曼哈顿距离错 1 格（症状：`nearest_enemy_dist` 恒偏小 1，轨迹迅速分叉）。修法是逐项 `F32(abs(F32(m.x) - F32(p.x))) + F32(abs(F32(m.y) - F32(p.y)))`，已抽成 `CombatEnvironment._dist` 供 4 处复用。② **动作枚举索引必须与 `action.h:10-16` 逐位对齐**（`MOVE_UP=5, WAIT=9`），最初把 `WAIT` 编成 6 导致 `legal` 列表整体错位。③ **parity_dump 的动作选择 LCG 需显式区分于 `action.h` 的 RNG**，两套常数相同但语义独立，已用具名常量 `DUMP_LCG_MUL/ADD` 标注。
  - **parity 闸门有效性用变异测试证明**（不是"跑通了"就算数）。故意植入 12 处语义偏差，全部被捕获：伤害系数 `0.4→0.45`、技能范围 `3.0→3.5`、`GRID_W 40→39`、`GRID_H 30→29`、`ATTACK_RANGE 2.0→2.5`、冷却步进 `0.25→0.3`、普攻冷却 `0.5→0.7`、技能冷却 `3.0→3.5`、击杀奖励 `50→40`、胜利奖励 `200→300`、反击概率 `0.4→0.5`。
  - ⚠️ **第一轮轨迹设计不合格，已重做**：初版每步用同一个 `seed+ep*7331+step*104729` 选动作，前几步轨迹高度雷同，**`GRID_W 40→39` 变异漏网**——因为玩家从未走到 x 边界。现改为① 每步推进 LCG 的 `ActionPicker`（25% 强制 ATTACK / 25% 强制技能 / 20% 强制移动，其余均匀）；② 6 种场景变体覆盖**网格全部 4 条边**（左上角、右下角、右边缘、上边缘）+ 贴身 + 中距。重做后 `GRID_W`/`GRID_H` 变异均被抓（各 1589 mismatches）。**教训：parity 通过不等于闸门有效，必须用变异测试反证覆盖面。**
  - 轨迹覆盖现状（12 局 615 步）：10 类动作全部出现（attack 11 / skill 53+52+52+37 / move 80+102+88+95 / wait 55），584 次击杀，reward 取值域 `{-6, -2, 0, 50, 250, 300}`，冷却峰值 2.75。

  - **G31-A~E 五个 C++ 缺陷修复**：
  - **G31-B1 `update()` bootstrap 越界**：`q_agent.cpp` 的 `max_next` 遍历**全部 10 个动作**，把非法动作（冷却中的技能、越界移动）的价值也算进 target，系统性高估。表格 Q 下是轻微高估，DQN 会把它直接拟合进去。改为只遍历 `next_legal`；新参数 `next_legal` **必传**，`update()` 与 `exploit_action()` 的正确性从此不可能被静默绕过。
  - **G31-B2 `exploit_action()` 游戏内路径无 mask**：该函数（`q_agent.cpp:37`，全仓库唯一调用点 `mirror_agent.cpp:320`）`_best_action` 遍历全部动作，**会选中冷却中的技能**。新增 `mirror_legal_actions()` 从 `MirrorBattleState` 构造合法集——只 gate `boss_can_attack`（该结构唯一可判定的合法性），SKILL/MOVE 一律视为可用并**在代码里显式注释这是信息量不足下的近似**，待 `MirrorBattleState` 补齐冷却/坐标后收紧。空集 → 返回 -1 不接管。
  - **G31-B5 `-1` 哨兵失效（连带修）**：`exploit_action` 返回 -1 后，`combat_action_to_arm(-1, ...)` 会落进 `default:` 返回 `RETREAT`（=1，**非负**），使调用方 `arm >= 0` 恒真 → **RL 层对 Q 表从未见过的状态也强行接管，且固定后撤**。`docs/AI_LEARNING_GUIDE.md:304` 描述的「置信度感知降级」形同虚设。
  - **G31-B4 训练场景打不到近战（致命）**：`rl_runner.cpp` 两处 `make_scenario` 的怪物距玩家 7 / 5 格，而 `get_possible_actions` 要求 `d<2`、`step()` 的技能伤害要求 `d<3` → **近战与技能伤害永远触发不了**，训出的策略只会绕圈和发呆（技能放出去 0 伤害还锁 11 步冷却）。改为 slime `d=1`（可近战）/ orc `d=2`（仅技能），mirror Boss `d=1`（双方可真实交战）。
  - **G31-B3 `to_key()` 丢字段 + 存档版本门**：`to_key()` 只编码 6 个字段，**`strongest_hp_ratio` 与 `buff_count` 未参与**，仅在这两项上不同的状态会撞进同一 Q 表条目。补齐为 8 字段（buff 上限截断 9 防键长失控）；因键格式变化，`save()` 写入 `"v": 2`，`load()` 对旧版存档**明确拒绝**而非静默错位读取。**副作用**：既有 `saves/rl_qtable.json` 与 `rl_mirror_q_*.json` 失效，`load()` 返回 false → `exploit_action` 全返 -1 → 优雅降级到克隆层/Thompson 层，不崩。**注意**：DQN 走 `to_vector()`（8 维全量），在这一点上严格优于现有表格 Q——这是升级路线的实打实收益。
  - 附带修 `action.h:33` 错注释（原写 "All 9 possible actions"，实际返回 8 且漏 WAIT；`CombatAction::COUNT` 为 10）。`all_actions()` 全仓库零调用。
  - **两个动作空间的发现**：RL 策略被用在**两个不同动作空间**——`rl_runner.cpp` 在 `CombatAction` 空间（合法性来自 `get_possible_actions(state)`），`mirror_agent.cpp:316` 在 `MirrorAction` 空间（经 `combat_action_to_arm` 折叠：4 技能→1 臂、4 移动→2 臂）。因此 mask **不能硬编码进 `QAgent`**，必须由调用方传入——两侧规则不同，这是把 mask 设计成参数而非内部逻辑的原因。
  - **`Observation` 对合法性是 POMDP**：8 个字段里**没有任何一个能推出动作是否合法**（缺 `attack_cooldown`、`skill_cooldowns[]`、`player.x/y`）。表格 Q 不在乎（它在 C++ 里枚举合法动作），DQN 拿到固定向量则物理上不可能知道。**结论：DQN 的 action mask 是必要条件而非可选优化**。这是下一阶段 DQN 设计（18 维输入 = 8 观测 + 10 mask）的前提。
  - 验证：Release **0 error** · ctest **78/78** · 新增 9 个回归测试（`q_test` 5 + `obs_test` 3 + 既有 6 全过）。新增测试逐条对旧实现**反向验证会失败**：`ExploitStaysInsideLegalSet`（旧实现无视 legal 返回 ATTACK）、`BootstrapIgnoresActionsOutsideNextLegal`（旧实现 WAIT 的 Q 会变成 4.5 而非 0）、`LoadRejectsLegacyVersion`、`KeySeparatesStrongestHpRatio`。
  - ⚠️ 遗留：① `run_rl_mode` 95 行 / `run_rl_mirror_mode` 89 行仍超 40 行（本轮各 +1 行参数，**未改善也未恶化**，属既有负债，留待专项拆分）；② `mirror_legal_actions` 的 SKILL/MOVE 无合法性判定，需先给 `MirrorBattleState` 加冷却/坐标字段才能收紧；③ 旧 Q 表存档需删除或改名，避免下次训练 load 时报"继续训练"误导。

- G30（开发版，未发布）：`game_scene.cpp` 超规清零 —— G29 账本里落在该文件的 12 个超 40 行函数全拆完，全仓 108→96。

  - 11 个提交分 11 批，全部纯结构搬运：`调度骨架 + ≤40 行子步骤`，执行/绘制顺序零改动。累计新增 **87 个 helper** 全部 ≤40 行。最大单体：`_render_ui_tail` 391→21（20 个子步骤）、`_process` 609→40（分 G30-H/I/J/K 四批）、`_collect_sim_stats` 218→28（12 个）、`_draw_entities` 128→13（11 个，含二级拆分 `_draw_monster` 再拆 5 个）、`_render` 169→13（12 个）、`_draw_mirror_analysis_panel` 102→11（8 个）、`new_game` 99→10（8 个）。
  - **拆 `_process` 前必须先敲定的三个陷阱**：① 竞技场分支与主路径存在**重复代码**（`_cleanup_dead_monsters` / 武器 VFX 池 / 武器 tick），两条互斥路径只能各搬各的，**不得合并去重**；② `_challenge.tick()` 被调两次（地牢/竞技场），同由；③ 段间顺序即逻辑，骨架必须严格按原顺序调用。
  - **`void`→`bool` 返回转换的坑**：四个门控子步骤（`_handle_player_death` / `_sim_wall_timeout_check` / `_sim_stuck_gate` / `_tick_hit_stop_gates`）要把原 `return;` 变成 `return true;` 以便调度骨架短路，但原函数体末尾是 fall-through 无 return —— 不补 `return false;` 就触发 `-Wreturn-type`。G30-J 首版就漏了，靠编译器抓住（CMakeLists 未开 `-Wall`，但 `-Wreturn-type` 默认开启）。
  - **本轮自抓的 4 个自身缺陷**，全在编译或校验前被拦下：① `reversed()` 遗漏致替换区间错位、文件乱码；② 拆出的 helper 残留调用未定义函数（编译期暴露）；③ 上述 `return false;` 漏补；④ 替换范围漏掉原签名行致签名重复。
  - **校验方法升级**：早期批次的 `is_scaffold` 式「剔除所有 `}`/空行」校验法会误删原正文的花括号与空行，已废弃；统一改用**带标签重建法** —— 把新 helper 建成 `[(文本, 原行号)]` 序列，与文件逐字节比对 + 校验行号覆盖集合 + 断言丢弃集合 ⊆ 空行∪冗余注释。G30-K 最终账：`_process` 208 行原正文 = 154 行入 helper + 24 行内联保留 + 29 空行/1 冗余注释丢弃，**零丢失**。
  - 验证：每批 Release 0 error 0 warning · ctest **78/78** · `--sim 2 --sim-seed 1234` 报告 SHA256 与固化基线**逐字节一致**（`7ADE6742E3EF1C80…`）。**基线零漂移**，与 G25、G28 并列第三个零漂移重构里程碑。收尾 `cmake --build build --clean-first` 全量重编 68 秒，全仓 **0 warning 0 error**——顺带证伪了上一个会话待办池里「`game_manager.cpp` 18 个 `-Wreturn-type` 警告」的说法，该文件在 `src/` 下并不存在，属幻觉记录。
  - ⚠️ 遗留：① `_process` 现正好 40 行踩在阈值边界，后续往里加调用会立刻超规，新增逻辑必须走 helper；② BOSS_CINEMATIC / HitStop / 运镜路径仍未被确定性校验覆盖（sim 只打到 1.2 层），G25-H 与 G30-G 都动过相关路径，建议进游戏打一场 Boss 手动验收登场运镜与击杀顿帧；③ 全仓仍 96 个超规，下一批最肥的三处是 `player_controller.cpp`（3 个 / 累计 743 行，`tick` 219、`_weapon_attack` 252、`handle_input` 272）、`hd2d_scene_builder.cpp`（4 个 / 722 行，`_build_effects` 419）、`boss.cpp`（4 个 / 615 行，`BossAI::_tick_boss_state` 394）。

- G29（开发版，未发布）：重复源树清理 + 两个守护工具。
  - **发现的脏数据**：`assets/assets/`(632 文件 58 MB)、`src/src/` 含 `src/src/src/`(720 文件)、`docs/docs/`、`tests/tests/`、`resources/resources/`、`tools/tools/`、`vendor/vendor/`、`.github/.github/` 八个嵌套自复制树，外加 `vendor/` 根层误放的 8 个仓库根副本（`CHANGELOG.md` 265957 B、`roguelike_cpp.exe` 7033 KB）和 `pc_spec.json`。全部 `git ??` 未跟踪——是某次同步脚本跑歪留下的。
  - **为什么危险**：`CMakeLists.txt:44` 的 `file(GLOB_RECURSE SOURCES src/*.cpp)` 会把嵌套副本递归全收，而 **`main.cpp` 当时存在三份**，`src` 下 486 个 .cpp（真实 162）。任何人做一次干净 `cmake -B build` 就 LNK2005 多定义编译失败；**增量构建完全无感**，`.gitignore` 也挡不住（那些文件本来就没被跟踪）。
  - 清理：删除前审计确认拟删目标里 **git 跟踪文件 = 0**，删完 `git ls-files` 1331→1331 无一丢失。释放 **67.6 MB**，`.cpp` 486→162，`main.cpp` 3→1。**这批清理本身不可提交**（未跟踪文件），所以只有新工具 + 文档可提交。
  - 验证：整个 `build` 删掉从零重跑——GLOB 重算出 162 个唯一 .cpp、`src/src` 残留 0，全量编译 107 秒，ctest **78/78**，基线 SHA256 与 G27 逐字节一致。
  - 新工具 `tools/fn_length_scan.py`：修掉 G28 期间暴露的三类误判——① `(\w+)\(` 匹配不到 `PlayerController::tick(`，**219 行的 tick 就这么漏了**；② 重复树让每个函数被数 3 遍；③ 字符串/注释里的花括号破坏深度统计。参数跨行用「定位 `)` 再看其后跟 `{`/`;`/`=`」判断，不为多行签名写正则。
  - 新工具 `tools/check_source_layout.py`：嵌套自复制目录 / 源目录构建产物 / 仓库根文档误入 vendor，退出码可接 CI。
  - **真实超规账本：162 个 .cpp 里 108 个函数超 40 行**（之前那份"36 个"是坏扫描器在重复树上数出的假数）。榜首 `_process` 609、`_build_effects` 419、`_tick_boss_state` 394、`_render_ui_tail` 391、`GameMap::draw` 315、`main` 292、`handle_input` 272、`_weapon_attack` 252、`tick` 219。
  - 写工具时自己抓到两个 bug：嵌套检测以顶层目录为 base 时外层名被 `relpath` 剥掉，`src/src/game` 变成 `src/game` 就不重复了（完全失效）；`parts[-2:-1]` 取倒数第二段导致 `lib/` 判断永假，把合法的 `vendor/raylib/lib/raylib.dll` 报成违规。两项都用「造故障→必须抓到→清掉→必须回 0」的回路验证。

- G28（开发版，未发布）：`enter_floor` 255 行拆成 14 个 ≤40 行子步骤 —— G25 之后的第二个零基线漂移重构批次。
  - **比 G25 难一层**：`_process` 的段之间靠成员变量传状态，而 `enter_floor` 有栈上局部变量跨段落存活 —— `DungeonGenerator gen` 在生成块创建、20 行后才被 `_spawn_normal_floor` 消费；`rooms` 被刷怪/事件/NPC 推进 4 个下游消费。必须先划"变量生命线"再决定哪段能抽走，`gen` 无法抽走（要么传引用要么污染成员），只能把不依赖它的段拆出去。
  - 拆法：按生命周期分三阶段 —— 重置（3 个）／生成+放置（6 个，`gen`/`rooms` 相关段留主函数或收进各自小方法）／事件+推进（5 个）。**`game_scene.cpp` 超规 1→0**。
  - 两处自抓的语义偏离（纯搬行的真实风险）：① 给 `if ((rng()%1000)/1000.0f >= ev_chance) continue;` 加了个防零守卫 `ev_chance > 0.0f &&` —— 短路会**改变 `rng()` 掷骰次数**，虽然 `ev_chance` 恒为 0.40/0.70 实际不影响，但这是"看着等价其实不等价"的典型，已还原；② 把 `fcfg->is_boss`（配置值）换成 `is_boss_floor(floor)`（楼层号推导），二者不能假设等价，已改回并把 `fcfg` 显式传入。另有 1 处 `std::make_shared<EquipmentItem(` 括号漏配，落盘后补修。
  - 验证：0 error 0 warning · **78/78** · 确定性基线**逐字节一致、G28 不变更基线**（保持 G27 的 `8EE12B30…`）。除 SHA256 全量比对，还单独 grep 了 `game_scene.cpp` 全部 `rng()` 出现点逐一核对 —— 3 处全保留且顺序未变（`_roll_floor_seed` → `_spawn_floor_events`×2 → `_setup_floor_intros`），掷骰次数或次序一变 500 局报告会整体偏移。
  - ⚠️ 遗留：`_spawn_normal_floor`(33 行) 是 14 个里最大的，内部武器掉落子块还能再拆一层但已在线内，不值得为拆而拆。另有 7~14 行的小方法密度偏碎，若将来合并同类小方法，必须先确认 `rng()` 调用顺序不会被并到一起。

- G27（开发版，未发布）：sim 批次局间独立性 —— 根因是**函数级 `static` 跨层跨局泄漏**，不是随机流。
  - `PlayerController::tick()` 内有 4 个函数内 `static`：`_sim_last_step`、`_last_mx/_last_my`、`_last_move_dir`、`_loot_dbg`。其中 `_sim_last_step` 与 `gs.game_time` 做节流比较，而 `game_time` 在 `enter_floor` **每次归零** → 进层后 `game_time - _sim_last_step` 恒为负，sim 玩家**永久无法移动**。
  - **影响范围被严重低估**：这不只是批次局间问题。单独跑同样中招 —— 第一层靠 `_sim_last_step` 初值 `-1` 还能动，**第二层起就冻住**。此前 G26 采集的整份平衡数据集都是在「玩家第一层之后基本不动」的状态下跑出来的，`avg_floor` 1.80 是被冻结假象压出来的。
  - 修复：4 个 `static` 归位为 `PlayerController` 实例成员（该类已持 `_last_seen_hp`/`_hit_window` 同类帧间状态，模式一致），新增 `reset_floor_state()`，由 `enter_floor` 在 `game_time = 0` 同一处调用。`PlayerController` 是 `GameScene` 成员，新局自动拿默认值 —— 跨局泄漏随 G27 的新建场景方案一并消解，跨层残留由 `reset_floor_state()` 显式清。
  - 连带修掉批次重启的两处边界：`scene_tree.cpp` 的 `change_scene` 提前置 `_skip_input`（覆盖"已请求切换、尚未 swap"那一轮，否则该轮 `process_input` 会把输入派发给正在退场的旧场景）；`game_scene.cpp` 新局首帧清零 `sim_stuck_watchdog`（旧场景 agent 可能残留置位）。
  - 取证过程的反面教材：先用 `CountingRng` 逐掷钩子比对，证明批次 run1 与单独跑的**随机流逐掷一致**（前 122+ 掷、每帧 draw 计数完全相同），且楼层地图 `rooms` 字节级相同 —— 于是判定「不是 RNG 问题」并转查移动/碰撞层，才在 `player_controller.cpp:197` 找到这个 `static`。中途被 `_loot_dbg` 这个**同类 static** 骗过一次：它让 SIM-LOOT 日志的采样相位跨局漂移，两跑 `inv=3` vs `inv=4` 纯属采样点不同，不是状态差异。
  - 验证：批次 5 局逐个对拍 5 个同种子单独跑，结构化摘要（楼层序列 / 累计卡死 / 卡死根因快照）**5/5 MATCH**；`SequenceMatcher` 逐行比对整局日志只剩 1 处差异 —— 单独跑多 3 行一次性监听器注册日志（`_s_registered` 静态量幂等守护正确跳过，属预期）。
  - 效果：`avg_floor` **1.80→4.0**、最深 4→**6 层**、`avg_turns` 1786.8。胜率仍 0/5，但**首次打到了 F5 Boss 层**。
  - 门禁：Release 0 error 0 warning · ctest **78/78** · 确定性基线更新为 `8EE12B30AC155DBBCCE672AF1F9A9D65AEBD86E4E9E4C709F64EF987BFF6BA42`（修 bug 必然变更，两次运行逐字节一致）。
  - ⚠️ 遗留：`PlayerController::tick`(219 行) / `new_game`(99 行) 仍超 40 行规范（`enter_floor`(255 行) 已由 G28 拆完）。

- G22c/G22d（开发版，未发布）：HUD 批收尾 4 函数 + 伪原子写漏洞修复。
  - `DrawRoundedRectBg` 48→18、`_draw_effect_body` 76→12、`draw_gamble_panel` 50→30、`_draw_mirror_learning` 54→10，9 个新 helper 全部 ≤40 行。**`game_renderer.cpp` 超规 4→0（首个清零文件）**，全仓 109→105。四段圆弧用 `ox/oy ∈ ±1` 参数化合并：`cx + ox*r*cosf(a)` 与原 `rect.x + r - r*cosf(a)` 是同一棵表达式树（`-1*r` 精确等于 `-r`），浮点无损。
  - 拆分中自己引入并修掉一处渲染回归：`_draw_gamble_odds` 返回值未接回调用方，odds 表末尾的 `y += 28` 跨函数边界丢失，「每次抽奖」与「5% 圣物」两行重叠。**77 个测试全绿也未察觉** —— 现有测试对渲染坐标零覆盖，靠逐行 diff 原代码才发现。这是纯结构拆分的真实风险：同函数内顺序写着的 `y += 28` 天然可见，拆出去就变成必须手工接住的线。
  - `MirrorMemoryStore::save_from` 伪原子写三个漏洞：①`out <<` / `out.close()` 均未检查，而数据是缓冲的、真正落盘在 close —— 磁盘满时**半成品被 rename 覆盖正式档**，这是数据损坏而非残留；②rename 失败分支无清理，`.tmp` 永久残留；③调用方 `boss_system_director.cpp:291` 裸调用，失败完全静默。拆为 `build_snapshot` + `write_atomic`（任一步失败都 `std::remove(tmp)`），`save_from` 20→4 行。
  - 失败注入不靠在生产代码留开关：把目标路径做成**目录**（文件不得替换目录），`std::rename` 必失败；断言返回 false、`.tmp` 已清理、**且目标仍是目录**（证明 rename 未发生、正式档未被覆盖）。`mirror_memory_store_test` 5→7。
  - 触发线索：桌面包 `saves/mirror_memory.json.tmp` 残留 134 字节。查证后是正式档经 `decay_counts` ×0.99 衰减的派生（`4→3.96→3`、`6→5.94→5`、`2→1.98→1`、`3→2.97→2`），非新数据 —— 删它无所谓，但它坐实了 rename 那次确实失败过。
  - 门禁：Release 0 error 0 warning · ctest 77/77 · `game_renderer.cpp` 超规清零。
- G21（开发版，未发布）：删除 `CombatCoordinator::on_monster_killed` 与 `cleanup_dead_monsters` 互相引用的死代码簇（118-189 行，76 行），`combat_coordinator.cpp` 193→119。该簇**互相引用**所以单点删除会编译失败，必须成对删。
- G20–G20d（开发版，未发布）：存档槽位绑定 + 备份闭环。
  - **读档不绑定活跃槽**：`load_game` 内补 `set_active_slot`，否则下一次自动存档写错槽；`_activate_stairs` 加 `_world_mode != DUNGEON` 守卫，防挑战竞技场误触发换层存档。
  - **写档前自动备份**：`.tmp` 暂存 → 旧档 `.bak` → `.tmp` 提升正式槽，伪原子三步；`delete_save` 与 `SlotGuard::cleanup` 连带删 `.bak`。
  - **槽位菜单可还原**：`SlotSummary` 加 `has_backup`/`backup_floor`/`backup_level`，有备份的卡片第四行 `备份 F7·Lv23 [R 还原]`；`restore_backup` 两代交换走 `.aside` 中转，不销毁任何一代；当前档不存在时直接把备份提升为正式档（崩溃丢档场景）。
  - **选关读档丢掉地牢种子**：`floor_select_scene` 传默认 0 → `enter_floor` 判为新楼层走 `rng()` 重滚，玩家进陌生地图，随后自动存档用新 seed 覆盖旧档、布局从此漂移。其余三个读档入口都是透传的，唯独这一处漏。
  - 测试 74→77。用户实机验收通过（2026-09-30）。
- G18/G19（开发版，未发布）：`execute_event` 254→28 行、`draw_hud` 198→28 行（G13 续，超规 103→102→113，后续批次持续回落）。
- G15–G17（开发版，未发布）：死代码三连清。G15 圣物系统死代码 + 修两处 buff 叠加/泄漏缺陷（net −165 行）；G16 全仓死方法三级连锁，37 文件 −430 行纯删除；G17 移除 MetaSystem 已死掉的 reward-log 审计特性。
- G14（开发版，未发布）：测试护栏补强 74→77。新增 `game_scene_smoke_test` 做 `_process` 主循环 headless 冒烟；替换 `tests/combat` 三个假测试为真调用测试（buff 8 / action 10 / relic 9 用例）；修三处 `view.monsters` 空指针 segfault。
- G13（开发版，未发布）：函数长度债收敛，超规 **149→105**。
  - 覆盖 save_manager 序列化全拆（`load_game` 364→34、`save_game` 164→27）、`dungeon_generator` 特殊房间/Challenge/孔径、`enemy_defs`+`weapon_defs` 解析器、`DependencyResolver` 拓扑排序分段（+6 用例）、`WorldReaction`/`Mod`/`Encounter`/`Item`/`SpecialRoom`、6 个 scene 的 `_render`/`_input`、`TeamCoordinator.evaluate`、`sim_ai::_evaluate_move` 154→27。
  - 遗留 TOP 6：`_process` 1072 / `_build_effects` 419 / `_tick_boss_state` 394 / `_render_ui_tail` 391 / `game_map::draw` 315 / `main` 292 —— 全部无测试护栏，需先建护栏再谈拆分。
  - **扫描盲区**：脚本按方法名全词计数，对 `tick`/`draw`/`reset` 这类重名方法失效；G21 那簇是人工排查引用链发现的，非扫描产出。

- B4（开发版，未发布）：挑战房 25% 隐藏压轴 Boss「远古魔像 GOLEM」全链路——`BossType::GOLEM(4)` 的 C++ 行为早已建成（DEFEND 盾、Phase2 三连震、`get_boss_def_for_type(4)→"golem"` 映射、视觉色），本批只补数据 def 与刷出路径，`boss_defs.h`/`boss.cpp`/`boss_system_director.*` 零改动。
  - **数据 def**：`bosses.json` 增第 6 条 `golem`（`is_defender=true`、`shield_pct=0.50`、`skill_cycle_bias=5`、`skills=[charge,shockwave,barrage]`、两段 combo 含 `defend`、`arena.danger_type="none"` 纯坦克不放房间陷阱）；其余 5 个 boss 逐字段零改动，`get_boss_def_for_floor(10)` 仍返回 `fire_demon` 未被 shadow，GOLEM 不进入任何主线楼层。
  - **压轴判定**：`has_boss_wave(dungeon_seed, room_index)` = `_deterministic_seed(dungeon_seed, room_index, kBossWaveSlot=99) % 100 < 25`。**不消耗全局 `rng`**（批次9 红线）：真实波占槽位 0..2，压轴独占保留槽位 99，经 avalanche `hash_combine` 后与真实波属不同哈希域，故判定不改变任何小怪波构成、存档/回放可比性不被破坏。`_spawn_wave` 入口加 `assert(wave_index != kBossWaveSlot)`——否则 `pick_challenge_monster(99)` 返回空指针、`_pick_monster_type` 静默回落 `"slime"`，「隐藏 Boss 波」会刷出 4 只史莱姆且无任何报错。
  - **刷出路径**：状态机 `波0→1→2→(判定)`，命中则复用挑战房既有 3 秒波间等待当「boss 登场前奏」，随后 `_spawn_boss_wave` 调 `boss_factory_create(BossType::GOLEM, cx, cy, floor)`（tile 坐标，内部已乘 `boss_hp_scale`/`boss_atk_scale`）；`is_boss` 保持 true 以拿到 director 的 arena/相机/演出行为，清场后统一走 `REWARD`+`CLEARED`，压轴波不叠加 challenge modifier（无双份放大）。判定标记按房间（而非按层）清除，跨层复用同房间不残留。
  - **奖励隔离守卫（bug fix）**：`game_scene_combat.cpp:102` 的 boss 奖励分支补前置 `is_boss_floor(_s.current_floor)`。原漏洞：`_drop_boss_reward` 的 `bf_idx = (floor==5)?0:(floor==10)?1:2` **else 分支固定取 index 2**，于是不在 5/10/15 层的任何 `is_boss` 怪物被击杀都会白拿 F15 终章武器 `sword_legendary`（倚天剑）+ 随机圣遗物 + 30% 回血——压轴 GOLEM 本会让玩家在 F6-9 直接拿到终章武器。新增 `tests/world/boss_floor_guard_test.cpp` 用 1..30 全枚举锁定 `is_boss_floor` 精确真值表（仅 5/10/15 为 true）。
  - **奖励叠加**：压轴加成叠加在现状之上、未出 boss 的房间与改造前逐字节一致，详见下方 B4-T4 条目（含 golden oracle 逐字节比对与掷骰数证明）。
  - **美术**：`tools/gen_boss_parts.py` 加 `golem_boss` 石质灰蓝调色板（`assert len(BOSS)` 5→6，DECOR 增石缝裂纹；无武器，沿用 fire_demon 全透明武器件先例），产出 5 个 `boss_golem_part_*.png` + `boss_golem_skeleton.json`（9 bone/7 part）；`actor_avatars.json` 登记 `boss_golem`（共享 `boss_anim.json`），`sprites.json` skeleton_parts 180→185、顶层 `sprites` 83→84。**承重修正**：`skeleton_parts` 段在 C++ 运行时完全无人读取，而 `boss.cpp:1213-1220` 探测 `"boss_"+visual_id` 未命中会回落 `boss_f10`、`"boss_f10"` 又不在 avatar 白名单内导致骨骼永不创建——补 `sprites` 段的 `boss_golem` 整图键（复用既有 `mon_golem.png`，零新增资产）使兜底退化为魔像轮廓而非火魔；该键同时过 `asset_manifest_test` 的 frame 红线 `16*2==32` 与 PNG 尺寸双校验。`animation_test` 期望集 35→36（该用例逐键断言 skeleton/anim 非空、文件存在、双方可解析并锁 `bones==9u`/`parts==7u`，属结构维度收紧而非放宽）。生成器确定性复验：`assets/sprites/boss_*part_*.png` + `resources/animations/*_skeleton.json` 共 67 文件重算 SHA-256，既有 61 个逐字节不变、仅新增 6 个。
  - **validator 自检**：`tools/world_validator.py` 新增「Boss defs 自检」段。`get_boss_def_for_floor` 是硬编码 switch（`boss_defs.cpp:170-176`）故其 3 个目标必须真存在于 JSON；`golem` 必须存在（否则 `BossType::GOLEM(4)` 映射断链、挑战房压轴不可达）且 `is_defender=true`、`shield_pct>0`、skills 含 `charge`+`shockwave`、`skill_cycle_bias ∉ {4,6}`、`is_summoner` 仅发 warning——`_next_cycle_skill`（`boss.cpp:425-435`）只在 `cycle_len==4`(idx3) 或 `==6`(idx4) 返回 Summon，偏置落在 4/6 会让纯坦克魔像周期性召唤小怪；而 `is_summoner` 只用于显示串、完全不 gating 召唤。技能按 **id-match** 覆盖参数（`boss.cpp:1240-1257`）与数组位置无关，故断言一律用「contains」而非下标。
  - 门禁：Release build **0 error** · ctest **71/71 全绿** · World Validator **0 error / 0 warning** · `--sim 12 --sim-seed 3` 实测 `wins:0, avg_floor:1.33`（agent 死在 F1-2，挑战房需钥匙且压轴仅 25%，**结构上无法验证本批**，与批次 8/9 同性质，实质证明落在用户实机验收）。sim sha：按批次9 既定归一化（原始 3482 行去掉 3 行——`__DATE__/__TIME__` 启动行、`[SpawnSlots] Loaded`、`[ChallengePools] Loaded` → 3479 行）得 `4d750dd42efa…d2e7c3609`，与批次9 基线 `953f1630…e762e9474` **不一致，但并非 RNG 行为变更**：差异恰为 2 行启动期 registry 计数日志（`[BOSS_DEFS] +5→+6`、`sprites.json: 83→84 个精灵定义`，固定在第 10/41 行、早于任何 gameplay 行，分别对应新增 golem def 与新增 boss_golem 整图键）；把这 2 行一并排除后 3477 行流 sha256 两侧均为 `5e3343d3a984…d5cefa7b2` **完全一致**，即游戏逻辑流零分歧。
  - 遗留：`world_validator` 仍**不做** sprites↔skeleton↔avatar↔def 四向交叉校验；该四向艺术链路目前由 `asset_manifest_test`（frame 红线 + PNG 尺寸）与 `animation_test` 白名单用例（逐键解析 skeleton/anim、锁结构维度）共同承载，本批 71/71 全绿，属已知接受的覆盖缺口。另 `generate_random_item()` 武器分支经 `_random_weapon_def` 拼名，`sword_legendary` 在随机池仍可达约 0.21%/件（非本批回归）。
- B4-T4（开发版，未发布）：挑战房奖励隔离——隐藏压轴波（GOLEM）的奖励加成**叠加在现状之上**，未出 boss 的房间奖励与改造前逐字节一致。`ChallengeRoomController::_grant_rewards` 新增 `boss_cleared` 参数（唯一来源 `_boss_wave_pending`，只在 COMBAT 全灭波次首次抵达总数时判定一次，本批未改判定路径）；结算参数抽为纯函数 `decide_reward_plan`（基础 3 件/重试 5/RARE+/金币 `50+15*floor` 全部原样，压轴额外 1 件/重试 8/EPIC+ 且金币 ×1.5，boss 分支只写 `bonus_*` 与 `gold`、从不触碰三个 `base_*` 字段，加性由 1..30 层全枚举逐字段比对锁死）；共享 `_grant_items` + `roll_item_at_least` 承载两段循环，`Rarity` 秩以 int 过界避免头文件牵连 `raylib.h`。压轴奖励不进入主线 Boss 奖励路径：不授 relic、不做 30% 回血、不调 `_drop_boss_reward`。新增 12 用例（`challenge_room_test` 35→47）：真值表、**golden oracle 逐字节比对**（改造前 `_grant_rewards` 原样复制为参考实现，64 seed × 5 层比对物品指纹/金币增量/背包数/**`CountingRng::draws` 掷骰数**，证明 RNG 流未被加性分支扰动）、叠加性（4 件且前 3 件指纹不变）、重试上限（每次生成消耗 2–5 掷，界取 9×5）、3 格背包溢出落房间中心、`tick()` 端到端（空 monster 向量 + 全墙地图 + dt 3.5f，seed 扫描区分 hit/miss，验证真实分支给出 3 件 200 金 / 4 件 300 金）。首版 3 处测试断言自身写错（误把每次生成当 1 次掷骰、预填满背包使 4 件全溢出、金币期望漏算起始 100 金），已修正。门禁：build 0 error · ctest **70/70** · World Validator 0/0 · 仅改 `challenge_room.h/.cpp` 与 `tests/economy/challenge_room_test.cpp`。**遗留**（其一半已清偿，见上方 B4 条目）：`game_scene_combat.cpp:102` 的 `is_boss_floor` 楼层守卫已在本批落地，GOLEM 死亡不再触发主线奖励（原漏洞：白拿 `sword_legendary` 倚天剑 + 圣遗物 + 30% 回血）；仍存 `generate_random_item()` 武器分支经 `_random_weapon_def` 拼名，`sword_legendary` 在随机池可达约 0.21%/件（改造前基础 3 件已存在同性质，非本批回归）。
- A5-T3/T4（开发版，未发布）：玩家分件骨骼接入 2D 与 HD-2D，共用姿态、镜像和缩放；3D 支持透明裁剪、分件投影和残影排序。37 项动画测试、63 项 CTest 与 World Validator 通过；隔离运行截图确认待机、行走和攻击姿态变化。绿色占位件已由 T5 红饰灰甲骑士分件替换。翻滚/受击视觉验收仍待完成；3D 分件 shader 不可用时回退旧静帧。
- A5-T5（开发版，未发布）：参照原 `player_fire.png` 的骑士造型重新补绘，不是无损裁切原图；原图保留作回退。5 张透明贴图复用为 7 件，骨架、pivot 与动画配置不变。隔离 2D/3D 截图确认新外观，3D 行走与攻击姿态变化正常。初版比例偏细长，已由比例修正回到敦实像素风（骨链 hips 20/躯干 28x20/腿 12x20/剑 6x24，实机剪影 ~16.5x24.5、宽高比 0.67，对照图 `a5_visual_20260917\proportion_before_after.png`）；3D 脚下矩形阴影有待美术调优。
- 素材工具：`conda run python tools/gen_player_knight_parts.py` 默认生成到 `reports/player_knight_parts/`；确认预览后才用 `--output-dir assets/sprites` 更新分件。`conda run python tools/anim_preview.py` 默认写 `reports/anim_preview.png`，拒绝覆盖原骑士及正式分件。固定 Pillow 环境下生成确定；离线预览不含镜像、受击/翻滚 overlay、残影和 3D 光照，不能代替实机验收。
- A6-S1（开发版，未发布）：玩家骨骼渲染泛化为全实体通用引擎 `SkeletonAvatar`（组合复用，PlayerAvatar 行为零变化）；新增数据驱动皮肤白名单 `resources/animations/actor_avatars.json` + `actor_avatar_defs` 加载器（缺省/空 = 全回退旧 sprite）；Monster 懒挂皮肤（仅渲染路径，sim/无头不触达），`monster_anim_input` 纯函数信号映射（moving=AI CHASE、attacking=0.25s 挥砍窗、hit=hp 下降沿、recovery=1.0）。首版白名单为空 → 游戏内零视觉变化：48 项动画测试、63 项 CTest、World Validator 0/0、`--sim 12 --sim-seed 3` 报告与基线逐字节一致、2D/3D 隔离截图差异低于旧版自对拍噪声底。
- A6-S2 批次1（开发版，未发布）：人形族 8 怪（兽人/精英兽人/哥布林弓手/萨满/猎手/重甲守卫/骨兵/骨骼弓手）接入骨骼——`tools/gen_mon_humanoid_parts.py` 参数化族生成器（4 体型 × 独立色板，深紫褐描边风格统一），每怪 5 件沿用骑士 rig 尺寸契约；8 份 `mon_*_skeleton.json` 克隆玩家 rig，动画共用 `player_anim.json`；白名单登记 8 键，未迁移怪（史莱姆族等）保持旧贴图。隔离 2D/HD-2D 实机截图确认骨骼怪渲染/镜像/脚贴地正常；sim 逐字节一致。后续批次：软体族、浮灵/魔像族、Boss/影武者/NPC。
- A6-S2 批次2（开发版，未发布）：软体族 5 怪（史莱姆/爆炸史莱姆/精英史莱姆/冰霜史莱姆/鲜血水蛭）接入骨骼——新增 `tools/gen_mon_soft_parts.py` 参数化族生成器（圆润躯干+顶部凸起+融滴下肢+触手短臂+尖刺，深紫褐描边与批次1风格统一），骨架复用骑士 rig 的 BONES/pivot 契约（ppu 0.8），5 份 `mon_*_skeleton.json` 共用 `player_anim.json`；`sprites.json` skeleton_parts 登记 25 键，白名单扩至 13 键。68 项 CTest、World Validator 0/0 通过；`--sim 12 --sim-seed 3` 与批次1基线逐字节一致；实机运行确认渲染/镜像/脚贴地正常。后续批次：浮灵/魔像族、人形补充、影武者、NPC、Boss。
- A6-S2 批次3（开发版，未发布）：浮灵/魔像族 7 怪（电光之核/火魔仆从/雷暴元素/虚空行者 + 魔像/石像守卫/铁卫）接入骨骼——新增 `tools/gen_mon_float_golem_parts.py` 双造型生成器（golem 石板躯干+方块头目缝+石锤；float 能量球+焰冠+垂穗腿+裂纹法杖，深紫褐描边风格统一），骨架复用骑士 rig（ppu 0.8）；`sprites.json` skeleton_parts 登记 35 键（总 105），白名单扩至 20 键；同步修复 `gen_mon_humanoid_parts.py` TIERS ppu 0.45/0.5→0.8 潜伏回归（9e6d6fc 只改了 JSON 未改生成器）。68 项 CTest、World Validator 0/0 通过；`--sim 12 --sim-seed 3` 与批次2基线逐字节一致。lightning_orb 当前无任何生成引用（内容缺口，待补生成池）。实机运行确认 7 怪渲染正常。后续批次：人形补充、影武者、毒液蠕虫、NPC、Boss。
- A6-S2 批次4（开发版，未发布）：人形补充 6 怪（冲锋兽人/哥布林召唤师/亡语者/冰狱守卫/血祭司/暗术师）接入骨骼——扩展 `gen_mon_humanoid_parts.py`（+6 色板：猩红/琥珀/腐绿/冰蓝/血红/暗紫；+4 新武器：长矛/镰刀/典籍/血杖；冰卫巨剑、暗术师法杖复用），重跑全量 14 怪旧 8 怪零 diff 回归通过；`sprites.json` skeleton_parts 登记 30 键（总 135），白名单扩至 26 键。68 项 CTest、World Validator 0/0 通过；`--sim 12 --sim-seed 3` 与批次3 基线逐字节一致。necromancer 双身份（Boss 版走 boss.cpp）不受影响。实机运行确认 6 怪渲染正常。后续批次：影武者、毒液蠕虫、NPC、Boss。
- A6-S2 批次5（开发版，未发布）：影武者+毒液蠕虫 4 怪（暗影潜伏者/暗影刺客/夜行猎手/毒液蠕虫）接入骨骼——humanoid 扩展（+3 色板：暮紫/墨黑/夜棕；+SPEAR 武器；夜猎持矛）、soft 扩展（+wyrm_venom 色板与环节虫形行覆盖），旧 19 怪严格零 diff 回归通过；`sprites.json` skeleton_parts +20（总 155），白名单 26→30，**enemies 30 怪全量骨骼化里程碑**。附带 lightning_orb 火山池**数据面清偿**（biomes+world/volcano 双源 +1，运行时刷出路径仍无、留后续 C++ 批次，本批不验收池出）。68 项 CTest、World Validator 0/0 通过；sim A/B 见 Task4 实测记录。实机运行确认 4 怪渲染正常。后续批次：NPC、Boss、lightning_orb 运行时接入。
- A6-S2 批次6（开发版，未发布）：10 世界 NPC 骨骼化——新建 `tools/gen_npc_parts.py`（复用 humanoid rig，10 色板；空手 weapon 全透明；键 `npc_20`…`npc_140`），`sprites.json` skeleton_parts +50（总 205），白名单 30→40；C++ 接线：`NpcView.npc_id` + `_npc_avatars_tick` + 2D/HD2D 骨骼优先静态图回落。对话肖像与标题页仍用旧 `npc_*.png`；旧 30 mon 分件零 diff。68 项 CTest、World Validator 0/0 通过；sim A/B：新旧白名单输出逐字节一致（sha 5f03ff21）；与批次5基线唯一差异为 __DATE__/__TIME__ 启动行（src/main.cpp:128），游戏 sim 内容跨重建逐字节相同。实机验收通过（F2/F3/F7/F11/F14，2D+HD2D，肖像仍旧图，脚贴地正常）。后续批次：Boss、lightning_orb 运行时接入。
- A6-S2 批次7（开发版，未发布）：5 Boss（暗影骑士/亡灵法师/血族伯爵/地狱火魔/终焉回响）骨骼化——新建 `tools/gen_boss_parts.py`（1.4× 大 rig：head 30×26 / torso 40×28 / arm 16×22 / leg 32×28 / weapon 10×30，本地 `build_palette`/`part_grid`/`skeleton_dict`，新增 RAPIER/MIRROR_BLADE 武器与每 Boss DECOR 装饰，火魔空手全透明）与 `resources/animations/boss_anim.json`（复用三剪辑，torso y 对齐新 bind 8）；`sprites.json` skeleton_parts 205→230，白名单 40→45（`boss_shadow_knight`/`boss_necromancer`/`boss_vampire`/`boss_fire_demon`/`boss_self`→demon_lord 骨架）；**C++ 0 行改动**（Boss 走 Monster 通用渲染路径，2D 骨骼分支与 HD2D `_build_entities` 均无 `is_boss` 排除）；animation_test 期望集合 40→45 并新增 45 份骨架/anim 真实解析校验。旧 mon/npc 分件与旧 `boss_*.png` 整图零 diff。68 项 CTest、World Validator 0/0 通过；sim A/B 白名单 45↔40 逐字节一致（sha 5f03ff21，与批次6 基线相同）。实机验收待完成。后续批次：lightning_orb 运行时接入。
- A6-S2 批次8（开发版，未发布）：`lightning_orb`（电光之核）运行时刷出路径打通——调研发现 `BiomeDef::enemy_pool` 与整个 `resources/world/*.json` 均为**死数据**（C++ 零消费者，批次5 往池里加的 +1 实际不产生任何运行时效果），真正的选怪决策是 C++ 硬编码字面量 + 编译期 12 槽位权重表；本批只动 3 处 C++：`floor_manager.cpp` slot 6 在 F6-10（volcano）50/50 轮换 `lightning_orb`/`charger`（`cfg.floor` 短路保证 F1-5/F11-15 逐字节不变）、`challenge_room.cpp` 挑战房火山波 1 池 `types[4]` 加满、`biome.cpp` 给 `enemy_weights` 补独立 `contains` 保护（原实现缺该字段会抛异常被 catch 吞掉、导致**全 15 层 biome 静默加载失败**，潜伏缺陷）。`enemies.json`/`floor_config.cpp` 权重表/`biomes.json`/`world/*.json` 零改动。门禁：build 0 error、ctest 68/68、World Validator 0/0、`grep lightning_orb src/` 命中 1→3；sim 12/seed3 实测 `wins:0, avg_floor:1.33`（agent 死在 F1-2，slot 6 在 F1-3 权重恒为 0）**结构上无法验证本批**，故 sim A/B 降为信息性记录（实测 sha 逐字节不变）。遗留教训：该缺口连跨 3 批次才被发现，根因是刷怪表应为数据而非 C++ 字面量（数据驱动刷怪管线另立设计）。
- A6-S2 批次6 修正（开发版，未发布）：**撤回 NPC 骨骼化，恢复原版整图**——实机反馈「NPC 容易和怪混淆」。根因不是配色接近，而是 `gen_npc_parts.py` 从 `gen_mon_humanoid_parts.py` 复用 `PART_ROWS`/`SIZES`/`TIERS`/`WEAPONS`，NPC 非武器部件与怪物/骑士**像素图完全相同**（同头盔、同护甲躯干、同手臂、同腿），只有色板+体型缩放不同；最糟 `npc_80` 维拉与 `mon_goblin_hunter` 连体型档、武器、绿色系都一致。**不存在 10 张原版 NPC 图**——旧整图只有 `npc_1/2/3/blacksmith` 四张 16×16 圆脸头像，楼层映射本就是 3+3+3+1 复用。本批利用既有「骨骼优先、静态图回落」路径（2D `game_scene.cpp:3161-3174`、HD2D `hd2d_scene_builder.cpp:982-1001`，懒建失败会缓存不重试）**零 C++ 改动**回退：移除 10 个 `npc_*` 白名单键（45→35）+ 测试期望集合同步，删除 50 分件 PNG + 10 骨架 JSON + 50 条 `sprites.json` 注册（230→180）+ 164 行生成器（骨架契约本体在上游 `gen_player_knight_parts.py`/`gen_mon_humanoid_parts.py`，删 NPC 生成器不丢 rig 资产）。NPC 恢复 28×28 圆脸整图、失去 idle 呼吸动画，换来「一眼认出是人不是怪」；C++ 接线保留为休眠，日后用新造型低成本接回。门禁：ctest 68/68、World Validator 0/0、新增资产一致性自检（`skeleton_parts`↔磁盘、skeleton→part.file、白名单→skeleton/anim 三向交叉）、`src/` 零改动。范围外：NPC 专属新造型（需新写生成器）、HD2D 补 NPC 名条。
- A6-S2 批次9（开发版，未发布）：刷怪表数据化——把批次8 遗留的「刷怪表应为数据而非 C++ 字面量」清掉。新增 `resources/enemy_slots.json`（12 槽位 → 候选怪，含 `aliases`/`default`，槽位 0 权重 1:2 复现 `rng()%3==0`，槽位 6 的 `lightning_orb` 带 `floors:[6,10]` 且**必须排在 `charger` 前**）与 `resources/challenge_pools.json`（3 群系 × 3 波）；新增 `src/data/spawn_tables.h/.cpp` 加载器（照 `biome.cpp` 惯例：ifstream + `json::parse`，**parse 成功后才 clear registry**，故加载失败保留旧数据而非清空）。`floor_manager.cpp` 的 12 case `switch` → `pick_slot_monster`，`challenge_room.cpp` 的 9 池 → `pick_challenge_monster`，`main.cpp` 接线并对失败 `LOG_ERROR`（否则全图静默刷 default 怪）。**RNG 逐位等价是核心正确性论证**：`rng` 是 `CountingRng`，`operator()` 自增 `draws` 使掷骰次数可观测；逐槽位推导后实现「候选 ≤1 绝不掷骰」（槽位 1-5/7 是字面量、槽位 6 在 F1-5/F11-15 被 `floors` 过滤后仅剩 charger），旧代码在这些路径 0 掷骰，多掷 1 次会让整条 RNG 流从此错位。设计修正：撤回「`biome.enemy_pool` 当合法集」的提议——它是 3 个群系级名册（如 `ash_volcano` 仅 5 怪），而槽位系统对 F6-10 实际产出约 17 种怪，粒度错配会每层 10+ 误报，只保留「其 id 须是真敌人」轻量校验。关键发现：挑战房 `type_rng = wave_seed ^ (i*7+13)` 由 `dungeon_seed` 经 `hash_combine` 派生，**整条挑战房刷怪链不触碰全局 rng 流**，故无需也无法用 `draws` 证明；这也解释了批次8 改 9 池为何不动 sim sha。测试采用 **golden oracle**（把旧 `switch` 与 9 池原样复制成参考函数，新路径与它逐骰比对），因 sim 结构上看不见 F4+ 刷怪（agent 全死 F1-2）而 sha 只能做信息性记录。新增 8 用例：12×15×256 内层输出比对、掷骰计数 vs 期望表、15 层×3000 次全序列**输出 + `draws` 双比对**（最强证明）、9 池 15×3×256、lightning_orb 楼层门控（F6-10 两值都出现、越界恒 charger 且不掷骰）、越界兜底、加载失败保数据、畸形 JSON 保数据。validator +7 条（候选 id 前向、12 槽位数、`floors` 闭区间 1..15、weight>0、空池、**反向可达性**——自动抓出 lightning_orb 类缺口、仅经挑战房可达清单）。门禁：build 0 error、ctest **69/69**（+1 target）、World Validator 0/0；`enemies.json`/`floor_config.cpp` 权重表/`FloorConfig`/`biomes.json`/`spawn_monster` 与 5 处召唤字面量零改动；sim 12/seed3 归一化后 **3479 行 SHA256 逐字节一致**（排除 `__DATE__/__TIME__` 启动行与本批 2 条加载日志）。遗留：`floor_config.json` 仍无加载器且字段已与 C++ 分叉（候选项 D）、`EncounterChoice.risk/effect` 只加载不执行、`biome_events.json` 无加载器。
- G5.5（开发版，未发布）：怪物 AI 与渲染性能优化批次。
  - **怪物记忆系统**：`MonsterAI` 新增记忆三元组（last-known 玩家位置 + 3 秒衰减），玩家离开视野后仍向最后已知位置推进，消除"贴脸才追"的空转感。
  - **普攻模式多样化**：新增 `MonsterAttackPattern`（basic/double_strike/cleave/lunge/spread），`enemies.json` 可配 `attack_pattern` 字段，空值按类型回退（坦克→cleave 1.5x+击退、精英→double_strike 双段、charger→lunge 0.18s 突进、射程≥3 的召唤类→spread 扇形 3 发）；近战怪不授予 spread，避免静默失效。`attack_target` 增加 `damage_mult` 参数承载 cleave 倍率。
  - **Boss 阶段变化增强**：二阶段新增周期性狂暴脉冲（近身击退 + 真实伤害，间隔随二阶段时长由 6s 收敛至 4s）与 HP<25% 一次性"背水一战"（攻速+43%/移速+20%/攻击+15% + 红色演出）。
  - **FX 粒子单批渲染**：`FX_PARTICLE` 由逐颗 8 次 `DrawSphere` 改为相机朝向 additive quad 单批提交（核心+外发光+4 环绕+2 光晕，轨道数学与旧版逐项一致），全帧 1 个 rlgl 批。
  - **视锥体裁剪落地**：FX_QUAD/环形/爆炸逐项按相机 ±220 世界单位裁剪，光束用双端判定避免长束误裁。
  - **天气粒子** 300→150；新增 `src/game/systems/object_pool.h` 通用对象池（基础设施，G11 已接入弹体）。
  - 门禁：Release 0 error · ctest 68/68 · World Validator 0 error / 0 warning。
- G11（开发版，未发布）：对象池接入弹体。
  - **ObjectPool 重构**：改为索引槽位设计。旧实现直接借出 `vector` 元素裸指针，`emplace_back` 扩容会使所有已借出指针同时悬垂；现在按索引访问，扩容不影响既有槽位。`release` 重置为默认状态，杜绝跨生命周期残留数据；`acquire` / `release` 均 O(1)，空闲索引栈复用槽位不缩容。
  - **弹体接入**：`GameScene::projectiles` 由 `std::vector<Projectile>` 改为 `ObjectPool<Projectile>`，每帧 `erase(remove_if)` 的 O(n) 元素搬移换成 `release_if` 逐槽位回收；四处遍历点（玩家弹体 tick、敌方弹体 tick、2D 绘制、3D `_build_projectiles`）改 `for_each`；`Monster::projectiles_ptr` 与 `WeaponExecutor` 签名同步改池类型。
  - 新增 `tests/systems/object_pool_test.cpp` 9 用例，含扩容安全回归与真实弹体集成。
  - 门禁：Release 0 error · ctest 68/68 · World Validator 0 error / 0 warning · `--sim 3` exit=0。
- P1-C7（开发版，未发布）：walkable 判定语义统一——三套判定（`_tile_rect_walkable`/`_sim_tile_passable`/`is_rect_walkable`）收敛为单一入口 `GameMap::is_passable_sim`（WALL/LOCKED/SEALED 不可走，CLOSED 门=Sim 自动开门语义），新增 PassableSimMatrix 测试矩阵。行为等价验证 + 68/68 测试。
- G15（开发版，未发布）：sim 武器获取链修复——拾取恢复 + 保底武器。
  - **拾取永久放弃 bug**：冷却块内 `_pickup_fail_streak` 每帧累加（1.5s 内 0→90）→ 永久放弃拾取 → 50 局全程空手。修复为仅在真实再次尝试时累加。
  - **怪清光后空转**：`_evaluate_move` 无怪时直接 0.1 分，尸体掉落全浪费；修复为先搜 loot/房间再兜底。
  - **第 1 层保底武器**：出生房旁固定 `sword_common`（教学化设计，真玩家同受益）。
  - 效果：avg_damage 10.7→27.7（2.6x）、武器局 1/10→3/10、TIMEOUT 归零。
  - 遗留（P1-C7）：walkable 判定三套语义不统一，拿武器局仍被卡死检测抓到 4/10。
- G14（开发版，未发布）：sim AI 卡死修复链——可达性判定统一 + 路径稳定化。
  - **OPEN 门 BFS 判定 bug**（`sim_ai.cpp:_tile_rect_walkable` 把 OPEN 门也当墙，BFS 永不过门、怪被"门隔离"）；执行层接触开门 `try_open_door_toward` 接线（原死代码）；攻击射程/理想距离对齐武器真实射程（弩不再当近战）。
  - **卡死行为链**：BFS 定向朝怪 / 近身直接攻击 / 近身战豁免 / progressed 三源信号 / 传送失败反向拉怪 / 强开 3x3 CLOSED 门；传送落 CLOSED 门允许+落地即开。
  - **sim 整格步进**（位置恒格点、rect 落单 tile，决策/执行一致）；**拾取冷却+3 次失败放弃**（破 pickup 死循环）；**路径记忆等距锁定**（破对称双路径震荡）；怪物生成排除门 tile + rect 级校验。
  - 门禁：Release 0 error · ctest 68/68 · World Validator 0/0。avg_floor 1.0 → 1.1~4.5。
  - ~~遗留 TODO（P1-C7 专项）：`_tile_rect_walkable` / `is_rect_walkable` / `_sim_tile_passable` 三套 walkable 语义不统一，需单一入口 + 判定测试矩阵。~~ **已收口**（2026-09-26 复核）：`_sim_tile_passable` 已删除，Sim 单 tile 判定统一到 `GameMap::is_passable_sim`（唯一真源，含 LOCKED/SEALED 门阻断）。仅剩矩形级 `is_rect_walkable` 走 tile 标志位、与门状态不联动，且 `player_controller.cpp:194` 已注明是有意的边界处理 —— 非缺陷。
- G13（开发版，未发布）：sim 卡死脱困死锁修复 + 卡死看门狗。
  - **死锁**：卡死 ≥8s 后调用兜底传送，成功与失败都无条件 `return "none"` 且不重置计时；传送失败（最近怪隔墙不同房间）即每帧空转直到烧满 36000 帧。修复为传送失败回落旋转脱困。原 50 行卡死判定块从 `best_action()` 抽出为 `_stuck_escape` (38 行) + `_rotation_escape` (12 行)。
  - **看门狗**：新信号 `sim_stuck_watchdog`，按本局**累计卡死时长** 120s（36000 帧预算约 600s 的 1/5）强制结算为 `STUCK_RECOVERED`。不用"传送失败次数"——传送会周期性成功清零计数，seed21 五局全部因此漏报。阈值取值实测调优，240s 反而更差。
  - **跨层时间回绕**：`enter_floor` 把 `game_time` 归零，残留 `_stuck_since` 变"未来时间"导致累计值出现 -244s 负数、看门狗判不成立。`set_time` 检测时间倒退即丢弃过期计时，累加处 `std::max(0,...)` 双保险。
  - 门禁：Release 0 error · ctest 68/68 · World Validator 0/0 · `git diff --check` 干净。`TIMEOUT_WALL` seed101 6→0、seed21 5→0。
  - 遗留：win_rate 仍为 0，属 AI 推进能力/平衡问题，非本次范围。
- G12（开发版，未发布）：武器攻击路径函数拆分。
  - `WeaponExecutor::execute` 由 91 行（HEAD 既有 90 行遗留，本次改动触及故顺带拆）拆为 30 行编排器 + 4 个单职责助手：`_try_stage3_special` 13 行（combo 第三段特殊技分发）、`_update_range_indicator` 6 行、`_resolve_normal` 25 行（弩弹道 vs 近战 SSOT 几何）、`_finalize_attack` 29 行（协同上下文 + 推进连段 + 事件 + 音频）。纯机械拆分，守卫顺序与事件映射逐行保留。
  - 顺手清理 `ai.cpp` 5 处、`ai.h` 4 处尾随空格，`git diff --check` 归零。
  - 门禁：Release 0 error · ctest 68/68（武器路径 69 用例）· World Validator 0/0 · `git diff --check` 干净 · `--sim 5` exit=0。

## 星标路线

| 版本 | 里程碑 |
|------|--------|
| v1.0.0 | 五项 Stable 冻结验收（API/Save/Mod/Regression/Performance） |
| v1.3.x | 定名「回响深渊」· 新手教程 · 窗口缩放 |
| v1.4.0 | 三存档槽 · 结局收集迁 Meta · 4 个历史 Bug 修复 |
| v1.4.13 | 三群系贴图 + RNG-001 修复 |
| v1.4.14 | 30 怪一怪一图 · Boss 专属立绘 · visual_id 数据驱动 |
| v1.4.32 | M6-n 视觉收口：HD-2D 深度/脚印/装饰/3D 楼梯 + bloom 减弱 |
| v1.5.0 | Demo Release 门禁达成（sim 直达 · 死因基线 · 平衡审计） |
| **v1.6.0** | **平台深化** — B2 死因仪表盘 · B1 Mirror 记忆可视化 HUD · B3M Mirror 学习闭环 (跨局记忆+遗忘曲线) · A1.1 Billboard 真轮廓 · A2 群系性格粒子 (PNG 贴图数据驱动) · A3 实体接收阴影 · A3.1 P0 shader 热修 (雾/阴影/bloom 首次真实生效) · A3.2+fix2 墙顶 2D 同源色调映射 · 未探索区虚空化 · A4 bloom 反馈实验后按预案回退 (教训存档) |
| **v1.7.0** | **翻滚/闪避 (B3)** — Shift+方向纯手感位移 (2 格/0.16s/0.7s 冷却·无无敌帧) · 2D/3D 倾斜压扁+3 段残影+尘土 · Mirror DODGE 采集接入 · sim 基线逐字节零变化 · 已发 Release |
| **v1.8.0** | **镜头语言 (A6)** — Boss 战镜头聚焦 2 秒后自动回归 · 击杀顿帧期间 focus_timer 继续倒计时，stun 结束后回归 BOSS_WAR · 3D 相机基线改为实体中心 · 64 项 CTest 通过 · 已打 tag v1.8-A6 |
| **v1.9.0** | **音效/音乐系统 (A7)** — 三群系 BGM 风格优化 (监牢/火山/深渊各有专属风格) · Boss 专属音乐 (F5/F10/F15 各有主题) · AudioServer 音效集成 · 怪物死亡音效触发点 · 65 项 CTest 通过 · 已打 tag v1.9-A7 |
| **v1.10.0** | **战斗 HUD 优化 (A8)** — HP/XP bar 像素风双层边框 + 高光 + 动态颜色 · 技能栏冷却提示 (图标旋转 + 数字倒计时 + 升级标识) · 小地图标记清晰度 (颜色区分 + 大小分级) · 金币/资源图标 (像素图标 + 数字对齐 + 圣物数量) · 66 项 CTest 通过 · 已打 tag v1.10-A8 |
| **v1.11.0** | **攻击特效优化 (A9)** — 已打 tag v1.11（`v1.11-A9` 内容已被 `v1.11` 覆盖，未重复建 tag） |
| **v1.12.0** | **动画与教程收口 (G12-6)** — AvatarDirector 消重 · 教程 3D 名条 · 披风动画 · 教程函数瘦身 |
| **v1.13.0** | **死代码三连清 + 存档槽位闭环** — G15-G17 死代码（37 文件 −430 行 / buff 叠加泄漏修复 / MetaSystem reward-log 移除）· G18 `execute_event` 254→28 · G19 `draw_hud` 198→28 · G20-G20d 读档绑槽 + 写档前自动备份 + 槽位菜单可还原上一代 + 选关读档种子漂移修复 · G14 测试护栏 74→77 |
| **v1.14.0** | **渲染层超规清零 + 伪原子写修复** — G21 CombatCoordinator 死代码簇 −76 行 · G22-G22c `game_renderer.cpp` 超规 **4→0**（首个清零文件，全仓 109→105）· G22d `MirrorMemoryStore::save_from` 修三处漏洞（**磁盘满时半成品覆盖正式档** / rename 失败残留 / 调用方静默失败）· 渲染回归测试盲区已记录 · ctest 77/77 |
| **v1.15.0** | **死代码扫描器修复 + 任务奖励补发** — G23 扫描器修 5 个 bug（调用点误判为定义差点误删主干方法 · 289s→1.1s）+ 删 21 个死方法 · G24 **任务奖励从静默丢弃改为真正发放**（quests.json 配了 8 条 relic/buff 奖励但发放入口从未被调用——legendary `infinity_orb` 等此前玩家从未真正拿到过）· validator 新增任务奖励交叉校验 · ctest **78/78** |
| **v1.16.0** | **`_process` 拆分完结（G25-A~H）** — 用户指定的遗留第 3 项，1072 行 god function 按 8 批拆成**调度骨架 + 34 个 ≤40 行子方法**：A 尾段计时器衰减 · B 旁白/Story 世界事件/FlowDirector 自动补救（AUTO_* 四分支各自成方法）· C sim 单局超时守卫 + P0-M1 诊断快照 · D **元素事件 VFX 127 行最大单块**（订阅 → 目标反查 → 按元素派发）· E Boss 系统（Phase2 提示/LastStand/Behavior+Domain VFX）· F 敌方投射物三层碰撞 · G 场地道具 switch（图腾/尖刺）· **H 表现层 gate**（BOSS_CINEMATIC 状态机 + HitStop/运镜/冻结三道「本帧停住」门，两路重复的 Boss 过渡去重）。累计 **1072→602 行（−470）**，执行顺序零改动，`--sim 5 --sim-seed 42` SHA256 每批都逐字节等于拆分前基线 · ctest **78/78**。⚠️ H 拆的 BOSS_CINEMATIC/HitStop/运镜路径**未被确定性校验覆盖**（sim 只打到 1.2 层），建议进游戏打一场 Boss 手动验收登场运镜与击杀顿帧 |
| **v1.17.0** | **sim STUCK_RECOVERED 死锁根因修复（G26）** — 确定性可复现的 2/5 死锁死链完整定位：`_pickup_fail_streak` 在**评估时**累加无法区分成败，出生点簇拥 ≥3 件物品时 3 次**成功**拾取被记为失败 → 开局 4.5s **永久禁用拾取** → 拾取是特殊房间唯一触发路径 → 房间永不触发 → 搜刮分支永久空转；再叠加 `_eval_move_loot_and_rooms` 对不匹配方向返回 `0.0f` 而非 `nullopt`，把下游 `_eval_move_approach` **整个挡死** → `atk=0%` 全程、怪恒定 220-407px 外、120s 累计卡死预算耗尽。修复：两处不匹配改返回 `std::nullopt` + 房间权重 0.6→0.5（原来与 approach 同分，方向索引低者恒胜）+ 删除永久放弃机制。结果 **avg_floor 1.20→1.80**、avg_turns −66%、承伤 2.7x→3.7x 但**伤害输出 17→42**、单局 picks 0→5-8、kills 0-2→9-14、最深 2→**4 层**。胜率仍 0/5 但**死亡原因已从死锁转为真实战斗**，8.0s 传送脱困门槛首次可到达。确定性基线已更新为 `4FA7ADBBAFC558B5F949AB628A1DD4B140FD167CBF0D7654A010D5A2C3B2B9FC`（修 bug 必然变更）· ctest **78/78** |
| **v1.18.0** | **sim 批次局间独立性（G27）** — 根因是**函数级 `static` 跨层跨局泄漏**而非随机流：`PlayerController::tick()` 内 `_sim_last_step` 与 `gs.game_time` 做节流比较，而 `game_time` 在 `enter_floor` **每次归零** → 进层后差值恒负，**sim 玩家永久无法移动**。影响范围远超预期：**单独跑同样中招**（第一层靠 `_sim_last_step` 初值 `-1` 还能动，第二层起冻住），G26 整份平衡数据集是在「玩家第一层之后基本不动」的状态下采出来的。修复：4 个 static 归位为 `PlayerController` 实例成员 + 新增 `reset_floor_state()` 由 `enter_floor` 在 `game_time = 0` 处调用；`PlayerController` 是 `GameScene` 成员，新局自动拿默认值，跨局泄漏随新建场景方案一并消解。连带修 `change_scene` 提前置 `_skip_input`（覆盖「已请求切换、尚未 swap」那一轮的输入派发）与新局首帧清 `sim_stuck_watchdog`。取证反面教材：先用 `CountingRng` 逐掷钩子证明**随机流逐掷一致**、楼层地图字节级相同，才转查移动/碰撞层；中途被同类 static `_loot_dbg` 骗过一次（它让 SIM-LOOT 采样相位跨局漂移，两跑 `inv=3` vs `inv=4` 纯属采样点不同）。验证：批次 5 局对拍 5 个同种子单独跑，结构化摘要 **5/5 MATCH**，`SequenceMatcher` 逐行只差 3 行一次性监听器注册日志（`_s_registered` 幂等守护正确跳过）。效果 **avg_floor 1.80→4.0**、最深 4→**6 层**、首次打到 F5 Boss 层。新基线 `8EE12B30AC155DBBCCE672AF1F9A9D65AEBD86E4E9E4C709F64EF987BFF6BA42`（两次运行逐字节一致）· ctest **78/78** |
| **v1.19.0** | **`enter_floor` 255 行拆完（G28）** — G25 之后的第二个零基线漂移重构批次。比 G25 难一层：`_process` 段间靠成员变量传状态，`enter_floor` 却有栈上局部变量跨段落存活（`DungeonGenerator gen` 生成后 20 行才由 `_spawn_normal_floor` 消费；`rooms` 被 4 个下游消费），必须先划"变量生命线"再决定哪段能抽走 —— `gen` 无法抽走，只能把不依赖它的段拆出去。按生命周期分三阶段拆出 **14 个 ≤40 行子步骤**（重置 3 · 生成+放置 6 · 事件+推进 5），**255→39 行，`game_scene.cpp` 超规 1→0**。两处自抓的语义偏离：给掷骰判断加防零守卫会**改变 `rng()` 调用次数**（已还原）、把 `fcfg->is_boss`（配置值）换成 `is_boss_floor(floor)`（楼层号推导）不可假设等价（已改回）。除 SHA256 全量比对还单独 grep 全部 `rng()` 调用点核对顺序未变。**基线零漂移，仍为 G27 的 `8EE12B30…`** · ctest **78/78** |
| **v1.20.0** | **重复源树清理 + 两个守护工具（G29）** — 清掉八个嵌套自复制树（`assets/assets/` 632 文件 58 MB、`src/src/`+`src/src/src/` 720 文件、`docs/tests/resources/tools/vendor/.github` 各一份）+ `vendor/` 根层误放的 8 个仓库根副本 + `pc_spec.json`，全部 `git ??` 未跟踪（某次同步脚本跑歪）。**危险点**：`file(GLOB_RECURSE SOURCES src/*.cpp)` 会把嵌套副本递归全收，而 `main.cpp` 当时存在三份、`src` 下 486 个 .cpp（真实 162）——任何人做一次干净 `cmake -B build` 就 LNK2005 多定义编译失败，**增量构建完全无感**，`.gitignore` 也挡不住。删除前审计确认拟删目标里 git 跟踪文件 = 0，删完 `git ls-files` 1331→1331 无一丢失，释放 **67.6 MB**。整个 `build` 删掉从零重跑验证：GLOB 重算 162 个唯一 .cpp、`src/src` 残留 0、全量 107 秒、ctest **78/78**、基线逐字节一致。新增 `tools/fn_length_scan.py`（修掉三类误判，其中 `(\w+)\(` 匹配不到 `PlayerController::tick(`，219 行的 tick 就这么漏了）与 `tools/check_source_layout.py`（嵌套自复制 / 源目录产物 / 根文档误入 vendor，退出码可接 CI）。**真实超规账本：108 个函数超 40 行**（此前"36 个"是坏扫描器的假数），榜首 `_process` 609 / `_build_effects` 419 / `tick` 219。清理本身不可提交（未跟踪），故仅工具与文档入库 |
| **v1.21.0** | **`game_scene.cpp` 超规清零（G30-A~K）** — 11 批纯结构搬运，该文件 12 个超 40 行函数全部拆完：**`_process` 609→40**（分四批，最大单体）· `_render_ui_tail` 391→21 · `_collect_sim_stats` 218→28 · `_render` 169→13 · `_draw_entities` 128→13 · `_draw_mirror_analysis_panel` 102→11 · `new_game` 99→10，累计 **87 个新 helper** 全部 ≤40 行 · 全仓超规 **108→96** · 校验方法升级为「带标签重建法」（旧 `is_scaffold` 法会误删正文花括号，已废弃）· 自抓并拦下 4 个自身缺陷（含 `void`→`bool` 漏补 `return false`）· **基线零漂移**，`--sim 2 --sim-seed 1234` SHA256 每批逐字节等于 G27 基线 `7ADE6742E3EF1C80…`，与 G25/G28 并列第三个零漂移重构里程碑 · ctest **78/78** |

