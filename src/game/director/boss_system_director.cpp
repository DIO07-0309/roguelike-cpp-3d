#include "boss_system_director.h"
#include "event_bus.h"
#include "monster.h"
#include "player.h"
#include "game_map.h"
#include "world_state.h"
#include "relationship_system.h"
#include "boss.h"              // BossAI, spawn_boss
#include "ai.h"               // F10.2-fix: MonsterAI (stationary weak point)
#include "data/boss_defs.h"    // F10.1: get_boss_def
#include "combat_system.h"     // rng, get_effective_max_hp
#include "systems/weapon_component.h"  // WeaponDef (for mirror weapon stages)
#include "config.h"            // F10.2: TILE_SIZE
#include "components/element_component.h" // F10.3: ElementType
#include "ai/player_behavior/player_behavior_recorder.h" // F15.3
#include "ai/mirror/mirror_memory_store.h"  // B3-M: 克隆表跨局闭环
#include "meta_progression.h"               // B3-M: g_readonly (sim 零接触)
#include "ai/player_behavior/player_behavior_analyzer.h"  // F15.3
#include "ai/mirror/mirror_agent.h"                       // F15.3
#include "ai/rl/q_agent.h"                                // v0.9.30: RL 镜像决策层
#include "core/logger.h"   // M4: [MIRROR-ACC] 战斗统计进 game.log

// ============================================================
// D6 Step3: BossSystemDirector — 组合所有Boss子系统
// ============================================================

void BossSystemDirector::reset_floor() {
    evolution = BossEvolutionData{};
    behavior = BossBehaviorState{};
    skill_queue = BossSkillQueue{};
    arena.clear();
    encounter = BossEncounterController{};
    replay_mem = BossReplayMemory{};
    battle_report = BossBattleReport{};
    cinematic = BossCinematicController{};
    timeline.clear();
    current_cmd = BossCommand::NONE;
    modifier_hook = BossModifierHook{};
    intro_text.clear();
    modifier_text.clear();
    dmg_done = 0; dmg_taken = 0;
    arena_state = BossArenaState::INTRO;
    domain_timer = 0.0f;
    boss_invulnerable = false;
    domain_cycle_count = 0;
    _behavior_type.clear();
    _active_core = nullptr;              // F10.2-fix: 跨楼层重置弱点点指针
    _weak_point_pool = nullptr;
    _vulnerable_duration = 10.0f;        // F10.3-fix
    _weakness_dmg_mult = 1.0f;           // F10.3-fix
    _enraged = false;                    // M4d-fix: 狂暴领域标记
    _arena_spawn_timer = 0.0f;   // G2.3
    _arena_cfg = nullptr;        // G2.3
    _arena_phase = 0;            // M4b
    _arena_spawn_mult = 1.0f;    // M4b
    // Q3.9: 镜像冻结计时归零 — F15 冻结中死亡后, 无 boss 的楼层 tick 不再递减 → 跨局泄漏
    _mirror_combat.reset_run();
}

