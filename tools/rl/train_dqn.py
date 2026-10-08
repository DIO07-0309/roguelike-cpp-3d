# -*- coding: utf-8 -*-
"""DQN 训练脚本 — 用 Python 版 CombatEnvironment 训练, 导出 dqn.onnx。

    python tools/rl/train_dqn.py --episodes 2000 --out tools/rl/dqn.onnx

场景多样化: 复用 parity_dump 的 6 变体思路, 覆盖贴身/中距/网格 4 条边,
否则学出来的策略只会应对一种局面 (这是 G31-B4 的教训: 固定场景训出废策略)。

合法性: mask 既是输入 (第 9-18 维) 也是 argmax 约束, 两处来自同一次
get_possible_actions 计算, 不会出现"喂进去的合法性和实际约束不一致"。

导出: torch.onnx.export 固定 batch=1, 输出 10 维 Q 值。C++ 侧推理时
必须自己做 argmax + mask, 网络不负责选动作。
"""
import argparse
import os
import sys
import time

import numpy as np
import torch
import torch.nn as nn

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import combat_env_py
from action_mask import (INPUT_DIM, OUTPUT_DIM, legal_mask, masked_argmax,
                         masked_random, build_input)
from combat_env_py import CombatEnvironment, SimulationState, MonsterSnapshot, WAIT
from dqn_model import DQN, ReplayBuffer

GAMMA = 0.99
LR = 1e-3
BATCH = 64
REPLAY_START = 1000
TARGET_SYNC = 200
EPS_START = 1.0
EPS_END = 0.05
#★ REAL_MODE 用更长的 episode: 1 步 ≈ 0.25 秒, 300 步 ≈ 75 秒游戏时间。
#  真实 Boss 战是这个量级; 而 60 步(15 秒)装不下任何高 HP 目标 ——
#  Boss pdef=10 时每击仅 3 点伤, 800 HP 需 228 步, 60 步预算下
#  所有场景都超时必败(实测胜率 1.6%)。
MAX_STEPS_TOY = 60
MAX_STEPS_REAL = 300
GRAD_CLIP = 10.0

# ★ reward 缩放 —— 不是调参偏好, 是必需项。
# C++ 的 reward 量级: 击杀 +50、胜利 +200/-200、受击 -(1..12)。
# 未缩放时Q 值目标量级 ≈ 296, MSE loss 实测 572 且下降极慢;
# 网络输出层要从 0 学到几百, 小初始化权重下梯度被巨大 target 淹没,
# 实测胜率卡在 66% (随机 59.6%) 上不去。
# 缩到 ~1.0 量级后 loss 降到个位数, 同一段时间内可学的有效步数提升一个数量级。
# 注意: 缩放只影响训练目标的数值尺度, 不改变最优策略 (正仿射变换)。
REWARD_SCALE = 0.01

# ── REAL 模式的额外奖励项 (修"策略锁死单一技能") ──
# 症状: REAL_MODE 下 skill_2 占 74%、attack 0%。根因是三条结构性优势叠加:
#   1. 技能射程 5.0 > 普攻 2.0  → 不用靠近就能输出
#   2. 技能伤害 1.5x + AoE     → 一次清一排
#   3. 冷却中只能 WAIT 且无代价 → 放完技能就等, 等待不扣分
# 于���最优解永远是"放技能", 其他动作 Q 值永远得不到梯度。
# 修法: 给"什么都没做"加惩罚, 让普攻(唯一能稳定输出且不被冷却锁死的动作)
# 在长线回报上占优。权重经网格搜索确定 (见 README G32 记录)。
IDLE_PENALTY = 0.06# 每次 WAIT 的惩罚 (原始reward 单位, 乘REWARD_SCALE 后入网)

# 训练期loss 采样缓冲 (train_episode 内无 return 写权限, 用模块级 sink)
losses_sink: list = []


