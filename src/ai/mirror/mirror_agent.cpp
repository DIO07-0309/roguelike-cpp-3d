#include "ai/mirror/mirror_agent.h"
#include "ai/rl/q_agent.h"        // v0.9.30: RL 决策层
#include "ai/rl/observation.h"
#include "ai/rl/dqn_mirror_bridge.h"
#include "core/logger.h"
#include "combat_system.h"   // rng()
#include <algorithm>
#include <cstdio>

// M4.5: 战术链符号 → 玩家动作类型 (定义在匿名命名空间, 供 should_interrupt_skill 前置使用)
static PlayerActionType chain_symbol_to_action(int s);

// v0.9.30: RL 适配 — MirrorBattleState → 训练观测 (字段与 rl_runner make_scenario 对齐)
static rl::Observation mirror_state_to_obs(const MirrorBattleState& st, int style) {
    rl::Observation o;
    o.player_hp_ratio = st.player_hp_pct;
    o.player_attack = 10.0f;          // 训练常量 (rl_runner 玩家攻击=10)
    o.enemy_count = 1.0f;             // 单 Boss 战
    o.nearest_enemy_dist = st.dist_tiles;
    o.strongest_hp_ratio = st.boss_hp_pct;
    o.boss_present = 1.0f;
    o.buff_count = 0;
    o.player_style = (float)style;
    return o;
}

// v0.9.30: RL 映射 — 玩家视角最优动作 → 镜像反制臂 (镜像语义: 用玩家学到的最优策略反打)
static int combat_action_to_arm(int action_id, float dist_tiles) {
    switch ((mcts::CombatAction)action_id) {
    case mcts::CombatAction::ATTACK: return (int)MirrorAction::COMBO;
    case mcts::CombatAction::SKILL_1:
    case mcts::CombatAction::SKILL_2:
    case mcts::CombatAction::SKILL_3:
    case mcts::CombatAction::SKILL_4: return (int)MirrorAction::SKILL;
    case mcts::CombatAction::MOVE_UP:
    case mcts::CombatAction::MOVE_DOWN:
    case mcts::CombatAction::MOVE_LEFT:
    case mcts::CombatAction::MOVE_RIGHT:
        return dist_tiles < 3.0f ? (int)MirrorAction::RETREAT : (int)MirrorAction::APPROACH;
    default: return (int)MirrorAction::RETREAT;   // WAIT → 拉扯
    }
}

MirrorAgent::MirrorAgent()
    : _debug_stats(std::make_unique<MirrorDebugStats>()) {}

MirrorAgent::~MirrorAgent() = default;

void MirrorAgent::init(const PlayerHabitProfile& profile) {
    _profile = profile;
    _phase = 1;

    // M4e: 创建在线学习策略, 注入离线画像先验
    _policy = std::make_unique<OnlineAdaptivePolicy>();
    _policy->init_prior(profile);
    _last_bucket = -1;
    _last_action = -1;

    // Derive preferred distance from profile
    if (_profile.style == PlayerStyle::AGGRESSIVE)
        _preferred_distance = 180.0f;   // stay mid-close, bait attacks
    else if (_profile.style == PlayerStyle::SNIPER)
        _preferred_distance = 320.0f;   // keep them cornered, close the gap
    else if (_profile.style == PlayerStyle::MAGE)
        _preferred_distance = 250.0f;   // pressure at mid-range, interrupt casts
    else if (_profile.style == PlayerStyle::DEFENSIVE)
        _preferred_distance = 150.0f;   // force them into a corner, no escape
    else
        _preferred_distance = 230.0f;   // balanced
}

const char* MirrorAgent::phase_name() const {
    if (_phase == 1) return "Observe";
    if (_phase == 2) return "Mirror";
    return "Evolve";
}

float MirrorAgent::recommend_distance() const {
    // Phase 1: match player's preferred distance exactly (mirror)
    // Phase 2: adjust slightly to counter
    // Phase 3: actively close/pull based on weakness
    float base = _preferred_distance;
    if (_phase >= 2 && _profile.predict_low_dodge)
        base *= 0.7f;  // pressure closer if they dodge poorly
    if (_phase >= 3 && _profile.predict_attack_heavy)
        base *= 1.25f; // stay safer distance vs hyper-aggressive
    return base;
}