void BossSystemDirector::init_on_spawn(Monster* boss, int floor,
    const WorldState& ws, BuildType bt, const RelationshipSystem& rels,
    GameMap* map, const Player* player) {
    if (!boss) return;

    // D5 Step1: BossModifier
    modifier_hook = BossNarrative::calc_modifier(floor, ws, bt, rels);
    if (modifier_hook.hp_scale != 1.0f) {
        boss->combat.max_hp = (int)(boss->combat.max_hp * modifier_hook.hp_scale);
        boss->combat.current_hp = boss->combat.max_hp;
    }
    if (modifier_hook.damage_scale != 1.0f)
        boss->combat.attack = (int)(boss->combat.attack * modifier_hook.damage_scale);

    // D5 Step2: Evolution
    evolution = calc_boss_evolution(floor, ws, bt, rels);
    float skill_dmg_avg = (evolution.charge_mod.damage_scale
                         + evolution.shockwave_mod.damage_scale
                         + evolution.summon_mod.damage_scale) / 3.0f;
    if (skill_dmg_avg > 1.0f || skill_dmg_avg < 1.0f)
        boss->combat.attack = (int)(boss->combat.attack * skill_dmg_avg);

    if (modifier_hook.disable_phase) {
        auto* bai = dynamic_cast<BossAI*>(boss->ai);
        if (bai) bai->is_enraged = true;
    }
    if (modifier_hook.add_minions && map) {
        if (rng() % 100 < 80) {
            auto [etx, ety] = map->pixel_to_tile(
                boss->entity.position.x + 32, boss->entity.position.y);
            if (map->is_walkable(etx, ety)) {
                // Note: caller must add to monsters vector — we just signal via the hook
                // The actual add_minions is handled in game_scene (monsters.emplace_back)
                modifier_hook.add_minions = true;  // flag for caller
            } else modifier_hook.add_minions = false;
        } else modifier_hook.add_minions = false;
    }

    modifier_text = modifier_hook.modifier_text ? modifier_hook.modifier_text : "";

    // D5 Step3: Behavior
    behavior = BossBehaviorState{};
    behavior.personality = boss_personality_for_floor(floor);

    // D5 Step5: Encounter
    encounter.start(floor);

    // D5 Step6: Cinematic + Timeline
    cinematic.start_intro();
    timeline.clear();
    timeline.record(0, "INTRO");

    // Replay memory
    replay_mem = BossReplayMemory{};
    replay_mem.build = bt;
    replay_mem.blood_ritual = ws.has(WorldFlag::Blood_Ritual);
    replay_mem.curse = ws.has(WorldFlag::Accepted_Curse);
    replay_mem.saved_priest = ws.has(WorldFlag::Saved_Priest);

    dmg_done = 0; dmg_taken = 0;

    // G2.3: Arena 配置从 BossAI 读取 (boss_factory_create 已设置)
    if (auto* bai = dynamic_cast<BossAI*>(boss->ai))
        _arena_cfg = bai->_arena_cfg;
    else
        _arena_cfg = nullptr;

    // D5 Step2: Apply cooldown/area modifiers to BossAI
    if (auto* bai = dynamic_cast<BossAI*>(boss->ai)) {
        bai->_charge->cooldown    *= evolution.charge_mod.cooldown_scale;
        bai->_shockwave->cooldown *= evolution.shockwave_mod.cooldown_scale;
        bai->_summon->cooldown    *= evolution.summon_mod.cooldown_scale;
        bai->_charge->fx_radius   *= evolution.charge_mod.area_scale
                                      + evolution.charge_mod.damage_scale - 1.0f;
        bai->_shockwave->fx_radius *= evolution.shockwave_mod.area_scale;
        if (evolution.summon_mod.double_strike)
            bai->_summon->cooldown *= 0.5f;
    }

    // Build combined modifier text for intro
    if (!modifier_text.empty() && evolution.evolution_name) {
        modifier_text = modifier_text + " [" + evolution.evolution_name + "]";
    }

    // F10.1: Read behavior_type + domain_config from BossDef (data-driven)
    if (auto* bai = dynamic_cast<BossAI*>(boss->ai)) {
        const BossDef* def = get_boss_def(bai->_boss_id ? bai->_boss_id : "");
        if (def && !def->behavior_type.empty()) {
            _behavior_type = def->behavior_type;
            if (_behavior_type == "domain") {
                domain_cycle_duration = def->domain_config.cycle_time;
                _vulnerable_dmg_mult  = def->domain_config.damage_multiplier;
                _vulnerable_duration  = def->domain_config.vulnerable_duration;
                _mechanic_duration    = def->domain_config.mechanic_duration;  // M4b
                // F10.3-fix: 数据驱动弱点元素 — 克制元素提升对核心伤害
                _weakness_dmg_mult = 1.0f;
                if (player && player->element.initialized) {
                    ElementType we = element_from_string(
                        def->domain_config.weakness_element.c_str());
                    if (we != ElementType::NONE && we == player->element.type)
                        _weakness_dmg_mult = 1.0f
                            + def->domain_config.weakness_bonus;
                }
            } else if (_behavior_type == "mirror") {
                _init_mirror_boss(boss, player);
            }
        }
    }
}

