# P1-C7 — walkable 判定语义统一 (2026-09-22)

> 统一 `_tile_rect_walkable` / `_sim_tile_passable` / `is_rect_walkable` 三套 walkable
> 语义为**单一入口** `GameMap::is_passable_sim`, 消除决策层/执行层分歧遗留
> (G14 卡死修复链的剩余根因层).

- **新增统一入口** (`src/game/world/game_map.h/.cpp`)
  - `bool is_passable_sim(tx, ty)`: WALL/LOCKED/SEALED 不可走; FLOOR/STAIRS/LAVA/
    OPEN 门/CLOSED 门可走 (CLOSED 门 = Sim 自动开门语义, 执行层移动前接线
    `try_open_door_toward` 已保证)
- **sim_ai.cpp 收敛**: `_tile_rect_walkable` 改为薄包装 `is_passable_sim`;
  删除 `_sim_tile_passable` (原与前者 CLOSED/rect 语义分歧), 调用点合并
- **测试矩阵** (`tests/world/door_truth_table_test.cpp` 新增 `PassableSimMatrix`):
  6 种 tile 类型 + 门四态 + 越界, 锁定统一语义防漂移
- 门禁: Release 0 error · ctest 68/68 (含新矩阵) · validator 0/0
- 行为等价验证: 10 局 sim 结果与收敛前一致 (统一不改变既有正确语义)
- 遗留: STUCK_RECOVERED 4/10 (武器局卡死) — 下一轮专项

# G15 — sim 武器获取链修复: 拾取恢复 + 保底武器 (2026-09-22)

> 50 局基线定位: win=0% 主因已从"卡死"转变为"全程空手" (50 局 weapon 全 fist_basic,
> avg_damage_dealt 42.9 vs 承伤 231.7). 本轮修复拾取链 3 项, 输出 2.6 倍.

- **拾取永久放弃 bug** (`src/core/sim/sim_ai.cpp:_evaluate_pickup`)
  - 根因: G14b 拾取冷却块里 `_pickup_fail_streak++` 在**冷却期间每帧执行** —
    1.5s 内从 0 涨到 90 → `>=3` 永久放弃拾取 → **picks=0 全程空手**
  - 修复: streak 只在**真实再次尝试** (冷却已过仍见物品) 时 +1; 冷却中仅返回 0
  - 实测: 玩家背包从 0 → 3 件 (拾取恢复)
- **怪清光后空转** (`sim_ai.cpp:_evaluate_move`)
  - 原 `if (!t) return 0.1f` — 全图无怪时直接中性分, 尸体掉落/未搜房间全浪费
  - 修复: 无怪时先走 loot (0.7) / 房间 (0.6) 分支, 0.1 仅兜底
- **第 1 层保底武器** (`src/game/scenes/game_scene.cpp`)
  - 第 1 层出生房旁固定 1 把 `sword_common` (roguelike 教学化设计, 真玩家同受益)
  - 空手死亡螺旋: 空手局 kills 0-2 / 承伤 210 磨死; 拿武器局 kills 8 (sword) 爬 2 层

- 门禁: Release 0 error · ctest 68/68 · validator 0/0
- 效果 (10 局 seed21): avg_damage_dealt 10.7 → 27.7 (2.6x) · 武器局 1/10 → 3/10
  (sword_common/spear_epic/dagger_common) · TIMEOUT_WALL 归零
- 遗留: 拿武器局仍被卡死检测抓 4/10 → walkable 判定不统一 (P1-C7 专项, 同 G14 遗留);
  win_rate 仍 0 (AI 推进/平衡)

# G14 — sim AI 卡死修复链: 可达性判定统一 + 路径稳定化 (2026-09-22)

> 本轮定位 sim win_rate=0 的根因链, 修复 6 项真实缺陷并将 avg_floor 从 1.0 提升到 1.1~4.5 (随修复推进波动)。
> 剩余核心问题 (多套 walkable 判定语义不统一) 已记录为 P1-C7 专项。

- **致命 bug: OPEN 门被 BFS 判为障碍** (`src/core/sim/sim_ai.cpp:_tile_rect_walkable`)
  - 原 `if (ds != DoorState::NONE) return false;` 把 **OPEN 门也当不可走** —— BFS 永远过不了任何
    门 → 怪被"门隔离" → 楼层永清。与执行层 `is_rect_walkable` (OPEN=true) 完全脱节
  - 修复: OPEN 走 `is_rect_walkable` (true), 仅 LOCKED/SEALED 拦截, CLOSED 由 Sim 自动开放行
- **执行层接触开门接线** (`src/game/player_controller.cpp`)
  - `GameMap::try_open_door_toward` (R1 接触开门) 此前**从未被任何调用者使用** (死代码) →
    玩家朝 CLOSED 门移动被 `is_rect_walkable` 拒绝 → 卡门旁死循环 (门永不开启)
- **攻击射程/理想距离对齐武器真实射程** (`sim_ai.cpp`)
  - 原硬编码 48px / 2.5-1.5 格: crossbow 射程 10 格 (320px) 被当成近战武器 → AI 判"出圈"从不射击
  - 现用 `max(weapon.current_range(), 1.5f)`: 保底 48px 规避 P1-C5 已知收窄回归, 只修正远程
- **卡死行为链修复** (`sim_ai.cpp`)
  - 卡死 ≥2s: 近身 (≤1.5 格) 直接 attack / 否则 **BFS 定向朝怪** (原盲目旋转只让 AI 原地转圈)
  - 近身战豁免 (2 格内有怪 = 真实战斗, 卡死机制让位) — 避免 120s 看门狗误杀拉锯战
  - progressed 信号三源化 (怪HP/玩家HP/存活数, 容差 0.01) — 环境 tick 不再误判"战斗"
  - 传送兜底: 玩家传送失败 → **反向拉怪到玩家旁** (孤岛怪破除) → 再强开 3x3 CLOSED 门
- **传送落 CLOSED 门允许+落地即开** (`sim_ai.cpp` / `sim_ai_teleport.cpp`)
  - `_tile_valid_for_landing` 原 `is_walkable` 拦截 CLOSED 门 → 怪被门围时传送永败
- **sim 整格步进** (`player_controller.cpp`)
  - 连续移动 (speed×dt) 使玩家停在**半格位置** → rect 跨 2 tile, 执行层 `is_rect_walkable`
    与 BFS (中心 tile) 分裂 → "永远走不动"。整格步进 (0.14s/格, 撞墙还原) 后位置恒格点对齐,
    rect 28px 落单 tile → 决策/执行一致
- **拾取防死循环** (`sim_ai.cpp`)
  - `_evaluate_pickup` 2 格内给 1.6 分压过移动, 但物品捡不掉 → AI 每帧 pickup 死原地
  - 修复: 拾取尝试冷却 1.5s + 连续 3 次失败 → 本局放弃该拾取 (让位移动/战斗)
- **路径记忆等距锁定** (`sim_ai.cpp` ×2 处)
  - 原 `nd < d` 对对称等距双路径不锁定 → 玩家在等距格间往返震荡 (实测 tile 14↔16)
  - 修复: `nd <= d + ε` 容忍浮点等值, 路径记忆生效打破震荡
- **怪物生成排除门 tile** (`src/game/systems/floor_manager.cpp`)
  - 原只查 `is_walkable(tile)` → 怪可生成在 OPEN 门 tile, Room Encounter 门组 LOCKED 后怪被锁异常位
  - 修复: 排除 DOOR/LOCKED/SEALED + rect 级校验 + 拆 `_try_place_monster` 保持 ≤40 行
- **诊断基础设施** (保留, 结算时低频输出): [SIM-DIAG] 动作分布/移动分支/卡死采样/连通性报告

- 门禁: Release 0 error · **ctest 68/68** · validator 0 error / 0 warning
- 指标: avg_floor 1.0 → 1.1~4.5 (随版本), 卡死场景从"怪 600-1080px 远不可达"收敛到
  "怪 111-322px 近可达但路径震荡" (部分场景已修复)
- **遗留 TODO (P1-C7 专项)**: 代码库存在 3 套 walkable 语义
  `_tile_rect_walkable` / `is_rect_walkable` / `_sim_tile_passable`, 边界条件互有分歧
  (CLOSED 门/半格 rect/门状态), 导致决策层与执行层仍有缝隙。彻底方案: 统一
  `IsPassable(map, tile, rect)` 单一入口 + 扩展 `door_truth_table_test` 覆盖矩阵
- 遗留: win_rate 仍为 0 (AI 推进/平衡), 非本轮范围

# G13 — sim 卡死脱困死锁修复 + 卡死看门狗 (2026-09-22)

- **死锁修复** (`src/core/sim/sim_ai.cpp`)
  - 根因：卡死 ≥8s 后调用兜底传送，**成功与失败都无条件 `return "none"` 且不重置计时**。
    传送失败（最近怪隔墙不同房间、周围无可落格）→ 每帧返回"什么都不做"，直到烧满 36000 帧
  - 修复：传送失败时回落旋转脱困（原 `else if` 链改平铺 `if`），不再空转
  - 原 `sim_ai.cpp` 50 行卡死判定块从 `best_action()` 抽出为 `_stuck_escape` (38 行) +
    `_rotation_escape` (12 行)，`best_action` 只剩一行调用
- **卡死看门狗**（新信号 `sim_stuck_watchdog`，sim_ai → game_scene）
  - 用**本局累计卡死时长**而非"传送失败次数"：传送会周期性成功并清零计数，失败计数永远凑不满
    阈值（实测 seed21 五局全部因此漏报）
  - 阈值 `kStuckTotalBudget = 120s`（36000 帧预算约 600s 的 1/5）。实测 240s 反而更差
    （seed101 真实死亡 8→1），局跑得越久越容易在卡死中耗光预算
  - `game_scene.cpp:_process` 顶格检查；**不设** `_sim_wall_timeout`，结算分类自然落到
    末尾 else → `STUCK_RECOVERED`（原该分支永不可达，每次都 0）
- **跨层时间回绕修复** (`sim_ai.h:set_time`)
  - 实测发现 `_stuck_total` 出现 -99 / -244s 负值：`enter_floor` 把 `game_time` 归零，
    残留的 `_stuck_since` 变成"未来时间"，`_stuck_total` 被负数拉爆、看门狗判不成立
  - `set_time` 检测时间倒退即丢弃过期计时；累加处再 `std::max(0, ...)` 双保险
- **阈值调参可观测**：`DecisionAgent::stuck_total()` + `[SIM-DIAG]` 结算日志

- 门禁: Release 0 error · **ctest 68/68** · validator 0 error / 0 warning · `git diff --check` 干净
- **TIMEOUT_WALL 归零**：seed101 `TIMEOUT_WALL 6→0`（现 DEATH_DOT=5 / DEATH_MONSTER=3 / STUCK_RECOVERED=2），
  seed21 `TIMEOUT_WALL 5→0`（STUCK_RECOVERED=5）
- 遗留：win_rate 仍为 0，是 AI 推进能力/平衡问题，非本次范围
- 桌面包已同步

# G12 — 武器攻击路径函数拆分 (2026-09-22)

- **`WeaponExecutor::execute` 91 行 → 30 行编排器** (`src/game/systems/weapon_executor.cpp`)
  - 拆为 4 个单职责助手：`_try_stage3_special` 13 行（combo 第三段特殊技分发）/
    `_update_range_indicator` 6 行（射程指示器）/ `_resolve_normal` 25 行（弩弹道 vs 近战 SSOT 几何）/
    `_finalize_attack` 29 行（协同上下文 + 推进连段 + 事件 + 音频）
  - 纯机械拆分：守卫顺序、`_set_attack_context` 先于 `execute_attack`、事件类型映射、音效回退全部逐行保留
  - `_resolve_normal` 未用到 `map`，直接去掉参数而非留 `(void)map` 压警告
- **顺手清理**：`ai.cpp` 5 处 + `ai.h` 4 处尾随空格，`git diff --check` 归零
- **遗留说明**：`execute` 原为 90 行（HEAD 既有遗留，本次改动触及故顺带拆分）；
  `for_each` 复杂度为 O(capacity) 而非 O(存活数)，已写入 `object_pool.h` 注释，
  不适合接到"高容量、低占用"容器

- 门禁: Release 0 error · **ctest 68/68**（武器路径 69 用例：special 13 / weapon 15 / synergy 18 / action 7 / projectile 16）
  · validator 0 error / 0 warning · `git diff --check` 干净 · `--sim 5` exit=0
- 桌面包已同步

# G11 — 对象池接入弹体 (2026-09-22)

- **ObjectPool 重构** (`src/game/systems/object_pool.h`)
  - 索引槽位设计取代裸指针借出；槽位用 `deque` 存储，`push_back` 不失效既有元素地址，
    故 `acquire()` 返回的 `T*` 跨扩容天然有效（旧实现用 `vector`，扩容后全部悬垂）
  - `release` 重置槽位为默认状态，杜绝跨生命周期残留数据；`clear` 回收不缩容
  - `acquire` / `release` 均 O(1)；空闲索引栈复用槽位
  - API: `acquire` / `insert` / `release` / `clear` / `for_each` / `release_if` / `at` / `size` / `capacity` / `idle`
- **弹体接入** (`GameScene::projectiles` → `Game::ObjectPool<Projectile>`)
  - 每帧 `erase(remove_if)` 的 O(n) 元素搬移 → `release_if` 逐槽位 O(1) 回收
  - 遍历点全部改 `for_each`：玩家弹体 tick (`tick_projectiles`) / 敌方弹体 tick / 2D 绘制 / 3D `_build_projectiles`
  - `Monster::projectiles_ptr` 与 `WeaponExecutor` 相关签名同步改池类型，`push_back` → `insert`
  - `_build_projectiles` 顺带拆为 4 个 ≤40 行函数（`_build_projectiles` 8 / `_build_warning_item` 20 / `_build_trajectory_item` 21 / `_build_active_item` 12）
- **单测** `tests/systems/object_pool_test.cpp`（9 用例：值写入 / 回收重置 / 槽位复用 / 幂等与越界 / 条件回收 / 跳过空闲 / clear / 扩容安全回归 / 真实弹体集成）

- 门禁: Release 0 error · **ctest 68/68** · validator 0 error / 0 warning · `--sim 3` exit=0
- 桌面包已同步

# G5.5 — 怪物 AI 与渲染性能优化 (2026-09-22)

- **怪物记忆系统** (`src/game/entities/ai.h` / `ai.cpp`)
  - `MonsterAI` 新增 `memory_x` / `memory_y` / `memory_timer` + `update_memory` / `memory_valid` / `tick_memory`
  - `_execute_chase`：玩家在视野内时刷新记忆（持续 3 秒），离开视野后向最后已知位置推进
- **普攻模式多样化** (`MonsterAttackPattern`)
  - 5 种模式：`basic` / `double_strike` / `cleave` / `lunge` / `spread`
  - 数据驱动：`EnemyDef::attack_pattern_str` ← `enemies.json` 的 `attack_pattern` 字段
  - 类型回退：tank→cleave、elite→double_strike、charger→lunge、射程≥3 的 summoner→spread
  - `Monster::attack_target` 增加 `damage_mult`（默认 1.0）承载 cleave 1.5x
  - 守卫：近战怪（`attack_range < 3.0`）不授予 spread，回退 basic，避免模式静默失效
  - `_execute_attack` 拆为分发器 + 5 个 ≤40 行执行器；攻击事件与特效抽为 `_emit_monster_attack` / `_push_melee_attack_feedback` / `_push_cast_vfx`
- **Boss AI 阶段变化增强** (`src/game/entities/boss.cpp`)
  - `_tick_phase2_behaviors`：二阶段狂暴脉冲（每 6s，近身 96px 内击退 30px + 0.5x 真实伤害），间隔随 `_phase2_elapsed` 由 6s 收敛至 4s 下限
  - `_enter_last_stand`：HP<25% 一次性强化（攻速×0.7 / 移速×1.2 / 攻击×1.15 / 体型 56px + 红色双环演出）
- **FX 粒子单批渲染** (`src/game/rendering3d/hd2d_renderer.cpp`)
  - `_draw_fx_particle`（逐颗 8 次 `DrawSphere`）→ `_draw_fx_particles_batch`（相机朝向 additive quad，全帧 1 个 rlgl 批）
  - 视觉构成保持不变：核心 + 外发光 + 4 环绕 + 2 外层光晕，轨道数学逐项对应
  - `_fx_emit_quad` / `_fx_emit_particle` 静态助手（匿名命名空间）
  - `_draw_fx_pass`：FX 五段绘制循环（粒子单批 + 4 类逐项）抽为独立函数，`_draw_scene` 47→39 行，绘制顺序与裁剪条件不变
- **视锥体裁剪落地**
  - `_fx_in_view`：FX_QUAD / FX_RING_3D / FX_EXPLOSION_3D 按相机焦点 ±`kFxCullMargin`(220) 世界单位裁剪
  - `_fx_beam_in_view`：光束双端判定，避免起点屏外/终点屏内的长束被误裁
- **天气粒子** `WeatherSystem::init` 300→150
- **对象池基础设施**：新增 `src/game/systems/object_pool.h`（模板池，G11 已接入弹体）

- 门禁: Release 0 error · **ctest 68/68** · validator 0 error / 0 warning
- 桌面包已同步

# A9 — 攻击特效优化 v1 (2026-09-21)

> 设计 spec: docs/superpowers/specs/2026-09-21-a9-attack-vfx-design.md
> 实施计划: docs/superpowers/plans/2026-09-21-a9-attack-vfx.md

- **粒子系统** (T1): 512 粒子池 + EmitterConfig + 发射/更新/绘制/清空
  - `ParticleSystem` 类：最大 512 粒子，发射器配置（速度/颜色/时长/大小）
  - `tests/vfx/vfx_test.cpp` 新增 4 个测试用例
- **武器特效增强** (T2): 5 类武器 × 3 段连击 = 15 种特效
  - 剑（扇形斩）：DrawRing 多层弧线 + 粒子拖尾
  - 矛（穿透）：直线光束 + 命中爆炸粒子
  - 双截棍（追踪）：多层弧线 + 残影效果
  - 弩（弹幕）：多点散射 + 命中粒子
  - 锤（重击）：冲击波 + 地面裂纹粒子
  - 新增 15 个 `_draw_*` 函数到 `game_renderer.cpp`
- **打击感组合** (T3): 闪白 + 震屏分级 + 飘字颜色
  - `HitFlash` 类：命中闪白效果（0.1s 默认时长，颜色叠加）
  - `tests/vfx/vfx_test.cpp` 新增 3 个 HitFlash 测试用例
- **Shader 效果** (T4): 发光/模糊/扭曲
  - `VFXShader` 类：3 种效果类型（BLOOM/MOTION_BLUR/SCREEN_WARP）
  - Fallback 机制：shader 未加载时使用程序绘制
  - `tests/vfx/vfx_test.cpp` 新增 4 个 VFXShader 测试用例

- 门禁: Release 0 error · **ctest 67/67** (新增 vfx_test 14 用例) · validator 0/0
- 桌面包已同步

# A8 — 战斗 HUD 优化 v1 (2026-09-21)

> 设计 spec: docs/superpowers/specs/2026-09-21-a8-combat-hud-design.md
> 实施计划: docs/superpowers/plans/2026-09-21-a8-combat-hud.md

- **HP/XP bar 视觉升级** (T1): 像素风双层边框 + 高光顶线 + 动态颜色（绿→黄→红）
- **技能栏冷却提示** (T2): 图标旋转 + 数字倒计时 + 升级标识
- **小地图标记清晰度** (T3): 标记颜色区分（怪物红/Boss 金/楼梯蓝/物品绿）+ 标记大小分级
- **金币/资源图标** (T4): 像素图标 + 数字对齐 + 圣物数量显示

- 门禁: Release 0 error · **ctest 66/66** (新增 hud_test) · validator 0/0
- 桌面包已同步

# A6 — 摄像机语言 v1 (2026-09-20)

> 设计 spec: docs/superpowers/specs/2026-09-20-a6-camera-language-design.md
> 实施计划: docs/superpowers/plans/2026-09-20-a6-camera-language.md

- **数据层** (T1): `resources/camera/boss_camera.json` + `camera_defs.h/.cpp` 加载器
  - Boss 战 zoom_in (fov_scale 0.75, 1.5s) / zoom_out (fov_scale 1.0, 0.8s)
  - 击杀顿帧 (0.08s) + 震动 (amplitude 3.0, frequency 20Hz)
  - World Validator 相机交叉引用校验段
- **HitStop** (T2): 独立击杀顿帧计时器 (wall clock, sim 模式跳过)
  - `HitStop` 类: trigger/update/active/remaining/is_stunned
  - GameScene._process 集成: 顿帧期间跳过游戏逻辑，仅推表现层
- **CameraDirector** (T3): 摄像机语言状态机 (NORMAL/BOSS_WAR/KILL_STUN)
  - 插值: FOV scale + focus_offset (玩家-Boss 中点跟踪)
  - 命名 `CameraLanguageDirector` (避免与 CameraDirector 常量冲突)
- **渲染接入** (T4): 2D/3D 双端相机偏移应用
  - Boss 出场触发 enter_boss_war()
  - 焦点偏移叠加到 _cam_x/_cam_y (2D) / 待接入 3D fov_scale
- **击杀顿帧** (T5): on_monster_killed 触发 HitStop + trigger_kill_stun
  - 顿帧时长: 普通怪 0.08s, Boss 战期间 0.12s
  - 震动强度: 普通 4.0, 精英 8.0, Boss 16.0

- 门禁: Release 0 error · **ctest 64/64** (新增 camera_test 15 用例) ·
  validator 0/0 · sim 逐字节一致
- 待实机验收: Boss 战运镜效果 + 击杀顿帧手感

# A5-T5 — 红饰灰甲骑士分件（开发版，待用户验收）

- 沿用原 Kenney 骑士的红饰、灰甲和面罩辨识特征，补绘透明头盔、胸甲、臂甲、腿靴和剑；原 `player_fire.png` 保留，不是原图无损拆件。
- 五张贴图复用七个绘制件；尺寸、骨架、pivot、动画及战斗逻辑不变。
- 增加确定性像素生成工具，修复预览资源路径并支持多姿态、骨点与原图对照；默认生成到 reports，预览禁止覆盖正式素材。
- A5-T5-fix: 根因定位 — `pixels_per_unit` 0.5 过小，骨骼角色仅渲染 18px 高（实体 28px）；改为 0.8 → 28.8px 匹配实体。腿 12→22px 加宽（与 head 同宽）。avatar parts 完全移除 blob shadow，仅依赖 depth shadow 剪影投影。
- 构建及 CTest 63/63、World Validator 0/0 通过。
- 限制：受击与翻滚完整目验尚待完成，未发布或打 tag。

