# 批次8：lightning_orb 运行时刷出路径 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 `lightning_orb`（电光之核）从「30 怪里唯一不可生成者」变成 F6-10（volcano）常规层与挑战房火山波 1 可刷出的怪，并修掉 `biome.cpp` 的潜伏崩溃点。

**Architecture:** 不改任何数据面。刷怪选怪是 C++ 硬编码字面量（非常见误解：`enemy_pool` 是死数据），所以本批只动 3 处 C++ 字面量/条件，把 slot 6 在火山层轮换出一个 id、把挑战房火山池加满到 4 个 id，并给 `enemy_weights` 补独立的 `contains` 保护。

**Tech Stack:** C++17 / MinGW / Raylib 5.0 / nlohmann::json / GoogleTest / PowerShell 5.1

**设计文档：** `docs/superpowers/specs/2026-09-24-a6s2-batch8-lightning-orb-spawn-design.md`（已批准，提交 `796713d`）

## Global Constraints

- 基线：HEAD `9dd97c8`，工作树 clean
- **零 JSON、零资源改动**：`enemies.json`、`floor_config.cpp` 权重表、`biomes.json`、`world/*.json` 一律不动
- 函数长度 ≤40 行；类只做一件事；组合优于继承；变量命名语义化
- 不允许一次生成超过 300 行代码
- **不改** CMakeLists.txt 编译器标志
- 禁止在 `.h` 里 `using namespace std`；禁止裸 `new`
- 提交：**单次提交** `feat(a6-s2): 批次8 - lightning_orb 运行时刷出路径`，覆盖全部 4 文件（无中间 commit）
- 不触碰 `Roguelike-CPP-初代版`（已冻结）
- 桌面同步保留 `saves/` 与 `3D模式.exe.lnk`

### 环境陷阱（务必照抄）

- 构建：`cmake --build build --config Release -- -j 4` —— **不要用 `/m`**，它会透传给 mingw32-make 并报错
- 测试：`ctest --test-dir build` —— **不要加 `--build-nofail`**（那是 cmake 的选项）
- Validator：`conda run python tools\world_validator.py`
- 跑 sim 必须先设 UTF-8 控制台，否则日志里的中文会变成 U+FFFD 乱码、无法 grep 怪物名：
  ```powershell
  [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
  $OutputEncoding = [System.Text.Encoding]::UTF8
  $env:PYTHONIOENCODING = "utf-8"
  ```
- **PowerShell 命令里不要写中文字面量**，PS 5.1 会把它编码成乱码并可能让整行解析失败。需要中文时用 Python + `chr(0x...)`
- `git show HEAD:path | Out-File -Encoding utf8` 会写入 BOM；需要字节精确时用 `[System.IO.File]::WriteAllText`
- `Get-FileHash` 取 sha256

---

## Task 1: 采集 sim 基线（必须在改任何代码之前）

**Files:**
- 产出：`C:\Users\HP\AppData\Local\Temp\opencode\sim_before8.txt`

**为什么必须先做：** sim 输出由二进制决定，改了代码重建后就再也跑不出「修改前」的输出。

- [ ] **Step 1: 用当前 `9dd97c8` 二进制跑 sim 基线**

```powershell
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8
$LASTEXITCODE = 0
.\build\roguelike_cpp.exe --sim 12 --sim-seed 3 2>&1 | Out-File -Encoding utf8 C:\Users\HP\AppData\Local\Temp\opencode\sim_before8.txt
"exit $LASTEXITCODE"
(Get-Item C:\Users\HP\AppData\Local\Temp\opencode\sim_before8.txt).Length
Get-FileHash C:\Users\HP\AppData\Local\Temp\opencode\sim_before8.txt | ForEach-Object { "sha: " + $_.Hash }
```

- [ ] **Step 2: 记录基线事实（改代码前的事实，不可事后复现）**

期望输出特征（`9dd97c8` 实测值，供交叉核对）：

| 项 | 值 |
|---|---|
| exit code | `0` |
| 文件大小 | 190450 bytes |
| 行数 | 3481 |
| sha256 | `AEEA0873BBF0CD2606CDDE445C8B5A3D8516D7B3382AAF0474EDDAD2C7991996` |
| 结局 | `wins:0, avg_floor:1.33` —— **12 个 agent 全死在 F1-2** |
| `电光之核` 命中 | **0** |
| `冲锋兽人` 命中 | **0**（charger 从 F4 就能刷，但 sim 到不了 F4） |

