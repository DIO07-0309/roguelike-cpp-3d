#include "game_scene_combat.h"
#include "game_scene.h"
#include "event_bus.h"
#include "combat_system.h"
#include "growth_curve.h"
#include "relic_progression.h"
#include "floor_manager.h"
#include "scene_tree.h"
#include "core/logger.h"
#include "audio_server.h"
#include "boss_system_director.h"
#include "gameplay_system_director.h"
#include "presentation_system_director.h"
#include "build_score.h"
#include "boss_narrative.h"
#include "world_state.h"
#include "relationship_system.h"
#include "combat_feel.h"
#include "config.h"
#include "skill.h"
#include <cstdio>
#include <algorithm>

// ============================================================
// GameSceneCombat — 战斗善后
// ============================================================
void GameSceneCombat::on_monster_killed(Monster* m) {
    // P0-A: Exactly-once guard — cleanup_dead_monsters 会再次遇到已标记的怪物
    if (!m || m->kill_processed) return;
    m->kill_processed = true;
    // D7 Step5: EventBus 广播
    EventBus::inst().emit(m->is_boss ? GameEventType::BOSS_DEAD
                                      : GameEventType::MONSTER_DIED,
                           m, _s.current_floor, 0.0f, m->name.c_str());
    
    // A6-T5: 击杀顿帧 + 震动 (sim 模式跳过)
    if (!_s._sim_mode && _s._camera_def_loaded) {
        float stun_duration = _s._camera_director.fov_scale() > 0.9f ? 
                              0.08f : 0.12f;  // Boss 战期间顿帧更长
        _s._hit_stop.trigger(stun_duration);
        _s._camera_director.trigger_kill_stun();
        // 震动强度: Boss 死亡 > 精英 > 普通
        float shake = m->is_boss ? 16.0f : (m->is_elite ? 8.0f : 4.0f);
        _s._presentation.trigger_shake(shake);
        // A6-T5: Boss 死亡退出运镜状态
        if (m->is_boss) {
            _s._camera_director.exit_boss_war();
        }
    }

    // A7-T4: 怪物死亡音效 (使用现有程序合成音效作为临时替代)
    if (!_s._sim_mode && _s.get_tree()) {
        const char* death_sfx = m->is_boss ? "bolt" : (m->is_elite ? "slash" : "hit");
        _s.get_tree()->get_audio()->play_sfx(death_sfx, m->is_boss ? 0.7f : 0.5f);
    }

    // D4.6: run stats
    _s._gameplay.run_stats.total_kills++;
    if (m->is_boss) _s._gameplay.run_stats.bosses_killed++;
    if (m->is_elite) _s._gameplay.run_stats.elite_kills++;

    // XP
    int xp = m->is_boss ? (int)(XP_PER_KILL_BOSS * g_growth.exp_scale(_s.current_floor))
                        : (int)((XP_PER_KILL_BASE + (int)(m->combat.max_hp * 0.5f))
                               * g_growth.exp_scale(_s.current_floor));
    if (m->is_elite && !m->is_boss) xp = xp * 3 / 2;

    _s.player->xp += xp;
    bool leveled = false;
    while (_s.player->xp >= _s.player->xp_to_next) {
        _s.player->level++; leveled = true;
        _s.player->xp -= _s.player->xp_to_next;
        _s.player->xp_to_next = Player::calc_xp_for_level(_s.player->level + 1);
        _s.player->combat.attack += 2;   // Q3.2 平衡: +1→+2 (无构筑前期DPS不足)
        _s.player->combat.physical_defense += 1;
        _s.player->combat.magical_defense += 1;
        _s.player->combat.max_hp += 10;  // Q3.2 平衡: +7→+10
        _s.player->combat.current_hp = get_effective_max_hp(_s.player.get());
        _s.get_tree()->get_audio()->play_sfx("levelup");
        // D9: 升级反馈 — shake + freeze + message
        _s._presentation.trigger_shake(CombatFeelSystem::SHAKE_LIGHT);
        _s._presentation.trigger_freeze(CombatFeelSystem::HEAVY_HIT);
        {
            char lv_buf[64];
            snprintf(lv_buf, sizeof(lv_buf), "升级了! Lv%d", _s.player->level);
            _s._presentation.show_message(lv_buf, 1.8f);
        }
        LOG_INFO("玩家升级! Lv%d HP:%d ATK:%d", _s.player->level,
            _s.player->combat.max_hp, _s.player->combat.get_effective_attack());
        if (_s.player->skills.can_learn()) {
            auto names = get_learned_names(_s.player->skills);
            auto sk = random_active_skill(names, true);  // G10: level-up only base 4 skills
            _s.player->skills.learn(std::move(sk));
            _s.player->skills.apply_all_passives(_s.player.get());
        }
        _s.on_player_leveled.emit(_s.player->level);
    }
    if (leveled) EventBus::inst().emit(GameEventType::PLAYER_LEVEL_UP,
                                        _s.player.get(), _s.player->level);

    // Boss reward
    if (m->is_boss) {
        LOG_INFO("Boss击杀! %s - 第%d层", m->name.c_str(), _s.current_floor);
        _s._boss.notify_death(_s._gameplay.world_state, _s._gameplay.rels,
                              _s._gameplay.quest_mgr);
        _s.get_tree()->get_audio()->play_sfx("victory");
        // D9: Boss击杀强化 — shake + freeze + full flash
        _s._presentation.trigger_shake(CombatFeelSystem::SHAKE_BOSS);
        _s._presentation.trigger_freeze(CombatFeelSystem::BOSS_HIT);
        {
            BuildType bt = calculate_build(_s.player.get()).identify();
            const BossDialogue* dd = _s._boss.narrative.find_death(
                _s.current_floor, _s._gameplay.world_state, bt);
            if (dd && dd->death) {
                _s._presentation.room_msg = dd->death;
                _s._presentation.room_msg_timer = 3.0f;
            }
        }
        // World flags
        if (_s.current_floor == 5) {
            _s._gameplay.world_state.set(WorldFlag::Boss1_Defeated);
            _s._gameplay.story.boss_dead(5);
            for (auto& r : _s._gameplay.rels.all()) _s._gameplay.rels.add_respect(r.npc_id, 1);
        }
        if (_s.current_floor == 10) {
            _s._gameplay.world_state.set(WorldFlag::Boss2_Defeated);
            _s._gameplay.story.boss_dead(10);
            for (auto& r : _s._gameplay.rels.all()) _s._gameplay.rels.add_respect(r.npc_id, 1);
        }
        if (_s.current_floor == 15) {
            _s._gameplay.world_state.set(WorldFlag::Boss3_Defeated);
            if (_s._gameplay.world_state.has(WorldFlag::Boss1_Defeated)
                && _s._gameplay.world_state.has(WorldFlag::Boss2_Defeated))
                _s._gameplay.world_state.set(WorldFlag::All_Boss_Defeated);
            _s._gameplay.story.boss_dead(15);
            for (auto& r : _s._gameplay.rels.all()) _s._gameplay.rels.add_respect(r.npc_id, 2);
        }
        if (_s._gameplay.world_state.has(WorldFlag::All_Boss_Defeated)
            && !_s._gameplay.world_state.has(WorldFlag::Accepted_Curse)
            && !_s._gameplay.world_state.has(WorldFlag::Blood_Ritual)
            && !_s._gameplay.world_state.has(WorldFlag::Merchant_Killed)
            && _s._gameplay.world_state.counter("priest_trust") >= 3)
            _s._gameplay.world_state.set(WorldFlag::True_Ending_Ready);

        _s._drop_boss_reward(m);

        int heal_amt = (int)(get_effective_max_hp(_s.player.get()) * 0.30f);
        heal_player(_s.player.get(), heal_amt);

        // Boss relic
        auto all_ids = get_all_relic_ids();
        std::vector<std::string> candidates;
        for (auto& id : all_ids)
            if (!player_has_relic(_s.player.get(), id))
                candidates.push_back(id);
        if (!candidates.empty()) {
            std::string chosen = candidates[rng() % candidates.size()];
            _s.player->add_relic(chosen, PersistenceScope::RUN);
            const RelicDef* def = get_relic_def(chosen);
            if (def) {
                _s._presentation.room_msg = "RELIC:" + def->name;
                g_relic_archive.mark_obtained(chosen, rarity_level(def->rarity));
                EventBus::inst().emit(GameEventType::RELIC_GAIN, _s.player.get(),
                                       rarity_level(def->rarity), 0.0f, chosen.c_str());
            }
            _s._presentation.room_msg_timer = 3.5f;
        }
        auto extra_item = generate_random_item();
        if (extra_item) {
            auto [btx, bty] = _s.game_map->pixel_to_tile(m->entity.position.x, m->entity.position.y);
            _s.ground_items.push_back({extra_item, btx + 3, bty});
        }
    }
    // Normal loot
    else {
        float drop_chance = m->is_elite ? LOOT_DROP_CHANCE * 1.5f : LOOT_DROP_CHANCE;
        drop_chance *= g_growth.gold_scale(_s.current_floor);
        if ((float)(rng() % 1000) / 1000.0f < drop_chance) {
            auto [tx, ty] = _s.game_map->pixel_to_tile(m->entity.position.x, m->entity.position.y);
            _s.ground_items.push_back({generate_random_item(), tx, ty});
        }
    }

    // Floor clear check
    if (FloorManager::is_floor_cleared(_s.monsters) && !_s.stairs_active)
        _s._activate_stairs();

    // Relic hooks
    if (_s.player && player_has_relic(_s.player.get(), "leech_blade")) {
        const RelicDef* def = get_relic_def("leech_blade");
        float chance = def ? def->param : 0.20f;
        int heal = def ? def->param2 : 5;
        if ((rng() % 1000) < (int)(chance * 1000.0f))
            heal_player(_s.player.get(), heal);
    }
    if (_s.player && player_has_relic(_s.player.get(), "battle_totem")) {
        if ((rng() % 1000) < (int)(0.15f * 1000.0f))
            apply_buff(_s.player.get(), "attack_up", 1);
    }
    // D8: battle_medal — 击杀回3HP (100%)
    if (_s.player && player_has_relic(_s.player.get(), "battle_medal"))
        heal_player(_s.player.get(), 3);
    // D8: vampire_fang — 击杀回8%最大生命
    if (_s.player && player_has_relic(_s.player.get(), "vampire_fang")) {
        int vheal = (int)(get_effective_max_hp(_s.player.get()) * 0.08f);
        if (vheal > 0) heal_player(_s.player.get(), vheal);
    }
    // D8: thunder_core — 击杀30%触发AOE闪电 (对附近怪物造成ATK×1.5伤害)
    if (_s.player && player_has_relic(_s.player.get(), "thunder_core")) {
        if ((rng() % 100) < 30) {
            float px = _s.player->entity.rect.x + _s.player->entity.rect.width/2;
            float py = _s.player->entity.rect.y + _s.player->entity.rect.height/2;
            int tdmg = (int)(get_effective_attack(_s.player.get()) * 1.5f);
            for (auto& om : _s.monsters) {
                if (!om->combat.is_alive) continue;
                float d = hypotf(om->entity.rect.x + om->entity.rect.width/2 - px,
                                 om->entity.rect.y + om->entity.rect.height/2 - py);
                if (d < 4.0f * 32.0f)
                    om->combat.take_damage(tdmg);
            }
            LOG_INFO("[RELIC] 雷霆核心触发AOE: %d伤害", tdmg);
        }
    }
    // D8: time_fragment — 击杀5%概率重置所有技能冷却
    if (_s.player && player_has_relic(_s.player.get(), "time_fragment")) {
        if ((rng() % 100) < 5) {
            for (auto& sk : _s.player->skills.active_skills)
                sk->reset_cooldown();
            LOG_INFO("[RELIC] 时之碎片重置所有技能冷却");
        }
    }

    // M1A.1: 新遗物系统 — 数据驱动 on_kill 效果
    if (_s.player && m) {
        std::vector<Monster*> raw;
        for (auto& om : _s.monsters) raw.push_back(om.get());
        _relic_fx.on_kill(_s.player.get(), m, raw);
    }
}

