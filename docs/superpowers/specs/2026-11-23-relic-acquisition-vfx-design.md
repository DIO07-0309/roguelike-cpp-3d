# 圣物获得演出（Relic Acquisition VFX）设计规范

> 日期：2026-11-23 · 状态：已实现并验收 · 用户确认方向：**只做获得时视觉演出，不实现圣物玩法效果**
> 覆盖范围：表现层内部 hook，**所有获得路径**均播放（用户 2026-11-23 拍板）

## 一、意图与边界

玩家获得圣物时，应通过一次性视觉演出感受到该圣物的独特主题（冰霜/雷电/召唤…），而非所有圣物共用一种粒子。

### Goals
- 31 个目标圣物各有**独立、可测试**的获得演出，体现类别主题 + 个体身份。
- 演出覆盖**所有获得路径**（Boss 掉落 / 宝箱 / 事件 / 任务 / 商店 / 读档）。
- 演出完全由真实 `add_relic` 调用驱动；表现层不反向修改任何游戏状态。

### Non-goals（本轮明确不做）
- **不实现**任何圣物玩法效果（审计确认：31 个圣物在代码中零触发逻辑）。
- **不虚构** `RELIC_TRIGGER`、冻结/复活/流血/连锁雷击等玩法事件。
- **不新增**任何玩法字段、存档字段、概率/冷却/伤害/持续时间。

### Anti-goals（做了就算失败）
- 表现层污染 gameplay `rng()` → sim 基线漂移。
- 所有圣物播放同一动画、或仅换色、或仅靠飘字区分。
- 演出遮挡玩家 / 敌人 / HUD 关键信息。
- 为 31 个圣物新建与 `VFXServer` 并行的特效框架。

## 二、审计事实（设计依据）

| 事实 | 证据 |
|---|---|
| `RELIC_GAIN` 只在 Boss 掉落 emit；其余路径直接 `add_relic`/`grant_relic` | `game_scene_combat.cpp:149`、`special_room.cpp:181`、`event_system.cpp:269`、`quest_manager.cpp:120`、`player_controller.cpp:605`、`save_manager.cpp:170` |
| `RELIC_GAIN` 现有订阅者会改玩法（攻击进化 3 件升级） | `attack_evolution.cpp:16-33`、`gameplay_system_director.cpp:51` |
| VFX 有图元 + recipe 分派 + 预设色，**图元用独立 `visual_rng`** | `vfx_server.h:33-69`、`vfx_server.cpp:62-112`、`vfx_recipes.json` |
| `_vfx_emit_table` **无 `pulse`**（现有 recipe 的 pulse 步骤被静默忽略） | `vfx_server.cpp:245-260` |
| 事件→队列→每帧 emit 的样板 | `game_scene.cpp:_process_element_events` `:1656-1719` |
| `GameEvent.str_val` 是 `const char*`，emit 处传局部 `string.c_str()` → 浅拷贝悬垂 | `event_types.h:103`、`game_scene_combat.cpp:150` |
| 存档仅 `rlc:id,scope`，无特效/触发字段 | `save_manager.cpp:156-172,406-419` |
| sim 不渲染 | 审计 C1：`src/core/sim/` 无 VFX 调用 |
| 31 个目标圣物零玩法实现 | 审计报告 A 节：全仓 id 零命中 |

**结论**：唯一全覆盖且零玩法副作用的入口 = `Player::add_relic` 末尾的**表现层 hook**（不经过 EventBus）。

## 三、实施方案

### 3.1 表现层 hook（核心机制）

新增 `src/game/systems/relic_vfx_hook.h/.cpp`——注册式函数指针桥：

```cpp
using RelicVfxHook = void (*)(const std::string& relic_id, float px, float py);
void set_relic_vfx_hook(RelicVfxHook hook);              // 渲染层 init 注册
void fire_relic_vfx_hook(const std::string& id, float px, float py); // 无 hook 时 no-op
```

