# B4 · 挑战房隐藏 Boss（远古魔像 GOLEM）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) 或 superpowers:executing-plans 逐任务执行。步骤用 checkbox（`- [ ]`）跟踪。

**Goal:** 让 GOLEM 成为挑战房的 25% 隐藏压轴 Boss——补上 `bosses.json` 的 `golem` def、挑战房第 4 波刷出路径、美术资产，并修复顺带发现的 boss 奖励无楼层保护漏洞。

**Architecture:** GOLEM 的 C++ 行为早已建成（DEFEND 盾、Phase2 三连震、`BossType::GOLEM`→`"golem"` 映射、视觉色），只缺数据 def 与刷出路径。压轴波复用挑战房既有 3 秒波间等待当「boss 登场前奏」，判定走 `dungeon_seed` 派生的保留波次槽位（不消耗全局 rng）。刷出沿用 `boss_factory_create`，`is_boss` 保持 true 以拿到 director 行为，同时用 `is_boss_floor` 守卫隔离主线奖励。

**Tech Stack:** C++17 + Raylib 5.0 + CMake 3.16+ / MinGW；nlohmann::json；Python（conda）生成美术；GoogleTest。

**设计文档:** `docs/superpowers/specs/2026-09-25-b4-golem-hidden-boss-design.md`（含 §2.7 归属与奖励隔离的全部依据）

---

## Global Constraints

- 函数 ≤40 行；类只做一件事；组合优于继承；变量命名语义化
- **单次生成 ≤300 行代码**
- PascalCase 类 / camelCase 方法 / snake_case 变量；`.h` 必须 `#pragma once` 且禁 `using namespace std`；JSON 字段全 snake_case
- **不改 CMakeLists.txt 编译器标志**；不新增第三方库；不新增裸 `new`（factory 内部既有 `new` 不动）
- **不消耗全局 `rng`** 于刷怪/判定路径（批次9 红线）
- 新增 `.cpp` 靠 `GLOB` 纳入 → **构建前必须重跑 `cmake -B build`**
- 构建：`cmake --build build --config Release -- -j 4`（**勿用 `/m`**）
- 测试：`ctest --test-dir build`
- 校验：`& "C:\Users\HP\anaconda3\python.exe" tools\world_validator.py`（**禁多行 `-c`**；本环境 `conda run` 不可用、`C:\Users\HP\miniconda3` 不存在，实测 `anaconda3\python.exe` 为 3.13.5 可直接跑）
- PS 5.1 命令内**禁中文字面量**；构建前 `$env:PATH` 前置 MSVC/WinSDK/Anaconda/Python/Rust
- 写文件后必读回校验；跑 sim 前 `[Console]::OutputEncoding = [System.Text.Encoding]::UTF8`
- 门禁全绿 → 同步桌面 `C:\Users\HP\Desktop\Roguelike-CPP-3D版`（exe 到包根）

### 关键既有事实（执行前勿重复调研）

| 事实 | 位置 |
|---|---|
| JSON `skills` 加载到 `def.skill_overrides`（不是 `def.skills`） | `boss_defs.cpp:79-82` |
| `BossSkillDef.id` 合法集：`charge`/`shockwave`/`summon`/`barrage`/`cone`/`blink` | `boss_defs.h:14-26` |
| `ComboDef.commands` 合法集：`normal`/`charge`/`shockwave`/`summon`/`defend`/`barrage`/`cone`/`blink`/`whirlwind` | `boss_defs.h:47` |
| DEFEND 覆写：`golem_shield_pct>0 && sk==1 && (skill_cycle_index%12)<6 → sk=3`。`sk` 来自 `_next_cycle_skill()`（`boss.cpp:754`）返回的**固定技能槽**（`-1`普攻/0 Charge/1 Shockwave/2 Summon/3 DEFEND/4 Whirlwind/5 Barrage/6 GravityPull），**不是 JSON `skills` 数组下标** | `boss.cpp:787-788, 425-435` |
| **决定召唤的唯一开关是 `skill_cycle_bias`**：`_next_cycle_skill` 只在 `cycle_len==4`（idx3）或 `==6`（idx4）时返回 Summon；其余值绝不召唤。填 6 的 GOLEM 会周期性召唤小怪 | `boss.cpp:425-435` |
| `is_summoner` **只用于显示串**，完全不 gating 召唤——不可当「不召唤」依据 | `boss.cpp:1145,1278` |
| JSON `skills` 由 **id-match** 覆盖技能参数，与数组位置无关 | `boss.cpp:1240-1257` |
| `boss_factory_create` 内部已乘 `boss_hp_scale`/`boss_atk_scale` | `boss.cpp:1200-1201` |
| factory 的 `out_monsters`/`map` 参数 `(void)` 未使用 | `boss.cpp:1285` |
| `boss_factory_create` 取 **tile** 坐标（内部 `*TILE_SIZE`） | `boss.cpp:1203` |
| `get_boss_def_for_floor` 是硬编码 switch，不扫描 JSON `floor` | `boss_defs.cpp:170-176` |
| `get_boss_def_for_type(4)` → `"golem"` | `boss_defs.cpp:185` |
| `is_boss_floor(f)` = `f==5 || f==10 || f==15` | `config.cpp:3-5` |
| `reset_floor()` 清 `_arena_cfg`，经 `FLOOR_ENTER` 每层自动调用 | `boss_system_director.cpp:53,617-618` |
| `Rarity`：COMMON 0 / RARE 1 / EPIC 2 / LEGENDARY 3 | `Rarity` 枚举 |
| `generate_random_item()` 返回 `shared_ptr<Item>`，消耗全局 `rng` | `item.h:95`, `item.cpp:188` |
| 挑战房 234 行单文件；`_deterministic_seed(uint32_t,int,int)`；`_total_waves=3` | `challenge_room.h:81,94` |
| `config.h` 已被 `game_scene_combat.cpp` 包含 | `game_scene_combat.cpp:19` |

---

### Task 1: 数据层——`golem` boss def + 加载器契约测试

**Files:**
- Modify: `resources/bosses.json`
- Create: `tests/boss/boss_defs_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `load_boss_defs(path)` / `get_boss_def(id)` / `get_boss_def_for_type(int)` / `get_boss_def_for_floor(int)`（`boss_defs.h:114-119`）
- Produces: `get_boss_def("golem")` 非空且满足 defender 契约——后续 Task 3 的 `boss_factory_create(BossType::GOLEM, …)` 依赖它

- [ ] **Step 1: 写失败的契约测试**

`tests/boss/boss_defs_test.cpp`：

```cpp
#include <algorithm>
#include <gtest/gtest.h>
#include "boss_defs.h"

namespace {
class BossDefsTest : public ::testing::Test {
protected:
    void SetUp() override { load_boss_defs("resources/bosses.json"); }
};

bool has_summon_skill(const BossDef* g) {
    for (const auto& s : g->skill_overrides) if (s.id == "summon") return true;
    return false;
}

TEST_F(BossDefsTest, GolemDefLoads) {
    const BossDef* g = get_boss_def("golem");
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->id, "golem");
    EXPECT_EQ(g->visual_id, "golem");
}

