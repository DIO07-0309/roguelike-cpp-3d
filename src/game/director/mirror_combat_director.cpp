#include "mirror_combat_director.h"
#include "monster.h"
#include "player.h"
#include "boss.h"
#include "combat_system.h"
#include "ai/mirror/mirror_agent.h"
#include "ai/player_behavior/player_habit_profile.h"
#include "data/weapon_defs.h"   // M4.3: CROSSBOW 槽查询
#include "vfx_server.h"
#include "core/logger.h"
#include "core/service_locator.h"        // G10.7-fix: 时停演出音效
#include "game/audio/audio_server.h"
#include <cmath>

MirrorCombatDirector::MirrorCombatDirector() = default;

// ── M4.1: 战术状态名 (F9 HUD 用) ──
const char* MirrorCombatDirector::tactic_name() const {
    switch (_tactic) {
    case MirrorTactic::OPEN_RANGED:  return "远程消耗";
    case MirrorTactic::ENGAGE_MELEE: return "压进近战";
    case MirrorTactic::KITE:         return "拉扯";
    default:                         return "平衡";
    }
}

// M1: 适应播报 — 玩家可读的"它在学我"话术 (战术→反制文案)
static const char* _tactic_callout(MirrorTactic t, const PlayerHabitProfile& p) {
    switch (t) {
    case MirrorTactic::KITE:
        return "回响: 「你总在被打后反击 — 我不再硬拼」";
    case MirrorTactic::OPEN_RANGED:
        return p.face_enemy_rate > 0.7f
            ? "回响: 「你从不回头 — 那我就从远处收割」" : nullptr;
    case MirrorTactic::ENGAGE_MELEE:
        return p.predict_low_dodge
            ? "回响: 「你很少闪避 — 贴身就是死局」" : nullptr;
    default:
        return nullptr;
    }
}

// ── M4.1: 画像 + 态势 → 战术状态 (带切换冷却防抖动) ──
void MirrorCombatDirector::_update_tactic(const PlayerHabitProfile& profile,
                                          float dist, float player_hp_pct) {
    if (_tactic_timer > 0) return;
    MirrorTactic next = decide_tactic(profile, dist, player_hp_pct);
    if (next != _tactic) {
        _tactic = next;
        _tactic_timer = 3.0f;   // 3s 内不再切换
        LOG_INFO("[MIRROR] 战术切换 → %s", tactic_name());
        // M1: 战术适应播报 (每个战术一次, 玩家可读)
        _emit_callout(_tactic_callout(_tactic, profile), 3.5f);
    }
}

// ── M4.1 决策纯函数 (验证: 三场景可复现单测) ──
MirrorTactic MirrorCombatDirector::decide_tactic(
    const PlayerHabitProfile& profile, float dist, float player_hp_pct) {
    if (player_hp_pct < 0.30f) {
        return MirrorTactic::ENGAGE_MELEE;              // 低血终结
    }
    if (profile.fight_back_rate > 0.6f) {
        return MirrorTactic::KITE;                      // 受压反击型
    }
    if (profile.fight_back_rate < 0.3f
        && profile.face_enemy_rate > 0.7f) {
        return MirrorTactic::OPEN_RANGED;               // 怂+单向癖
    }
    if (profile.face_enemy_rate > 0.0f
        && profile.face_enemy_rate < 0.35f) {
        return MirrorTactic::ENGAGE_MELEE;              // 四面转
    }
    if (profile.style == PlayerStyle::SNIPER
        || profile.average_distance > 260.0f) {
        return (dist < 4.0f * 32.0f) ? MirrorTactic::KITE
                                     : MirrorTactic::OPEN_RANGED;
    }
    if (profile.aggression_score > 0.55f
        || profile.predict_low_dodge) {
        return MirrorTactic::ENGAGE_MELEE;
    }
    return MirrorTactic::ADAPTIVE;   // 无匹配 → 平衡 (与原行为一致)
}

