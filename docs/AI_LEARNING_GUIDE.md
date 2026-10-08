# Roguelike C++ AI 技术完整学习手册

> 项目：Roguelike C++ v1.0.0（C++17 + Raylib 5.0）
> 性质：理论原理 × 真实代码级实践 —— 每个知识点都有你项目中的文件:行号证据
> 适用：人工智能专业学生 | 前置：概率论、线性代数、C++/Python 基础
> 本版 = 技术全景指南 ∪ 源码深度审查（精华合并版）

---

## ⚠️ 第 14 章的结论已反转（2026-10-08 补记）

**第 14 章「升级实验路线：DQN 完整实现」不再是待办事项，而是已完成的实验 —— 且实验结论是负面的。**

G32 系列做了十轮实测（详见 `README.md` 的 G32-C~G32-F），最终结论：

| 策略 | 胜率（L2 场景，独立种子） |
|---|---|
| **规则策略**（`src/ai/mirror/tactical_policy.cpp`） | **60.0%** |
| DQN（含课程学习 + Double DQN + 14 维状态） | 36.7% |
| PPO（Actor-Critic + GAE） | 18.9% |
| 参考：固定「普攻+技能」基线 | 60.0% |

完整结项报告（含被证伪的假设、工具链踩坑、重开此线的前置条件）见 `docs/AI_RL_ROUTE_CLOSEOUT.md`。

**结论：当前玩具环境不适合用学习算法。** 10 个动作里只有「普攻/技能/等冷却」有意义；最优策略接近确定性规则（"冷却好了就打，否则等"）；奖励稀疏（胜负在 60~300 步后才出现）。任务本身接近被规则解决，学习算法无增量价值。

**三个被证伪的假设**（都写在第 14 章里，别再当结论用）：
1. ❌ 「Q 表 → DQN 是下一步升级方向」—— 实测 DQN 显著劣于规则
2. ❌ 「Q 值 bootstrap 不动点是 `wait` 主导的根因」—— PPO 同样 `wait 83.9%`，该假设不成立
3. ❌ 「`wait` 占比高 = 策略退化」—— 基线策略 `wait` 也占 91.6% 却赢 60%（冷却期间本来就只能等）

**第 14 章保留为教学材料**（网络定义 / 经验回放 / 训练循环的写法是对的，且踩坑记录有价值），但**不要再照它去做升级**。若日后真要用 RL，前置条件是先做**真实游戏环境的 Python 镜像**（命中判定、Buff 触发、走位收益），而不是继续调 `CombatEnvironment` 这个 toy env。

**当前镜像 Boss 的生产策略是规则驱动的**：`src/ai/mirror/tactical_policy.cpp`（命令行 `--tactical-mirror` 开启）。

---

## 目录

**第一篇 全景与决策架构**
1. AI 技术全景与代码地图
2. Utility AI：DecisionAgent 评分决策
3. 行为树：BTAgent 与节点框架

**第二篇 搜索与学习算法**
4. MCTS：蒙特卡洛树搜索
5. Q-Learning 强化学习
6. A* 导航与 BFS 危险避让

**第三篇 在线学习与模仿**
7. Thompson 采样（多臂赌博机）
8. 行为克隆 / 模仿学习
9. n-gram 序列建模
10. 镜像学习系统（MirrorAgent）

**第四篇 工程基础**
11. 确定性游戏与对拍验证
12. 教科书错误集合专题（六道活教材题）
13. 修复优先级路线图
14. 升级实验路线：DQN 完整实现 ⚠️ **已实测, 结论负面, 勿照做**(见文首补记)
15. 参考文献与资源

---

# 第一篇 全景与决策架构

## 1. AI 技术全景与代码地图

```
搜索与规划          机器学习              序列建模
├── A*             ├── Q-Learning       ├── n-gram
├── MCTS           ├── Thompson 采样     ├── 行为克隆
└── BFS            ├── Utility AI        └── 镜像学习
                   └── 贝叶斯在线更新
```

| 技术 | 目录 | 关键文件 |
|------|------|----------|
| DecisionAgent | `src/core/sim/` | sim_ai.cpp/h（评分决策+BFS 导航） |
| 行为树 | `src/ai/behavior_tree/` + `src/ai/agents/` | bt_agent.cpp, bt_selector.h |
| MCTS | `src/ai/mcts/` | mcts_search.cpp, simulation_state.cpp |
| Q-Learning | `src/ai/rl/` | q_agent.cpp, environment.cpp |
| A* | `src/ai/navigation/` | pathfinder.cpp |
| Thompson | `src/ai/mirror/` | online_adaptive_policy.cpp |
| 行为克隆 | `src/ai/mirror/` | behavior_clone_table.cpp |
| n-gram | `src/ai/mirror/` | tactical_chain_table.cpp |
| 镜像学习 | `src/ai/mirror/` | mirror_agent.cpp（13 文件核心） |
| 确定性 | `src/core/replay/` | state_hash.cpp |
| 行为画像 | `src/game/ai/player_behavior/` | player_behavior_analyzer.cpp |

**成熟度评级**（源码审查结论）：