# v1.7-B3 — 翻滚/闪避: 纯手感位移 + 表现全套 (2026-09-17)

> 用户三拍: 纯手感定位 (无无敌帧/不改战斗数学) · Shift+方向键 (任一 Shift,
> 按住方向=翻滚方向, 无方向=面朝) · 表现件 B 案 (倾斜+压扁+尘土+残影)。
> 设计 spec: docs/superpowers/specs/2026-09-17-b3-dodge-roll-design.md

- **机制**: 2 格 (64px) / 0.16s / 独立冷却 0.7s, 末帧余量补足位移精确;
  撞墙复用 G10.6-B 二分贴墙早停; 零 RNG/dt 定步长 (DodgeComponent)
- **Mirror 采集**: 起翻帧显式 `g_behavior.on_dodge` → DODGE 意图开始积累
  (每帧位移 ~6.7px « 200px 自动阈值, 无双记; DecisionAgent 不加 dodge →
  **sim 12×seed3 报告与改前 sha256 逐字节一致**, 平衡零扰动实锤)
- **2D** (player.cpp): 脚底 origin 倾斜 ±12° + 压扁 110/85 正弦回弹 +
  3 段残影 alpha 衰减; 重击路径零影响 (tilt 静止恒 0)
- **3D** (HD2D): 新 `HD2DDrawItem.scale_w/scale_h` (默认 1 全存量不变) +
  残影 billboard; 倾斜按 spec §9-2 勘误不做 (DrawBillboardRec 无旋转)
- **尘土**: VFXServer ring+spark 直发 → active_effects 2D/3D 双消费
  (§9-1 勘误: 不新增 vfx_recipes.json 配方)
- **码点**: 1936→1939 (翻滚), 字体 atlas 运行时全命中
- 门禁: Release 0 error · **ctest 62/62** (新增 dodge_test 7 用例,
  ctest 按可执行计 §9-5) · validator 0/0 · sim 逐字节 · 2D/3D autoshot 出图
- 待实机验收: 翻滚手感 (参数集中在 dodge_component.h 顶部 constexpr 一行可调)

# v1.6-A3.2-fix2 — 顶面色调映射 + 未探索虚空化 (2026-09-17)

> 实机复测反馈: 墙顶"镂空"依旧 + 未探索区轮廓从俯视泄露。debug 实证
> (品红/青双色标记 + 像素扫描) 后结案:
>
> 1. **顶面从未缺失, 缺的是对比度** — A3.2 顶面 = 墙面同贴图×1.1,
>    深渊群系墙面本身暗 (wall_face 28,18,38) → 顶面暗同虚空, 读作镂空。
>    修复: HD2DDrawItem 新增 top_tint, builder 按 2D 同源
>    wall_top/wall_face 通道比映射顶面色 (深渊≈2×亮), 实测顶面/侧面
>    亮度比 3.22 (抓图像素扫描)。
> 2. **未探索区不再绘制** — 原 M6-i.1 "暗色岩石块"方案从俯视泄露走廊/
>    房间轮廓; 改为跳过 (虚空, 雾战争语义), 已探索不可见仍压暗 40%。
>    副作用: 未探索岩浆不再漏光 (正确性提升)。存档不保存探索标记为
>    2D 既有设计, 载入后全图迷雾重置, 行为一致。
> 3. 保留 rlDisableBackfaceCulling (顶面 -Y 绕序防御; 无配对 re-enable,
>    与 raylib 默认关剔除一致)。
>
> - 验证: Release 0 error; ctest 61/61; 渲染层改动不触逻辑/RNG (sim 免跑)
> - 教训: 调制型 debug 色会淹没在暗纹理里, 像素扫描阈值必须按雾后实际色域
# v1.6-A3.2-fix — 墙顶镂空修复 (背面剔除) + A4 回退到手调 (2026-09-17)

> **A3.2-fix**: 实机反馈"墙顶直接没了，能看进镂空内部" → 在 _draw_wall_block
> 的 rlBegin 前后调用 rlDisableBackfaceCulling/rlEnableBackfaceCulling，
> 保证顶面 quad 从上方任意视角可见 (raylib 推荐做法，双侧面渲染)。
>
> **A4 退避预案执行**: 实机验收"看不出任何变化" → 移除 EMA 亮度反馈机制，
> 回退到 v2g 手调三档 preset (监狱/深渊/火山)。原因：岩浆桶代表光数
> (0..7) granularity 不足，典型火山层可见岩浆桶数饱和 → lum≡center，
> 反馈通道实际无信号。保留代码整洁，移除 _pl_lava_count/_bloom_lum_ema 等
> 内部状态。详见 docs/M6_HD2D_RENDERING.md 已知限制。
>
> - 验证：Release 0 error; ctest 61/61; 墙顶实机目检通过

# v1.6-A4 — bloom 逐帧亮度反馈 (B 案: EMA, 零回读) (2026-09-16)

> 清偿 v2g 遗留: bloom 三档手调常量在岩浆密度逐层不同的火山会偏。
> A 案 (真 HDR pass) 需每帧 readPixels stall, 不采纳; B 案用渲染器
> 已有信号做亮度代理: `lum = 0.34 + 0.045 × 岩浆代表光数(0..7)`,
> EMA τ≈0.17s, 偏差 d 联动 threshold(±0.06/+0.10 内) 与
> intensity(×0.6~×1.5 内); biome 切换 ema 直接重同步无穿帮。
> 设计详见 docs/M6_HD2D_RENDERING.md A4 节。

> ## 性质与验证
> - 监狱/深渊无岩浆恒 d=0 → 与 v2g 手调值逐位一致 (反馈只在火山生效)
> - F6 满布岩浆火山实测: 与 A3.2 画面 diff 26/614400 (0.004%, 仅粒子
>   相位) = 落在标定中心, 零回归; 目检 bloom 光晕正常
> - 纯渲染器私有状态 (_pl_lava_count/_bloom_lum_ema), 不触 PostFX
>   接口/Shader/逻辑层
> - Release 0 error; ctest 61/61; sim 12×2 双跑逐行一致;
>   _upload_point_lights 压回 40 行合规

> ## 取证伪影结案 (A4 期间顺带查明)
> - 历史截图"HUD 中文乱码"= autoshot 裸导 RT 的字形纵向镜像伪影
>   (2D 模式截图同样中招; 实机屏幕正常, 用户从未反馈此问题)。
>   曾试 LoadImageFromScreen 抓 backbuffer → hidwin 下全黑, 已回滚。
>   读文本以 game.log 为准。详见 docs/M6_HD2D_RENDERING.md 已知限制

# v1.6-A3.2 — 墙顶白块替换: 同贴图顶面 quad (2026-09-16)

> A3.1 让 shader 真实生效后, v2a 遗留的"顶面亮 10% 无贴图 DrawCube 盖"
> 成为画面最刺眼缺陷: 纯白 1 单位浮块盖住走廊、不收雾/阴影、与 caster
> 深度顶面 (y=h) 不共面。

> ## 修复 (纯渲染)
> - `_draw_wall_block` 贴图分支: DrawCube 白盖 → `_wall_top_quad`
>   同贴图水平 quad @ y=h (与 shadow caster 顶面共面, 影带自动对齐);
>   "亮 10% 伪受光"语义保留在 tint 上, 但颜色来自贴图不再爆白
> - 无贴图回退分支不动 (纯色块 + 盖本来就自洽)
> - 附带收益: 墙体视觉高度 -1 单位 = 真实高度, 走廊/上层房间可见性提升

> ## 验证
> - F6/F3 取证: 白块全消, 墙面六面纹理一致, 零 shader 编译失败
> - 函数长度合规 (`_draw_wall_block` 34 行, tint 计算挪入 helper)
> - 重构前后像素 diff 26/614400 (0.004%, 仅粒子相位) = 零视觉变化
> - Release 0 error; ctest 61/61; 桌面同步 (src + exe + docs)

# v1.6-A3.1 — P0 热修: HD2D shader 从未编译过 (`#` 注释 + bank 假阳性 + 悬垂路径) (2026-09-16)

> 目检取证截图时发现画面与"shader 生效前"完全同构。深挖后确认三个叠加
> 缺陷, 导致 v2c 起 (v1.4.x→v1.5.0→v1.6-A1/A2/A3) 的雾/阴影/岩浆动画/
> bloom/描边 shader **从未在 GPU 上编译成功**, 游戏一直静默跑默认回退管线。

> ## 三个根因
> - `assets/shaders/*.fs|*.vs` 全部 8 个文件: 头部注释行 `// ` 被写成
>   `# ` (非法 GLSL 预处理指令) → 编译必败; git 考古确认自创建提交
>   (ab93b44 v2c) 起即如此, 非近期回归
> - `hd2d_shader_bank.cpp` 有效性判定 `shader.id > 0` 假阳性: raylib
>   编译失败返回**默认 shader (id>0)**, 日志永远记"加载成功" → 此前
>   取证"7 shader 全绿零 WARN"结论作废 (WARN 在 stderr, game.log 不含)
> - 同文件 `_shader_path(...).c_str()` 对临时 std::string 取指针 =
>   悬垂 UB → 路径随机损坏 (偶发 "Failed to open" / depth 单帧失败)

> ## 修复
> - 8 个 shader 文件 `(?m)^# ` → `// ` (合法 `#version` 不受影响)
> - bank: `entry.valid = id>0 && id != rlGetShaderIdDefault()` (+rlgl.h)
> - bank: vs/fs 路径改持有 std::string 生命周期

> ## 验证 (真实 shader 首次上 GPU)
> - F3/F6/F11 三楼层取证: stderr 零编译失败, game.log 全 INFO 加载成功
> - 像素 diff: 修复前后 46.2% 像素变化 (全小 delta 无爆图) = fog/shadow/
>   point-light/lava/bloom 真实生效, 画面稳定无异常
> - F11 深渊萤火虫 PNG 贴图正常渲染 (A2.2 首次肉眼可见)
> - Release 0 error; ctest 61/61; sim 双跑逐行一致 (sim 路径零渲染引用);
>   对旧基线差异归因 = 取证游玩使镜像学习表进化 (设计内), 非本批代码
> - 桌面同步: assets/shaders + src + exe

# v1.6-A3 — 实体接收阴影 (billboard shadow map 逐像素采样) (2026-09-16)

> v1.5 技术债清单第 3 项 ("实体不接收阴影") 清偿。v2f 起实体只投影不
> 收影: 站在墙面影带里依然全亮, 与地形脱节。A3 让描边路径角色
> (玩家/怪/Boss/NPC) 与地形共享同一张 shadow map。

> ## 实现 (纯渲染, 逻辑层零改动)
> - `hd2d_billboard_outline.fs`: 新增 shadow map 采样段 — sample_shadow
>   与 `hd2d_fog.fs` 逐字同源 (lightViewProj + PCF 3x3 + 光域外豁免);
>   压暗系数 `0.78 + 0.22*shadow` 与地形 M6-i.1 环境保底同一档位
> - 逐像素投影 (非整片单采样): 身上影带与地面阴影自然对齐,
>   高于遮挡物的部位沿光路自动复亮 — 物理正确的"半身入影"
> - 描边色同步压暗 (影中轮廓不浮亮); tint/fade/呼吸语义不变
> - C++: `_upload_shadow_uniforms` 抽公共体 `_upload_shadow_to`
>   (fog/outline 两 shader 一套数据源, 转置/包装/缺失兜底共用)
> - 降级安全: shadow map 未就绪→shadowEnabled=0 原样渲染;
>   outline shader 加载失败→旧 4 向描边路径 (不收影, 不崩溃)

> ## 影响面与边界
> - 生效面 = item.outline 实体 (builder: 玩家/怪/Boss/NPC);
>   装饰/道具 blob 接地阴影不变 (全 shader 化留 A5 一并评估)
> - 性能: 每实体 +5 SetShaderValue + PCF 9 采样/像素, ≤60 实体无感
> - sim 零分岔: 双跑一致 + 对 A2.2 基线仅构建戳差异 (实测)

> ## 验证
> - Release 0 error; ctest 61/61; sim 红线电池绿
> - **无头取证链** (`--hd2d --goto-floor F --autoshot N --hidwin` + 桌面
>   slot_1 借档): F3/F6 截图帧全 shader (含新 outline fs) 加载成功,
>   渲染 300 帧零 WARN; 像素审计描边/半影带正常 (非全黑/无破图),
>   取证后 slot/meta/game.log 现场已完整还原
> - 待实机目检: 影带对齐观感 (唯一不可自动化项)

# v1.6-A2.2 — 粒子资产升级: 群系 PNG 贴图 + 数据驱动风格 (2026-09-16)

> A2.1 的三性格是代码里按 biome id 硬映射 + 程序化软光圆。A2.2 补上
> ART_ASSET_PLAN P2 "Ambient particles PNG" 欠账: 风格与贴图进 biomes.json
> 数据驱动, 三个手绘 16×16 像素粒子图, 为 MOD/新群系留好扩展口。

> ## 新增
> - `tools/make_particle_textures.py` (PIL, 确定性零随机) →
>   `assets/textures/particles/particle_{dust,ember,firefly}.png`
>   - dust: 灰紫柔球+碎屑斑 / ember: 白热核+橙环+上飘尾迹 / firefly: 亮核+十字星芒
> - `biomes.json` ambient 段新增 `style` + `texture` 字段 (全可选)
> - 解析链: `AmbientDef.style/texture` → `AmbientCfg.style/texture` (2D 侧不消费)
> - 3D: builder 优先 `cfg.style` 派生 `MoteStyle` (空→A2.1 id 回退);
>   `item.texture` 经 ResourceManager 现成缓存加载, 像素贴图放大系数收紧 1.6→1.3
> - renderer 批量内按 item.texture 切换 (同帧同群系实际仅 1 次), 缺图→程序化软光

> ## 影响面
> - 2D AmbientLayer 行为零改动 (新字段 2D 不读取); JSON 为"只新增字段"不破坏旧读者
> - sim 零逻辑分岔 (实测双跑一致 + 对 B3M 基线仅构建戳差异)
> - validator 扩展: ambient.style 白名单 {dust,ember,firefly} + texture 文件存在性

> ## 验证
> - Release 0 error; ctest 61/61; world_validator 0/0 (新检查生效)
> - `--sim 12 --sim-seed 3` 双跑一致, 对 B3M 基线零逻辑 diff
> - 桌面包已同步: src/ resources/ tools/ assets/ + exe

# v1.6-B3M — Mirror 学习闭环: 克隆表跨局记忆 (2026-09-16)

> "会学习你的 Roguelike" 的最后一块拼图: 此前跨局唯一持久的学习只有 slot 档
> 内的 Thompson 后验 (mra/mrb)；M1 克隆表每局从清零的 action stream 现算,
> 换个进程就忘光 — AI_LEARNING_GUIDE §8.4 点名的技术债 ("load 空 stub") 清偿。

> ## 新增
> - `MirrorMemoryStore` (src/ai/mirror/): saves/mirror_memory.json 读写,
>   克隆表快照序列化 (nlohmann), .tmp+rename 伪原子写, 128 条熔丝
> - **跨局遗忘曲线**: 每次读入整体 ×0.99 截断, 单次证据一局即过期,
>   40+ 证据可撑 ~20 局 — 旧习惯自然让位新打法
> - `BehaviorCloneTable::table()/merge_entry()` 持久化注入口
> - 注入点: `_init_mirror_boss` 本局 build 后 merge (每局新建表, 天然不叠加)
> - 回写点: `export_mirror_memory` (楼梯存档/回标题/死亡三处现有调用全覆盖);
>   本局未遇 F15 (agent 空/表空) 跳过落盘, 不清空历史记忆

> ## 影响面与红线
> - **sim 确定性零接触**: `MetaSystem::g_readonly` 双端 guard, 实测
>   `--sim 12 --sim-seed 3` 不落盘不注入, 双跑一致, 对 A2.1 基线零逻辑 diff
> - 2D/3D 逻辑层/战斗数值/JSON 资源零改动; Profile/ChainTable 留后续批
> - 记忆跟人走 (saves/ 根), 不挤占 slot 档与 meta 伪 JSON 格式

> ## 验证
> - Release 构建新增文件 0 warning; ctest **61/61** (新增 5 例:
>   存取自愈/衰减精确值/merge 累加/微量记忆过期/损坏文件防脆)
> - 桌面同步: src+tests+exe

# v1.6-A2.1 — 3D 环境粒子: 群系性格软光 (2026-09-16)

> M6-v2e 的氛围粒子在 3D 层是"逐颗 DrawSphere + 每颗切一次 blend":
> 24 颗 ≈ 1.2 万三角面 + 24 次状态切换, 且挤在同一高度带像悬浮小球地毯。
> A2.1 把它做成真正的 3D 氛围层: 程序软光纹理 billboard 单批 + 群系性格运动。

> ## 新增
> - **软光渲染重构**: 32×32 白色径向渐变程序纹理 + 相机朝向 quad,
>   全部 AMBIENT_MOTE 一次 `rlBegin/rlEnd` additive 批 (24 次 flush → 1)
> - **群系性格** (`MoteStyle`, 按 `get_biome_for_floor` 派生, 纯渲染语义):
>   - `DUST` 监牢尘埃: 贴地 4~20, 慢摆 + 弱微闪
>   - `EMBER` 火山余烬: 急升 8~72, 大幅蜿蜒, 亮度随熄灭衰减
>   - `FIREFLY` 深渊幽光: 中层悬浮 bob, 强周期明灭
>   相位由 spawn 稳定字段 (vx/size) 位哈希 + `GetTime()` 推导 —
>   无新增状态、无随机 (同 v2a 呼吸帧惯例)
> - 首尾渐隐包络与 2D `AmbientLayer::draw` 同式, 2D/3D 观感同源

> ## 影响面
> - AmbientLayer (2D 共享逻辑) 零改动; sim 不跑渲染层, 逻辑零分岔
> - ShadowCaster / PostFX / JSON / Save / RNG 零接触
> - draw call: 粒子 24→1 批 (球体 ~500 tris/颗 → quad 2 tris/颗)
> - shutdown 对称释放 `_mote_glow_tex`

> ## 验证
> - Release 构建 0 warning; ctest 60/60; world_validator 0/0
> - `--sim 12 --sim-seed 3` 双跑一致 + 对 A1.1 基线仅构建戳差异
> - 函数长度审查: 全部新增/改动函数 ≤30 行
> - 桌面包已同步 src/ + exe
> - 实机待验收: F1 尘埃贴地感 / F6 余烬上升蜿蜒 / F11 幽光明灭 + 性能不降

# v1.6-A1.1 — 3D-aware Billboard 真轮廓描边 (2026-09-16)

> M6-n 的 4 向偏移是"2D 描边搬进 3D": 世界 x/z 偏移在 45° 俯角下退化成左右
> 两向、矩形剪影不贴精灵轮廓、每个描边实体 5 次 draw。A1.1 把它升级为真正的
> 3D 渲染管线 Billboard Outline — 单 draw call, 轮廓精确贴合 Sprite Alpha Mask。

> ## 新增
> - **hd2d_billboard_outline.fs**: 8 邻域 alpha-mask 采样 (4 正向 + 4 对角),
>   本体像素走原色, 透明且邻域为剪影 → 描边色, 其余 discard; 阈值 0.5 与
>   shadow depth pass 同源; 复用 hd2d_world.vs
> - **世界空间 → 屏幕空间自适应** (核心, 拒绝固定 texel):
>   基准宽 2.0 世界单位 → 每帧 `_update_px_per_world()` 按相机距离/FOV 换算
>   屏幕像素 (透视投影) → clamp [1, 4] px (远距可辨识 / 近距 Boss 不过粗)
>   → 再转图集 UV offset 传入 shader
> - **outline 色经 uniform 传入** (默认 RGB 24,24,27 / a=220, 与旧系统同色),
>   为后续群系色相微调留钩子
> - **NPC 开启描边** (与玩家/怪/Boss 同权); 拾取物/Arena 物件/Projectile/VFX
>   保持不描边 (kind 天然隔离, 未动)

> ## 影响面
> - 描边实体 draw call 5→1 (outline 生效时); 非描边实体路径零改动
> - **描边不参与 shadow map**: ShadowCaster 仍用本体几何 (hd2d_depth.fs),
>   视觉轮廓 ≠ 实体几何, 两 pass 语义保持干净
> - HD2DPostFX / fog / lava / bloom / Gameplay / RNG / Save 零接触
> - shader 加载失败 → `_outline_ok=false` → 完整回退 M6-n 旧 4 向偏移路径

> ## 验证
> - Release 构建 0 warning; ctest 60/60 全绿
> - world_validator 0 错误 (JSON 未动, 零漂移)
> - `--sim 12 --sim-seed 3`: 新 exe 双跑逐行一致; 与 A1.1 前旧 exe 对比仅
>   构建时间戳/META 存档环境差异, 逻辑零分岔
> - 桌面包已同步 src/ + assets/shaders/ + exe (根目录一键跑)
> - 实机待验收: F1 轮廓清晰不糊黑块 / F6 深色背景不融边 / F11 紫环境不染色 /
>   F15 Boss 近拉远屏幕厚度稳定 1–4 px

# v1.6-B1.1 — Mirror 记忆实时可视化 HUD (2026-09-16)

> B1 前作只把"它眼中的你"堆在右上 Echo 面板下方; 玩家看不到 Mirror 到底在
> 猜什么、习惯什么。B1.1 让"它在学"从抽象数字变成战斗中一眼读懂的画面。

> ## 新增
> - **MirrorHudPanel 组件**: 顶部 reveal + 顶部观察卡 (零逻辑侵入)
>   - `MIRROR ANALYSIS` 500×88 分析卡: Echo 登场 reveal 6.5s 淡入淡出, 不常驻
>   - 左半 = 技能偏好 4 柱 (SL/FB/SH/TW, 本命金框)
>   - 右半 = **战斗习惯** 4 方块 (ATK/SKL/RET/APP, 明确"非角色属性")
> - **观察卡 340×54**: reveal 结束后接手同位置, 战斗全程常驻
>   - 首行 = "它猜你下一步 → ATTACK 68%" (缓存 action+confidence, 决策同频)
>   - 尾行 = "本局节奏 A3.2 S1.4 D0.8 H0.3 · 42s" (实时/秒)
> - **MirrorAgent 决策缓存**: `predict_next_action` 内部各分支返回前写入
>   `_last_pred_action/_last_pred_conf` (mutable, const 语义保持);
>   `cache_pred_time(gt)` 由 MirrorCombatDirector 打时间戳; HUD 严禁重跑 predict
> - **stale 语义**: 观察卡 >2s 未刷新降级"上次预测 Ns前"; >200s 兜底"观察中…"

