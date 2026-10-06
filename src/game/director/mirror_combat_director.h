#pragma once
#include <vector>
#include <string>
#include <functional>
#include "types/weapon_types.h"
#include "ai/player_behavior/player_action.h"
#include "ai/mirror/mirror_agent.h"

class Player;
class Monster;
class MirrorAgent;
class BossAI;
struct Effect;
struct PlayerHabitProfile;

// ============================================================
// MirrorCombatDirector — 接管F15 Mirror Boss全部战斗决策
// 组合模式: BossSystemDirector持有 → 不继承BossAI
// ============================================================

struct MirrorSkillDef {
    std::string name;
    float cooldown = 3.0f;
    float dmg_mult = 1.0f;
    float range = 100.0f;    // pixels
    int   skill_type = 0;    // 0=melee, 1=projectile, 2=self_heal, 3=aoe, 4=time_stop
    float last_used = -99.0f;
};

// ── M4.1: 战术状态 — 由玩家画像 + 战斗态势驱动 ──
enum class MirrorTactic : int {
    OPEN_RANGED = 0,  // 开局远程消耗: 保持距离, 弹幕/远程技能为主
    ENGAGE_MELEE = 1, // 压进近战: 贴近玩家, 武器连招为主
    KITE = 2,         // 拉扯: 边退边打, 消耗玩家耐心
    ADAPTIVE = 3      // 平衡: 按距离自动切换 (默认)
};

// ── M4.3: 镜像武器槽 — 近战槽(玩家武器) / 远程槽(CROSSBOW) ──
struct MirrorWeaponSlot {
    int  type = (int)WeaponType::FIST;
    int  total_stages = 1;
    float mults[3] = {1.0f, 1.0f, 1.0f};
    float ranges[3] = {48.0f, 48.0f, 48.0f};
    float cd = 1.2f;
};

class MirrorCombatDirector {
public:
    MirrorCombatDirector();
    void init(const Player* player, Monster* boss, BossAI* bai,
              MirrorAgent* agent);
    // 每帧主循环 — 返回true表示本帧被镜像战斗占用
    bool tick(float dt, Monster* boss, Player* player, double game_time,
              MirrorAgent* agent, std::vector<Effect>* effects);

    // 公开供BossSystemDirector读取
    int  combo_stage() const { return _combo_stage; }
    int  behavior_state() const { return _behavior_state; }
    const std::vector<MirrorSkillDef>& skills() const { return _skills; }
    // M4.1: 战术状态只读
    MirrorTactic tactic() const { return _tactic; }
    const char* tactic_name() const;
    // M4.1 验证: 画像+态势 → 战术决策 (纯函数, 无副作用 — 便于单测三场景)
    static MirrorTactic decide_tactic(const PlayerHabitProfile& profile,
                                      float dist, float player_hp_pct);
    // ── M4.2: 镜像专属真冻结 — 玩家禁移动/攻击, Echo 可行动 ──
    // 冻结计时 (秒), >0 表示玩家被镜像时停冻结中; 在 tick() 内递减
    bool freeze_active() const { return _freeze_timer > 0.0f; }
    float freeze_remaining() const { return _freeze_timer; }
    // Q3.9: 新对局重置 — 防 F15 冻结计时跨局泄漏 (无 boss 时 tick 不跑 → 永远冻结)
    void reset_run() { _freeze_timer = 0.0f; }
    // ── M4.3: 当前武器槽只读 ──
    const char* active_weapon_name() const;
    // M1: 适应播报回调 — GameScene 注入 show_message 通道 (战斗中"它在学我"话术)
    void set_callout_sink(std::function<void(const char*, float)> sink) {
        _callout_sink = std::move(sink);
    }
  
private:
    // M4.1: 战术层 — 画像驱动战术选择 + 技能映射
    void _update_tactic(const PlayerHabitProfile& profile, float dist,
                        float player_hp_pct);
    int  _pick_skill_for_tactic() const;
    void _tick_tactic_timer(float dt);
    // M4.3: 武器槽切换 — 战术 → 近战/远程槽
    void _sync_weapon_slot();
    void _switch_to_slot(const MirrorWeaponSlot& slot);
    // M4.2: 镜像冻结状态
    float _freeze_timer = 0.0f;
    void _weapon_attack(Monster* boss, Player* player, double gt,
                        std::vector<Effect>* effects);
    void _mirror_skill(Monster* boss, Player* player, double gt,
                       int skill_idx, std::vector<Effect>* effects);
    void _ai_decide(Monster* boss, Player* player, double gt,
                    MirrorAgent* agent);

    // G36-A: _ai_decide helpers
    MirrorBattleState _build_battle_state(Monster* boss, Player* player, double gt);
    void _apply_pressure_close(MirrorAgent* agent, const MirrorBattleState& st, float tactic_dist);
    void _apply_prediction_adjustment(MirrorAgent* agent, PlayerActionType pred, const MirrorBattleState& st);
    // M4e: 在线决策动作映射 — act<0 返回false (观察期走规则)
    bool _apply_online_action(int act);
    void _chase_player(Monster* boss, Player* player, float dt);
    // M2: 识别玩家本帧实际动作 (攻击/技能/闪避/喝药) — 供在线准确率反馈
    PlayerActionType _detect_player_action(Player* player, double gt,
                                           float dx, float dy, float* last_hp);

    // 武器镜像数据
    int   _combo_stage = 0;
    float _combo_timer = 0.0f;
    float _attack_cd = 1.2f;
    float _last_attack = -99.0f;
    float _stage_mults[3] = {1.0f, 1.0f, 1.0f};
    float _stage_ranges[3] = {48.0f, 48.0f, 48.0f};
    int   _total_stages = 1;
    // M4.3: 武器槽 — 近战(玩家武器) + 远程(CROSSBOW), 战术驱动切换
    MirrorWeaponSlot _slot_melee;    // 玩家武器
    MirrorWeaponSlot _slot_ranged;   // 远程弹幕
    bool _slot_is_ranged = false;    // 当前激活槽
    float _weapon_switch_timer = 0.0f;  // 武器切换冷却 (防抖)
    // 技能镜像数据
    std::vector<MirrorSkillDef> _skills;
    int   _skill_idx = 0;
    float _skill_cd_timer = 0.0f;
    // AI 状态
    int   _behavior_state = 0;  // 0=approach, 1=attack, 2=skill, 3=retreat
    float _decision_timer = 0.0f;
    float _preferred_dist = 200.0f;
    float _aggression_bonus = 1.0f;
    // Boss 引用
    Monster* _boss = nullptr;
    BossAI*  _bai = nullptr;
    // M4.1: 战术状态 (画像驱动)
    MirrorTactic _tactic = MirrorTactic::ADAPTIVE;
    float _tactic_timer = 0.0f;      // 战术切换冷却 (防止抖动)
    // M4e: 在线自适应反馈
    MirrorAgent* _agent = nullptr;
    float _last_player_x = 0.0f;   // 玩家闪避检测
    float _last_player_y = 0.0f;
    float _last_player_hp = 0.0f;  // M2: 玩家喝药检测 (HP 上升判定)
    // M1: 适应播报通道
    std::function<void(const char*, float)> _callout_sink;
    void _emit_callout(const char* msg, float dur = 3.0f) {
        if (_callout_sink && msg) _callout_sink(msg, dur);
    }
};
