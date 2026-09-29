#include "game_flow_director.h"
#include "scenes/game_scene.h"
#include "scenes/death_scene.h"
#include "scenes/victory_scene.h"
#include "scenes/credits_scene.h"
#include "scene_tree.h"
#include "core/logger.h"

void GameFlowDirector::new_game() {
    if (!_scene) return;
    current_state = GameFlowState::NEW_GAME;

    // 委托给 GameScene 已有的 new_game() 实现
    _scene->new_game();
    current_state = GameFlowState::PLAYING;
}

void GameFlowDirector::load_saved_game(int floor, int max_f, std::unique_ptr<Player> p,
                                        uint32_t seed,
                                        const std::vector<bool>& special_triggered,
                                        const std::vector<bool>& special_discovered,
                                        const std::unordered_map<std::string, int>& rule_counters,
                                        const std::unordered_map<int, int>& quest_states,
                                        float play_time) {
    if (!_scene) return;
    current_state = GameFlowState::ENTER_FLOOR;
    // G10.9-B2: unlocked_endings 参数移除 (Meta 账号级恢复)
    _scene->load_saved_game(floor, max_f, std::move(p), seed, special_triggered,
                            special_discovered, rule_counters, quest_states, play_time);
    current_state = GameFlowState::PLAYING;
}

void GameFlowDirector::on_boss_intro_confirm() {
    if (!_scene) return;
    current_state = GameFlowState::BOSS_FIGHT;
}

void GameFlowDirector::on_player_dead() {
    if (!_scene) return;
    current_state = GameFlowState::PLAYER_DEAD;

    // 组装 DeathScene
    auto ds = std::make_shared<DeathScene>();
    ds->name = "DeathScene";
    ds->final_floor = _scene->current_floor;
    ds->final_level = _scene->player->level;
    ds->ending_name  = _scene->_gameplay.ending_dir.ending_name();
    ds->final_line   = _scene->_gameplay.ending_dir.final_line();
    ds->meta_soul    = _scene->_gameplay.ending_dir.meta_reward_soul() / 2;
    ds->meta_knowledge = _scene->_gameplay.ending_dir.meta_reward_knowledge() / 2;
    // v1.6-B2: 本局死因 (last_damage_source 全程追踪: 怪名/环境/DOT/Boss技能)
    // 前缀翻译成玩家友好文案 (sim CSV 用原始码, 实机 UI 用译文)
    ds->death_cause = _scene->player->combat.last_damage_source;
    if (ds->death_cause.rfind("dot:", 0) == 0)
        ds->death_cause = "持续伤害·" + ds->death_cause.substr(4);
    else if (ds->death_cause.rfind("env:", 0) == 0)
        ds->death_cause = "环境·" + ds->death_cause.substr(4);
    if (ds->death_cause.empty()) ds->death_cause = "未知原因";
    _fill_mirror_verdict(*ds);
    // v1.6-B2: 死因史入账 (sim readonly 模式 record 内不落盘)
    g_meta.record_death(ds->final_floor, ds->death_cause);

    _scene->get_tree()->change_scene(ds);
    current_state = GameFlowState::ENDING;
    LOG_INFO("死亡→DeathScene (第%d层 Lv%d)", ds->final_floor, ds->final_level);
}