> ## 影响面
> - 单测 `predict_next_action` 返回值语义不变 (3 处测试原样通过)
> - Thompson 早退分支不刷新 predict 缓存, age 自然增大 → HUD 显示陈旧标记
> - `begin_battle` 追加清缓存; 二周目/调试重开不残留

> ## 验证
> - ctest 60/60 全绿
> - 字体码位: 新增 24 中文字符 + `…` `·` `%` 全部命中现有 1831 码位表
> - 桌面包已同步 exe + src/ (根目录 roguelike_cpp.exe 一键跑)

# v1.6-B2 — 死因仪表盘游戏内化 (2026-09-15)

> 死因数据止于 sim CSV — 玩家死后只看到"你死了"。B2 把死因谱搬进游戏内:
> DeathScene 显示本局死因 + 跨局死因谱 Top3。

> ## 新增
> - **本局死因行**: DeathScene "死于: XXX" (红字醒目, 位于层数行上方);
>   `dot:`/`env:` 前缀翻译为 "持续伤害·"/"环境·" 友好文案 (sim CSV 仍用原始码)
> - **跨局死因谱**: 账号级 death_history (最近 8 条环形), 账号级删档不丢;
>   ≥2 类死因才显示 Top3 ("尖刺史莱姆 ×3"); 首死只看本局不显示谱
> - **MetaSystem::record_death / top_death_causes**: 环形入账 + 聚合查询
> - **存档兼容**: 老档无 `deaths` 键 → 空史加载不炸 (实测); buf 2048→4096
>   手写异常文件 (>8 条) 也截到最近 8 条 (丢最旧, 与 record 语义一致)

> ## 验证
> - ctest 60/60 (含新增 DeathHistoryRingAndTopCauses: 环形/Top聚合/删档不丢)
> - sim12 seed3 基线零漂移 (排除时间戳); meta 哈希前后一致 (readonly 生效)
> - 磁盘往返: 9 条手写 → load 截 8; 中文死因 UTF-8 回读正确

# v1.4.33 — v1.5.0 P0 门禁推进 (2026-09-15)

> M6 后直接进入 v1.5.0 发布门禁自动化验证。

> ## 新增能力 (工具)
> - **sim goto-floor 直达通道**: `--sim N --goto-floor F` — 深层取证/冒烟
>   不再需要 bot 自然爬层 (20 局仅到 F11)。仅 sim 模式生效; 玩家强度对标
>   `_sim_goto_scale_player` (等级/HP/攻防/药水随层数缩放, 防 F15 镜像
>   Boss 6 击秒杀裸装 bot); 默认 0 时与原路径完全一致 — sim12 1853 行
>   基线对比零漂移已证
>
> ## P0 门禁验证结果 (自动化项)
> | 项 | 结果 |
> |:---|:---|
> | 旧档迁移 save.json→slot_1+.bak | ✅ 实测 (构造旧档启动即迁移) |
> | Boss 全链 F5/F10/F15 | ✅ F5/F10=65%/40% (sim20); F15 直达: 触发/镜像Echo/Phase2/击杀/win 全链 |
> | ctest 60/60 + validator 0/0 | ✅ |
> | 冒烟 20 局 | ✅ exit 0 / NaN 0 / avg_floor 7.75 |
> | HD-2D 三群系截图 | ✅ F1/F6/F11 实拍 (像素统计与 M6-n 基线签名一致), README 首图挂 F6 |
> | Release zip | ✅ 45.3MB 自包含 (objdump 仅 raylib.dll+系统库; 干净目录解压冒烟通过) |
> | LIMITATIONS 核对 | ✅ 各行与实际一致 |
> | 键位表 vs input_map | ✅ 100% 一致 (R/M/F1/N/C/F/T 全部对上) |
>
> ## P0 待实机 (用户 30min)
> - 30min 新档闭环 / 三槽存档流 / 选关解锁 / crash.log 无新增
>
> ## P1 已验 (自动化)
> - 三群系怪物分布: F1(兽人/史莱姆) F6(+冲锋兽人) F11(+召唤师/夜行猎手/精英) 递进无全orc ✅

# v1.4.32 — M6-n 完成: 视觉收口 Final Polish (2026-09-15)

> M6 HD-2D 第一阶段收官: 门贴墙/实体描边/光照定稿/性能基线。
> 红线遵守: 零新系统, 纯收口。

> ## 渲染 (rendering3d)
> - **门贴墙**: DOOR_PANEL billboard→wall-aligned — builder 探测门两侧
>   墙走向(door_axis), renderer rlBegin quad 沿X/Z展开+法线朝相机侧;
>   LOCKED 红罩/SEALED 紫十字按 axis 取向; 无贴图棕色立板同步;
>   新增门楣横梁(棕木色)提高辨识
> - **实体描边**: outline 字段(玩家+怪), 4 向偏移中性深色
>   (24,24,27,220) 底稿偏移 size*0.02 — 分离主体与背景;
>   中性色防色偏 (暖棕实测稀释深渊紫 R-B -4.5→+1.3 故弃)
> - **FPS 基线**: autoshot 导出帧记录 GetFPS (修 %.0f 格式错配垃圾值)
>
> ## 光照定稿 (全图统计跨次取证零波动)
> | 楼层 | warm% | cold% | 亮区色 | 语义 |
> |:---:|:---:|:---:|:---:|:---:|
> | F1 | 1 | 8 | (201,209,216) 中性 | 石灰监狱 |
> | F6 | 28 | 3 | (206,199,193) 暖 | 火山暖棕 |
> | F11 | 1-2 | 8 | (198,197,210) 紫 | 深渊紫光 |
>
> ## M6 全链路验收
> - 60/60 ctest + validator 0/0 + 构建 0 警告
> - sim 12 双跑 1853 行逐行零分岔 (RNG-002 红线保持)
> - FPS=1800 全五层 (F1/F5/F6/F11/F15) 零楼层性退化

# v1.4.31 — M6-m 完成: 角色资产升级 (2026-09-14)

> 玩家与怪物使用真实 sprite 纹理替代程序化占位,
> 4 个 Hero Asset 验证通过后可批量扩展。
>
> ## 资产 (resources/sprites.json)
> - **player_default**: 新增,映射到 player_fire.png (默认玩家外观)
> - sprites.json 精灵定义 81→82
>
> ## 渲染 (rendering3d)
> - **_monster_sprite_key_for_3d**: 新增怪物 sprite 映射函数
>   (按 MonsterType + 名称匹配,与 2D monster.cpp 同源)
> - **_build_entities**: 怪物渲染改用 sprite 映射而非硬编码 "mon_orc"
>
> ## 验证
> - 60/60 ctest + 0 警告 + world_validator 0 err
> - F1 截图确认 sprite 加载 (82 定义)

# v1.4.30 — M6-l 完成: Boss FOV 红雾 3D 呈现 (2026-09-14)

> Boss 可见但玩家不可见的区域叠红色半透明覆盖,提示危险区域。
> 修复 _draw_floor_decal 支持无纹理纯色 quad + 使用 item.tint。
>
> ## 渲染 (rendering3d)
> - **_build_terrain**: 遍历 tile 时检查 `boss_visible && explored && !visible`
>   → 添加 FLOOR_DECAL 红色 quad `{180,40,40,50}`
> - **_draw_floor_decal**: 支持无纹理纯色 quad (DrawPlane)
>   + 使用 item.tint 而非硬编码白色
>
> ## 验证
> - 60/60 ctest + 0 警告 + world_validator 0 err
> - F5 Boss 层截图验证 (Boss 未遭遇时无 fog, 符合预期)

# v1.4.29 — M6-k 完成: Arena 物件 3D 呈现 (2026-09-14)

> 战场环境元素(爆炸桶/图腾/毒池/岩石/尖刺)从 2D 翻译到 3D,
> 程序化纹理零外部素材,确定性生成保 Sim 兼容。
>
> ## 渲染 (rendering3d / sprite_renderer)
> - **gen_arena_prop**: 5 类物件程序纹理 32×32
>   - EXPLOSIVE_BARREL: 木桶+黑箍,点燃时红脉冲 tint
>   - HEALING_TOTEM: 绿符文柱+光晕
>   - POISON_POOL: 绿色毒池贴地 quad
>   - ROCK: 灰岩石矮 billboard
>   - SPIKE: 红尖刺三角贴地
> - **_build_arena_objects**: 遍历 map->arena_objects,
>   POISON_POOL/SPIKE 走 FLOOR_DECAL(贴地), 其他走 ENTITY_BILLBOARD
> - ResourceManager.procedural_arena_prop 缓存 per-type 纹理
>
> ## 验证
> - F6 火山层生成 7 个 arena 物件,截图检测到
>   绿 4434px(totem) / 橙 1648px(barrel) / 红 23786px(spike)
> - 60/60 ctest + 0 警告 + world_validator 0 err

# v1.4.28 — M6-i.1 完成: 3D 群系色相可见 + 静默取证监视链 (2026-09-14)

> 本切片解决 HD2D 3D 渲染"全屏蓝灰、群系不可辨"问题,并建立后台静默
> 取证管线(不弹窗/不抢焦点/不干扰用户/不产生键盘事件)。
>
> ## 核心修复 (hd2d_scene_builder / hd2d_renderer)
> - **墙 tint bug**: 有贴图墙的 tint 从蓝灰回退色(74,78,96)改为
>   WHITE + 探索态压暗,与地板同规则; 消除实测 R-B=-18 冷偏
> - **未探索 tint 群系化**: 从 palette 推导(火山 wall_top×0.45
>   / floor_dirt×0.50),中性灰→群系暗色; 全屏黑空洞→暗色群系岩石
> - **build 范围扩大**: ±16/±12 → ±22/±16,覆盖 440 相机视野
>   (实测 ClearBackground 从 52% → 0%)
> - **后处理链暖化**: ClearBackground/夜色层/雾带/雾色全部从蓝→暖暗
>   (冷色占比 62.7% → 3.3%)
>
> ## 静默取证管线
> - scene_tree: `--hidwin` 仅 SetWindowPosition 移屏外(不禁渲染);
>   `--autocontinue` 读首存档直接进场景(免键盘 Title/SlotSelect);
>   `--goto-floor N` 覆盖读档楼层; autoshot 导出 screenshot.png
> - C# GameCaptureV2: CaptureGotoFloor 封装 exe 参数+轮询复制
>
> ## 三群系实测验证
> | 楼层 | 群系 | 中心 R-B | 暖% | 冷% | 语义 |
> |:---:|:---:|:---:|:---:|:---:|:---:|
> | F1 | 遗忘监狱 | -7.2 | 1% | 8% | 中性石灰 |
> | F6 | 灰烬火山 | +14.7 | 31% | 3% | 暖棕岩浆 |
> | F11 | 虚空深渊 | -4.5 | 1% | 8% | 暗紫晶光 |
>
> 验证: 60/60 ctest + 0 警告 + world_validator 0 err 0 warn。
> 同步: 桌面开发测试包已更新。

# v1.4.27 — M6-i+j 完成: 群系材质风格化 + 地面细节哈希装饰 (2026-09-13)

> 用户拍板 M6 后半段路线: 技术特效边际收益下降, 转向资产密度。
> 本切片: 群系"长出自己的皮肤"(材质分歧) + 地面打破单调(确定性装饰)。
>
> ## 渲染 (rendering/sprite_renderer + rendering3d)
> - **M6-i 群系材质**: gen_biome_tile per-biome 画法 — 监狱湿石砖+
>   苔斑 / 火山玄武岩+熔岩裂纹 / 深渊晶柱+发光符文; 回退链末端按
>   biome_id 选风格, 贴图资产到位自动让位 (GENERIC=原画法零回归)
> - **M6-j 地面细节**: 坐标哈希 (2D 同款公式, 零 RNG 流 — Sim
>   确定性红线保持) — tint 变体 (污渍 6%/石块 4%) + 7% decal
>   贴片 (裂缝/苔藓/符文 per-biome 配色); gen_floor_decal 程序生成,
>   FLOOR_DECAL 贴地 quad 防 z-fight
> - ResourceManager 新增 procedural_biome_tile / procedural_floor_decal
>   缓存 (key 编码风格+色值)
>
> 验证: 60/60 ctest + 0 警告 + validator 0 err + sim 12 与基线零分岔
> + --hd2d 冒烟 exit 0 + 实机键链稳定 (像素分布同族)。

# v1.4.26 — M6-v2h 完成: 地图可读性三件套 (门/楼梯/特殊房间) (2026-09-13)

> 3D 模式覆盖度补齐: 玩家导航依赖的三大地标 (门/楼梯/特殊房间)
> 从 2D 翻译到 3D。此前 3D 缺门 (只有棕色平地块) / 楼梯不显眼 /
> 特殊房间与普通地板无异。
>
> ## 渲染 (rendering3d)
> - **门 DOOR_PANEL**: 竖立贴图面板 — 四态纹理与 2D 同 manifest;
>   LOCKED 红罩+锁徽记 / SEALED 紫脉冲十字 (2D overlay 语义)
> - **楼梯**: tint 棕金阶调 (2D 60/48/26 系), 与地板明确区分
> - **特殊房间**: 九色地板 tint (triggered 压暗) + 中心 ROOM_ICON
>   (贴地菱形底 + 浮空 billboard; room_* 素材同源)
> - 审计: 3D vs 2D 世界层覆盖差距全清单入档 M6 文档 (Arena 物件/
>   Boss FOV 红雾/地板装饰变体为下批候选)
>
> 验证: 60/60 ctest + 0 编译警告 + sim 12 与基线零分岔 +
> --sim 2 --hd2d 冒烟 exit 0 + 实机键链稳定 (像素分布同族, avg 略升
> = 房间色增量, 无黑屏回归)。

# v1.4.25 — M6-v2g 完成: LAVA 点光聚类 + bloom biome 自适应 (2026-09-13)

> v2g 两块视觉小活收官: 火山层岩浆光照从"前 7 块"变"全区均匀亮";
> bloom 参数首次按场景分档 (此前一直是默认常量)。
>
> ## 渲染 (rendering3d)
> - **LAVA 点光网格聚类**: 4-tile 网格分桶每桶 1 光 (桶内首 tile
>   锚点), 光源数与岩浆 tile 总数解耦 (恒 ≤7); 半径 3→3.5 tile
> - **bloom 三档 biome 预设**: 监狱 0.60/0.25/0.58 (低阈值补亮) /
>   深渊 0.68/0.22/0.50 (幽紫光晕) / 火山 0.80/0.15/0.42 (压强度
>   防泛红); set_params 首次接线
>
> 已知限制: bloom 三档为手调常量, 待人眼验收微调。
>
> 验证: 60/60 ctest + validator 0 err + sim 12×2 双跑零分岔
> (剔时间戳, 与基线一致) + --sim 2 --hd2d 冒烟 exit 0 + 实机键链
> (PostMessage 注入, 全 shader 链零 WARN, 渲染循环稳定)。

# v1.4.24 — M6-v2f 完成: 实体剪影进 shadow map (2026-09-13)

> v2e 只投影墙, billboard 实体走 blob 回退 ("墙有影怪没影") — 本切片
> 补齐实体投影。alpha-discard 深度 shader + 深度几何与主 pass 同源。
>
> ## 渲染 (rendering3d)
> - **Billboard 深度剪影**: hd2d_depth.fs (alpha<0.5 discard, 透明像素
>   不写深度) + hd2d_world.vs 共享顶点; shader_bank 回退机制复用
>   (编译失败仅墙投影, 不崩溃)
> - **几何同源**: _draw_billboard_depth 复用 DrawBillboardRec 顶点公式
>   (含 flip_x), 相机只参与朝向数学 — 深度剪影像素 = 主 pass 可见像素
> - **当帧相机**: update_light_camera 注入主相机; render_frame 相机
>   定位提前 (深度 pass 不吃上帧残值)
> - **blob 三态降级**: 剪影生效=40 / 仅墙投影=60 / 全回退=120
> - 实机键链验证 (PostMessage 注入 N→ENTER→SPACE→移动): hd2d_depth
>   编译成功 + 深度 RT 480x320 就绪 + 全 shader 链零 WARN
>
> 验证: 60/60 ctest + validator 0 err + sim 12×2 双跑零分岔
> (剔时间戳, 与 RNG-002 基线一致) + --sim 2 --hd2d 冒烟 exit 0。

# v1.4.23 — M6-v2e 完成: Shadow map + 点光源 + 打磨 (2026-09-12)

> v2e 合体方案落地。HD2D 风格核心拼图: 方向光阴影 + 岩浆/火把点光。
> 附带 3D 相机 shake 接线 + 氛围粒子 3D 化。修复深度 pass FBO 恢复 bug。
>
> ## 渲染 (rendering3d)
> - **Shadow map**: hd2d_shadow_caster 新模块 — rlgl 原语 depth-only
>   fbo (480x320); 45° 方向光正交投影跟相机; 墙几何深度 pass;
>   地形 shader PCF 3x3 软化 + 环境光 45% 保底; blob shadow 在
>   shadow map 激活时 alpha 减半 (接地感保留)
> - **点光源**: LAVA tile 暖橙自发光 (半径 3 tile × 7) + 玩家火把暖光;
>   距离平方衰减; shader uniform 数组只读收集, 逻辑层零改动
> - **3D 相机 shake**: set_camera_shake 注入 (2D shake_offset 同源,
>   RNG 红线语义保留 — 独立视觉流)
> - **氛围粒子 3D 化**: AMBIENT_MOTE additive 微光球 (AmbientLayer
>   particles() 只读视图; 火山余烬/深渊幽光 3D 对应)
> - **修复**: render_depth FBO 恢复 bug — 曾绑回 FBO 0 (屏幕) 而非
>   scene_tree 主 RT, 主场景被 blit 丢弃 (截图全屏近黑;
>   DEBUG 常量色 shader 分层隔离定位, caller 注入 outer_fbo 结案)
>
> 验证: 60/60 ctest + validator 0 err + sim 12×2 零分岔 + --hd2d
> 实机键链 (shader/shadow RT 全加载) + 像素分布回归正常
> (avg 57,62,75 / 纯黑 0.2%, 修复前全屏 (2,2,5))。

# v1.4.22 — M6-v2d 完成: 战斗反馈打磨 (拖尾 + 名条遮挡) (2026-09-12)

> v2d 小步打磨。3D 投射物拖尾 + 2D/3D 同条件名条视线裁剪。
>
> ## 渲染 (rendering3d) + 世界 (world)
> - **投射物拖尾**: _draw_projectile_trail — 反速度 3 段渐隐线
>   (0.03/0.06/0.09s, 递减 50/33/17%, BLEND_ADDITIVE); 语义对齐 2D
>   穿透弹 back 线 (0.03s) 并加强; trail_dir=p.vel 直接传递, 零状态
>   (不加弹实例 id, 不触碰逻辑层)
> - **名条遮挡裁剪**: GameMap::has_line_of_sight 新增 (Bresenham
>   tile 步进, 只读, 端点不检查, 越界保守挡视线); 2D 名条与 3D 投影
>   名条同条件判定 — 墙后名条不再穿透 (怪本体绘制不变)
>
> 验证: 60/60 ctest + validator 0 err + sim 12×2 双跑零分岔 +
> --hd2d 实机 (键链自动进游戏) shader 全加载 + 优雅退出 exit 0。

# v1.4.21 — M6-v2c 完成: 3D 表现层 C 档 shader 全量 (2026-09-12)

> v2c shader 档收官。距离雾/blob 阴影/岩浆动画材质/bloom 四项落地,
> 全部带失败回退 (shader 编译失败自动降级默认管线, 不崩溃不黑屏)。
>
> ## 渲染 (rendering3d)
> - **平滑距离雾**: hd2d_fog.fs + hd2d_world.vs — 克隆 rlgl 默认采样管线
>   + viewPos→片元距离 smoothstep 雾色混合; 替代逐 tile 40% 阶跃压暗
>   (FOV 探索语义仍由 builder tint 保留)
> - **Blob shadow**: GenImageGradientRadial 64x64 程序纹理贴地椭圆
>   替换黑扁片 cube (_draw_blob_shadow 提取, 失败回退原实现)
> - **岩浆动画材质**: hd2d_lava.fs — 世界坐标 value-noise 三层分段
>   (暗壳/亮流/热核, 对齐 2D LAVA 深底/裂纹/热核) + 双向流动 +
>   emissive 呼吸 (频率 4.0 同源); builder is_lava 标记分流,
>   探索压暗编码进 tint 灰度
> - **Bloom 后处理链**: 亮部提取 (1/4 RT, Rec.709 亮度阈值+平方衰减) →
>   9-tap 高斯乒乓 (水平/垂直) → additive 全屏叠加 (BLEND_ADDITIVE)
> - **性能**: 地形两遍分区 (lava 单批 + 其余单批), 每帧仅 2 次 shader
>   切换, 避免逐 tile 切换的数百次 batch flush
> - **raylib 5.0 嵌套 RT 坑**: EndTextureMode 盲绑 FBO 0 并重置投影 →
>   HD2DPostFX::process 尾部手动恢复 FBO+viewport+960x640 ortho;
>   SceneTree::main_target() 新增只读 getter
> - 新模块: hd2d_shader_bank (GLSL 懒加载/缓存/回退标记) +
>   hd2d_post_fx (bloom 链); GLSL 6 文件放 assets/shaders/
>   (随 POST_BUILD assets 拷贝, 桌面镜像自动覆盖)
>
> 已知限制: 雾不作用于岩浆 (自发光); blob shadow 非形状阴影
> (shadow map 列 v2d); bloom 参数为全局常量未自适应。
>
> 验证: 60/60 ctest + world_validator 0 err + sim 12×2 双跑零分岔
> (剔时间戳) + --sim 2 --hd2d 冒烟 exit=0 无 WARN/ERROR/FATAL。

# v1.4.20 — M6-v2b 完成: 3D 战斗表现层 B 档全量 (2026-09-12)

