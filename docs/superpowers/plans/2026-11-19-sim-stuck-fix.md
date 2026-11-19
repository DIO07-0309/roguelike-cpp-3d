# sim AI 卡死修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 sim AI 在清怪尾段主动追远处满血怪，把 84.7% 的 STUCK_RECOVERED 卡死率降下来。

**Architecture:** 新增纯值输入的 `CombatFocusTracker`（单一职责：判断"长时间无怪掉血→该纯追怪"），DecisionAgent 组合它。每帧 observe(怪HP总和, 存活数, game_time)，僵持 ≥8s 时 `hunting()==true`，在 `_eval_move_loot_and_rooms` 开头门控跳过 loot/room 评分，让 approach 驱动 AI 持续追怪；怪掉血即自动退出。不动卡死判定/平衡参数，保留 120s 看门狗兜底。

**Tech Stack:** C++17 + gtest；sim CLI (`--sim`)；golden baseline 回归；Windows。

**Spec:** `docs/superpowers/specs/2026-11-19-sim-stuck-fix-design.md`（含 builder 审查约束）

## Global Constraints

- CLAUDE.md 全部规范：函数≤40行 / 单一职责 / 组合优于继承 / 语义命名 / 改前 Grep 影响 / 改后 Code Review / 单次代码≤300行 / 可维护性优先 / Milestone 更 README（本轮用 CHANGELOG 记载）。
- **game_time 换层归零**（game_scene.cpp:395）：tracker 必须检测 `t < _last_time` 回退并重置内部计时（最高危）。
- **门控只在 `_eval_move_loot_and_rooms` 开头**（sim_ai.cpp:766），绝不上移——否则误禁 recovery/dodge/heal。
- HP 口径必须与 `_stuck_progress`（sim_ai.cpp:1014-1017）一致：只统计 `is_alive` 怪的 `current_hp`。
- observe 钩子在 `is_action_just_pressed` 的 `_cached_frame != _frame` 分支内（sim_ai.cpp:1501），每帧恰好一次，读上一帧 game_time（单帧滞后，单调）。
- tracker 状态在 `start()`/构造清零，不依赖 `_cached_frame`。
- 2D 不同步（已冻结）。
- Golden baseline 行为改动后必碎，须重采集 + commit message 说明玩法为何变；确定性仍须通过。
- 验证：ctest 全绿 / fn_length 0 / world_validator / 卡死率下降 / 确定性稳定 / 桌面包同步（exe 到根目录，禁碰初代版）。

## Review Focus

- 换层 game_time 归零致 hunting 永久失效 → Task 1 契约测试"时间回退后重新计时"。
- 门控上移误禁生存分支（残血送死）→ Task 2 Code Review 核查门控位置 + 集成断言。
- Golden baseline 碎裂未重采集 → Task 3 重采集 + commit message。
- "延迟结算"假修复（卡死率没真降）→ Task 3 观察 sim_progress_src/sim_tp_attempts + 300 局对比。
- tracker 与看门狗 HP 口径不一致互相打架 → Task 1 口径对齐测试。

---

### Task 1: CombatFocusTracker 纯值类 + 契约测试

**Files:**
- Create: `src/core/sim/combat_focus_tracker.h`（纯 C++，无 namespace，仿 sim_ai_teleport.h，#pragma once）
- Test: `tests/sim/combat_focus_tracker_test.cpp`

**Interfaces:**
- Consumes: 三个纯数值（怪HP总和 float、存活怪数 int、game_time float）。
- Produces: 类 `CombatFocusTracker` 接口 `void observe(float hp_sum, int alive_count, float game_time)` / `bool hunting() const` / `void reset()`；类内 `static constexpr float kHuntThresholdSec = 8.0f;`（测试可引用该常量做边界断言）。

- [ ] **Step 1: 写失败测试**