def place_monsters(s, variant, rng):
    """摆位必须让**两只怪都可被交战**, 否则 victory 不可达。

    第一版把 orc 放在 p.x+2 / (7,5): 技能范围 d<3 勉强够到, 普攻 d<2 永远够不到,
    于是"走向 orc"既无收益又有代价, 而 victory 要求全员死亡 → 场景必输。
    症状: DQN 胜率 12.7% 低于随机 58.3%, 且 loss 单调上升 (在拟合噪声)。

    现在的规则: slime 放 d=1 (可普攻), orc 放 d=2 (仅技能, 促使玩家用技能
    或调整距离), 两者都在可交战距离内。variant 只改玩家的位置/朝向边界。
    """
    p = s.player
    m0, m1 = s.monsters
    if variant == 0:        # 正常位: slime 贴身, orc 侧向 2 格
        m0.x, m0.y = p.x, p.y + 1.0
        m1.x, m1.y = p.x + 2.0, p.y
    elif variant == 1:      # orc 贴身, slime 侧向 —— 交换角色, 防止学偏
        m1.x, m1.y = p.x, p.y + 1.0
        m0.x, m0.y = p.x + 2.0, p.y
    elif variant == 2:      # 双怪同侧不同行: 都在 d=2, 只能靠技能
        m0.x, m0.y = p.x + 2.0, p.y
        m1.x, m1.y = p.x + 2.0, p.y + 1.0
    elif variant == 3:      # 左上角
        p.x, p.y = 0.0, 0.0
        m0.x, m0.y = 1.0, 0.0
        m1.x, m1.y = 2.0, 0.0
    elif variant == 4:      # 右下角
        p.x, p.y = 39.0, 29.0
        m0.x, m0.y = 38.0, 29.0
        m1.x, m1.y = 37.0, 29.0
    else:                   # 右上角 (覆盖 x 上界 + y 上界的组合)
        p.x, p.y = 39.0, 0.0
        m0.x, m0.y = 38.0, 0.0
        m1.x, m1.y = 37.0, 0.0
    # 扰动幅度 ±1 格, 制造 d=1 与 d=2 的混合, 但不破坏"可交战"前提
    m0.x = float(np.clip(m0.x + rng.integers(-1, 2), 0, 39))
    m0.y = float(np.clip(m0.y + rng.integers(-1, 2), 0, 29))
    m1.x = float(np.clip(m1.x + rng.integers(-1, 2), 0, 39))
    m1.y = float(np.clip(m1.y + rng.integers(-1, 2), 0, 29))


def make_scenario(variant: int, rng: np.random.Generator, level: int = 2):
    """REAL_MODE 真实场景 + 课程学习难度档

    ★ level = 课程学习档位: 0 入门 / 1 进阶 / 2 完整
      背景: 实测 DQN 在 level2 学出 wait 87.5%、胜率 10.8%, 而固定策略
      95% —— 说明它根本没学会基本动作。根因是 300 步 episode 里探索成本
      过高: 一次随机探索很可能直接送命, 有效梯度信号被淹没。
      故拆成三档递进: level0 低 HP 低攻贴身(只需学会"贴上去打")
      → level1 中 HP 中距(学会走位与技能) → level2 完整 Boss 场景。
    """
    s = SimulationState()
    p = s.player
    p.hp, p.max_hp = 100.0, 100.0
    p.attack, p.pdef, p.mdef, p.alive = 10, 3, 1, True
    p.x, p.y = 5.0, 5.0

    if combat_env_py.REAL_MODE:
        return _make_real_scenario(variant, rng, s, p, level)
    return _make_toy_scenario(variant, rng, s, p)


def curriculum_params(level: int):
    """难度档 → (hp_mul, atk_mul, dist_add, step_budget)

    step_budget 随难度递增: 入门 60 步足够打完, 让探索的失败代价小;
    完整场景 300 步。budget 随 level 变, 故 max_steps() 需接受 level 参数。
    """
    if level == 0:
        return 0.30, 0.45, 0, 60
    if level == 1:
        return 0.60, 0.75, 2, 140
    return 1.0, 1.0, 0, MAX_STEPS_REAL


