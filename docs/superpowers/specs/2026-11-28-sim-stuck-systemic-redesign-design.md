# Sim 卡死系统性 Redesign — 设计文档

> 日期：2026-11-28 · 状态：**已实现并固化，win_rate 未稳达标** · 用户目标：**sim 稳定通关 F15 + 赢若干局**（win_rate≥5%、F10/F15 有击杀）
>
> **实施结果**：5 个 per-change 改动全部落地。avg_floor 4.2→8~9.5、卡死 18→10~20、F5 Boss 击杀 3→25、F10 首次可击杀（0→8）、最高到 F15（曾 1/30 胜利）。但 **win_rate 不稳定（0-3.3%）、未稳过 5%**，深层 F12-13 远怪清不掉。用户拍板先固化提交当前成果，F15 稳定通关留后续迭代。Reviewer 修正：SIM_CURVES 第5-11列恢复原表（初版误压平 exp_scale 等）。
> 前置：攻击空挥已修好（`2026-11-28-sim-facing-precision-design.md`，91.8%→17.4%）；本轮攻下一个瓶颈"卡死清不完怪"。

## 一、意图与 why

攻击修复后（20 局 seed4242）：win_rate 仍 0、avg_floor 4.2、**STUCK_RECOVERED 18/20**、F10/F15 零击杀。攻击不再是瓶颈，卡死（清不完怪）成为唯一拦路石。历史多次单点修补均无效（用户此前已回退全部实验），本轮按用户拍板做**系统性 redesign**，先设计后编码。

## 二、根因链（完整证据）

### 2.1 卡死现场（20 局诊断）
- 存活怪：`hp=满血`、`dist=12~35 格`、`path_reach=1`（可达）
- stuck 采样：`mdist=357/528px`（11-16 格）、AI 位置连续 8 次采样不变（原地不动）
- 动作分布：`move=75-83%`、`pick=24%`、`room=45%`、`approach=44%`、`atk=0-2%`
- 假进展 `progress_src.move=2295`（位移信号极高）、`hunt_enters=100` 但 `hunt_approach/hunt_frames=40%`

### 2.2 代码级根因（决策链）
1. **站桩死区**（`_eval_move_in_range` sim_ai.cpp:809-840）：`ideal_dist = atk_range + prefer_range×2`。当怪在 `reach < dist ≤ ideal_dist` 时，返回 0 分站桩（:837），压制 approach 0.6。
2. **攻击分也为 0**：`_evaluate_attack` 在 `dist > reach` 时返回 0（:229）。于是死区内 attack=0、move 站桩=0。
3. **搜刮分趁虚而入**：`_eval_move_loot_and_rooms` 在 `dist > reach+32` 时给 loot 0.7 / room 0.5（:790/:802），压过 0 分的站桩和 approach → AI 去捡东西/找房间，怪永远清不掉。
4. **hunting 不稳定**：`CombatFocusTracker::observe` 在怪掉血时 `hunting=false`（重置），而 AI 偶尔命中近处怪会不断重置；远处满血怪的僵持计时反复归零。
5. **假进展**：`_stuck_progress` 的 `p_move`（位移>2格）反复重置看门狗（2295 次），把结算拖到 120s。

### 2.3 为何单点无效（历史教训 + 本轮实验）
- 提高 hunting 时 approach 分 0.6→1.2：实测 **无效**（room 仍 46691、stuck 18/20）——因为 approach 分被站桩的 0 分在 `_eval_move_in_range` 阶段就短路了，根本没到比较环节。
- 单独改站桩死区（历史）：胜率 10%→3.5%（风筝震荡破坏平衡），已回退（:823-825）。
- 标量折扣攻击距离：5.8%（未达标）、0.5 折扣负回归（已回退）。

**结论：站桩死区 / 搜刮竞争 / hunt 不稳定 / 假进展四者协同，必须一起改 + 500 局回归。**

## 三、方案（协同改，sim-only）

### 3.1 消除站桩死区（核心）
改 `_eval_move_in_range`：当 `dist > reach`（攻击够不到）且存在可达怪时，**不再返回 0 站桩**，而是交还（nullopt）给 `_eval_move_approach`，让 BFS 追怪。仅当 `dist ≤ reach`（真能打到）才站桩。
- 不激活历史有害的"拉开距离/风筝"分支（:823-825 的教训）——只做"够不到就追"，不做"太近就退"。
- 保留近战贴脸步进（:832-835）。

### 3.2 清怪阶段压制搜刮
引入"清怪阶段"判定：场上有存活怪且未清完时，`_eval_move_loot_and_rooms` 的 loot/room 分**降至低于 approach**（如 loot 0.3 / room 0.2），让追怪优先；无怪时才恢复原搜刮权重（房间触发仍需 loot 路径，保留但降权而非禁用）。
- 复用 P6 hunt 开关的语义，但改为"有活怪即降权"而非"8s 僵持才禁"，避免 hunt 反复重置问题。

### 3.3 收紧假进展
`_stuck_progress` 的 `p_move`：有活怪时，单纯位移（loot/room 游荡）不再算进展，只有"距最近怪缩短"或"怪掉血/死亡"才算。这样看门狗能及时触发脱困（传送/拉怪）。

