# sim AI 卡死问题 —— 一小时诊断报告（2026-11-19）

> 目标：用户给一小时，能修多少修多少。结论：**定位到根因方向，但未修成**（不宜盲改）。
> 纯调查 + 一次未生效的尝试，未留下任何代码改动（工作区干净）。

## 测量基线

`--sim 300 --sim-seed 1234`（175s）→ `reports/balance_report.json`：
- win_rate **0.0%（0/300）**，avg_floor 3.49，avg_turns 3593
- outcome：**STUCK_RECOVERED 254（84.7%）** / DEATH_MONSTER 28 / DEATH_DOT 16 / TIMEOUT_WALL 2 / VICTORY 0
- boss_kill_counts `[12,0,0]`（12 局杀了第一个 Boss）
- 卡死局统计：`stuck_rot` 214/254=0，`stuck_tp` 235/254=0，`rooms` 139/254=0，`kills` 全部>0

**关键判断**：win_rate=0 的主因不是"平衡太难"，而是 sim AI 卡死（84.7% 被看门狗强制结算）。
历史 P1-B 500 局也是 0/500，数字一致。

## 卡死采样诊断（单 seed 复现，`game.log` 的 `[SIM-DIAG]`）

```
stairs: total=2924 descend=0% move=40%     ← 楼梯阶段 2924 帧, 从不 descend
stuck#1-7: ai=(18,8) 完全相同, open=2 door=1 mdist=326px(10格) r=-1
bfs_stuck_hit=116 bfs_stuck_fail=0          ← BFS 说有路, 但玩家位置不变
近身战豁免 near=8821                         ← 异常高
reach=627 怪可达=1 楼梯可达=0               ← 从玩家位置 BFS 找不到任何楼梯 tile
```

## 根因方向（按证据强度）

1. **楼梯不可达（最强证据）**：`楼梯可达=0` —— 从玩家位置 BFS 到不了 STAIRS_DOWN tile。
   这是**地图连通性/生成问题**（楼梯被墙/门封死，或楼梯不在该连通域），不是 AI 决策逻辑。
   84.7% 卡死局符合"AI 清完能到的怪后，找不到楼梯，在原地耗到看门狗"。
2. BFS 给出方向但玩家不动（`bfs_stuck_hit=116` 位置不变）——可能是 BFS 把 CLOSED 门
   当可走（`is_passable_sim` 语义），玩家实际被门碰撞挡住。但这是一 hour 内尝试的方向，见下。

## 一小时内的一次尝试（未生效，已回退）

假设：BFS 走楼梯路径穿过 CLOSED 门 → 玩家被门挡 → 反复 move 不动。
尝试：在 `_stairs_descend_step` 里，BFS 方向相邻格若是 CLOSED 门则先开门。
结果：脚本改动实际未写入 .cpp（仅 .h 留了一行 extern，已回退）；即便按逻辑推演也未必改善——
因为 `楼梯可达=0` 说明楼梯根本不在可达域，开门也到不了。
跑 300 局对照：outcome 仍 254 卡死，**指标无改善**。故回退，不留无根据改动。

## 结论与建议

- **不宜在一小时内盲改**：根因指向地图生成的楼梯连通性（`楼梯可达=0`），
  修这个要动 dungeon_generator 的楼梯放置/连通性保证，风险高、需完整设计+测试，
  不是"一小时修成啥样"能安全交付的。
- 若要继续：按 brainstorming→设计→计划→TDD 正规流程，先加一个"每局楼梯必可达"的
  world_validator / 地图生成契约测试（RED），再修生成器保证连通性（GREEN），
  最后复测 win_rate/卡死率。这是独立的、边界清晰的任务。
- 次要线索（BFS 门穿透、近身战豁免 near=8821）可在后续单独排查，但优先级低于楼梯可达性。

## 追加诊断（2026-11-19 第二轮，按用户"先定位再修"）