bool MirrorAgent::should_interrupt_skill(const MirrorBattleState& st) const {
    if (_phase < 2) return false;
    // M4e: 玩家正在施放技能 → 立即打断; 或按画像预判技能流
    if (st.player_using_skill || _profile.predict_skill_spam) return true;
    // M4.5-B: 战术链预测玩家将放技能 → 提前进入打断准备 (需链命中且高置信)
    if (_seq_b >= 0 && _seq_a >= 0) {
        ChainPrediction cp = _chain ? _chain->predict(_seq_a, _seq_b)
                                    : ChainPrediction{};
        if (cp.level < 0 && _chain)
            cp = _chain->predict_fuzzy2(_seq_b);
        if (cp.level >= 0 && cp.confidence > clone_confidence_threshold()
            && chain_symbol_to_action(cp.best) == PlayerActionType::SKILL)
            return true;
    }
    return false;
}

bool MirrorAgent::should_pressure_close(const MirrorBattleState& st) const {
    if (_phase < 2) return false;
    // Pressure close: player HP is low and they tend to heal at this threshold
    if (st.player_hp_pct * 100.0f < _profile.hp_counter_threshold
        && _profile.heal_frequency > 0.01f)
        return true;
    // Phase 3: always pressure if player has low dodge
    if (_phase >= 3 && _profile.predict_low_dodge)
        return true;
    return false;
}

void MirrorAgent::set_clone_table(std::unique_ptr<BehaviorCloneTable> t) {
    _clone = std::move(t);
}

// M4.4: 注入战术链表 (离线构建, F15 enter 时与克隆表同步注入)
void MirrorAgent::set_chain_table(std::unique_ptr<TacticalChainTable> t) {
    _chain = std::move(t);
}

// M4.4: 滚动战术符号缓冲 — 新符号进入, 旧 a 移位
int MirrorAgent::_recent_tactical(int symbol) {
    int prev_a = _seq_a;
    _seq_a = _seq_b;
    _seq_b = symbol;
    return prev_a;
}

static PlayerActionType intent_to_action(PlayerIntention i) {
    switch (i) {
    case PlayerIntention::ATTACK: return PlayerActionType::ATTACK;
    case PlayerIntention::SKILL:  return PlayerActionType::SKILL;
    case PlayerIntention::DODGE:  return PlayerActionType::DODGE;
    case PlayerIntention::HEAL:   return PlayerActionType::HEAL;
    default:                      return PlayerActionType::NONE;  // no mapping -> rules
    }
}

// M4.5-A: 战术链符号 → 玩家动作类型 (SKILL_*→SKILL, COMBO_*→ATTACK; MOVE 非决策动作→NONE)
static PlayerActionType chain_symbol_to_action(int s) {
    switch ((TacticalSymbol)s) {
    case TacticalSymbol::SKILL_0:
    case TacticalSymbol::SKILL_1:
    case TacticalSymbol::SKILL_2:
    case TacticalSymbol::SKILL_3: return PlayerActionType::SKILL;
    case TacticalSymbol::COMBO_1:
    case TacticalSymbol::COMBO_2:
    case TacticalSymbol::COMBO_3: return PlayerActionType::ATTACK;
    default:                      return PlayerActionType::NONE;
    }
}

// M4.5-A + v1.6-B1.1: 战术链预测内联在 predict_next_action — 缓存 action+confidence
// 供 HUD 只读消费; 避免独立静态 helper 与 mutable 缓存分离