void BossSystemDirector::_init_mirror_boss(Monster* boss, const Player* player) {
    if (!boss || !player) return;
    // Analyze player behavior
    const auto& history = g_behavior.history();
    if (history.empty()) return;

    PlayerHabitProfile profile = PlayerBehaviorAnalyzer::analyze(history);
    _mirror_agent = std::make_unique<MirrorAgent>();
    _mirror_agent->init(profile);

    // M1-fix (M4 验收发现): 克隆表从未在运行时注入 — 补上真实链路:
    // F1-F14 采集流 → state→意图分布 → MirrorAgent 预测与仲裁
    auto clone = std::make_unique<BehaviorCloneTable>();
    clone->build(history);
    clone->set_profile(profile);
    // B3-M: Mirror 闭环 — 读本局之前的跨局克隆记忆 (表每次都是新建,
    // 同局重复注入天然不叠加; sim/只读模式下零接触, 保确定性红线)
    if (!MetaSystem::g_readonly &&
        mirror::MirrorMemoryStore::load_into(*clone)) {
        LOG_INFO("[MIRROR] cross-run clone memory merged");
    }
    _mirror_agent->set_clone_table(std::move(clone));
    LOG_INFO("[MIRROR] CloneTable built: %zu entries from %zu actions",
             _mirror_agent->clone_table()->entries(), history.size());

    // M4.4: 战术链序列记忆 — 与克隆表同步离线 build + 注入 (F15 enter)
    auto chain = std::make_unique<TacticalChainTable>();
    chain->build(history);
    _mirror_agent->set_chain_table(std::move(chain));
    LOG_INFO("[MIRROR] ChainTable built: %zu triples from %zu actions",
             _mirror_agent->chain_table()->entries(), history.size());

    // v0.9.30: RL 决策层 — 按玩家风格加载离线训练 Q 表 (F15.4 RL self-play 产物)
    // 文件缺失 → 不注入 (降级现有仲裁链, 安全)
    auto rl_q = std::make_unique<rl::QAgent>();
    std::string rl_path = std::string("saves/rl_mirror_q_") +
                          profile.style_name() + ".json";
    if (rl_q->load(rl_path)) {
        size_t rl_entries = rl_q->table_size();
        _mirror_agent->set_rl_policy(std::move(rl_q));
        LOG_INFO("[MIRROR] RL policy loaded: %s (%zu entries)",
                 rl_path.c_str(), rl_entries);
    } else {
        LOG_INFO("[MIRROR] RL policy missing (skip): %s", rl_path.c_str());
    }

    // ── 数值: HP=玩家×2.5, ATK=玩家×0.85 (Q3.10: 原 ×5/×1.2 → ×3/×1.0 仍不可胜,
    //   自愈 10% 太频 + 单段 135 太高 → 再削) ──
    int p_hp = get_effective_max_hp(player);
    int p_atk = player->combat.get_effective_attack();
    int p_pdef = player->combat.get_effective_defense(AttackType::PHYSICAL);
    // G10.7-fix: 数值膨胀护栏 — 镜像公式对高养成存档会把 Echo 放大成
    // 万攻战神 (实测 atk=11119 单击 3883, 任何 build 被一击秒杀)。
    // 威胁模型: 玩家应在 6-8 击内死亡 (PDEF 后 12% maxHP/击), 而非一击。
    // HP 上限 2000: 防止"玩家 HP×2.5"在高血量存档下变成血条刷不动。
    const int kMirrorHpCap = 2000;
    int hp_target = (int)(p_hp * 2.5f);
    if (hp_target > kMirrorHpCap) hp_target = kMirrorHpCap;
    if (hp_target < 200) hp_target = 200;          // 下限: 不让小号把 Echo 削成纸
    boss->combat.max_hp   = hp_target;
    boss->combat.current_hp = boss->combat.max_hp;
    // ATK = max(玩家ATK×0.85, PDEF×0.85) — 但不得超过 6击致死线 (12% maxHP 经防前 raw)
    int min_atk = (int)(p_pdef * 0.85f);
    int atk_target = std::max((int)(p_atk * 0.85f), min_atk);
    int atk_cap = std::max(20, p_hp / 8);          // raw 上限 ≈ 玩家 8 击线
    if (atk_target > atk_cap) atk_target = atk_cap;
    boss->combat.attack   = atk_target;
    boss->combat.physical_defense = player->combat.physical_defense + 5;
    boss->combat.magical_defense  = player->combat.magical_defense + 3;
    boss->attack_cooldown = 1.2f;  // 镜像攻击间隔

    // ── 镜像武器: 读取玩家武器类型/范围/连招 ──
    auto* bai = dynamic_cast<BossAI*>(boss->ai);
    if (!bai) return;
    bai->_is_mirror = true;
    int wtype = (int)player->weapon.weapon_type();
    bai->_mirror_weapon_type = wtype;
    const WeaponDef* wdef = player->weapon.current_def();
    if (wdef) {
        bai->_mirror_max_stages = wdef->stage_count;
        for (int i = 0; i < bai->_mirror_max_stages && i < 3; i++) {
            bai->_mirror_stage_mults[i] = wdef->stages[i].damage_multiplier;
            if (i == 0) {
                bai->_mirror_active_range = wdef->stages[i].range * TILE_SIZE;
                bai->_mirror_width = wdef->stages[i].width;
                bai->_mirror_hit_shape = (int)wdef->stages[i].hit_shape;
            }
        }
        bai->_mirror_weapon_range = bai->_mirror_active_range / TILE_SIZE;
    } else {
        // 拳头 fallback
        bai->_mirror_max_stages = 1;
        bai->_mirror_stage_mults[0] = 1.0f;
        bai->_mirror_weapon_range = 1.5f;
        bai->_mirror_active_range = 48.0f;
    }

    // ── 初始化 MirrorCombatDirector (替代旧BossAI补丁) ──
    _mirror_combat.init(player, boss, bai, _mirror_agent.get());
    // G10.7-fix: 数值护栏可观测 — 实机一查便知 Echo 最终面板
    LOG_INFO("[MIRROR] Echo stats: HP=%d ATK=%d (player HP=%d ATK=%d PDEF=%d, caps hp<=2000 atk<=HP/8)",
             boss->combat.max_hp, boss->combat.attack,
             p_hp, p_atk, p_pdef);
    _mirror_agent->begin_battle();   // 验收: 每场战斗重置 AI 调用链统计
    _mirror_stats_logged = false;

    int mirror_hp = boss->combat.max_hp;
    int mirror_atk = boss->combat.attack;
    (void)mirror_hp; (void)mirror_atk;
}

