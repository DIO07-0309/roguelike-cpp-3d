# B4 · 挑战房隐藏 Boss（远古魔像 GOLEM）设计

- 日期：2026-09-25
- 路线：v1.6+ Line B（玩法纵深）B4「第 6 Boss」
- 状态：设计已拍板；评审中追加 §2.7（Boss 归属与奖励隔离）——发现 `game_scene_combat.cpp:102` 无楼层保护的真实漏洞
- 基线：代码基线 HEAD `bb8cc21`（批次9 刷怪表数据化已完成）；spec 提交 `bc633f8`

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
| `floor` | 10 | **纯信息字段**：`get_boss_def_for_floor` 是硬编码 switch（`boss_defs.cpp:170-176`，5→shadow_knight / 10→fire_demon / 15→demon_lord），**不扫描 JSON 的 `floor`** → 填 10 不会与 fire_demon 撞车；数值缩放走 factory 实参 `floor`（`boss.cpp:1200`），也不读此字段 |
| `is_defender` | `true` | 触发 DEFEND 机制 |
| `is_summoner` | `false` | 不召唤（技能里不放 summon，见下） |
| `shield_pct` | `0.50` | DEFEND 时减伤 50%（`boss.cpp:939`：`>0` 则用此值，否则默认 0.70） |
| `skill_cycle_bias` | 6 | 通用节律 |
| `behavior_type` | **不填** | 加载器默认 `""`（`boss_defs.cpp:61`）→ 不进领域/镜像；且挑战房路径永不触发 `init_on_spawn`，`_behavior_type` 恒空（§2.7②） |
| `skills` | `[charge, shockwave, barrage]` | **数组下标 1 必须是 shockwave**——`boss.cpp:790-796` 按 `sk`（即 skills 下标）分发 `sk==0→_charge` / `sk==1→_shockwave`，`:787` 的 DEFEND 覆写写死 `sk == 1` 后改置 `sk = 3`（DEFEND）。另注：`barrage` 的 `range` 字段经 `boss.cpp:1257` 映射为 `spread_deg`（扇形角度），非射程 |
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

`_spawn_wave`（`:160`）：`wave_index >= _total_waves` 时走 boss 分支，否则维持现有 `spawn_monster` 循环。

**spawn 细节（已核实的接口约定）**：

| 项 | 事实 | 位置 |
|---|---|---|
| 坐标单位 | `spawn_monster` 取**像素**（`tile_to_pixel`），`boss_factory_create` 取 **tile**（内部 `*TILE_SIZE`）→ boss 分支直接传 tile，不做像素换算 | `boss.cpp:1203` |
| 参数 5/6 | `out_monsters` 与 `map` 当前**均未使用**（`(void)out_monsters; (void)map;`）→ 用默认值省略即可；类型不匹配（`vector<Monster*>*` vs `vector<unique_ptr<Monster>>&`）不构成问题 | `boss.cpp:1285` |
| 楼层缩放 | `boss_factory_create` **内部已乘** `boss_hp_scale`/`boss_atk_scale` → 压轴波**不得**再套 `gc.monster_hp` / `mod.hp_multiplier`，否则双份放大 | `boss.cpp:1200-1201` |
| 生成位置 | 房间中心 tile（`_room_rx + _room_rw/2`），避免小房间内 48×48 boss 贴墙 | `challenge_room.cpp:221` |
| 存活计数 | COMBAT 计数用 `rect.x / 32` 算 tile 再 `_room_contains`；boss 的 `entity.rect` 由 factory 置 `{x,y,48,48}`，落房中心必然命中 → **计数零改动** | `boss.cpp:1208` / `challenge_room.cpp:127-128` |
| 房间尺寸 | 普通挑战房 = 生成房间（≥ `_min_room`）；`CHALLENGE_ARENA` = **15×15**（房间 13×13）→ 均够放 48×48 boss | `game_scene.cpp:3652` |

```cpp
Monster* b = boss_factory_create(BossType::GOLEM, cx, cy, floor);   // is_boss 保持 true
monsters.emplace_back(b);
_monsters_alive_this_wave++;
```

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