def _make_toy_scenario(variant, rng, s, p):
    """玩具模式 — 与 parity 契约一致, 2 只固定小怪"""
    s.monsters = [
        MonsterSnapshot("slime", 20, 20, 5, 6, 3, 1, 0, [], True, False),
        MonsterSnapshot("orc", 30, 30, 7, 5, 7, 3, 1, [], True, False),
    ]
    place_monsters(s, variant, rng)
    p.hp = p.max_hp = float(rng.integers(70, 131))
    p.attack = int(rng.integers(7, 15))
    s.monsters[0].max_hp = float(rng.integers(15, 26))
    s.monsters[0].hp = s.monsters[0].max_hp
    s.monsters[0].attack = int(rng.integers(2, 7))
    s.monsters[1].max_hp = float(rng.integers(20, 36))
    s.monsters[1].hp = s.monsters[1].max_hp
    s.monsters[1].attack = int(rng.integers(4, 10))
    s.player_style = int(rng.integers(0, 5))
    return s


def _make_real_scenario(variant, rng, s, p, level: int = 2):
    """REAL_MODE — 6 类真实配置, 覆盖单 Boss / 群怪 / 边界

    ★★数值配平 (用算术推导, 不是拍脑袋):
      玩家普攻每击 = 8.5 - int(boss_pdef*0.5); 冷却 3.4 步
      玩家受击每步期望 = (boss_atk - int(p.pdef*0.5)) * 0.4  (d<1.5 时)
      要求: 玩家存活步数 ≈ 打死 Boss 所需步数的 1.5~2 倍 (留操作空间)

      旧配平 (boss HP300/pdef5/atk25) 实测: 玩家 14 步死, Boss 需 291 步
      → 必死, 胜率 5%。根因是三处同时失衡: Boss 血量按真实 800+ 给,
      但 attack 也按真实 25 给, 而玩具 env 没有减伤/格挡/走位收益。
      现按可玩性反推: 玩家能撑 ~150 步, Boss 需 ~90 步打死。
    """
    hp_mul, atk_mul, dist_add, _ = curriculum_params(level)

    # ★ 位置随机化 — 破除"记住 boss 固定在某处"的捷径。
    #   诊断: 玩家固定 (5,5) 只改 boss 位置, Q 值确实随之变化
    #   (0.072~0.254, 说明网络用到了位置维度), 但 argmax 恒为 SK3。
    #   原因不是位置记忆, 而是 4 个技能槽同质(见下) —— 但仍需随机化,
    #   否则网络可能走"固定走位脚本"的捷径而非学会空间推理。
    px, py = float(rng.integers(2, 38)), float(rng.integers(2, 28))
    p.x, p.y = px, py

    def mk(name, hp, atk, pdef, dx, dy, boss=False):
        # 相对玩家放置时加随机抖动 (±2 格), 避免绝对位置固定
        jx = int(rng.integers(-2, 3))
        jy = int(rng.integers(-2, 3))
        mx = float(np.clip(px + dx + jx, 0, 39))
        my = float(np.clip(py + dy + jy, 0, 29))
        return MonsterSnapshot(name, hp * hp_mul, hp * hp_mul,
                               mx, my,
                               max(1, int(atk * atk_mul * rng.uniform(0.85, 1.15))),
                               max(0, int(pdef + rng.integers(-1, 2))),
                               0, [], True, boss)

    # Boss: HP 110 / pdef 4 / atk 7
    #   玩家每击 8.5-2 = 6 点 → 110 HP 需 ~62 步(每 4 步一击)
    #   玩家受击期望 (7-3)*0.4 = 1.6/步 → 190 HP 撑 ~118 步
    #   余量 ~1.9 倍: 够学"贴身磨血", 也留了走位容错
    #   (先前 HP150/atk7 时余量仅 1.2 倍, 纯普攻策略 0% 胜率)
    if variant == 0:
        # 单 Boss 贴身 (真实最重要场景: AoE 无优势 → 必然靠普攻)
        s.monsters = [mk("boss", 110.0, 7, 4, 0, 1, True)]
    elif variant == 1:
        # 单 Boss 中距 (需先靠近才能普攻, 或用远程技能)
        s.monsters = [mk("boss", 130.0, 8, 4, 4, 0, True)]
    elif variant == 2:
        # 群怪 3 只 (AoE 有意义 → 应偏向技能)
        s.monsters = [mk("slime", 40.0, 4, 2, 1, 0),
                      mk("slime", 40.0, 4, 2, 0, 1),
                      mk("orc", 70.0, 6, 3, 2, 0)]
    elif variant == 3:
        # 单只精英 (中 HP, AoE 无优势)
        s.monsters = [mk("elite", 120.0, 7, 4, 0, 1)]
    elif variant == 4:
        # 群怪 4 只密集 (AoE 最优场景)
        s.monsters = [mk("slime", 35.0, 4, 2, 1, 0),
                      mk("slime", 35.0, 4, 2, 0, 1),
                      mk("orc", 60.0, 6, 3, 1, 1),
                      mk("slime", 35.0, 4, 2, 2, 0)]
    else:
        # 左上角落单怪 (覆盖 x/y 下界; Boss 远处)
        m = mk("boss", 110.0, 7, 4, 2, 0, True)
        m.x, m.y = float(np.clip(2 + rng.integers(-1, 3), 0, 39)), 0.0
        s.monsters = [m]

    # ★ 难度档 0 只保留贴身单怪 —— 课程学习的第一课就是"贴上去打"
    if level == 0:
        s.monsters = [s.monsters[0]]
    for m in s.monsters:
        m.max_hp = m.hp
    return s


