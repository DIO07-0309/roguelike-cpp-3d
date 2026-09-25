# B4 · 挑战房隐藏 Boss（远古魔像 GOLEM）设计

- 日期：2026-09-25
- 路线：v1.6+ Line B（玩法纵深）B4「第 6 Boss」
- 基线：HEAD `bb8cc21`（批次9 刷怪表数据化已完成）
- 状态：设计已拍板，待评审

## 1. 背景与目标

`V1_6_ROADMAP.md` 把 B4 列为「第 6 Boss（可选，数据驱动 bosses.json 直接加）」。立项调研发现这只 Boss **并非从零设计**——GOLEM 在代码里已 90% 建成，只差一条数据 def 和一条刷出路径。

### 调研关键发现

| 维度 | 状态 | 位置 |
|---|---|---|
| 枚举槽位 | ✅ 已建 | `boss.h:309` `GOLEM`（注释「远古魔像 (F10)」） |
| C++ 行为 | ✅ 已建 | `boss.cpp:780-781` Phase2 三连震；`:786-787` DEFEND 覆写（`sk==1`、12 步周期前半）；`:937-939` 减伤（默认 0.70） |
| 类型映射 | ✅ 已建 | `boss.cpp:1134` `case BossType::GOLEM: return "golem"`；`boss_defs.cpp:185` `get_boss_def_for_type(4)` → `"golem"` |
| 视觉色 | ✅ 已建 | `boss.cpp:1120` `vid=="golem" → {100,100,130,255}` |
| 属性管线 | ✅ 已建 | `boss_defs.h:62` `is_defender`、`:75` `shield_pct`（两处注释都写 Golem）；`boss.cpp:1282` `ai->golem_shield_pct = def->shield_pct` |
| 美术分件 | ✅ 已建（敌人版） | `mon_golem_skeleton.json` + 5 个 part PNG；`actor_avatars.json:55-58` 已登记 |
| **JSON def** | ❌ **缺失** | `bosses.json` 无 `golem` 条目 |
| **刷出路径** | ❌ **不可达** | `boss.cpp:1172-1173` F10 固定 `FIRE_DEMON`（注释 `G10: F10 is domain boss only`），GOLEM 不在任何池 |

缺 def 时的表现是安全降级：`boss.cpp:1184-1197` 查不到 def 就回落 shadow_knight，**不崩**。所以 GOLEM 是一只「建好了但永远刷不出来」的死 Boss。

### 目标

让 GOLEM 成为**挑战房的稀有隐藏压轴 Boss**——补上 data + 刷出路径，不触碰 15 层主线弧线。

## 2. 设计

### 2.1 触发与判定（隐藏机制）

挑战房现有状态机（`challenge_room.cpp`）：

```
INACTIVE →(钥匙) PORTAL_ACTIVE → ARMED →(锁门) WAVE_SPAWNING
  → COMBAT →(全灭) WAIT_NEXT_WAVE(3s) → WAVE_SPAWNING … 3 波后 → REWARD → CLEARED
```

新增：第 3 波清完后，按 `dungeon_seed` 派生判定，**25% 概率进入压轴波**（第 4 波，只刷 1 只 GOLEM）。压轴打完走正常 REWARD 并叠加 boss 额外奖励。

玩家视角：进房前完全不知有无 boss；既有 3 秒波间等待即为「boss 登场前奏」，不引入新演出系统。

### 2.2 GOLEM Boss def（`resources/bosses.json` 追加一条）

定位 **defender（盾）**：高 pdef、高 hp、中等 atk，靠 DEFEND 硬扛拉长时间。

| 字段 | 值 | 说明 |
|---|---|---|
| `id` / `visual_id` | `golem` | 匹配 `BossType::GOLEM` → `"golem"` 映射 |
| `name` | 远古魔像 | |
| `title` | 挑战房秘藏·远古魔像 | |
| `floor` | 10 | 仅为 `get_boss_def_for_floor` 兜底用；实际不出主线层 |
| `is_defender` | `true` | 触发 DEFEND 机制 |
| `is_summoner` | `false` | 不召唤（技能里不放 summon，见下） |
| `shield_pct` | `0.50` | DEFEND 时减伤 50%（`boss.cpp:939`：`>0` 则用此值，否则默认 0.70） |
| `skill_cycle_bias` | 6 | 通用节律 |
| `skills` | `charge` / `shockwave` / `barrage` | **index 1 必须是 shockwave**——`boss.cpp:787` 的 DEFEND 覆写写死在 `sk == 1` |
| `arena.danger_type` | `shadow_wall` | 通用默认，跨群系安全 |
| `phase2_hp_threshold` | 0.50 | 通用值；Phase2 三连震为 GOLEM 专属、已实现 |