// ── M4.1: 按战术挑选技能 (替代循环轮转: 远程消耗优先弹幕/AOE, 压进优先近战/时停) ──
int MirrorCombatDirector::_pick_skill_for_tactic() const {
    if (_skills.empty()) return -1;
    int best = _skill_idx;
    for (size_t i = 0; i < _skills.size(); i++) {
        const auto& ms = _skills[i];
        bool fits = false;
        switch (_tactic) {
        case MirrorTactic::OPEN_RANGED:
            fits = (ms.skill_type == 1 || ms.skill_type == 3);
            break;
        case MirrorTactic::ENGAGE_MELEE:
            fits = (ms.skill_type == 0 || ms.skill_type == 4);
            break;
        case MirrorTactic::KITE:
            fits = (ms.skill_type == 1 || ms.skill_type == 2);
            break;
        default:
            fits = true;
        }
        if (fits) { best = (int)i; break; }
    }
    return best;
}

void MirrorCombatDirector::_tick_tactic_timer(float dt) {
    if (_tactic_timer > 0) _tactic_timer -= dt;
}

// ── M4.3: 武器槽切换 ──
void MirrorCombatDirector::_switch_to_slot(const MirrorWeaponSlot& slot) {
    _total_stages = slot.total_stages;
    _attack_cd = slot.cd;
    for (int i = 0; i < 3; i++) {
        _stage_mults[i]  = slot.mults[i];
        _stage_ranges[i] = slot.ranges[i];
    }
    _combo_stage = 0;   // 换武器重置连招
    LOG_INFO("[MIRROR] 武器切换 → %s", weapon_type_name((WeaponType)slot.type));
}

// 战术 → 期望武器族, 切换冷却 2.5s 防抖 (独立于战术冷却)
void MirrorCombatDirector::_sync_weapon_slot() {
    if (_weapon_switch_timer > 0) return;
    bool want_ranged = (_tactic == MirrorTactic::OPEN_RANGED || _tactic == MirrorTactic::KITE);
    if (want_ranged != _slot_is_ranged) {
        _slot_is_ranged = want_ranged;
        _weapon_switch_timer = 2.5f;
        _switch_to_slot(want_ranged ? _slot_ranged : _slot_melee);
    }
}

const char* MirrorCombatDirector::active_weapon_name() const {
    return weapon_type_name((WeaponType)(_slot_is_ranged ? _slot_ranged.type : _slot_melee.type));
}

// ── G52: init 子步骤 ──

// 技能类型从名字识别 (中文/英文双语关键字)。返回 skill_type,
// 并通过引用回填 range / dmg_mult。
// 顺序即优先级: 自愈 > 时停 > 斩 > 罚 > 冰 > 影 > 血 > 召唤 > 默认
static int mirror_classify_skill(const std::string& name, float& range,
                                 float& dmg_mult) {
    if (name.find("愈") != std::string::npos ||
        name.find("Heal") != std::string::npos ||
        name.find("回") != std::string::npos) {
        range = 0;      dmg_mult = 1.0f;  return 2;   // self_heal
    }
    if (name.find("时停") != std::string::npos ||
        name.find("World") != std::string::npos) {
        range = 999;    dmg_mult = 0;     return 4;   // time_stop
    }
    if (name.find("斩") != std::string::npos ||
        name.find("Slash") != std::string::npos) {
        range = 96;     dmg_mult = 1.5f;  return 0;   // melee
    }
    if (name.find("罚") != std::string::npos ||
        name.find("Wrath") != std::string::npos) {
        range = 140;    dmg_mult = 0.8f;  return 3;   // aoe
    }
    if (name.find("冰") != std::string::npos ||
        name.find("Ice") != std::string::npos) {
        range = 200;    dmg_mult = 1.2f;  return 1;   // projectile
    }
    if (name.find("影") != std::string::npos ||
        name.find("Shadow") != std::string::npos) {
        range = 80;     dmg_mult = 2.0f;  return 0;   // melee burst
    }
    if (name.find("血") != std::string::npos ||
        name.find("Blood") != std::string::npos) {
        range = 0;      dmg_mult = 1.3f;  return 2;   // self_buff+heal
    }
    if (name.find("召唤") != std::string::npos ||
        name.find("Summon") != std::string::npos) {
        range = 160;    dmg_mult = 0.6f;  return 3;   // aoe summon
    }
    range = 120;        dmg_mult = 1.0f;  return 1;   // projectile default
}

