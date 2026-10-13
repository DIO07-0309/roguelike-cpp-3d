#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "boss_narrative.h"
#include "boss_evolution.h"
#include "boss_behavior.h"
#include "boss_command.h"
#include "arena_manager.h"
#include "boss_replay.h"
#include "boss_encounter.h"
#include "boss_cinematic.h"
#include "boss_timeline.h"
#include "build_score.h"

// F15.3: MirrorAgent
#include "ai/mirror/mirror_agent.h"
// F15 Mirror Combat
#include "mirror_combat_director.h"

class Monster;
class Player;
class GameMap;
class WorldState;
class RelationshipSystem;
struct Effect;
struct GameEvent;

// ============================================================
// D6 Step3: BossSystemDirector — 收纳所有Boss子系统
// 组合模式: 持有Narrative/Evolution/Behavior/Command/
//           Encounter/Replay/Cinematic/Arena/Timeline
// GameScene 仅通过 BossSystemDirector 协调 Boss 生命周期
// ============================================================
class BossSystemDirector {
public:
    // ── 聚合的子系统 (组合, 非继承) ──
    BossNarrative         narrative;
    BossEvolutionData     evolution;
    BossBehaviorState     behavior;
    BossSkillQueue        skill_queue;
    ArenaManager          arena;
    BossEncounterController encounter;
    BossReplayMemory      replay_mem;
    BossBattleReport      battle_report;
    BossCinematicController cinematic;
    BossTimeline          timeline;
    BossCommand           current_cmd = BossCommand::NONE;
    BossModifierHook      modifier_hook;

    // ── F10.1: Arena state machine (domain boss) ──
    BossArenaState arena_state = BossArenaState::INTRO;
    float  domain_timer = 0.0f;
    float  domain_cycle_duration = 30.0f;  // seconds per full domain→mechanic→vulnerable cycle
    bool   boss_invulnerable = false;       // set true during DOMAIN/MECHANIC phases
    int    domain_cycle_count = 0;          // how many cycles completed
    std::string _behavior_type;             // from BossDef ("" = standard, "domain" = domain boss)

    // ── F10.2: Weak point tracking ──
    Monster* _active_core = nullptr;          // currently spawned fire_core (nullptr if broken)
    float    _vulnerable_dmg_mult = 2.0f;    // damage multiplier during VULNERABLE_PHASE
    float    _vulnerable_duration = 10.0f;   // F10.3: 弱点阶段时长 (数据驱动)
    float    _weakness_dmg_mult = 1.0f;      // F10.3: 克制元素对核心伤害倍率 (数据驱动)
    std::vector<std::unique_ptr<Monster>>* _weak_point_pool = nullptr;  // set by caller

    // ── F15: Mirror Boss ──
    std::unique_ptr<MirrorAgent> _mirror_agent;
    MirrorCombatDirector _mirror_combat;
    void _init_mirror_boss(Monster* boss, const class Player* player);
    // G52: 从 BossDef 读 behavior_type 并初始化 domain/mirror 子系统
    void _init_behavior_from_def(Monster* boss, const class Player* player);
    // M4e: 跨对局记忆 — 导出/注入 (空 vector 安全)
    void export_mirror_memory(std::vector<float>& alpha,
                              std::vector<float>& beta) const;
    void inject_mirror_memory(const std::vector<float>& alpha,
                              const std::vector<float>& beta);
    // M1: 镜像学习剧场接线 — GameScene 注入播报通道 (战术/阶段晋升 → 屏幕消息)
    void connect_mirror_theater(std::function<void(const char*, float)> sink);
    // v1.6-B1: 阶段晋升横幅通道 — "它开始模仿你/看穿你" 醒目播报
    void hook_phase_banner(std::function<void(int, const char*)> banner);

    std::string intro_text;
    std::string modifier_text;
    int  dmg_done = 0, dmg_taken = 0;

