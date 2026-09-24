# A6-S2 批次8：lightning_orb 运行时刷出路径 — 设计文档

- 日期：2026-09-24
- 状态：已批准（方案 A 全量：火山层 slot6 轮换 + 挑战房火山池 + biome.cpp 附带修复；接受 sim 基准变动）
- 关联提交：批次5 `6fcdccd`（enemies 30/30，含 lightning_orb 骨骼化）、批次6 `7bbf799`（NPC 10/10）、批次7 `9dd97c8`（5 Boss）
- 基线：HEAD `9dd97c8`，工作树 clean

## 背景与范围

批次3 已把 `lightning_orb`（电光之核）骨骼化，批次5 又把它写进 `biomes.json:39` 与 `world/volcano.json:14` 的 `enemy_pool`。本批清偿的正是批次5 明确留下的尾巴：**「运行时刷出路径仍无」**。

调研发现该缺口比预想更深：`enemy_pool` 与整个 `resources/world/*.json` 都是**死数据**，C++ 零消费者。真正的选怪决策是 C++ 硬编码字面量 + 编译期常量权重表。所以「把怪加进 JSON 池」这个批次5 的做法根本不产生任何运行时效果。

### 已核实现状

- **唯一生成工厂** `spawn_monster(px,py,type)`（`src/game/entities/monster.cpp:413`）：`get_enemy_def(lookup)` 全量查 `enemies.json`，本身不做「选谁」的决策。
- **主路径（常规层）** `GameScene::enter_floor` → `FloorManager::spawn_floor_monsters`（`floor_manager.cpp:71`）→ `_try_place_monster`（`:47`）→ `_pick_monster_type(cfg)`（`floor_manager.cpp:13-42`）：12 个类型槽位，每槽位返回**硬编码 id 字面量**，槽位 8/9/10/11 用 `(rng()%2==0)?A:B` 二选一。
- **权重来源不是 JSON**：`FloorConfig::enemy_weights[12]`（`floor_config.h:24`）来自**编译期常量表** `FLOORS[15]`（`floor_config.cpp:8-29`）。
- **挑战房** `ChallengeRoomController::_pick_monster_type`（`challenge_room.cpp:15-50`）：按 floor 分 biome、按 wave 分难度，**唯一真正的 biome 过滤刷怪**，同样硬编码；`Pool::types[4]`（`:19`）容量 4。
- **biome 归属**：`floor_config.cpp:10` 注释与 `bgm` 字段一致 —— F1-5 `prison`、**F6-10 `volcano`**（`floor_config.cpp:19-23`）、F11-15 `abyss`。
- **`BiomeDef::enemy_pool` 确认死代码**：全 `src/` 仅 3 处引用 —— `biome.h:31` 声明、`biome.cpp:64` 解析、`tools/world_validator.py`（Python 校验器，非运行时）。
- **`resources/world/*.json` C++ 零加载**：与 `biomes.json` 是并行的重复双源（含 landmarks 双源），因为都没加载所以不存在覆盖关系。
- **可达性**：`enemies.json` 共 30 怪，**29 个可刷出，`lightning_orb` 是唯一不可生成者**（`src/` 无任何字面量引用它）。
- **lightning_orb 定义完整可用**：`enemies.json:326-350` —— `hp 18 / atk 9 / pdef 1 / mdef 3 / type "charger" / role "flank" / attack_type "magical" / speed 120 / attack_range 2.5 / skills [charge 4.0]`。`get_enemy_def("lightning_orb")` 必定返回有效指针。
- **渲染链路已就绪**：骨架 `mon_lightning_orb_skeleton.json` + 白名单键 `actor_avatars.json:67` + 5 分件 PNG + 配色 `monster.cpp:398`（`{250,230,110,255}`）全部就位。**唯一断点就是没人调用它。**
- **对比对象** `charger`（`enemies.json:251-273`）：`hp 40 / atk 10 / pdef 4 / speed 110 / attack_range 2.0`。lightning_orb 更脆但更快、攻击范围更大、走魔法攻击，`type` 同为 `"charger"`。

### 范围内