| 模块 | 成熟度 | 一句话 |
|------|--------|--------|
| DecisionAgent | ★★★★☆ | 实战淬炼最充分，注释带事故编号 |
| MirrorAgent | ★★★★☆ | 架构最佳但有数学 bug（先验爆炸） |
| MCTS / RL | ★★★☆☆ | 高质量原型，含教科书级错误 |
| BTAgent | ★★☆☆☆ | 树存在接线致命伤，未闭环 |
| A* / 画像管线 | ★★★★☆ | 高质量但未接入主线 |

---

## 2. Utility AI：DecisionAgent 评分决策

### 2.1 原理

Utility AI 不用固定优先级或状态机，而是给每个候选动作打一个**效用分数**，选最高者。优势：动作之间自然权衡（攻击 0.8 vs 拾取 0.6 vs 撤离 1.4 → 选撤离），无需枚举状态转移。

### 2.2 你的实现（sim_ai.cpp:624-696 argmax 选择器）

**Build 感知画像参数**（sim_ai.cpp:73-131）：12 种 BuildType 各有参数档：

| 参数 | 含义 | 极端值 |
|---|---|---|
| `_prefer_range` | 风筝倾向 | ICE_MAGE=0.9 / BERSERKER=0.0 |
| `_prefer_aoe` | 群战倾向 | ICE_MAGE=0.8 |
| `_prefer_skill` | 技能优先度 | SUMMON_LORD=0.8 |
| `_prefer_heal` | 治疗血线阈值 | SUPPORT=0.50 / 默认 0.35 |

**各动作评分构成**：

```
attack  = 近战偏好 × 距离线性衰减          （射程 48px 硬截断 :148）
skill   = 技能偏好 × (0.5 + AoE群体加成) + Boss战补DPS 0.9   (:154-169)
pickup  = 1.5 × (1 - 最近怪威胁场)         （须站在房间上 :503-527)
heal    = 硬覆盖 best_score=3.0            （血线触发，非评分 :641-654）
event   = 三级自杀保护一票否决 + 高价值加权   (:710-730)
```

**Move 内部隐式优先级阶梯**（sim_ai.cpp:428-501）——量级设计纪律：

| 分值 | 情形 |
|------|------|
| 1.4 | Boss 蓄力中逃离 |
| 1.2 | 站毒池圈内找安全方向 |
| 0.8 | 路径记忆延续步 |
| 0.6 | BFS 逼近 / 战斗间隙走向特殊房 |
| 0.4 | 远程拉开距离 |
| -1.0 | 落点是危险圈 |
| -999 | 目标格不可行走 |

**底层导航三件套**：
- `_bfs_toward`：首步传播技巧（`first[ni] = (cur==起点) ? d : first[cur]`）免回溯链
- `_bfs_away`：怪物为源的全图距离场 flood fill
- 降级链 BFS(避毒)→BFS(不避毒)→greedy_step；卡死逃脱状态机 ≥2s 轮换方向 / ≥8s 传送兜底

### 2.3 活教材①：远程 build 攻击死区

```cpp
// sim_ai.cpp:148  attack 射程硬截断
if (d > 1.5f * 32.0f) return 0;
// sim_ai.cpp:471-475  move 站桩半径
float ideal_dist = atk_range + _prefer_range * 2.0f;  // ICE_MAGE ≈ 138px
```
**症状**：48px ~ 138px 环带内 attack=0 且所有 move≈0 → AI 在怪面前随机抖动。
**教训**：分段线性函数的接缝处必须连续——Utility AI 各动作的分值域要设计成互补覆盖，否则出现"所有选项都不想选"的死区。
**修复方向**：attack 在 [射程, 理想距离] 区间给"逼近收益斜坡"而非断崖。

### 2.4 学习亮点（工程范本）

1. **BFS 越界防御矩阵**（sim_ai.cpp:283/325/334/371 四处 clamp）——每处注释写明对应的事故场景（玩家出图/怪被击退出图/数据异常），从真实堆损坏逆向出来的防御体系
2. **tile↔rect 双语义对齐**（:238-243）——BFS 用 32×32 探针矩形调 is_rect_walkable，保证"BFS 说能走"="物理引擎说能走"
3. **路径记忆键选型论证**（sim_ai.h:92-95）——用 instance_id 不用指针："跨进程堆地址不同+地址复用 → 旧记忆污染新怪 → 决策分叉"
4. **同帧一致性缓存**（:742-746）——保证"这帧想做什么"对所有查询单一事实来源
5. **卡死语义辨析**——用"怪方 HP 总量变化=战斗进行中"区分真卡死与换血僵持，比"怎么脱困"更见功力

### 2.5 已知问题速查

- 治疗覆盖循环缺 break → 多自愈槽时优先级倒置（:643-653）
- 口袋传送第二遍循环与第一遍逐字节相同（死代码），落点可能踩熔岩（:206-220）
- 搜刮看门狗跨楼层不复位 → 清层后直接下楼丢资源（sim_ai.h:100-101）
- `_aggro_bias` 参数在 11 个 profile 赋值但零消费（死参数）
- 性能：每方向独立重跑全图 BFS，最坏 ~12 次/帧——应提升到 tick 层每帧每类一次

---

## 3. 行为树：BTAgent 与节点框架

### 3.1 原理

行为树用四种节点组织分层决策：Selector（OR 短路）/ Sequence（AND 短路）/ Condition（谓词）/ Action（副作用）。游戏工业标准（Unreal 内置），优势是逻辑可视化与复用。

### 3.2 你的实现

框架完整（bt_node/selector/sequence/condition/action/blackboard 八件套 + clone 深拷贝协议）。树结构（bt_agent.cpp:83-173）：