def compute_loss(q_net, target_net, batch, double_dqn=True):
    """done 时 target 只等于 reward — 终局不得自举未来价值 (AI_LEARNING_GUIDE 第⑤题)

    ★ bootstrap 必须过 mask —— 这正是 G31-B1 在 tabular Q 上的同一个 bug:
      max_a' Q(s',a') 若不限制在合法动作内, 会把"当前不可选的动作"的价值
      当成未来价值上界, 系统性高估 target 并把噪声灌进梯度。
      batch 每条携带 next_mask, 用 -inf 屏蔽非法动作的 Q。
    """
    s, a, r, s2, d, m2 = map(list, zip(*batch))
    s = torch.FloatTensor(np.asarray(s, dtype=np.float32))
    a = torch.LongTensor(a)
    r = torch.FloatTensor(np.asarray(r, dtype=np.float32))
    s2 = torch.FloatTensor(np.asarray(s2, dtype=np.float32))
    d = torch.BoolTensor(d)
    m2 = torch.FloatTensor(np.asarray(m2, dtype=np.float32))

    qsa = q_net(s).gather(1, a.unsqueeze(1)).squeeze(1)
    with torch.no_grad():
        next_q = target_net(s2)
        # 非法动作置 -inf。全非法行 (理论上不会发生, WAIT 恒合法) 用 0 兜底
        masked_q = torch.where(m2 > 0, next_q, torch.full_like(next_q, -1e9))
        max_next = masked_q.max(1).values
        # 全非法行修正: -1e9 会污染 target
        dead = m2.sum(1) == 0
        max_next = torch.where(dead, torch.zeros_like(max_next), max_next)
        target = r + GAMMA * max_next * (~d).float()

    td_error = qsa - target
    # ★ Double DQN — 缓解 max 操作导致的过估计。
    #   症状: 课程学习下"档内胜率随 epsilon 下降而降低"(L1 34%→33%,
    #   L2 18%→16%), 即利用阶段反而不如探索 —— 典型过估计表现。
    #   原因: 标准 DQN 的 target 用同一张 target_net 做"选动作"和"打分",
    #   max 会系统性高估。Double DQN 改用 online_net 选动作、
    #   target_net 打分, 切断这条高估通路。
    if double_dqn:
        with torch.no_grad():
            online_next = q_net(s2).argmax(1, keepdim=True)
            online_best = target_net(s2).gather(1, online_next).squeeze(1)
            # 在线网络也可能选中非法动作 → 用 next_mask 的 max 兜底
            online_best = torch.where(dead, torch.zeros_like(online_best),
                                      online_best)
            online_best = torch.where((~d), online_best,
                                     max_next)
            target = r + GAMMA * online_best * (~d).float()
        td_error = qsa - target
    return (td_error ** 2).mean(), qsa.detach(), target