PlayerActionType MirrorAgent::predict_next_action(
    const MirrorBattleState& st) const {
    // M4.5-A: 战术链层先行 — 预测玩家下一步战术动作 (网型序列优于单步克隆)
    ChainPrediction cp;
    if (_chain && _seq_a >= 0 && _seq_b >= 0) {
        cp = _chain->predict(_seq_a, _seq_b);
        if (cp.level < 0) cp = _chain->predict_fuzzy2(_seq_b);
    }
    if (cp.level >= 0 && cp.confidence > clone_confidence_threshold()) {
        PlayerActionType chain_act = chain_symbol_to_action(cp.best);
        if (chain_act != PlayerActionType::NONE) {
            _last_pred_action = chain_act;
            _last_pred_conf   = cp.confidence;
            _debug_stats->on_predict(0);   // 验收: 战术链命中 (与克隆层级共统计)
            return chain_act;
        }
    }
    // M1: behavior-clone layer (exact/fuzzy/profile), rules as fallback
    if (_phase >= 2 && _clone) {
        ClonePrediction p = _clone->predict(st.dist_tiles, st.player_hp_pct,
                                            st.player_skills_ready);
        _debug_stats->on_predict(p.level);   // 验收: 记录降级链等级
        if (p.level <= 2 && p.confidence >= clone_confidence_threshold()) {
            PlayerActionType t = intent_to_action(p.best);
            if (t != PlayerActionType::NONE) {
                _last_pred_action = t;
                _last_pred_conf   = p.confidence;
                return t;
            }
        }
    }
    // Rule-based fallback (existing profile logic)
    _debug_stats->on_predict(-1);            // 验收: 记录规则兜底
    PlayerActionType fallback = PlayerActionType::ATTACK;
    if (st.dist_tiles < 3.0f && _profile.attack_frequency > 0.6f)
        fallback = PlayerActionType::ATTACK;
    else if (st.player_hp_pct < _profile.hp_counter_threshold / 100.0f
        && _profile.heal_frequency > 0.01f)
        fallback = PlayerActionType::HEAL;
    else if (_profile.predict_skill_spam && _phase >= 2)
        fallback = PlayerActionType::SKILL;
    _last_pred_action = fallback;
    _last_pred_conf   = 0.35f;               // 规则兜底固定低置信
    return fallback;
}

// ── M4e: 在线自适应 — Thompson 采样决策 ──
float MirrorAgent::_rand01() {
    return (float)((rng() % 1000000) / 1000000.0);
}

namespace {
// M3: 玩家意图 → Boss 应对臂 (镜像反制语义: 惩罚治疗/打断技能/拉扯走位)
int boss_arm_for_intent(PlayerIntention i) {
    switch (i) {
    case PlayerIntention::ATTACK:  return (int)MirrorAction::COMBO;
    case PlayerIntention::SKILL:   return (int)MirrorAction::SKILL;
    case PlayerIntention::HEAL:    return (int)MirrorAction::APPROACH;
    case PlayerIntention::DODGE:
    case PlayerIntention::RETREAT: return (int)MirrorAction::APPROACH;
    case PlayerIntention::ADVANCE: return (int)MirrorAction::RETREAT;
    default: return -1;
    }
}
int boss_arm_for_action(PlayerActionType t) {
    return boss_arm_for_intent(intention_from_action(t));
}
// M4.4: 在线类型级近似符号 — observe_actual 只有 PlayerActionType, 无 skill_id/combo_stage 细节
// SKILL→SKILL_0, ATTACK→COMBO_1 (连招起点近似); MOVE 在线无方向 → 不进入战术链
int type_level_symbol(PlayerActionType t) {
    switch (t) {
    case PlayerActionType::SKILL:  return (int)TacticalSymbol::SKILL_0;
    case PlayerActionType::ATTACK: return (int)TacticalSymbol::COMBO_1;
    default:                       return -1;
    }
}
// M4.4: 战术符号 → 玩家意图 (供战术链层仲裁)
PlayerIntention intention_from_chain_symbol(int s) {
    switch ((TacticalSymbol)s) {
    case TacticalSymbol::SKILL_0:
    case TacticalSymbol::SKILL_1:
    case TacticalSymbol::SKILL_2:
    case TacticalSymbol::SKILL_3: return PlayerIntention::SKILL;
    case TacticalSymbol::MOVE_L:
    case TacticalSymbol::MOVE_R:   return PlayerIntention::ADVANCE;
    case TacticalSymbol::MOVE_U:
    case TacticalSymbol::MOVE_D:   return PlayerIntention::RETREAT;
    case TacticalSymbol::COMBO_1:
    case TacticalSymbol::COMBO_2:
    case TacticalSymbol::COMBO_3: return PlayerIntention::ATTACK;
    default: return PlayerIntention::IDLE;
    }
}
}  // namespace

