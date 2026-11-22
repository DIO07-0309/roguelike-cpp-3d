#pragma once
// ============================================================
// D7 Step3: boss_types.h — 所有Boss子系统共享数据结构
// 所有 boss/ director/ 统一引用此文件
// ============================================================
#include <string>
#include <vector>
#include "raylib.h"
#include "build_score.h"
#include "world_state.h"

// ── BossBehavior ──
enum class BossDecision {
    CHASE, RETREAT, CHARGE, SUMMON, DEFEND,
    RANGED, MELEE, SPECIAL, PHASE_CHANGE, LAST_STAND, IDLE
};
enum class BossPersonality { AGGRESSIVE, AREA_CONTROL, MANIPULATOR };

struct BossContext {
    float hp_pct;
    float dist_tiles;
    bool  player_low_hp;
    bool  player_using_skill;
    bool  player_combo_high;
    bool  player_far;
    bool  player_near;
    BuildType build;
    StoryStage stage;
    bool  last_stand;
    bool  near_arena_danger;
};

struct BossMemory {
    int  attacks = 0, skills = 0, heals = 0, dodges = 0, combos_max = 0, potions_used = 0;
    float timer = 0;
    void record_attack() { attacks++; }
    void record_skill()  { skills++; }
    void record_heal()   { heals++; }
    void record_dodge()  { dodges++; }
    void record_combo(int c) { if (c > combos_max) combos_max = c; }
    void tick(float dt, bool reset);
};

struct BossBehaviorState {
    BossDecision current = BossDecision::IDLE;
    BossDecision last_decision = BossDecision::IDLE;
    float decision_timer = 0;
    BossPersonality personality = BossPersonality::AGGRESSIVE;
    BossMemory memory;
    const char* decision_name = nullptr;
    const char* personality_name = nullptr;
    int weights[11] = {0};
};

// ── BossCommand ──
enum class BossCommand { NONE, NORMAL, MOVE, CHARGE, SHOCKWAVE, SUMMON, DEFEND, RETREAT, CAST, PHASE, LAST_STAND, RANGED, CONE, BLINK, WHIRLWIND };

struct BossAction {
    BossCommand command = BossCommand::NONE;
    float duration = 0.0f, cooldown = 0.0f, windup = 0.0f;
};

struct BossSkillQueue {
    std::vector<BossCommand> queue;
    int  current = -1;
    float next_delay = 0.0f;
    bool active = false;
    void clear() { queue.clear(); current = -1; active = false; }
    void enqueue(BossCommand c) { queue.push_back(c); }
    void start() { active = true; current = 0; next_delay = 0.3f; }
    BossCommand current_cmd() const {
        return (current >= 0 && current < (int)queue.size()) ? queue[current] : BossCommand::NONE;
    }
    void advance() { current++; if (current >= (int)queue.size()) clear(); }
};

// ── BossReplay ──
enum class BossRank { D, C, B, A, S, SS };

struct BossReplayMemory {
    int melee_hits = 0, ranged_hits = 0, skills_used = 0;
    int dodge_count = 0, potion_used = 0, combo_max = 0;
    float survive_time = 0.0f;
    BuildType build = BuildType::NONE;
    bool blood_ritual = false, curse = false, saved_priest = false;
    int analyze_strategy() const;
    const char* strategy_name() const;
};

struct BossBattleReport {
    BossReplayMemory replay;
    BossRank rank = BossRank::C;
    int total_damage = 0, damage_taken = 0;
    float battle_time = 0.0f;
    int arena_zones_spawned = 0;
    const char* rank_name() const;
};

// ── BossEvolution ──
struct BossSkillMod {
    float cooldown_scale = 1.0f, damage_scale = 1.0f, area_scale = 1.0f;
    int   extra_projectiles = 0;
    bool  double_strike = false;
    const char* variant_name = nullptr;
};

struct BossEvolutionData {
    int   boss_floor = 0;
    bool  last_stand_triggered = false;
    bool  build_counter_active[6] = {false};
    BossSkillMod charge_mod, shockwave_mod, summon_mod;
    const char* evolution_name = nullptr;
    bool  arena_effect = false;
};

// ── BossNarrative ──
struct BossDialogue {
    StoryStage  stage = StoryStage::INTRO;
    int         boss_floor = 0;
    WorldFlag   required_flag = WorldFlag::NONE;
    BuildType   build = BuildType::NONE;
    int         relation_threshold = 0, npc_id = 0;
    const char* intro = nullptr, *phase2 = nullptr, *death = nullptr;
    bool        once = false;
};

struct BossModifierHook {
    bool  disable_phase = false, disable_skill = false, add_minions = false;
    float hp_scale = 1.0f, damage_scale = 1.0f;
    const char* modifier_text = nullptr;
};

// ── Arena ──
enum class DangerType { LAVA, SHADOW_WALL, VOID_CRACK, FIRE_COLUMN };

struct DangerZone {
    DangerType type;
    float world_x, world_y;
    float radius = 48.0f, remaining = 0.0f, warn_timer = 0.0f;
    float damage_timer = 0.0f;  // M4a-fix: 伤害结算间隔 (0.5s), 避免每帧秒杀
    int   damage = 0;
    Color warn_color{255,80,40,100}, active_color{255,40,20,180};
    bool is_warning() const { return warn_timer > 0; }
};

// ── Timeline ──
struct TimelineEvent {
    float time = 0.0f;
    const char* label = nullptr;
};

// ── Cinematic / Encounter ──
enum class CinematicPhase { NONE, INTRO, PHASE2_TRANSITION, LAST_STAND, DEATH };
enum class EncounterPhase { OPENING, PRESSURE, CONTROL, LAST_STAND, ENDED };

// ═══════════════════════════════════════════════════════════════
// F10.2: WeakPoint type + lifecycle state
// ═══════════════════════════════════════════════════════════════
enum class WeakPointType { CORE };
enum class WeakPointState { ACTIVE, BROKEN };  // BROKEN 保留: F10.2 生命周期状态, 待补写入

// ═══════════════════════════════════════════════════════════════
// F10.1: BossArenaState — domain-boss arena cycle
// ═══════════════════════════════════════════════════════════════
enum class BossArenaState {
    INTRO,            // 开场 → 进入领域
    DOMAIN_PHASE,     // Boss 无敌, 领域规则激活, 玩家解机制
    MECHANIC_PHASE,   // 机制进行中 (core spawning etc. — F10.2)
    VULNERABLE_PHASE, // 弱点暴露, 玩家输出窗口
    ENRAGED_PHASE,    // 最终狂暴
    DEAD
};
