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

**方式一 · 即玩（推荐）**：GitHub [Release 页](https://github.com/DIO07-0309/roguelike-cpp/releases) 下载 zip → 解压 → 双击 `roguelike_cpp.exe`。exe 自包含运行库，无需任何环境。

**方式二 · 源码构建**（第三方库已入库，clone 即编译）：

```bash
git clone https://github.com/DIO07-0309/roguelike-cpp.git
cd roguelike-cpp
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

- **61 个 ctest 全绿** · 确定性模拟器（同种子字节级复现，`--sim N` 批量平衡评估）
- **AI 研究平台**：行为树 / MCTS / Q-Learning / 镜像学习（行为克隆 + n-gram 战术链 + Thompson 采样五层仲裁）
- **架构**：组合优于继承（5 Director 组合）、EventBus 45 事件、数据驱动 Registry、函数 ≤40 行规范

→ 完整技术文档：**[docs/TECH.md](docs/TECH.md)** · 架构权威：[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) · 开发流水：[CHANGELOG.md](CHANGELOG.md)

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
