# -*- coding: utf-8 -*-
"""CombatEnvironment — CombatEnvironment (ai/rl/environment.cpp) 的 Python 逐行移植。

移植契约 (parity contract):
  1. RNG: uint32 LCG, 每步之后必须 & 0xFFFFFFFF (numpy/Python 整数默认无限精度)
  2. 浮点: 全部走 float32, 与 C++ 的 float 对齐
  3. 动作语义/冷却常量/边界 40x30 全部照抄, 改一处即视为破坏 parity
测试: tests/rl/parity_test.py (同 seed 双跑逐位比对)
"""
from dataclasses import dataclass, field
import numpy as np

F32 = np.float32

# ── 动作空间 (对齐 mcts::CombatAction, COUNT == 10) ──
# 枚举顺序必须与 action.h:10-16 完全一致, 索引错位会让所有 Q 值张冠李戴
ATTACK = 0
SKILL_1, SKILL_2, SKILL_3, SKILL_4 = 1, 2, 3, 4
MOVE_UP, MOVE_DOWN, MOVE_LEFT, MOVE_RIGHT = 5, 6, 7, 8
WAIT = 9
ACTION_COUNT = 10

# parity_dump.cpp 里的动作选择 LCG 与 action.h 是两套独立常数, 不可混用
DUMP_LCG_MUL = 1664525
DUMP_LCG_ADD = 1013904223

GRID_W, GRID_H = 40, 30          # action.h:66 的边界
ATTACK_RANGE = 2.0               # get_possible_actions: d < 2.0f
SKILL_RANGE = 3.0                # step(): d < 3.0f
COOLDOWN_STEP = F32(0.25)
ATTACK_COOLDOWN_SET = F32(0.5)
SKILL_COOLDOWN_SET = F32(3.0)

# ── REAL_MODE 的伤害奖励与时间成本 ──
# 现象: DQN 学出"一直往下走逃跑"(move_down 82%), 贪心胜率 0%, 而站着
# 打能赢 79%。逐帧追踪显示: 逃跑时每步只扣 REAL_TIME_COST(0.15), 而站着
# 打每步挨打扣 4+ —— 奖励函数让"逃跑"严格优于"战斗"。
# 根因: 负奖励(挨打 + 时间)与正奖励(击杀/胜利)量级失衡, 且挨打只发生在
# 近身时, 于是"离开近身"成了最优解。
# 修法: ① 每点实际造成的伤害给正奖励, 使"输出"本身有价值, 与"挨打"
#       对称; ② 移除无条件的时间成本(它鼓励"尽快结束"而"结束"的方式
#       是逃跑而非胜利)。改由胜负 + 效率隐含表达。
REAL_DAMAGE_REWARD = 0.5      # 每点实际伤害的奖励 (0.5 × 6 点/击 = 3/击)
REAL_TIME_COST = 0.0          # 置0: 见上方说明

# ── 真实战斗参数模式 (REAL_MODE) ──
# 起因: 玩具 env 里普攻与技能共用同一个虚拟 attack 值, 技能硬编码 1.5x 系数,
# 于是技能客观上碾压普攻, DQN 学出 "attack 占比 0%"。而真实数据恰好相反。
#
# 真实参数全部取自 resources/*.json (见 tools/rl/real_stats.py):
#   weapons.json  21 把武器 base_damage 4~13 (剑/弩均值 8.5),
#                 base_range 1~12, combo_timeout 0.8~0.95s
#                 → 最优普攻 DPS = 8.5 / 0.85 = 10.0
#   skills.json   16 个主动技能冷却 1.5~20.0s (中位数 7.0s)
#                 → 即便单次伤害等于普攻一击, 技能 DPS 也只有 0.4~5.7
# 结论: 真实游戏里**普攻是 DPS 主力, 技能是爆发补充**, 与我先前拍脑袋的
# "技能 1.5x" 恰好相反。故 REAL_MODE 下技能不再有伤害加成, 靠 AoE 与
# Buff 触发取胜; 普攻按真实武器伤害 + combo_timeout 计算。
#
# REAL_MODE 会**破坏 parity** (与 C++ CombatEnvironment 不再逐位一致),
# 默认关闭, 仅训练真实策略时开。
#
# 时基: 每"步"推进 COOLDOWN_STEP(=0.25) 冷却单位; 1 步 ≈ 0.25 秒。
REAL_MODE = False
SECONDS_PER_STEP = 0.25
TICKS_PER_SECOND = 4.0