TEST_F(BossDefsTest, GolemIsDefenderWithActiveShield) {
    const BossDef* g = get_boss_def("golem");
    ASSERT_NE(g, nullptr);
    EXPECT_TRUE(g->is_defender);          // 触发 golem_shield_pct 赋值 (boss.cpp:1281)
    EXPECT_GT(g->shield_pct, 0.0f);       // boss.cpp:787 要求 golem_shield_pct > 0
    EXPECT_FALSE(g->is_summoner);         // 仅显示串 (boss.cpp:1145,1278), 不 gating 召唤
    EXPECT_TRUE(g->skill_overrides.size() == 3u);
}

TEST_F(BossDefsTest, GolemHasChargeAndShockwaveSkillEntries) {
    // 技能按 id-match 覆盖 ai->_charge / ai->_shockwave (boss.cpp:1240-1257),
    // 与数组位置无关; 缺一条则该技能参数走编译期默认。
    const BossDef* g = get_boss_def("golem");
    ASSERT_NE(g, nullptr);
    auto has = [&](const std::string& id) {
        for (const auto& s : g->skill_overrides) if (s.id == id) return true;
        return false;
    };
    EXPECT_TRUE(has("charge"));
    EXPECT_TRUE(has("shockwave"));
    EXPECT_TRUE(has("barrage"));
}

// 真正决定是否召唤的不是技能表, 而是 skill_cycle_bias:
// _next_cycle_skill (boss.cpp:425-435) 只在 cycle_len==4 或 ==6 时返回 Summon。
// is_summoner 只用于显示串 (boss.cpp:1145,1278), 完全不 gating 召唤。
TEST_F(BossDefsTest, GolemCycleBiasNeverSummons) {
    const BossDef* g = get_boss_def("golem");
    ASSERT_NE(g, nullptr);
    EXPECT_NE(g->skill_cycle_bias, 4);   // idx3 -> Summon
    EXPECT_NE(g->skill_cycle_bias, 6);   // idx4 -> Summon
    EXPECT_FALSE(has_summon_skill(g));   // 双保险
}

TEST_F(BossDefsTest, GolemComboCommandsAreLegal) {
    const BossDef* g = get_boss_def("golem");
    ASSERT_NE(g, nullptr);
    ASSERT_FALSE(g->combos.empty());
    const std::vector<std::string> ok = {"normal", "charge", "shockwave", "summon",
                                         "defend", "barrage", "cone", "blink", "whirlwind"};
    for (const auto& c : g->combos)
        for (const auto& cmd : c.commands) {
            EXPECT_NE(cmd, "") << "empty command in combo " << c.id;
            EXPECT_TRUE(std::find(ok.begin(), ok.end(), cmd) != ok.end())
                << "illegal combo command: " << cmd;
        }
}

// BossType::GOLEM(4) 映射不得断链
TEST_F(BossDefsTest, BossTypeGolemResolves) {
    const BossDef* d = get_boss_def_for_type(4);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->id, "golem");
}

// 反回归: golem 不得顶掉 F10 主线 boss (get_boss_def_for_floor 硬编码 fire_demon)
TEST_F(BossDefsTest, FloorTenStillMapsToFireDemon) {
    const BossDef* d = get_boss_def_for_floor(10);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->id, "fire_demon");
}
}  // namespace
```

- [ ] **Step 2: 注册测试**

`tests/CMakeLists.txt` 的 `# ── Test targets ──` 段追加一行（照邻近格式）：

```cmake
add_roguelike_test(boss_defs_test   boss/boss_defs_test.cpp)
```

- [ ] **Step 3: 跑测试确认失败**

```powershell
cmake -B build
cmake --build build --config Release -- -j 4
ctest --test-dir build -R boss_defs_test --output-on-failure
```

预期：`GolemDefLoads` 等 6 例 FAIL（`get_boss_def("golem")` 返回 null）。

- [ ] **Step 4: 追加 `golem` def**

`resources/bosses.json` 数组/对象末尾追加（字段名与既有 5 条完全同构）：

```json
  {
    "id": "golem",
    "name": "远古魔像",
    "title": "挑战房秘藏·远古魔像",
    "lore": "远古时代的攻城兵器，被遗忘在地牢深处。\n三波小怪之后，它会亲自站起来迎接你。",
    "visual_id": "golem",
    "floor": 10,
    "is_summoner": false,
    "is_defender": true,
    "hp": 200, "atk": 13, "pdef": 14, "mdef": 8,
    "phase2_hp_threshold": 0.50,
    "phase2_pause": 0.50,
    "phase2_speed_mult": 1.30,
    "phase2_atk_mult": 1.20,
    "phase2_cd_mult": 0.80,
    "shield_pct": 0.50,
    "summon_speed": 1.0,
    "skill_cycle_bias": 5,
    "skills": [
      {"id": "charge",   "cooldown": 6.0, "damage_mult": 2.5, "windup": 0.6, "range": 120},
      {"id": "shockwave","cooldown": 8.0, "damage_mult": 1.6, "windup": 0.7, "range": 100},
      {"id": "barrage",  "cooldown": 7.0, "damage_mult": 0.35, "windup": 0.5, "range": 40}
    ],
    "combos": [
      {"id": "probe", "commands": ["shockwave","normal","defend","normal"], "interval": 0.6, "end_delay": 1.0},
      {"id": "rage",  "commands": ["charge","shockwave","barrage","shockwave","normal","charge"], "interval": 0.5, "end_delay": 1.2}
    ],
    "arena": {"danger_type": "shadow_wall", "spawn_interval": 10.0, "max_zones": 3, "zone_duration": 3.0, "spawn_radius": 120}
  }
```

要点：
- **`skill_cycle_bias: 5`（关键）**——`_next_cycle_skill`（`boss.cpp:425-435`）只在 `cycle_len==4` 或 `==6` 时返回 Summon。填 6 会让魔像周期性召唤小怪，与 spec 的纯 tank 定位冲突，且召唤物会污染挑战房存活计数。取 3/5/7 均可（idx2 仍给 Shockwave），此处取 5 = `Charge, 普攻, Shockwave, 普攻, 普攻`。
- `skills` 必须**包含** `charge` 与 `shockwave` 两条——`boss.cpp:1240-1257` 按 **id-match** 覆盖 `ai->_charge` / `ai->_shockwave` 参数，与数组位置无关。
- `is_defender: true` + `shield_pct > 0`：`:1281` 据此赋 `golem_shield_pct`，`:787` 要求 `> 0` 才启用 DEFEND 覆写。
- `is_summoner: false` 仅影响显示串（`boss.cpp:1145,1278`），**不 gating 召唤**；真正的召唤开关是上一条的 `skill_cycle_bias`。
- `arena` 填合法值（本路径不生成 zone，仅为 validator 与 UI 兜底）。

- [ ] **Step 5: 跑测试确认通过**

```powershell
ctest --test-dir build -R boss_defs_test --output-on-failure
```

预期：6 例全 PASS。

- [ ] **Step 6: 跑 validator 基线**

```powershell
& "C:\Users\HP\anaconda3\python.exe" tools\world_validator.py
```

预期：0 error / 0 warning（本任务尚未加 boss 自检，仅确认新 def 不破坏既有校验）。

- [ ] **Step 7: 提交**