### 3.4 hunt 锁存（协调）
hunting 一旦触发，保持到该层清空（alive==0）或换层，不因零星怪掉血退出——与 3.1/3.2 协同，避免 3.2 降权被 hunt 重置打断。新增独立 `floor_hunting` 状态（不破坏原 hunting 契约/测试）。

### 3.5 不做什么
- 不改武器数值/冷却/伤害/成长曲线（上次攻击修复已让数值够用，卡死是决策问题）。
- 不改真实游戏（非 sim）——所有改在 DecisionAgent / sim 判定内。
- 不碰 rng 调用顺序、存档、sim 确定性红线。
- 不做历史有害的"风筝/拉开距离"分支。

## 四、验证（TDD + 500 局回归）

1. **契约测试**：`_eval_move_in_range` 死区交还 approach、清怪阶段搜刮降权、`p_move` 收紧、`floor_hunting` 锁存。
2. **切片协同验证**：3.1-3.4 作为一个原子变更一起验证（历史证明拆开单独验证会因中间负回归被误退）。
3. **KPI**：300 局 `--sim --sim-seed 4242` + 多 seed，对比 before/after：`STUCK_RECOVERED` 占比 <20%（基线 90%）、avg_floor 明显上升、`win_rate>0`、`boss_kill_counts` F10/F15 非零。
4. **确定性**：GOLDEN-003 同 seed 一致；GOLDEN-001 按 SOP 重采集 + 归因注释。
5. **闸门**：ctest 全量、fn_length 0、validator 0/0、桌面同步。

## 五、约束与红线

- 函数≤40 行、单一职责、语义命名（CLAUDE.md）。
- 不新增 rng 调用、不改调用顺序（P1C8/RNG-002）。
- 不破坏真实游戏行为（改动全在 sim 侧 DecisionAgent）。
- 不回退已修好的攻击方位角修复。
- 每 Milestone 更新 CHANGELOG、同步桌面包。

## 六、风险与诚实评估

- 这是历史多次失败的系统性问题，本方案虽基于完整证据链，但**不保证一次达标**——可能需要多轮 500 局调参。
- 3.1 站桩死区与 3.2 搜刮降权耦合，需一起调；单独改任一个可能负回归。
- 3.4 hunt 锁存需新增状态且不破坏 P6 契约测试，实现需谨慎。
- 达标 = win_rate≥5% 且 F10/F15 有击杀；若多轮仍不达标，如实回退并留档，不再盲目试。
- **这是 sim 核心决策的较大改动，务必先设计审查 + 用户批准再编码。**

## 八、Builder 审查后的决定（用户拍板 + 实施者决定）

### 用户拍板
1. **包含数值+预算调整**（恢复 11-23 授权）：F15 Boss HP/ATK 倍率下调、玩家成长上调、预算放宽，与卡死修复协同。全部加 `g_sim_mode` guard，真实游戏不变。原因：F15 Boss = 620×15=9300HP，sim 裸装 12ATK 需 ~290 次命中，不改数值则 win_rate≥5% 数学上不可达。
2. **房间降权但保留机会窗口**：有活怪时 loot/room 降至低于 approach（非零），清完怪后 15s 楼梯阶段仍可触发房间。确保 sim 能拿到装备/圣物。

### 实施者决定（builder Q3/Q4/Q5/Q6/Q7）
3. **flag 语义**：`floor_hunting`（新增锁存，清怪阶段=true）统一控制 loot/room 降权；原 `hunting()` 保留供 P6 契约测试（纯诊断），`:776` 门控改为 `floor_hunting()`。清空该层（alive==0）或换层时解锁。
4. **p_move carve-out**：dodge/retreat/hazard 分支（会增大距怪距离）不算假进展——本帧动作是这些时，`p_move` 不参与收紧判定，避免 Boss 战斗中误触发脱困。
5. **二分定位**：4 个改动带 per-change 开关（默认开），A/B 失败时可逐个关闭二分定位，避免盲目 4-change 回退。
6. **GOLDEN-001 时机**：调参期间 xfail/disable，最终一次性重采集 + 归因注释；GOLDEN-002/003 全程保持。
7. **§3.1 边界**：nullopt 严格用 attack reach（`max(range,1.5)×32`，与 `_evaluate_attack:229` 同源），不是 ideal_dist；BFS 不可达时不震荡，保留 stand-still。`_eval_move_loot_and_rooms` 短路在 §3.2 落地前使 §3.1 在 `dist>reach+32` 段 inert——这是预期，两改动协同后才生效。
8. **数值 guard**：growth_curve 已有 `set_sim_mode(true)`（main.cpp 置位），玩家成长/预算同样加 `g_sim_mode` 分支。

### Builder Q1 补充
"win"判定口径 = `is_sim_victory`（到达 F15 + 存活 + 无强制结算，不要求 F15 Boss 击杀）；`boss_kill_counts` F10/F15 非零作为额外交叉验证（用户达标线明确要求 F10/F15 有击杀）。设计 KPI 同时看两者。

## 七、留给 builder 的决定

- 3.1-3.4 的具体阈值（降权值、hunt 锁存触发条件）由实施中按 300 局实测迭代确定，设计只给方向。
- 是否暂用 `--goto-floor 15` 做 Boss 战快速代理（需明确其面板写死、无自然装备的口径豁免），视迭代成本定。