# 普攻: 取真实武器均值(剑/弩 8.5), combo_timeout 0.85s → 3.4 步/击
REAL_ATTACK_BASE_DAMAGE = 8.5
REAL_ATTACK_COMBO_STEPS = 3.4# 连续攻击间隔 (0.85 秒)
REAL_ATTACK_RANGE = 2.5# 剑的 base_range (真实最短的通用近战武器)
# 普攻无伤害系数(就是武器伤害本身); 保留该变量以便按稀有度微调
REAL_ATTACK_DAMAGE_MULT = 1.0

# 技能: 真实冷却秒数 (slash 2.0 / fireball 5.0 / self_heal 8.0 / the_world 20.0)
REAL_SKILL_COOLDOWNS = (2.0, 5.0, 8.0, 20.0)
# ★ 技能伤害不再有 1.5x 加成 —— 真实数据里同伤害下技能 DPS 远低于普攻,
#   加成只会让 DQN 彻底锁死技能。技能的价值在 AoE(命中多目标) 与 Buff。
REAL_SKILL_DAMAGE_MULT = (2.2, 1.6, 0.5, 4.0)   # 见 _skill_damage_mult 注释
REAL_SKILL_RANGE = 5.0# 火球等远程技能的真实射程量级


def _attack_range() -> np.float32:
    return F32(REAL_ATTACK_RANGE) if REAL_MODE else F32(ATTACK_RANGE)


def _attack_damage() -> np.float32:
    """普攻基础伤害 —— REAL_MODE 用真实武器均值"""
    if not REAL_MODE:
        return F32(1.0)                 # 玩具模式用 p.attack, 见调用处
    return F32(REAL_ATTACK_BASE_DAMAGE * REAL_ATTACK_DAMAGE_MULT)


def _attack_cooldown_set() -> np.float32:
    """普攻冷却 (冷却单位)"""
    if not REAL_MODE:
        return ATTACK_COOLDOWN_SET
    return F32(REAL_ATTACK_COMBO_STEPS * SECONDS_PER_STEP)


def _skill_cooldown_set(si: int) -> np.float32:
    """把"持续秒数"换算成本 env 的冷却单位"""
    if not REAL_MODE:
        return SKILL_COOLDOWN_SET
    sec = REAL_SKILL_COOLDOWNS[si % len(REAL_SKILL_COOLDOWNS)]
    return F32(sec / SECONDS_PER_STEP)


def _skill_damage_mult(si: int = 0) -> np.float32:
    """★ 4 个技能槽必须真正可区分, 否则网络无法学习。

    诊断: 原本 4 槽同质(同伤害/同 AoE/同射程), 只有冷却不同。实测
    无论 boss 在哪个方向, argmax **恒为 SK3** —— SK3(8 秒=128 步冷却)
    在一个 episode 内只会用 1 次, 梯度信号极稀疏, 网络学不到冷却代价。
    现按真实定位分化(对应 skills.json 的 slash/fireball/self_heal/the_world):
      slot0 斩击: 2.2x近战主力, 冷却 2 秒
      slot1 火球: 1.6x 远程 AoE, 冷却 5 秒
      slot2 治疗: 0.5x (env 无治疗机制, 按低伤处理 = 不值得用)
      slot3 时停: 4.0x 终极技, 冷却 20 秒
    """
    if not REAL_MODE:
        return F32(1.5)
    return F32(REAL_SKILL_DAMAGE_MULT[si % len(REAL_SKILL_DAMAGE_MULT)])


def _skill_range() -> np.float32:
    return F32(REAL_SKILL_RANGE) if REAL_MODE else F32(SKILL_RANGE)


def clamp_u32(x: int) -> int:
    return x & 0xFFFFFFFF


@dataclass
class MonsterSnapshot:
    type: str = ""
    hp: float = 20.0
    max_hp: float = 20.0
    x: float = 0.0
    y: float = 0.0
    attack: int = 4
    pdef: int = 1
    mdef: int = 0
    buffs: list = field(default_factory=list)
    alive: bool = True
    is_boss: bool = False


@dataclass
class PlayerSnapshot:
    hp: float = 100.0
    max_hp: float = 100.0
    x: float = 0.0
    y: float = 0.0
    attack: int = 10
    pdef: int = 3
    mdef: int = 1
    buffs: list = field(default_factory=list)
    attack_cooldown: float = 0.0
    skill_cooldowns: list = field(default_factory=lambda: [0.0] * 4)
    alive: bool = True