```bash
git add resources/bosses.json tests/boss/boss_defs_test.cpp tests/CMakeLists.txt
git commit -m "feat(b4): golem boss def + 加载器契约测试

- resources/bosses.json 追加 golem (defender, hp200/atk13/pdef14/mdef8, shield_pct 0.50)
- skill_cycle_bias=5: 避开 Summon 槽 (cycle_len==4/6 才会召唤, is_summoner 不 gating)
- skills 按 id-match 覆盖 _charge/_shockwave, 须含 charge+shockwave 两条, 与顺序无关
- arena 填 shadow_wall 合法值但压轴路径不生成 zone (reset_floor 已清 _arena_cfg)
- 新测试 6 例含反回归: get_boss_def_for_floor(10) 仍返回 fire_demon"
```

---

### Task 2: 触发层——压轴判定 `has_boss_wave()`

**Files:**
- Modify: `src/game/world/challenge_room.h:78-95`
- Modify: `src/game/world/challenge_room.cpp:42-50`（`reset()`）、`:25-35` 之后
- Modify: `tests/economy/challenge_room_test.cpp`

**Interfaces:**
- Consumes: `_deterministic_seed(uint32_t dungeon_seed, int room_index, int wave_index) const`（`challenge_room.cpp:26-35`，avalanche hash_combine）
- Produces: `bool ChallengeRoomController::has_boss_wave(uint32_t, int) const`——Task 3 的 COMBAT 分支调用它；`_boss_wave_pending` 成员——Task 5 的 `_grant_rewards` 读它

- [ ] **Step 1: 写失败的确定性测试**

`tests/economy/challenge_room_test.cpp` 末尾追加：

```cpp
// --- B4: 压轴波判定 ---

TEST(ChallengeRoomTest, BossWaveIsDeterministic) {
    ChallengeRoomController c;
    bool first = c.has_boss_wave(0xDEADBEEFu, 7);
    for (int i = 0; i < 100; i++)
        EXPECT_EQ(c.has_boss_wave(0xDEADBEEFu, 7), first);
}

TEST(ChallengeRoomTest, BossWaveVariesAcrossRooms) {
    ChallengeRoomController c;
    int hits = 0;
    for (int room = 0; room < 200; room++)
        if (c.has_boss_wave(0x12345678u, room)) hits++;
    // 25% 期望: 200 房约 50 命中, 放宽到 [20, 80] 容统计波动
    EXPECT_GT(hits, 20);
    EXPECT_LT(hits, 80);
}

TEST(ChallengeRoomTest, BossWaveVariesAcrossSeeds) {
    ChallengeRoomController c;
    int hits = 0;
    for (uint32_t s = 0; s < 200; s++)
        if (c.has_boss_wave(s, 3)) hits++;
    EXPECT_GT(hits, 20);
    EXPECT_LT(hits, 80);
}

// 不碰全局 rng: 判定前后全局流位应一致 (靠重复调用同一入参不改变结果间接验证)
TEST(ChallengeRoomTest, BossWaveResultStableRegardlessOfReset) {
    ChallengeRoomController a, b;
    a.reset();
    b.reset();
    for (int room = 0; room < 50; room++)
        EXPECT_EQ(a.has_boss_wave(99u, room), b.has_boss_wave(99u, room));
}
```

- [ ] **Step 2: 跑测试确认失败**

```powershell
cmake -B build
cmake --build build --config Release -- -j 4
ctest --test-dir build -R ChallengeRoomTest --output-on-failure
```

预期：编译失败（`has_boss_wave` 未定义）。

- [ ] **Step 3: 加成员与方法声明**

`src/game/world/challenge_room.h` private 段（`:78-95`）追加两个成员与一个方法声明：

```cpp
    bool _boss_wave_decided = false;
    bool _boss_wave_pending = false;
```

以及 private 方法（放在 `_spawn_wave` 声明附近）：

```cpp
    bool has_boss_wave(uint32_t dungeon_seed, int room_index) const;
```

- [ ] **Step 4: 实现判定**

`challenge_room.cpp`，在 `_deterministic_seed` 定义（`:35`）之后追加：

```cpp
namespace {
// 保留波次槽位: 真实波用 0..3, 99 与其无碰撞且经 avalanche 后不同域
constexpr int kBossWaveSlot = 99;
constexpr int kBossWaveChancePct = 25;
}

bool ChallengeRoomController::has_boss_wave(uint32_t dungeon_seed,
                                            int room_index) const {
    uint32_t s = _deterministic_seed(dungeon_seed, room_index, kBossWaveSlot);
    return (int)(s % 100u) < kBossWaveChancePct;
}
```

- [ ] **Step 5: `reset()` 复位两个成员**

`challenge_room.cpp:42-50` 的 `reset()` 内追加：

```cpp
    _boss_wave_decided = false;
    _boss_wave_pending = false;
```

- [ ] **Step 6: 跑测试确认通过**

```powershell
cmake --build build --config Release -- -j 4
ctest --test-dir build -R ChallengeRoomTest --output-on-failure
```

预期：全 PASS（含既有全部用例）。

- [ ] **Step 7: 提交**

```bash
git add src/game/world/challenge_room.h src/game/world/challenge_room.cpp tests/economy/challenge_room_test.cpp
git commit -m "feat(b4): 挑战房压轴判定 has_boss_wave

- 走 _deterministic_seed(dungeon_seed, room_index, kBossWaveSlot=99), 不消耗全局 rng
- 保留波次槽位 99 与真实波 0..3 经 avalanche 后不同域
- 25% 概率, 整场只判定一次 (_boss_wave_decided/_boss_wave_pending, reset() 复位)
- 4 个确定性用例: 同入参稳定 / 跨房分布 / 跨 seed 分布 / reset 后一致"
```

---

### Task 3: 刷出层——压轴波 boss 生成

**Files:**
- Modify: `src/game/world/challenge_room.h`（加 `WaveAdvance` 枚举、`decide_advance` 静态纯函数、`_spawn_boss_wave` 声明）
- Modify: `src/game/world/challenge_room.cpp`（实现 `decide_advance`、`:134-147` COMBAT 分支、`:160` `_spawn_wave` 分流、新增 `_spawn_boss_wave`）
- Modify: `tests/economy/challenge_room_test.cpp`

**Interfaces:**
- Consumes: `has_boss_wave(uint32_t,int) const`（Task 2）；`boss_factory_create(BossType, int tile_x, int tile_y, int floor)`（`boss.h:317`，后两参有默认值）
- Produces:
  - `enum class WaveAdvance { WAIT, BOSS_WAIT, REWARD }`（`challenge_room.h`，`ChallengePhase` 附近）
  - `static WaveAdvance ChallengeRoomController::decide_advance(int wave_after_increment, int total_waves, bool boss_pending)` —— 纯函数，无成员访问，全真值表可测
  - `_boss_wave_pending` 在 `_grant_rewards` 调用时反映本场是否有压轴——Task 5 依赖

- [ ] **Step 1: 写失败的波次推进真值表测试**

`tests/economy/challenge_room_test.cpp` 追加。波次分支抽成纯函数 `decide_advance` 后，整条追踪可无副作用地全量断言——不需要构造 `GameMap`/`Player`/`Monster`：

