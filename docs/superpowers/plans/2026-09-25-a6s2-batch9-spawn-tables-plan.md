# A6-S2 批次9 实现计划：刷怪表数据化

- 日期：2026-09-25
- 依据：`docs/superpowers/specs/2026-09-25-a6s2-batch9-spawn-tables-design.md`（已批准）
- 基线：HEAD `b18b622`，工作树 clean
- 执行方式：待用户确认（Inline / Subagent）

---

## 步骤总览

| # | 动作 | 产出文件 | 回滚 |
|---|---|---|---|
| 1 | 写 slot→id 数据表 | `resources/enemy_slots.json` | `git checkout --` / 删文件 |
| 2 | 写挑战房池 | `resources/challenge_pools.json` | 同上 |
| 3 | 写加载器头文件 | `src/data/spawn_tables.h` | 同上 |
| 4 | 写加载器实现 | `src/data/spawn_tables.cpp` | 同上 |
| 5 | 重接 `floor_manager.cpp` | 同文件 | `git checkout --` |
| 6 | 重接 `challenge_room.cpp` | 同文件 | 同上 |
| 7 | 接线 `main.cpp` | 同文件 | 同上 |
| 8 | 扩展校验器 | `tools/world_validator.py` | 同上 |
| 9 | 写 golden oracle 测试 | `tests/spawn_tables_test.cpp` | 删文件 |
| 10 | 门禁全绿 | — | — |
| 11 | CHANGELOG + 桌面同步 | `README.md` + 桌面包 | — |

---

## 1. `resources/enemy_slots.json`

**动作**：按设计 §4.1 原样落盘（UTF-8 无 BOM）。

**约束**：
- `slots` 数组 12 项，下标 = 槽位序号，与 `FloorConfig::enemy_weights[12]` 位置对齐
- **候选顺序 = 抽签顺序**，必须与 `floor_manager.cpp:26-37` 的分支返回顺序逐字一致（已核对）
- 槽位 0 权重必须 `orc:1` / `slime:2`（复现 `rng()%3==0`）
- 槽位 6 中 `lightning_orb` 必须排在 `charger` **之前**且带 `"floors":[6,10]`
- `"elite"` 保留为槽位 5 的 id（运行期由 `spawn_monster` 解析），同时在 `aliases` 中声明供校验用

**验证**：`conda run python -c` 不可用 → 写临时脚本，断言 12 项、候选 id 全在 `enemies.json`、槽位 6 顺序。

**回滚**：删除文件。

---

## 2. `resources/challenge_pools.json`

**动作**：按设计 §4.2 落盘。

**约束**：
- 3 个 biome，`floors` 闭区间 `[1,5]` / `[6,10]` / `[11,15]`（等价于旧 `floor<=5` / `<=10` / else）
- 各 biome 3 个 wave，id 列表与 `challenge_room.cpp:27-30` / `36-39` / `44-47` 逐字一致
- 无权重字段（保持均匀抽签语义）

**验证**：脚本比对 9 个池与 C++ 字面量逐个相等。

**回滚**：删除文件。

---

## 3. `src/data/spawn_tables.h`

**动作**：按设计 §5.1 落盘。`#pragma once` + `<cstdint>` / `<string>` / `<utility>` / `<vector>`。

**约束**：
- 不 `using namespace std`
- 返回 `const std::string*`，不返回 `const char*`（动态字符串生命周期）
- 函数签名见 §5.1，`load_*` 带默认路径参数
- 全局 registry 用 `extern` 声明，定义放 `.cpp`

**验证**：语法靠编译。

**回滚**：删除文件。

---

## 4. `src/data/spawn_tables.cpp`

**动作**：实现两个加载器 + 三个查询函数。

**约束**：
- 加载器用 `std::ifstream` + `nlohmann::json::parse`，异常 → `false` 并 `printf` 报错
- `pick_slot_monster` 的核心：**候选 ≤1 时绝不掷骰**（设计 §6 表；这是 RNG 逐位等价的关键，不得优化掉）
- 加权累计用 `uint64_t` 防溢出；`floor_end` 默认 15、`floor_start` 默认 1
- 函数 ≤40 行（CLAUDE.md 铁律）
- 不碰 `rng` 之外任何全局状态

**验证**：编译 + 单测。

**回滚**：删除文件。

---

## 5. 重接 `floor_manager.cpp`

**动作**：
- 顶部加 `#include "spawn_tables.h"`
- 把 `switch (i) { case 0..11 }` 整块（`:25-38`）替换为：

```cpp
            const std::string* id = pick_slot_monster(i, cfg.floor);
            return id ? id->c_str() : g_spawn_default.c_str();
```

**严禁改动**：`:14-18` 的 `w[12]` 拷贝、`total` 累加、`total <= 0` 兜底；`:19` 的 `rng() % total`；`:20-24` 的 `sum`/`roll` 遍历与 `if (w[i] == 0) continue`；`:41` 的末尾 `"slime"`。

**验证**：`grep -n "case " src/game/systems/floor_manager.cpp` 应无残留槽位 case；diff 应只剩 switch 块 → 两行。

**回滚**：`git checkout -- src/game/systems/floor_manager.cpp`。

---

## 6. 重接 `challenge_room.cpp`

**动作**：
- 顶部加 `#include "spawn_tables.h"`
- `_pick_monster_type` 保留 `static` 与签名 `(int floor, int wave, uint32_t rng)`
- 删除 `struct Pool`、lambda `pick`、三个 `if/else` 分支与 9 个池字面量（`:18-49`），替换为：