int MirrorAgent::recommend_action(const MirrorBattleState& st) {
    if (!_policy || _phase < 2) return -1;   // 观察期: 走规则
    // M3: ML 插槽优先 (G5 — 注册即启用, 默认关闭)
    if (_ml_predictor) {
        int arm = boss_arm_for_action(_ml_predictor(st));
        if (arm >= 0) {
            _debug_stats->on_arbitrate(false, true);   // 验收: ML 仲裁
            return _record_arm(arm, st);
        }
    }
    // M4.4: 战术链层仲裁 — 预测玩家下一步战术符号 → 意图 → 应对臂
    if (_chain && _seq_b >= 0 && _seq_a >= 0) {
        ChainPrediction cp = _chain->predict(_seq_a, _seq_b);
        if (cp.level < 0) cp = _chain->predict_fuzzy2(_seq_b);
        if (cp.level >= 0 && cp.confidence > clone_confidence_threshold()) {
            PlayerIntention it = intention_from_chain_symbol(cp.best);
            int arm = boss_arm_for_intent(it);
            if (arm >= 0) {
                _debug_stats->on_arbitrate(true, false);   // 验收: 战术链仲裁
                return _record_arm(arm, st);
            }
        }
    }
    // v0.9.30: RL 层仲裁 — 离线训练 Q 表 (全局最优策略经验, 优先于个体克隆)
    if (_rl_policy) {
        int arm = _rl_arm(st);
        if (arm >= 0) {
            _debug_stats->on_rl();   // 验收: RL 仲裁
            return _record_arm(arm, st);
        }
    }
    // M3: 克隆层仲裁 — 置信度>门槛 时克隆意图驱动 Boss 臂 (M4: 漂移大则门槛上浮)
    if (_clone) {
        ClonePrediction p = _clone->predict(st.dist_tiles, st.player_hp_pct,
                                            st.player_skills_ready);
        if (p.level <= 1 && p.confidence > clone_confidence_threshold()) {
            int arm = boss_arm_for_intent(p.best);
            if (arm >= 0) {
                _debug_stats->on_arbitrate(true, false);   // 验收: 克隆仲裁
                return _record_arm(arm, st);
            }
        }
    }
    _debug_stats->on_arbitrate(false, false);   // 验收: Thompson 采样
    float dist_px = st.dist_tiles * 32.0f;
    _last_bucket = OnlineAdaptivePolicy::bucket_for(dist_px, st.player_hp_pct);
    int act = _policy->select_action(_last_bucket);
    // 技能窗口探索: 玩家放技能时 40% 概率尝试技能反制 (反馈落在实际臂上)
    if (st.player_using_skill && act != (int)MirrorAction::SKILL
        && _rand01() < 0.4f)
        act = (int)MirrorAction::SKILL;
    _last_action = act;
    LOG_DEBUG("[MIRROR] 在线决策 phase=%d bucket=%d action=%d (%.0fpx, HP%.0f%%)",
        _phase, _last_bucket, _last_action, dist_px,
        st.player_hp_pct * 100.0f);
    return _last_action;
}

// v0.9.36 G32-E: 从 MirrorBattleState 构造 CombatAction 合法集 (G32-E-B2)
// MirrorBattleState 只有 boss_can_attack 能判定 ATTACK 是否可用; 技能冷却
// 与 Boss 坐标不在此结构中, 故 SKILL/MOVE/WAIT 一律视为可用 —— 这是当前
// 信息量下的最优近似, 待 MirrorBattleState 补齐冷却/坐标后应收紧此处。
// ★ boss_can_attack 语义是"Boss 能否反击"(安全性), 不是"玩家能否攻击",
//   故只 gate ATTACK 而非反转判断。
static std::vector<mcts::CombatAction> mirror_legal_actions(
    const MirrorBattleState& st) {
    std::vector<mcts::CombatAction> legal;
    if (st.boss_can_attack) legal.push_back(mcts::CombatAction::ATTACK);
    for (int i = 0; i < 4; i++)
        legal.push_back((mcts::CombatAction)((int)mcts::CombatAction::SKILL_1 + i));
    legal.push_back(mcts::CombatAction::MOVE_UP);
    legal.push_back(mcts::CombatAction::MOVE_DOWN);
    legal.push_back(mcts::CombatAction::MOVE_LEFT);
    legal.push_back(mcts::CombatAction::MOVE_RIGHT);
    legal.push_back(mcts::CombatAction::WAIT);
    return legal;
}