1. 常规层 F6-10（volcano）slot 6 轮换出 `lightning_orb`
2. 挑战房火山波 1 池加入 `lightning_orb`
3. `biome.cpp` `enemy_weights` 读取补 `contains` 保护（附带健壮性修复）

### 范围外（记录，另立设计）

- 让 `BiomeDef::enemy_pool` / `enemy_weights` 真正被 C++ 消费（数据驱动刷怪管线）
- `resources/world/*.json` 与 `biomes.json` 双源合并
- `resources/floor_config.json` 无加载器且字段已与 `FloorConfig` 分叉（JSON F1 `hp 1.00/monsters 5` vs C++ `1.0f/3`；JSON bgm 全 `dungeon`/`boss` vs C++ `prison`/`volcano`/`abyss`）
- `EncounterChoice.risk/effect` 只加载不执行（`encounter.cpp:45-46` 仅读入内存，无执行器）
- `resources/biome_events.json` 无 C++ 加载器
- `floor_config.h:12` 注释写 chapter `0-4`，实际数据只有 `0/1/2`
- `EncounterDef` 仅用于 30% 改 SECRET 房间（`dungeon_generator.cpp:202-209`）
- 挑战房需钥匙解锁（`challenge_room.cpp:92-99`）→ 11 个怪不解锁就整局不可见，这是设计取舍不是本批 bug
- 召唤类路径（`boss.cpp:119-120`、`ai.cpp:757-758`、`ai.cpp:845-846`、`boss_command.cpp:107`、`skill.cpp:706`）全部只出 orc/slime/archer —— 有意为之，不加
- 12 槽位扩成 13 槽（方案 B，已否决：触及编译期表，为一个怪不值）

## §1 常规层：volcano slot6 轮换

`floor_manager.cpp:32`：

```cpp
case 6: return (cfg.floor >= 6 && cfg.floor <= 10 && rng() % 2 == 0) ? "lightning_orb" : "charger";
```

- **为什么用 `cfg.floor` 而不是 biome 名**：`_pick_monster_type` 只收 `const FloorConfig&`（`floor_manager.cpp:13`），`cfg.floor` 与 `cfg.chapter` 都在结构体里，无需改签名。用 `bgm` 字符串比较属于拿音效字段当地理字段，不可维护。F6-10 与 `bgm "volcano"`、`chapter 1` 三者严格重合。
- **为什么复用 slot 6 而非新增槽位**：slot 6 的语义标签就是 `Charger`（`floor_config.h:24`），`lightning_orb` 的 `type` 字段就是 `"charger"`，语义严丝合缝；且沿用文件内 slots 8/9/10/11 现成的 `(rng()%2==0)?A:B` 惯例。
- **概率量级**：slot 6 权重 F6=3/F7=3/F8=4/F9=3/F10=3，占该层总权重约 2.7%~3.4%；减半后 lightning_orb 单怪刷出率约 **1.3%~1.7%**。可见但罕见，不改变层压力结构。
- **零回归保证**：`cfg.floor < 6 || > 10` 时表达式恒为 `"charger"`，F1-5/F11-15 行为**逐字节不变**。
- **函数长度**：`_pick_monster_type` 现 30 行（`:13-42`），改后仍 30 行，不越 40 行上限。

## §2 挑战房：火山波 1 池

`challenge_room.cpp:37`：

```cpp
{{"fire_imp", "bomber", "frost_slime", "lightning_orb"}, 4},
```

- `Pool::types[4]`（`:19`）容量正好 4，此前火山波 1 用 3 个，**加满不越界**。
- `count` 由 3 改 4；`pick` 用 `r % count`（`:22`），确定性种子 `_deterministic_seed`（`:53-62`）不变，仅分布域变化。
- 只在 `floor <= 10` 分支内（`:34`），Prison/Abyss 池**零改动**。
- 语义契合：挑战房火山波 1 主题是「火/爆炸/冷系」，lightning_orb 是电气浮灵，属同章元素系。

## §3 附带修复：biome.cpp 缺省保护

`biome.cpp:63-66` 现状：