// M4e: 导出镜像跨对局记忆 (供保存)
void BossSystemDirector::export_mirror_memory(
    std::vector<float>& alpha, std::vector<float>& beta) const {
    alpha.clear();
    beta.clear();
    if (_mirror_agent) _mirror_agent->export_memory(alpha, beta);
    // B3-M: 克隆表快照回写跨局记忆 (agent 不在=本局未遇 F15, 跳过保旧记忆;
    // 仅当本局克隆表非空才落盘, 空快照=未遭遇, 不清空历史记忆)
    if (!MetaSystem::g_readonly && _mirror_agent &&
        _mirror_agent->clone_table() && _mirror_agent->clone_table()->entries()) {
        if (!mirror::MirrorMemoryStore::save_from(*_mirror_agent->clone_table()))
            LOG_WARN("B3-M: 跨局记忆落盘失败, 本局学习丢失 (旧记忆保留)");
    }
}

// M4e: 注入镜像跨对局记忆 (新对局/读档时, 叠加进先验)
void BossSystemDirector::inject_mirror_memory(
    const std::vector<float>& alpha, const std::vector<float>& beta) {
    if (_mirror_agent) _mirror_agent->import_memory(alpha, beta);
}

// M1: 学习剧场接线 — 战术播报 + 阶段晋升播报都走 GameScene 的 show_message
void BossSystemDirector::connect_mirror_theater(
    std::function<void(const char*, float)> sink) {
    if (!sink) return;
    _mirror_combat.set_callout_sink(sink);
    if (_mirror_agent)
        _mirror_agent->set_phase_sink(
            [sink](int phase, const char* reason) {
                (void)phase;
                if (reason) sink(reason, 3.5f);
            });
}

