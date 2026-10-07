#pragma once
#include "ai/player_behavior/player_habit_profile.h"
#include "ai/player_behavior/player_action.h"
#include "ai/mirror/online_adaptive_policy.h"
#include "ai/mirror/behavior_clone_table.h"
#include "ai/mirror/rolling_accuracy.h"
#include "ai/mirror/mirror_debug_stats.h"
#include "ai/mirror/mirror_tuning.h"
#include "ai/mirror/tactical_chain_table.h"
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <map>
#include <string>
#include <functional>

class Monster;
class Player;
namespace rl { class QAgent; class DqnMirrorBridge; }  // v0.9.30/v0.9.33: RL 决策层 (前向声明, 避免头文件耦合)

// ============================================================
// F15.3: MirrorAgent — analysis layer, NOT a control layer
// Reads player habits → adjusts BossAI strategy parameters.
// Does NOT call attack/move — BossAI still makes final decisions.
// ============================================================

// Simple battle state snapshot (avoid depending on full BattleState)
struct MirrorBattleState {
    float boss_hp_pct = 1.0f;
    float player_hp_pct = 1.0f;
    float dist_tiles = 5.0f;
    bool  player_attacking = false;
    bool  player_using_skill = false;
    bool  boss_can_attack = false;
    bool  boss_in_domain = false;       // F10 domain phase
    int   player_skills_ready = 0;      // M1: cooldown-ready skill count
};

class MirrorAgent {
public:
    MirrorAgent();
    ~MirrorAgent();   // v0.9.30: 完整类型析构 (unique_ptr<rl::QAgent>)

    // ── Initialize from player behavior data (called on F15 enter) ──
    void init(const PlayerHabitProfile& profile);

    // ── Phase 1-2-3 behavior selection ──
    int  current_phase() const { return _phase; }   // 1=observe, 2=mirror, 3=evolve
    void set_phase(int p) { _phase = p; }
    // M1: 阶段晋升播报 — GameScene 注入 show_message 通道 (晋升即播"它学会了")
    void set_phase_sink(std::function<void(int, const char*)> sink) {
        _phase_sink = std::move(sink);
    }
    void set_phase_announced(int p, const char* reason) {
        bool changed = (p != _phase);
        _phase = p;
        if (changed && _phase_sink) _phase_sink(p, reason);
    }

    // ── M2: 在线观测与动态阶段触发 ──
    // 预测后立即上报 (附上下文用于同桶命中统计); 玩家实际动作出现时上报比对
    void on_prediction(PlayerActionType predicted, float dist_tiles,
                       float hp_pct, int skills_ready);
    void observe_actual(PlayerActionType actual);
    float prediction_accuracy() const { return _rolling_accuracy.accuracy(); }
    int   observed_actions() const { return _observed_actions; }
    // 当前战斗攻击/技能频率 vs 画像频率的归一化偏差 [0,1] (数据不足返回0)
    float profile_drift() const;
    // M4: 克隆层置信门槛 — 漂移大时上浮 (模仿降权, 交 Thompson 在线适应)
    float clone_confidence_threshold() const;
    // 动态阶段判定: P1→P2 (准确率/观察数/时间), P2→P3 (核心模式/濒危) — 替代纯计时
    void  tick_phase(float dt, const MirrorBattleState& st);

    // ── 验收: 战斗内 AI 调用链统计 (F9 HUD / 战斗结束日志) ──
    void begin_battle() {
        _debug_stats->reset();
        _last_pred_time = -1.0f;         // v1.6-B1.1: 新战斗 → HUD 显示"观察中…"
        _last_pred_action = PlayerActionType::NONE;
        _last_pred_conf = 0.0f;
    }
    void report_interrupt(bool success) { _debug_stats->on_interrupt(success); }
    void debug_behavior_state(int state) { _debug_stats->on_behavior_state(state); }
    const MirrorDebugStats* debug_stats() const { return _debug_stats.get(); }

    // ── M1: behavior-clone layer (built from F1-F14 action stream) ──
    void set_clone_table(std::unique_ptr<BehaviorCloneTable> t);
    const BehaviorCloneTable* clone_table() const { return _clone.get(); }

    // ── M4.4: 战术链序列层 ──
    void set_chain_table(std::unique_ptr<TacticalChainTable> t);
    const TacticalChainTable* chain_table() const { return _chain.get(); }

    // ── v0.9.30: RL 决策层 (F15.4 离线训练 Q 表, 仲裁链插入克隆层之前) ──
    void set_rl_policy(std::unique_ptr<rl::QAgent> q);
    bool has_rl_policy() const { return _rl_policy != nullptr; }

    // ── v0.9.33: DQN 决策层 (手写推理, 通过 MlPredictor 接入仲裁链) ──
    bool set_dqn_policy(const std::string& weights_path, int player_style);

    // ── During combat: recommend BossAI adjustments ──
    float recommend_distance() const;
    bool should_interrupt_skill(const MirrorBattleState& st) const;
    bool should_pressure_close(const MirrorBattleState& st) const;
    PlayerActionType predict_next_action(const MirrorBattleState& st) const;

    // ── M4e: 在线自适应 — Thompson 决策与反馈 ──
    // Phase<2 返回 -1 (观察期用规则); 否则采样决策并记录待反馈状态
    int recommend_action(const MirrorBattleState& st);
    // 战斗中上报最近一次决策的结果: 命中/落空 → 更新 Beta 后验
    void report_outcome(bool hit, float damage);