- `Player::add_relic()` 末尾调用 `fire_relic_vfx_hook(id, 玩家中心x, 玩家中心y)`（深拷贝 id，非裸指针）。
- 调用发生在 `PersistenceScope` 分支之后，但 hook 只读 id/坐标，不改 scope/存档。
- **sim 构建不注册 hook** → `fire` 为空操作 → 基线不变；tests 默认无 hook 可链接。
- 不经过 EventBus → 不触发 `AttackEvolutionManager`、不改 run_stats。

### 3.2 队列薄层（单一职责）

`RelicAcquisitionVfx`（组合，非继承）：
- `install()`：向 hook 注册 `on_acquired` 回调。
- `on_acquired(id, px, py)`：把 `{id 字符串副本, px, py}` 入队。
- `process_queue()`：每帧取出，查映射表 → 取 recipe/accent/label → `VFXServer::play_recipe` + `spawn_label` → push 到 `active_effects`。
- 接入点：`GameScene` 主循环（与 `_tick_active_effects` 同频），不新建刷新路径。

### 3.3 数据：`resources/relic_acquisition_vfx.json`

```json
{
  "frozen_crystal": {
    "recipe": "relic_gain_frozen_crystal",
    "accent": [112, 223, 255],
    "label": "冰晶",
    "motif": "shard_growth",
    "future": { "event": "ATTACK_SLOW_APPLY", "status": "planned" }
  },
  ...  // 31 条
}
```

- `recipe`：**每圣物一条独立 recipe**（写入 `vfx_recipes.json`），不用 7 类共享 —— 差异化可测试。
- `accent`：个体强调色（RGB 数组）。**落地方式**：recipe 的 step 用现有 preset 名（字符串，不改 VFXStep schema）；薄层在 `play_recipe` 后对该批 effects 的颜色做 tint 混合 accent → 个体色生效，无需扩展 VFXServer。
- `motif`：个体母题的**文档标注**（体现在 recipe 的 step 组合里），非运行时字段。
- `future`：未来触发演出规划（§5），本轮不消费。

### 3.4 recipe 设计（31 条，全部用合法 kind）

合法 kind 仅 12 种：`ring/beam/bolt/lightning/explosion/shockwave/slash_arc/cone/smoke/spark/aura/flash`（分派表 `vfx_server.cpp:245-260`）。**不使用 `pulse`**（不存在的 kind，会被静默丢弃）。

7 类基底 + 个体参数示例：
- 冰：`ring`(2-3 层) + `spark`(冰屑) + `flash`(中心)
- 雷：`bolt`(垂直) + `lightning`(分支) + `flash`
- 流血：`ring`(扩散) + `smoke`(血雾) + `spark`
- 暗影/时间：`smoke` + `aura` + `spark`（时间向加 `ring` 刻度感）
- 防御/复活：`aura`(盾轮廓) + `ring`(多层) + `flash`
- 召唤：`ring`(法阵) + `spark` + `smoke`
- 火毒：`explosion` + `spark` + `smoke`

所有参数（半径/颜色/时长/数量）来自 JSON 静态值 + 玩家坐标。

### 3.5 确定性与存档保护

- **随机源核实**：VFX 图元用 `visual_rng` 独立流（`vfx_server.cpp:67-111`，RNG-002），**不吃 gameplay rng** → 全部 9 种图元可用。禁用的只有 `ParticleSystem::emit`（用 C `rand()`，`particle_system.cpp:35-44`）。
- hook 不写 `player` / `world_state` / 存档；不调 gameplay `rng()`。
- 验证：sim 基线 `GOLDEN-001/002/003` 逐字节不变；存档 `rlc:id,scope` 格式不变。

## 四、31 个圣物获得演出设计表

> 原规范描写「触发瞬间」。本轮转译为「获得那一刻的一次性预告仪式」：保留主题、颜色、阶段感与身份，不虚构触发事件。每个圣物 = 独立 recipe（基底 + 个体参数）+ accent + 飘字。