> v2b 切片收官。战斗反馈全链 3D 化: 投射物/预警/飘字/技能几何/危险区,
> 3D 模式下战斗信息不再缺席。逐项对齐 2D 配色与触发条件 (只读翻译)。
>
> ## 渲染 (rendering3d)
> - **投射物三态**: PROJECTILE_BODY 发光双球 (穿透金/敌元素火红冰蓝/玩家
>   土金); WARNING 相 AOE→贴地空心预警环 (红/橙/黄三级 + 脉冲),
>   点弹→TRAJECTORY_LINE 轨迹线 (撞墙截止算法与 2D _preview 同源) + 落点圈
> - **伤害飘字**: _render_damage_text 提取共用样式 (暴击1.6x/元素标签),
>   2D 相机偏移 / 3D world_to_screen 投影两路分发
> - **射程指示环**: WARNING_RING 复用 — NUNCHAKU 双环带 (内环+外环+淡带),
>   SPEAR/CROSSBOW 单环; 暖金色与 2D 同源
> - **Boss 技能预警** (只读 BossAI): 弹幕在飞弹道线 + 蓄力扇形预警
>   (CONE_FAN rlGL 三角扇, 朝玩家实时角度); 扇形斩蓄力面 (橙红);
>   瞬移落点紫圈; 旋风蓄力白环/旋转紫圈
> - **Boss 战场危险区**: 岩浆/影墙/虚空 贴地危险圈 (warn橙黄/active红,
>   boss_ctrl() 只读访问器)
> - **弱点光环 + Tank 守护连线**: WARNING_RING 橙脉冲环 + ENTITY_LINK 3D线
> - **空心环原语**: _draw_flat_ring (rlGL RL_LINES 圆周线段) —
>   DrawCircle3D 实心无法挖空心, 预警环全部走原语
> - **v2a 修复**: 传送门环朝向修正 ({0,1,0}/45° 朝相机竖立, 原 {1,0,0}/90°
>   是平躺贴片 — Code Review 抓获)
>
> 验证: 60/60 ctest + world_validator 0 err + sim 12×2 双跑零分岔 +
> --hd2d 与基线一致 (剔时间戳/HD2D行) + 冒烟 20s 无崩溃。

# v1.4.19 — M6-v2a 完成: 3D 表现层 A 档纯接线全量 (2026-09-12)

> v2a 收尾 (方案一: 3D 世界 + 2D 屏幕空间 UI)。7 项 A 档缺失全部接通,
> 3D 模式功能对齐 2D 的可玩闭环。
>
> ## 渲染 (rendering3d + game_scene 3D 桥)
> - **地面物品 billboard**: item_icon_key 图标 3D 化, 可见性同 2D (缺素材跳过)
> - **NPC billboard**: npc_sprite_key 楼层映射 (npc_views() 只读快照)
> - **挑战传送门**: PORTAL_RING 竖立脉冲双环 (DrawCircle3D, 入口蓝/返回绿,
>   颜色/脉冲频率与 2D 同源) + 地面基准圈
> - **HUD 参数补齐**: echo 面板 (F15 Ending Echo 镜像数据) + 挑战波次全量传入
>   — _build_echo_panel_data() 提取为 2D/3D 共用方法 (buff 腐化名表重构为
>   数据驱动 MAP, _fill_echo_buffs)
> - **UI 尾段共用**: _render() 的 370 行 UI (红屏/黑屏/挑战选择/小地图/背包/
>   赌博/对话/事件/冻结/时停/Boss 演出) 提取为 _render_ui_tail(sw,sh),
>   2D/3D 分支同 UI — 单一真相源, 消除双份维护
> - **3D 世界标签**: world_to_screen() 投影 (GetWorldToScreen);
>   怪名条 (Boss红/精英金/普通灰) + E 对话/拾取气泡, 样式与 2D 同款
>
> ## 结构
> - GameScene 新增只读访问器 (3D 红线): npc_views() / dropped_items() /
>   in_challenge_arena() — rendering3d 无 friend, 无可变访问
> - _render_hd2d_ui_bridge / _render_hd2d_world_labels (≤40行, 拆分合规)
>
> 验证: 60/60 ctest + world_validator 0 err + sim 12×2 双跑零分岔 (剔时间戳)
> + --hd2d 冒烟 20s 浸泡无崩溃 (每项增量验证 + 收尾全协议)。

# v1.4.18 — M6-v2a 第一刀: 3D 地形贴图 + billboard 帧动画 (2026-09-12)

> v2a 切片 (方案一: 3D 世界 + 2D 屏幕空间 UI) 开工。本刀: 地形/墙体接群系贴图,
> 实体接呼吸帧动画 + flip_x, 全部复用 2D 同源素材回退链, 零 shader 依赖。
>
> ## 渲染 (rendering3d)
> - 地板: rlGL 原语贴地 quad 采样 tile 贴图 (v1 的顶视 billboard 近似退役),
>   无贴图回退 DrawPlane 纯色
> - 墙体: 四侧面 rlGL quad 贴图 + 顶面亮 10% 伪受光; 无贴图回退 v1 纯色盒子
> - 贴图解析: 群系 wall_<biome>/floor_<biome> → 通用 → 程序化 (与 2D
>   GameMap::draw 同链, hd2d_scene_builder._resolve_tile_tex)
> - billboard 帧动画: `((int)(GetTime()*4))&1` 呼吸 2 帧轮换 (与 2D 实体同款,
>   GetTime 非随机, 不触 RNG 红线); flip_x 负宽源矩形接线生效
>
> ## 红线遵守
> - 视觉随机零新增 (帧驱动只用 GetTime); rendering3d 仍只读 GameScene
>
> 验证: 60/60 ctest + world_validator 0 err + sim 12×2 双跑零分岔 (剔时间戳)
> + --hd2d sim 与基线一致 + --hd2d 冒烟 20s 浸泡无崩溃 (HD2D 激活日志确认)。

# v1.4.17 — P1-C9: 3D包输入失灵调查结案 + --input-diag 诊断开关 (2026-09-11)

> 用户报告 3D 包 exe "进层后键盘失灵"。系统化排查后结案: f6019ee 无罪, 环境瞬态。
> 3D 模式入口补齐: HD2D 激活日志 + 桌面包 "3D模式" 快捷方式。
>
> ## 调查结论
> - 代码审查: f6019ee 输入链零改动, `--hd2d` 无参数时为死分支
> - PostMessage 全流程探针 (标题→选档→进层→Esc存档) 原包 exe 全通, 输入链完好
> - 用户复测诊断版: keys=1 到达 GLFW, focus=1 全程, 正常玩到第2层
> - 环境线索: 当日系统日志 VMware hcmon USB 驱动风暴 (3.4万条, 键盘=USB HID),
>   17:08/18:29 失灵会话为驱动层瞬态干扰, 与游戏代码无关
>
> ## 新增
> - `--input-diag` 启动参数: 主循环每 2 秒记录 [INPUT-DIAG] 键盘队列/焦点/
>   鼠标三态到 game.log — 键盘失灵复发时一跑即定位 (失焦/消息不达/状态卡死)
> - SceneTree::set_input_diag(); 默认关, 零日志噪声
> - HD2DRenderer::ensure_init 激活日志 (区分 2D/3D 路径, 排障可辨)
> - 桌面 3D 包根目录新增 "3D模式" 快捷方式 (roguelike_cpp.exe --hd2d)
>
> 验证: 60/60 ctest; --hd2d 启动 3D 激活日志确认; 无参启动 0 条 DIAG。

# v1.4.16 — M6-HD2D 切片: 3D 表现层骨架 (--hd2d 可切换) (2026-09-11)

> 大更新第一步: HD-2D 渲染切片落地。逻辑层零改动, 默认仍是 2D。
>
> ## 新增
> - `src/game/rendering3d/` 模块: HD2DRenderer (Camera3D 45° 俯视 + 分层绘制 +
>   后处理占位) + HD2DSceneBuilder (GameScene 只读状态 → 绘制列表纯翻译层)
> - `--hd2d` 启动参数: 3D 世界层 + 2D HUD 桥; 初始化失败自动回退 2D
> - 墙体盒子伪光照 / 地板分色 / 实体 billboard (精灵与 2D 同源) / 特效脉冲片
> - 设计文档: docs/M6_HD2D_RENDERING.md (含 v2 路线与一致性验证协议)
>
> ## 红线遵守
> - rendering3d 只读 GameScene, 无 gameplay 副作用
> - 视觉随机只吃 visual_rng (RNG-001/002)
> - sim 无头模式不进 3D (--hd2d 与 --sim 并存验证通过)
>
> 验证: 60/60 ctest + world_validator + sim 12×2 与 RNG-002 基线
> **剔除启动时间戳后逐行一致** (哈希对比教训: 时间戳行必假阳性)。

# v1.4.15 — P1-C8 结案: RNG-002 视觉掷骰流污染修复 (2026-09-11)

> v1.4.14 记录的 sim 间歇非确定性 (同 exe 同 seed 12~50% 批次分岔) 根因锁定并修复。
>
> ## 根因 (RNG-002)
> - `vfx_server.cpp` 5 个 VFX 函数 (lightning/explosion/smoke_puff/spark_burst/
>   blood_frenzy) 用 gameplay `rng()` 生成纯视觉粒子参数 — RNG-001 (_add_noise
>   偷吃主 rng 流) 的同族漏网。一次攻击特效 = 50~150 draws, 同帧事件路径差异
>   经此放大成 RNG 流永久错位 → 怪池/id/进层全雪崩
> - 证据链 (RNGSPIKE/RNGSITE 探针 + ASLR slide call 边界唯一确定法):
>   分岔 tick B 独有 117-draw spike, 调用点全解析为 VFX 内联掷骰 + 击杀链;
>   帧末状态哈希仍一致 (纯掷骰差, 无 gameplay 差异) → 流错位后雪崩
>
> ## 修复 (方案 A)
> - vfx_server.cpp 11 处 `rng()` → `visual_rng()` (RNG-001 同法, 独立视觉流)
> - 全部 P1-C8 探针移除 (game_scene.cpp FP/POS/SPD/RNGSPIKE/RNGSITE +
>   combat_system.h CountingRng 返回地址环形缓冲)
>
> ## 验证
> - 60/60 ctest + world_validator 全绿
> - **16 对并行 (32 进程批) --sim 12 --sim-seed 3 零分岔**, 同哈希
>   061A2BE6... (修复前同协议每批 1~4 对分岔)
> - 新基线: avg_floor=7.00 dmg_dealt=1987.2 (旧基线被 RNG-002 污染, 轻微
>   移动属预期; 后续对照以新基线为准)
> - 遗留: combat_coordinator.cpp:72 pre_hp 裸指针快照 (已证非本例根因, 清理候选);
>   "第一信号"上游机理未深挖 (32 批零分岔下未再现, 若复发按 WIP §9.1 方法论重启)

# v1.4.14 — M5 尾批: 共用图清零 — visual_id 数据驱动全量接线 + 潜伏者死规则修复 (2026-09-10)

> V1_4 审计缺口①③清零 (17/30 共用 orc + 3 个 F5 Boss 共用一图)。
>
> ## 修复 (Bug)
> - **潜伏者死规则**: monster.cpp 名字规则匹配"潜行者"但 enemies.json 实名
>   "暗影**潜伏**者" (UTF-8 字节不重合) → mon_shadow_stalker 专属图自 M5-B
>   接线以来从未命中 — 最高覆盖兜底怪一直渲染成 orc。修复 = 1 行 + visual_id
>   接线根治整类问题
>
> ## 视觉改动 (visual_id 数据驱动, 名字规则降级为回退链)
> - **F5/F10 Boss 分图**: 暗影骑士/亡灵法师/血族伯爵/地狱火魔 各获专属图
>   (boss_<visual_id>), Boss 立绘/战场精灵同链路; F15 镜像保持玩家形象
>   (设计意图)。旧 key boss_f5/boss_f10 保留兜底 (title_scene 仍用)
> - **enemies.json visual_id 语义修正 ×20**: 20 只怪的 visual_id 原是
>   "体型模板复用" (dark_mage 顶著 shaman 的 vid), 回归"自身视觉标识" —
>   这是"共用图"的另一半根源
> - **专属图 ×25** (m5_sprite_gen.py 扩展, 全 16x16 统一描边): 哥布林弓手/
>   冰霜史莱姆/电光之核/毒液蠕虫/魔像/亡语者/雷暴元素/血祭司/石像守卫/
>   铁卫/骨兵/骷髅弓手/哥布林猎手/暗术师/虚空行者/夜行猎手/冰狱守卫/
>   鲜血水蛭 + 4 Boss — 30 只怪全部一怪一图, 共用 orc 图时代结束
> - **_visual_to_color 色表扩展 ×20**: visual_id 修正后程序化占位/几何
>   回退的身体色同步专属配色
>
> ## 过程插曲 (诚实记录)
> - 冒烟对照发现 HEAD 预存在**间歇非确定性**: 12 连跑 3 次分岔 (F3 毒 tick
>   时序差 1 条), 同 exe 同种子 — 与本批无关 (分岔行号/模式与改动态完全
>   一致), 记 **P1-C8 候选**。v1.4.13 的"24 连跑一致"结论按间歇检出概率
>   需要重新审视
>
> ## 复核补遗 (M5-E 收尾, 同日)
> - **mon_elite_slime 缺图补齐**: 复核发现 "30 怪一怪一图" 漏了精英史莱姆
>   (visual_id 一直正确但图从未生成, 回退渲染成普通史莱姆)。金冠三尖
>   画法 + sprites.json 注册 — 交叉引用核查脚本现在 mon_ missing = []
> - **demon_lord 不补 (设计意图)**: F15 终焉回响 = 镜像玩家形象 boss_self,
>   visual_id 派生链正确跳过
>
> 验证: world_validator 0 错 + 60/60 测试 + asset_manifest 补测 + 冒烟
> (af=6.65/dmg=1668.8, 与视觉接线前 subset 逐值一致)。

---

# v1.4.13 — M5 视觉批: 三群系贴图 + 兜底怪专属图 + RNG-001 违规修复 (2026-09-10)

> V1.4 Roadmap M5 (数据驱动美术)。程序化生成 (tools/m5_sprite_gen.py,
> 复刻既有 48 张手工像素画的画法规律: 16x16/统一描边/高饱和主色)。
>
> ## 视觉改动
> - **三群系专属 wall/floor 贴图** (V1.4 审计的"最大杠杆"): 监狱 (石砖+青苔)/
>   火山 (玄武岩+熔岩裂缝发光)/深渊 (紫岩+发光符文+幽光裂缝), 全游戏不再一套贴图
>   tint 到底。GameMap 按 biomes.json id 选择, 回退链: 群系图→通用图→程序化
> - **兜底怪专属图 ×4**: mon_shadow_stalker (暗影潜行者, 监狱15%+深渊30% 双
>   出场)/mon_fire_imp (火魔, 火山30%)/mon_elite_orc (精英兽人, 火山25%)/
>   mon_shadow_assassin (暗影刺客, 深渊25%) — 名字规则扩展 (法师/守卫/兽人精英
>   分流), 兜底 mon_orc 覆盖面大幅缩小
>
> ## 修复 (本轮最大意外收获)
> - **RNG-001 违规**: `SpriteRenderer::_add_noise` 程序化纹理噪声吃的是
>   gameplay `rng()` — 素材命中与否 (渲染层差异) 直接污染 gameplay 随机流,
>   造成同 seed 间歇世界线分岔 (~7% 触发, M5 接线把它从潜伏炸到 2/3 概率才
>   暴露)。修复: 改吃 `visual_rng` (G9.3 规范的独立视觉流)。**24 连跑
>   确定性面板全一致**; 修复后冒烟与 P1-C7 结项基线逐值一致 (af=6.75/
>   dmg=1823.4) — 纯渲染层改动零 gameplay 影响, 完美自证
>
> 验证: world_validator 0 错 + 60/60 测试 + asset_manifest_test + 冒烟基线一致。

---

# v1.4.12 — P1-C7-A 结项: 双轨判定统一落地 + Boss reset 语义修复 (2026-09-10)

> 大更新批（四任务一天完成）：空手攻击正式迁入 WeaponExecutor 数据驱动轨。
>
> ## 玩法/系统改动
> - **空手/持械统一判定轨**（T1 B方案）: fist 走 weapons.json 数据驱动
>   (range 1.5=48px/recovery 0.5s 与 legacy 数学等值), 删 player_controller
>   legacy 分支 ~70 行。三败后的成功配方: **节奏保持, 只换实现**。
>   聚合验证 5 种子×100: af 1.60→1.66, dmg 196→201 (小幅偏好, CIRCLE 判定
>   圈内群杀 + executor 暴击梯度贡献)
> - **MCTS 感知统一**（T2）: build_sim_state 改读 weapon.can_attack —
>   空手/持械 AI 感知不再错轨; Player::can_attack/ATTACK_COOLDOWN 死代码删除
> - **Boss reset 语义拆分**（T3/P1-C7-C）: reset()→reset_floor()/reset_run(),
>   enter_floor/new_game 显式调用点补全。WIP "零调用"前提修正 — FLOOR_ENTER
>   EventBus 路径原本就在跑, 修复为零行为差 (逐字节实证)。replay_mem 语义
>   审计: init_on_spawn 每战重建, 无跨层持久项; mirror 跨局记忆走独立通道
> - **spear 局尾 special 残留嫌疑划掉**（T4）: 30 局探针 0 残留
>
> ## 过程插曲
> - T2 双跑分岔假阳性: bisect 排查 + 6 连跑 1 哈希证伪 — P1-C4
>   "间歇判定 N≥4" 手册教训再验证
>
> 验证: 基线复现/T1 冒烟+聚合/T2 6连跑+60测试/T3 逐字节+60测试/T4 探针 全过。
> 结项报告: docs/P1C7A_FINAL_REPORT.md

---

# v1.4.11 — P1-C7-A 会话4: 聚合判决, 迁移三连败坐实 (2026-09-09)

> 判决批：v3 迁移 (fist 48px/0.35s + 删 legacy 分支) 跑满 5 种子×100 局聚合 —
> **全面负回归 (af 1.60→1.30, dmg 196→99, s7 极端形态 dealt=2.1/局)** → 回退。
> 三次尝试 (v1/v2/v3) 翻在同一处: 0.35s 出手节奏改变 AI 决策相位, 而 Q3.15
> 决策权重表按 0.5s 节奏标定 — **迁轨前置条件是先重构决策权重表**。
> 诊断增量: ①chase 再平衡实验 (move 0.65 > attack 分 → AI 永不选 attack,
> 负回归回退) ②"run2+ 零出手"之谜解开 = 速死局果非因 (GATE2 采样缺失,
> 局生命周期 <600帧) ③决策层健康证明 (atk>0 211/288 样本)。
> 新工具: `tools/p1c7a_summary.py` (5 种子聚合器)。路线改道: 第 5 会话
> 推荐 B 方案 (fist recovery=0.5 节奏保持迁移) — 详见 WIP 文档。
> 工作区回退至基线 (af=6.25/dmg=1746.2 精确复现)。

---

# v1.4.10 — P1-C7-B 泄漏排查: 五假说全灭, 真因定位 (2026-09-09)

> 排查批：镜像冻结/UI/GameState/输入门/executor 门五个泄漏假说逐一探针证伪。
> 真因 = 迁移出手节奏 (0.5s→0.35s/击) 触发"战斗-搜刮时序再平衡"，串行批
> 混沌流中演化为 F1 全灭 — 与 P1-C6 教训同构 (出手时序深度耦合)。
> 附带发现: `BossSystemDirector::reset()` 全仓零调用 (设计意图未兑现) —
> 记 P1-C7-C。工作区回退至基线等价 (af=6.25/dmg=1746.2 复现)。

---

# v1.4.9 — P1-C7-A 会话2: 泄漏实锤 + 顺序调整 (2026-09-08)

> 判决性对照批：单局 ×5 种子证明迁移链路无害 (af 7.2 vs 7.0, 无 F1 灭);
> 串行批 run2 起空手局零出手 + 同 exe 双批逐字节一致 = **确定性批内局间泄漏**
> (基线与 v2 共有, 修它优先于迁移 — 立项 P1-C7-B)。
> WIP 文档大更新: 单局/串行对照表、已排除项、泄漏嫌疑清单 ×5、定位法。
> 工作区回退至 e542af0 行为等价 (af=6.25/dmg=1746.2 精确复现)。

---

# v1.4.8 — P1-C7-A 双轨判定统一: WIP 交接 (2026-09-08)

> 进行中批：空手 legacy 轨→WeaponExecutor 迁移一版冒烟 run0/1 起飞 (F10/F11)
> 但 run2-19 空手 F1 全灭 → 回退。链路探针已证 executor 命中/节奏正常，
> 遗留谜团与下场清单见 `docs/P1C7A_DUAL_TRACK_UNIFICATION_WIP.md`。
> 工作区回退至 e542af0 行为等价 (af=6.25/dmg=1746.2 精确复现)。

---

# v1.4.7 — P1-C6 冷却感知出手时序实验: 四版负回归拦截 (2026-09-08)

> 方法论批（续 C5）：C5 处方"冷却感知决策"经四版参数/结构实验全部证伪，逐一回退。零行为改动落地。

## 实验与判决 (详见 `docs/P1C6_COOLDOWN_AWARENESS_NEGATIVE_RESULTS_REVIEW.md`)

- **v1 全 ε=0.02 让渡** (af 6.25→6.05, dmg -12%): 挨打窗口全部让渡给
  撤退/喝药 → 生存↑输出↓ 同源置换 (taken -11%, heal -34%)。
  **d3=77% 边缘圈站桩不是纯病理 — 是"最大化输出姿态"**，莽性是当前
  平衡的隐性成分
- **v1.3/v1.4 比例衰减 30%/50%** (af→3.85/1.40, F1 灭 11/18 局): 冷却期
  攻击分低于拾取 1.6/撤退 0.6 → 持械残局"捡逃死循环" (3 次传送零出手)
- **防御性门全部零命中**: special 追击期排除 + 单怪步进门在 s3 冒烟下
  三版 20 局 log 逐字节一致 — 分岔从 run3 起混沌传播
- **步进门是 F1 全灭放大器**: 衰减+步进 af 1.40 vs 只衰减 4.45

## 新发现 (记入 P1-C7 输入)

- **双轨攻击判定**: 空手 legacy 0.5s CD vs 持械 0.15s CD — 冷却感知的
  感知源错轨，统一判定是冷却类方案的**前置修复** (S 工作量)
- **跨 run 世界状态无隔离**: 同 seed 冒烟 run0-2 与基线逐字节一致、
  run3 起分岔 — 单局对比无效，新增"run 边界对齐 + 首分岔定位"方法论