// M1 "它真的在学我": 开局本命技 + 节奏模仿
static void mirror_init_learning(std::vector<MirrorSkillDef>& skills,
                                 int& skill_idx, float& skill_cd_timer,
                                 float& slot_melee_cd, MirrorAgent* agent) {
    if (!agent) return;
    const auto& prof = agent->profile();
    // 1) 最爱技能开局: Echo 首技 = 玩家本命技能 (面板承诺真上场)。
    //    skill_preference[0..3] 与玩家 skills 顺序对齐; 找最大偏好对应槽位。
    //    自奶技能 (type 2) 不作开场表演。
    int fav = prof.predicted_fav_skill;
    if (fav >= 0 && fav < (int)skills.size()
        && skills[fav].skill_type != 2) {
        skill_idx = fav;
        skill_cd_timer = 1.5f;   // 开场 1.5s 后甩出本命技
        LOG_INFO("[MIRROR] 开局本命技: %s (偏好%.0f%%)",
                 skills[fav].name.c_str(),
                 prof.skill_preference[fav] * 100.0f);
    }
    // 2) 节奏模仿: 攻击 CD 从玩家 attack_frequency 派生 (乱拳玩家→乱拳镜像)
    if (prof.attack_frequency > 0.1f) {
        float cd = 1.0f / prof.attack_frequency;
        slot_melee_cd = (cd < 0.6f) ? 0.6f : (cd > 1.6f) ? 1.6f : cd;
    }
}

void MirrorCombatDirector::init(const Player* player, Monster* boss,
                                 BossAI* bai, MirrorAgent* agent) {
    if (!player || !boss || !bai) return;
    _boss = boss; _bai = bai;
    _agent = agent;   // M4e: 在线自适应反馈通道
    _combo_stage = 0; _combo_timer = 0; _last_attack = -99.0f;
    _skill_idx = 0; _skill_cd_timer = 0;
    _behavior_state = 0; _decision_timer = 0;
    _preferred_dist = agent ? agent->recommend_distance() : 200.0f;

    // 复制武器3段数据
    const WeaponDef* wdef = player->weapon.current_def();
    _total_stages = wdef ? wdef->stage_count : 1;
    for (int i = 0; i < _total_stages && i < 3; i++) {
        if (wdef) {
            _stage_mults[i]  = wdef->stages[i].damage_multiplier;
            // G10.7-fix: Echo 近战射程钳 96px — 镜像 Boss 不得复制玩家长枪
            // 224px 矩形判定在屏幕上形成"超视距瞬杀"体感 (用户实测反馈)
            float mr = wdef->stages[i].range * 32.0f;
            _stage_ranges[i] = (mr > 96.0f) ? 96.0f : mr;
        } else {
            _stage_mults[i]  = 1.0f;
            _stage_ranges[i] = 48.0f;  // 拳头范围
        }
    }

    // ── M4.3: 武器槽初始化 — 近战=玩家武器, 远程=CROSSBOW ──
    _slot_melee.type   = (int)(wdef ? wdef->type : WeaponType::FIST);
    _slot_melee.total_stages = _total_stages;
    _slot_melee.cd = 1.2f;
    for (int i = 0; i < 3; i++) {
        _slot_melee.mults[i]  = _stage_mults[i];
        _slot_melee.ranges[i] = _stage_ranges[i];
    }
    const auto crossbows = get_weapon_defs_for_type(WeaponType::CROSSBOW);
    const WeaponDef* bow = crossbows.empty() ? nullptr : crossbows[0];
    _slot_ranged.type = (int)WeaponType::CROSSBOW;
    _slot_ranged.total_stages = bow ? bow->stage_count : 1;
    _slot_ranged.cd = 1.4f;   // 远程射速略慢
    for (int i = 0; i < 3; i++) {
        if (bow && i < bow->stage_count) {
            _slot_ranged.mults[i]  = bow->stages[i].damage_multiplier * 0.8f; // 远程略弱
            // G10.7-fix: 弩射程钳 220px (~7 tile 屏内可辨) — 原值 320-384px
            // 屏幕边缘外/刚入视野即三连发 = "超视距瞬间杀" (满血即杀体感)
            float br = bow->stages[i].range * 32.0f;
            _slot_ranged.ranges[i] = (br > 220.0f) ? 220.0f : br;
        } else {
            _slot_ranged.mults[i]  = 0.8f;
            _slot_ranged.ranges[i] = 220.0f;
        }
    }
    _slot_is_ranged = false;

    // 复制技能数据 (类型由名字识别)
    _skills.clear();
    for (auto& sk : player->skills.active_skills) {
        MirrorSkillDef ms;
        ms.name = sk->name;
        ms.cooldown = sk->cooldown;
        ms.last_used = -99.0f;
        ms.skill_type = mirror_classify_skill(sk->name, ms.range, ms.dmg_mult);
        _skills.push_back(ms);
    }
    _attack_cd = 1.2f;

    // ── M1: "它真的在学我" ──
    mirror_init_learning(_skills, _skill_idx, _skill_cd_timer,
                        _slot_melee.cd, agent);

    LOG_INFO("[MIRROR] Combat init: stages=%d skills=%d", _total_stages, (int)_skills.size());
}