基础数值 `hp 200 / atk 13 / pdef 14 / mdef 8`，经 `boss_factory_create` 的 `curve(floor)` 缩放：

| 楼层 | `boss_hp` | `boss_atk` | GOLEM 等效 | 同层主线 boss 等效 |
|---|---|---|---|---|
| F5 | 4.50 | 2.30 | 900 / 30 | shadow_knight 990 / 25 |
| F10 | 8.00 | 4.00 | 1600 / 52 | fire_demon 2400 / 64 |
| F15 | 15.0 | 7.00 | 3000 / 91 | demon_lord 9300 / 182 |

比同层主线 boss 更坦克、输出略低，符合 defender 定位；F15 明显弱于终 boss，符合「奖金遭遇」而非「终章」的定位。

参考：GOLEM 敌人本体 `enemies.json:499` 为 `hp 100 / atk 12 / pdef 12 / mdef 5`、tank/frontline、speed 35。

### 2.3 刷出路径（C++，`challenge_room.cpp`）

`_total_waves` 保持 3（小怪波数不变），压轴波占用**波次索引 3**（越界即 boss）。

`COMBAT` 全灭分支（`:134-147`）改为：

```
if (alive <= 0) {
    _current_wave++;
    if (_current_wave == _total_waves) {
        if (!_boss_wave_decided) {                 // 整场只判定一次
            _boss_wave_decided = true;
            _boss_wave_pending = has_boss_wave(dungeon_seed, room_index);
        }
        if (_boss_wave_pending) {                  // 3 秒等待 = boss 登场前奏
            _wave_timer = 3.0f;
            _phase = WAIT_NEXT_WAVE;
            return;
        }
    }
    if (_current_wave >= _total_waves) { REWARD; CLEARED; }
    else { _wave_timer = 3.0f; WAIT_NEXT_WAVE; }
}
```

判定写在**全灭分支内**而非 `on_doors_locked()`——因为 `on_doors_locked()` 没有 `dungeon_seed`/`room_index` 入参，只有 `tick()` 有。判定所需入参在 `tick()` 签名里齐备，无需改任何现有方法签名。

**波次追踪验证**（`_total_waves = 3`）：
- 波 0 清 → `_current_wave=1`，`1==3` 否，`1>=3` 否 → WAIT → 刷波 1
- 波 1 清 → `=2` → WAIT → 刷波 2
- 波 2 清 → `=3`，`3==3` 命中 → 判定；有压轴则 WAIT → 刷波 3（boss）
- boss 清 → `=4`，`4==3` 否，`4>=3` 是 → REWARD + CLEARED

`_spawn_wave`（`:160`）：`wave_index >= _total_waves` 时走 `boss_factory_create(BossType::GOLEM, tx, ty, floor, &monsters, map)`，否则维持现有 `spawn_monster` 循环。

**不动的**：
- `boss.cpp:1172-1173` `boss_type_for_floor` —— F10 仍固定 fire_demon，**不撤销** G10「domain boss only」的刻意决定
- COMBAT 存活计数（`:126-131`，`m->combat.is_alive`）对 boss 天然生效，无需改
- 既有刷怪池 `challenge_pools.json`（批次9 产物）不动
- 压轴波不给 boss 叠加挑战房 modifier（`mod.hp_multiplier 1.5`）——boss 走自身 `boss_hp/atk` 缩放，避免双份放大

### 2.4 判定与 RNG（确定性）

压轴判定走 `dungeon_seed` 派生，沿用既有 `_deterministic_seed(dungeon_seed, room_index, wave_index)` 模式，用**保留波次槽位**与真实波（0..3）区分，两个 salt 常量都进 avalanche 混合（`:27-32`）：

```cpp
static constexpr int kBossWaveSlot = 99;   // 保留槽位, 不与真实波 0..3 碰撞

bool has_boss_wave(uint32_t dungeon_seed, int room_index) const {
    uint32_t s = _deterministic_seed(dungeon_seed, room_index, kBossWaveSlot);
    return (int)(s % 100u) < 25;            // 25%
}
```

判定在首个「3 波全灭」时刻算一次并存 `_boss_wave_decided` / `_boss_wave_pending`，**整场挑战房结果固定，不逐波重摇**。

**关键：不消耗全局 `rng`**——与批次9 的 RNG 红线一致。挑战房现有刷怪本就走 `wave_seed ^ (i*7+13)` 派生（`challenge_room.cpp:186`），压轴判定同模式，全程确定性。

新增两个成员到 `ChallengeRoomController`：`bool _boss_wave_decided = false`、`bool _boss_wave_pending = false`，`reset()` 里一并复位。

### 2.5 美术（走推荐方案）