```cpp
    const std::string* id = pick_challenge_monster(floor, wave, rng);
    return id ? id->c_str() : "slime";
```

**严禁改动**：`:213` 调用点 `type_rng = wave_seed ^ (i*7+13)` 与 `:214` 调用（设计 §6.1 证明此处不用全局 `rng()`）。

**验证**：`grep -n "Pool\|pools\[" src/game/world/challenge_room.cpp` 应无残留。

**回滚**：`git checkout -- src/game/world/challenge_room.cpp`。

---

## 7. 接线 `main.cpp`

**动作**：
- 加 `#include "spawn_tables.h"`
- 在 `:307 load_biome_defs(...)` 之后（与 landmark/encounter 同组）追加：

```cpp
    load_spawn_slots("resources/enemy_slots.json");
    load_challenge_pools("resources/challenge_pools.json");
```

**验证**：构建通过；启动无异常。

**回滚**：`git checkout -- src/main.cpp`。

---

## 8. `tools/world_validator.py`

**动作**：新增 7 条检查（设计 §7.2），错误/警告/info 三级。

**实现约束**：
- 复用既有 `enemies.json` 加载，不新造解析路径
- 缺文件时跳过并记 info（不硬崩），避免开发中途误伤
- 反向可达性（规则 5）需展开别名后比对
- 规则 6 输出「仅经挑战房可达」清单为 info

**验证**：`conda run python tools/world_validator.py` → 0 error，info 段含规则 6 清单。

**回滚**：`git checkout -- tools/world_validator.py`。

---

## 9. `tests/spawn_tables_test.cpp`

**动作**：golden oracle 测试，6 个用例（设计 §7.1）。

**核心 oracle**：把 `floor_manager.cpp:25-38` 的 12 case **原样复制**成 `legacy_pick_slot(int i, int floor, uint32_t r)`（逐字比对已完成）。

**用例清单**：
1. `OracleMatchesNewPicker` — 15 层 × 2000 次外层抽签，重放外层 roll → 槽位 → 新旧 picker 逐条比对
2. `DrawCountMatches` — 比对 `rng.draws` 增量与 §6 表的期望（逐槽位按表统计）
3. `ChallengeOracleMatches` — 3 群系 × 3 波 × 固定 roll 序列，新 `pick_challenge_monster` vs 旧 9 池字面量
4. `LightningOrbFloorGate` — F6-10 槽位 6 能出 `lightning_orb`；F1-5 / F11-15 恒 `charger` 且 `draws` 不增
5. `MissingSlotFallsBack` — `get_spawn_slot(99)` 与 `pick_slot_monster(99, 5)` 返回 null
6. `LoadFailureReturnsFalse` — 不存在路径 → `false` 且 registry 为空

**前置**：测试需先 `load_spawn_slots` / `load_challenge_pools`（用 `resources/` 相对路径，需确认测试工作目录；若不可靠则用绝对路径或加工作目录修正）。播种用 `seed_rng(...)`。

**验证**：`ctest --test-dir build` 全绿。

**回滚**：删除文件 + 还原 CMake 测试注册（若需）。

---

## 10. 门禁

1. `cmake --build build --config Release -- -j 4` → 0 error
2. `ctest --test-dir build` → **68 + 新增 ≈74 全通过**
3. `conda run python tools/world_validator.py` → 0 error
4. 零回归断言（`git diff --name-only` 人工核对）：`enemies.json`、`floor_config.json`、`biomes.json`、`floor_config.cpp`、`floor_config.h`、`monster.cpp`、`spawn_monster` 及 5 处召唤字面量 **一律未改**
5. 静态接线：`grep -n '"elite"\|"charger"\|"slime"' src/game/systems/floor_manager.cpp src/game/world/challenge_room.cpp` 只剩兜底字面量
6. sim sha（`seed 3, 12 次`）：预期**逐字节不变**（挑战房在 RNG 流外、主刷怪流逐位等价）

---

## 11. CHANGELOG + 桌面同步

**动作**：
- `README.md` 追加批次9 条目（插在批次8 之后）：数据化范围、RNG 等价性结论、新增 6 测试、校验器 7 条新检查、遗留（候选项 D：`floor_config.json` 激活）
- 构建 exe → 桌面同步到 `C:\Users\HP\Desktop\Roguelike-CPP-3D版`：robocopy /MIR 8 目录（`src` `resources` `tools` `tests` `docs` `assets` `.github` `vendor`）+ 5 根文件 + exe/dll 到桌面包根
- 保留桌面 `saves\`、`3D模式.exe.lnk`；勿动 `Roguelike-CPP-初代版`

**验证**：桌面 exe 时间戳更新；`git status` 干净。

---

## 风险登记

| 风险 | 影响 | 缓解 |
|---|---|---|
| 槽位 6 在 F1-5/F11-15 误多掷 1 次 `rng()` | 整条流错位，行为静默改变 | 单候选短路 + `DrawCountMatches` 用例硬锁 |
| 候选顺序写反 | 该槽位 50% 概率翻面 | 已逐字核对 `floor_manager.cpp:26-37`；oracle 用例逐骰比对 |
| 测试工作目录找不到 `resources/` | 新测试挂掉 | 步骤 9 前置明确核查；必要时改绝对路径 |
| 加载失败静默全图刷史莱姆 | 游戏可玩性崩坏 | 加载器 `printf` 报错 + validator 规则 1-4 在开发期抓 |
| validator 规则 5 误报 `elite` 别名 | 假 error 阻塞 | `aliases` 显式声明并展开 |