// v1.6-B1: F15 死亡时的镜像复盘 — "它靠什么赢了你" (非 F15 留空不显示)
void GameFlowDirector::_fill_mirror_verdict(DeathScene& ds) {
    if (!_scene->_boss._mirror_agent) return;
    const auto& agent = *_scene->_boss._mirror_agent;
    const MirrorDebugStats* st = agent.debug_stats();
    float acc = agent.prediction_accuracy();
    int phase = agent.current_phase();
    // 结论文案: 按 phase + 准确率分档
    char vbuf[160];
    if (phase >= 3)
        snprintf(vbuf, sizeof(vbuf), "回响进入了进化期 — 它完整复刻了你的战斗人格");
    else if (acc > 0.5f)
        snprintf(vbuf, sizeof(vbuf), "它预判了你 %.0f%% 的动作 — 你的习惯成了破绽", acc * 100);
    else
        snprintf(vbuf, sizeof(vbuf), "它仍在学习你 (命中 %.0f%%) — 这一次, 是它比你强", acc * 100);
    ds.mirror_verdict = vbuf;
    // 被针对习惯 Top3 (与战斗内面板同源逻辑)
    const PlayerHabitProfile& pf = agent.profile();
    ds.mirror_habits.clear();
    auto add = [&](const char* s) { ds.mirror_habits += s; ds.mirror_habits += '\n'; };
    if (pf.predict_attack_heavy) add("· 攻击成瘾 — 被『引诱后惩罚』反复针对");
    if (pf.predict_low_dodge)    add("· 几乎不闪避 — 被近身压制");
    if (pf.predict_panic_heal)   add("· 治疗时机可预测 — 被读秒打断");
    if (pf.fight_back_rate > 0.6f) add("· 受击必反击 — 被后手预判");
    if (pf.attack_rhythm_var < 0.25f && pf.total_actions > 40)
        add("· 固定攻击节奏 — 被节奏反制");
    if (st && st->snapshot().interrupt_success > 0) {
        char ib[64];
        snprintf(ib, sizeof(ib), "· 技能被时停打断 %d 次", st->snapshot().interrupt_success);
        add(ib);
    }
}

void GameFlowDirector::on_game_clear() {
    if (!_scene) return;
    current_state = GameFlowState::GAME_CLEAR;
    // Q3.16: 通关 BGM 由 VictoryScene::get_bgm_name() 声明, change_scene 管线自动切换

    // G10.9-B1: 结局判定/奖励发放已由 GameScene 调 _gameplay.on_game_clear() 完成
    // (旧代码这里再 begin() + add_currency → 结局双判定/奖励双发), 此处只读结果建场景
    auto* gs = _scene;
    auto& ending = gs->_gameplay.ending_dir;

    // ── VictoryScene ──
    auto vs = std::make_shared<VictoryScene>();
    vs->name = "VictoryScene";
    vs->final_level = gs->player->level;
    vs->ending_name   = ending.ending_name();
    vs->final_line    = ending.final_line();
    vs->sky_color     = ending.sky_color();
    vs->meta_soul     = ending.meta_reward_soul();
    vs->meta_knowledge = ending.meta_reward_knowledge();

    // 填充 Credits 数据 (NPC / Timeline / Report / Summary)
    auto& npc_ends = ending.npc_endings();
    vs->npc_count = (int)npc_ends.size();
    for (size_t i = 0; i < npc_ends.size() && i < 8; i++) {
        vs->npc_names[i]   = npc_ends[i].name;
        vs->npc_results[i] = npc_ends[i].result;
        vs->npc_details[i] = npc_ends[i].detail;
    }
    vs->timeline_count = gs->_boss.timeline.count();
    auto& tl = gs->_boss.timeline.events();
    for (int i = 0; i < gs->_boss.timeline.count() && i < 16; i++) {
        vs->timeline_times[i]  = tl[i].time;
        vs->timeline_labels[i] = tl[i].label ? tl[i].label : "";
    }
    vs->boss_rank     = gs->_boss.battle_report.rank_name();
    vs->boss_dmg_done = gs->_boss.battle_report.total_damage;
    vs->boss_dmg_taken= gs->_boss.battle_report.damage_taken;
    vs->boss_time     = gs->_boss.battle_report.battle_time;
    vs->boss_arena    = gs->_boss.battle_report.arena_zones_spawned;
    vs->run_floor     = gs->_gameplay.run_stats.floor_reached;
    vs->run_bosses    = gs->_gameplay.run_stats.bosses_killed;
    vs->run_kills     = gs->_gameplay.run_stats.total_kills;
    vs->run_elites    = gs->_gameplay.run_stats.elite_kills;
    vs->run_relics    = gs->_gameplay.run_stats.relics_collected;
    vs->run_quests    = gs->_gameplay.run_stats.quests_done;
    vs->run_combo     = gs->_gameplay.run_stats.combo_max;
    vs->run_playtime  = gs->_gameplay.run_stats.play_time;

    // G10.9-B1: Meta 奖励已在 _gameplay.on_game_clear → reward_from_ending 内发放
    // (此处旧代码再 add_currency → 双发), 这里只保证 meta 落盘一次
    g_meta.save();

    gs->get_tree()->change_scene(vs);
    current_state = GameFlowState::ENDING;
    LOG_INFO("通关→VictoryScene (%s)", ending.ending_name());
}
