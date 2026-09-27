# Roguelike — 地牢肉鸽 (C++17 + Raylib 5.0)

## 架构
- C++17 + Raylib 5.0 + CMake 3.16+
- 数据驱动：JSON 配置 → Registry → 游戏运行时
- 240+ 源文件，按模块分层：`src/core/` `src/game/` `src/ai/` `src/data/`
- 不引入第三方 ECS；使用组合优于继承的手工架构
- 构建：`vendor/raylib/` (include+lib) + `vendor/json/` (header-only)

## 版本策略（2026-09-27 定，最高优先级）
- **本仓库 = 3D 版。2D 开发已停止，新功能一律往 3D 做。**
- `C:\Users\HP\Desktop\Roguelike-CPP-初代版` 是 **2D 最终保留包**，已冻结，
  禁止任何改动/同步（除非修致命 bug，且需用户明确同意）。
- 本仓库不再做「2D/3D 二选一」的开关、兼容层或双模式菜单——游戏是 3D，
  2D 绘制路径属于遗留代码，只在修 3D 问题时被动触碰，不主动投入重构。
- 涉及表现层的改动前，先确认目标形态是 3D，不是 2D。

## 开发命令
- 构建 Release：`cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`
- 构建 Debug：`cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build`
- 运行：`build/roguelike_cpp.exe`
- 跑测试：`cmake -B build -DENABLE_TESTS=ON && cmake --build build && cd build && ctest`
- 跑 World Validator (G7.1)：`python tools/world_validator.py`
- Python 工具均需 conda 环境：`conda run python tools/xxx.py` 或先 `conda activate`

## 开发规范（必须遵守）
1. 任何函数长度不超过40行
2. 一个类只负责一件事情
3. 优先组合而不是继承
4. 所有变量命名必须语义化
5. 每次修改代码前先分析影响（用 Grep 搜索所有引用点）
6. 修改完成必须进行 Code Review
7. 每完成一个 Milestone 更新 README 的 CHANGELOG
8. 不允许一次生成超过 300 行代码
9. 如果有更好的设计方案，请先讨论，再编码
10. 永远以可维护性优先
11. 每次代码/配置更新并验证通过后，必须同步桌面开发测试包 `C:\Users\HP\Desktop\Roguelike-CPP-3D版`：镜像 `src/ resources/ tools/ tests/ docs/ assets/ .github/ vendor/` + 根文件（CMakeLists.txt README.md CLAUDE.md CMakePresets.json .gitignore），**exe 必须复制到桌面包根目录**（用户测试只点根目录的 `roguelike_cpp.exe`，不进 build/ 子目录）。保留桌面独有的 `saves/`（含 `.bak` 备份）、`3D模式.exe.lnk` 等。桌面 `Roguelike-CPP-初代版` 是 2D 最终保留包，**已冻结，禁止任何改动/同步**

## 代码风格
- 使用 `PascalCase` 类名，`camelCase` 方法名，`snake_case` 变量名
- 所有 `enum class`，禁止裸 `enum`
- 所有 `new`/`delete` 必须逐步替换为 `std::unique_ptr`
- 每个 `.h` 必须有 `#pragma once`
- JSON 字段名全部 `snake_case`

## 常见陷阱
- Raylib 的 `DrawText` 不支持中文，中文文本需用 `GuiFont::DrawTextCH()`
- MinGW 编译需要 `-finput-charset=UTF-8 -fexec-charset=UTF-8`
- 修改 JSON 配置后必须跑 World Validator 检查一致性
- Registry 查找返回裸指针，调用方必须检查 `nullptr`
- `src/data/` 里的 JSON 加载器返回 `std::optional`，调用方必须处理 `std::nullopt`

## 目录约定
- `src/core/` — 底层基础设施（容器、数学、引擎抽象）
- `src/game/` — 游戏逻辑（entities/systems/world/scenes/rendering/audio/save）
- `src/ai/` — AI 系统（BT节点、MCTS、导航、RL环境）
- `src/data/` — JSON 配置加载器与数据验证
- `tests/` — GoogleTest 单元测试，按模块分目录
- `assets/` — 运行时资源（音频、纹理、字体 — 当前仅含 `jojo_timestop.mp3`）
- `resources/` — **数据骨干**：20+ JSON 配置（enemies/relics/skills/encounters/dialogues/biomes/bosses/items/buffs/quests/endings 等），由 `world_validator.py` 校验
- `docs/` — 设计文档（设计决策前查阅对应文档）
- `tools/` — Python 开发工具脚本
- `vendor/` — 第三方库：raylib 5.0 (`include/` + `lib/`)，nlohmann/json (`include/` header-only)

## 不要做
- 不要新增 2D 功能或 2D/3D 双模式菜单（2D 开发已停止，见上方版本策略）
- 不要为遗留 2D 绘制路径做美化或大重构（除非它在阻塞 3D 工作）
- 不要安装新的 C++ 第三方库除非我明确同意
- 不要修改 CMakeLists.txt 的编译器标志（UTF-8 配置已验证）
- 不要直接 `new` 创建对象，使用工厂方法或智能指针
- 不要在 `.h` 文件中 `using namespace std`
- 不要跨过 `src/` 的模块边界直接访问内部实现

## 开发工具

| 工具 | 用途 | 何时使用 |
| :--- | :--- | :--- |
| `python tools/world_validator.py` | 全量 JSON 交叉引用校验（20+ 类资源一致性） | 修改任何 JSON 配置后 |
| `python tools/extract_chars.py` | 提取所有 CJK 字符用于字体生成 | 新增中文文本/UI 文案后 |
| `python tools/replace_methods.py` | 批量方法体替换（重构辅助脚本） | 大规模重构 game_scene.cpp 时 |

## 设计文档

| 文档 | 内容 | 何时查阅 |
| :--- | :--- | :--- |
| `docs/ARCHITECTURE.md` | 模块架构、核心类关系、数据流 | 跨模块改动、理解项目结构 |
| `docs/G4_PLATFORM_BIBLE.md` | 平台兼容性规范（Win/Mac/Linux） | 编译/构建/平台兼容问题 |
| `docs/WORLD_LORE.md` | 游戏世界观、剧情、boss 设定 | 新增内容/怪物/剧情事件 |
| `docs/D1_GAMEPLAY_LOOP_DESIGN.md` | 游戏循环设计、回合制逻辑 | 修改核心循环/战斗机制 |
| `docs/ART_ASSET_PLAN.md` | 美术资源规划（角色/怪物/UI） | 资源替换、新增精灵/纹理 |
| `docs/ART_STYLE_GUIDE.md` | 美术风格指南、配色规范 | UI/特效/战斗界面样式 |
