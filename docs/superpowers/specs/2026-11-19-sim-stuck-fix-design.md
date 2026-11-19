# sim AI 卡死修复立项设计（2026-11-19）

状态：**待用户确认方案**
依赖诊断：`docs/SIM_STUCK_DIAGNOSIS.md`（四轮诊断，根因已锁定）

## 1. 根因（诊断已锁定）

sim 300 局 84.7% 是 STUCK_RECOVERED。第四轮 dump 决定性证据：
- 存活怪 **HP 满血**（AI 从未打到）、距离 9~29 格、`path_reach=1`（路径可达）
- AI 杀掉近处怪后，**不主动追远处怪**——在 loot(0.7)/room(0.5) 与 approach(0.6) 间摇摆，loot 分数高于 approach
- `_stuck_progress`（sim_ai.cpp:1018-1032）把"玩家移动>2格"当作进展，AI 游荡时 `p_move=true`
  反复重置 `_stuck_since`，看门狗拖到 120s 才结算

**核心矛盾**：AI 有资源获取偏好（loot/room）压过战斗推进，且"移动"被误算为战斗进展，
导致"清怪尾段"永远僵持。

## 2. 修复方案（选定方案2：纯追怪模式）

不动卡死判定语义（保留 120s 看门狗兜底），新增独立状态跟踪：

**CombatFocusTracker**（新类，单一职责：根据"无怪掉血时长"判断该不该纯追怪）
- 状态：`_no_combat_since`（首次无怪掉血时间）、`_active`（是否纯追怪模式）
- 输入：每帧怪 HP 总和 + 存活怪数 + game_time
- 规则：
  - 怪 HP 总和下降 或 存活怪数下降 → 重置计时、`_active=false`（有战斗进展）
  - 无进展持续 ≥ N 秒（建议 8s）且仍有存活怪 → `_active=true`（进入纯追怪）
  - 无存活怪 → `_active=false`（清层，交还搜刮/楼梯逻辑）
- 输出：`bool hunting() const`

**DecisionAgent 组合 CombatFocusTracker**（规范3：组合优于继承）
- 在每帧决策入口（`is_action_just_pressed` 的 `_cached_frame` 更新处，sim_ai.cpp:1501）
  调 tracker.observe(monster_hp_sum, alive_count, game_time)
- `_eval_move_loot_and_rooms`（sim_ai.cpp:766）开头：`if (_combat_focus.hunting()) return nullopt;`
  → 纯追怪模式下跳过 loot/room 评分，只剩 approach(0.6)/in_range/attack 竞争
- 这样 AI 持续追远怪直到进入攻击范围打掉它 → 怪 HP 变化 → tracker 自动退出

## 3. 为什么这样设计

- **单一职责**：CombatFocusTracker 只判断"该不该专注战斗"，不碰卡死判定、不碰寻路
- **最小改动**：只在 1 个新类 + 2 个接线点（observe 调用 + loot/room 门控）
- **不动平衡参数**：loot 0.7/room 0.5/approach 0.6 全部不变，只在僵持时临时禁用 loot/room
- **自动退出**：怪一掉血就退出，正常战斗资源获取不受影响
- **保留兜底**：120s 看门狗不动，纯追怪若仍失败仍会被强制结算（不引入新死循环）

## 4. 目标 / 非目标 / 反目标

**目标**：300 局卡死率显著下降（从 254/300 降到明显更低），win_rate/avg_floor 改善。
**非目标**：不改 gameplay 数值、不改资源、不改地图生成、不改卡死看门狗阈值。
**反目标**：破坏 sim 确定性（hash 必须变且稳定——这是行为修复，hash 会合法变化，需固化新基线）、
平衡退化（loot 获取骤降导致深层资源不足）。

## 5. 约束与验收

- 遵守 CLAUDE.md 全部规范（函数≤40行、单一职责、组合、语义命名、影响分析、Code Review、
  Milestone 更新 README、单次≤300行、可维护性优先）
- 契约测试：`CombatFocusTracker` 纯函数测试（进入/退出/重置规则）+ sim 集成测试
- 确定性：固定 seed 多次跑 hash 一致（新行为基线，合法变化一次）
- 回归：ctest 全绿、fn_length 0、world_validator、P3/P3b/P4/P5 闸门不退化
- 平衡验证：跑 300 局对比卡死率，确认下降；抽查 loot/资源未显著退化
- 同步桌面包 + exe 重编译（CLAUDE.md 规范11）

## 6. 待 builder 审查确认

1. CombatFocusTracker 放在哪个模块（src/core/sim/ 还是 src/ai/）？倾向 src/core/sim/（与 DecisionAgent 同层）
2. observe 钩子点是否在每帧都调用（sim_ai.cpp:1501 的 `_cached_frame != _frame` 是否每帧可靠）？
3. N=8s 阈值是否合理（需实跑调参）
4. 是否需要在 2D 路径同步（2D 开发已停止，不需要）