`_grant_rewards`（`:206-234`，现有签名 `_grant_rewards(Player& player, GameMap* map, int floor, std::vector<DroppedItem>& ground_items)`，见 `challenge_room.h:91-92`）现有：3 件物品（重试 5 次至 RARE+）+ 金币 `50 + floor*15`。

追加 `bool boss_cleared` 入参，调用处传 `_boss_wave_pending`。为真时额外：1 件物品（**重试 8 次至 EPIC+**）+ 50% 额外金币。`Rarity` 枚举上限 `LEGENDARY=3`（COMMON 0 / RARE 1 / EPIC 2 / LEGENDARY 3）。未触发压轴则 `boss_cleared` 为假，奖励完全不变。

**RNG 影响面**：`generate_random_item()` 消耗全局 `rng`（`item.cpp:188`）。但现有奖励路径本就调用它 3+ 次，全局流今天就在扰动。本任务的增量只发生在**压轴触发（25%）**那 1/4 的场次；另外 75% 场次的全局 RNG 流与现状**逐位一致**。批次9 的刷怪 oracle 测试用注入 rng、不经过奖励路径，不受影响。

## 2.7 Boss 归属与奖励隔离（关键，含一处必要的越界改动）

`is_boss = true` 由 `boss_factory_create` 置位（`boss.cpp:1206`），保留它换取完整 boss 体验。但 `is_boss` 同时被两条路捡到，必须分别核实：

**① `_get_boss()` 会捡起它（期望行为）**

`game_scene.cpp:2281` `_get_boss()` 遍历 `monsters` 找 `is_boss && is_alive` → GOLEM 自动成为「当前 boss」，`BossSystemDirector::tick`（`game_scene.cpp:833`）每帧驱动它。得到：behavior 决策、evolution 成长、encounter 阶段递进（`bai->set_encounter_phase`）、boss HUD 指令标签。`notify_death`（`boss_system_director.cpp:604-612`）**只写 director 内部统计**（`replay_mem`/`encounter`/`battle_report`/`cinematic`/`timeline`），`ws`/`rels`/`qm` 全 `(void)` → 对 GOLEM 无害。

**② Arena / 领域不会误伤（自动安全，原风险已排除）**

`_arena_cfg` 与 `_behavior_type` 是 director **成员**，仅在 `init_on_spawn`（`boss_system_director.cpp:122-123` / `:148`）赋值，而 `init_on_spawn` **只从 boss 层入场调用**（`game_scene_input.cpp:136`）。挑战房路径永不触发它。更关键：`reset_floor()` 把 `_arena_cfg` 清 `nullptr`（`:53`），且经 `FLOOR_ENTER` 事件订阅（`:617-618`）+ `enter_floor`（`game_scene.cpp:391`）在**每层入场自动调用**。

→ 结论：**压轴 GOLEM 不生成 arena zone、不进入领域/镜像**。小房间压迫风险不存在，`arena.danger_type` 字段对本路径无效（仍可填合法值以保 validator 通过）。

**③ Boss 死亡奖励路径必须隔离（真实漏洞）**

`game_scene_combat.cpp:102` `if (m->is_boss) {` **无楼层保护**。GOLEM 死后会走完整主线 Boss 奖励：
- `_drop_boss_reward(m)`（`game_scene.cpp:2287-2298`）→ `bf_idx = (current_floor==5)?0 : (current_floor==10)?1 : 2` → **非 5/10 层一律给 index 2 = `sword_legendary`（倚天剑，F15 终 boss 武器）**
- 随机圣遗物入背包（`:151-167`）
- 30% 回血 + 随机物品

即：F6-9 刷一次挑战房就能白拿终章武器与一件圣遗物。

**修法**：`game_scene_combat.cpp:102` 改为

```cpp
if (m->is_boss && is_boss_floor(_s.current_floor)) {
```

GOLEM 死亡即落回 `else` 常规掉落分支（`:175`，走 `LOOT_DROP_CHANCE`）。`is_boss_floor` 是既有 public helper（`config.h:88`），且**仅在 F5/F10/F15 为真**，与「boss 奖励本就按 boss 层发」的原意一致。`Boss1/2/3_Defeated` 世界旗本已各自 gate 在 `current_floor==5/10/15`，不受影响。