### 冰霜系（基底 ring+spark+flash，主色 #70DFFF）
| 圣物 | accent | motif（获得演出） |
|---|---|---|
| 冰晶 frozen_crystal | #70DFFF | 冰晶从无到有生长，脚下一圈霜环 |
| 冰川碎片 glacier_shard | #D9F7FF | 4~8 枚冰屑向外迸射后悬停 |
| 寒冬王冠 winter_crown | #287FAE | 冰环如冠冕层叠上升环绕玩家 |
| 绝对零度 absolute_zero | #FFFFFF | 三阶段：寒气聚集→冰封剪影→白光碎裂 |

### 雷电系（基底 bolt+lightning+flash，主色 #FFE66D）
| 圣物 | accent | motif |
|---|---|---|
| 风暴呼唤 storm_caller | #FFE66D | 垂直落雷劈在玩家脚前，地面残留电弧 |
| 避雷针 lightning_rod | #547DFF | 电弧沿玩家身体游走后聚于武器 |
| 天罚风暴 divine_storm | #A78BFA | 三道闪电从玩家向外扇形连到虚空 |

### 流血系（基底 ring+smoke+spark，主色 #C8102E）
| 圣物 | accent | motif |
|---|---|---|
| 猩红之刃 crimson_blade | #C8102E | 一道猩红斩击弧光掠过 |
| 血祭匕首 sacrificial_dagger | #720D24 | 脚下血色法阵升起，能量内聚 |
| 血疫 hemoplague | #FF334F | 血雾从中心向四周扩散 |

### 暗影/时间系（基底 smoke+aura+spark，暗紫 #44205F / 时间金 #FFD166）
| 圣物 | accent | motif |
|---|---|---|
| 暗影斗篷 shadow_cloak | #44205F | 剪影短暂虚化，暗紫粒子上飘 |
| 刺客印记 assassin_mark | #6357C8 | 暗紫匕首虚影浮现后消散 |
| 夜幕 night_veil | #14101D | 黑纱自地面包裹玩家再退去 |
| 时之石 chrono_stone | #FFD166 | 金色时间刻度环顺时针旋转一周 |
| 悖论宝珠 paradox_orb | #8D78FF | 金紫双环收缩后爆散 |
| 混沌骰子 chaos_dice | #C9A227 | 骰子虚影翻转定格（与时之石区分色） |
| 魔力羽毛笔 magic_quill | #E9F5FF | 笔痕光弧掠过，末尾绽出符文点 |

### 防御/复活系（基底 aura+ring+flash，神圣金 #FFE6A0）
| 圣物 | accent | motif |
|---|---|---|
| 神之手 god_hand | #FFE6A0 | 半透明金色手掌虚影压下后化粒子 |
| 堡垒之盾 bastion_shield | #76B9E8 | 蓝金盾牌轮廓在玩家周围展开 |
| 不屈 unyielding | #FF7138 | 红金脉冲 + 向上能量柱 |
| 钢铁意志 iron_determination | #9AA7B5 | 轮廓覆一层银灰金属高光 |
| 凤凰涅槃 phoenix_rebirth | #B33912 | 凤凰双翼虚影展开→火焰羽毛飞散 |
| 凤凰羽毛 phoenix_feather | #FFC038 | 单枚金橙羽毛旋转升起化火点 |

### 召唤系（基底 ring+spark+smoke，灵魂青 #79E8DB）
| 圣物 | accent | motif |
|---|---|---|
| 灵魂锁链 soul_chain | #79E8DB | 青色能量环串成锁链逐段亮起 |
| 亡灵之书 necromancer_tome | #9B7BCA | 暗紫书虚影翻开，符文升起 |
| 虫群女王 swarm_queen | #A3D977 | 绿色巢穴光点涌出后向外扩散 |
| 亡灵大军 spirit_army | #F4D58D | 地面浮现数个召唤法阵后齐亮 |