`tools/gen_boss_parts.py`（批次7 产物）加 golem 配置，生成 1.4× 大骨架：

- 产出 `assets/sprites/boss_golem_part_*.png`（5 件）+ `resources/animations/boss_golem_skeleton.json`
- 动画复用 `resources/animations/boss_anim.json`（批次7 的 `torso y` bind 对齐已为 boss 尺寸调好）
- `actor_avatars.json` 追加 `boss_golem` 键，`sprites.json` skeleton_parts 追加 5 条

拼键路径已验证：`boss.cpp:1213` `vkey = "boss_" + def->visual_id` → `boss_golem`。未登记则回落 `boss_f5`/`boss_f10` 静态图（`boss.cpp:1219`），造型会错，故不采用。

### 2.6 奖励

`_grant_rewards`（`:206-234`，现有签名 `_grant_rewards(Player&, GameMap&, int floor, std::vector<GroundItem>&)`）现有：3 件物品（重试 5 次至 RARE+）+ 金币 `50 + floor*15`。

追加 `bool boss_cleared` 入参，调用处传 `_boss_wave_pending`。为真时额外：1 件物品（**重试 8 次至 EPIC+**）+ 50% 额外金币。`Rarity` 枚举上限 `LEGENDARY=3`（COMMON 0 / RARE 1 / EPIC 2 / LEGENDARY 3）。未触发压轴则 `boss_cleared` 为假，奖励完全不变。

## 3. 改动清单

| 文件 | 动作 |
|---|---|
| `resources/bosses.json` | 追加 `golem` def（唯一数据新增） |
| `src/game/world/challenge_room.h` | `ChallengeRoomController` 加 `_boss_wave_pending` / `has_boss_wave()` |
| `src/game/world/challenge_room.cpp` | 压轴判定 + 压轴波 boss 刷出 + 奖励叠加 |
| `tools/gen_boss_parts.py` | 加 golem 配置并生成分件/骨架 |
| `resources/animations/boss_golem_skeleton.json` | 新生成 |
| `resources/animations/actor_avatars.json` | 追加 `boss_golem` |
| `resources/sprites.json` | skeleton_parts +5 |
| `tests/` | 压轴判定确定性 + def 加载 + validator 交叉 |
| `tools/world_validator.py` | `bosses.json` 技能 id 合法性、`BossType` 映射一致性 |

**不改**：`boss_defs.*`（字段已齐）、`boss.cpp`（行为全在）、`growth_curve.*`、`floor_config.*`、`challenge_pools.json`。

## 4. 门禁

1. `cmake --build build --config Release -- -j 4` → 0 error
2. `ctest --test-dir build` → 全绿（新增压轴判定测试）
3. `conda run python tools/world_validator.py` → 0 error / 0 warning
4. 美术一致性：分件 PNG ↔ `sprites.json` ↔ `actor_avatars` ↔ `bosses.json.visual_id` 四向对齐
5. sim：`--sim 12 --sim-seed 3` 记录 sha 变化（挑战房需钥匙 + 压轴 25%，F1-2 段大概率不触发；若触发则按批次5/8 先例如实记录为预期行为变更）
6. **实机验收**（用户门禁）：2D + HD2D 打一场有压轴的、一场没有的

## 5. 风险

| 风险 | 缓解 |
|---|---|
| 挑战房房间小，GOLEM arena zone 可能过于压迫 | 压轴 arena 用保守参数：`max_zones 3`、小 `spawn_radius`、较长 `zone_duration` |
| `boss_factory_create` 给 `out_monsters` 传 `&monsters` 时，boss 的召唤子怪会与压轴波存活计数交织 | GOLEM `is_summoner: false` 且技能表不含 summon，从源头避免 |
| `boss_anim.json` 的 `torso y` bind 与 golem 骨架不匹配 | 生成时按批次7 的 rig 契约调 bind；实机看姿态 |
| F15 挑战房压轴 GOLEM 明显弱于终 boss，玩家预期落空 | 定位即「奖金遭遇」；F15 等效 3000/91 仍高于同层小怪数倍，非白给 |
| 路线图 B 线红线「每批先跑 500 局基线对比」参考价值有限 | sim 胜率 0%、agent 死在 F1-2，基线以实机手感为准（已在方案中说明） |

## 6. 范围外

- 不把 GOLEM 加入任何主线楼层（F10 保持 fire_demon，不撤销 G10 决定）
- 不做「整间 Boss 房」变体（后续可选，需房间生成期决定类型）
- 不改挑战房小怪波数与刷怪池（`challenge_pools.json` 不动）
- 不新增 arena `danger_type`（用现有 `shadow_wall`）
- 不改 `boss_defs.h` / `boss.cpp`——字段与行为均已就绪，本批零逻辑层改动
