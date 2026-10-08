# AI 路线结项报告（G32 系列）

> 日期：2026-10-08
> 范围：RL / 神经网络路线（"Q 表 → DQN"）
> 结论：**已完成，负收益，生产路径改用规则策略**

---

## 一、最终结论

| 策略 | 实现 | 胜率（L2 场景，独立种子集） |
|---|---|---|
| **规则策略** | `src/ai/mirror/tactical_policy.cpp` | **60.0%** |
| DQN + 课程学习 + Double DQN + 14 维状态 | `tools/rl/train_dqn.py` | 36.7% |
| PPO（Actor-Critic + GAE） | `tools/rl/train_ppo.py` | 18.9% |
| 参考：固定"普攻+技能"基线 | 无代码 | 60.0% |

评测脚本：`tools/rl/eval_ppo.py`（DQN/PPO/基线统一口径）

**为什么学习算法不如规则**：

1. **动作空间高度退化** — `CombatAction` 10 个动作里，只有「普攻 / 技能 / 等冷却」三种有意义，移动价值极低（`get_possible_actions` 里普攻需 `d<2`、技能需冷却结束）
2. **最优策略接近确定性规则** — "冷却好了就打，否则等"，规则代码 10 行可写对，学习算法需百万级样本才逼近
3. **奖励极稀疏** — 胜负在 60~300 步后才出现，中间无有效梯度

---

## 二、有效的改进（保留的技术价值）

即便最终结论负面，以下改进是有效的，且都留在了代码里：

| 改进 | 效果 | 位置 |
|---|---|---|
| **课程学习**（3 档难度递进） | 训练胜率 21.7% → 48.4% | `train_dqn.py:curriculum_params` |
| **Double DQN** | 档内胜率 33% → 47%（治 max 过估计） | `train_dqn.py:compute_loss` |
| **Observation 8→14 维** | 训练 48% → 69.4% | `src/ai/rl/observation.h` |
| **bootstrap 过 mask** | 消除系统性高估 | `train_dqn.py` / `q_agent.cpp:_bootstrap_max` |
| **parity 闸门** | C++ 与 Python 移植逐位一致（12 局 615 步 bit-exact） | `tools/rl/parity_test.py` |
| **手写推理替代 ONNX Runtime** | 138 参数网络，零第三方依赖 | `src/ai/rl/dqn_net.cpp` |

---

## 三、三个被证伪的假设

记录在此，避免日后重犯。

### ❌ 假设 1：「Q 表 → DQN 是下一步升级方向」
**证伪**：DQN 36.7% 显著劣于规则 60.0%。详见 `docs/AI_LEARNING_GUIDE.md` 第 14 章原补记。

### ❌ 假设 2：「Q 值 bootstrap 不动点是 `wait` 主导的根因」
**推理**：`Q(s,WAIT) = r + γ·max Q(s',·)`，而 WAIT 不改变状态，可构造自我循环的 Q 不动点 → 改用 PPO（不做 bootstrap）应能解决。
**证伪**：PPO 的 `wait` 仍占 83.9%。该不动点假设不成立。

### ❌ 假设 3：「`wait` 占比高 = 策略退化」
**证伪**：基线策略 `wait` 占 **91.6%** 却赢 60%。普攻冷却 3.4 步、技能冷却 32~320 步，**冷却期间本来就只能 WAIT**。
**代价**：这个误诊让我花了四轮迭代去"消灭 WAIT"，方向完全错误。
**教训**：**诊断指标必须先与基线对照**。`eval_ppo.py` 与 `bench.py` 都是为此而生。

---

## 四、关键 bug 与踩坑记录

按"是否留在代码里"分类。

### 已修（留在代码里）

| 缺陷 | 后果 | 位置 |
|---|---|---|
| `boss_can_attack` 语义误用 | 它表示"Boss 能否反击"（安全性），不是"玩家能否攻击" | `tactical_policy.cpp` |
| `_apply_attack` 射程不一致 | 声称射程 2.5 但判定 `d<1.5`，导致 d=2 的目标"合法却打不到" | `combat_env_py.py` |
| RL 冷却换算方向反了 | `sec*0.25` 应为 `sec/0.25`，写反后技能近乎无冷却 | `combat_env_py.py` |
| 训练场景怪物打不到近战 | 怪物距玩家 7/5 格，ATTACK 需 d<2 → 近战永远触发不了 | `rl_runner.cpp` |
| `-1` 哨兵被吞 | `combat_action_to_arm(-1)` → `RETREAT`(=1非负)，使 `arm>=0` 恒真 | `mirror_agent.cpp` |

