# A6-S2 批次9：刷怪表数据化（slot→id + 挑战房池）— 设计文档

- 日期：2026-09-25
- 状态：**已批准**（2026-09-25 用户确认），进入实现计划
- 关联：批次8 `e982d9b`（lightning_orb 运行时刷出路径）；批次6 修正 `b18b622`
- 基线：HEAD `b18b622`，工作树 clean

---

## 1. 背景与目标

批次8 打通 `lightning_orb` 刷出路径时，改动方式是**往 C++ 硬编码字面量里插一条地板区间条件**（`floor_manager.cpp:32`）。这条缺口能连跨 3 个批次无人发现，根因不是漏改，而是**刷怪选怪表整个活在 C++ 里，JSON 里那份是死的**。

本批目标：把选怪表迁到 JSON，让「某怪刷不出」从此由校验器自动抓出，不再靠人肉发现。

用户已选定范围（方案 1）：**只做 slot→id 与挑战房池的数据化；楼层权重表保持编译期不动。**

---

## 2. 现状盘点

### 2.1 当前是两层结构，不是两层都硬编码

| 层 | 内容 | 现状 |
|---|---|---|
| 层权重 | 12 个原型槽位权重 | **已经是数据**，但存在编译期 C++ 常量表 `FLOORS[15]`（`floor_config.cpp:8-29`） |
| 槽位→具体怪 id | 12 个 `case` | **真正的硬编码**（`floor_manager.cpp:25-38`） |
| 挑战房池 | 3 群系 × 3 波 = 9 池 | **硬编码**（`challenge_room.cpp:25-49`） |

### 2.2 承重结构：楼层级权重不可动

`FLOORS[15]` 的权重差异极大，注释里全是调参历史：

| 层 | `enemy_weights[12]` | 语义 |
|---|---|---|
| F1 | `{96,2,2,0,0,0,0,0,0,0,0,0}` | P1-C1 教学层特调（P1-B 基线 96.6% 死于 F1） |
| F13 | `{50,10,10,8,8,7,6,5,6,6,5,7}` | 全槽位铺开，最难层 |
| F5/10/15 | boss 层 | `monster_count=1` |

**因此 `biomes.json` 的 `enemy_pool` 不能当驱动源** —— 它只有 3 个群系级池子覆盖 15 层，表达不了 F1 教学特调 vs F13 全槽位难度。拿它驱动 = 平衡回退。

### 2.3 死数据现状

- `BiomeDef::enemy_pool` / `enemy_weights`：有解析器（`biome.cpp:63-69`）、**零消费者**
- `resources/floor_config.json`：存在但**无加载器**，且字段名已与 C++ 分叉（`label`/`story`/`hp`/`monsters`/`special`/`boss`/`rest` vs C++ `chapter_label`/`story_msg`/`hp_mult`/`monster_count`/`special_room_count`/`is_boss`/`is_rest_floor`），并缺 `enemy_weights`/`team_coop_chance`/`arena_density`/`fov_radius`

### 2.4 一个只在 C++ 里可见的隐藏映射

`"elite"` 不是 `enemies.json` 条目。它在 `spawn_monster`（`monster.cpp:416-418`）运行时随机解析成 `elite_slime`/`elite_orc`，**消耗一次 `rng()`**。任何基于 JSON 的校验都不认识它。

### 2.5 `rng()` 的可计数性

`rng` 是全局 `CountingRng`（`combat_system.h:23-28`，继承 `std::mt19937`，`operator()` 自增 `draws`）。**掷骰次数是可观测的** —— 这给「行为等价」提供了比 sim sha 更硬的验证信号。

---

## 3. 范围界定

### 3.1 范围内