```
Selector 根
├── BossIntro → 确认      ← ⚠️ 实际挂的是裸条件
├── StairsActive → 下楼    ← ⚠️ 同上
├── hpLow ∧ healReady → skill_3
├── bossNear → attack
├── ≥2敌 ∧ skillReady → skill_2
├── enemyNear → attack
├── nearRoom → pickup
└── Wander → move_up 兜底
```

Blackboard 用 `unordered_map<string, std::any>` 投影游戏状态（hp_ratio/enemy_near/boss_near/skill_ready 等）。

### 3.3 活教材②：接线级致命伤（两处）

```cpp
// P0-1: bt_agent.cpp:162-163 — 动作创建了但从未挂进树
root_children.push_back(bossIntro->clone());     // 裸 Condition! 不是 Sequence{cond, actConfirm}
root_children.push_back(stairsActive->clone());  // actDescend 从未进树（零引用死对象）

// P0-2: bt_agent.cpp:56 — 冷却判断传死时间
player->skills.active_skills[i]->can_use(0)      // 应传 game_time!
// Skill::can_use 实现: (game_time - last_use_time) >= cooldown
// → 游戏开局恒 true（假阳性）；用过一次后 0-T < CD 永远 false（永久假阴性）
```
**症状**：`--sim-ai bt` 下永远无法下楼、无法确认 Boss 开场。
**教训**：(a) Selector 命中裸条件的 SUCCESS 即短路，默认 action 保持 move_up——"看起来能跑的树"和"真正接线的树"差一步；(b) 时间参数传递错一位，语义全毁。**修好前不要用 BT 做 A/B 对比实验（结论无效）。**

### 3.4 升级路径

当前树无 RUNNING 语义（纯一次性反应树）。演进方向：
1. 引入 RUNNING 状态 → 跨帧执行记忆（传统执行树）
2. Decorator 节点：Cooldown / Retry / Timeout / Inverter / Parallel
3. 补同帧缓存（照抄 DecisionAgent 的 Q3.1 模式）
4. 两份 profile 表（sim_ai 与 bt_agent 各一份 switch）已开始漂移 → 合并共享 ProfileProvider

### 3.5 实践练习

1. 修复 P0-1/P0-2（第一题就是真 bug！），跑 `--sim 20 --sim-ai bt` 验证能通关到 F2+
2. 给树加 CooldownDecorator：让 Wander 每 0.5s 才换一次方向
3. 实现 Parallel 节点：移动同时保持攻击判定

---

# 第二篇 搜索与学习算法

## 4. MCTS：蒙特卡洛树搜索

### 4.1 原理

MCTS 用**随机模拟统计**替代精确评估：不需要知道每个状态的绝对价值，模拟大量对局后哪个动作胜率高选哪个。AlphaGo 核心 = MCTS + 神经网络评估。

每轮迭代四阶段：

```
Selection   从根用 UCT 公式下沉到叶子
Expansion   在叶子创建新子节点
Simulation  从新节点随机玩到终局（或深度截断）
Backprop    结果沿路径向上更新统计
```

**UCT 公式**（探索-利用权衡的数学解）：

```
UCT = Q̄(s,a) + C·√(ln N(s) / N(s,a))
      └─利用─┘   └───────探索───────┘
```

### 4.2 你的实现（代码级）

```cpp
// mcts_node.h:20-25 — UCT 确切形式
double uct_value(double C = 1.414) const {
    if (visits == 0) return 1e9;          // 未访问节点乐观初始化
    double exploitation = reward / visits;
    double exploration  = C * sqrt(log(parent->visits) / visits);
    return exploitation + exploration;
}
```

| 阶段 | 实现细节 |
|---|---|
| Selection | 自根向下取 UCT 最大（mcts_search.cpp:155-169） |
| Expansion | **一次性展开全部动作**，每子节点克隆父状态（:171-182） |
| Simulation | rollout 深度上限 10，动作 `(rng+depth)%n`（:184-195） |
| Backprop | 沿 parent 累加，**每上一层 reward ×= 0.95**（:202） |

终局评估（combat_evaluator.cpp:6-18）：胜负 ±1000 ≫ 击杀 +200 ≫ HP×2 ≫ 怪HP×1.5 − 深度×5。

架构亮点：SimulationState 零依赖快照（无 GameScene/渲染依赖），clone() 值语义线程友好；状态内嵌 LCG + 质数偏移使同状态搜索可复现。

### 4.3 活教材③：C=√2 与 ±1000 的量纲失配（全项目最经典错误）

```
奖励范围 [-1000, +1200] → 兄弟节点 Q 差值通常几十到几百
探索项上界 C·√(lnN/N) ≈ 2.5
→ 探索项永远翻不动利用项排序 → UCT 退化为纯贪心！
```

**原理**：C=√2 是 Kocsis & Szepesvári 2006 论文中奖励归一化到 [0,1] 时的理论值。量纲变了常数不变，等于没做探索。你的 MCTS 表现"总是选第一个看起来好的动作、从不尝试别的"，根因在此。
**修复**：评估函数输出 sigmoid 压缩到 [0,1]，或 C 提到 ~250（≈奖励量级的 1/4）。一行改动，行为剧变——亲自改一次胜过读十遍论文。

### 4.4 其余问题与优化