@dataclass
class SimulationState:
    player: PlayerSnapshot = field(default_factory=PlayerSnapshot)
    monsters: list = field(default_factory=list)
    seed: int = 0
    calls: int = 0
    depth: int = 0
    terminal: bool = False
    victory: bool = False
    player_style: int = 0

    def is_terminal(self) -> bool:
        return self.terminal or not self.player.alive

    def clone(self) -> "SimulationState":
        c = SimulationState()
        c.player = PlayerSnapshot(**{**self.player.__dict__,
                                    "buffs": list(self.player.buffs),
                                    "skill_cooldowns": list(self.player.skill_cooldowns)})
        c.monsters = [MonsterSnapshot(**{**m.__dict__, "buffs": list(m.buffs)})
                      for m in self.monsters]
        c.seed, c.calls, c.depth = self.seed, self.calls, self.depth
        c.terminal, c.victory, c.player_style = self.terminal, self.victory, self.player_style
        return c


@dataclass
class StepResult:
    observation: np.ndarray
    reward: float = 0.0
    terminal: bool = False


class CombatEnvironment:
    """Gym 风格接口: reset(initial) → step(action) → StepResult"""

    def __init__(self):
        self._state = SimulationState()
        self._rng_seed = 42

    def _lcg(self) -> float:
        self._rng_seed = clamp_u32(self._rng_seed * 1664525 + 1013904223)
        return float(F32(F32(clamp_u32(self._rng_seed) & 0x7FFFFFFF) / F32(2147483648.0)))

    @staticmethod
    def _dist(m: MonsterSnapshot, p: PlayerSnapshot) -> np.float32:
        """曼哈顿距离 — 全程 float32, 与 C++ std::abs(float) 逐位一致"""
        return F32(F32(abs(F32(m.x) - F32(p.x))) + F32(abs(F32(m.y) - F32(p.y))))

    def _apply_attack(self, p: PlayerSnapshot) -> None:
        best, best_d = None, F32(999.0)
        # REAL_MODE 用真实武器射程 (REAL_ATTACK_RANGE=2.5); 玩具模式必须保持
        # C++ 的 d<1.5 硬编码, 否则破坏 parity。
        # ★ 曾漏改此处 → 普攻射程声称2.5 但判定仍 1.5, 导致 d=2 的目标
        #   "合法却打不到", Boss 战永远无法取胜 (实测 variant1/5 300步零输出)
        hit_range = _attack_range() if REAL_MODE else F32(1.5)
        for m in self._state.monsters:
            if not m.alive:
                continue
            d = self._dist(m, p)
            if d < hit_range and d < best_d:
                best_d, best = d, m
        if best is not None:
            # REAL_MODE: 用真实武器伤害; 玩具模式: 用 p.attack (与 C++ 一致)
            base = (int(_attack_damage()) if REAL_MODE else p.attack)
            base = base - int(F32(best.pdef) * F32(0.5))
            dmg = max(1, int(F32(base) * (F32(0.8) + F32(0.4) * F32(self._lcg()))))
            best.hp -= float(F32(dmg))
            if best.hp <= 0:
                best.alive = False
        p.attack_cooldown = float(_attack_cooldown_set())

    def _apply_skill(self, p: PlayerSnapshot, si: int) -> None:
        for m in self._state.monsters:
            if not m.alive:
                continue
            d = self._dist(m, p)
            if d < _skill_range():
                base = int(F32(p.attack) * _skill_damage_mult(si)) - int(F32(m.pdef) * F32(0.5))
                dmg = max(1, int(F32(base) * (F32(0.8) + F32(0.4) * F32(self._lcg()))))
                m.hp -= float(F32(dmg))
                if m.hp <= 0:
                    m.alive = False
        p.skill_cooldowns[si] = float(_skill_cooldown_set(si))

    def _decay_cooldowns(self, p: PlayerSnapshot) -> None:
        if p.attack_cooldown > 0:
            p.attack_cooldown -= float(COOLDOWN_STEP)
        for i in range(4):
            if p.skill_cooldowns[i] > 0:
                p.skill_cooldowns[i] -= float(COOLDOWN_STEP)

    def _monster_retaliation(self, p: PlayerSnapshot) -> None:
        for m in self._state.monsters:
            if not m.alive:
                continue
            d = self._dist(m, p)
            if d < F32(1.5) and self._lcg() < 0.4:
                dmg = max(1, m.attack - int(F32(p.pdef) * F32(0.5)))
                p.hp -= float(F32(dmg))
                if p.hp <= 0:
                    p.alive = False

    def _compute_reward(self, prev_hp: float, prev_alive: int,
                        prev_enemy_hp: float) -> float:
        p = self._state.player
        alive_now = sum(1 for m in self._state.monsters if m.alive)
        reward = 50.0 * (prev_alive - alive_now)
        # REAL_MODE: 每点实际造成的伤害给正奖励 ——
        # 使"输出"与"挨打"对称, 否则最优解是"离开近身范围"而不是打赢
        if REAL_MODE:
            enemy_hp_now = sum(m.hp for m in self._state.monsters if m.hp > 0)
            dealt = max(0.0, prev_enemy_hp - enemy_hp_now)
            reward += REAL_DAMAGE_REWARD * dealt
        hp_delta = F32(p.hp) - F32(prev_hp)
        if hp_delta < 0:
            reward += float(hp_delta)
        if self._state.victory:
            reward += 200.0
        if not p.alive:
            reward -= 200.0
        if REAL_MODE:
            reward -= REAL_TIME_COST
        return reward

    def _check_terminal(self) -> None:
        if not any(m.alive for m in self._state.monsters):
            self._state.victory = True
            self._state.terminal = True
        if not self._state.player.alive:
            self._state.terminal = True

    def reset(self, initial: SimulationState) -> np.ndarray:
        self._state = initial.clone()
        self._rng_seed = self._state.seed
        return self.observation()

    def step(self, action: int) -> StepResult:
        if self._state.is_terminal():
            return StepResult(self.observation(), 0.0, True)
        self._state.depth += 1
        p = self._state.player
        prev_hp, prev_alive = p.hp, sum(1 for m in self._state.monsters if m.alive)
        prev_enemy_hp = sum(m.hp for m in self._state.monsters if m.hp > 0)

        if action == ATTACK:
            self._apply_attack(p)
        elif SKILL_1 <= action <= SKILL_4:
            self._apply_skill(p, action - SKILL_1)
        elif action == MOVE_UP:
            p.y -= 1.0
        elif action == MOVE_DOWN:
            p.y += 1.0
        elif action == MOVE_LEFT:
            p.x -= 1.0
        elif action == MOVE_RIGHT:
            p.x += 1.0

        self._decay_cooldowns(p)
        self._monster_retaliation(p)
        self._check_terminal()
        return StepResult(self.observation(),
                          self._compute_reward(prev_hp, prev_alive, prev_enemy_hp),
                          self._state.is_terminal())

    def is_done(self) -> bool:
        return self._state.is_terminal()

    def state(self) -> SimulationState:
        return self._state

    def possible_actions(self) -> list:
        """对齐 mcts::get_possible_actions"""
        p = self._state.player
        if not p.alive:
            return []
        out = []
        in_range = any(m.alive and self._dist(m, p) < _attack_range()
                       for m in self._state.monsters)
        if in_range and p.attack_cooldown <= 0:
            out.append(ATTACK)
        for i in range(4):
            if p.skill_cooldowns[i] <= 0:
                out.append(SKILL_1 + i)
        # 逐方向独立判定 — 照抄 action.h:64-68 的 mdx/mdy 写法
        mdx = (0.0, 0.0, -1.0, 1.0)
        mdy = (-1.0, 1.0, 0.0, 0.0)
        moves = (MOVE_UP, MOVE_DOWN, MOVE_LEFT, MOVE_RIGHT)
        for d in range(4):
            nx = p.x + mdx[d]
            ny = p.y + mdy[d]
            if 0 <= nx < GRID_W and 0 <= ny < GRID_H:
                out.append(moves[d])
        out.append(WAIT)
        return out

    def observation(self) -> np.ndarray:
        """Observation::from_state + to_vector 的 8 维 float32"""
        from observation_py import to_vector14
        return to_vector14(self._state)


def run_episode(env: CombatEnvironment, initial: SimulationState,
                seed: int, ep: int, max_steps: int = 60) -> dict:
    """复现 parity_dump.cpp 的动作选择循环 — 同一 LCG, 同一取模"""
    initial.seed = seed + ep * 7919
    env.reset(initial)
    steps = []
    for t in range(max_steps):
        if env.is_done():
            break
        legal = env.possible_actions()
        if not legal:
            break
        s = (seed + ep * 7331 + t * 104729) & 0xFFFFFFFF
        s = (s * DUMP_LCG_MUL + DUMP_LCG_ADD) & 0xFFFFFFFF
        action = legal[s % len(legal)]
        sr = env.step(action)
        steps.append({"t": t, "action": action, "reward": sr.reward,
                      "terminal": sr.terminal})
    return {"steps": steps, "victory": env.state().victory}