    // ── M3: ML 预测器插槽 (G5) — 注册即启用, 仲裁优先于克隆层 ──
    // 默认 nullptr = 关闭 (当前版本无外部 ML)
    using MlPredictor = std::function<PlayerActionType(const MirrorBattleState&)>;
    void set_ml_predictor(MlPredictor fn) { _ml_predictor = std::move(fn); }

    // 跨对局记忆: 导出 Beta (alpha, beta) 供存档; 导入叠加回先验
    void export_memory(std::vector<float>& alpha, std::vector<float>& beta) const;
    void import_memory(const std::vector<float>& alpha,
                       const std::vector<float>& beta_vals);

    // ── M4e: HUD 学习可视化查询 (只读) ──
    int  last_action() const { return _last_action; }
    int  last_bucket() const { return _last_bucket; }
    float arm_win_rate(int bucket, int action) const;

    // ── v1.6-B1.1: Mirror 决策缓存 (供 HUD 只读消费; 决策链单向写入) ──
    // 写入者: predict_next_action (mutable 字段; 保持 const 语义, 单测透明)
    //         + 时间戳由 MirrorCombatDirector::_ai_decide 调 cache_pred_time 补上
    // HUD 严禁重跑 predict — 保证"玩家看到的百分比" = "Echo 下一步动作" 同一次决策
    void cache_pred_time(float now) { _last_pred_time = now; }
    PlayerActionType last_pred_action() const { return _last_pred_action; }
    float last_pred_conf() const { return _last_pred_conf; }
    // 距上次预测经过多少秒; 未决策时返回 9999 (供 HUD 判定"观察中…")
    float last_pred_age(float now) const {
        return _last_pred_time < 0.0f ? 9999.0f : now - _last_pred_time;
    }

    // ── v1.6-B1.1: 本局实时频率计数 (供 HUD 数字面板; 只读) ──
    int   obs_attack_count() const { return _obs_attack; }
    int   obs_skill_count()  const { return _obs_skill; }
    int   obs_dodge_count()  const { return _obs_dodge; }
    int   obs_heal_count()   const { return _obs_heal; }
    float battle_seconds()   const { return _battle_time; }

    // ── F15.4: Mirror reward for RL self-play ──
    // Returns a bonus reward when the Boss successfully counters the player's
    // predicted action (interrupting a skill, punishing a heal, baiting an attack).
    // Caller adds this to the base damage-healing reward.
    static double mirror_reward(const PlayerHabitProfile& profile,
        int boss_action, double damage_dealt, double damage_taken,
        bool player_dodged, bool player_healed);

    // ── F15.4: Convert PlayerStyle to an int for SimulationState ──
    static int style_to_int(PlayerStyle s);

    // ── M4.1: 画像访问 (战术层驱动) ──
    const PlayerHabitProfile& profile() const { return _profile; }

    // ── Debug ──
    const char* phase_name() const;
    const char* strategy_summary() const { return _profile.counter_strategy_text(); }

private:
    PlayerHabitProfile _profile;
    int  _phase = 1;
    std::function<void(int, const char*)> _phase_sink;   // M1: 晋升播报
    float _preferred_distance = 250.0f;
    // M4e: 在线学习
    std::unique_ptr<OnlineAdaptivePolicy> _policy;
    int _last_bucket = -1;    // 最近一次决策的上下文桶
    int _last_action = -1;    // 最近一次决策的动作臂
    // v0.9.30: RL Q 表决策层 (观察期不启用, 仲裁链在克隆层之前)
    std::unique_ptr<rl::QAgent> _rl_policy;
    std::shared_ptr<rl::DqnMirrorBridge> _dqn_bridge;  // v0.9.33: DQN 桥接, 默认关闭
    int _rl_arm(const MirrorBattleState& st) const;   // 返回 MirrorAction 或 -1 (不接管)
    std::unique_ptr<BehaviorCloneTable> _clone;   // M1: 行为克隆层
    static float _rand01();   // [0,1) 均匀采样 (技能窗口探索用)

    // M4.4: 战术链层 + 在线滚动符号缓冲 (最近 2 个战术符号)
    std::unique_ptr<TacticalChainTable> _chain;
    int _seq_a = -1;    // 最近战术符号 (前一个)
    int _seq_b = -1;    // 最近战术符号 (当前)
    int _recent_tactical(int symbol);   // 滚动更新 _seq_a/_seq_b, 返回旧 a

    // M2: 在线观察
    RollingAccuracy _rolling_accuracy;
    PlayerActionType _last_prediction = PlayerActionType::NONE;
    std::string _last_obs_key;                       // 最近预测的 ObservationKey
    std::map<std::string, int> _bucket_hits;         // 同桶命中计数
    int _observed_actions = 0;
    int _obs_attack = 0, _obs_skill = 0;
    int _obs_dodge = 0, _obs_heal = 0;
    float _battle_time = 0.0f;
    int _max_bucket_hits() const;
    // M3: 记录选中臂 (含上下文桶) — 保证 report_outcome 反馈链
    int _record_arm(int act, const MirrorBattleState& st);
    MlPredictor _ml_predictor;                       // M3: G5 插槽, 默认关闭
    std::unique_ptr<MirrorDebugStats> _debug_stats;  // 验收: AI 调用链统计

    // v1.6-B1.1: Mirror 决策缓存 (mutable 允许 predict_next_action const 语义下写)
    mutable PlayerActionType _last_pred_action = PlayerActionType::NONE;
    mutable float _last_pred_conf = 0.0f;
    float _last_pred_time = -1.0f;                   // 由 cache_pred_time 更新
};