// ============================================================
// 主循环 — 每帧由 BossSystemDirector::tick 调用
// ============================================================
// ── G53: tick 拆出的四态行为机 ──────────────────────────────

// state 0: approach — 追击到首选距离, 到位则转 attack
void MirrorCombatDirector::_tick_behavior_approach(
    Monster* boss, Player* player, float dt, float dist) {
    if (dist > _preferred_dist * 1.2f) {
        _chase_player(boss, player, dt);
    } else {
        _behavior_state = 1;  // 进入攻击
    }
}

// state 1: attack — 武器连招; 每2次攻击后按冷却决定转 skill 还是回 approach
void MirrorCombatDirector::_tick_behavior_attack(
    Monster* boss, Player* player, double game_time, std::vector<Effect>* effects) {
    _weapon_attack(boss, player, game_time, effects);
    if (_combo_stage == 0) _behavior_state = (_skill_cd_timer <= 0) ? 2 : 0;
}

// state 2: skill — 冷却到点且技能非空时按战术挑选 (替代循环轮转)
void MirrorCombatDirector::_tick_behavior_skill(
    Monster* boss, Player* player, double game_time, std::vector<Effect>* effects) {
    if (_skill_cd_timer > 0 || _skills.empty()) {
        _behavior_state = 0;
        return;
    }
    int pick = _pick_skill_for_tactic();
    if (pick < 0) pick = (_skill_idx + 1) % (int)_skills.size();
    _skill_idx = pick;
    _mirror_skill(boss, player, game_time, _skill_idx, effects);
    _skill_cd_timer = _skills[_skill_idx].cooldown;
    _behavior_state = 0;
}

// state 3: retreat — 后撤到首选距离的 0.6 倍以下
void MirrorCombatDirector::_tick_behavior_retreat(
    Monster* boss, Player* player, float dt, float dist) {
    if (dist >= _preferred_dist * 0.6f) {
        _behavior_state = 0;
        return;
    }
    float dx = boss->entity.rect.x - player->entity.rect.x;
    float dy = boss->entity.rect.y - player->entity.rect.y;
    float len = hypotf(dx, dy);
    if (len > 1) boss->entity.position.x += dx / len * 100.0f * dt;
    if (len > 1) boss->entity.position.y += dy / len * 100.0f * dt;
    boss->entity.sync_rect();
}