1. 新增 `resources/enemy_slots.json`：12 个原型槽位 → 候选怪列表（含权重、可选地板区间、别名声明）
2. 新增 `resources/challenge_pools.json`：9 个挑战房池
3. 新增 `src/data/spawn_tables.h` 与 `src/data/spawn_tables.cpp`：两个加载器 + 纯函数查询接口
4. `floor_manager.cpp:_pick_monster_type` 的 `switch` 块改为调用数据查询（**外层 12 槽位加权抽签逻辑一字不改**）
5. `challenge_room.cpp:_pick_monster_type` 的 9 个池子改为从数据取
6. `main.cpp` 接线两个加载器
7. `tools/world_validator.py` 增加前后向交叉校验
8. 新增 golden oracle 单元测试证明 RNG 逐位等价

### 3.2 范围外（明确记录，不碰）

- `FLOORS[15]` 编译期表、`FloorConfig` 结构体与 `enemy_weights[12]` —— 全部不动
- `floor_config.json` 的激活与字段统一 —— 归候选项 D（死代码/双源清理），另立批次
- `spawn_monster` 的 `"elite"` 别名分支 —— **保留**。`game_scene.cpp:1644` 仍直接传 `"elite"`，别名解析保持在工厂层最安全
- 召唤路径的硬编码字面量（`boss.cpp:119-120`、`ai.cpp:757-758`、`ai.cpp:845-846`、`boss_command.cpp:107`、`skill.cpp:706`）—— 5 处，全部只出 orc/slime/archer，属行为设计（召唤物刻意弱），不在选怪表范畴
- `biome.enemy_pool` 的驱动化 —— 见 §3.3

### 3.3 一处对我之前提议的修正（需你确认）

我之前说「`biome.enemy_pool` 挪到校验器侧当合法集」。**这个提议不成立，我撤回。**

原因：`biome.enemy_pool` 是 3 个群系级代表性名册（如 `ash_volcano` 只有 5 个怪），而槽位系统对 F6-10 实际会产出约 17 种怪（slime、archer、shaman、tank…全都来自权重非零的槽位）。拿前者当后者的合法集，每层会报 10+ 条误报 —— 是噪声不是信号。两者是不同粒度的模型，不该互相约束。

**修正后的处理**：`biome.enemy_pool` 只加一条轻量校验（其 id 必须是真敌人），保持非死状态；真正的新价值来自 §7 的前向 + 反向可达性校验，**这两条完全不依赖 `biome.enemy_pool`**。

---

## 4. 数据格式

### 4.1 `resources/enemy_slots.json`

```json
{
  "default": "slime",
  "aliases": {
    "elite": ["elite_slime", "elite_orc"]
  },
  "slots": [
    { "archetype": "NORMAL",     "candidates": [ {"id": "orc", "weight": 1}, {"id": "slime", "weight": 2} ] },
    { "archetype": "Archer",     "candidates": [ {"id": "archer", "weight": 1} ] },
    { "archetype": "Shaman",     "candidates": [ {"id": "shaman", "weight": 1} ] },
    { "archetype": "Bomber",     "candidates": [ {"id": "bomber", "weight": 1} ] },
    { "archetype": "Tank",       "candidates": [ {"id": "tank", "weight": 1} ] },
    { "archetype": "Elite",      "candidates": [ {"id": "elite", "weight": 1} ] },
    { "archetype": "Charger",    "candidates": [ {"id": "lightning_orb", "weight": 1, "floors": [6, 10]}, {"id": "charger", "weight": 1} ] },
    { "archetype": "Summoner",   "candidates": [ {"id": "summoner", "weight": 1} ] },
    { "archetype": "Sniper",     "candidates": [ {"id": "skeleton_archer", "weight": 1}, {"id": "goblin_hunter", "weight": 1} ] },
    { "archetype": "Controller", "candidates": [ {"id": "dark_mage", "weight": 1}, {"id": "void_walker", "weight": 1} ] },
    { "archetype": "Ambush",     "candidates": [ {"id": "shadow_assassin", "weight": 1}, {"id": "night_stalker", "weight": 1} ] },
    { "archetype": "Guardian",   "candidates": [ {"id": "stone_guardian", "weight": 1}, {"id": "iron_sentinel", "weight": 1} ] }
  ]
}
```