def max_steps(level: int = 2) -> int:
    """episode 长度上限。

    REAL_MODE 下随课程档位变化 (60/140/300) —— 入门档用短 episode 让
    探索的失败代价小; 玩具模式固定 60 步 (维持 parity 契约)。
    """
    if not combat_env_py.REAL_MODE:
        return MAX_STEPS_TOY
    return curriculum_params(level)[3]


def train_episode(env, q_net, target_net, opt, buf, rng, epsilon, variant,
                  seed, level=2, grad_steps_per_env_step=1):
    """每个 env step 后做一次梯度更新 (标准 DQN)。

    早期版本是"一局只更新一次" — 60 局仅20 次梯度更新, 网络基本还是随机
    初始化, 而 epsilon 已退火到 0.05 → 确定性策略立刻塌缩到单一动作
    (通常是 WAIT, 因为未训练网络对所有动作输出近似常数, argmax 恒取第一个),
    表现为胜率 3.3%, 远低于随机策略的 58.3%。
    """
    limit = max_steps(level)
    initial = make_scenario(variant, rng, level)
    initial.seed = int(seed) & 0xFFFFFFFF
    obs = env.reset(initial)
    total_r, steps = 0.0, 0
    while not env.is_done() and steps < limit:
        mask = legal_mask(env)
        x = build_input(obs, mask)
        if rng.random() < epsilon:
            action = masked_random(rng, mask)
        else:
            with torch.no_grad():
                q = q_net(torch.FloatTensor(x).unsqueeze(0)).squeeze(0).numpy()
            action = masked_argmax(q, mask)
        sr = env.step(action)
        next_mask = legal_mask(env)
        next_x = build_input(sr.observation, next_mask)
        # ★ 空闲惩罚只在 REAL_MODE 生效: 玩具模式与 C++ parity 对齐,
        # 加了会破坏逐位一致。REAL_MODE 用来训真实战斗节奏的策略,
        # 没有 parity 约束 (它本就不与 C++ 环境逐位相同)。
        shaped = sr.reward
        if combat_env_py.REAL_MODE and not sr.terminal:
            # 时间成本已在 env 侧计入 reward; 这里额外对WAIT 加罚,
            # 因为 WAIT 不推进输出却消耗时间预算 (env 的每步成本对所有
            # 动作一视同仁, 但 WAIT 的收益恒为 0)
            if action == WAIT:
                shaped -= IDLE_PENALTY
        buf.push(x, action, shaped * REWARD_SCALE, next_x, sr.terminal,
                 next_mask)
        obs, total_r = sr.observation, total_r + shaped
        steps += 1

        if len(buf) >= REPLAY_START:
            for _ in range(grad_steps_per_env_step):
                loss, _, _ = compute_loss(q_net, target_net, buf.sample(BATCH))
                opt.zero_grad()
                loss.backward()
                nn.utils.clip_grad_norm_(q_net.parameters(), GRAD_CLIP)
                opt.step()
                losses_sink.append(float(loss))
    return total_r, steps