此改动是本批唯一越出 `challenge_room.*` 的编辑，属必要修复而非顺带重构。

## 3. 改动清单

| 文件 | 动作 |
|---|---|
| `resources/bosses.json` | 追加 `golem` def（唯一数据新增） |
| `src/game/world/challenge_room.h` | `ChallengeRoomController` 加 `_boss_wave_decided` / `_boss_wave_pending` / `has_boss_wave()` |
| `src/game/world/challenge_room.cpp` | 压轴判定 + 压轴波 boss 刷出 + `_grant_rewards` 奖励叠加 |
| `src/game/scene/game_scene_combat.cpp` | **唯一越界改动**：`:102` `if (m->is_boss)` 加 `is_boss_floor(_s.current_floor)` 保护（§2.7③） |
| `tools/gen_boss_parts.py` | 加 golem 配置并生成分件/骨架 |
| `resources/animations/boss_golem_skeleton.json` | 新生成 |
| `resources/animations/actor_avatars.json` | 追加 `boss_golem` |
| `resources/sprites.json` | skeleton_parts +5 |
| `tests/` | 压轴判定确定性 + def 加载 + 奖励隔离回归 + validator 交叉 |
| `tools/world_validator.py` | `bosses.json` 技能 id 合法性、`BossType` 映射一致性 |

**不改**：`boss_defs.*`（字段已齐）、`boss.cpp`（行为全在，`is_boss`/缩放/技能覆写均由 factory 处理）、`boss_system_director.*`（`reset_floor` 已保证 arena 干净）、`growth_curve.*`、`floor_config.*`、`challenge_pools.json`、`game_scene.cpp`（`_get_boss()` 捡取 boss 是期望行为）。

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
| GOLEM 被 `_get_boss()` 自动捡起，误继承上一层残留的 arena / 领域配置 | `reset_floor()` 经 `FLOOR_ENTER` 事件**每层入场自动清空** `_arena_cfg` / `_behavior_type`，`init_on_spawn` 仅在 boss 层入场调用 → 压轴 GOLEM **不生成 arena zone、不进领域/镜像**（§2.7② 逐行核实；原「小房间被 zone 压迫」风险实际不存在） |
| GOLEM 死亡触发主线 Boss 奖励（非 boss 层白拿 `sword_legendary` 倚天剑 + 随机圣遗物） | `game_scene_combat.cpp:102` 加 `is_boss_floor` 保护（§2.7③），落回常规掉落分支；加回归测试锁定 |
| `out_monsters` / `map` 参数类型不匹配，或召唤子怪污染压轴波存活计数 | 两参数当前 `(void)` 未使用，用默认值省略；GOLEM `is_summoner: false` 且技能表不含 summon —— 双保险 |
| `boss_anim.json` 的 `torso y` bind 与 golem 骨架不匹配 | 生成时按批次7 的 rig 契约调 bind；实机看姿态 |
| 奖励守卫改动影响既有 boss 层流程 | 守卫条件在 F5/F10/F15 恒真 → 主线行为零变化；`Boss1/2/3_Defeated` 本就各自 gate `current_floor`，不受影响 |
| F15 挑战房压轴 GOLEM 明显弱于终 boss，玩家预期落空 | 定位即「奖金遭遇」；F15 等效 3000/91 仍高于同层小怪数倍，非白给 |
| 路线图 B 线红线「每批先跑 500 局基线对比」参考价值有限 | sim 胜率 0%、agent 死在 F1-2，基线以实机手感为准 |

## 6. 范围外

- 不把 GOLEM 加入任何主线楼层（F10 保持 fire_demon，不撤销 G10 决定）
- 不做「整间 Boss 房」变体（后续可选，需房间生成期决定类型）
- 不改挑战房小怪波数与刷怪池（`challenge_pools.json` 不动）
- 不新增 arena `danger_type`（用现有 `shadow_wall`）
- 不改 `boss_defs.h` / `boss.cpp`——字段与行为均已就绪。**Boss 系统本体零改动**；全批唯一的逻辑层改动是 §2.7③ 那行奖励守卫