设计要点：
- **数组下标 = 槽位序号**，与 `FloorConfig::enemy_weights[12]` 位置对齐（12 项，0..11）
- **候选顺序即抽签顺序**，必须与旧 `switch` 分支返回顺序一致（见 §5.2）
- 单候选槽位（1..5, 7）不消耗 rng；多候选槽位消耗恰好 1 次
- `floors: [6, 10]` 闭区间，缺省 `[1, 15]`；不在区间的候选被过滤，**若过滤后只剩 1 个候选则不掷骰**（这是保 RNG 逐位等价的关键）
- `aliases` 只作**文档与校验用**，运行期不展开（§3.2）
- `"default"` 是外层加权抽签落空时的兜底（旧代码是字面量 `"slime"`）

### 4.2 `resources/challenge_pools.json`

```json
{
  "biomes": [
    { "floors": [1, 5],   "waves": [
        ["slime", "skeleton_archer", "bone_soldier"],
        ["orc", "shadow_stalker", "blood_leech"],
        ["elite_slime", "charger", "summoner", "orc"] ] },
    { "floors": [6, 10],  "waves": [
        ["fire_imp", "bomber", "frost_slime", "lightning_orb"],
        ["orc", "shaman", "poison_wyrm"],
        ["storm_elemental", "golem", "necromancer"] ] },
    { "floors": [11, 15], "waves": [
        ["shadow_stalker", "void_walker", "dark_mage"],
        ["ice_warden", "blood_priest", "night_stalker"],
        ["stone_guardian", "iron_sentinel", "elite_orc"] ] }
  ]
}
```

- 旧代码边界 `floor <= 5` / `floor <= 10` / else → 闭区间 `[1,5]` / `[6,10]` / `[11,15]` 完全等价
- 池内为**均匀**抽签（`roll % size`），与旧 `p.types[r % p.count]` 一致；保持无权重，不引入新平衡维度
- 列表内允许重复 id（旧代码 `["elite_slime","charger","summoner","orc"]` 里 orc 出现两次于不同波，属合法数据）

---

## 5. C++ 设计

遵循 `biome.h/.cpp` 既有惯例：struct + `extern` 全局 registry + `bool load_x(const char* path = "...")` + 纯查询函数。放入 `src/data/`（CLAUDE.md 约定：JSON 配置加载器与数据验证归 `src/data/`）。

### 5.1 `src/data/spawn_tables.h`

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct SpawnCandidate {
    std::string id;
    int weight = 1;
    int floor_start = 1;
    int floor_end = 15;
};

struct SpawnSlot {
    std::string archetype;
    std::vector<SpawnCandidate> candidates;
};

struct ChallengeBiomePool {
    int floor_start, floor_end;
    std::vector<std::vector<std::string>> waves;
};

extern std::vector<SpawnSlot> g_spawn_slots;
extern std::vector<std::pair<std::string, std::vector<std::string>>> g_spawn_aliases;
extern std::string g_spawn_default;
extern std::vector<ChallengeBiomePool> g_challenge_pools;

bool load_spawn_slots(const char* path = "resources/enemy_slots.json");
bool load_challenge_pools(const char* path = "resources/challenge_pools.json");

const SpawnSlot* get_spawn_slot(int slot_index);
const std::string* pick_slot_monster(int slot_index, int floor);
const std::string* pick_challenge_monster(int floor, int wave, uint32_t roll);
```

返回值用 `const std::string*`（指向 registry 内稳定内存），不用 `const char*` —— 避免动态字符串的生命周期陷阱。

### 5.2 重接：`floor_manager.cpp`

**外层加权抽签（`:14-24`、`:41`）与 `if (w[i] == 0) continue` 的遍历语义完全不动**，只把 `switch (i) { ... }` 块（12 个 case）替换为：

```cpp
            const std::string* id = pick_slot_monster(i, cfg.floor);
            return id ? id->c_str() : g_spawn_default.c_str();