bool MirrorCombatDirector::tick(float dt, Monster* boss, Player* player,
                                 double game_time, MirrorAgent* agent,
                                 std::vector<Effect>* effects) {
    if (!boss || !player) return false;

    float dist = hypotf(
        player->entity.rect.x - boss->entity.rect.x,
        player->entity.rect.y - boss->entity.rect.y);

    // M4.1: 战术状态机 — 画像 + 态势驱动 (带切换冷却)
    if (_agent) {
        float php = player->combat.max_hp > 0
            ? (float)player->combat.current_hp / player->combat.max_hp : 0.0f;
        _update_tactic(_agent->profile(), dist, php);
        _tick_tactic_timer(dt);
        // M4.3: 战术 → 武器槽同步 (2.5s 防抖)
        _sync_weapon_slot();
    }

    // M4e: 玩家闪避检测 (单帧位移>200px) → 最近决策视为落空
    float pdx = player->entity.rect.x - _last_player_x;
    float pdy = player->entity.rect.y - _last_player_y;
    if (_agent && (pdx * pdx + pdy * pdy) > 200.0f * 200.0f)
        _agent->report_outcome(false, 0.0f);   // 已反馈过则内部自动跳过
    _last_player_x = player->entity.rect.x;
    _last_player_y = player->entity.rect.y;

    // M2: 玩家实际动作识别 → 在线准确率反馈
    if (_agent) {
        PlayerActionType actual = _detect_player_action(player, game_time,
                                                        pdx, pdy, &_last_player_hp);
        if (actual != PlayerActionType::NONE) _agent->observe_actual(actual);
    }

    // 每0.5s做一次AI决策
    _decision_timer += dt;
    if (_decision_timer >= 0.5f && agent) {
        _decision_timer = 0.0f;
        _ai_decide(boss, player, game_time, agent);
    }

    // 技能冷却 / 冻结 / 武器切换 三个独立计时器
    if (_skill_cd_timer > 0) _skill_cd_timer -= dt;
    if (_freeze_timer > 0) _freeze_timer -= dt;
    if (_weapon_switch_timer > 0) _weapon_switch_timer -= dt;

    switch (_behavior_state) {
    case 0: _tick_behavior_approach(boss, player, dt, dist); break;
    case 1: _tick_behavior_attack(boss, player, game_time, effects); break;
    case 2: _tick_behavior_skill(boss, player, game_time, effects); break;
    case 3: _tick_behavior_retreat(boss, player, dt, dist); break;
    }
    return true;
}

// ============================================================
// 武器连招 — 模拟玩家3段攻击
// ============================================================
void MirrorCombatDirector::_weapon_attack(Monster* boss, Player* player,
                                           double gt, std::vector<Effect>* effects) {
    if (gt - _last_attack < _attack_cd) return;
    float dist = hypotf(
        player->entity.rect.x - boss->entity.rect.x,
        player->entity.rect.y - boss->entity.rect.y);
    int stage = _combo_stage;
    float range_px = _stage_ranges[stage];

    if (dist > range_px) { _behavior_state = 0; return; }

    float mult = _stage_mults[stage];
    int raw = (int)(boss->combat.attack * mult * _aggression_bonus);
    // G10.7-fix: Echo 时停期间伤害减半 — 原设计冻结玩家1.5s且自身可输出,
    // 弩三连+技能连发在同一冻结窗口叠加 = "满血瞬间杀"体感 (Q3.12 平衡红线)
    if (_freeze_timer > 0.0f) raw /= 2;
    int dmg = calculate_damage(raw,
        player->combat.get_effective_defense(AttackType::PHYSICAL));
    player->combat.take_damage(dmg);
    if (dmg > 0) {
        player->combat.mark_damage_logged();
        LOG_INFO("[DMG] Echo镜像[%d段] atk=%d mult=%.2f raw=%d → %d 伤害",
            stage + 1, boss->combat.attack, mult * _aggression_bonus, raw, dmg);
    }
    // M4e: 决策结果反馈 (命中/被防)
    if (_agent) _agent->report_outcome(dmg > 0, (float)dmg);
    _last_attack = (float)gt;
    _combo_stage++;
    bool combo_end = (_combo_stage >= _total_stages);
    if (combo_end) {
        _combo_stage = 0;
        _last_attack += 0.4f;  // 收招硬直
    }

    // VFX
    if (effects) {
        Effect ef;
        ef.kind = "pulse"; ef.duration = 0.25f; ef.elapsed = 0;
        ef.world_x = player->entity.rect.x + player->entity.rect.width / 2;
        ef.world_y = player->entity.rect.y + player->entity.rect.height / 2;
        ef.radius = combo_end ? 40.0f : 24.0f;
        ef.color = combo_end ? Color{200, 50, 40, 180} : Color{160, 80, 60, 140};
        effects->push_back(ef);
    }
}