- **A6 快照锻造禁普攻**：`attack_cooldown=max(0,0.5)` 恒 0.5（sim_ai.cpp:35），而合法动作要求 ≤0 → 根节点永远没有 ATTACK 选项
- **A2 WAIT 偏差**：expand-all 后恒取 `children.back()`（WAIT 排最后）→ 新节点首轮模拟系统性偏向等待
- **A3 回传折扣 0.95**：不同深度节点均值不可比，而 UCT 恰在同一父下比较兄弟均值 → 建议删除
- buffs/mdef 克隆了但伤害公式从不消费（"深克隆"是假象）；怪物永不动；穿墙合法
- 优化路线：真实冷却注入（2 行）→ 单子扩展 → 树复用（现在每个决策帧丢弃整棵树）→ 截断 rollout 用评估函数打分降方差

### 4.5 升级路径（AlphaGo 方向）

规则评估 ±1000 → 监督训练策略网络（用玩家行为数据）→ 策略网络引导 simulation → 价值网络替代评估函数 → Self-Play 迭代。

---

## 5. Q-Learning 强化学习

### 5.1 原理

贝尔曼方程驱动的值迭代：

```
Q(s,a) ← Q(s,a) + α·[r + γ·max_a' Q(s',a') − Q(s,a)]
```

收敛条件（随机逼近）：Σαₜ=∞ 且 Σα²ₜ<∞。

### 5.2 你的实现（代码级）

**Q 表键结构**（q_agent.cpp:18-20 + observation.cpp:42-53）：

```cpp
// 键 = "<hp>:<atk>:<ec>:<dist>:<boss>:<style>|<action_id>"
hp_b  = player_hp_ratio * 10;        // 11 桶
atk_b = player_attack / 5.0f;        // ⚠️ 无上限
nd_b  = nearest_enemy_dist / 2.0f;   // ⚠️ 无敌哨兵 999 → 桶49
```

注意：Observation 向量实际是 **8 维**（含 buff_count、player_style），但 `to_key()` 只离散化 6 维——buff_count 被表格版丢弃。

超参数：α=0.1（训练）/0.15（镜像）**全程不衰减**，γ=0.9，ε 从 0.12 线性退火至 0.005。Reward：击杀+50 / 掉血按差罚 / 胜±200。

工程亮点：
- `exploit_action` 返回 -1 表示"Q 表没见过该状态"（q_agent.cpp:37-45）→ 让离线模型以**置信度感知**方式插入仲裁链——教科书式降级设计
- Q 表 JSON 持久化续训；文件缺失静默跳过注入（容错隔离）
- 末段 10% 低探索胜率窗口作为收敛度量（rl_runner.cpp:74,94-96）

### 5.3 活教材④⑤⑥：三个经典错误连击

**④ 缺 done 标志 → 终局自举污染**（q_agent.cpp:77-84）
```cpp
if (max_next < -999) max_next = 0;  // 把"键不存在"当终止
// 但真终止转移若键已学过，照常自举 γ·maxQ(s_terminal)
```
DQN 论文第一课：done 时 target 必须只等于 r。签名缺 `bool done` 参数是结构性缺陷。

**⑤ 学习率不衰减 → 永不收敛**
α 恒 0.1 违反 Σα²<∞。随机伤害浮动下 Q 值永远震荡。修复一行：`α = α₀/(1 + visits(s,a)·k)`。

**⑥ 击杀奖励按尸体存量发**（environment.cpp:29 vs :101）
```cpp
int prev_alive = 0;  // ← 算了不用！死变量暴露原意是增量式
...
if (!m.alive) reward += 50.0;   // 每步对每具尸体重复发奖
```
总回报 = 50×(T−t_kill)，γ 折扣下的回报结构扭曲。改为前后存活差一次性发放。

### 5.4 DQN 升级路径（详见第 14 章完整代码）

修 done → 观测切 to_vector() 8 维并归一化 → 经验回放 → 目标网络 → ε 步级衰减 + Adam。

---

## 6. A* 导航与 BFS 危险避让

### 6.1 原理

```
f(n) = g(n) + h(n)     h 用曼哈顿距离（4 连通网格可采纳且一致 → 保证最优解）
```
优先队列（堆）+ 惰性删除过期项是标准数据结构组合。

### 6.2 你的实现

pathfinder.cpp 实现质量良好：扁平 SoA 数组、惰性删除 priority_queue、WalkableFn 回调注入零游戏依赖、7 维测试覆盖（最优性/绕障/不可达/确定性等）。

**关键事实**：`find_path` 仅被测试引用——运行时导航全部走 sim_ai 手写 BFS。A* 是一座建成未通车的桥。

### 6.3 活教材⑦：同一仓库两种防御标准

- pathfinder.cpp:42-46 对 start/goal **零边界校验**——越界即 vector UB
- 而 sim_ai.cpp 有四处 clamp（Q3.13 血泪教训："否则 first[] 越界写堆损坏"）
实体坐标可能出图（击退/传送，见 sim_ai.cpp:324 注释），A* 一旦接入主线必炸。**教训**：防御性编程标准要全局统一，不能只有"出过事故的模块"才设防。

### 6.4 升级路径

JPS（均匀网格剪枝，4 向版实现简单）/ Flow Field（多单位共享导航场）/ D* Lite（动态地图增量重规划）/ 部分路径 API（max_steps 耗尽返回最接近目标的可达点）。前提：先统一 A* 与 BFS 两套导航栈的 hazard 代价语义。