    // ── 生命周期 Hooks (GameScene 调用) ──
    // P1-C7-C: reset 语义拆分 — 原单一 reset() 设计注释"新楼层开始时调用"
    // 但全仓零调用, boss 子系统状态跨层跨局全残留。拆分后:
    //   reset_floor() — enter_floor 调用: 清跨层运行态 (evolution/behavior/
    //     encounter/cinematic/timeline/domain/arena 等; replay_mem 由
    //     init_on_spawn 每战重建, 此处一并清属双保险)
    //   reset_run() — new_game 调用: = reset_floor(); boss 子系统无"局内
    //     跨层须持久"字段 (mirror 跨局记忆走 export/inject 独立通道, 不清)
    void reset_floor();
    void reset_run() { reset_floor(); }
    void init_on_spawn(Monster* boss, int floor, const WorldState& ws, BuildType bt,
                       const RelationshipSystem& rels, GameMap* map,
                       const class Player* player = nullptr);
    // M4.2: 镜像冻结透传 (GameScene 只读)
    bool mirror_freeze_active() const { return _mirror_combat.freeze_active(); }
    float mirror_freeze_remaining() const { return _mirror_combat.freeze_remaining(); }
    void tick(float dt, Monster* boss, Player* player, int floor, float game_time,
              const WorldState& ws, const RelationshipSystem& rels,
              StoryStage stage, std::vector<std::unique_ptr<Monster>>& monsters,
              std::vector<Effect>* effects = nullptr);
    void notify_phase2();
    void notify_last_stand(Monster* boss);
    void notify_death(const WorldState& ws, const RelationshipSystem& rels,
                      const QuestManager& qm);
    void on_core_maybe_erased();   // F10.2-fix: UAF guard — cleanup 前置钩子

    void init_events();  // D7 Step5: 订阅 EventBus
    void unregister_events();  // 场景析构时注销, 防止悬挂订阅

private:
    void notify_death_ev(const struct GameEvent&);
    bool _mirror_stats_logged = false;              // 验收: 战斗统计只导出一 次
    void _tick_domain_state(float dt, Monster* boss);  // F10.1
    void _spawn_domain_core(Monster* boss);             // F10.2
    bool _enraged = false;                              // M4d-fix: 狂暴领域标记
    void _tick_core_phase(float dt, Monster* boss, float max_duration);  // M4d-fix
    void _enter_vulnerable_phase(Monster* boss);    // M4d-fix
    float _enraged_cycle_duration() const;      // M4d-fix: 狂暴时核心周期减半
    float _enraged_vulnerable_duration() const; // M4d-fix: 狂暴时弱点窗口减半
    // M4b: 机制阶段 — 核心破坏后弹幕演出段 (Boss 无敌, 密集弹幕, 结束→易伤)
    float _mechanic_duration = 3.5f;            // 数据驱动 (domain_config.mechanic_duration)
    float _mechanic_timer = 0.0f;               // 演出剩余时间
    float _mechanic_barrage_timer = 0.0f;       // 演出弹幕间隔计时
    void _enter_mechanic_phase(Monster* boss);  // 核心破坏→演出
    void _tick_mechanic_phase(float dt, Monster* boss);
    void _force_barrage(Monster* boss);         // 演出弹幕 (快速蓄力释放)

public:
    // ── 查询接口 ──
    const BossBattleReport& report() const { return battle_report; }
    BossCommand command() const { return current_cmd; }
    bool cinematic_active() const { return cinematic.is_running(); }
    const BossTimeline& get_timeline() const { return timeline; }

    // ── G2.3: Arena spawn timer (属于 BossSystemDirector — Encounter 节奏) ──
    float _arena_spawn_timer = 0.0f;
    const BossArenaDef* _arena_cfg = nullptr;
    int   _arena_phase = 0;     // M4b: 0=normal, 1=phase2, 2=last_stand
    float _arena_spawn_mult = 1.0f; // M4b: spawn_interval multiplier per phase
};