**关键推论（决定后续验收设计）**：slot 6 权重在 F1-F3 恒为 `0`，sim 只打到 F1-2 → **sim 结构上无法验证本批改动**。sim A/B 因此降级为「信息性记录 + 无意外 diff」，真正的证明落在 Task 7 的实机验收。

---

## Task 2: 常规层 slot 6 火山轮换（spec §1）

**Files:**
- Modify: `src/game/systems/floor_manager.cpp:32`

**Interfaces:**
- 不改签名：`static const char* _pick_monster_type(const FloorConfig& cfg)`（`:13`）
- 消费 `FloorConfig::floor`（`floor_config.h:11`，取值 1-15）
- 产出 id 字符串喂给 `spawn_monster(px, py, type)`（`monster.cpp:413`）

- [ ] **Step 1: 改一行**

`src/game/systems/floor_manager.cpp:32`，把

```cpp
                case 6: return "charger";   // D8
```

替换为

```cpp
                case 6: return (cfg.floor >= 6 && cfg.floor <= 10 && rng() % 2 == 0) ? "lightning_orb" : "charger";   // D8; 批次8: F6-10 火山轮换
```

- [ ] **Step 2: 自查零回归语义**

`cfg.floor < 6 || cfg.floor > 10` 时 `&&` 短路，表达式恒为 `"charger"` → **F1-5 与 F11-15 行为逐字节不变**。函数长度保持 30 行（`:13-42`），不越 40 行上限。

- [ ] **Step 3: 静态确认接线**

```powershell
conda run python -c "import pathlib,re; t=pathlib.Path(r'C:\Demo\roguelike_cpp\src\game\systems\floor_manager.cpp').read_text(encoding='utf-8'); print('lightning_orb hits:', t.count('lightning_orb'))"
```

期望 `1`。

---

## Task 3: 挑战房火山波 1 池加满（spec §2）

**Files:**
- Modify: `src/game/world/challenge_room.cpp:37`

**Interfaces:**
- 不改签名：`static const char* ChallengeRoomController::_pick_monster_type(int floor, int wave, uint32_t rng)`（`challenge_room.h:95`，`private static`）
- 消费 `Pool{ const char* types[4]; int count; }`（`challenge_room.cpp:19`）—— **容量上限 4，加满不越界**
- `pick` 用 `r % count`（`:22`），种子 `_deterministic_seed`（`:53-62`）不变

- [ ] **Step 1: 改一行（加一项 + count 3→4）**

`src/game/world/challenge_room.cpp:37`，把

```cpp
            {{"fire_imp", "bomber", "frost_slime"}, 3},
```

替换为

```cpp
            {{"fire_imp", "bomber", "frost_slime", "lightning_orb"}, 4},
```

- [ ] **Step 2: 自查范围隔离**

该字面量位于 `if (floor <= 10)` 分支内（`:34`），Prison（`floor <= 5`）与 Abyss 池**零改动**。

- [ ] **Step 3: 语义核对**

`lightning_orb` 属 F6-10 volcano 章，且 `enemies.json:326-350` 中 `type` 为 `"charger"`、`role` 为 `"flank"`，与槽位/池的既有语义一致。

---

## Task 4: biome.cpp 缺省保护（spec §3）

**Files:**
- Modify: `src/game/world/biome.cpp:63-66`

**Interfaces:**
- 不改任何公共 API；`BiomeDef::enemy_pool` / `enemy_weights` 字段与类型不变
- 运行时行为**零变化**（当前 3 个 biome 的 pool 与 weights 成对存在）

- [ ] **Step 1: 拆 `if` 补 `contains`**

`src/game/world/biome.cpp:63-66`，把

```cpp
            if (obj.contains("enemy_pool")) {
                for (auto& e : obj["enemy_pool"]) b.enemy_pool.push_back(e.get<std::string>());
                for (auto& w : obj["enemy_weights"]) b.enemy_weights.push_back(w.get<float>());
            }
```

替换为