---

# 第三篇 在线学习与模仿

## 7. Thompson 采样（多臂赌博机）

### 7.1 原理

MAB 问题：K 台胜率未知的老虎机，有限投币次数，最大化总收益。Thompson 解法：**给每个臂维护 Beta(α,β) 后验，每轮从各臂采样取最大**——天然平衡探索（不确定性大的臂偶尔采出高值）与利用（好臂大概率采出高值）。理论遗憾界最优。

Beta 共轭性：Beta(α,β) 观测到成功 → Beta(α+1, β)，失败 → Beta(α, β+1)。

### 7.2 你的实现（代码级，全项目数值最严谨的模块）

```cpp
// online_adaptive_policy.cpp
// 分桶: dist <96/<192/else × hp <35%/<70%/else = 3×3 = 9 桶 (:27-32)
// 每桶 4 臂 (APPROACH/COMBO/SKILL/RETREAT), 各维护 alpha[9][4], beta[9][4]
// 决策: 每臂采 Beta 样本取 argmax (:34-42)
// 更新: alpha += reward; beta += 1-reward   (reward 是 [0,1] 连续值 :44-49)

// reward shaping (mirror_agent.cpp:326-328):
float reward = hit ? min(1.0f, 0.6f + 0.4f * min(1.0f, damage/30.0f)) : 0.0f;

// Gamma 采样: Marsaglia-Tsang 2000 完整实现 (:102-122)
// 含 a<1 的 boost 变换 _sample_gamma(a+1)*u^(1/a)、squeeze 快速路径、出处注释
```

画像先验注入（冷启动知识）：predict_low_dodge → APPROACH 臂 α+=2.0 等，注释明确"只影响冷启动，反馈会纠正错误先验"——语义正确且量级保守。

工程亮点：
- **幂等反馈防护**：更新后立即 `_last_bucket=_last_action=-1`（mirror_agent.cpp:332-333）——防多处调用方重复上报同一决策。在线 bandit 集成最易踩的坑，这里有标准答案。
- Box-Muller 有 u 下限保护；Beta 归一化除零回退 0.5

### 7.3 活教材⑧：跨局先验爆炸（探索能力衰减至零）

```
会话 N:   fresh α=1 → init_prior +2 → 战斗奖励 r → 导出 (3+r) 存档
会话 N+1: fresh α=1 → init_prior +2 → import +(3+r) → α=6+r
会话 N+k: α ≈ 1 + 2(k+1) + Σr    ← 线性膨胀！
```

import 是纯叠加无遗忘（online_adaptive_policy.cpp:67-78），而 init_prior **每次会话都重新执行再被存档固化**。后果链：
1. Thompson 后验方差 ∝ 1/(α+β)² → 探索概率随局数趋零 → Boss 对玩家改打法彻底失去适应力
2. float 到 ~2²⁴ 后 `+= reward` 静默失效
3. 存档 %.4f 截断进一步损失精度

**修复一行**：`alpha ← λ·alpha + reward`（λ≈0.99 指数遗忘），或导出时减去已知先验分量。

### 7.4 升级路径

9 桶是对连续上下文的粗暴量化（96px 边界两侧行为完全不同）→ **LinTS/LinUCB**（θ_a ~ N(μ,Σ)，特征向量 [1,dist,hp,skills_ready,attacking,in_domain] 消除桶边界效应，闭式更新 O(A·d²)）；再加 discount factor 处理非平稳玩家。

---

## 8. 行为克隆 / 模仿学习

### 8.1 原理

模仿学习最简形式：从专家（玩家）轨迹学映射 s→a。问题：数据稀疏（80 个桶 vs 无限状态）、分布漂移（DAgger 动机）、无奖励信号（GAIL 动机）。

### 8.2 你的实现（代码级）

```cpp
// behavior_clone_table.cpp
// 状态桶: dist 5档(<2/<4/<8/<14/≥14格) × hp 4档 × skill 4档 = 80 exact 键
// 键编码: "d%d:h%d:s%d" 字符串 → unordered_map<string, array<int,7>>
// 置信度 = best_n / total (:102)

// fuzzy 匹配真实逻辑 (:114-130): 不是查 fuzzy_key()，
// 而是遍历合并全部 4 个 skill 桶的计数后 argmax —— 正确做法！

// 降级链: exact(level 0) → fuzzy(1) → profile fallback(2) → default(3)
```

采集管线（player_behavior/）同样讲究：上下文回填（缺省字段用最新帧补齐，recorder.cpp:27-35）、MOVE 方向变化去重压缩流体积、hook 方零侵入产出可学习样本。

### 8.3 活教材⑨：压线置信度的隐式耦合

profile/default 层置信度**硬编码 0.5**（behavior_clone_table.cpp:156,161），恰好贴着仲裁门槛 clone_confidence=0.50。默认配置下 profile 层"永远刚好通过"；一旦漂移检测把门槛上浮到 0.75，预测链的 profile 层**永久静默失效**——没人改过任何东西，行为却变了。
**教训**：魔法数之间的隐式耦合要用常量关系表达（如 profile_conf = threshold - ε），否则调参时必然踩雷。

### 8.4 特征缺陷（画像管线）