```

`pick_slot_monster` 内部实现：

```cpp
const std::string* pick_slot_monster(int slot_index, int floor) {
    const SpawnSlot* s = get_spawn_slot(slot_index);
    if (!s || s->candidates.empty()) return nullptr;
    std::vector<const SpawnCandidate*> elig;
    uint64_t total = 0;
    for (const auto& c : s->candidates) {
        if (floor < c.floor_start || floor > c.floor_end) continue;
        elig.push_back(&c);
        total += (uint64_t)c.weight;
    }
    if (elig.empty() || total == 0) return nullptr;
    if (elig.size() == 1) return &elig[0]->id;          // 不掷骰 — RNG 逐位等价关键
    uint64_t roll = rng() % (uint64_t)total;
    uint64_t acc = 0;
    for (const auto* c : elig) { acc += (uint64_t)c->weight; if (roll < acc) return &c->id; }
    return &elig.back()->id;
}
```

### 5.3 重接：`challenge_room.cpp`

`_pick_monster_type` 保留签名与 `static` 属性，9 个池子字面量替换为：

```cpp
    const std::string* id = pick_challenge_monster(floor, wave, rng);
    return id ? id->c_str() : "slime";
```

`"slime"` 这里保留为字面量兜底（旧代码路径无兜底、直接下标越界风险；此处仅防御，不改任何合法输入下的行为）。

### 5.4 接线：`main.cpp`

`main.cpp:307` 的 `load_biome_defs` 之后追加：

```cpp
    load_spawn_slots("resources/enemy_slots.json");       // A6-S2 批次9
    load_challenge_pools("resources/challenge_pools.json");