```cpp
// --- B4: 波次推进决策 (纯函数真值表) ---

TEST(ChallengeRoomTest, WaveAdvanceTraceNoBoss) {
    EXPECT_EQ(ChallengeRoomController::decide_advance(1, 3, false), WaveAdvance::WAIT);
    EXPECT_EQ(ChallengeRoomController::decide_advance(2, 3, false), WaveAdvance::WAIT);
    EXPECT_EQ(ChallengeRoomController::decide_advance(3, 3, false), WaveAdvance::REWARD);
}

TEST(ChallengeRoomTest, WaveAdvanceTraceWithBoss) {
    EXPECT_EQ(ChallengeRoomController::decide_advance(1, 3, true), WaveAdvance::WAIT);
    EXPECT_EQ(ChallengeRoomController::decide_advance(2, 3, true), WaveAdvance::WAIT);
    EXPECT_EQ(ChallengeRoomController::decide_advance(3, 3, true), WaveAdvance::BOSS_WAIT);
    // boss 波清完后 current=4: 越过 total, 必须回 REWARD 而非再次 BOSS_WAIT
    EXPECT_EQ(ChallengeRoomController::decide_advance(4, 3, true), WaveAdvance::REWARD);
}

TEST(ChallengeRoomTest, WaveAdvanceBossFlagCannotTriggerBelowTotal) {
    EXPECT_EQ(ChallengeRoomController::decide_advance(0, 3, true), WaveAdvance::WAIT);
    EXPECT_EQ(ChallengeRoomController::decide_advance(1, 3, true), WaveAdvance::WAIT);
    EXPECT_EQ(ChallengeRoomController::decide_advance(2, 3, true), WaveAdvance::WAIT);
}

TEST(ChallengeRoomTest, WaveAdvanceTotalWavesUnchanged) {
    ChallengeRoomController c;
    c.reset();
    EXPECT_EQ(c.total_waves(), 3);   // 压轴占用波次索引 3, 不改 _total_waves
    EXPECT_FALSE(c.boss_wave_pending());
    EXPECT_FALSE(c.boss_wave_decided());
}
```

> **执行者注意：** `tick()` 的 COMBAT 全灭分支需要 `GameMap*`/`Player*`/`monsters` 三个非空实参，且依赖 `load_challenge_pools` / `load_boss_defs` 先加载才能安全跑通；本批**不把该链路塞进单测**，改由 `decide_advance` 纯函数覆盖分支语义 + Step 5 的 `LOG_INFO` 实机日志覆盖真实时序。`has_boss_wave` 的确定性已由 Task 2 覆盖。两者合起来即本批 RNG 红线与波次追踪的完整覆盖。

- [ ] **Step 2: 加 `WaveAdvance` 枚举、`decide_advance` 与只读 getter**

`challenge_room.h`，在 `ChallengePhase` 枚举之后：

```cpp
// 波次全灭后的下一步 (纯函数返回值, 便于全真值表测试)
enum class WaveAdvance { WAIT, BOSS_WAIT, REWARD };
```

`challenge_room.h` public 段（紧邻 `total_waves()` 之后）：

```cpp
    bool boss_wave_pending() const { return _boss_wave_pending; }
    bool boss_wave_decided() const { return _boss_wave_decided; }
    static WaveAdvance decide_advance(int wave_after_increment, int total_waves,
                                      bool boss_pending);
```

`challenge_room.h` private 段追加声明：

```cpp
    void _spawn_boss_wave(GameMap* map,
                          std::vector<std::unique_ptr<Monster>>& monsters,
                          int floor);
```

`challenge_room.cpp` 顶部确认已包含 boss 工厂头（若缺则加）：

```cpp
#include "boss.h"
```

`challenge_room.cpp`，放在 `has_boss_wave` 实现之后：

```cpp
WaveAdvance ChallengeRoomController::decide_advance(
    int wave, int total, bool boss_pending) {
    if (wave == total && boss_pending) return WaveAdvance::BOSS_WAIT;
    if (wave >= total) return WaveAdvance::REWARD;
    return WaveAdvance::WAIT;
}
```

> `BOSS_WAIT` 与 `WAIT` 在调用处**当前处理相同**（都是 3 秒 → `WAIT_NEXT_WAVE`）：前者是压轴登场前奏，后者是普通波间等待。这个区分是追踪契约，测试靠它证明波 2 清→boss、boss 清→REWARD 不互相串。

- [ ] **Step 3: 改 COMBAT 全灭分支**

`challenge_room.cpp:134-147`，把

```cpp
        if (alive <= 0) {
            _current_wave++;
            if (_current_wave >= _total_waves) {
                _phase = ChallengePhase::REWARD;
```

改为（保留原有 REWARD 块内的 `_grant_rewards` / return portal / LOG_INFO 语句不动）：

```cpp
        if (alive <= 0) {
            _current_wave++;
            if (_current_wave == _total_waves && !_boss_wave_decided) {
                _boss_wave_decided = true;
                _boss_wave_pending = has_boss_wave(dungeon_seed, room_index);
                LOG_INFO("[CHALLENGE] Boss wave roll: %s",
                         _boss_wave_pending ? "HIT" : "miss");
            }
            WaveAdvance adv =
                decide_advance(_current_wave, _total_waves, _boss_wave_pending);
            if (adv == WaveAdvance::WAIT || adv == WaveAdvance::BOSS_WAIT) {
                _wave_timer = 3.0f;
                _phase = ChallengePhase::WAIT_NEXT_WAVE;
            } else {
                _phase = ChallengePhase::REWARD;
```

`_grant_rewards` 调用行保持原样（Task 5 再改签名）。

**波次追踪（`_total_waves = 3`）：** 波0清→`=1`，`decide_advance(1,3,·)`→WAIT → 刷波1；波1清→`=2`→WAIT → 刷波2；波2清→`=3`，若 `!_boss_wave_decided` 先判定一次，`decide_advance(3,3,pending)`→BOSS_WAIT 或 REWARD；boss 清→`=4`→`decide_advance(4,3,true)`→REWARD+CLEARED。

- [ ] **Step 3.5: 按房间（而非按层）清判定标记 + 槽位 99 兜底**

Task 2 审查发现两个覆盖空洞，必须在刷出层一并补上：

**① 判定标记的失效时序。** `reset()` 只在 `game_scene.cpp:401`（enter_floor）与 `:3707`（exit_challenge_arena）被调用，是**按层**粒度；而 `game_scene.cpp:1294` 的进场重入路径直接 `_challenge.set_phase_for_test(ChallengePhase::ARMED)`，**不调 `reset()`**。一旦 Step 3 写入 `_boss_wave_decided = true`，同一层内的第二次挑战房会**跳过压轴判定**、永远不出 GOLEM。
修法是**按房间**清标记，在真正的房间起点 `on_doors_locked()`（`challenge_room.cpp:73-78`，由 `game_scene.cpp:1271` 与 `:1303` 两路到达）追加：

```cpp
    _boss_wave_decided = false;
    _boss_wave_pending = false;
```

`reset()` 里 Task 2 Step 5 的两行**保留**（按层兜底仍有价值）。两处都清，任一生效即可保证语义。

**② 槽位 99 误路由的静默降级。** 若 Step 4 分流写错，`pick_challenge_monster` 对 `wave = 99` 返回 `nullptr`（`spawn_tables.cpp:117` 越界判空），`_pick_monster_type` 静默回落 `"slime"`（`challenge_room.cpp:22`）——「隐藏 Boss 波」会刷出 4 只史莱姆且无任何报错。在 `_spawn_wave` 入口的分流处加显式断言：