- retreat_rate 用绝对"上方向"冒充撤退（analyzer.cpp:55-58）——敌在上方时向上恰是进攻；facing_dir 已采集未使用
- fight_back_rate 可 >1（动作数/受击数，量纲错）
- avg_damage_taken 除以末条记录楼层号而非楼层数（:67-71）
- hp 字段已采集从未消费——治疗时 HP 分布直方图可替代拍脑袋的阈值
- JSON 序列化丢全部 M1/M5 特征，load 空 stub → 跨进程训练无法闭环

### 8.5 升级路径

计数表 → softmax 策略（按概率采样）→ 神经网络策略 → DAgger（专家状态下收集模型数据迭代训练）→ GAIL（对抗学奖励）→ 逆强化学习。

---

## 9. n-gram 序列建模

### 9.1 原理

给定前 n−1 个符号预测第 n 个。NLP 语言模型的起点（n-gram → RNN → Transformer 演进链的第一环）。核心权衡：n 大捕获长模式但数据稀疏。

### 9.2 你的实现（代码级）

```cpp
// tactical_chain_table.h — 11 符号
// SKILL_0..3 (skill_id) / MOVE_×4 (方向) / COMBO_1..3 (combo_stage)
// DODGE/HEAL 不参与（返回 -1 过滤）

// base-11 三位数打包键 (:51-54) — O(1) 零分配 cache 友好
key = s0*121 + s1*11 + s2;
_prefix_total[s0*11+s1]  // 2-前缀分母 121 项
_s0_total[11]            // 1-前缀分母

// 退化: 3-gram 分母<3 或最优计数≤0 → fuzzy2(s1); 2-gram s0_total<5 → miss
```

### 9.3 活教材⑩：在线符号坍缩

在线路径的类型级近似（mirror_agent.cpp:226-232）把所有技能映射到 SKILL_0、所有攻击段映射到 COMBO_1、MOVE 直接不进链：

```
离线 build(): 11 符号流, 键空间 11³=1331
在线 predict(): 只有 {SKILL_0, COMBO_1} 两符号, 有效键空间 = 8
→ 离线表含 MOVE 的 2-前缀行在线永远命不中 → 大半成死数据
```
**教训**：训练与推理的特征空间必须一致（train/serve skew）——机器学习系统最常见的隐性事故。

另一问题：硬切换降级而非 backoff 插值平滑；confidence=MAP 频率比在小样本(n=3 全同后继)时达 1.0 无收缩——应加 Laplace 平滑或 Wilson 下界。

### 9.4 升级路径

Katz/Kneser-Ney 回退插值 → 4-gram（11⁴≈14.6k 键仍很小）→ GRU（隐藏维 16 足够）→ Attention/Transformer。

---

## 10. 镜像学习系统（MirrorAgent）

### 10.1 架构：双链分离 + 五层仲裁

**两条独立的降级链**（职责清晰，很多模仿学习实现会把它们搅在一起）：

```
预测链 predict_next_action (供打断/预判, mirror_agent.cpp:171-201):
  战术链(conf>门槛, >=) → 克隆层(phase≥2, level≤2 含profile!, conf>=门槛) → 规则兜底

仲裁链 recommend_action (选Boss行为, mirror_agent.cpp:252-308):
  L0 ML插槽(默认关) → L1 战术链(>) → L2 RL Q表(exploit≥0) → L3 克隆(level≤1 仅exact/fuzzy, >)
  → L4 Thompson 兜底(含40%施法旁路探索)
```

三阶段人格参数（mirror_tuning.h 单例）：P1→P2 需准确率≥0.65∧观测≥20（或 40 次兜底/12s 时间兜底）；P2→P3 需同桶命中 10 次∧准确率≥0.70（或任一方 HP<35% 直通）。跨局记忆 export/import 各 36 floats（桶主序 b*4+a；头文件注释写 "144 float" 是字节与浮点数混淆的文档 bug）。

### 10.2 设计问题

- 固定优先级短路无融合：ML 说 A、战术链说 B(0.9)、克隆说 C(0.95) 时最终取 ML——没有投票或"高层须显著优于低层才接管"机制 → MoE 门控升级方向
- off-policy 反馈污染：report_outcome 更新的臂无论决策来自哪层 → RL 层偏好会污染 Thompson 统计（需 importance weighting 或分层独立统计）
- 漂移检测是启发式：相对误差对小基率敏感必误报、无统计检验、忽略 dodge/heal 维度 → CUSUM/Page-Hinkley 变化点检测升级
- P2→P3 的 HP<35% 直通几乎每场触发（phase2_hp_danger=0.35 太松）

### 10.3 学习亮点

1. 全链路可观测：每层命中计数 + summary() 一行进 HUD（mirror_debug_stats）——多层系统从不黑盒
2. 反事实奖励塑形（mirror_reward :449-464）：damage_dealt×0.5 − taken×0.3 + 画像兑现奖金——bandit 学的是"针对该玩家的策略质量"而非泛泛输出
3. 漂移驱动门槛自适应："数据新鲜度调制模型信任度"的模式值得借鉴

---

# 第四篇 工程基础

## 11. 确定性游戏与对拍验证

**三支柱**：
1. CountingRng（mt19937 + 掷骰计数，combat_system.h:22-28）
2. 种子公式 seed_start + run×1234567（sim_runner.h:106）
3. Replay hash 链（compute_state_hash 逐帧混入玩家/怪物状态，黄金比例常数 mixer）