- 回退后基线精确复现: af=6.25 / dmg=1746.2 / d3=77.1% (n=118) ·
  ctest 60/60 · validator 0 错 0 警 — 500 局历史数据延续有效

---

# v1.4.6 — P1-C5 攻击圈对齐实验: 三项负回归拦截 (2026-09-08)

> 方法论批：三项"显然正确"的改动在冒烟数据下全部暴露为负回归，逐一回退。零行为改动落地。

## 实验与判决 (详见 `docs/P1C5_ATTACK_ALIGNMENT_NEGATIVE_RESULTS_REVIEW.md`)

- **决策圈对齐武器半径**：6 版参数扫描全部负回归 (af 6.25→1.35~3.60)。
  核心发现："边缘圈空挥"实为**预判性试挥** — 判定 48px > FIST hit 32px，
  试挥中冷却流转，怪进圈瞬间命中；收窄判定后出手时机被 move 步进抢占。
  **与 Q3.15 风筝同构的时序耦合，判定圈不可单点改**。
- **current_stage 动态段 reach**：同 seed **3 种结局随机** (继 P1-C4 后第二个
  "同 seed 多结局"缺陷)。五层探针收网到 kill #117 目标选择分岔 (同帧同 rng
  序列下 A 杀兽人#142/B 杀史莱姆#143)。回退 stages[0]，根因记 P1-C6 专项。
- **空手武器追击**：F1 怪密度下直线穿怪=送头，理论风险确认；基建保留。

## 落地 (零行为差异, 已验证)

- GroundSpot.is_weapon + `_near_weapon_loot_dist` + `_decision_attack_reach_px`
  + `_is_bare_fisted` — 决策基建四件套
- C4PROBE 相对化刻度 (d×4/reach)：**d3 (75-100% 判定圈) = 77.1%** — AI 攻击
  决策 77% 集中在边缘圈，消它需出手时序层方案 (P1-C6: 冷却感知决策)
- 500 局 log 与 P1-C4 逐字节等价 (13332 行仅 1 行时间戳差) — 基线数据延续有效
- 验证：4 连跑 MD5 一致 · 基线 6.25/1746 精确复现

---

# v1.4.5 — P1-C4 时停 UAF 根治 + 模拟器确定性恢复 (2026-09-08)

> 本批核心产出：发现并修复 P1-C3 全部历史 500 局数据的**地基缺陷**——同 seed 双结局。

## 改动

### UAF 根治 (game_scene.h / game_scene_combat.cpp / player_controller.cpp / combat_coordinator.*)

- **根因**：`pending_damage` 存裸 `Monster*`。时停期间目标怪被 cleanup/kill 路径
  erase 释放 → 结算时悬空。堆地址被新怪复用会**欺骗 valid 检查把伤害打错怪**；
  常规失效则**伤害凭空丢失**（84 点挂起伤害消失 → 兽人不死 → 世界线分裂）。
  是否触发取决于进程堆布局（Windows ASLR）——纯运行期运气
- **修复**：`pending_damage` 改存 `instance_id`，结算按 id 在 monsters 中查找，
  id 失效诚实跳过。删除零调用者的死代码 `CombatCoordinator::apply_pending_damage`
- **验证**：同 seed 8 连跑 MD5 全一致（修复前 ~50% 概率分岔成两种结局）

### SimAI 贴脸拉开分 0.9→0.6 (sim_ai.cpp)

P1-C3 数据：270 局 F1 围殴死 100% 零杀（0.9 分撤退持续压过贴脸攻击 0.67，
全程逃命被咬死）。0.6 让贴脸攻击反超 → "逃一步打一下"轮换。

### C4PROBE 常驻探针 (sim_ai.cpp / sim_runner.cpp)

攻击评分命中距离分布：**d1（32-48px 边缘圈）占 82.2%**，d0 稳定出手区仅 17.8%
——F1 围殴死亡真因定位为"攻击圈边缘站桩"，是 P1-C5 贴脸步进泛化的直接依据。

## 定位过程（五层探针收网，详见 `docs/P1C4_UAF_DETERMINISM_DATA_REVIEW.md`）

传送指纹 → 帧级 FPDIAG → KILLDIAG → 时停/挂起计数 → PENDDIAG（DANGLING 实锤）。
每层一个可证伪假设。**教训：MD5 双跑验证对间歇性缺陷是假阳性，
N≥4 次重复才可信。**

## 500 局对比（P1-C3 → P1-C4）

- 胜利 0 → **1/500**（s19 首胜 ⭐）；F1 死亡 90.2%→91.0%、TWall 7.8%→7.0% 持平
- 判读：本批是**地基修复批**非调优批——数值持平符合预期；
  F1 围殴 91% 仍是最大瓶颈，C4PROBE 已精确到"边缘圈站桩"
- 验证：60/60 ctest · Validator 0 错 · 8 连跑 MD5 一致（s3）+ 4 连跑（s7）

---

# v1.4.4 — P1-C3 楼梯导航 + 层级搜刮预算 (2026-09-07)

> C3DIAG 探针定位"清层不下楼"死锁：AI 返回 descend 但从不导航去楼梯格，站原地按 E 600s。

## 改动 (sim_ai.cpp/h + game_scene.cpp)

- `_bfs_to_stairs`：BFS 导航至楼梯格，在格才返回 "descend"（原 `_check_floor_transition`
  只认"站在楼梯上按 E"——AI 从不走路，9/20 冒烟局困死于此）
- `set_stairs_pos` 每帧只读注入楼梯坐标（`set_ground_items` 同契约）+ 换层重置搜刮状态
- 层级搜刮预算 15s：stairs 激活起计时，超时放弃余下搜刮直奔楼梯
- `_loot_abandoned` 锁定：看门狗触发后不再回头搜刮（原 descend 后又走向搜刮房死循环）
- C3DIAG 常驻探针（墙超时分支快照，与 P0DIAG 同构）

## 方法论记录（本轮核心产出）

- **无进展看门狗传送方案两轮迭代均产生传送风暴**（273 次/20 局）——回退。
  教训：没有复现个案前不写修复，探针先行。
- **混沌重排陷阱**：楼梯修复后 s3 deep 22→5 表面回归；MD5 对拍证明无注入时与
  P1-C2 逐字节一致——差异全部来自"AI 真的下楼了"之后的 RNG 分岔。预算 15s→25s
  实验证明调参不单调——**以结构指标定参，不拟合种子噪声**。

## 500 局对比（P1-C2 → P1-C3）

- 清层不下楼死锁：45% TWall 局 → **1/500 局**（根除）
- TIMEOUT_WALL 16.8% → **7.8%**（-9pp）；节奏 x7
- 深层率 14.2%→8.2%（真实代价：节奏快→资源积累少→围殴死；但 P1-C2 的 deep
  近半是"TWall 耗尽"局，非胜利路径）
- 瓶颈清晰化：F1-F2 围殴 90.2% 是 P1-C4 主攻方向（战斗效率，非导航）
- 验证：60/60 ctest · Validator · 双跑 MD5 · 无注入对照 MD5 与 P1-C2 逐字节一致

---

# v1.4.3 — P1-C2 SimAI 毒对策 (2026-09-07)

> 决策层两条规则，零数值改动。DOT 是基线最大单一死因（33.6%），AI 此前对自身中毒状态全盲。

## 改动 (sim_ai.cpp)

- 中毒时药水线 0.35→0.55：毒 tick 3-6/0.5s，35% 线才喝必然被追上（喝 30HP 同时毒继续吃血）
- 自身中毒时攻击毒源怪 +0.25 分（orc/elite_orc/poison_wyrm，读 `on_hit_triggers` 数据）
- 感知源纯只读：`Player::active_buffs` + `Monster::on_hit_triggers`，无新耦合

## 500 局对比（P1-B → C1 → C2 累计）

- F1 死亡 96.6%→**82.4%**；深层率 3.2%→**14.2%**（×4.4）；最深 F11→**F14**
- 武器获取局 ×6（18%）；Boss 击杀局 ×22（44 局）；picks/kills 均 ×5.3
- DEATH_DOT 绝对数持平但结构改善：F1 死亡 -5.8pp 精确转化为深层 +5.8pp
- TIMEOUT_WALL 16.8% 成最大增长项 = P1-C3（拾取/搜刮效率）的接力信号
- 验证：60/60 ctest · Validator · 双跑 MD5 一致 · 详见
  `docs/P1C2_POISON_COUNTERPLAY_DATA_REVIEW.md`

---

# v1.4.2 — P1-C1 F1 教学层数值带下调 (2026-09-07)

> P1-B 基线判定"0% 通关是 F1 平衡数值问题"。本批仅动 2 处配置级数值，零逻辑改动。

## 数值改动 (floor_config.cpp + game_scene.cpp)

- F1 怪物数量 4→3，archer 权重 8→2（远程骚扰对走位差 AI 不公平压力）
- 初始治疗药水 2 瓶→3 瓶（毒 DOT 单次 24HP，2×30HP 不够对冲 2-3 次中毒）
- 不动：F2+ 全部楼层、growth_curve、怪物基础数值、毒 buff 强度

## 500 局对比（同 5 seeds × 100，`reports/p1c1/`）

- F1 死亡率 96.6%→**88.2%**；深层局（≥F3）3.2%→**8.4%**（×2.6）
- Boss 击杀局 2→**20**（max 双 Boss）；TIMEOUT_WALL 2.6%→9.8%（活到兜底的副产品）
- picks 0.49→1.39/局；DEATH_DOT 33.6% 持平（毒源未动，属 P1-C2）
- 判定：拐点已过但未达 20-30% 目标带；剩余瓶颈应交给 AI 行为修复（C2 毒对策/
  C3 拾取主动性）而非继续压数值（教学层 3 只怪已近下限）
- 验证：60/60 ctest · Validator 0 错 · 双跑 MD5 一致 · 详见
  `docs/P1C1_F1_TUNING_DATA_REVIEW.md`

---

# v1.4.1 — P1-B 500 局基线 + NPC 对话越界崩溃修复 (2026-09-07)

> P1-A4 三死锁修复后的正式 500 局基线采集；跑批过程发现并修复一处真玩家可触发的越界崩溃。

## P1-B-fix — NPC 二次对话越界崩溃（4/5 种子跑批必现）

- **表象**：`--sim 100 --sim-seed 3` 在 run 4 楼层切换后 SIGSEGV（exit 0xC0000005），4/5 种子在前几局崩溃
- **诊断**：gdb 栈 `strlen ← GameSceneInteraction::start_dialogue ← GameScene::_input`
- **根因**：`start_dialogue` 用 `i<5` 统一遍历对话池，但 `repeat_dialogue` 只有 2 槽且全满（无 nullptr 终止）→ NPC 重谈（`met=true`）越界读 3 个野指针
- **暴露链**：P1-A4 让 sim 能自动推进对话 → `met=true` 残留 → 下局重谈走 repeat 池 → 越界。**真玩家任何 NPC 二次交谈同样触发**（与 sim 无关）
- **修复 A**：`std::size` 按池真实容量遍历
- **修复 B**：`spawn_floor_npcs` 落位写错槽（`_npc_count-1` 是"最后创建"槽而非本 NPC 槽）→ 按 npc_id 定位
- **修复 C**：`new_game()` 不清 NPC 状态（`_npc_state/_npc_count/_dialogue` 跨局残留）→ 统一重置
- **修复 D**：BalanceReport `avg_*` 均值 int 截断（"1 局 2046 伤害 + 99 局 0" 算出 avg=0）+ `avg_heal` 漏序列化 → 改 float

## P1-B — 500 局正式基线（5 seeds × 100，`reports/p1b/`）

- 5×100 全 exit=0；同 seed 双跑 MD5 一致；60/60 ctest；Validator 0 错
- **胜率 0%**，96.6% 死于 F1：DEATH_MONSTER 61.4% / DEATH_DOT 35.6% / TIMEOUT_WALL 2.6%
- **攻击消费链复活但极窄**：picks 0→0.49/局，深层局（≥F3）0→16/500（3.2%），最深 F11（1 局双 Boss 击杀，55 杀 5383 伤害）
- 拿到武器的 15 局全部进入 F3+（fist_basic avg_floor=1.0 vs 真武器 2.5-8.0）——"活得久→捡到武器→活更久"正循环被 F1 数值掐断
- **结论**：M4 时 0% 是"AI 坏了"；P1-B 后 0% 是"F1 平衡数值问题"——16 个深层局证明链路走通后 AI 能推进。详见 `docs/P1B_BASELINE_DATA_REVIEW.md`
- **v1.5.0 Release Gate 依据**：崩溃修复必须发版；F1 数值带下调（P1-C1）建议在 Release 前完成

---



# v1.3.2 — G10 视觉链闭环 + P0 sim 死锁修复 + F15 平衡 (2026-09-01)

> G10.3→G10.7 视觉垂直切片全线贯通：游戏内空间可信度 + 游戏身份层。
> 期间穿插 P0 运行时死锁调查（sim 永久卡 F1）与 F15 镜像 Boss 平衡修复。

## P0-M1/M2 — Sim 永久 F1 死锁 (`0e3023c`)

- **初始误判**：疑似堆损坏（0xC5 = 诊断环境缺 MinGW DLL 的烟雾弹）
- **真根因**：v1.2.3 房间边界 × Encounter LOCKED 封门 × 旧传送落点不限房间 → 怪永久 IDLE + 玩家够不着 → 900s 死循环
- **修复**：`sim_ai_teleport_target()` 房间归属契约（分层落点/排除墙锁占用）+ 卡死检测锚点半径化 + 3 契约回归测试
- **红线达成**：同 seed sim 逐字节可重复（MD5 一致）、零 900s 死锁；SEH 崩溃处理器升级为符号化栈跟踪

## G10.4 — 地图视觉辨识度 (`c1026d0` · `018ef47`)

- 群系 tint 接线（监狱棕/火山橙红/虚空紫，色相主导替代亮度递减）
- 确定性 (x,y) 哈希变体 6% 污渍/4% 亮石块；墙底受光高光
- A.1 校色：floor.png 中性灰载体 + palette 亮度带统一 78-87

## G10.4-B — 战斗反馈 (`7be4700`)

- 命中 HitStop 分级（KILL>CRIT>HEAVY>LIGHT 单次不叠加）+ 命中音回归武器路径
- 暴击数字 alpha 溢出 bug 修复（0.6f 硬编码除数 → max_lifetime 字段）+ 1.6× 字号接线
- SWORD stage2 终结感（flash 0.18s/爆炸火花 12）

## G10.5 — 战斗空间统一 (`ed4d00d`)

- **AttackGeometry SSOT**：判定与 VFX 共用同一几何（origin/shape/range/width）
- 长矛 224px 突刺轨迹、双节棍 128px 范围环、短剑视觉收正、弓弩脚底冗余特效删除
- 斩弧 30° 斜置修正、矩形判定 +16→14、特殊段每击命中 VFX

## G10.6 — 玩家环境真实感 (`759f872`)

- C1 朝墙姿态衰减（前倾×0.3/重击 1.08/武器锚点减半）——攻击照常、仅视觉入墙收敛
- C2 停靠贴合（二分逼近最大合法位移，判定函数零改动）——停靠抖动 [0,3.33px)→稳定

## G10.7 — 游戏身份层 (B1-B4)

- **B1 图标管线** (`35a5ff8`)：剑与门 256 图标（16×16 逻辑像素+游戏色板）→ 六尺寸 .ico/.rc/CMake WIN32 GUI 子系统/SetWindowIcon/README logo；GUI 子系统 stdout 保活（dup2）
- **竞技场专属 BGM** (`eb781be`)：challenge 曲 160bpm C 小调急促波次战斗风
- **B2 舞台层** (`15a10ce`)：纵深渐变/透视地板/两侧石墙/拱门红光/火把余烬/径向 vignette
- **B3 角色层 v2** (`4c5dc7c`)：全画布 20 素材海报散布（左右纵深队列+顶部剪影带+暗影骑士出血裁切+底部战利品带），中央菜单净空
- **B4**：全屏菜单项点击修复（_activate 补 fullscreen 分支）

## F15 镜像 Boss 平衡三部曲 (`def9614` · `24e61ae` · `8bc602e`)

- 射程钳制：Echo 近战 ≤96px（禁复制长枪 224px）、弩 ≤220px
- Echo 时停窗口内自身伤害减半；玩家时停可见性（三重紫环+音效+常驻施法环）
- **数值护栏**（game.log 实锤 atk=11119 单击 3883）：HP ≤2000、ATK ≤玩家HP/8——威胁模型 6-8 击致死

## 验证

- **60/60 ctest 全绿**（+p0_teleport_test 3 契约）
- 同 seed sim 逐字节可重复；桌面版全程同步

---

# v1.3.1 — G9 审计闭环 + Asset Manifest + 字体迁移 + 竞技场/音效修复 (2026-08-30)

> v1.3.0 之后的技术债清理与基础设施升级批次，涵盖 G9 生命周期审计、
> 资源管理管线重构、中文字体替换、竞技场战斗修复与外部音效加载。

## G9.2–G9.4 技术审计闭环 (`b3143ec`)

- **G9.2 生命周期残留修复** — 清理 Monster/Player 析构顺序问题，UAF 防护补全
- **G9.3 RNG 边界隔离** — `CountingRng` 边界检查 + 确定性回归验证
- **G9.4 DoorState Truth Table** — 门状态转移表形式化验证，覆盖 4×4 状态组合

## G10.2-B1 Asset Manifest 基础 (`8f24e58`)

- `resources/sprites.json` schema v2：新增 `"assets"` 段，声明外部资源路径/来源/回退
- `ResourceManager::asset_by_id()` ID 查询 API + `load_assets_config()` 懒加载
- `world_validator.py` 新增 assets 段校验规则

## G10.2-B2 Door 迁移 (`683cee0`)

- DoorRenderer 4 种贴图从硬编码路径迁移至 Asset Manifest（`door.open/closed/locked/sealed`）
- 消除 4 处硬编码 `.png` 路径

## G10.2-B3A Audio Manifest + WAV 转换 (`9b8bd91` · `6e6c6a4` · `eaadd9a` · `8a559a5`)

- `sprites.json` 新增 `"audio"` 段：`timestop` / `domain_expand` 声明外部音频路径与回退
- **根因修复**：`InitAudioDevice()` 原在 `InitWindow()` 之前调用，导致 `LoadSound()` 对外部文件静默失败 → 移入 `SceneTree` 构造函数 `InitWindow()` 之后
- **MP3→WAV 转换**：`jojo_timestop.mp3` → `.wav`（44100Hz stereo 3.55s），`domain_expand.mp3` → `.wav`（5.47s）
- **ResourceManager 时序修复**：`AudioServer::init()` 在 `ResourceManager` 加载 `sprites.json` 之前运行，`asset_by_id()` 返回 nullptr → 改为硬编码相对路径直接加载，WAV 优先 MP3 回退
- 加载链：`LoadFileData()` → `LoadWaveFromMemory()` → `LoadSoundFromWave()`（绕过 `LoadSound()` 文件 I/O 问题）

## G10.2-B3B 字体迁移 (`3a902b1`)

- 替换系统字体为 `assets/fonts/NotoSansCJKsc-Regular.otf`（思源黑体，OFL-1.1 许可）
- 1831 码位（1729 CJK + 102 符号），per-codepoint 逐字验证 100% 覆盖
- `ResourceManager::_init_font()` 简化为单一字体路径，零系统字体依赖
- `font_codepoints.h` 移除 4 个不支持符号（☠⚔✗❄ + BOM），`tools/extract_chars.py` 同步更新
- `resources/landmarks.json` 等 JSON 中 ⚒→锤、⚔→剑 符号替换

## 挑战房怪物多样化 (`c6b4b02`)

- 新增 `_pick_monster_type(floor, wave, rng)` 按楼层/生态群落/波次分池：
  - 地牢 F1-5：slime/skeleton → orc/shadow → elite/charger/summoner
  - 熔岩 F6-10：fire_imp/bomber → orc/shaman → storm/golem/necro
  - 虚空 F11-15：shadow/void/dark_mage → ice_warden/priest → guardian/sentinel

## 竞技场修复 (`afb002e` · `0359b7f`)

- **时停修复**：竞技场 `m->update_ai()` 未检查 `time_stop_remaining` → 移除重复 AI 调用，由 `_player_ctrl.tick()` 内部门控统一管理
- **WeaponExecutor 补全**：竞技场新增 `tick_specials()` + `tick_projectiles()`（弩箭/矛/双节棍特殊攻击状态不再卡死）
- **进化名乱码修复**：`evo_name()` 返回 `const char*` 但内部 `get_evolution_text()` 返回临时 `std::string`，`.c_str()` 为悬垂指针（use-after-free）→ 改为返回 `std::string`
- **地面掉落修复**：`_draw_ground_items()` 在竞技场被 `WorldMode::CHALLENGE_ARENA` 门控跳过 → 开启渲染 + `enter/exit_challenge_arena()` 保存恢复 `ground_items` 防止坐标污染

## 验证

- **59/59 ctest 全绿**（新增 font_manifest_test 等资产管线测试）
- 音效验证：时停/领域展开 WAV 在 Release 构建下正常播放
- 竞技场验证：地面掉落可见可拾取、退出后地牢物品恢复

---

# v1.3.0 — Batch 3: 经济系统 + 赌徒房 + 挑战房 (2026-08-29)

> v1.2.x 门/房间遭遇系统之上的内容扩展批次（3A→3I），引入金币经济与两类可重复特殊房。
> 设计: `docs/BATCH_3A_ECONOMY_REWARD_DESIGN.md` · `BATCH_3B_GAMBLE_ROOM_PLAN.md` · `BATCH_3E_CHALLENGE_ROOM_FINAL_PLAN.md` · `BATCH_3I_CHALLENGE_PORTAL_PLAN.md`

## Batch 3A — 经济与持久化基础

- `PersistenceScope(FLOOR/RUN)` 圣物作用域；Player 金币/钥匙字段
- `get_sell_value` / `sell_item`（装备出售）；RewardManager 统一奖励发放
- **Save v4**（跨版本兼容追加字段）；HUD 金币/钥匙显示；5 测试

## Batch 3B — 赌徒房 MVP

- 金币开房（40 + floor×10）；奖励池 75% 装备 / 20% 钥匙 / 5% RUN 圣物
- 奖励耗尽回退钥匙；`is_repeatable` 旁路特殊房一次性限制；8 测试

## Batch 3C — 背包出售 UI