```cpp
    assert(wave_index != kBossWaveSlot);  // 压轴波必须走 _spawn_boss_wave, 禁止静默降级成史莱姆
```

- [ ] **Step 4: 实现 `_spawn_boss_wave` 并在 `_spawn_wave` 入口分流**

`challenge_room.cpp`，在 `_spawn_wave` 定义之后追加：

```cpp
void ChallengeRoomController::_spawn_boss_wave(
    GameMap* map,
    std::vector<std::unique_ptr<Monster>>& monsters,
    int floor) {

    int cx = _room_rx + _room_rw / 2;
    int cy = _room_ry + _room_rh / 2;

    bool ok = map->is_walkable(cx, cy);
    for (int r = 1; !ok && r <= 4; r++)
        for (int dy = -r; !ok && dy <= r; dy++)
            for (int dx = -r; !ok && dx <= r; dx++) {
                if (map->is_walkable(cx + dx, cy + dy)) {
                    cx += dx;
                    cy += dy;
                    ok = true;
                }
            }

    if (!ok) {
        LOG_WARN("[CHALLENGE] No walkable tile near center, skip boss wave");
        return;
    }

    Monster* boss = boss_factory_create(BossType::GOLEM, cx, cy, floor);
    if (!boss) {
        LOG_WARN("[CHALLENGE] boss_factory_create returned null");
        return;
    }

    monsters.emplace_back(boss);
    _monsters_alive_this_wave++;
    LOG_INFO("[CHALLENGE] Boss wave spawned at tile %d,%d (floor %d)", cx, cy, floor);
}
```

然后在 `_spawn_wave`（`:160`）函数体**最开头**加分流：

```cpp
    if (wave_index >= _total_waves) {
        _spawn_boss_wave(map, monsters, floor);
        return;
    }
```

**关键：不要**对 boss 套用 `gc.monster_hp` / `mod.hp_multiplier`——`boss_factory_create` 内部已乘 `boss_hp_scale`/`boss_atk_scale`，再套就是双份放大。

- [ ] **Step 5: 构建并跑全量测试**

```powershell
cmake -B build
cmake --build build --config Release -- -j 4
ctest --test-dir build
```

预期：build 0 error；ctest 全绿。

- [ ] **Step 6: 提交**

```bash
git add src/game/world/challenge_room.h src/game/world/challenge_room.cpp tests/economy/challenge_room_test.cpp
git commit -m "feat(b4): 挑战房压轴波刷 GOLEM

- COMBAT 全灭分支在 _current_wave==_total_waves 时判定一次, 命中则走 3s WAIT 当登场前奏
- _spawn_boss_wave: 房中心就近搜索可走格, boss_factory_create(BossType::GOLEM, tile 坐标)
- 不给 boss 套 challenge modifier (factory 内部已乘 boss_hp/atk 缩放, 否则双份放大)
- 存活计数走既有 _room_contains 逻辑, 零改动"
```

---

### Task 4: 奖励隔离守卫（修复真实漏洞）

**Files:**
- Modify: `src/game/scene/game_scene_combat.cpp:102`
- Modify: `tests/world/floor_lifecycle_test.cpp`（或新增 `tests/world/boss_floor_guard_test.cpp`）

**Interfaces:**
- Consumes: `is_boss_floor(int)`（`config.cpp:3-5`，`config.h:88` 已声明，`game_scene_combat.cpp:19` 已包含 `config.h`）
- Produces: 非 boss 层杀 `is_boss` 怪物走常规掉落分支，不触发 `_drop_boss_reward` / 圣遗物 / 30% 回血

- [ ] **Step 1: 写失败的契约测试**

新建 `tests/world/boss_floor_guard_test.cpp`（守卫依赖 `is_boss_floor` 的精确真值表，须锁定）：

```cpp
#include <gtest/gtest.h>
#include "config.h"

TEST(BossFloorGuardTest, BossFloorsOnlyFiveTenFifteen) {
    EXPECT_TRUE(is_boss_floor(5));
    EXPECT_TRUE(is_boss_floor(10));
    EXPECT_TRUE(is_boss_floor(15));
}

TEST(BossFloorGuardTest, AllOtherFloorsReturnFalse) {
    for (int f = 1; f <= 30; f++) {
        bool expected = (f == 5 || f == 10 || f == 15);
        EXPECT_EQ(is_boss_floor(f), expected) << "floor " << f;
    }
}

// 关键回归: 压轴波只可能在非 boss 层触发 (F1/F6/F7/F11/F12 等), 守卫必须为假
TEST(BossFloorGuardTest, ChallengeRoomFloorsAreNeverBossFloors) {
    EXPECT_FALSE(is_boss_floor(1));
    EXPECT_FALSE(is_boss_floor(6));
    EXPECT_FALSE(is_boss_floor(9));
    EXPECT_FALSE(is_boss_floor(11));
    EXPECT_FALSE(is_boss_floor(14));
}
```

- [ ] **Step 2: 注册测试**

`tests/CMakeLists.txt` 追加：

```cmake
add_roguelike_test(boss_floor_guard_test world/boss_floor_guard_test.cpp)
```

- [ ] **Step 3: 跑测试**

```powershell
cmake -B build
cmake --build build --config Release -- -j 4
ctest --test-dir build -R BossFloorGuardTest --output-on-failure
```

预期：全 PASS（`is_boss_floor` 语义本就正确，这是锁定契约而非修 bug）。

- [ ] **Step 4: 加守卫**

`src/game/scene/game_scene_combat.cpp:102`，把

```cpp
    if (m->is_boss) {
```

改为

```cpp
    if (m->is_boss && is_boss_floor(_s.current_floor)) {
```

**为何必要：** 非 5/10 层杀 `is_boss` 怪物会走 `_drop_boss_reward` → `bf_idx = (current_floor==5)?0 : (current_floor==10)?1 : 2` → 非 boss 层一律给 index 2 = `sword_legendary`（倚天剑，F15 终 boss 武器）+ 随机圣遗物 + 30% 回血。压轴 GOLEM 会让玩家在 F6-9 白拿终章武器。

**为何安全：** 守卫条件在 F5/F10/F15 恒真 → 主线行为零变化；`Boss1/2/3_Defeated` 世界旗本就各自 gate 在 `current_floor==5/10/15`，不受影响。

- [ ] **Step 5: 构建 + 全量测试**

```powershell
cmake --build build --config Release -- -j 4
ctest --test-dir build
```

预期：build 0 error；ctest 全绿。

- [ ] **Step 6: 提交**

```bash
git add src/game/scene/game_scene_combat.cpp tests/world/boss_floor_guard_test.cpp tests/CMakeLists.txt
git commit -m "fix(b4): boss 奖励加 is_boss_floor 楼层守卫

- 原 if(m->is_boss) 无楼层保护: 非 boss 层杀 boss 会白拿 sword_legendary(倚天剑)
  + 随机圣遗物 + 30% 回血 (bf_idx else 分支固定给 F15 武器)
- 压轴 GOLEM 在 F6-9 触发即成可利用漏洞, 必须隔离
- 守卫在 F5/F10/F15 恒真, 主线零变化; Boss1/2/3_Defeated 本就各自 gate 楼层
- 新测试锁定 is_boss_floor 精确真值表 (1..30 全枚举)"
```

---

### Task 5: 奖励层——压轴额外奖