### 工具链陷阱

| 陷阱 | 后果 | 应对 |
|---|---|---|
| **MinGW 依赖追踪只比时间戳** | 脚本改 `.cpp` 再 `Copy-Item` 恢复后 mtime 不变 → 不重编 → 拿变异版 exe 跑校验得到**假失败** | 删 `<target>.dir` 后重编（已固化进 `check_all.py`） |
| **Python `abs()` 是 float64** | 表达式里只裹一个 `F32()` 会被整体提升，曼哈顿距离错 1 格 | 逐项 `F32()` 显式转换 |
| **anaconda cmake 不在子进程 PATH** | `subprocess.run(["cmake",...])` 抛 FileNotFoundError | `find_cmake()` 显式回退 |
| **并行 `git checkout` 会冲掉改动** | 实测 `tests/CMakeLists.txt` 被还原 4 次、`mirror_agent.cpp` 5 次 | 改动放独立文件，不碰对方也在动的文件 |

---

## 五、生产路径现状

```
镜像 Boss 仲裁链（5 层，从高到低）
  ML 层      ← TacticalPolicy（--tactical-mirror 开启时）或 DqnMirrorBridge
  战术链层   ← TacticalChainTable（n-gram 战术序列）
  RL 层      ← QAgent（saves/rl_mirror_q_<STYLE>.json）
  克隆层     ← BehaviorCloneTable（玩家行为克隆）
  Thompson   ← OnlineAdaptivePolicy（在线自适应兜底）
```

**默认配置**：不加 `--tactical-mirror` 时，行为与接入前逐位一致。

### ⚠️ 待人工验收

`--tactical-mirror` **未经游戏内验证**：sim 模式只打到 2 层（`summary.avg_floor = 2.0`），从未触发 F15 镜像 Boss。

验收步骤：
```
roguelike_cpp.exe --tactical-mirror
```
进 F15 镜像 Boss 战 → 查 `game.log` 出现 `[MIRROR] TacticalPolicy active`。

回退：去掉参数即可，零残留。

---

## 六、若将来要重开这条线

**前置条件（缺一不可）**：

1. **真实游戏环境的 Python 镜像** — 需含命中判定、Buff 触发、走位收益、武器伤害系数。当前 `CombatEnvironment` 是简化模型，与真实战斗差距是负收益的根因之一。
2. **补齐 `MirrorBattleState`** — 缺技能冷却与坐标，导致 `Observation` 对合法性是 POMDP（网络物理上无法预测"现在能不能攻击"）。
3. **动作空间去退化** — 4 个技能槽在 `step()` 里完全同质（同伤害/同 AoE/同射程），只有冷却不同，网络学不出差异。真实游戏里斩击/火球/治疗/时停差异巨大。

**推荐顺序**：1 → 2 → 3，每步单独评测。跳过 1 直接调参是 G32 走过的弯路。

---

## 七、资产清单

### 生产代码（可用）
- `src/ai/mirror/tactical_policy.{h,cpp}` — 规则策略
- `tests/mirror/tactical_policy_test.cpp` — 14 测试

### RL 基础设施（保留但不接入生产）
- `src/ai/rl/dqn_net.{h,cpp}` — 手写 MLP 前向
- `src/ai/rl/dqn_policy.{h,cpp}` — 输入构造 + masked argmax
- `src/ai/rl/dqn_mirror_bridge.{h,cpp}` — 桥接到 MlPredictor
- `tools/rl/` — 21 个脚本（parity 闸门 / 训练 / 评测 / 校验）

### 权重产物
**已全部删除并加入 `.gitignore`** —— 权重与 Observation 维度强绑定（8→14 维即失效），入库会造成"陈旧权重 + 新代码"的静默不匹配。