// v0.9.30: RL 决策 — 观测适配 → exploit → 反制臂映射; 无 Q 值条目返回 -1 (落下级链)
int MirrorAgent::_rl_arm(const MirrorBattleState& st) const {
    if (!_rl_policy) return -1;
    int style = MirrorAgent::style_to_int(_profile.style);
    rl::Observation obs = mirror_state_to_obs(st, style);
    int act = _rl_policy->exploit_action(obs, mirror_legal_actions(st));
    // v0.9.36 G32-E: -1 哨兵必须先拦下 (G32-E-B5)
    // combat_action_to_arm(-1) 会落进 default 返回 RETREAT(=1, **非负**),
    // 使调用方的 `arm >= 0` 判断恒真 → RL 层对 Q 表从未见过的状态也强行
    // 接管, 且固定后撤。原实现在此直接透传 -1, "置信度感知降级"
    // (AI_LEARNING_GUIDE.md:304) 形同虚设。
    if (act < 0) return -1;      // Q 表未见过该状态 → 不接管, 落下级链
    return combat_action_to_arm(act, st.dist_tiles);
}

void MirrorAgent::set_rl_policy(std::unique_ptr<rl::QAgent> q) {
    _rl_policy = std::move(q);
}

// v0.9.33 G32-B: 注册 DQN 决策层 — 加载失败保持关闭, 返回 false
bool MirrorAgent::set_dqn_policy(const std::string& weights_path,
                                 int player_style) {
    auto bridge = std::make_shared<rl::DqnMirrorBridge>();
    if (!bridge->load(weights_path)) return false;   // 未就绪 → 不注册
    _dqn_bridge = bridge;
    int style = player_style;
    set_ml_predictor([bridge, style](const MirrorBattleState& st) {
        return bridge->predict(st, style);
    });
    return true;
}

void MirrorAgent::report_outcome(bool hit, float damage) {
    if (!_policy || _last_bucket < 0 || _last_action < 0) return;
    // 命中: 伤害越高奖励越接近1; 落空/被躲: 0
    float reward = hit
        ? std::min(1.0f, 0.6f + 0.4f * std::min(1.0f, damage / 30.0f))
        : 0.0f;
    _policy->update(_last_bucket, _last_action, reward);
    LOG_DEBUG("[MIRROR] 反馈 bucket=%d action=%d hit=%d dmg=%.0f reward=%.2f",
        _last_bucket, _last_action, hit ? 1 : 0, damage, reward);
    _last_bucket = -1;   // 一次决策只允许一次反馈 (防 dodge 重复上报)
    _last_action = -1;
}

void MirrorAgent::export_memory(std::vector<float>& alpha,
                                std::vector<float>& beta) const {
    if (_policy) {
        _policy->export_alpha(alpha);
        _policy->export_beta(beta);
    } else {
        alpha.clear();
        beta.clear();
    }
}

void MirrorAgent::import_memory(const std::vector<float>& alpha,
                                const std::vector<float>& beta_vals) {
    if (!_policy) return;
    _policy->import_alpha(alpha);   // 旧后验 = 新先验 (叠加)
    _policy->import_beta(beta_vals);
}

float MirrorAgent::arm_win_rate(int bucket, int action) const {
    if (!_policy) return 0.0f;
    return _policy->win_rate(bucket, action);
}

// M3: 记录选中臂与上下文桶 — 保持 report_outcome 反馈链完整
int MirrorAgent::_record_arm(int act, const MirrorBattleState& st) {
    float dist_px = st.dist_tiles * 32.0f;
    _last_bucket = OnlineAdaptivePolicy::bucket_for(dist_px, st.player_hp_pct);
    _last_action = act;
    return act;
}

// ── M2: 在线观测 ──
void MirrorAgent::on_prediction(PlayerActionType predicted, float dist_tiles,
                                float hp_pct, int skills_ready) {
    if (!is_decision_action(predicted)) return;
    _last_prediction = predicted;
    _last_obs_key = CloneContext::from_state(dist_tiles, hp_pct, skills_ready).key();
}

void MirrorAgent::observe_actual(PlayerActionType actual) {
    if (!is_decision_action(actual)) return;
    _observed_actions++;
    // M4.4: 滚动战术符号缓冲 (在线仅类型级近似符号; 缺 skill_id/combo 细节 → 自然降级)
    int sym = type_level_symbol(actual);
    if (sym >= 0) _recent_tactical(sym);
    switch (actual) {
    case PlayerActionType::ATTACK: _obs_attack++; break;
    case PlayerActionType::SKILL:  _obs_skill++;  break;
    case PlayerActionType::DODGE:  _obs_dodge++;  break;
    case PlayerActionType::HEAL:   _obs_heal++;   break;
    default: break;
    }
    if (_last_prediction == PlayerActionType::NONE) return;
    bool hit = (actual == _last_prediction);
    _rolling_accuracy.add(hit);
    if (hit) _bucket_hits[_last_obs_key]++;
    _last_prediction = PlayerActionType::NONE;
}