**Files:**
- Modify: `src/game/world/challenge_room.h:91-92`
- Modify: `src/game/world/challenge_room.cpp:206-234`
- Modify: `tests/economy/challenge_room_test.cpp`

**Interfaces:**
- Consumes: `_boss_wave_pending`（Task 2）、`generate_random_item()`（`item.h:95`）、`Rarity::EPIC`、`player.inventory.add(item, &player)`（现有 `:218` 同用法）、`RewardManager::grant_gold`
- Produces: `_grant_rewards(Player&, GameMap*, int, vector<DroppedItem>&, bool boss_cleared)`

- [ ] **Step 1: 改签名与调用点**

`challenge_room.h:91-92`：

```cpp
    void _grant_rewards(Player& player, GameMap* map, int floor,
                        std::vector<DroppedItem>& ground_items, bool boss_cleared);
```

`challenge_room.cpp` 的调用点（`:138`）：

```cpp
                _grant_rewards(*player, map, floor, ground_items, _boss_wave_pending);
```

- [ ] **Step 2: 实现奖励叠加**

`challenge_room.cpp:206-234`，函数签名加 `bool boss_cleared`，末尾金币段改为：

```cpp
void ChallengeRoomController::_grant_rewards(Player& player, GameMap* map, int floor,
                                               std::vector<DroppedItem>& ground_items,
                                               bool boss_cleared) {
    int granted = 0;
    for (int i = 0; i < 3; i++) {
        auto item = generate_random_item();
        int tries = 0;
        while (item && item->rarity < Rarity::RARE && tries < 5) {
            item = generate_random_item();
            tries++;
        }
        if (!item) continue;

        if (player.inventory.add(item, &player)) {
            granted++;
        } else {
            int cx = _room_rx + _room_rw / 2;
            int cy = _room_ry + _room_rh / 2;
            DroppedItem di;
            di.item = std::move(item);
            di.tile_x = cx;
            di.tile_y = cy;
            ground_items.push_back(std::move(di));
        }
    }

    if (boss_cleared) granted += _grant_boss_bonus(player, ground_items);

    int gold = 50 + floor * 15;
    if (boss_cleared) gold = (int)(gold * 1.5f);
    RewardManager::grant_gold(player, gold);
    LOG_INFO("[CHALLENGE] Rewards: %d items + %d gold%s",
             granted, gold, boss_cleared ? " (boss cleared)" : "");
}
```

新增私有方法（声明加到 `challenge_room.h` private 段，定义放在 `_grant_rewards` 之后）：

```cpp
// 压轴 boss 额外奖: 1 件 EPIC+, 最多重试 8 次
int ChallengeRoomController::_grant_boss_bonus(Player& player,
                                                std::vector<DroppedItem>& ground_items) {
    auto item = generate_random_item();
    int tries = 0;
    while (item && item->rarity < Rarity::EPIC && tries < 8) {
        item = generate_random_item();
        tries++;
    }
    if (!item) return 0;

    if (player.inventory.add(item, &player)) return 1;

    DroppedItem di;
    di.item = std::move(item);
    di.tile_x = _room_rx + _room_rw / 2;
    di.tile_y = _room_ry + _room_rh / 2;
    ground_items.push_back(std::move(di));
    return 1;
}
```

- [ ] **Step 3: 写测试**

`tests/economy/challenge_room_test.cpp` 追加（走公共接口 `tick`，用 `_boss_wave_pending` 的只读 getter 断言）：

```cpp
TEST(ChallengeRoomTest, RewardPathCompilesWithBossClearedFlag) {
    // 签名变更的冒烟用例: 确认 5 参 _grant_rewards 可被调用
    // 直接调私有方法不可行, 故用 controller 默认状态走 reset 后判定路径
    ChallengeRoomController c;
    c.reset();
    EXPECT_FALSE(c.boss_wave_pending());
}
```

- [ ] **Step 4: 构建 + 全量测试**

```powershell
cmake --build build --config Release -- -j 4
ctest --test-dir build
```

预期：build 0 error（签名变更的所有调用点已同步）；ctest 全绿。

- [ ] **Step 5: 跑 validator**

```powershell
& "C:\Users\HP\anaconda3\python.exe" tools\world_validator.py
```

预期：0 error / 0 warning。

- [ ] **Step 6: 提交**

```bash
git add src/game/world/challenge_room.h src/game/world/challenge_room.cpp tests/economy/challenge_room_test.cpp
git commit -m "feat(b4): 压轴 boss 额外奖 (EPIC+ 物品 + 50% 金币)

- _grant_rewards 加 bool boss_cleared 入参, 调用点传 _boss_wave_pending
- _grant_boss_bonus: 1 件物品重试 8 次至 EPIC+ (Rarity: COMMON0/RARE1/EPIC2/LEGENDARY3)
- 金币 50+floor*15 在 boss_cleared 时乘 1.5
- 未触发压轴时行为与现状完全一致"
```

---

### Task 6: 美术层——`boss_golem` 骨骼资产

**Files:**
- Modify: `tools/gen_boss_parts.py`（`PALETTES` + `BOSS` + `DECOR` + `assert`）
- Create: `assets/sprites/boss_golem_part_{torso,head,arm,leg,weapon}.png`（生成）
- Create: `resources/animations/boss_golem_skeleton.json`（生成）
- Modify: `resources/animations/actor_avatars.json`
- Modify: `resources/sprites.json`

**Interfaces:**
- Consumes: `gen_boss_parts.py` 既有管线（`part_rows` 对 `weapon is None` 返回全透明，fire_demon 已是先例）
- Produces: `boss_factory_create` 的 `vkey = "boss_" + def->visual_id`（`boss.cpp:1213`）探测 `sprite_by_key("boss_golem")` 命中 → 骨骼渲染；否则回落 `boss_f5`/`boss_f10` 静态图（造型错，不采用）

- [ ] **Step 1: 加调色板**

`tools/gen_boss_parts.py` 的 `PALETTES` 字典追加（石质灰蓝，对齐 `boss.cpp:1120` 的视觉色 `{100,100,130,255}`）：

```python
    "golem_boss": {"s": (38, 38, 48), "m": (78, 78, 92), "l": (122, 122, 140),
                   "h": (166, 166, 186), "r": (112, 96, 80),
                   "R": (146, 144, 162), "p": (204, 204, 220),
                   "b": (22, 22, 30), "g": (96, 88, 70)},
```

- [ ] **Step 2: 加 BOSS 条目与装饰**

`BOSS` 字典追加（无武器，`weapon is None` → 全透明 weapon 分件，fire_demon 同款先例）：

```python
    "golem": ("golem_boss", None),
```

`assert len(BOSS) == 5` 改为 `assert len(BOSS) == 6`。

`DECOR` 追加（石缝裂纹，格式同邻近条目 `(y0, y1, x0, x1, char)`）：

```python
    ("golem", "head"): [(2, 4, 10, 25, "b"), (8, 10, 8, 27, "R")],
    ("golem", "torso"): [(5, 7, 6, 20, "b"), (12, 14, 22, 35, "R"),
                         (18, 19, 10, 30, "b")],
    ("golem", "arm"): [(5, 7, 4, 13, "l")],
    ("golem", "leg"): [(8, 10, 8, 24, "b")],
```

- [ ] **Step 3: 运行生成器**

```powershell
& "C:\Users\HP\anaconda3\python.exe" tools\gen_boss_parts.py
```