### 火焰/毒素系（基底 explosion+spark+smoke）
| 圣物 | accent | motif（本轮补齐，完整差异化） |
|---|---|---|
| 引燃之戒 ignition_ring | #FF7A24 | 燃烧指环虚影旋转，滴落火星 |
| 炼狱核心 inferno_core | #C93A20 | 核心蓄势→火焰新星爆裂 |
| 毒脉 venom_vein | #82D94F | 荧光绿脉络沿地面蔓延 |
| 瘟疫之心 plague_heart | #C5FF63 | 毒心搏动向四周放出毒雾涟漪 |

> 除 accent 外，每个 recipe 的 step 组合/半径/层数/数量均不同（如绝对零度 3 段 vs 风暴呼唤 2 段），保证「31 条 recipe 两两不同」可测试。

## 五、未来触发演出待接入清单（本轮不实现）

| 圣物 | 未来需要的玩法事件（当前不存在） | 规划演出 |
|---|---|---|
| 绝对零度 | 冻结秒杀判定 | 蓄势→冰封→碎裂 |
| 凤凰羽毛/神之手/凤凰涅槃/不屈 | `PLAYER_DEAD`/致命伤判定 | 保命/复活演出 |
| 风暴呼唤/天罚风暴 | 落雷计时器/感电连锁 | 天降雷击/动态连锁电网 |
| 血疫/猩红之刃 | 流血传染/结算 | 血雾传播/伤口脉冲 |
| 暗影斗篷/夜幕 | 进层隐身/残血面纱 | 虚化/黑纱包裹 |
| 亡灵之书/亡灵大军/虫群女王 | 召唤单位生成 | 法阵召唤/虫群涌出 |
| 火毒四件 | 元素触发（部分 `ELEMENT_*` 已存在） | 火焰新星/毒雾扩散 |

接入原则：未来玩法落地时复用本轮同名 recipe / motif，只加玩法 emit，不重造特效。

## 六、分阶段实施（每片构建 + 测试）

1. **切片 A — 数据与 recipe**：`relic_acquisition_vfx.json` 31 条 + `vfx_recipes.json` 31 条 recipe；JSON 引用完整性测试。
2. **切片 B — hook 与薄层**：`relic_vfx_hook` + `RelicAcquisitionVfx` + `Player::add_relic` 调用 + 接入主循环；构建 + hook no-op 测试。
3. **切片 C — 全量演出**：31 条映射全部生效；视觉抽查 + 遮挡检查。
4. **切片 D — 验证**：ctest 全量、sim 基线逐字节、存档兼容性、负向测试（sim 下 hook 不触发玩法变化）。

## 七、约束与验收

- 函数 ≤40 行、单一职责、组合优于继承、语义命名（CLAUDE.md）。
- 不修改编译标志；不新增第三方库；不碰 2D 初代冻结包；桌面 3D 包同步。
- JSON 字段 snake_case；每个 `.h` 有 `#pragma once`。
- 验收：31/31 映射齐全且 recipe 两两不同、sim 基线不变、存档不变、原测试全绿。

## 八、本轮设计决定（builder 审查后，供复核）

1. **hook 而非 EventBus**（用户拍板）：全覆盖且零玩法副作用。
2. **31 条独立 recipe**：差异化可测试；不用 7 类共享。
3. **`pulse`→`ring`**：分派表无 pulse；顺带建议（不本轮做）修现有 recipe 与 validator 的 pulse 误判。
4. **`visual_rng` 允许**：独立流不吃 gameplay rng；仅禁用 `ParticleSystem`（C `rand()`）。
5. **深拷贝 id**：hook 传 `const std::string&`，不复用 `GameEvent` 裸 `str_val`。
6. **玩家实时坐标**：从 `Player` 传 hook，非固定屏幕常量。
7. **测试归属**：扩展 `world_validator.py`（JSON 静态）+ 新增 `relic_acquisition_vfx_test`（运行时/确定性）。
8. **范围=31**：其余 29 个已实现圣物走默认 fallback，不在本轮。