**对拍验证**：3 种子 × 20 局 × 2 批逐字节一致（130 万行级）。历史修复：三处指针键 → instance_id/uint64（Q3.14——堆地址复用导致跨进程分叉）。

SimRunner 隐藏亮点：all-builds 轮换时 _current_run 归零（sim_runner.cpp:62-64）——每种 Build 重放完全相同种子序列，即**公共随机数（CRN）方差削减**，对比实验的统计学正确姿势。

已知尾巴：EnemyStats.kills 无写入点（威胁报告恒空）；finalize N=0 除零风险；avg_turns 实为帧数（命名误导）。

**为什么重要**：AI 研究的可复现性基础设施。你的对拍流程（同种子双进程逐字节 diff）就是定位非确定性 bug 的终极手段。

---

## 12. 教科书错误集合专题（六道活教材题）

你项目里的真实 bug 恰好覆盖了 AI 课程最核心的六个考点。每题按"症状→诊断→原理→修复"组织，建议动手修一遍胜过读十遍书。

| # | 错误 | 位置 | 考点 |
|---|------|------|------|
| ③ | C=√2 配 ±1000 奖励 → UCT 退化为贪心 | mcts_node.h:20 + combat_evaluator.cpp:6 | MCTS 探索项的量纲前提 |
| ④ | update() 缺 done → 终局自举污染 | q_agent.cpp:77-84 | Bellman target 的终止处理 |
| ⑤ | 学习率恒 0.1 不衰减 | rl_runner.cpp:68,150 | 随机逼近收敛条件 Σα²<∞ |
| ⑥ | 击杀奖励按尸体存量发 | environment.cpp:29 vs :101 | reward 设计与回报结构 |
| ⑧ | 跨局先验爆炸 → 探索归零 | online_adaptive_policy.cpp:67-78 | 非平稳环境的后验遗忘 |
| ② | can_use(0) 时间语义错位 | bt_agent.cpp:56 | 状态查询的时间参数传递 |

附加三题：⑦ A* 零边界校验 vs BFS 防御矩阵（防御标准统一性）、⑨ 压线置信度隐式耦合（魔法数关系化）、⑩ 在线符号坍缩（train/serve skew）。

---

## 13. 修复优先级路线图

**第一批（正确性，半天）**
1. BT P0-1：Sequence{cond, actConfirm/actDescend} 挂接进树
2. BT P0-2：can_use(_game_time)
3. RL B1：update() 加 done 参数
4. Mirror MP1：import/update 加指数遗忘 λ≈0.99
5. MCTS A6：快照读真实剩余冷却

**第二批（算法质量，一天）**
6. MCTS A1：奖励 sigmoid 归一化 [0,1]
7. RL B2：学习率按访问次数衰减
8. RL B3：击杀奖励改增量式（prev_alive 已算好没用上）
9. DA P0-3：attack 加逼近收益斜坡消死区
10. DA P1-2：治疗循环加 break

**第三批（性能与结构）**
11. DA P1-5：BFS 提升到 tick 层每帧一次（省 ~75% 导航开销）
12. MCTS：单子扩展 + 树复用
13. 画像：激活 hp 字段 + 修 retreat_rate/fight_back_rate

**第四批（升级实验，学习导向）**
14. Thompson → LinTS（contextual bandit）
15. n-gram → Katz backoff 插值平滑
16. 仲裁链 → MoE 门控软融合
17. ~~Q 表 → DQN（下一章）~~ **G32 已实测完毕，负收益，见第 14 章开头**
18. MCTS 评估函数 → 神经网络（AlphaGo 路线）—— **建议先别做**，理由同上

### 第 17 项的实测结论（2026-10-08）

「Q 表 → DQN」已完整实施并评测：
- 玩具 env 下 DQN 贪心 **36.7%**，规则策略 **60.0%**，PPO **18.9%**
- 有效的改进：课程学习（3 档难度）、Double DQN（治 max 过估计）、
  Observation 8→14 维（补位置与冷却维度）—— 但都不足以逆转
- **不做的理由**：环境动作空间高度退化（10 个动作只有「普攻/技能/等冷却」
  有意义），最优策略接近确定性规则（"冷却好了就打，否则等"），学习算法
  需要百万级样本才能逼近规则代码的 10 行

**若将来要做真正的神经网络 AI，前置条件是先做真实游戏环境的 Python 镜像**
（含命中判定、Buff 触发、走位收益），而不是继续调 `CombatEnvironment`。

---

## 14. 升级实验路线：DQN 完整实现

> ⚠️ **本章已实测完毕，结论是「不要这么做」。**
> 下方的代码写法本身正确、教学价值高（网络定义 / 经验回放 / target net /
> done 处理都是标准做法），但作为**本项目的升级方向已被否决**：
> 实测 DQN 36.7% < 规则策略 60.0%（PPO 18.9%）。
> 本章保留作教学材料，**不要照此改造项目**。
> 完整结论（含被证伪的假设、踩坑记录、重开此线的前置条件）见
> `docs/AI_RL_ROUTE_CLOSEOUT.md`。

把 Q 表换成神经网络——改动最小、学习价值最高的升级。

### Step 1 网络定义