// v1.6-B1: 阶段晋升横幅通道 — 覆盖 M1 的小字播报为醒目横幅
// (connect_mirror_theater 之后调用; phase 2=镜像期 3=进化期)
void BossSystemDirector::hook_phase_banner(
    std::function<void(int, const char*)> banner) {
    if (!banner) return;
    if (_mirror_agent)
        _mirror_agent->set_phase_sink(
            [banner](int phase, const char* reason) {
                banner(phase, reason);
            });
}

void BossSystemDirector::tick(float dt, Monster* boss, Player* player, int floor,
    float game_time, const WorldState& ws, const RelationshipSystem& rels,
    StoryStage stage, std::vector<std::unique_ptr<Monster>>& monsters,
    std::vector<Effect>* effects) {
    if (!boss || !boss->is_boss) return;

    // Build context for behavior evaluation
    BossContext ctx;
    ctx.hp_pct = (float)boss->combat.current_hp / boss->combat.max_hp;
    ctx.dist_tiles = hypotf(
        boss->entity.rect.x - player->entity.rect.x,
        boss->entity.rect.y - player->entity.rect.y) / 32.0f;
    ctx.player_low_hp    = player->combat.current_hp < player->combat.max_hp * 0.3f;
    ctx.player_combo_high = player->combo.count >= 4;
    ctx.player_far       = ctx.dist_tiles > 7;
    ctx.player_near      = ctx.dist_tiles < 3;
    ctx.last_stand       = evolution.last_stand_triggered;
    ctx.build = calculate_build(player).identify();
    ctx.stage = stage;

    behavior.personality = boss_personality_for_floor(floor);
    behavior.memory.tick(dt, false);
    evaluate_boss_decision(floor, ctx, ws, rels, behavior, dt);

    // Behavior→Command
    current_cmd = boss_decision_to_command((int)behavior.current);

    // Encounter tick + M4b: 注入遭遇阶段 (驱动连招模板切换)
    encounter.tick(dt, ctx.hp_pct);
    if (auto* bai = dynamic_cast<BossAI*>(boss->ai))
        bai->set_encounter_phase(encounter.phase());

    // Cinematic tick
    cinematic.tick(dt);

    // ── G2.3 + M4b: Arena 生成 — 阶段驱动间隔加速 ──
    if (_arena_cfg && _arena_cfg->danger_type != "none") {
        _arena_spawn_timer += dt;
        float effective_interval = _arena_cfg->spawn_interval * _arena_spawn_mult;
        if (_arena_spawn_timer >= effective_interval) {
            _arena_spawn_timer = 0.0f;
            ArenaEvent ev;
            ev.type     = ArenaEventType::SPAWN_ZONE;
            ev.count    = 1;
            ev.duration = _arena_cfg->zone_duration;
            float bx = boss->entity.rect.x + boss->entity.rect.width / 2;
            float by = boss->entity.rect.y + boss->entity.rect.height / 2;
            float px = player->entity.rect.x + player->entity.rect.width / 2;
            float py = player->entity.rect.y + player->entity.rect.height / 2;
            arena.execute_event(ev, *_arena_cfg, bx, by, px, py);
        }
    }
    arena.tick(dt, player, monsters);

    // ══════════════════════════════════════════════════════
    // F10.1: Domain boss arena state machine
    // ══════════════════════════════════════════════════════
    if (_behavior_type == "domain") {
        _tick_domain_state(dt, boss);
        boss->combat.domain_invulnerable = boss_invulnerable;
    }
    // F15: Mirror boss — MirrorCombatDirector 接管全部战斗决策
    else if (_behavior_type == "mirror" && _mirror_agent) {
        // M2: 动态阶段触发 — 准确率/观察数/时间/濒危, 不再纯计时
        MirrorBattleState mst;
        mst.boss_hp_pct = boss->combat.max_hp > 0
            ? (float)boss->combat.current_hp / boss->combat.max_hp : 1.0f;
        mst.player_hp_pct = player->combat.max_hp > 0
            ? (float)player->combat.current_hp / player->combat.max_hp : 1.0f;
        _mirror_agent->tick_phase(dt, mst);
        _mirror_combat.tick(dt, boss, player, game_time, _mirror_agent.get(),
                            effects);   // F15-fix: 传递特效通道 — 镜像攻击可见
        // 验收: 战斗结束导出 AI 调用链统计 (任一阵亡, 只记一次)
        if (!_mirror_stats_logged &&
            (boss->combat.current_hp <= 0 || player->combat.current_hp <= 0)) {
            LOG_INFO("[MIRROR-ACC] battle ended — %s",
                     _mirror_agent->debug_stats()->summary().c_str());
            _mirror_stats_logged = true;
        }
    }
}