- `[T]` 出售选中装备（sell_selected_item 静态 helper）+ 键位提示更新；8 测试

## Batch 3D/3E — 挑战房审计与设计冻结

- SpecialRoom/Key/Room/Monster/Reward 全系统架构审查
- 设计冻结: 7 阶段状态机 + ChallengeRoomController + 3 波×4 怪 + 确定性 RNG 派生 + 背包满奖励 fallback

## Batch 3F/3G — 挑战房 MVP + HUD

- ChallengeRoomController: 7-phase 状态机、出口附近放置、波次战斗、HUD wave 显示（3F，14 测试）
- 挑战房 HUD: 进度条 + 击杀计数 + 剩余波次 + floor 横幅 + Boss 变体（精英/双怪）（3G，10 测试）

## Batch 3H/3I — 传送门系统 + 竞技场修复

- **传送门系统**: 出口传送门生成 → E 键交互 → 选择面板 → 独立竞技场地图 → 返回传送门
- **Arena 移动式架构**: 进场保存地牢怪物/地图状态，退场恢复
- 竞技场内战斗修复: 武器 recovery_timer/冷却正常推进；VFX/连招/演出 tick（特效与屏震正确过期）
- E 键返回直查 `challenge return_portal_tx/ty`（竞技场无 special_rooms）；ESC 退竞技场跳过存档但允许回标题
- 性能: O(n²) 怪物列表构建移出内层循环；字体扩容（1835 codepoints）
- `challenge_portal_test` + `challenge_room_test` 合计 53/53 全绿

## 期中热修复

- 字体 codepoints 修正 + 赌徒房奖励洗牌修复 + 背包金币显示（`3a9c13b`）
- 赌徒房生成侧 + 全屏缩放 + 特殊房放置（`a6ed40a`，Batch 3G）

## 验证

- **53/53 ctest 全绿**（新增 economy/challenge_room_test、challenge_portal_test 等 36 测试）
- GitHub Actions CI 通过；README 结构/键位/存档 v4 同步更新
- 实机验证: 传送门进出竞技场、波次战斗、返回恢复地牢状态

---


# v1.2.6 — Boss FOV + 弹幕穿墙 + UI 键位提示 (2026-08-28)

## Boss FOV 视野系统

- **Tile** 新增 `boss_visible` 字段
- **GameMap::update_boss_fov()** — 360°射线投射，Boss 周围8格视野，受墙壁遮挡
- **主地图**：Boss 视野区域叠加红色半透明覆盖（已探索+boss可见+不在玩家视野）
- **小地图**：Boss 视野绘制 — 未探索区域暗红色，已探索区域红色叠加
- 每帧追踪 Boss 实际位置并更新 FOV

## 弹幕墙体碰撞

- `Projectile` 新增 `pierce_walls` 字段（默认 false）
- 玩家/怪物弹幕 tick 循环加 `is_walkable` 墙体检测，碰墙销毁
- 弩箭 power shot（stage 3）`pierce_walls = true` 可穿墙
- Boss BarrageSkill 已有独立墙体碰撞（Shot struct）

## UI 键位提示

- 战斗 HUD 右下角：`[R]圣物 [B]背包 [F1]日志 [M]地图 [ESC]保存`
- 标题画面操作说明：新增 `M - 小地图`
- 小地图面板下方：`[M] Map` 提示

## 测试

- 44/44 ctest 全绿

---

# v1.2.5 — Door Visual System: Kenney 素材 + 状态动画 (2026-08-28)

> 4种门状态4种外观 + 0.3秒过渡动画

## DoorRenderer (新增 `src/game/rendering/door_renderer.h/.cpp`)

- **素材**: Kenney Tiny Dungeon 16×16 tile (tile_0003/0018/0022) → 32×32
- **OPEN**: 空拱门 `tile_0003` — 暗色拱形通道
- **CLOSED**: 木门 `tile_0022` — 棕色木门+把手
- **LOCKED**: 木门 `tile_0022` + 红色锁 icon (代码绘制)
- **SEALED**: 深色门 `tile_0018` + 紫色封印十字纹 (sinf 脉冲)
- **动画**: 状态切换触发 0.3s 过渡 — 旧 tile alpha 渐隐 + 新 tile alpha 渐显 + scale 0.8→1.0
- 叠加标记只在最终状态绘制，过渡中不画

## 集成

- `GameMap::set_door_state()` 自动触发 `DoorRenderer::on_state_change()`
- `GameMap::draw()` DOOR 分支 → `DoorRenderer::inst().draw_door()`
- `GameScene::enter_floor()` 初始化, `_process()` 每帧 update

## 测试

- `door_renderer_test`: 7 case — 单例/init安全/绘制不崩溃/动画状态切换/过渡完成
- 44/44 ctest 全绿

---

# v1.2.4 — Batch A: 门交互 & 碰撞修复 (2026-08-28)

> 分离视觉/碰撞尺寸 · E 键开门 · LOCKED 门语义 · SimAI 门处理

## Entity 碰撞/视觉分离 (A1)

- `Entity` 新增 `collision_size` 字段 (28×28)，独立于 `size` (32×32)
- `sync_rect()` 将碰撞矩形居中放置于视觉矩形内
- `draw_rect()` 仍返回视觉尺寸 (32×32)，攻击判定用 `rect` (28×28)
- Player 构造: `entity(x, y, 32, 32, 28, 28)`

## 锁门 API (A1)

- `GameMap::lock_room_doors()` — 设置 DoorState::LOCKED (不可 E 键打开)
- `blocks_sight()` 改为 `door_state != DoorState::OPEN` (LOCKED 也遮挡视线)
- `close_room_doors()` → CLOSED, `lock_room_doors()` → LOCKED, `open_room_doors()` → OPEN 严格对称

## E 键开门 (A2)

- PlayerController 拾取处理中新增 E 键 → 检测4方向 CLOSED 门 → 打开
- 仅响应 CLOSED 门 (LOCKED/SEALED 不可 E 键打开)

## 删除自动撞门 (A3)

- `_update_player()` 中 `try_open_door_toward()` 调用已移除
- 现在必须显式按 E 键开门

## Room Encounter → LOCKED (A4)

- `RoomManager::_try_lock()` 改调 `lock_room_doors()` (原 `close_room_doors()`)
- 怪物房间封锁用 LOCKED 语义，防止 E 键误开

## SimAI 门处理 (A5)

- `_tile_rect_walkable`: CLOSED=true (可穿透), LOCKED=false (绕路)
- `best_action()`: 目标 tile 为 CLOSED 门 → 返回 "pickup" (先开门)
- LOCKED 门对 Sim 不可规划

## 硬编码修正 (A6)

- 8 处 `+16` 改为 `rect.width / 2` / `rect.height / 2` (game_scene.cpp ×6, sim_ai.cpp ×2)
- 测试 `place_player()` 修正碰撞中心偏移 (room_encounter_test T3)

## 新增测试 (A7)

- `entity_center_test`: 视觉中心=碰撞中心不变量、sync_rect保中心、Player碰撞尺寸、draw_rect用视觉尺寸

## 验证

- 43/43 ctest 全绿; 构建 0 警告; sim smoke 通过

---

# v1.2.3 — 怪物房间边界约束 + 小地图位置修正 (2026-08-28)

## 怪物房间约束

- **AI 视觉**：`_decide_state()` 增加 `room_at()` 检查 — 玩家不在怪物房间时强制 IDLE，禁止 CHASE/ATTACK
- **AI 移动**：`_apply_movement()` 目标 tile 不在怪物房间内 → 阻止移动
- **远程技能**：SNIPER/RAPID_SHOT/SCATTER/CONTROLLER 跨房间 → 跳过攻击
- **传送技能**：AMBUSH/CHARGE/LEAP 目标不在房间内 → 跳过
- **脱困传送**：`_unstuck_wedged_monsters` 只传送到怪物自己房间内，无有效 tile 则重置到 home
- **架构**：`MonsterAI::update()` 新增 `monster_room`/`player_room`/`room_mgr` 参数，由 `GameScene::_update_monsters()` 计算传入

## 小地图位置

- 面板上移 26px (`sh - MINIMAP_HEIGHT - 14` → `sh - MINIMAP_HEIGHT - 40`)，不再遮挡底部快捷键提示 `[R]圣物 [B]背包 [F1]日志 [ESC]保存`

## 验证

- 42/42 ctest 全绿; 构建 0 警告; 桌面已同步

---

# v1.2.2 — Batch 2C: Room Encounter (进房→封门→清房→开门) (2026-08-28)

> 在 A1 密封拓扑 + DoorState + R1 接触开门之上, 实现以撒式房间战斗状态机。
> 设计: `docs/BATCH2C_ROOM_ENCOUNTER_DESIGN.md` (已审核)

## RoomManager (新增 `src/game/world/room_manager.h/.cpp`)

- **状态机**: IDLE → ARMED → LOCKED → CLEARED
  - 玩家进入有怪房 → ARMED → (E1 无压门 / E2 房怪在房内 / E3 原子关门) → LOCKED 全门 CLOSED
  - 房内怪清零 → CLEARED 门 OPEN
- **性能约束** (用户审核): 只维护/检查当前激活 Encounter, IDLE 房间零扫描; 玩家跨 tile 才检测
- **映射固化**: build() 时一次性建立 房间矩形 + 门组, 运行时不搜索门
- **解耦**: 通过回调 (on_locked/on_cleared) 通知 GameScene — 可单元测试

## Door Group API (GameMap)

- `close_room_doors(door_tiles)` / `open_room_doors(door_tiles)`: 多门房间原子开闭 (E3)

## EventBus

- +`ROOM_LOCKED` / `ROOM_CLEAR` (2 枚举)

## 边界规则

- E1 实体压门 → 暂缓落锁 | E2 房怪门外 → 暂缓 | E3 多门原子
- E4 Boss 房跳过 (现有 BOSS_INTRO 流程) | E5 特殊房照常 | E7 楼梯房照常
- 击退/传送推出 LOCKED: CLOSED 门=碰撞墙 (Batch 1 语义天然防)

## 验证

- **42/42 ctest 全绿** (新增 `room_encounter_test` 6 用例: 闭环/无怪不触发/压门不锁/Boss 跳过/多门原子/映射集成)
- 构建 0 警告; 确定性保持 (同 seed 逐字节一致)
- Sim 回归: seed100 100 局 F5=39% (Room Encounter 引入真实关门, Sim 经 S1 正常通过, 无卡死)
- 清房掉落钩子 Batch 3 接; 封门演出仅一次性 room_msg

---

# v1.2.1 — Batch 2B: Door Interaction (R1 接触开门 + S1 Sim 语义) (2026-08-28)


> Batch 1 (v1.2.0) 完成 DoorState 数据模型后, Batch 2B 接入交互层。门保持默认 OPEN (D2 决策), 不改变 gameplay。

## R1 — 接触开门

- `GameMap::try_open_door_toward(rect, mx, my)`: 玩家移动中心 tile 指向 CLOSED 门时自动开启 (无按键)
- 接入 `PlayerController` 移动碰撞: 被 CLOSED 门阻挡时先开门再移动 (水平/垂直两轴)
- `GameScene::on_door_opened()`: 开门后立即重算 FOV (门后区域揭示)

## S1 — Sim 语义

- `_tile_rect_walkable`: CLOSED 门视为可通行 (Sim 与玩家共用 R1 规则, 零 Sim 专用逻辑)

## 验证

- **41/41 ctest 全绿** (新增 `door_interact_test` 5 用例: 四方向接触开门/非门不触发/CLOSED 挡人挡视线/生成图默认 OPEN)
- 构建 0 警告; 确定性保持 (同 seed 逐字节一致)
- Sim 冒烟: seed100 50 局与 Batch 1 基线一致 → **未改变 gameplay** (门默认 OPEN)
- 门 CLOSED 语义已由 Batch 1 `door_seal_test` 覆盖; 真实关门逻辑 (Room Encounter) 在 Batch 2C

---

# v1.2.0 — 地牢密封 (Batch 1): 门是房间唯一孔径 + DoorState (2026-08-28)

> A1 孔径修复把地牢从"开放地板团块"修成真正的 Room→Door→Corridor 拓扑。
> 详细: `docs/BATCH1_DUNGEON_SEAL_ACCEPTANCE.md` / `docs/BATCH1_DUNGEON_SEAL_IMPL_PLAN.md`

## A1 — Door Aperture Integrity（孔径完整性）

- **问题**（审计发现）：`_carve_diamond` 雕走廊时在房间环墙留下平均 6.25 个/房的非门缺口（"隐形门"），关门无法密封、FOV 隔门泄露 93.2%
- **修复**：`_repair_room_apertures`（确定性后处理，零 RNG）— 环墙缺口回墙(94%)/door 化(6%)
- **结果**（27 seeds）：密封率 0%→100%，非门缺口 6.25→0，房间内部泄漏 0，无死房，全图连通，门数 18.7→22.7
- **INVARIANT(seal)**：`door_seal_test` T1-T6 永久回归（27 seeds = 7 基准 + 20 fuzz）

## DoorState 数据模型

- `Tile.door_state`（OPEN/CLOSED，LOCKED/SEALED 预留）+ `GameMap` 门态 API
- 语义：OPEN = walkable + 透视线（现状保持）；CLOSED = 不可走 + 挡视线（Batch 2 启用）
- 生成后门默认 **OPEN**（D2 决策：独立验证孔径修复与 FOV 效果）
- FOV 半径可配置：`FOV_RADIUS_DEFAULT=8` + `FloorConfig.fov_radius`（0=默认）

## 验证

- **40/40 ctest 全绿**（含 door_seal_test 6 子断言 + fov_test 3 新用例）
- 构建 0 警告；确定性保持（同 seed 逐字节一致，A1 零 RNG 消耗）
- Sim 回归：baseline 10.4% → 2.6%（A1 后新基线）。归因审计排除 path/chokepoint/walkable/怪物出生/Boss 结构后判定为**确定性 Sim 的决策链分叉**，非拓扑 bug。**用户裁决接受新基线**，真人 F5 体验并行验证中

---

# v1.1.0 — 可见性与空间体验 (Phase 1-3): FOV + 地牢拓扑 + Minimap (2026-08-28)


> v1.0.0 之后的地牢空间感三连深耕：从"做完"到"做得像"。全部保证 FOV/Save/AI/战斗系统零改动（除 Phase 1 自身）。

## Phase 1 — FOV 可见性系统

- **Tile 三层状态**：`is_visible`（当前帧 FOV 内）/ `is_explored`（曾探索）/ `is_walkable`（碰撞，独立于可见性）三字段解耦
- **360° 射线投射**：`update_fov(cx,cy,radius)` 逐度投射，撞墙/越界即断；`reset_visibility()` 进层清空
- **三层渲染**：未探索=全黑不渲染；当前可见=全亮；已探索但不可见=60% 暗（记忆态）
- **实体剔除**：怪物/NPC/地面物品按中心 tile 的 `is_visible` 剔除，未探索区看不到生物
- 8 个 FOV 单测（`tests/world/fov_test.cpp`）
- 阻断问题：实体可见性判定错误、贯穿墙视线

## Phase 2 — 地牢拓扑 (Room → Door → Corridor)

- **`TileType::DOOR`**：`walkable=true`、`blocks_sight=false`（静态开启门，预留 CLOSED/LOCKED/SEALED 扩展）
- **边缘连接算法**：`_pick_room_edge`（房间边缘中点）+ `_compute_door_pos`（向外 1 格放门）+ `CorridorConnection` 结构
- **`_carve_diamond` 墙壁保护**：`if (g[ty][tx]=='#')` 只雕刻墙壁，走廊绝不侵入房间 Interior
- **门边界安全**：`_pick_room_edge` 过滤 Door 越界的边缘（地图侧)
- 12 拓扑测试 + 5 结构回归（`dungeon_topology_test` / `dungeon_verify_test`，永久保留）
- 验收报告：`docs/PHASE2_ACCEPTANCE_REPORT.md`（房间环墙覆盖率 71~78%，墙体密度 41~52%，无巨型开放区）

## Phase 3 — Minimap 小地图

- **`MinimapRenderer`**（`src/game/ui/`）：只读 `isExplored/isVisible`，**无第二套探索状态**；职责=坐标换算/绘制/标记/面板背景
- **不泄露原则**（纯函数可测）：`should_show_boss`（最后已知位置，仅已探索）/ `should_show_stairs`（发现后永久）/ `should_show_entity`（仅当前可见，离开视野即消失）
- **常量集中**：`MINIMAP_TILE_SIZE/WIDTH/HEIGHT`（非魔法数字，便于扩展）
- 右下角常驻面板，**M 键**开关（默认显示）；Boss 不实时追踪不可见区移动
- 12 单测（`tests/ui/minimap_test.cpp`）

## 验证

- **39/39 ctest 全绿**（含 minimap 12 + dungeon_verify 5 + dungeon_topology 12）
- world_validator 0 error 0 warning
- 多 seed 实机 smoke test 无崩溃；桌面打包版已同步
- 完整审计/设计/验收：`docs/PHASE1_FOV_PLAN.md` / `PHASE3_MINIMAP_PLAN.md` / `PHASE2_ACCEPTANCE_REPORT.md`

---

# v0.9.35 — 实测反馈修复: 背包键位冲突 + 怪物房间守卫 + 通关专属 BGM (2026-08-25)

## 玩家实测三连 (来源: 试玩反馈)

- **背包 D 键二义性**: 打开背包后按 D 丢弃被翻页抢占 (D 同时绑定 move_right, 翻页判断在丢弃之前) — 重构背包分支: X/U/D 动作键优先判定, 光标移动改用 WS/↑↓, 翻页改用 ←→ 方向键, 彻底解耦动作与导航
- **怪物房间守卫 (leash)**: 原 IDLE 随机巡逻使怪走出房间 → 进入视野全图追击 → 前期怪涌向主角、中后期无怪可打。新增出生锚点 + 双重束缚: 巡逻半径 4.5 格 (超出折返) / 追击上限 8 格 (超出放弃回家); 掉血即视为挑衅解除束缚 (含毒/环境伤); Boss 不受束缚
- **通关专属欢快 BGM**: 新增 victory 曲目 (C 大调 I-V-vi-IV 进行 @132bpm, square 主音上行琶音) — 原通关动画沿用紧张 Boss 曲直到回标题; 经 VictoryScene::get_bgm_name() 声明走 change_scene 场景级管线自动切换, 回标题后由 TitleScene 的 title 曲接管

## 验证

编译 0 警告; 34/34 ctest; **300 局 sim 回归 9.0%** (区间 6-10%, leash 后 sim 由玩家 BFS 主动寻怪, 胜率稳定)

---

# v0.9.34 — AI 系统代码审查修复: 11 处算法正确性 bug (2026-08-25)

> 源起: 全仓 AI 子系统源码级深度审查 (发现与修复记录整理于 docs/AI_LEARNING_GUIDE.md), 修复其中经回归验证的 11 处

## BTAgent (行为树, `--sim-ai bt`)
- **P0-1 接线修复**: confirm/descend 动作已创建但从未挂入树 (根节点 push 的是裸 Condition, Selector 命中即短路) — 改为 Sequence{cond, act}, BT agent 首次具备下楼/确认能力
- **P0-2 时间语义**: 技能冷却判断 can_use(0) → can_use(_game_time); 新增 BTAgent::set_time 注入链 (game_scene 每帧同步, 原 BT 模式拿不到时间 → 用过一次技能后永久假阴性)