预期输出 `wrote golem parts+skeleton`。

- [ ] **Step 4: 校验产物与确定性**

```powershell
Get-ChildItem assets\sprites\boss_golem_part_*.png | Select-Object Name,Length
```

预期 5 个 PNG（torso/head/arm/leg/weapon）。

```bash
git status --short assets/sprites resources/animations
```

**关键校验：** diff 中除 `boss_golem_*` 新文件外，既有 5 个 boss 的分件 PNG 与 skeleton JSON **不应有任何变化**（生成器确定性）。若有变化，说明生成器非确定性，须停下排查。

- [ ] **Step 5: 登记 avatar**

`resources/animations/actor_avatars.json`，在 `boss_fire_demon`（`:135`）与 `boss_self`（`:139`）之间插入：

```json
    "boss_golem": {
      "skeleton": "resources/animations/boss_golem_skeleton.json",
      "anim": "resources/animations/boss_anim.json"
    },
```

- [ ] **Step 6: 登记 sprite**

`resources/sprites.json` 两处追加。**第一处是承重环节，第二处只是素材注册。**

**整图 1 条（承重，放在 `sprites` 段 `boss_fire_demon` 之后，格式同其旁的 `boss_f10`）：**

```json
    "boss_golem": {
      "file": "assets/sprites/mon_golem.png",
      "frame_w": 16,
      "frame_h": 16
    },
```

**为何承重：** `resource_manager.cpp:290` 只从 `j.at("slides")` 以外的 `"sprites"` 段填充 `_sprite_defs`，而 `sprite_by_key`（`:307-313`）只查这张表。`boss.cpp:1213-1220` 用 `vkey = "boss_" + visual_id` 探测：**命中** → `sprite_override = "boss_golem"`；**未命中** → 回落 `"boss_f10"`。骨骼白名单在 `game_scene.cpp:3005` 用 `monster_actor_key(m)` 查表，而 `monster_actor_key`（`monster.cpp:173-175`）就是 `sprite_override` 优先、否则 `m.name`。`boss_f10` 不在 `actor_avatars.json` 白名单里（实测无此键）→ `continue` → **无骨骼** → GOLEM 以火魔静态图渲染。所以缺这一条时，Step 5 的 avatar 登记与 Step 6 的分件注册**全部失效**。

复用 `assets/sprites/mon_golem.png`（16×16，已作为 `mon_golem` 注册，无需新增素材）：整图仅用于遭遇战立绘（`game_renderer.cpp:578-600`，16→48 缩放）与骨骼失败时的静态兜底，战斗内走骨骼分件。造型是石头魔像而非火魔，退化方向正确。

**分件 5 条**（放在 `skeleton_parts` 段，照 `boss_shadow_knight_part_*` 在 `:1242-1266` 的格式；`w`/`h` 必须与 `gen_boss_parts.py` 的 `SIZES` 一致，与 `boss_fire_demon_part_*` 同尺寸）：

```json
    "boss_golem_part_torso": {
      "file": "assets/sprites/boss_golem_part_torso.png",
      "w": 40,
      "h": 28
    },
    "boss_golem_part_head": {
      "file": "assets/sprites/boss_golem_part_head.png",
      "w": 30,
      "h": 26
    },
    "boss_golem_part_arm": {
      "file": "assets/sprites/boss_golem_part_arm.png",
      "w": 16,
      "h": 22
    },
    "boss_golem_part_leg": {
      "file": "assets/sprites/boss_golem_part_leg.png",
      "w": 32,
      "h": 28
    },
    "boss_golem_part_weapon": {
      "file": "assets/sprites/boss_golem_part_weapon.png",
      "w": 10,
      "h": 30
    },
```

（`w`/`h` 必须与 `gen_boss_parts.py` 的 `SIZES` 一致；若不确定，从同段邻近 `boss_fire_demon_part_*` 条目照抄。）

- [ ] **Step 7: 校验四向对齐**

```powershell
& "C:\Users\HP\anaconda3\python.exe" tools\world_validator.py
```

预期：0 error / 0 warning。若 validator 有精灵↔骨架↔avatar↔def 的交叉校验，此处即验证：`bosses.json.visual_id="golem"` → `boss_golem` avatar → `boss_golem_skeleton.json` → 5 个 PNG 全部存在。

- [ ] **Step 7.5: 更新 `animation_test` 的白名单期望集**

`tests/animation/animation_test.cpp:701-702` 的 `RepoDefaultWhitelistCoversA6HumanoidFamily` 硬编码了 35 键期望集（`:703` 断言 `out->size() == expected.size()`）。Step 5 的 avatar 登记会让它变 36 条而失败。在 `:701` 的 `"boss_fire_demon",` 之后插入一行：

```cpp
                                             "boss_fire_demon", "boss_golem",
```

这不是削弱测试：该用例对期望集里每个键还会逐个断言 `skeleton`/`anim` 非空、文件存在、可解析，且 `sk->bones.size() == 9u`、`sk->parts.size() == 7u`（`:714-715`）。所以这一行同时锁定了新骨架的结构维度——生成器必须产出与既有 5 个 boss 同形的 9 骨 7 件骨架。若维度不符，此处即报，不要去改 `9u`/`7u` 的期望值。

- [ ] **Step 8: 构建 + 全量测试**

```powershell
cmake --build build --config Release -- -j 4
ctest --test-dir build
```

预期：build 0 error；ctest 全绿。

- [ ] **Step 9: 提交**

```bash
git add tools/gen_boss_parts.py assets/sprites/boss_golem_part_*.png resources/animations/boss_golem_skeleton.json resources/animations/actor_avatars.json resources/sprites.json
git commit -m "feat(b4): boss_golem 骨骼美术 (6 Boss)

- gen_boss_parts.py 加 golem_boss 调色板 (石质灰蓝, 对齐视觉色 100,100,130) + BOSS 条目 (无武器)
- assert 5->6; DECOR 加石缝裂纹
- actor_avatars 登记 boss_golem -> boss_golem_skeleton.json + boss_anim.json
- sprites.json 追加 5 条分件 (尺寸同 SIZES)
- 生成器确定性已校验: 既有 5 个 boss 资产零变化"
```

---

### Task 7: 收尾——validator 自检 + CHANGELOG + 桌面同步 + 实机验收

**Files:**
- Modify: `tools/world_validator.py`
- Modify: `README.md`

**Interfaces:**
- Consumes: 前 6 个 Task 的全部产物
- Produces: 本批可关闭

- [ ] **Step 1: 加 validator 自检**

`tools/world_validator.py`，在 `boss_ids` 汇总（`:73`）之后追加一个独立段：