// ============================================================
// 镜像技能 — 实际效果的Boss版本
// ── G52: _mirror_skill 按技能类型拆为 5 个助手 (原 105 行) ──
// 每个助手负责: 射程判定 → 伤害计算 (时停窗口减半) → 应用 → 日志 → agent
// 反馈。agent 的成败/数值语义必须与原逐 case 完全一致 — 它是在线自适应
// 强化的判断依据, 改错会让 Echo 学错方向且不报错。

// 近战 (type 0): 单体 + 物理防御
void MirrorCombatDirector::mirror_skill_melee( Player* player,
                               const MirrorSkillDef& ms, float dist, int atk) {
    if (dist > ms.range) { _agent->report_outcome(false, 0.0f); return; }
    int raw = (int)(atk * ms.dmg_mult * _aggression_bonus);
    if (_freeze_timer > 0.0f) raw /= 2;   // G10.7-fix: 时停窗口伤害减半
    int dmg = calculate_damage(raw,
        player->combat.get_effective_defense(AttackType::PHYSICAL));
    player->combat.take_damage(dmg);
    LOG_INFO("[MIRROR] 镜像近战 [%s]: raw=%d → %d dmg", ms.name.c_str(), raw, dmg);
    if (dmg > 0) player->combat.mark_damage_logged();
    _agent->report_outcome(dmg > 0, (float)dmg);
}

// 弹幕 (type 1): 三连发, 每发独立过魔防
void MirrorCombatDirector::mirror_skill_projectile( Player* player,
                                    const MirrorSkillDef& ms, float dist,
                                    int atk) {
    if (dist > ms.range) { _agent->report_outcome(false, 0.0f); return; }
    int total_dmg = 0;
    for (int i = 0; i < 3; i++) {
        int raw = (int)(atk * ms.dmg_mult * 0.6f);
        if (_freeze_timer > 0.0f) raw /= 2;
        int dmg = calculate_damage(raw,
            player->combat.get_effective_defense(AttackType::MAGICAL),
            AttackType::MAGICAL);
        player->combat.take_damage(dmg);
        total_dmg += dmg;
        if (dmg > 0) player->combat.mark_damage_logged();
    }
    LOG_INFO("[MIRROR] 镜像弹幕 [%s]: 3发", ms.name.c_str());
    _agent->report_outcome(total_dmg > 0, (float)total_dmg);
}

// 自愈 (type 2): 真回血 (Q3.10: max_hp/15 ≈ 6.7%)
void MirrorCombatDirector::mirror_skill_heal( Monster* boss,
                              const MirrorSkillDef& ms) {
    int heal_amt = boss->combat.max_hp / 15;
    boss->combat.heal(heal_amt);
    LOG_INFO("[MIRROR] 镜像自愈 [%s]: +%d HP", ms.name.c_str(), heal_amt);
    _agent->report_outcome(true, 0.0f);   // 验收: 执行成功正反馈
}

// AOE (type 3): 范围魔伤
void MirrorCombatDirector::mirror_skill_aoe( Player* player,
                             const MirrorSkillDef& ms, float dist, int atk) {
    if (dist > ms.range) { _agent->report_outcome(false, 0.0f); return; }
    int raw = (int)(atk * ms.dmg_mult * _aggression_bonus);
    if (_freeze_timer > 0.0f) raw /= 2;   // G10.7-fix: 时停窗口伤害减半
    int dmg = calculate_damage(raw,
        player->combat.get_effective_defense(AttackType::MAGICAL),
        AttackType::MAGICAL);
    player->combat.take_damage(dmg);
    LOG_INFO("[MIRROR] 镜像AOE [%s]: raw=%d → %d dmg", ms.name.c_str(), raw, dmg);
    if (dmg > 0) player->combat.mark_damage_logged();
    _agent->report_outcome(dmg > 0, (float)dmg);
}