int MirrorAgent::_max_bucket_hits() const {
    int best = 0;
    for (const auto& kv : _bucket_hits) {
        if (kv.second > best) best = kv.second;
    }
    return best;
}

float MirrorAgent::profile_drift() const {
    if (_observed_actions < 10 || _battle_time <= 0.01f) return 0.0f;
    float cur_attack = (float)_obs_attack / _battle_time;
    float cur_skill  = (float)_obs_skill  / _battle_time;
    float attack_norm = _profile.attack_frequency > 0.01f
        ? fabsf(cur_attack - _profile.attack_frequency) / _profile.attack_frequency : 0.0f;
    float skill_norm = _profile.skill_frequency > 0.01f
        ? fabsf(cur_skill - _profile.skill_frequency) / _profile.skill_frequency : 0.0f;
    float drift = (attack_norm + skill_norm) * 0.5f;
    return drift > 1.0f ? 1.0f : drift;
}

// M4: 漂移降权 — 玩家已换打法 → 克隆/画像模仿降权 (门槛上浮, 交 Thompson)
float MirrorAgent::clone_confidence_threshold() const {
    const MirrorTuning& t = mirror_tuning();
    float bar = t.clone_confidence;
    if (profile_drift() > t.drift_penalty_threshold) bar += t.drift_penalty_step;
    return bar;
}

void MirrorAgent::tick_phase(float dt, const MirrorBattleState& st) {
    _battle_time += dt;
    _debug_stats->tick_phase(_phase, dt);   // 验收: 各 Phase 时长统计
    const MirrorTuning& t = mirror_tuning();
    switch (_phase) {
    case 1: {
        float acc = _rolling_accuracy.accuracy();
        bool learned = (acc >= t.phase1_accuracy_threshold
                     && _observed_actions >= t.phase1_min_observations)
                    || _observed_actions >= t.phase1_obs_backstop;
        // M1: 晋升播报 — 观察期结束 = "它已经摸清你的路数"
        if (learned)
            set_phase_announced(2, "镜像已摸清你的出招路数");
        else if (_battle_time >= t.phase1_time_backstop)
            set_phase_announced(2, "镜像停止观望 — 开始模仿你");
        break;
    }
    case 2: {
        bool pattern = _max_bucket_hits() >= t.phase2_same_bucket_hits
                    && _rolling_accuracy.accuracy() >= t.phase2_accuracy_threshold;
        bool danger = st.player_hp_pct < t.phase2_hp_danger
                   || st.boss_hp_pct < t.phase2_hp_danger;
        // M1: 进化播报 — 命中同桶预测或濒危 = "它预判了你的习惯"
        if (pattern)
            set_phase_announced(3, "镜像已能预判你的习惯 — 进化!");
        else if (danger)
            set_phase_announced(3, "镜像进入濒死进化 — 全力反扑!");
        break;
    }
    default: break;
    }
}

// ── F15.4: Mirror reward ──
double MirrorAgent::mirror_reward(const PlayerHabitProfile& profile,
    int /*boss_action*/, double damage_dealt, double damage_taken,
    bool player_dodged, bool player_healed)
{
    double reward = damage_dealt * 0.5 - damage_taken * 0.3;

    // Counter bonuses
    if (profile.predict_panic_heal && player_healed)
        reward += 1.0;  // punished a panic-heal player
    if (profile.predict_low_dodge && player_dodged)
        reward += 0.5;  // predicted and hit a low-dodge player
    if (profile.predict_attack_heavy)
        reward += 0.3;  // aggressive profile = more opportunities to counter

    return reward;
}

int MirrorAgent::style_to_int(PlayerStyle s) {
    switch (s) {
    case PlayerStyle::AGGRESSIVE: return 1;
    case PlayerStyle::DEFENSIVE:  return 2;
    case PlayerStyle::SNIPER:     return 3;
    case PlayerStyle::MAGE:       return 4;
    default: return 0;
    }
}