void GameSceneCombat::cleanup_dead_monsters() {
    auto it = _s.monsters.begin();
    while (it != _s.monsters.end()) {
        if (!(*it)->combat.is_alive) {
            on_monster_killed(it->get());
            it = _s.monsters.erase(it);
        } else ++it;
    }
    if (FloorManager::is_floor_cleared(_s.monsters) && !_s.stairs_active)
        _s._activate_stairs();
}

void GameSceneCombat::apply_pending_damage() {
    // P1-C4-fix(UAF): 挂起伤害按 instance_id 查找目标 — 原裸 Monster* 在
    // 时停中可被 cleanup 释放 (Q3.5 修复的遗留缺口: 指针失效只靠 valid 检查
    // 兜底, 但堆地址复用会欺骗检查 / 常规失效直接丢伤害 → 同 seed 双结局).
    for (auto& [target_id, dmg] : _s.pending_damage) {
        for (auto& mm : _s.monsters) {
            if (mm->instance_id == target_id && mm->combat.is_alive) {
                mm->combat.take_damage(dmg);
                break;
            }
        }
    }
    auto it = _s.monsters.begin();
    while (it != _s.monsters.end()) {
        if (!(*it)->combat.is_alive) {
            on_monster_killed(it->get());
            it = _s.monsters.erase(it);
        } else ++it;
    }
    _s.pending_damage.clear();
}