`tests/sim/combat_focus_tracker_test.cpp`（独立轻量 gtest，仿 p0_teleport_test.cpp）：
- `HuntingAfterNoDamage`：observe(100,3,0) 起，每秒 observe 同值，7s 时 `!hunting()`、9s 时 `hunting()`。
- `ResetsOnDamage`：9s 进入 hunting 后 observe(90,3,9)（HP 降）→ `!hunting()`。
- `ResetsOnMonsterDeath`：observe(100,2,9)（存活数降）→ `!hunting()`。
- `ResetsOnTimeRollback`：observe(100,3,50) 后 observe(100,3,0)（换层归零）→ 内部计时重置，需再满 8s 才 hunting。
- `NoHuntWhenNoMonsters`：observe(0,0,t) 任意时长 → 从不 hunting。
- `ResetClearsState`：hunting 后 reset() → `!hunting()` 且重新计时。

- [ ] **Step 2: 验证 RED**

Run: `cmake --build build --target combat_focus_tracker_test && cd build && ctest -R combat_focus_tracker_test --output-on-failure`
Expected: FAIL（类不存在，编译失败）。

- [ ] **Step 3: 实现 CombatFocusTracker**

`src/core/sim/combat_focus_tracker.h` 单文件 header-only（方法与状态内联）：
- 私有：`float _last_hp_sum=-1, _last_time=-1; int _last_alive=-1; float _no_combat_since=-1; bool _hunting=false;`
- `observe`（≤20行）：若 `game_time < _last_time` → reset()（换层）；若 hp_sum 下降或 alive 下降 → `_no_combat_since=game_time, _hunting=false`；否则若 alive>0 且 `_no_combat_since<0` → 锚定；若 `game_time-_no_combat_since>=8` → `_hunting=true`；更新 `_last_*`。
- `hunting()`：return `_hunting`。`reset()`：全部恢复初值。
- 注意 HP 口径：alive==0 时 hp_sum 视为 0（与 sim_ai.cpp:1014-1017 一致，只累加存活怪）。

- [ ] **Step 4: 注册测试 + 验证 GREEN**

`tests/CMakeLists.txt` 加 `add_roguelike_test(combat_focus_tracker_test sim/combat_focus_tracker_test.cpp)`（在 p0_teleport_test 附近）。
Run: `cmake --build build && ctest --test-dir build -R combat_focus_tracker --output-on-failure`
Expected: 全 PASS（6 测试）。

- [ ] **Step 5: 提交** `test(sim): add CombatFocusTracker pure-value contract tests`

---

### Task 2: DecisionAgent 组合 tracker + 门控接线

**Files:**
- Modify: `src/core/sim/sim_ai.h`（成员 + include + start 清零）
- Modify: `src/core/sim/sim_ai.cpp`（observe 钩子 + 门控）
- Test: （接线真伪由 Task 3 的 300 局卡死率验证，本任务不新增测试文件）

**Interfaces:**
- Consumes: Task 1 的 `CombatFocusTracker`（`src/core/sim/combat_focus_tracker.h`）。
- Produces: `DecisionAgent::_combat_focus` 成员；`_eval_move_loot_and_rooms` 在 hunting 时返回 nullopt。

- [ ] **Step 1: 实现接线**

（Task 1 的契约测试已覆盖 tracker 接口；接线真伪靠 Task 3 的卡死率变化验证，不写冗余的包装函数测试。）

sim_ai.h：`#include "core/sim/combat_focus_tracker.h"`；私有成员 `CombatFocusTracker _combat_focus;`
（放在 195-219 状态区附近，语义命名）。
sim_ai.cpp：
- observe 钩子：在 `is_action_just_pressed` 的 `if (_cached_frame != _frame)` 分支内（:1501），
  `best_action` 调用前，计算 `float hp_sum=0; int alive=0;`（只统计 is_alive 怪 current_hp，口径同 :1014-1017），
  调 `_combat_focus.observe(hp_sum, alive, (float)_game_time)`。
