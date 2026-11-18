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

## 产物

- `reports/balance_report.json`（300 局测量，被最后一次 300 局对照覆盖，数字同上）
- `game.log` 的 `[SIM-DIAG]` 段（卡死采样）
- 无代码改动，工作区干净。