```python
# ── Boss defs 自检 (B4: GOLEM 挑战房压轴 boss) ──
VALID_BOSS_SKILLS = {"charge", "shockwave", "summon", "barrage", "cone", "blink"}
VALID_COMBO_CMDS = {"normal", "charge", "shockwave", "summon", "defend",
                    "barrage", "cone", "blink", "whirlwind"}

if isinstance(bosses, dict):
    _boss_list = list(bosses.values())
elif isinstance(bosses, list):
    _boss_list = list(bosses)
else:
    _boss_list = []
_boss_by_id = {b.get("id"): b for b in _boss_list if b.get("id")}

# get_boss_def_for_floor 是硬编码 switch (boss_defs.cpp:170-176), 目标必须存在
for _fl, _fid in ((5, "shadow_knight"), (10, "fire_demon"), (15, "demon_lord")):
    if _fid not in _boss_by_id:
        err("bosses.json: get_boss_def_for_floor(%d) 硬编码 '%s' 但 JSON 无该条目"
            % (_fl, _fid))

# BossType 映射一致性: get_boss_def_for_type(4) -> "golem" (boss_defs.cpp:185)
if "golem" not in _boss_by_id:
    err("bosses.json: 缺 'golem' — BossType::GOLEM(4) 映射断链, 挑战房压轴不可达")
else:
    _g = _boss_by_id["golem"]
    if not _g.get("is_defender"):
        err("bosses.json [golem]: is_defender 必须为 true (golem_shield_pct 赋值开关)")
    if not (_g.get("shield_pct") or 0) > 0:
        err("bosses.json [golem]: shield_pct 必须 > 0 (boss.cpp:787 生效条件)")
    # 技能按 id-match 覆盖参数 (boss.cpp:1240-1257), 与数组位置无关
    _ids = [_s.get("id") for _s in _g.get("skills", [])]
    for _req in ("charge", "shockwave"):
        if _req not in _ids:
            err("bosses.json [golem].skills 缺 '%s' — ai->_%s 参数无法被覆盖"
                % (_req, _req))
    if _g.get("skill_cycle_bias") in (4, 6):
        err("bosses.json [golem].skill_cycle_bias=%s 会让 _next_cycle_skill "
            "返回 Summon, 与纯 tank 定位冲突" % _g.get("skill_cycle_bias"))
    if _g.get("is_summoner"):
        warn("bosses.json [golem].is_summoner=true — 该字段只用于显示串, "
             "不 gating 召唤")
    for _i, _s in enumerate(_g.get("skills", [])):
        if _s.get("id") not in VALID_BOSS_SKILLS:
            err("bosses.json [golem].skills[%d].id '%s' 非法"
                % (_i, _s.get("id")))
    for _ci, _c in enumerate(_g.get("combos", [])):
        for _cmd in _c.get("commands", []):
            if _cmd not in VALID_COMBO_CMDS:
                err("bosses.json [golem].combos[%d].commands '%s' 非法"
                    % (_ci, _cmd))
```

> 执行者注意：`err()` / `warnings.append()` 是该脚本既有 helper（见 `:223`、`:330`）。若实际函数名不同，照邻近用法对齐。

- [ ] **Step 2: 跑 validator**

```powershell
& "C:\Users\HP\anaconda3\python.exe" tools\world_validator.py
```

预期：**0 error / 0 warning**。

- [ ] **Step 3: 跑 sim 记录 sha**

```powershell
$env:PATH = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.43.34808\bin\Hostx64\x64;C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64;C:\Users\HP\miniconda3;C:\Users\HP\miniconda3\Scripts;C:\Python313;C:\Users\HP\.cargo\bin;" + $env:PATH
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
cmake --build build --config Release -- -j 4
ctest --test-dir build
.\build\roguelike_cpp.exe --sim 12 --sim-seed 3 > C:\Users\HP\AppData\Local\Temp\opencode\sim_b4.txt
```

记录 sha256 与批次9 基线 `953f1630…e762e9474`（归一化 3479 行）的差异：
- **期望**：大概率无差异（agent 死在 F1-2，挑战房需钥匙且压轴仅 25%）
- **若有差异**：按批次5/8 先例如实记录为「挑战房奖励消耗全局 rng 的预期行为变更」，并确认差异仅出现在挑战房结算之后的行

- [ ] **Step 4: 更新 README CHANGELOG**

`README.md:127-136` 的批次 1-9 块之后追加本批条目，含：数据 def、压轴判定（25% / 保留槽位 99 / 不碰全局 rng）、刷出路径、奖励隔离守卫（bug fix）、奖励叠加、美术、门禁结果（build/ctest/validator/sim sha）。

- [ ] **Step 5: 全量门禁复跑**

```powershell
cmake --build build --config Release -- -j 4
ctest --test-dir build
& "C:\Users\HP\anaconda3\python.exe" tools\world_validator.py
```

预期：build 0 error；ctest 全绿；validator 0/0。

- [ ] **Step 6: 同步桌面测试包**

镜像 `src/ resources/ tools/ tests/ docs/ assets/ .github/ vendor/` + 根文件（CMakeLists.txt README.md CLAUDE.md CMakePresets.json .gitignore）到 `C:\Users\HP\Desktop\Roguelike-CPP-3D版`，**exe 必须复制到包根目录**。保留桌面独有的 `saves/`（含 `.bak`）与 `3D模式.exe.lnk`。**不动** `Roguelike-CPP-初代版`（已冻结）。

- [ ] **Step 7: 提交收尾**

```bash
git add tools/world_validator.py README.md
git commit -m "docs(b4): validator boss 自检 + CHANGELOG"
```

- [ ] **Step 8: 实机验收（用户门禁）**

请用户用桌面包根目录的 `roguelike_cpp.exe` 验证：
1. **2D + HD2D 各打一场有压轴的**：3 波小怪清完后等 3 秒刷出远古魔像，观察 DEFEND 减伤姿态、Phase2 三连震、boss HUD
2. **确认魔像全程不召唤小怪**（`skill_cycle_bias=5` 避开了 Summon 槽）
3. **打一场没有压轴的**：3 波后直接结算，奖励与现状一致
4. **确认 GOLEM 死后不掉倚天剑 / 圣遗物**（奖励隔离守卫生效）
5. **确认造型是石头魔像而非 fire_demon**（`boss_golem` avatar 命中，未回落 `boss_f10`）

---

## 验收清单

- [ ] `bosses.json` 有 `golem` def，`skills` 含 `charge`+`shockwave`，`skill_cycle_bias ∉ {4,6}`（不召唤）
- [ ] `get_boss_def_for_floor(10)` 仍返回 `fire_demon`（未 shadow）
- [ ] `has_boss_wave` 确定性 25%，不消耗全局 `rng`
- [ ] 波次追踪：波0→1→2→(判定)→boss→REWARD+CLEARED
- [ ] 压轴波**不叠加** challenge modifier（无双份放大）
- [ ] 非 boss 层杀 `is_boss` 怪物走常规掉落（无倚天剑 / 圣遗物）
- [ ] 压轴额外奖：EPIC+ 物品 + 50% 金币；未触发时零变化
- [ ] `boss_golem` 四向对齐（def.visual_id → avatar → skeleton → 5 PNG）
- [ ] 生成器确定性：既有 5 个 boss 资产零变化
- [ ] build 0 error / ctest 全绿 / validator 0 error 0 warning
- [ ] sim sha 差异已记录并判定为预期
- [ ] README CHANGELOG 已更新
- [ ] 桌面测试包已同步，exe 在包根
- [ ] 用户实机验收 4 项通过

## 范围外

- 不把 GOLEM 加入任何主线楼层（F10 保持 fire_demon）
- 不做「整间 Boss 房」变体
- 不改挑战房小怪波数与刷怪池（`challenge_pools.json` 不动）
- 不改 `boss_defs.h` / `boss.cpp` / `boss_system_director.*`
- 不动「挑战房整间变 Boss 房」与「固定楼层必出」两个已否决方向