// ── F10.1: Domain state machine tick ──
void BossSystemDirector::_tick_domain_state(float dt, Monster* boss) {
    switch (arena_state) {
    case BossArenaState::INTRO:
        domain_timer += dt;
        if (domain_timer >= 1.5f) {
            domain_timer = 0.0f;
            arena_state = BossArenaState::DOMAIN_PHASE;
            boss_invulnerable = true;
            _spawn_domain_core(boss);
            EventBus::inst().emit(GameEventType::BOSS_DOMAIN_ENTER,
                boss, 0, 0.0f, "domain_enter");
        }
        break;

    case BossArenaState::ENRAGED_PHASE:   // M4d-fix: 狂暴领域 — 核心周期减半
        _tick_core_phase(dt, boss, _enraged_cycle_duration());
        break;

    case BossArenaState::DOMAIN_PHASE:
        _tick_core_phase(dt, boss, _enraged_cycle_duration());
        break;

    case BossArenaState::VULNERABLE_PHASE:
        domain_timer += dt;
        if (domain_timer >= _enraged_vulnerable_duration()) {
            domain_timer = 0.0f;
            boss->combat.vulnerable_dmg_mult = 1.0f;
            arena_state = _enraged
                ? BossArenaState::ENRAGED_PHASE : BossArenaState::DOMAIN_PHASE;
            boss_invulnerable = true;
            _spawn_domain_core(boss);
            EventBus::inst().emit(GameEventType::BOSS_DOMAIN_ENTER,
                boss, domain_cycle_count, 0.0f, "domain_enter");
        }
        break;

    case BossArenaState::MECHANIC_PHASE:   // M4b: 弹幕演出段
        _tick_mechanic_phase(dt, boss);
        break;

    default: break;
    }
}

// ── M4d-fix: 核心阶段共用逻辑 (DOMAIN/ENRAGED): 核心破坏→演出; 超时强转 ──
void BossSystemDirector::_tick_core_phase(float dt, Monster* boss,
                                          float max_duration) {
    // 核心被破坏 (普攻: 本帧检测; DOT: cleanup 前置钩子置 null) → 进入弹幕演出
    if (_active_core && !_active_core->combat.is_alive)
        _active_core = nullptr;
    if (!_active_core) {
        _enter_mechanic_phase(boss);   // M4b: 破坏→演出段
        return;
    }
    domain_timer += dt;
    if (domain_timer >= max_duration) {
        domain_timer = 0.0f;
        _active_core->combat.is_alive = false;
        _active_core = nullptr;
        _enter_vulnerable_phase(boss);
    }
}