## Q-Learning (`src/ai/rl/`)
- **B1 终局自举污染**: update() 增加 done 参数 — done 时 target=reward 不自举 max Q(s') (原把"键不存在"当终止, 真终局反而自举); rl_runner 两处调用传 env.is_done()
- **B2 学习率衰减**: α/(1+0.05·visits(s,a)) — 常数学习率违反 Σα²<∞ 收敛条件, Q 值永远震荡
- **B3 击杀奖励增量式**: 原"+50/尸体/步"每步重复发放 (prev_alive 死变量佐证原意), 改为 prev vs now 差值一次性 +50

## MirrorAgent Thompson 采样 (`src/ai/mirror/`)
- **MP1 先验爆炸**: init_prior 的 +2 伪计数随存档每局固化叠加 + import 纯加法无遗忘 → 后验无界增长, 探索概率随局数衰减至零; 双重修复: ① export 扣除本局 pending 先验 (画像只服务当局冷启动) ② update 引入全臂折扣遗忘 λ=0.995 + floor 0.25 (非平稳环境恢复探索)

## MCTS (`src/ai/mcts/`, `--sim-ai mcts`)
- **A1 奖励归一化**: sigmoid(score/250) 映射 [0,1] — C=√2 的理论前提是单位化奖励, ±1000 量纲下探索项上界 ~2.5 永远翻不动利用项 → UCT 退化为纯贪心
- **A2 WAIT 偏差**: expand-all 后恒取 children.back()(WAIT) 使新节点首轮统计系统性偏向等待 → 按迭代序轮换 (保持确定性)
- **A3 回传折扣**: 删除 0.95 衰减 — 不同深度均值不可比而 UCT 在同一父下比较兄弟
- **A6 冷却伪造**: 快照 attack_cooldown 恒 0.5/skill 恒就绪 → 根节点永久禁用普攻; 改读真实 remaining_cooldown (build_sim_state 增加 game_time 参数)

## DecisionAgent (默认 sim AI)
- **P1-2 治疗优先级倒置**: 自愈槽遍历无 break, 最低优先级槽反向覆盖 → _skill_priority 首个可用即 break
- **P0-3 死区 (实测回退)**: 确认 [48px, ideal] 区间 attack/move 双零且"拉开距离"分支为不可达死代码; 尝试激活后 200 局胜率 10%→3.5% (风筝震荡破坏 Q3.12 平衡), **回退保留站桩行为**并在注释中记录缺陷与数据

## 验证
- 编译 0 警告; 34/34 ctest; world validator 0 error
- **500 局平衡回归 9.0% (45/500)** — 区间 6-10% 内 (基线 v0.9.33 为 10.0%, 波动范围内)

---

# v0.9.33 — 收官体检修复: 死配置清理 + 木桶闭环 + Boss 冷却恢复 (2026-08-19)

## 全面代码体检 (240+ 源文件)
- **删除 hazards.json 死链路**: 零消费者配置 (G6.3 未接入) — 删 JSON/hazard.h/cpp/加载/测试引用/world_validator 6 处校验; `_is_hazard_near` (熔岩/毒池/尖刺) 为活系统保留
- **EXPLOSIVE_BARREL 最小闭环**: 玩家攻击 (近战/武器) 或敌方投射物命中 → 点燃 (0.6s 引信红闪警告) → AOE 爆炸 (2 格, 3×arena_scale, 玩家+怪物) → 爆炸 VFX + 震屏 → 销毁; sim_ai 危险感知避开木桶; 复用现有 VFX/伤害系统, 无新 Manager
- **Boss 技能冷却恢复判定**: can_use 读端接入 (原写-only 死数据) — 连招命令 + 普攻循环技能释放前判冷却, 冷却中该步退普攻 (不空转)
- **其他**: 修复 bgm_engine 音符解析 narrowing 警告 (显式 char 转换)
- 验证: 34/34 测试, validator 0 error, 编译 0 警告; **200 局 sim 实测 38 次 点燃→爆炸 完全成对** (伤害随楼层缩放); 500 局平衡回归 **10.0%** (区间 6-10% 上沿, Boss 技能冷却后略升); F1-F15 全楼层 sim 跑通无回归
- 清理 2.8GB game.log (验证日志已重建为干净小文件)

---

# v0.9.32 — v1.0.0 Release Standard 验收 (五项 Stable 全部达标) (2026-08-19)

## 五项 Stable 冻结验收
- **Save Stable**: 新增 `SaveStable.*` 3 验收测试 (v1 旧档兼容/坏条目容错/全字段 roundtrip); **修复真实 bug** — elem 字段写元素名 ("fire") 而读端 atoi=0, 元素类型读档永久丢失, 改写 int (M4b-fix)
- **API Stable**: 对外契约冻结 2+ 版本 (存档 v3 格式/Registry MergeMode/Mod 管线/Replay hash 链)
- **Mod Stable**: mods scan + ModProvider + MergePatch + DependencyResolver 全链路 + registry 引用完整性测试
- **Regression Stable**: Q3.14 确定性对拍 (逐字节一致) + 500 局平衡回归 8.0% (区间 6-10%) + 37 gtest 全绿
- **Performance Stable**: sim 500 局并行 53s / 单核 ~9.4 局/s / 全量测试 0.46s
- 验收报告: `docs/V1_0_0_ACCEPTANCE.md`

---

# v0.9.31 — M4b: 地狱火魔领域作战 (弹幕演出 + 机制阶段 + Boss 房地形) (2026-08-19)

## M4b.1 弹幕图案化 (茶杯头式)
- `BarrageSkill` 图案化: `pattern` 0=扇形 1=环形 2=螺旋多波; `waves/wave_interval` 波次发射; `spiral_turn_deg` 每波偏转
- 弹丸飞行从硬编码 0.016f 步进改为帧间时间差 (修复帧率相关弹速)
- fire_demon 接入连招路径: probe/press/rage 三模板 (含 5 波螺旋弹幕), 数据驱动 (`BossSkillDef` 扩展)
- `BossEncounterController::phase()` 接线 `_select_combo`: OPENING/PRESSURE→probe, CONTROL→press, LAST_STAND→rage

## M4b.2 机制阶段激活 (MECHANIC_PHASE)
- 核心破坏 → 弹幕演出段 (Boss 无敌, 每 1s 强制快速弹幕风暴, 演出 4s) → 易伤窗口 (奖励节奏)
- 核心超时 → 直接易伤 (不变); 狂暴期演出减半; `domain_cycle_count` 双计数修复
- `domain_config.mechanic_duration` 数据驱动; 播报文案 + 冻结演出增强

## M4b.3 Boss 房机制地形 (熔岩环带安全区)
- `TileType::LAVA`: 可走地砖 + 橙红脉动绘制 + 0.5s 灼烧 (玩家/非 Boss 怪物, Boss 免疫)
- F10 Boss 房: 清空随机 ArenaObject + 中央安全区 + 外圈熔岩带 (欧式圆环, 自适应房间尺寸)
- `BossArenaDef.terrain` 数据驱动 (enabled/safe_radius/lava_band/clear_objects); `DungeonGenerator::get_boss_room_rect()`
- SimAI 危险视野感知熔岩 (3x3 邻格), BFS 可穿越

## 验证
- 500 局评估: 7.0% (s7 9% / s500 5% / s1000 6% / s2000 11% / s9999 4%) — 在 6-10% 目标区间, 较 RL 基线 6.6% 微升 (Boss 强化)
- 34/34 单元测试 + World Validator 通过

---

# v0.9.30 — RL 决策层接入镜像 Boss (F15 实战) (2026-08-18)

## RL 训练产物 → 运行时决策 (闭环打通)
- `QAgent::exploit_action(obs)`: 纯 exploit 决策 (无 SimulationState), 未见过的状态返回 -1 (不接管)
- MirrorAgent 仲裁链插入 RL 层: ML → 战术链 → **RL** → 克隆 → Thompson
- 镜像语义: Q 表学的是玩家视角最优策略 → 映射为 Boss 反制臂 (ATTACK→COMBO, SKILL→SKILL, MOVE→按距离 APPROACH/RETREAT)
- `MirrorBattleState → Observation` 适配 (字段与 rl_runner 训练场景对齐), 按玩家风格加载 `saves/rl_mirror_q_<STYLE>.json`
- 文件缺失 → 不注入 (降级现有仲裁链, 安全); 观察期 (phase<2) 不启用
- 验收统计: MirrorDebugSnap 新增 `rl_used` 计数, HUD 摘要仲裁[Clone/ML/RL/Tho]

## 验证
- 实测: 战斗仲裁 `[Clone:0 ML:0 RL:11/25/26 Tho:0]` — RL 完全接管仲裁, Thompson 不再触发
- 500 局评估: 胜率 8.6% → 6.6% (s7 6% / s500 6% / s1000 4% / s2000 7% / s9999 10%) — RL 镜像 Boss 变强, 仍在目标区间 6-10% 内
- 34/34 单元测试通过

---

# v0.9.29 — RL 训练收敛: epsilon 退火, 胜率突破 95% (2026-08-18)

## epsilon 退火
- `QAgent::set_epsilon()`, 训练循环按进度线性退火: 0.12 → 0.005 (常量 `EPS_START/EPS_END`)
- 原理: 固定探索率 0.12 是天花板 (~91% 封顶), 后期降探索后利用率提升, 胜率突破 95%
- 新增"末段 10% 低探索统计" (tail): 训练末尾 500 局 (epsilon≈0.005) 胜率即真实收敛水平

## 训练结果 (续训 5000 局/风格, 累计 ~20000+ 局)
| 目标 | 200局基线 | 退火前 | 退火后 tail (低探索) |
|------|-----------|--------|----------------------|
| RL TRAIN | 100% | 100% | **100%** |
| AGGRESSIVE | 74.5% | 91.5% | **96.8%** |
| DEFENSIVE | 80.0% | 91.2% | **99.0%** |
| SNIPER | 72.5% | 91.9% | **96.4%** |
| BALANCED | 64.5% | 90.7% | **99.2%** |
- 全部 ≥95% 达标; Q 表已饱和 (2380-2374 条目, 状态空间覆盖完毕)
- 34/34 单元测试通过

---

# v0.9.28 — RL 训练管线: 入口合并 + Q 表持久化续训 (2026-08-18)

## Q 表持久化
- `QAgent::save(path)` / `QAgent::load(path)`: JSON 格式 (`{"q": {obs|action: value}}`), 目录自动创建, 损坏/缺文件安全返回 false
- `--rl-train N`: 训练前自动加载 `saves/rl_qtable.json` (存在则继续训练), 训练后保存
- `--rl-mirror N`: 4 风格各独立 Q 表 `saves/rl_mirror_q_<STYLE>.json`, 同样支持续训
- 训练产物不纳入版本库 (gitignore 新增)

## 命令行入口合并
- 原 `--rl-train` 分支提前 `return 0` → `--rl-mirror` 永远不可达 (死路径)
- 改为顺序执行: `run_rl_mode` → `run_rl_mirror_mode` → 统一退出, 两参数可同跑

## 验证
- `--rl-train 100 --rl-mirror 50` 同跑正常, 第二次运行 `[load] ... entries — 继续训练` 生效
- 实测续训: 镜像 4 风格 200+50 局 (AGGRESSIVE 2078→2369 条目), 单风格胜率 48-86%
- 34/34 单元测试通过

---

# v0.9.27 — Sim 确定性修复: 指针键/跨层残留三连 (2026-08-18)

## 背景: 同种子双进程评估结果逐字节不一致 (可复现性回归)
- 症状: `--sim N --sim-seed S` 两次运行日志在运行中间帧分叉, 报告随机不同 (胜率 5%~15% 抖动)
- 排查: 对拍 (RNGDBG 打点 + rng.draws 轨迹) 缩小到 F5 f=4 帧内击杀分叉 — 状态全同却一只史莱姆死亡
- 根因定位: 三处裸指针跨进程不确定 (堆地址不同) + 跨层/跨局残留 (地址复用 → 污染新对象)

## 根因 #1: 怪物脱卡状态指针键
- `_unstuck_last_pos/_unstuck_since` 以 `const Monster*` 为键 — 换层后旧怪释放, 新怪 malloc 地址复用 → 残留键把新怪当成"卡住已久"秒传送
- 修复: 键改 `uint64_t instance_id` (monster.cpp 静态递增计数器), enter_floor 时清空两 map

## 根因 #2: SimAI 路径记忆指针键
- `_mem_target` 以 `const void*` 记录上一目标 — 同内存地址的新怪沿用旧路径记忆 → 决策分叉
- 修复: 改 `uint64_t` + 空指针判 `mem_t ? mem_t->instance_id : 0`

## 根因 #3: 双节棍连击自动追踪裸指针 (主凶)
- `WeaponSpecialState::tracked` 存 `Monster*`: 激活于 F1 (第3段连击), 跨 ~3700 帧残留到 F5 仍 active
- 换层后地址复用: 一个进程的 tracked 恰好指向史莱姆 (打死, hp 35→0), 另一进程指向别的怪 → 帧内击杀分叉 (该帧 rng 消耗 13 vs 6)
- 修复: 改 `uint64_t tracked_instance`, tick_specials 用 `std::find_if` 按 instance_id 查找 + is_alive 校验, re-acquire 时同步更新

## 验证
- 三种子 (500/1000/2000) × 20 局 × 2 批: 全部逐字节一致 (130万行级对拍)
- 评估基准 (修复后确定性): seed2000 5% / seed1000 15% / seed500 10%
- 大样本验证: 5 种子 × 100 局 = 500 局, 胜率 8.6% (43/500, seed7 10% / s500 9% / s1000 8% / s2000 9% / s9999 7%), 对比 Q3.12 基线 5.8% — 确定性修复后进入目标区间 6-10%; Boss 击杀 F5=63% F10=33% F15=11%
- 34/34 单元测试通过

---

# v0.8.0 — Architecture Freeze

## Release Metadata

| Field | Value |
|-------|-------|
| Version | v0.8.0 |
| Codename | Architecture Freeze |
| Date | 2026-07-17 |
| Phase | G1-G3 Complete |
| Status | Stable Baseline for G4 |

---

# v0.9.0 — C++/Python Dual Sync (G5-G6)

| Field | Value |
|-------|-------|
| Version | v0.9.0 |
| Codename | Dual Sync |
| Date | 2026-07-21 |
| Phase | G5-G6 Complete |
| Status | Current Release |

# v0.9.1 — Boss Combat Hardening + Online Adaptive Mirror AI (2026-08-04)

# v0.9.26 — Q4 品质打磨批2: 反馈补全 (2026-08-11)

## Q4.7 玩家受击红屏
- `trigger_hit_flash()` + `hit_flash_timer`: 全屏主题 hit_flash_tint 叠加, alpha 随计时衰减
- 受击两处 (弹幕路径/近战路径) 同步触发 — 视觉反馈闭环

## Q4.6 VFX recipe 消费 sfx/camera_shake 字段
- `play_recipe` 现消费 recipe 的 `sfx`/`camera_shake` — 此前 28 处配置全部死数据
- 补 3 个缺失合成音: `ice_crack`/`lightning`/`summon`
- 经 ServiceLocator 间接访问 (VFXServer 值对象不持引用, 模块边界不变)

## Q4.5 UI 音效 + 标题菜单高亮
- 新增合成音: `ui_click` (短促)/`ui_confirm` (双音上行)
- 标题菜单: 鼠标悬停高亮 (禁灰项不可悬停) + hover 切换音效 + 左键点击激活
- `TitleScene::_activate()`: 键盘/鼠标共用动作分发 (单一职责)
- 游戏内面板开关 (背包/圣物/任务日志) 播放 ui_click

# v0.9.25 — Q4 品质打磨批1: 打击感与音频补全 (2026-08-11)

## Q4.1 HitStop 修复 (隐藏全局短板)
- `freeze_timer` 原只递减不消费 — 所有 trigger_freeze 调用形同虚设
- `PresentationSystemDirector::is_frozen()` + GameScene 主循环接入:
  冻结期跳过世界模拟 (怪物/弹幕/Buff/玩家), 仅表现层计时器推进
- 打击感三件套 (HitStop/震屏/飘字) 至此全部真正生效

## Q4.2 BGM 循环 + stop 修复
- `BGMEngine::stop()` 原停的是 `_cache.begin()` (第一首) 而非当前曲 — 已修
- 新增 `BGMEngine::update()`: 曲目播放结束后自动重播 (Sound 无自带 loop)
- `AudioServer::update()` 接入 SceneTree 主循环 (process_frame 每帧驱动)
- 地牢/Boss BGM 不再每 30 秒静音

## Q4.3 拾取反馈 (音效+特效)
- 拾取物品: `play_sfx("pickup")` + ring+spark 闪光 (圣物金色/普通暖色)
- 拾取不再无声无息 (此前仅教程场景有拾取音)

## Q4.4 受击/攻击音效补全
- 新增合成音: `hurt` (玩家受击闷响) + `monster_atk` (怪物攻击嘶吼)
- 玩家受击 2 处 (弹幕/近战) 播放 hurt
- 怪物攻击 (近战/远程) 经 `MONSTER_ATTACK` 事件解耦 — AI 层不持音频引用
- `SceneTree` 注册进 ServiceLocator (事件回调访问音频)
- 新增 `GameEventType::MONSTER_ATTACK`

# v0.9.24 — M4.5 战术链跨场景预测 + M4.4 E2E 验证 (2026-08-07)

## M4.5 跨场景预测 (战术链不再只驱动应付臂)
- `predict_next_action`: 战术链层优先于克隆层 — 预测玩家下一步动作类型
  (SKILL_*→SKILL, COMBO_*→ATTACK); 链 miss 才回落克隆/规则
- `should_interrupt_skill`: 链预测玩家将放技能 (高置信) → 提前进入打断准备
- 新增 `chain_symbol_to_action`/`chain_predict_action` (静态, 单一职责)

## M4.4 E2E 真机路径验证
- `sequence_e2e_test`: 走真实采集链 (PlayerBehaviorRecorder API) → 画像 → 克隆 +
  战术链注入 → 在线观察 → 仲裁/预测/打断, 3 用例 (含技能连发套路)
- 全量 **34/34 绿**

# v0.9.23 — M4.4 战术链序列记忆: 镜像学习玩家战术套路 (2026-08-07)

## M4.4 Tactical Sequence Memory
- **采集层扩展**: `PlayerAction` 新增 `weapon_type` (武器类型) + `combo_stage` (连招段),
  `on_weapon_attack` 传连招段与武器类型 (weapon_executor 调用处已接)
- **TacticalChainTable** (新): 12 战术符号 n-gram (技能×4/位移×4/连招段×3) —
  3-gram 计数表 + 2-gram 降级表, 离线 build (与克隆表同步, F15 enter 注入)
- **降级链**: 3-gram → 2-gram → 克隆表 → 规则; 仲裁链: **ML槽 → 战术链 → 克隆 → Thompson**
- **在线仲裁**: `observe_actual` 维护最近 2 战术符号缓冲 (类型级近似符号), 高置信预测
  玩家下一步战术动作 → 意图 → 应对臂; 缺 skill_id/combo 细节时自然降级不产生错误动作
- **M4.1 验证回归**: 决策抽为 `decide_tactic` 纯函数 + 三场景回归单测 (SNIPER/时停/低血)
- 新增 8 个 tactical_chain 单测 (符号映射/3-gram 计数/2-gram 降级/空流/仲裁), 全量 **33/33 绿**

# v0.9.22 — M5 条件维度: 镜像读懂受压反击/朝向/节奏 (2026-08-07)

## M5 条件维度 (采集 → 统计 → 执行闭环)
- **采集层**: `PlayerAction` 新增 `facing_dir` (朝向) + `hit_in_1s` (近1s受击窗口);
  recorder 新增 `set_battle_context()`, PlayerController 以 HP diff 追踪 1s 受击窗口
- **统计层**: analyzer 新增 3 个真实习惯维度 —
  - `fight_back_rate` 受压反击率 (被打后 1s 内反击占比, 0.6+ 硬刚 / 0.3- 怂包)
  - `face_enemy_rate` 朝向稳定度 (主朝向占比, 高=单向癖可预测退避轴)
  - `attack_rhythm_var` 攻击节奏方差 (相邻攻击间隔 stddev, 小=固定连段可挡)
  - 修正 `player_action.h` 朝向注释 (Direction: 0=下 1=上 2=左 3=右)
- **执行绑定**: M4.1 战术层消费新维度 — 反击型→KITE 拉扯耗链路; 怂+单向癖→
  远程多角度封锁退路; 四面转→贴身缠斗; HUD 画像摘要新增 Counter/Face/Rhythm
- 新增 5 个 analyzer 单测 (反击率/无受击/朝向稳定/节奏方差/少样本安全), 全量 **31/31 绿**
- 克隆表**不加**条件维度: 80 桶已稀疏, 加维度会稀释 (M5 维度走执行层, 不走意图预测)

# v0.9.21 — M4.1/M4.2/M4.3 镜像战术层 (2026-08-06)

## M4.1 战术脚本层
- 新增 `MirrorTactic` 枚举 (OPEN_RANGED/ENGAGE_MELEE/KITE/ADAPTIVE), 画像+态势驱动
  - 玩家 HP<30% → 压进近战终结; SNIPER 或平均距离>260px → 远程消耗 (近身 KITE)
  - aggression>0.55 或 predict_low_dodge → 压进近战; 3s 切换冷却防抖
- 技能选择由循环轮转改为战术映射: 远程→弹幕/AOE, 压进→近战/时停, 拉扯→弹幕/治疗
- `_ai_decide` 首选距离按战术 (260/96/220/200px), 压力探测覆盖 → 80px + 1.3× 攻势加成
- `MirrorAgent` 新增 `profile()` 访问器

## M4.2 镜像专属真冻结
- time_stop: Phase≥2 时冻结玩家 3s (禁移动/攻击/技能), Phase<2 观察期仅减速
- PlayerController 加镜像冻结门控, 冻结期间怪物 AI 与玩家受击保持
- 红霜 overlay 提示, 与玩家时停 (蓝/白) 区分

## M4.3 武器槽切换
- 镜像武器双槽: 近战=玩家武器, 远程=CROSSBOW (倍率×0.8, 射速略慢)
- 战术驱动切换: 远程消耗/拉扯→远程槽, 压进/平衡→近战槽, 2.5s 独立防抖
- 切换重置连招段, 视觉/日志即时反馈

- 全量 **30/30 绿**

# v0.9.20 — 热修复: 玩家时停期间世界未冻结 (镜像/尖刺/弹体/DOT 穿透) (2026-08-06)

## 热修复
- **Bug 复现**: 玩家放 The World 时停后, 镜像 Boss/尖刺/敌方弹体/敌方 DOT 仍在结算 —
  玩家在"时停期间"被镜像伤害击杀 (日志: 镜像 AOE/时停减速照常命中)
- **根因**: 时停门控只覆盖普通怪物 AI (`player_controller.cpp` L94 `_update_monsters`),
  Boss/镜像 (`_boss.tick`)、arena 尖刺毒池、敌方弹体、敌方 buff 四条伤害链全部绕过
- **修复** (game_scene.cpp, 4 处门控 `time_stop_remaining <= 0`):
  - `_boss.tick` 调用 (BossAI/镜像/领域/arena 区域伤害)
  - arena 物体循环 (尖刺/毒池/图腾)
  - 敌方弹体 (MONSTER/ENVIRONMENT owner)
  - 敌方 buff tick (毒 DOT/venom_fang; 玩家自身 buff 不受影响)
- 全量 **30/30 绿** · 桌面已同步

# v0.9.19 — 热修复: 死亡后继续游戏闪退 (EventBus 悬挂订阅) (2026-08-06)

## 热修复
- **闪退根因**: `EventBus::subscribe` 的 `Sub.owner` 从未填充 (写死 `nullptr`),
  且 GameScene 析构不注销订阅 — 玩家死亡 → GameScene (`_gameplay`/`_boss`/
  `_presentation`) 析构后, EventBus 仍保留捕获 `[this]` 的 lambda
- 继续游戏 → 新 GameScene `enter_floor` → `emit(FLOOR_ENTER)` → 调用已析构对象的回调 →
  未定义行为 → 闪退 (首次进 11 层正常, 死亡后再继续必崩 — 与日志完全吻合)
- **修复** (5 文件):
  - `event_bus.h/.cpp`: `subscribe` 增加 `owner` 参数, 正确填充 `Sub.owner`
  - 三个 Director 各加 `unregister_events()` (gameplay: RELIC_GAIN/FLOOR_ENTER;
    boss: BOSS_DEAD/FLOOR_ENTER; presentation: 6 类事件), 订阅时传 `this`
  - `GameScene::~GameScene` 析构时统一注销, 消除悬挂回调
- 全量 **30/30 绿** · 桌面已同步

# v0.9.18 — 热修复: 选关进入普通层闪退 (F9 overlay 空指针) (2026-08-06)

## 热修复
- **闪退根因**: v0.9.17 修改 F9 MIRROR AI overlay 时误删外层守卫,
  `game_scene.cpp` L1545 无条件解引用 `_boss._mirror_agent` — 普通层 (选关11层)
  不创建镜像 agent, `unique_ptr` 为空 → 0xC0000005 (SEH) → 闪退; 15 层 Boss 层 agent
  非空, 故读档从未触发
- 修复: 恢复守卫 `if (g_show_mirror_acc && _boss._mirror_agent && g_font_loaded)`
- 调试工具增强: `seh_handler` 崩溃日志增加 RVA+模块基址 (配合 Debug 构建 addr2line 定位)
- 全量 **30/30 绿** · 桌面已同步 (Release exe 3.3MB)

# v0.9.17 — M4 调参基础设施: MirrorTuning 参数表 + 漂移降权消费 (2026-08-06)

## M4 (第一批: 参数化 + 断链修复)
- 新增 `MirrorTuning` (`src/ai/mirror/mirror_tuning.h`): 全部 Phase 触发阈值/仲裁置信度/漂移降权集中管理 (单例可调), 为实测标定留入口
- **修复第二个"算了没用"断链**: `profile_drift()` 此前零调用方 — 现在被消费:
  - `clone_confidence_threshold()`: 漂移>0.5 → 克隆置信门槛 0.50→0.75 (玩家换打法 → 模仿降权, 交 Thompson 在线适应)
  - predict_next_action / recommend_action 克隆分支改用动态门槛
- **Phase 时间兜底按实战标定**: P1→P2 兜底 20s→12s (实战第1局战斗约20s, 旧值在短战斗几乎必然只走兜底/打不完)
- F9 HUD 加 `Drift:% Bar:` 行 (漂移与当前门槛可视化)
- 单测: 漂移降权 2 项 + tuning 时间兜底可调 1 项, 全量 **30/30 绿** · World Validator 通过 · 桌面已同步
- 待实测第2局: 确认 `[MIRROR] CloneTable built` 非空 + `[MIRROR-ACC]` 摘要 (决定下一批数值标定)

# v0.9.16 — M4 链路线接通: 运行时注入克隆表 (验收发现致命断链) (2026-08-06)

## M4 前置修复 (实战验收第1局暴露)
- **致命断链修复**: `set_clone_table` 在游戏运行时代码**零调用** — 克隆表只在单测注入, 实战 `_clone==nullptr`, Echo 反制全来自规则/画像而非克隆层
- `_init_mirror_boss` 现从 `g_behavior.history()` 构建 `BehaviorCloneTable` (build + set_profile + set_clone_table) 并 LOG `CloneTable built: N entries`
- `[MIRROR-ACC]` 战斗摘要从 printf 改走 `LOG_INFO` → 统计进 `game.log` (不再丢在控制台)
- 30/30 全绿 · 桌面已同步 — **需再实测一局验证 `[MIRROR-ACC]` 摘要与 `CloneTable built` 日志**

# v0.9.15 — F15 M3 后验验收: MirrorDebugStats AI 链路闭环证据 (2026-08-06)

## M3-AC (后验验收, 无新 AI 功能, 只证明链路真闭环)
- 新增 `MirrorDebugStats` (`src/ai/mirror/`): Predict/克隆(精确/模糊)/画像/默认/规则 降级链计数 + 仲裁[Clone/ML/Thompson] + 打断(尝试/成功) + 行为分布(A/S/R/Approach) + 各 Phase 时长
- MirrorAgent 全面打点: predict_next_action / recommend_action / tick_phase 每分支计数 (const 安全, 非侵入)
- Director 打点: 打断尝试 + 行为状态每决策帧采样
- **技能映射核对 (验收点4)**: director case 0-4 全真实效果 (heal=`boss.combat.heal(max/5)`、时停=`slow×4`、近战/弹幕/AOE 真实伤害) — 无"名字镜像"; **修复**: 自愈/时停此前缺 `report_outcome` 在线反馈 → 已补正反馈
- **F9 HUD**: 战斗中 toggle MIRROR AI 统计 overlay (Predict/CloneHit/Rule/打断/行为分布/Phase时长)
- 战斗结束日志: boss_system_director 导出 `[MIRROR-ACC] battle ended — <summary>` (每场只记一次, `begin_battle()` 重置)
- 单测 10 项 (统计逻辑 7 + MirrorAgent 真实路径集成 3), 全量 **30/30 绿**
- 验收手册写入设计文档 §7: 前 14 层埋"低血回血"习惯 → F15 按 F9 验收克隆驱动/调用链/Phase 行为/技能真实效果
- 已知缺陷记录: `--sim` 需标题画面手按 N (G5.6 无自动开始), 无人值守验证不可用 → 验收需人工实操; 若 Predict=0 则停止 M4 · 桌面版已同步

## Bugfix: sim/正常退出不再崩
- **根因 (gdb 栈回溯定位)**: main.cpp 显式 `ResourceManager::inst().unload_all()` 后, 静态单例析构再调一次 `unload_all()` → 二次 `UnloadFont` → 字体 double-free → 堆损坏 (Release 0xC0000409 / Debug 0xC0000374), 崩在程序退出阶段
- 修复: `unload_all()` 加 `_loaded` 防重入保护 (一次性卸载), 二次调用直接返回
- 验证: `--sim 1` 退出码 0 (修复前稳定崩溃), Debug+gdb backtrace 确认崩溃帧 = 单例析构卸载字体; 29/29 全绿
- 顺带: `.gitignore` 补 `build-dbg/` · 桌面版已同步

## M3: 克隆层接入行为选择仲裁 (G5)
- `recommend_action` 仲裁链: **ML 插槽 (G5, 注册即启用, 默认关闭)** → **克隆层 (Phase≥2, 置信度>0.5 驱动行为臂)** → **Thompson 采样** → 规则兜底 (观察期)
- 玩家意图 → Boss 应对臂映射 (镜像反制语义): HEAL/DODGE/RETREAT→压近惩罚, SKILL→技能打断, ATTACK→连招, ADVANCE→拉扯
- `_record_arm` 统一记录臂+上下文桶, 保持 `report_outcome` 在线反馈链完整
- `set_ml_predictor(std::function<PlayerActionType(state)>)` 插槽预留 (G5), 默认 nullptr 关闭
- 单测 6 项 (高/低置信度仲裁、ML 覆盖克隆、非决策忽略、观察期不介入), 全量 29/29 绿
- ⚠️ 已知问题: `--sim` 冒烟崩 (0xC0000374 堆损坏) 为**既有缺陷** (M2 exe 复现一致), 待独立修复, 与 M3 无关 · 桌面版已同步

## M2: Phase 1-2-3 从纯计时改为数据驱动
- 新增 `RollingAccuracy` (`src/ai/mirror/`): 32 次滑动窗口在线命中率, 只关注近期表现
- **动态 Phase 触发** 替代 `tick_phase_timer` (删除死代码与相位计时字段):
  - P1→P2: 准确率≥0.65 且观察≥20 / 观察≥40 / 战斗时间≥20s
  - P2→P3: 同桶命中≥10 且准确率≥0.7 (核心模式) / 玩家或BOSS HP<35% (濒危)
- **在线观测**: MirrorAgent 新增 `on_prediction`(附 ObservationKey 上下文) + `observe_actual`(玩家实际动作反馈), 命中/落空滚窗统计
- **画像一致性**: `profile_drift` — 当前战斗攻击/技能频率 vs 画像频率归一化偏差 [0,1]
- MirrorCombatDirector 集成: 每帧识别玩家实际动作 (攻击/技能/闪避位移/喝药HP上升) → 反馈观察器; 预测后立即上报上下文
- BossSystemDirector 每帧动态判定 (传 HP 快照)
- 新增 `player_action.h::is_decision_action()` 语义化过滤 (ATTACK/SKILL/DODGE/HEAL)
- 单测 12 项 (滚窗滑动/触发阈值/低准确率滞留/漂移计算), 全量 28/28 绿 · 桌面版已同步

## M1: Player Clone Agent 第一层学习模块
- 新增 `BehaviorCloneTable` (`src/ai/mirror/`): 从 F1-F14 PlayerAction 流构建 state→意图分布, 零神经网络
- **可解释 ObservationKey**: `"d<距离桶>:h<血量桶>:s<技能就绪桶>"` (d: 贴身/近/中/远/极远, h: 危急/低/中/高, s: 就绪技能数)
- **战斗意图枚举 PlayerIntention** (7 类): ATTACK/SKILL/DODGE/HEAL/ADVANCE/RETREAT/IDLE — 非"简单 ATTACK/SKILL"
- **4 级降级链**: 精确状态 → 模糊状态(合并技能维度) → PlayerHabitProfile 规则 → 默认策略
- PlayerAction 扩展响应上下文快照 (hp / enemy_dist / skill_ready_mask), recorder `set_context` 每帧注入 (player_controller), 旧流向后兼容 (-1 = 未知)
- MirrorAgent 集成克隆层: Phase≥2 优先查表 (置信度≥0.5), 规则层兜底; `MirrorBattleState` 加 `player_skills_ready`
- 单测 9 项 (含验收: 低血+近距离+技能Ready → 预测 HEAL), 全量 27/27 绿 · 桌面版已同步

## 稳定性修复
- **数据加载器幂等化**: enemy/boss/skill/item/buff/relic 的 `load_xxx_defs` 统一补 `|| is_xxx_defs_loaded()` 快路径, 重复加载不再触发 MergeMode::Skip 空档
- **World 加载器指针悬垂修复**: biome/encounter/hazard/landmark 从 “push_back 后取 `&back()`” 改为 "先 push 全部再建索引", 消除 vector 扩容导致的悬挂指针
- `item_defs` 流读取顺序修复 (先读全文再 parse, 避免 `f >> j` 后迭代器读到空)
- `WeaponSpecialState::should_fire_next`: 连击末击后去激活但保留 `hit_count/tracked`, 修复第 5 击伤害错用第 1 档倍率
- `AttackContext::valid()` 补 `t >= timestamp` 过滤, 未来时间戳不再判定有效
- CMake `enable_testing()` 补全 (ENABLE_TESTS 分支)

## 测试套件 26/26 全绿
- save_test 重写为自足 roundtrip (原依赖运行时生成的 `saves/` 产物)
- astar "不可达" 用例改为 3×3 墙环孤岛 (原包围圈逻辑实际可达)
- 同步过时断言: observation 8 特征/999 哨兵, element 冰冻曲线 (Lv6≈33.7), sim 浮点序列化, q_agent 空状态 ATTACK 合法性, buff DOT 末档计数, mcts 邻近怪物收敛, condition 空串语义
- World Validator 0 错误 · Release 构建 100% · --sim 20 冒烟无崩 · 桌面版已同步

## 素材覆盖补齐最后一块
- `Monster.sprite_override`: 素材 key 覆盖字段 — Boss 工厂按层指定 (F5→boss_f5 暗影骑士图, F10→boss_f10 地狱火魔图, F15→boss_self 玩家形象), 降级路径默认 F5 形象
- `_monster_sprite_key()` 改为优先 override; Boss 也走数据驱动素材 (程序化占位此前无 Boss 专属差异)
- 特殊房间中心: 祭坛/宝箱/泉水 中心图标从字符 (+, $, ~) 升级为素材精灵 (altar/chest/spring_top, 0.75× 缩放), 触发后仍显灰字; 其余房间 (商店/铁匠/图书馆/赌徒/圣地/秘室) 维持字符
- 构建 100% · 冒烟 5s 无崩 · 桌面版已同步重编译

# v0.9.5 — 数据驱动素材接入: Kenney Tiny CC0 精灵上线 (2026-08-06)

## CC0 美术素材落地 — 程序化占位正式被替换
- 素材源: **Kenney "Tiny Dungeon" (CC0 地牢砖块)** + **Clint Bellanger "Tiny Creatures" (CC0 精灵扩展, 16×16 与 Tiny 系无缝兼容)**, 原料库入 `assets/vendor/` (330 文件 + License)
- 工具链: `extract_chars` 同族 Python 辅助 — 从图集按 (col,row) 抠出 17 个精灵 (RGBA), 装饰类剥背景色变透明, 墙/地板保留实心无缝
- 选定精灵: 玩家毒/冰/火三元素形象 (t16/t17/t18)、史莱姆/哥布林/炸弹/坦克/冲锋/召唤师、Boss F5/F10、墙 t040/地板 t049/宝箱/泉水上下/祭坛
- **数据驱动管线**: `resources/sprites.json` (snake_case) → `ResourceManager::load_sprite_config()` (load_all 挂载) → `sprite_by_key(key, def)` — 三态 fallback **素材精灵 > 程序化占位 > 几何回退**
- `SpriteDef.path` 由 `const char*` 改 `std::string` (默认 "" = 程序化占位), 管线统一
- 玩家: `element.type` (FIRE/ICE/POISON) 映射三形象; 怪物: `MonsterType`/名字 → key; GameMap: 墙/地板全部 tile 走素材纹理
- 冒烟运行 5s 无崩溃 · Release 100% · World Validator 0 错误 · 桌面版已同步重编译

# v0.9.4 — 怪物差异化 + 待机帧动画 (2026-08-05)

## 像素管线补全角色辨识度
- `SpriteRenderer::gen_pixel_sprite` body 生成升级为 **2 帧 spritesheet** (32×64: 待机/呼吸), 经 `_blit_frame` (RGBA8 行拷贝, raylib 5.0 无 ImageDrawImage) 拼帧; 呼吸帧亮度 +18 — 与 `frame_rect` 管线直通, 真素材到位仅改 `frame_count`
- Player/Monster 绘制处新增待机帧轮换 (`(int)(GetTime()*4)&1`), `SpriteDef.frame_count=2`
- **怪物差异化体型** (variant 3-6): Charger=箭形三角+冲刺亮条, Tank=方甲+头盔+甲缝, Bomber=圆身+引信火花, Summoner/Shaman=尖帽法袍+水晶; 映射 `_sprite_variant_for(is_boss, MonsterType, name)` 与形状层解耦 (SpriteRenderer 不依赖 game 枚举)
- `_brighten()` 亮度工具替代原先发带的 std::clamp 内联计算
- 验证: Release 100%, 4s 冒烟运行无崩溃, 桌面版已同步重编译

# v0.9.3 — 渲染管线闭环: 角色/怪物/VFX 全接入 SpriteRenderer (2026-08-05)

## 像素管线的圆心落在实体与特效
- `SpriteRenderer::gen_pixel_sprite(body, accent, variant, eye_dir)`: 程序化角色占位 32×32 — variant 0=人形(玩家/普通怪), 1=圆形(史莱姆), 2=大体型(Boss); eye_dir 0下/1上/2左/3右 驱动瞳孔偏移; 头+发带亮条+躯干+噪点+眼
- `Player::draw_no_cam`: 连击段位色(绿→金黄)程序化精灵, 按方向四向占位 (`ply_<dir>_<rgb>` 缓存), 保留阴影/重击放大/Combo 数字, 缺纹回退原几何绘制
- `Monster::draw`: 按体型/类型选 variant 程序化精灵 (`mon_<rgb>_<variant>`), 保留 Boss 光晕/Bomber 脉冲/Tank 边框/Charger 箭头/Summoner 光环/血条等全部功能标记; Boss 继承自动升级
- `SpriteRenderer::gen_pixel_blast(c)`: 程序化 VFX 爆点 32×32 (8 向放射线+中心白核+噪点)
- `GameRenderer::draw_effects`: spark/flash 分支改走爆点纹理 (`fx_<rgb>` 缓存 + tint 淡出缩放), bolt/slash_arc/cone 等仍几何绘制, 缺纹回退原圆
- 素材位替: 管线闭环验证通过 (Release 100%, 4s 冒烟运行无崩溃); 素材到位后 `SpriteDef.path` 即插即用

# v0.9.2 — M4f 美术管线骨架 (2026-08-05)

## 像素渲染管线 (Dark Pixel Fantasy 起点)
- 新增 `src/game/rendering/sprite_renderer.h/.cpp`: `SpriteDef` (path/帧尺寸/帧数) + `SpriteRenderer` (frame_rect/draw_sprite/gen_pixel_tile) — 素材就位后管线零改动
- ResourceManager: `load_texture()` 文件纹理缓存 (失败占位) + `procedural_tile()` 程序化像素纹理缓存 + unload 扩展
- GameMap: `set_palette()` biome 调色板注入 (值拷贝, nullptr 安全) — 墙/地板改用程序化像素纹理 (基色噪点+砖缝/接缝), 缺纹退回几何矩形
- GameScene.enter_floor: biome → 地图调色板 (三 Biome 各自色偏)

## Boss 战斗六大 Bug 修复 (F10/F15)
- BUG 1 UAF: `on_core_maybe_erased()` 钩子 + DOMAIN_PHASE 空核心路径 + reset 清理
- BUG 2 镜像 VFX 禁用: BossSystemDirector 透传 `effects` 通道
- BUG 3 ENRAGED_PHASE 实装: 狂暴攻击×1.3、周期/弱点窗口减半、震屏+文案
- BUG 4 领域核心追玩家: 惰性 MonsterAI 静态桩 (attack_cooldown=999999)
- BUG 5 数据驱动: vulnerable_duration / weakness_dmg_mult 从 domain_config 读取
- BUG 6 弹幕必中: 弹幕/AOE 距离判定

## M4e — 在线自适应 Mirror AI (Thompson Sampling)
- 新增 `src/ai/mirror/online_adaptive_policy.h/.cpp`: contextual bandit (9 上下文桶 × 4 动作臂), Marsaglia-Tsang Beta 采样, 画像先验注入
- MirrorAgent: `recommend_action()` (Phase≥2 接管) + `report_outcome()` (命中/落空反馈)
- MirrorCombatDirector: 决策接管 + 命中/闪避(位移>200px)反馈回路, `_apply_online_action` 动作映射
- 冷启动知识: 玩家习惯画像 → Beta 先验; 战斗中实时纠正
- **跨对局记忆**: Beta 参数持久化到 `saves/save.json` (`mra`/`mrb`), 旧后验叠加为新先验, 镜像跨局累积适应玩家 — 对标觉悟人机"累计学习"
- **玩家技能上下文**: `Player._last_skill_time` 记录技能施放, `player_using_skill` 实装; 技能窗口 40% 探索性反制 (Thompson 决策) + 观察期即时打断 (`should_interrupt_skill(st)`) — 不扩桶保存档兼容
- **日志收敛 + 学习可视化**: 决策/反馈日志降 `LOG_DEBUG`; 镜像面板下方新增"在线学习"HUD — 实时显示上次决策臂 + 当前桶 4 臂胜率进度条 (`_draw_mirror_learning`)

## G5 (C++ Sync)
- 5 new skill behavior classes: IceNova, ChainLightning, ShadowStrike, BloodFrenzy, SummonSpirit
- AIArchetype (4 types: Sniper/Controller/Ambush/Guardian) + MonsterSkillType (12)
- Boss Phase2 (6 unique: Whirlwind/LaserBarrage/GravityPull/etc.)
- BuildType 6→12 (Ice/Fire/Poison/Time/Support/Projectile/IceMage/LightningMage/BleedBlade/ShadowStriker/Juggernaut/SummonLord)
- 10 JSON 100% C++ parity (buffs 25, relics 63, enemies 31, bosses 6, skills 20, items 36, quests 12, dialogues 34, endings 5, meta 10)

## G6 (Architecture)
- EventBus (30 event types, pub/sub)
- ReplaySystem (Record + Playback + StateHash)
- SimRunner (Automated balance testing, --sim N)

## G5.8 (Presentation Layer — 4 commits)
- **BuildTheme**: 7-field struct, 12 presets, 3-tier dmg_color_for()
- **VFX Recipes**: vfx_recipes.json — 12 recipes, 11 color presets, play_recipe()
- **Camera**: shake/dash offset/boss landing zoom
- **Audio Director**: crossfade, boss Phase2 cue, BGM ducking
- **Timeline**: delay/duration/callback sequenced events + include()
- **PresentationEvent + dispatch()**: unified pipeline, Gameplay→Presentation fully decoupled
- **Timeline Presentation**: 12 recipes with staged delays (IceNova: ring→explosion→shatter→flash, Boss Phase2: freeze→flash→roar→shockwave→zoom)

## Python Edition

桌面版同时包含 `python_edition/` 目录，含完整 Python/pygame 源码。
启动方式：`python_edition/main.py`（需 Python 3.11+ + pygame）。

---

## Original v0.8.0 below

## M4a 系列 — Boss 核心环革新 (C++ 版)

| Milestone | 内容 | 状态 |
|-----------|------|------|
| M4a | 暗影骑士连招机器: combo 驱动 (弹幕/扇形斩/瞬移/旋风/召唤) + BossSkillQueue + 技能预警 + zone 修正 | ✅ |
| M4a.1 | 战斗体验修复: 连招触发距离 48→192px / 脱战 384px / 旋风范围圈 / 狂暴演出 / 弹幕特效 | ✅ |
| M4a.2 | 数值平衡: 毒池 0.5s DOT / 弹幕撞墙消失 / 旋风 1.6× 扇形 1.25× | ✅ |
| M4a.3 | 伤害日志全链路: attack_target 统一标签 + logged_hp 记账去重 + 每帧兜底 + [COMBO] 可见性 | ✅ |
| M4b | 第二章 Boss 领域作战 (茶杯头式) | ⏳ 开发中 |

## Scope

G1 (7 steps) — Architecture Foundation
G2 (5 sub-stages) — Content Pipeline & Data Driven
G3 (5 sub-stages) — Data Framework & Architecture Freeze

## Key Metrics

- 172 source files (h/cpp/json)
- 10 JSON config files, 156 data entries
- 12 Data registry modules with unified API
- 30 EventBus event types
- 4 Directors orchestrating 20+ subsystems
- Save format v3 with backward compatibility
- 7-layer layered architecture
- 10 modules under Architecture Freeze (no-refactor)

## Architecture Documents

- [docs/ARCHITECTURE.md] — authoritative architecture reference
- [README.md] — gameplay bible + progress tracking
- [docs/WORLD_LORE.md] — world lore bible
- [docs/D1_GAMEPLAY_LOOP_DESIGN.md] — core loop design

## Development Bible (Frozen Rules)

1. Runtime/Def separation — Def immutable, Runtime mutable
2. Registry pattern — load/get/get_all/is_loaded
3. Manager statelessness — static methods only
4. EventBus decoupling — Gameplay→EventBus→Presentation
5. Save append-only — add fields, preserve semantics
6. Minimal change — add > modify > delete > rewrite

## No-Refactor List (Architecture Freeze)

Object/Node/SceneTree · InputMap · EventBus/ServiceLocator
CombatSystem damage formula · BossAI state machine
DungeonGenerator (BSP) · GameFlowDirector state machine
SaveManager core format · Player/Monster lifecycle
6 Skill execute() methods

## Next Phase: G4 — Platform & Mod Support

- Mod resource override paths
- JSON schema validation
- Manifest system
- Optional hot-reload

## Target: v1.0.0 — Release Candidate (G5)

- Performance profiling
- Memory audit
- Balance pass
- Automated tests
- Package & deploy