```

---

## 6. RNG 逐位等价性证明（核心正确性论证）

旧 `switch` 各槽位对 `rng()` 的消耗与取值映射：

| 槽位 | 旧代码 | 消耗 | 旧映射 | 新实现 | 等价性 |
|---|---|---|---|---|---|
| 0 | `rng()%3==0 ? orc : slime` | 1 | 0→orc, 1..2→slime | `[orc w1, slime w2]` total=3, roll=rng()%3 | acc: orc<1, slime<3 → 0→orc, 1,2→slime ✓ |
| 1..4, 7 | 字面量返回 | 0 | — | 单候选短路 | 0 次掷骰 ✓ |
| 5 | 字面量 `"elite"` | 0 | — | 单候选返回 `"elite"` | 0 次；别名留在 `spawn_monster` 解析 ✓ |
| 6 (F6-10) | `rng()%2==0 ? lightning_orb : charger` | 1 | 0→lo, 1→charger | `[lo w1, charger w1]` total=2 | 0→lo, 1→charger ✓ |
| 6 (F1-5/F11-15) | `false ? ... : "charger"` | **0** | 恒 charger | 过滤后仅剩 `[charger]` → 单候选短路 | **0 次掷骰** ✓ |
| 8 | `rng()%2==0 ? skeleton_archer : goblin_hunter` | 1 | 0→s_a, 1→g_h | `[skeleton_archer w1, goblin_hunter w1]` | ✓ |
| 9 | `rng()%2==0 ? dark_mage : void_walker` | 1 | 0→dm, 1→vw | `[dark_mage w1, void_walker w1]` | ✓ |
| 10 | `rng()%2==0 ? shadow_assassin : night_stalker` | 1 | 0→sa, 1→ns | 同构 | ✓ |
| 11 | `rng()%2==0 ? stone_guardian : iron_sentinel` | 1 | 0→sg, 1→is | 同构 | ✓ |

外层 `rng() % total` 与遍历逻辑未改，槽位内掷骰次数与取值映射逐位一致 → **`rng.draws` 计数与整条 RNG 流逐位相同**。

> 注：F1-5 / F11-15 的槽位 6 是本次最容易踩的坑 —— 若实现成「先算 total 再统一掷骰」，会多消耗 1 次 `rng()`，此后整条流错位。单候选短路是刻意设计，不得优化掉。

### 6.1 挑战房路径不需要 RNG 等价证明（已核实）

`_pick_monster_type(floor, wave_index, type_rng)` 的第 3 个参数**不是全局 `rng()`**。调用点 `challenge_room.cpp:213` 传入 `type_rng = wave_seed ^ (i * 7 + 13)`，`wave_seed` 由 `_deterministic_seed` 的 avalanche `hash_combine` 从 `dungeon_seed`/`room_index`/`wave_index` 派生 —— 整条挑战房刷怪链**确定性、不触碰全局 RNG 流**。

因此：
- 挑战房迁移的等价性判定只需**输出比对**（`roll % size` 语义一致），无需、也无法用 `rng.draws` 证明
- 这解释了为何批次8 的 9 池改动与本次 9 池数据化都不改变 sim sha —— 挑战房本来就在 RNG 流之外

---

## 7. 测试设计

### 7.1 Golden oracle 单元测试（新增，主力门禁）

新建 `tests/spawn_tables_test.cpp`，思路是**把旧硬编码逻辑原样复制成参考 oracle**，与新数据驱动路径逐骰比对。这是「行为不变」的最强证明，且不受 sim 在低楼层失明（批次8 已实证：agent 全死 F1-2，`avg_floor` 1.33）的限制。

```cpp
// 旧 floor_manager.cpp:25-38 的 switch，原样作为 oracle
static const char* legacy_pick_slot(int i, int floor, uint32_t r) {
    switch (i) {
        case 0:  return (r % 3 == 0) ? "orc" : "slime";
        case 1:  return "archer";
        case 2:  return "shaman";
        case 3:  return "bomber";
        case 4:  return "tank";
        case 5:  return "elite";
        case 6:  return (floor >= 6 && floor <= 10 && r % 2 == 0) ? "lightning_orb" : "charger";
        case 7:  return "summoner";
        case 8:  return (r % 2 == 0) ? "skeleton_archer" : "goblin_hunter";
        case 9:  return (r % 2 == 0) ? "dark_mage" : "void_walker";
        case 10: return (r % 2 == 0) ? "shadow_assassin" : "night_stalker";
        case 11: return (r % 2 == 0) ? "stone_guardian" : "iron_sentinel";
    }
    return "slime";
}
```

用例：
1. **`OracleMatchesNewPicker`**：固定种子播种 `rng`，对 15 层 × 每层 2000 次外层抽签，重放「外层 roll → 槽位 → 新 picker」与「外层 roll → 槽位 → `legacy_pick_slot`」，逐条比对 id 相等。
2. **`DrawCountMatches`**：同跑一遍，比对 `rng.draws` 增量与 oracle 期望（逐槽位按 §6 表统计），直接锁死「不多不少掷骰」。
3. **`ChallengeOracleMatches`**：对 3 群系 × 3 波 × 固定 roll 序列，比对新 `pick_challenge_monster` 与旧 9 池字面量的输出。
4. **`LightningOrbFloorGate`**：F6-10 内槽位 6 能产出 `lightning_orb`；F1-5 / F11-15 恒为 `charger` 且 `draws` 不增。
5. **`MissingSlotFallsBack`**：`get_spawn_slot(99)` 返回 null，`pick_slot_monster(99, 5)` 返回 null → 调用方落 `g_spawn_default`。
6. **`LoadFailureDoesNotSewSlimesSilently`**：加载器传不存在路径返回 false，且 `g_spawn_slots` 保持为空（不残留半成品）。

> 说明：这些用例直接验证「等价」而非「正确」—— 正确性由校验器（§7.2）与数据本身保证。这是回退/迁移类改动的正确测试姿态。

### 7.2 `tools/world_validator.py` 交叉校验

新增检查（复用既有 `enemies.json` 加载）：

| # | 规则 | 级别 |
|---|---|---|
| 1 | `enemy_slots.json` 每个候选 id 必须在 `enemies.json`，或是已声明别名 key | **error** |
| 2 | 每个别名目标必须在 `enemies.json` | **error** |
| 3 | `challenge_pools.json` 每个 id 必须在 `enemies.json` 或已声明别名 | **error** |
| 4 | `slots` 数量必须 == 12（与 `FloorConfig::enemy_weights[12]` 对齐） | **error** |
| 5 | **反向可达性**：`enemies.json` 每个 enemy id 必须能从 `enemy_slots` 或 `challenge_pools` 到达（别名展开） | **error** |
| 6 | 仅经挑战房可达（需钥匙解锁）的 enemy id → 列出清单 | **info** |
| 7 | `biomes.json` 的 `enemy_pool` id 必须是真敌人 | **error** |

规则 5 就是能自动抓出 lightning_orb 类缺口的检查；规则 6 把「11 只怪需钥匙才见」从口耳相传变成每次校验都可见的清单。

> 注意：批次8 已使全部 30 只怪可达，规则 5 当前应通过。规则 6 当前应列出约 12 只（bone_soldier / shadow_stalker / blood_leech / summoner / frost_slime / fire_imp / poison_wyrm / golem / necromancer / storm_elemental / ice_warden / blood_priest）。

### 7.3 既有测试不动

68 项 ctest 全部保留；新增文件只增不改。

---

## 8. 失败模式

| 场景 | 行为 |
|---|---|
| JSON 文件缺失 / 损坏 | 加载器 `return false` 并 `printf` 报错；registry 为空；`pick_*` 返回 null；调用方落 `g_spawn_default`（`"slime"`）。**不崩溃、不静默**，但全图刷史莱姆 —— 需靠启动日志与校验器发现 |
| 槽位序号越界 | `get_spawn_slot` 返回 null → 兜底 |
| 某槽位过滤后无候选 | 返回 null → 兜底 |
| 挑战房波次越界 | 返回 null → 兜底（旧代码此处是下标越界 UB，新实现反而更稳） |

**决策**：加载失败不做「回退到编译期硬编码」。理由：本批的目的就是消灭硬编码，留一份内建副本 = 留一个漂移源；失败应响亮地被日志和校验器抓住。

---

## 9. 门禁

1. `cmake --build build --config Release -- -j 4` → 0 error
2. `ctest --test-dir build` → **68 + 6 = 74 全通过**（新增 `spawn_tables_test`）
3. `python tools/world_validator.py` → 0 error；info 段应出现规则 6 的清单
4. 零回归断言：`enemies.json`、`floor_config.json`、`biomes.json`、`floor_config.cpp`、`FLOORS[15]`、`FloorConfig` 结构体 **一律不改**；召唤路径 5 处字面量不改；`spawn_monster` 不改
5. 静态接线证明：`grep -rn '"elite"\|"charger"\|"slime"' src/game/systems/floor_manager.cpp src/game/world/challenge_room.cpp` 应只剩兜底字面量
6. 实机验收（用户门禁）：2D + HD2D 跑通，怪物构成与批次8 基线观感一致

---

## 10. 遗留（本批明确不处理）

1. `floor_config.json` 激活 + 字段名统一 + 迁移 `FLOORS[15]` 与 `CHAPTERS` —— 候选项 D，另立批次（涉及 `FloorConfig` 的 `const char*` → `std::string`，波及所有消费方）
2. `EncounterChoice.risk/effect` 只加载不执行
3. `biome_events.json` 无加载器
4. `world/*.json` 与 `biomes.json` 双源重复（`world/*` 零 C++ 消费者）
5. `biome.enemy_pool` 的语义定稿 —— 当前是群系代表性名册，非驱动源；若日后要让它承重，需先解决与 12 槽位模型（3 群系 vs 15 层）的粒度错配
6. 召唤路径 5 处字面量（`boss.cpp:119-120`、`ai.cpp:757-758`、`ai.cpp:845-846`、`boss_command.cpp:107`、`skill.cpp:706`）—— 属行为设计，非选怪表