```cpp
            if (obj.contains("enemy_pool")) {
                for (auto& e : obj["enemy_pool"]) b.enemy_pool.push_back(e.get<std::string>());
            }
            if (obj.contains("enemy_weights")) {
                for (auto& w : obj["enemy_weights"]) b.enemy_weights.push_back(w.get<float>());
            }
```

- [ ] **Step 2: 为什么必须修（记入 Code Review 说明）**

nlohmann `operator[]` 对非 const `json` 缺键会构造空 `null` 节点，随后对非数组 `.push_back` 抛 `type_error` → 被外层 `catch`（`biome.cpp:78`）吞掉 → **整个 `load_biome_defs` 返回 false** → 全 15 层 biome 的 `palette`/`ambient`/`bgm`/`boss_id` 全部失效。这是潜伏缺陷：下次有人往 `enemy_pool` 加条目却漏了 `enemy_weights`，整个地图会被静默刷掉而无人察觉。

- [ ] **Step 3: 验证 biome 仍能加载**

在 Task 5 的 sim 输出里确认出现 `[Biome] Loaded 3 biomes`。

---

## Task 5: 门禁验证

**Files:** 无改动，只跑检查

- [ ] **Step 1: 构建**

```powershell
cmake --build build --config Release -- -j 4
```

期望：0 error（末行 `Built target ...`）。

- [ ] **Step 2: ctest**

```powershell
ctest --test-dir build
```

期望：**`100% tests passed, 0 tests failed out of 68`**。本批**不新增断言**，测试数保持 68（理由见 spec §4 附带说明：两个 `_pick_monster_type` 都是不可直接测的静态私有函数，走公共接口测 spawn 需先搭 GameMap + Registry 全量基础设施，团队已在 `challenge_room_test.cpp:92` 记录过这个成本）。

- [ ] **Step 3: World Validator**

```powershell
conda run python tools\world_validator.py
```

期望：`Errors: 0` / `Warnings: 0`。

- [ ] **Step 4: 静态接线证明**

```powershell
conda run python -c "import pathlib; hits=[]; [hits.append(str(p.relative_to(pathlib.Path(r'C:\Demo\roguelike_cpp').resolve()))) for p in pathlib.Path(r'C:\Demo\roguelike_cpp\src').rglob('*.cpp') if 'lightning_orb' in p.read_text(encoding='utf-8', errors='ignore')]; print(len(hits)); [print(' ', h) for h in hits]"
```

期望命中 **3 个文件**：`src/game/entities/monster.cpp`（既有配色）、`src/game/systems/floor_manager.cpp`（本批）、`src/game/world/challenge_room.cpp`（本批）。

- [ ] **Step 5: sim A/B（信息性，不作为通过条件）**

```powershell
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8
$LASTEXITCODE = 0
.\build\roguelike_cpp.exe --sim 12 --sim-seed 3 2>&1 | Out-File -Encoding utf8 C:\Users\HP\AppData\Local\Temp\opencode\sim_after8.txt
"exit $LASTEXITCODE"
(Get-FileHash C:\Users\HP\AppData\Local\Temp\opencode\sim_before8.txt).Hash
(Get-FileHash C:\Users\HP\AppData\Local\Temp\opencode\sim_after8.txt).Hash
(Get-Content -Encoding utf8 C:\Users\HP\AppData\Local\Temp\opencode\sim_after8.txt | Measure-Object).Count
```

判定：
- **预期 sha 相同**（agent 死在 F1-2，slot 6 在 F1-3 权重为 0，改动不可达）
- 若 sha 不同：diff 中**只允许**出现启动时间行（`__DATE__`/`__TIME__`，`src/main.cpp:128`）；**出现其它差异必须停下来排查**
- 无论是否相同，确认输出含 `[Biome] Loaded 3 biomes`

---

## Task 6: README CHANGELOG

**Files:**
- Modify: `README.md`（在批次7 条目之后插入）

- [ ] **Step 1: 定位锚点**

```powershell
Select-String -Path README.md -Pattern "A6-S2" | ForEach-Object { $_.LineNumber }
```

批次7 条目当前在第 133 行，新条目插在它之后（`- G5.5` 条目之前）。

- [ ] **Step 2: 插入一行 CHANGELOG**

内容（单行，`- ` 开头，与前批次同一缩进风格）：