def train(episodes: int, out_path: str, seed: int = 42, real_mode: bool = False,
          curriculum: bool = False, verbose: bool = True):
    global losses_sink
    # REAL_MODE 开关必须在任何场景构造前设置 (make_scenario 会读冷却/射程)
    combat_env_py.REAL_MODE = real_mode
    torch.manual_seed(seed)
    rng = np.random.default_rng(seed)
    q_net, target_net = DQN(), DQN()
    target_net.load_state_dict(q_net.state_dict())
    opt = torch.optim.Adam(q_net.parameters(), lr=LR)
    buf = ReplayBuffer()
    env = CombatEnvironment()

    wins, tail_wins = 0, 0
    tail_start = int(episodes * 0.9)
    losses_sink = []
    t0 = time.time()

    # ★ 课程学习: 把总预算切成三段, 每段一档难度, 阶段末重置 epsilon
    #   以便在更难的任务上重新探索。关键点: 上一档的 Q 值不能直接用
    #   (状态分布已变), 故每档重置 epsilon 但保留网络与 replay buffer。
    stages = ([(0, episodes // 3), (1, episodes // 3), (2, episodes - 2 * episodes // 3)]
              if curriculum else [(2, episodes)])

    ep_cursor = 0
    for level, stage_len in stages:
        if stage_len <= 0:
            continue
        stage_win = 0
        for i in range(stage_len):
            frac = i / max(1, stage_len - 1)
            epsilon = EPS_START + (EPS_END - EPS_START) * frac
            variant = ep_cursor % 6
            train_episode(env, q_net, target_net, opt, buf, rng,
                          epsilon, variant, seed + ep_cursor * 7919, level)
            won = env.state().victory
            wins += won
            stage_win += won
            ep_cursor += 1
            if ep_cursor >= tail_start and won:
                tail_wins += 1
            if (ep_cursor + 1) % TARGET_SYNC == 0:
                target_net.load_state_dict(q_net.state_dict())
            if verbose and (ep_cursor + 1) % max(1, stage_len // 4) == 0:
                avg = float(np.mean(losses_sink[-2000:])) if losses_sink else 0.0
                print(f"  L{level} [{ep_cursor+1}/{episodes}] eps={epsilon:.3f} "
                      f"win={wins}/{ep_cursor+1} ({wins*100.0/(ep_cursor+1):.1f}%) "
                      f"档内={stage_win}/{i+1} loss={avg:.3f} "
                      f"grads={len(losses_sink)} [{time.time()-t0:.0f}s]")
        if verbose and curriculum:
            print(f"  == 难度档 {level} 完成: 档内胜率 "
                  f"{stage_win*100.0/max(1,stage_len):.1f}% ==")

    tail_n = episodes - tail_start
    mode = "REAL" if real_mode else "TOY"
    cur = " + 课程" if curriculum else ""
    print(f"\n[{mode}{cur} 模式] 训练完成: {wins}/{episodes} 胜 "
          f"({wins*100.0/episodes:.1f}%), "
          f"末段 {tail_wins}/{tail_n} ({tail_wins*100.0/max(1,tail_n):.1f}%)")

    q_net.eval()
    dummy = torch.zeros(1, INPUT_DIM, dtype=torch.float32)
    torch.onnx.export(q_net, dummy, out_path,
                      input_names=["state"], output_names=["q_values"],
                      dynamic_axes=None, opset_version=13, dynamo=False)
    print(f"导出: {out_path}")
    torch.save(q_net.state_dict(), out_path.replace(".onnx", ".pt"))
    print(f"保存: {out_path.replace('.onnx', '.pt')}")
    return wins / max(1, episodes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--episodes", type=int, default=2000)
    ap.add_argument("--out", default="tools/rl/dqn.onnx")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--real-mode", action="store_true",
                    help="用 resources/skills.json 的真实冷却/射程训练 "
                         "(技能冷却 2/5/8/20s 而非统一 3.0)")
    ap.add_argument("--curriculum", action="store_true",
                    help="课程学习: 分三档难度递进 (入门/进阶/完整)")
    args = ap.parse_args()
    win_rate = train(args.episodes, args.out, args.seed, args.real_mode,
                     args.curriculum)
    return 0 if win_rate > 0.5 else 1


if __name__ == "__main__":
    sys.exit(main())