加了 sim 整格步进计数器（`sim_step_attempts` / `sim_step_rollbacks`，player_controller.cpp:236-243
+ sim_ai.h/cpp + game_scene 日志），跑单 seed 复现得到：

```
step attempts=387 rollbacks=6 rollback_pct=1.6%   ← 步进回滚率仅 1.6%
reach=77 怪可达=1 楼梯可达=0                       ← 玩家可达域仅 77 格(全场 1200)
```

**结论修正**：
- 上一轮假设"tile BFS 可走但 rect 步进回滚导致位置不变"**被排除**——回滚率只有 1.6%，
  381 次 move 都成功执行，但净位移为 0 → AI 在几个格之间**来回震荡**（BFS 方向反复横跳），
  不是被门/障碍挡住。
- 玩家被困在 77 格小区域，区域内有怪（`怪可达=1`）但清不掉 → 楼梯永不激活。
- 下一步若要继续定位：需 dump 卡死时"最近怪距离序列 + 怪 HP 是否下降"，
  区分"BFS 方向震荡"与"攻击距离/命中判定失效"。

诊断计数器已提交（e39adb9），不影响 sim 确定性（hash 与基线逐字节一致）、ctest 90/90。

## 第三轮诊断（2026-11-19，"先定位再修"继续）

用现有 P1-C4 探针 + 动作分布日志（未加新代码，直接读）拿到决定性数据：

```
[C4PROBE] attack-eval hit dist (tiles): d0=96.1% d1=0% d2=2% d3=2% ... (n=51)
动作: none=0% move=23% atk=64% skill=0% pick=12% desc=0% pot=0% oth=0%
移动分支: rec=0% loot=4% room=26% appr=17% stand=52%
[SIM-LOOT] best=attack (多次)
```

**结论修正（排除假设）**：
- "攻击距离/命中判定"**被排除**：攻击判定时怪在 d0 桶（贴脸 0~0.25×射程）占 96.1%，
  且 atk=64%、有伤害输出（damage_dealt 均值≈177）、卡死局 kills 均值=13。
  AI 确实贴脸、确实在攻击、确实在杀怪。
- 卡死机制收敛到"**清怪尾段**"：总怪约 17（kills=13 + alive=4），杀到剩最后 4 只时清不掉。
  `怪可达=1` 说明区域内有怪，但 AI 走不到/打不到最后这几只。
- 下一步（若要继续）：专门 dump "最后存活怪的位置 + 可达性 + HP"，看是否怪卡在
  不可达格（墙内/门后/远离）导致 `_find_nearest` 找到但 BFS 走不到，或 HP 异常。
  这是比"攻击判定"更深一层的问题。

## 第四轮诊断（2026-11-19，最终定位）

加了看门狗结算时的存活怪 dump（`_dump_surviving_monsters`），跑 10 局拿到决定性数据：

```
alive#N mon=(18,6) hp=31/31 dist=17t path_reach=1 boss=0     ← 怪满血、在远处、路径可达
alive#N mon=(4,25) hp=36/36 dist=26t path_reach=1 boss=0
surviving_total=4 player=(4,15)                                ← 4 只怪全满血全可达
G26 progress_src move=102                                       ← "玩家移动"被当作进展信号
```

**根因最终锁定**：
- 所有存活怪 **HP 满血**（AI 从未打到它们）、距离 9~29 格、`path_reach=1`（路径可达）。
- AI 杀掉了近处的怪（d0 贴脸攻击那些），但**不主动追远处的怪**——它在 loot(0.7)/room(0.5)
  与 approach(0.6) 之间摇摆（loot 分数高于 approach）。
- 关键机制：`_stuck_progress`（sim_ai.cpp:1018-1032）把"玩家移动>2格"(`p_move`)当作
  **任一类进展**，即使怪 HP 未变（`p_mon=false`）、无怪死亡（`p_alive=false`）。
  AI 在物品/房间间游荡 → `p_move=true` → `_stuck_since` 反复重置 → 看门狗迟迟不触发，
  直到累计 120s 才强制结算 STUCK_RECOVERED。这是 84.7% 卡死的直接机制。