- 门控：`_eval_move_loot_and_rooms`（:766）开头加 `if (_combat_focus.hunting()) return std::nullopt;`
  （**仅此一处**，绝不上移）。
- `start()`（:140 附近）：`_combat_focus.reset()`（跨局清零）。

- [ ] **Step 2: 验证 GREEN**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 除 golden_baseline_test 外全绿（golden 此时会 FAIL——行为改动，Task 3 重采集）；
CombatFocusTracker 6 测试 PASS、fn_length 0。

- [ ] **Step 3: 提交** `feat(sim): combat focus tracker gates loot when chasing stalls`

---

### Task 3: Golden baseline 重采集 + 卡死率/确定性验证

**Files:**
- Modify: `tests/sim/golden_baseline_test.cpp`（base_floor/kills/dmg/weapon 数组 + 注释说明玩法变化）

**Interfaces:**
- Consumes: Task 2 接线后的行为。
- Produces: 新 golden 基线（卡死修复后的固定 seed 结果）。

- [ ] **Step 1: 跑 300 局对比卡死率**

`build\roguelike_cpp.exe --sim 300 --sim-seed 1234`，读 reports/balance_report.json 的 summary.outcome_dist，
断言 STUCK_RECOVERED 显著下降（对比基线 254/300）。同时观察 game.log 的 `sim_progress_src`/`sim_tp_attempts`，
确认是"真修复"（追怪致死）而非"延迟结算"。

- [ ] **Step 2: 重采集 golden 基线**

`roguelike_cpp.exe --sim 4 --sim-seed 4242 --sim-build none`，读 4 局的 floor/kills/damage_dealt/weapon，
更新 golden_baseline_test.cpp:77-81 的数组 + 注释（写明"P6 清怪模式：僵持时禁 loot/room 强制追怪，玩法有意改变"）。
再跑 GOLDEN-003 确认同 seed 确定性仍通过（新行为稳定）。

- [ ] **Step 3: 全量回归**

ctest 全绿、fn_length 0、world_validator、P3/P3b/P4/P5 闸门不退化。

- [ ] **Step 4: 提交** `test(sim): rebaseline golden after combat-focus fix (stuck 84%->lower)`

---

### Task 4: CHANGELOG + 桌面同步 + 最终审查

**Files:** Modify `CHANGELOG.md`（P6 条目）

- [ ] **Step 1: 写 CHANGELOG**（根因四轮诊断摘要、CombatFocusTracker 方案、卡死率前后、golden 重采集说明）

- [ ] **Step 2: 同步桌面包**：src/ tools/ tests/ docs/ assets/ + 根文件镜像到 `C:\Users\HP\Desktop\Roguelike-CPP-3D版`，
  **exe 复制到根目录**，哈希校验（CLAUDE.md 规范11）。禁碰初代版。

- [ ] **Step 3: 提交并推送** `docs+feat: sim AI stuck fix complete`；`git push origin master`。

- [ ] **Step 4: 最终整支审查**（fresh reviewer，Range = 本 plan base..HEAD）

## Self-Review

- Spec coverage：§1 根因 → Task 1/2/3；§2 方案 → Task 1（tracker）/2（接线门控）/3（验证）；§4 约束 → Global Constraints + 各 Task 测试；builder 8 条约束 → Task 1（时间回退/HP口径/清零）/2（门控位置/observe时序）/3（golden/真修复）。
- Review Focus：5 条均有对应任务测试（时间回退契约 / 门控 Code Review / golden 重采集 / 真修复观测量 / HP 口径对齐）。
- Type consistency：`CombatFocusTracker` / `observe(hp_sum,alive_count,game_time)` / `hunting()` / `reset()` / `kHuntThresholdSec` 贯穿一致。
- Proportion：计划以契约和接线点为主，无大段代码；实现细节由 builder 级决策。