```python
import torch, torch.nn as nn

class DQN(nn.Module):
    def __init__(self, state_dim=8, action_dim=9):   # 用 to_vector 的 8 维!
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(state_dim, 64), nn.ReLU(),
            nn.Linear(64, 64), nn.ReLU(),
            nn.Linear(64, action_dim))
    def forward(self, x): return self.net(x)
```

### Step 2 经验回放

```python
from collections import deque
import random

class ReplayBuffer:
    def __init__(self, capacity=10000):
        self.buffer = deque(maxlen=capacity)
    def push(self, *transition):
        self.buffer.append(transition)
    def sample(self, batch=32):
        return random.sample(self.buffer, batch)
    def __len__(self): return len(self.buffer)
```

### Step 3 训练循环（含 done 处理——第⑤题的正确答案）

```python
def compute_loss(q_net, target_net, batch, gamma=0.9):
    s, a, r, s2, d = map(list, zip(*batch))
    s  = torch.FloatTensor(s);  a = torch.LongTensor(a)
    r  = torch.FloatTensor(r);  s2 = torch.FloatTensor(s2)
    d  = torch.BoolTensor(d)

    qsa = q_net(s).gather(1, a.unsqueeze(1)).squeeze()
    with torch.no_grad():
        max_next = target_net(s2).max(1).values
        # ★ done 时 target 只等于 r —— 你项目缺的就是这一步 (活教材④)
        target = r + gamma * max_next * (~d)

    return ((qsa - target) ** 2).mean()

q_net, target_net, buffer = DQN(), DQN(), ReplayBuffer()
target_net.load_state_dict(q_net.state_dict())
opt = torch.optim.Adam(q_net.parameters(), lr=1e-3)   # α 衰减问题交给 Adam (活教材⑤)

for ep in range(1000):
    state, done = env.reset(), False                  # env = CombatEnvironment 的 Python 移植
    while not done:
        if random.random() < epsilon:
            action = random.randrange(9)
        else:
            with torch.no_grad():
                action = q_net(torch.FloatTensor(state)).argmax().item()
        next_state, reward, done = env.step(action)   # 击杀奖励改增量式 (活教材⑥)
        buffer.push(state, action, reward, next_state, done)
        state = next_state
        if len(buffer) > 200:
            loss = compute_loss(q_net, target_net, buffer.sample())
            opt.zero_grad(); loss.backward(); opt.step()
    if ep % 10 == 0:
        target_net.load_state_dict(q_net.state_dict()) # 目标网络周期同步
```

### Step 4 部署回 C++（可选）

```python
torch.onnx.export(q_net, torch.zeros(1, 8), "dqn.onnx")
```
C++ 侧在 `set_ml_predictor` 插槽接入 ONNX Runtime——MirrorAgent 的 L0 层预留接口正好派上用场。

### 训练效率要点

- self-play：单网络 + style one-hot 条件化输入（4 风格共享表征），替代现在的 4 张独立表串行训练
- 对手池（league）：从 player_behavior_analyzer 真实数据流采样 profile，替代固定 4 档脚本——消除 B7 过拟合
- 部署侧解耦：mirror_agent.cpp:16-21 的硬编码适配字段（player_attack=10 等）必须改为真实填充，否则 train/serve skew 翻车

---

## 15. 参考文献与资源

### 经典论文（按本手册章节对应）

| 技术 | 论文 | 年份 |
|------|------|------|
| UCT-MCTS（第4章③的出处） | Kocsis & Szepesvári, "Bandit based Monte-Carlo Planning" | 2006 |
| AlphaGo | Silver et al., "Mastering the game of Go..." Nature | 2016 |
| DQN | Mnih et al., "Playing Atari with Deep RL" | 2015 |
| Thompson（第7章） | Thompson, "On the Likelihood that One Unknown Probability Exceeds Another..." | 1933 |
| MT Gamma 采样（你的实现出处） | Marsaglia & Tsang, "A Simple Method for Generating Gamma Variables" | 2000 |
| 行为克隆/IRL | Ng & Russell, "Algorithms for Inverse Reinforcement Learning" | 2000 |
| DAgger | Ross et al., "A Reduction of Imitation Learning...to No-Regret Online Learning" | 2011 |
| GAIL | Ho & Ermon, "Generative Adversarial Imitation Learning" | 2016 |
| A* | Hart et al., "A Formal Basis for the Heuristic Determination of Minimum Cost Paths" | 1968 |

### 课程与书

- Sutton & Barto《Reinforcement Learning: An Introduction》— RL 圣经（第⑤⑥题的理论依据在第 2 章）
- David Silver RL Course (UCL/DeepMind) — 视频课，MCTS 与 Q-Learning 两讲直接对应第 4/5 章
- OpenAI Spinning Up — 深度 RL 实践教程（DQN 到 PPO）
- Millington《AI for Games》— 行为树与游戏 AI 工业实践

### 开源库

stable-baselines3（RL 算法参考实现）/ gymnasium（环境接口规范，你的 CombatEnvironment 已符合）/ d3rlpy（离线 RL）

---

> **使用建议**：先做第 13 章第一二批修复（全是真 bug，每修一个对照第 12 章理解原理），再挑第四批一项升级实验。修完第一批后重跑 `--sim 500 --sim-seed 7` 对比胜率变化——你会亲眼看到教科书错误的实际影响量级。
>
> 本手册基于 v1.0.0（commit ef75296+）源码逐行审查生成；技术全景部分继承自初版学习指南，代码事实以源码审查为准。

