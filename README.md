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

- A5-T3/T4（开发版，未发布）：玩家分件骨骼接入 2D 与 HD-2D，共用姿态、镜像和缩放；3D 支持透明裁剪、分件投影和残影排序。37 项动画测试、63 项 CTest 与 World Validator 通过；隔离运行截图确认待机、行走和攻击姿态变化。当前仍为绿色占位素材，翻滚/受击的视觉验收及 T5 正式素材尚待完成。3D 分件 shader 不可用时回退旧静帧。

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