// 时停 (type 4): Phase<2 仅减速, Phase≥2 真冻结 + 三重环演出
void MirrorCombatDirector::mirror_skill_time_stop( Monster* boss,
                                   Player* player, const MirrorSkillDef& ms,
                                   std::vector<Effect>* effects) {
    if (!_agent || _agent->current_phase() < 2) {
        apply_buff(player, "slow", 4);
        LOG_INFO("[MIRROR] 镜像时停 [%s]: 观察期仅减速", ms.name.c_str());
        return;
    }
    _freeze_timer = 1.5f;   // Q3.10: 冻结 3s→1.5s (原 3s 占战斗一半时长,
                                  // 免费输出窗口过大)
    LOG_INFO("[MIRROR] 镜像时停 [%s]: 玩家冻结", ms.name.c_str());
    // G10.7-fix: 发动演出 — 玩家必须"看到时停全过程"而非只承受结果
    if (effects) {
        float bx = boss->entity.rect.x + boss->entity.rect.width / 2;
        float by = boss->entity.rect.y + boss->entity.rect.height / 2;
        for (int ring_i = 0; ring_i < 3; ring_i++) {
            Effect ef;
            ef.kind = "ring";
            ef.world_x = bx; ef.world_y = by;
            ef.radius = 60.0f + ring_i * 50.0f;
            ef.duration = 0.5f; ef.start_delay = ring_i * 0.12f;
            ef.color = Color{130, 80, 220, 200};
            effects->push_back(ef);
        }
    }
    if (auto* audio = ServiceLocator::get<AudioServer>())
        audio->play_sfx("timestop", 0.9f);   // 玩家自己的时停音效 — 听感同源
}

void MirrorCombatDirector::_mirror_skill(Monster* boss, Player* player,
                                          double gt, int idx,
                                          std::vector<Effect>* effects) {
    if (idx < 0 || idx >= (int)_skills.size()) return;
    auto& ms = _skills[idx];
    ms.last_used = (float)gt;
    const float dist = hypotf(
        player->entity.rect.x - boss->entity.rect.x,
        player->entity.rect.y - boss->entity.rect.y);
    const int atk = boss->combat.attack;

    switch (ms.skill_type) {
    case 0: mirror_skill_melee(player, ms, dist, atk); break;
    case 1: mirror_skill_projectile(player, ms, dist, atk); break;
    case 2: mirror_skill_heal(boss, ms); break;
    case 3: mirror_skill_aoe(player, ms, dist, atk); break;
    case 4: mirror_skill_time_stop(boss, player, ms, effects); break;
    default: break;
    }
}

// ============================================================
// AI决策 — MirrorAgent驱动
// ============================================================
void MirrorCombatDirector::_ai_decide(Monster* boss, Player* player,
                                       double gt, MirrorAgent* agent) {
    if (!agent) return;
    MirrorBattleState st = _build_battle_state(boss, player, gt);

    if (_apply_online_action(agent->recommend_action(st))) return;

    PlayerActionType pred = agent->predict_next_action(st);
    agent->on_prediction(pred, st.dist_tiles, st.player_hp_pct,
                         st.player_skills_ready);   // M2
    agent->cache_pred_time((float)gt);              // v1.6-B1.1

    float tactic_dist = _preferred_dist;
    switch (_tactic) {
    case MirrorTactic::OPEN_RANGED: tactic_dist = 260.0f; break;
    case MirrorTactic::ENGAGE_MELEE: tactic_dist = 96.0f;  break;
    case MirrorTactic::KITE:         tactic_dist = 220.0f; break;
    default:                         tactic_dist = 200.0f; break;
    }

    _apply_pressure_close(agent, st, tactic_dist);

    if (agent->should_interrupt_skill(st)) {
        _skill_cd_timer = 0.0f;
        _behavior_state = 2;
        agent->report_interrupt(false);
    }

    agent->debug_behavior_state(_behavior_state);

    _apply_prediction_adjustment(agent, pred, st);
}
MirrorBattleState MirrorCombatDirector::_build_battle_state(Monster* boss, Player* player, double gt) {
    MirrorBattleState st;
    st.boss_hp_pct = boss->combat.max_hp > 0
? (float)boss->combat.current_hp / boss->combat.max_hp : 1.0f;
    st.player_hp_pct = player->combat.max_hp > 0
? (float)player->combat.current_hp / player->combat.max_hp : 1.0f;
    float dx = player->entity.rect.x - boss->entity.rect.x;
    float dy = player->entity.rect.y - boss->entity.rect.y;
    st.dist_tiles = hypotf(dx, dy) / 32.0f;
    st.player_attacking = (gt - player->_last_attack_time < 1.0);
    st.player_using_skill = (gt - player->_last_skill_time < 1.0);
    return st;
}

