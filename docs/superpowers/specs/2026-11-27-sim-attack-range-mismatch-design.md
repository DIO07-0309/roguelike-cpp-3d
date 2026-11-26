# Sim 攻击距离判定不一致 — 根因诊断与定点修复设计

> 日期：2026-11-27 · 状态：**诊断完成；标量方案实测未达标已回退，转角度/宽度几何方案待立项** · 用户目标：**先修"攻击打不掉血"单点**

## 实施结果（2026-11-27）

**保留产物**：`attack_diagnostics.h/.cpp` + `_weapon_attack` 采样 + `balance_report.json.attack_diagnostics` + `simulation_test` 链接修复 —— 攻击链路可观测性（纯观测，不改 gameplay）。

**标量折扣方案实测（已回退）**：按方案给近战攻击半径×0.7 / ×0.5，固定 build A/B：
- 0.7 折扣：命中率 1.2%→5.8%，边缘攻击 92%→57%，但 **未达 10% 护栏**，kills/damage 无提升。
- 0.5 折扣：命中率跌回 1.4%、dmg 90（负回归）——过度收紧后近距离 bucket0=1027 仍大量 miss，证明近距离 miss 是**角度/宽度**问题，非距离。

**结论**：builder 判断被实测证实——根因是命中判定受朝向/形状宽度限制（扇形角度、矩形/胶囊横向宽度），单纯径向折扣有天花板。标量方案已回退；下一步应复用 `AttackGeometry` 做几何命中判定（sim-only），作为独立立项。本设计 §3 的标量方案未证实有效，勿直接复用。

## 一、意图与 why

多轮 redesign 中"最后一只怪清不掉"始终无解。本轮改用诊断优先，拿到了**代码级确凿证据**，定位到攻击链路的双层断点。目标是先修复这个有证据的单点，不再盲目试错。

## 二、根因（实测证据）

### 诊断手段
新增 `src/core/sim/attack_diagnostics.h/.cpp`（纯观测，不改 gameplay），在 `PlayerController::_weapon_attack` 采样，输出 `balance_report.json` 的 `attack_diagnostics` 段。固定命令 `--sim 4 --sim-seed 4242 --sim-build none`。

### 实测数据（4 局固定 build）
| 指标 | 值 | 含义 |
|---|---|---|
| `attack_calls` | 31022 | AI 选了 3 万次 attack |
| `can_attack_pass` | 2016（6.5%） | **93.5% 被武器冷却门控拦截**，没真正出手 |
| `whiffs` | 1991（放行后的 98.8%） | 放行的攻击几乎全部 miss |
| `total_results` | 25（命中率 1.2% = 25/2016 放行） | 2016 次真正出手中只有 25 次命中（非 25/31022） |
| `dist_bucket` | [112,16,37,**1851**] | 放行攻击中 **91.8% 在"怪距>射程3/4"的边缘**（用真最近怪复核值） |
| `crossbow_calls` | 3956 | 弩走弹体池，其空 results 已排除出 whiff 统计 |

> 数据与口径来自 `--sim 4 --sim-seed 4242 --sim-build none` 的一次固定运行；dist_bucket 用真正最近怪计算，reach 用 execute 前的 stage 推导（与决策期同源），whiff 排除弩的正常空 results，无存活怪的哨兵不计入距离桶。

### 根因链（代码级）
1. **`_evaluate_attack`（sim_ai.cpp:216-244）用圆形直线距离判定**：`reach_px = max(current_range,1.5)×32`，`if (d > reach_px) return 0`。怪在 d≈0.75×reach（C4PROBE d3=95.7%）时判"够得到"，给攻击分。
2. **真实命中走方向形状判定**：`WeaponExecutor::execute` → `hit_detect_by_shape`（扇形判角度、矩形判方向延伸，`weapon_executor.cpp:314`）。在射程边缘 d≈reach 时，扇形/矩形的有效命中区域远小于圆形距离圈 → 大量擦边 miss。
3. **武器冷却门控**（`weapon_component.cpp:85-89`）：AI 每帧选 attack，但 `can_attack` 只允许 6.5% 通过。

**结论**：AI 的攻击判据（圆形距离）与武器命中判据（方向形状）不一致，导致 AI 在够不到（形状上）的位置反复攻击、全部 miss；叠加冷却门控，atk 比例虚高而伤害极低。

## 三、修复方案（单点，数据驱动）

### 核心：让 AI 的攻击判据贴合真实命中几何
把 `_evaluate_attack` 的距离阈值从"圆形 reach"收紧为"有效命中半径"，只在真正大概率命中时才给攻击分，否则交给 approach 靠近。

- 新增纯函数 `effective_attack_reach_px(weapon_range_tiles, hit_shape)`：
  - 近战 SECTOR/CAPSULE/RECTANGLE：有效半径 ≈ `reach × 0.7`（扇形边缘折扣，保守）
  - CROSSBOW：保持 reach（弹体不受形状折扣，但走 ProjectilePool）
  - 保底 `max(range, 1.5)`
- `_evaluate_attack` 与 `_select_move_action` 的 `_eval_move_in_range` 统一用这个有效半径——**AI 攻击与移动判据同源**。
- 不修改 `WeaponExecutor` / `hit_detect`（真实游戏命中行为不变），只改 sim AI 的"什么时候决定攻击"。

### 不做什么
- 不改武器数值、冷却、伤害、群系平衡。
- 不改真实游戏（非 sim）的任何行为——`_evaluate_attack` 只在 sim 的 DecisionAgent 里。
- 不碰 rng / 存档 / sim 确定性红线。

## 四、验证方案（严格 A/B）

1. **契约测试**：`effective_attack_reach_px` 纯函数（近战折扣、弩不折扣、保底）。
2. **诊断验证**：修复后重跑 `--sim 4 --sim-seed 4242 --sim-build none`，对比 `attack_diagnostics`：
   - `can_attack_pass` 中 `dist_bucket[3]` 应显著下降（不再在边缘攻击）
   - 命中率 `total_results/can_attack_pass` 应从 1.2% 显著上升
   - `kills/damage` 应上升
3. **确定性**：GOLDEN-003 同 seed 必须一致；GOLDEN-001 因玩法改变按 SOP 重采集（本轮改动 sim AI 决策，必须重采）。
4. **达标**：sim 能正常清怪（atk 与 kills/damage 匹配）；若命中率与 kills 明显改善即算单点达标，F15 通关不作为本轮门槛。

## 五、约束与红线

- 函数 ≤40 行、单一职责、语义命名（CLAUDE.md）。
- 不新增 rng 调用、不改调用顺序（P1C8/RNG-002）。
- 不破坏存档只读、sim 确定性。
- 每 Milestone 更新 CHANGELOG、同步桌面包（规范 7/11）。

## 六、风险

- 收紧攻击半径可能让 AI 过度 approach、减少攻击频率 → 需 A/B 确认 kills/damage 不降反升。
- 有效半径系数 0.7 是估值，需按诊断数据迭代（主指标命中率，护栏 kills）。
- 历史多次单点修补无效——本轮有确凿证据支撑，但仍需严格 A/B 验证，不达预期即回退。