**为何不在此盲改**：修复须改 `_stuck_progress` 的"进展"语义（核心判定，注释明确"任一为真"
是刻意设计），或重调 loot/approach 分数（注释警告"必须重跑 500 局平衡回归"）。
按 CLAUDE.md 规范 9（更好设计先讨论）与规范 10（可维护性优先），这需要独立设计 +
TDD + 完整回归，不是诊断轮能安全交付的。

**建议的正确修复方案（下次立项）**：
1. `_stuck_progress` 增加"战斗进展"判定：当存在存活怪但 `p_mon==false && p_alive==false` 时，
   `p_move` 只有在"与最近怪距离缩短"时才算进展，避免游荡重置计时。
2. 或：给"楼层未清空 + 长时间无怪 HP 变化"加一个独立计时器，超时强制切到"纯追怪模式"
   （临时禁用 loot/room 评分），直到进入攻击范围。
3. 无论哪种，必须加契约测试 + 重跑 300-500 局验证卡死率下降且平衡不退化。

## 当前收敛状态

四轮诊断逐层排除：① 楼梯可达=0 是伪信号 → ② 步进回滚仅 1.6% → ③ 攻击判定正常 →
④ 存活怪满血可达但 AI 不追 + `_stuck_progress` 把游荡算进展。
根因锁定在 sim AI 决策/卡死判定语义。未改任何 gameplay 行为；
仅加诊断计数器（e39adb9）与存活怪 dump，sim hash 与基线逐字节一致、ctest 90/90。

## 第五轮诊断（2026-11-19，P7: hunt 触发/追怪行为）

加了 hunt 计数器（enters/frames/approach_frames）+ 存活怪 dump，并做了一次门控实验：

**诊断数据（P6 现状，8s 阈值）**：
```
hunt enters=33 frames=42590 approach_frames=3631 approach_pct=8.5%   ← hunt 触发了, 但只有 8.5% 帧在追
```
即 P6 的 hunt 模式**虽然触发**，但 AI 绝大多数帧没选 move——门控"禁 loot/room"后，
`_eval_move_in_range`（sim_ai.cpp:809-839）的"射程内站桩 0 分"仍压制 approach
（远程 build ideal_dist≈10格, 9格内怪被判"该站桩"）。

**门控实验**（临时让 hunt 也跳过 `_eval_move_in_range` 站桩分支）：
```
approach_pct 8.5% → 70.0%                    ← AI 终于真的在追了
avg_floor 4.01 → 4.44
STUCK_RECOVERED 239 → 237                    ← 卡死率几乎不变!
存活怪仍满血, 距离 12~37 格, player 停在角落 (9,11)
```

**决定性结论**：即便 AI 全程在追（approach 70%），也**清不完散布在 12~37 格外的怪**——
卡死的 239 局不是"AI 不追怪"能解决的。根因是 sim 探索/寻路/预算的系统性问题
（地图 40x30、怪散布远、120s 预算 + 可能的 BFS 震荡），不是单点 bug。
故**回退该实验门控**（approach 70% 却卡死不变, 且改变远程 build 正常战斗行为有风险）。

**最终判定**：sim AI 卡死是系统性问题, 单点修补（禁 loot/room、禁站桩、调阈值）均无效或微弱。
需整体 redesign（探索策略/怪分布/预算/目标选择协同）, 不是可在本会话安全交付的改动。
P6 的 CombatFocusTracker 保留为"部分缓解 + 诊断基础设施", 不再加新门控。

## 产物

- `reports/balance_report.json`（300 局测量，被最后一次 300 局对照覆盖，数字同上）
- `game.log` 的 `[SIM-DIAG]` 段（卡死采样 + 步进回滚率）与 stdout 的 `[C4PROBE]`/`动作` 分布
- 诊断计数器改动已提交（e39adb9），无 gameplay 行为改动。