// ── M4b: 进入弹幕演出段 (核心破坏奖励: 无敌弹幕风暴 → 易伤) ──
void BossSystemDirector::_enter_mechanic_phase(Monster* boss) {
    _active_core = nullptr;
    domain_timer = 0.0f;
    _mechanic_timer = _enraged ? _mechanic_duration * 0.5f : _mechanic_duration;
    _mechanic_barrage_timer = 0.4f;   // 首波快速起手
    arena_state = BossArenaState::MECHANIC_PHASE;
    boss_invulnerable = true;
    EventBus::inst().emit(GameEventType::WEAK_POINT_BREAK,
        boss, domain_cycle_count, 0.0f, "core_destroyed");
}

// ── M4b: 演出段 tick — 密集弹幕 + 倒计时 → 易伤 ──
void BossSystemDirector::_tick_mechanic_phase(float dt, Monster* boss) {
    _mechanic_timer -= dt;
    _mechanic_barrage_timer -= dt;
    if (_mechanic_barrage_timer <= 0.0f) {
        _force_barrage(boss);
        _mechanic_barrage_timer = 1.0f;
    }
    if (_mechanic_timer <= 0.0f)
        _enter_vulnerable_phase(boss);
}

// ── M4b: 演出弹幕 — 快速蓄力释放一波 (插入式, 不打断连招队列) ──
void BossSystemDirector::_force_barrage(Monster* boss) {
    auto* bai = dynamic_cast<BossAI*>(boss->ai);
    if (!bai || !bai->_barrage) return;
    bai->_barrage->windup_left = 0.2f;
    bai->_barrage->reset_waves();
    bai->_barrage->fired = false;
    bai->_barrage->finished = false;
    bai->boss_state = BossState::RANGED_BARRAGE;
}

// ── M4d-fix: 统一进入弱点阶段 (核心破坏/超时共用) ──
void BossSystemDirector::_enter_vulnerable_phase(Monster* boss) {
    _active_core = nullptr;
    domain_timer = 0.0f;
    arena_state = BossArenaState::VULNERABLE_PHASE;
    boss_invulnerable = false;
    boss->combat.vulnerable_dmg_mult = _vulnerable_dmg_mult;
    domain_cycle_count++;
    EventBus::inst().emit(GameEventType::BOSS_VULNERABLE_ENTER,
        boss, domain_cycle_count, _vulnerable_dmg_mult, "vulnerable_enter");
}

float BossSystemDirector::_enraged_cycle_duration() const {
    return domain_cycle_duration * (_enraged ? 0.5f : 1.0f);
}

float BossSystemDirector::_enraged_vulnerable_duration() const {
    return _vulnerable_duration * (_enraged ? 0.5f : 1.0f);
}

// ── F10.2: Spawn a domain core (weak point) near the boss ──
void BossSystemDirector::_spawn_domain_core(Monster* boss) {
    if (!boss || !_weak_point_pool) return;  // _weak_point_pool is set by caller
    float cx = boss->entity.rect.x + boss->entity.rect.width / 2;
    float cy = boss->entity.rect.y + boss->entity.rect.height / 2;
    float dist = 80.0f + (float)(rng() % 40);
    float angle = ((float)(rng() % 360)) * 3.14159f / 180.0f;
    float sx = cx + cosf(angle) * dist;
    float sy = cy + sinf(angle) * dist;

    // F10.3-fix: 数据驱动弱点加成 — 克制元素提升核心受击伤害 (CombatStats 乘区)
    // F10.2-fix: 惰性AI (静止桩) — 不追逐玩家、无普攻
    auto* core = new Monster(sx, sy, "火焰核心", 200, 0, 8, 4,
        Color{255, 120, 30, 255}, new MonsterAI(0.0f, 0.0f, 0.0f, 0.0f));
    core->combat.vulnerable_dmg_mult = _weakness_dmg_mult;
    core->attack_cooldown = 999999.0f;   // 静止桩: 永不普攻
    core->is_weak_point = true;
    core->weak_point_type = (int)WeakPointType::CORE;
    core->weak_point_state = (int)WeakPointState::ACTIVE; // F10.3
    core->_weak_point_owner = boss;
    core->entity.size = {28, 28};
    core->entity.sync_rect();
    _active_core = core;
    _weak_point_pool->push_back(std::unique_ptr<Monster>(core));
}