```cpp
if (obj.contains("enemy_pool")) {
    for (auto& e : obj["enemy_pool"]) b.enemy_pool.push_back(e.get<std::string>());
    for (auto& w : obj["enemy_weights"]) b.enemy_weights.push_back(w.get<float>());
}
```

`enemy_weights` 的读取嵌套在 `contains("enemy_pool")` 里却**没有自己的 `contains` 判定**。nlohmann `operator[]` 对非 const `json` 缺键会构造空节点，随后 `.push_back` 对非数组抛 `type_error.304` → 被外层 `catch`（`:78`）吞掉 → **整个 `load_biome_defs` 返回 false** → 全 15 层 biome 的 `palette`/`ambient`/`bgm`/`boss_id` 全部失效。

改为：

```cpp
if (obj.contains("enemy_pool")) {
    for (auto& e : obj["enemy_pool"]) b.enemy_pool.push_back(e.get<std::string>());
}
if (obj.contains("enemy_weights")) {
    for (auto& w : obj["enemy_weights"]) b.enemy_weights.push_back(w.get<float>());
}
```

- 当前 3 个 biome 的 pool 与 weights 成对存在，**所以还没炸** —— 这是潜伏缺陷，下一次有人手改 JSON 加池就会全局崩。
- 与 §1/§2 的关系：批次5 往 `enemy_pool` 里加 `lightning_orb` 时，若哪个 biome 顺手漏了 weights，就会把整个地图刷掉而没人察觉。修掉它才是让「JSON 池」这个叙述不至于变成陷阱。
- 运行时行为**零变化**（当前数据两个 `contains` 都成立）。

## §4 测试与门禁

本批是**内容变更**而非视觉批次，因此**放弃「sim 逐字节一致」当零影响门禁**，改为下列门禁：

1. **构建**：`cmake --build build --config Release -- -j 4` 0 error
2. **ctest**：`ctest --test-dir build` → **68/68**（测试数不变；本批无新增断言）
3. **World Validator**：`conda run python tools/world_validator.py` → **0 错误 0 警告**
4. **sim A/B 对照**（替代逐字节一致）：
   - `--sim 12 --sim-seed 3` 跑修改前/后两次，比对 sha256 与行数
   - **必须变化**（slot6 与火山池分布变了），但 diff 中**只允许出现怪物 id / 怪物名相关行**
   - **不得出现**：崩溃、`NaN`、异常打印、楼层推进中断、行数异常增减
   - 同时确认 `src/game/world/biome.cpp` 修改后仍打印 `[Biome] Loaded 3 biomes`
5. **零回归断言**：`enemies.json` 不改、`floor_config.cpp` 权重表不改、`biomes.json`/`world/*.json` 不改 —— 本批**零 JSON 改动**
6. **实机验收**（用户门禁）：F6-10 常规层跑几层，确认能刷出电光之核且骨骼/镜像/脚贴地正常；挑战房火山波 1 确认池内可见

## §5 提交策略

单次提交，覆盖 §1+§2+§3+README CHANGELOG：

```
feat(a6-s2): 批次8 - lightning_orb 运行时刷出路径
```

沿用批次6/7 约定：验证通过后同步桌面开发包 `C:\Users\HP\Desktop\Roguelike-CPP-3D版`（镜像 `src/ resources/ tools/ tests/ docs/ assets/ .github/ vendor/` + 5 根文件，exe 复制到包根目录；保留 `saves/` 与 `3D模式.exe.lnk`；不动初代版）。

## §6 改动量预估

| 文件 | 改动 | 行数 |
|---|---|---|
| `src/game/systems/floor_manager.cpp` | slot 6 换行 | +1 / -1 |
| `src/game/world/challenge_room.cpp` | 火山波 1 池加一项、count 3→4 | +1 / -1 |
| `src/game/world/biome.cpp` | 拆 `if` 补 `contains` | +2 / -1 |
| `README.md` | CHANGELOG 一行 | +1 |
| `docs/superpowers/specs/…` | 本设计文档 | 新增 |

**总计 4 个源/文档文件，约 4 行 C++ 实质改动。零 JSON、零资源、零新文件（除本文档）。**