```markdown
- A6-S2 批次8（开发版，未发布）：lightning_orb（电光之核）运行时刷出路径打通——调研发现 `BiomeDef::enemy_pool` 与整个 `resources/world/*.json` 均为**死数据**（C++ 零消费者，批次5 往池里加的 +1 实际不产生任何运行时效果），真正的选怪决策是 C++ 硬编码字面量 + 编译期 12 槽位权重表；本批只动 3 处 C++：`floor_manager.cpp` slot 6 在 F6-10（volcano）50/50 轮换 `lightning_orb`/`charger`（F1-5/F11-15 恒退化 charger，零回归）、`challenge_room.cpp` 挑战房火山波 1 池 `types[4]` 加满、`biome.cpp` 给 `enemy_weights` 补独立 `contains` 保护（原实现缺该字段会抛异常被 catch 吞掉、导致**全 15 层 biome 静默加载失败**，潜伏缺陷）。`enemies.json`/`floor_config.cpp` 权重表/`biomes.json`/`world/*.json` 零改动，本批零 JSON 零资源。门禁：build 0 error、ctest 68/68、World Validator 0/0、`grep lightning_orb src/` 命中 1→3；sim 12/seed3 实测 `wins:0, avg_floor:1.33`（agent 死在 F1-2，slot 6 在 F1-3 权重为 0）**结构上无法验证本批**，故 sim A/B 降为信息性记录（预期 sha 不变）。遗留教训：该缺口连跨 3 批次才被发现，根因是刷怪表应为数据而非 C++ 字面量（数据驱动刷怪管线另立设计）。
```

- [ ] **Step 3: 确认插入成功**

```powershell
Select-String -Path README.md -Pattern "A6-S2" | ForEach-Object { $_.LineNumber }
```

期望批次8 出现在批次7 之后一行。

---

## Task 7: 桌面开发包同步

**Files:**
- 目标：`C:\Users\HP\Desktop\Roguelike-CPP-3D版`

- [ ] **Step 1: 镜像 8 目录**

```powershell
$dst = "C:\Users\HP\Desktop\Roguelike-CPP-3D版"
foreach ($d in "src","resources","tools","tests","docs","assets",".github","vendor") {
    robocopy "C:\Demo\roguelike_cpp\$d" "$dst\$d" /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
    "  $d exit $LASTEXITCODE"
}
```

robocopy exit 0/1 均为成功（1 = 有文件被复制）。

- [ ] **Step 2: 5 个根文件 + exe/dll 到包根目录**

```powershell
$dst = "C:\Users\HP\Desktop\Roguelike-CPP-3D版"
foreach ($f in "CMakeLists.txt","README.md","CLAUDE.md","CMakePresets.json",".gitignore") {
    Copy-Item "C:\Demo\roguelike_cpp\$f" "$dst\$f" -Force
}
Copy-Item build\roguelike_cpp.exe "$dst\roguelike_cpp.exe" -Force
Copy-Item build\raylib.dll "$dst\raylib.dll" -Force
```

**exe 必须复制到包根目录**（用户测试只点根目录的 `roguelike_cpp.exe`，不进 `build/`）。

- [ ] **Step 3: 校验同步 + 保留项未动**

```powershell
$dst = "C:\Users\HP\Desktop\Roguelike-CPP-3D版"
(Get-FileHash "$dst\roguelike_cpp.exe").Hash.Substring(0,12)
(Get-FileHash build\roguelike_cpp.exe).Hash.Substring(0,12)
Test-Path "$dst\saves"
Test-Path "$dst\3D模式.exe.lnk"
```

期望：两个 hash 前 12 位相同；两个 `Test-Path` 均为 `True`。

---

## Task 8: 用户实机验收 + 单提交

**Files:** 无改动

- [ ] **Step 1: 提请用户实机验收**

告知用户：`C:\Users\HP\Desktop\Roguelike-CPP-3D版\roguelike_cpp.exe`，验收点：

1. **F6-10 常规层**（熔岩炼狱章，BGM 为 volcano）——多跑几层，确认能刷出**电光之核**，且骨骼行走/挥砍/受击、镜像、脚贴地正常
2. **挑战房火山波 1**（需钥匙解锁）——确认池内可见电光之核
3. **零回归**：F1-5 与 F11-15 怪物构成无变化；其它怪与 NPC 外观无变化
4. **地图完整**：各层 biome 配色/氛围/BGM 正常（验证 `biome.cpp` 修改后加载未破坏）

**必须等用户回复「通过」才能提交。** 这是本批唯一的实质证明（sim 证明不了）。

- [ ] **Step 2: Code Review 自查（提交前）**

```powershell
git status --short
git diff --stat
```

期望恰好 4 个 ` M`：`README.md`、`src/game/systems/floor_manager.cpp`、`src/game/world/biome.cpp`、`src/game/world/challenge_room.cpp`；`git diff --stat` 约 `+6 / -3`。

- [ ] **Step 3: 单提交**

```powershell
git add -A
git commit -m "feat(a6-s2): 批次8 - lightning_orb 运行时刷出路径" -m "打通 lightning_orb（电光之核）刷出路径——它是 30 怪里唯一不可生成者（enemies.json:326-350 定义完整、骨架与白名单 actor_avatars.json:67 与渲染色值 monster.cpp:398 全部就位，唯一断点就是没人调用它）。调研发现 BiomeDef::enemy_pool 与整个 resources/world/*.json 均为死数据（src/ 零消费者），批次5 往池里加的 +1 不产生任何运行时效果；真正的选怪是 C++ 硬编码字面量 + 编译期 12 槽位权重表。三处改动：floor_manager.cpp slot 6 在 F6-10（volcano，bgm 与 F6-10 严格重合）50/50 轮换 lightning_orb/charger，cfg.floor 短路保证 F1-5/F11-15 逐字节不变；challenge_room.cpp 火山波 1 池 types[4] 加满（fire_imp/bomber/frost_slime/lightning_orb，count 3->4）；biome.cpp 给 enemy_weights 补独立 contains 保护——原实现缺该字段会抛 type_error 被 catch 吞掉导致全 15 层 biome 静默加载失败，属潜伏缺陷。零 JSON 零资源改动；enemies.json、floor_config.cpp 权重表、biomes.json、world/*.json 一律不动。门禁：build 0 error、ctest 68/68（本批无新增断言，两个 _pick_monster_type 均为不可直接测的静态私有函数）、World Validator 0/0、grep lightning_orb src/ 命中 1->3。sim 12/seed3 实测 wins:0 avg_floor:1.33，agent 死在 F1-2 且 slot6 在 F1-3 权重为 0，故 sim 结构上无法验证本批，A/B 降为信息性记录（预期 sha 不变）。范围外并留档：数据驱动刷怪管线、world/*.json 与 biomes.json 双源合并、floor_config.json 无加载器且字段已分叉、EncounterChoice.risk/effect 只加载不执行、biome_events.json 无加载器。遗留教训：该缺口连跨 3 批次才被发现，根因是刷怪表应为数据而非 C++ 字面量。实机验收通过。"
git log --oneline -2
```

- [ ] **Step 4: 收尾确认**

```powershell
git status --short
```

期望输出为空（工作树 clean）。

---

## 自检记录

- **spec 覆盖**：§1 → Task 2；§2 → Task 3；§3 → Task 4；§4 → Task 5（并新增 Task 1 基线采集，因实测发现 sim 结构性无效）；§5 → Task 6/7/8；§6 改动量预估 → Task 5 Step 5 与 Task 8 Step 2 交叉校验
- **无占位符**：无 TBD/TODO，所有代码块为可逐字粘贴的完整内容
- **类型/签名一致**：`_pick_monster_type(const FloorConfig&)`、`_pick_monster_type(int, int, uint32_t)`、`Pool{types[4], count}` 与源码一致
- **与已批准 spec 的差异（已回写 spec）**：spec §4 原判「sim 必须变化、diff 只允许怪物 id 行」经实测证伪——`wins:0, avg_floor:1.33` 使 agent 到不了 F4，slot 6 在 F1-3 权重恒 0，且基线里连 F4 就能刷的 `charger`（冲锋兽人）命中数也是 0。已据此改写 spec §4 为「静态接线证明 + sim 信息性记录 + 实机验收为唯一实质证明」，并在 spec 补「为什么本批不加单元测试」一节