// ── F10.2-fix: UAF guard — 核心死于 DOT/延迟结算时, cleanup 前解除引用 ──
void BossSystemDirector::on_core_maybe_erased() {
    if (_active_core && !_active_core->combat.is_alive)
        _active_core = nullptr;   // cleanup 将 erase 该对象, 状态转换由下一帧 tick 完成
}

void BossSystemDirector::notify_phase2() {
    cinematic.trigger_phase2();
    timeline.record(encounter.total_time(), "PHASE2");
    // M4b: Phase2 arena — 生成间隔减半, 战场压力升级
    _arena_phase = 1;
    _arena_spawn_mult = 0.5f;
}

void BossSystemDirector::notify_last_stand(Monster* boss) {
    evolution.last_stand_triggered = true;
    if (auto* bai = dynamic_cast<BossAI*>(boss->ai)) {
        // M4d: LastStand = faster skills + bigger AOEs, NOT more ATK (damage already high from Phase2)
        bai->_charge->cooldown    *= 0.55f;
        bai->_shockwave->cooldown *= 0.55f;
        bai->_summon->cooldown    *= 0.55f;
        bai->_shockwave->fx_radius *= 1.6f;
        bai->_charge->fx_radius   *= 1.4f;
    }
    cinematic.trigger_last_stand();
    timeline.record(encounter.total_time(), "LAST_STAND");
    // M4b: LastStand arena — INTENSIFY 重置所有zone + 间隔再加速
    _arena_phase = 2;
    _arena_spawn_mult = 0.35f;
    if (_arena_cfg && _arena_cfg->danger_type != "none") {
        ArenaEvent ev;
        ev.type     = ArenaEventType::INTENSIFY;
        ev.duration = _arena_cfg->zone_duration;  // 重置为满 duration
        arena.execute_event(ev, *_arena_cfg, 0, 0, 0, 0);
    }
    // M4d-fix: 狂暴领域 — domain boss 进入 ENRAGED_PHASE (攻击+30%, 周期/弱点减半)
    if (_behavior_type == "domain" && boss) {
        _enraged = true;
        boss->combat.attack = (int)(boss->combat.attack * 1.3f);
        if (_active_core) { _active_core->combat.is_alive = false; }
        _active_core = nullptr;
        boss->combat.vulnerable_dmg_mult = 1.0f;
        domain_timer = 0.0f;
        arena_state = BossArenaState::ENRAGED_PHASE;
        boss_invulnerable = true;
        _spawn_domain_core(boss);
        EventBus::inst().emit(GameEventType::BOSS_DOMAIN_ENTER,
            boss, domain_cycle_count, 0.0f, "enraged_domain");
    }
}

void BossSystemDirector::notify_death(const WorldState& ws,
    const RelationshipSystem& rels, const QuestManager& qm) {
    replay_mem.survive_time = encounter.total_time();
    encounter.end(replay_mem, dmg_done, dmg_taken, (int)arena.zones().size());
    battle_report = encounter.report();
    cinematic.trigger_death();
    timeline.record(encounter.total_time(), "DEATH");
    (void)ws; (void)rels; (void)qm;
}

void BossSystemDirector::init_events() {
    EventBus::inst().subscribe(GameEventType::BOSS_DEAD,
        [this](const GameEvent& ev) { notify_death_ev(ev); }, "BossSys", this);
    EventBus::inst().subscribe(GameEventType::FLOOR_ENTER,
        [this](const GameEvent&) { reset_floor(); }, "BossSys", this);
}

void BossSystemDirector::unregister_events() {
    EventBus::inst().unsubscribe(GameEventType::BOSS_DEAD, this);
    EventBus::inst().unsubscribe(GameEventType::FLOOR_ENTER, this);
}

void BossSystemDirector::notify_death_ev(const GameEvent&) {
    // D7 Step5: reserved — Boss死亡事件处理
}