void MirrorCombatDirector::_apply_pressure_close(MirrorAgent* agent, const MirrorBattleState& st, float tactic_dist) {
    // 压力追击 (M4.1: 压进战术时探测到压力同样贴近)
    if (agent->should_pressure_close(st)) {
_preferred_dist = 80.0f;
_aggression_bonus = 1.3f;
    } else {
_preferred_dist = tactic_dist;
_aggression_bonus = 1.0f;
    }
}

void MirrorCombatDirector::_apply_prediction_adjustment(MirrorAgent* agent, PlayerActionType pred, const MirrorBattleState& st) {
    // 根据预测调整
    if (pred == PlayerActionType::HEAL) {
_skill_cd_timer *= 0.6f;  // 预测喝药→技能加速
    } else if (pred == PlayerActionType::ATTACK && agent->should_pressure_close(st)) {
_behavior_state = 3;  // 预测攻击+可压制→战略后撤
    }
}


// M2: 识别玩家本帧实际动作 — 优先级: 攻击/技能(0.5s内) > 闪避(位移>200px) > 喝药(HP上升)
PlayerActionType MirrorCombatDirector::_detect_player_action(Player* player,
                                                             double gt,
                                                             float dx, float dy,
                                                             float* last_hp) {
    if (gt - player->_last_attack_time < 0.5f) return PlayerActionType::ATTACK;
    if (gt - player->_last_skill_time  < 0.5f) return PlayerActionType::SKILL;
    if (dx * dx + dy * dy > 200.0f * 200.0f)   return PlayerActionType::DODGE;
    float hp = player->combat.current_hp;
    bool healed = (*last_hp > 0.0f) && (hp > *last_hp + 1.0f);
    *last_hp = hp;
    return healed ? PlayerActionType::HEAL : PlayerActionType::NONE;
}

// M4e: 在线决策动作映射 — Thompson 采样臂 → 行为状态
bool MirrorCombatDirector::_apply_online_action(int act) {
    if (act < 0) return false;   // 观察期: 走规则
    switch ((MirrorAction)act) {
    case MirrorAction::APPROACH: _behavior_state = 0; break;
    case MirrorAction::RETREAT:  _behavior_state = 3; break;
    case MirrorAction::SKILL:    _behavior_state = 2; _skill_cd_timer = 0.0f; break;
    case MirrorAction::COMBO:    _behavior_state = 1; break;
    }
    return true;
}

// ============================================================
// 追击移动
// ============================================================
void MirrorCombatDirector::_chase_player(Monster* boss, Player* player, float dt) {
    float dx = player->entity.rect.x - boss->entity.rect.x;
    float dy = player->entity.rect.y - boss->entity.rect.y;
    float len = hypotf(dx, dy);
    if (len < 1) return;
    float speed = boss->ai ? 120.0f : 90.0f;
    boss->entity.position.x += dx / len * speed * dt;
    boss->entity.position.y += dy / len * speed * dt;
    boss->entity.sync_rect();
}
