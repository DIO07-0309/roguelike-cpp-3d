#include "game_scene_input.h"
#include "core/logger.h"      // A4-probe
#include "game_scene.h"
#include "title_scene.h"
#include "config.h"
#include "combat_system.h"
#include "scene_tree.h"
#include "audio/audio_server.h"
#include "save/save_manager.h"
#include "core/logger.h"
#include "event_system.h"
#include "boss.h"
#include "build_score.h"
#include "boss_system_director.h"
#include "gameplay_system_director.h"
#include "presentation_system_director.h"
#include "game_flow_director.h"
#include "player_controller.h"
#include "ending_director.h"

// ============================================================
// GameSceneInput — main entry
// ============================================================
void GameSceneInput::handle_input(const InputMap& input) {
    if (!_s.player) return;
    auto* tree = _s.get_tree();
    if (!tree) return;

    // M6-i.1: 游戏内截图 (F7 — 最顶部, 任何 UI/状态都可用; Release 生效)
    if (IsKeyPressed(KEY_F7)) {
        TakeScreenshot("screenshot.png");
        LOG_INFO("[M6-i.1 截图] screenshot.png 已保存 (工作目录)");
    }

    // D4 Step2: event UI intercept
    if (_s._is_event_running()) { handle_event_input(input); return; }
    // D4 Step4: dialogue UI intercept
    if (_s._dialogue.active) { handle_dialogue_input(input); return; }
    // D4 Step4: Quest log (F1 key)
    if (IsKeyPressed(KEY_F1)) {
        _s._quest_log_open = !_s._quest_log_open;
        if (_s._quest_log_open) _s.inventory_open = false;
        tree->get_audio()->play_sfx("ui_click", 0.35f);  // Q4.5
        return;
    }
    if (_s._quest_log_open) {
        if (IsKeyPressed(KEY_F1) || _s._is_action_just_pressed(input,"cancel")) {
            _s._quest_log_open = false;
            tree->get_audio()->play_sfx("ui_click", 0.35f);  // Q4.5
        }
        return;
    }

    // Batch 3I: Challenge choice UI
    if (_s.challenge_choice_active) {
        if (_s._is_action_just_pressed(input, "ui_up") || IsKeyPressed(KEY_UP)) {
            _s.challenge_choice_cursor = (_s.challenge_choice_cursor + 1) % 2;
            _s.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
        }
        if (_s._is_action_just_pressed(input, "ui_down") || IsKeyPressed(KEY_DOWN)) {
            _s.challenge_choice_cursor = (_s.challenge_choice_cursor + 1) % 2;
            _s.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
        }
        if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE)) {
            if (_s.challenge_choice_cursor == 0) {
                if (_s._challenge.consume_key_for_challenge(*_s.player)) {
                    _s.enter_challenge_arena();
                    _s.get_tree()->get_audio()->play_sfx("door_open", 0.6f);
                } else {
                    _s._presentation.room_msg = "需要钥匙才能开启挑战。";
                    _s._presentation.room_msg_timer = 2.0f;
                }
            }
            _s.challenge_choice_active = false;
            _s.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
        }
        if (_s._is_action_just_pressed(input, "cancel") || IsKeyPressed(KEY_ESCAPE)) {
            _s.challenge_choice_active = false;
            _s.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
        }
        return;
    }

    if (_s._is_action_just_pressed(input,"cancel")) {
        // B3H-fix: 背包/赌局是全屏压暗模态, ESC 先关面板再走保存退出
        if (_s.inventory_open || _s.gamble_open) {
            _s.inventory_open = false;
            _s.gamble_open = false;
            _s.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
            return;
        }
        if (_s.state == GameState::PLAYING && _s.player->combat.is_alive) {
            if (!_s.is_save_blocked()) {
                _s.max_unlocked_floor = std::max(_s.max_unlocked_floor, _s.current_floor);
                std::vector<bool> spr, spd;
                if (_s.game_map) for (auto& sr : _s.game_map->special_rooms) {
                    spr.push_back(sr.triggered);
                    spd.push_back(sr.discovered);
                }
                std::unordered_map<std::string, int> rcm;
                static const char* ALL_RULES[] = {
                    "rule_shadow_charge","rule_summon_priority","rule_arena_movement",
                    "rule_shield_patience","rule_rule_override", nullptr
                };
                for (int i = 0; ALL_RULES[i]; i++) {
                    int v = _s._gameplay.world_state.counter(ALL_RULES[i]);
                    if (v > 0) rcm[ALL_RULES[i]] = v;
                }
                // M1-D1: last-known 镜像记忆 — Boss 存活则更新缓存, 否则透传
                {
                    std::vector<float> fresh_alpha, fresh_beta;
                    _s._boss.export_mirror_memory(fresh_alpha, fresh_beta);
                    if (!fresh_alpha.empty()) {
                        _s._mirror_mem_alpha = fresh_alpha;
                        _s._mirror_mem_beta = fresh_beta;
                    }
                }
                // G10.9-B2: 槽位化 + play_time; endings 不再入档 (Meta 落盘)
                SaveManager::save_game(SaveManager::active_slot(),
                    _s.player.get(), _s.current_floor,
                    _s.max_unlocked_floor, _s._dungeon_seed, spr, spd, rcm,
                    _s._gameplay.quest_mgr.export_states(),
                    _s._mirror_mem_alpha, _s._mirror_mem_beta, (float)_s.game_time);
                LOG_INFO("Save→第%d层", _s.current_floor);
            }
        }
        auto ts = std::make_shared<TitleScene>();
        ts->name = "TitleScene";
        ts->has_save = SaveManager::save_exists();
        tree->change_scene(ts);
        return;
    }
    if (_s.state == GameState::BOSS_INTRO) {
        if (_s._is_action_just_pressed(input,"confirm")) {
            auto pos = _s.game_map->tile_to_pixel(_s.stairs_pos.first, _s.stairs_pos.second);
            // D8: BossFactory 按 floor+seed 随机 Boss 类型
BossType btype = boss_type_for_floor(_s.boss_floor, _s._dungeon_seed);
auto* boss = boss_factory_create(btype, _s.stairs_pos.first, _s.stairs_pos.second,
                                  _s.boss_floor);
            _s.monsters.emplace_back(boss);

            BuildType bt = calculate_build(_s.player.get()).identify();
            _s._boss.init_on_spawn(boss, _s.boss_floor, _s._gameplay.world_state,
                                    bt, _s._gameplay.rels, _s.game_map.get(),
                                    _s.player.get());
            // M4e: 跨对局镜像记忆注入 (空 vector 安全)
            _s._boss.inject_mirror_memory(_s._mirror_mem_alpha,
                                          _s._mirror_mem_beta);
            // M1 + v1.6-B1: 镜像播报接线 — 战术切换走消息; 阶段晋升走
            // 醒目横幅 (它开始模仿你/看穿你 — 招牌时刻不该是小字)
            _s._boss.connect_mirror_theater(
                [&s = _s](const char* msg, float dur) {
                    if (msg) s._presentation.show_message(msg, dur);
                });
            _s._boss.hook_phase_banner(
                [&s = _s](int phase, const char* reason) {
                    s._mirror_banner_phase = phase;
                    s._mirror_banner_timer = 3.0f;
                    if (reason) s._presentation.show_message(reason, 2.5f);
                });
            _s._presentation.boss_modifier_text = _s._boss.modifier_text;

            _s.boss_cinematic_timer = 1.0f;
            _s._boss_entrance_timer = 2.0f;
            _s._boss_entered = false;
            _s._boss_phase2_shown = false;
            _s.state = GameState::BOSS_CINEMATIC;
            _s._flow.on_boss_intro_confirm();
        }
        return;
    }

    if (_s.state != GameState::PLAYING) return;

    if (IsKeyPressed(KEY_R)) {
        _s._show_relic_panel = !_s._show_relic_panel;
        tree->get_audio()->play_sfx("ui_click", 0.35f);  // Q4.5
    }

    // M6-i.1: 游戏内截图已挪 handle_input 顶部 (任何状态可用)

#ifdef _DEBUG
    handle_debug_keys();
#endif

    _s._player_ctrl.handle_input(input);
}

// ============================================================
// Event input handler
// ============================================================
void GameSceneInput::handle_event_input(const InputMap& input) {
    auto& ui = _s._event_ui;
    if (!ui.active) return;

    // P1-A4-fix: sim 模式事件 UI 自动推进 — AI 决策集无 confirm/cancel 词汇,
    // 真实卡死链: AI 按 E 触发事件 → DESC/CHOICE 无限等待 → 攻击消费链断 → 围殴致死.
    // sim 的目的是战斗/平衡测试, 叙事 UI 直接选第一项通过 (确定性: 选项0).
    if (_s._sim_mode) {
        if (ui.phase == EventPhase::DESC) {
            ui.phase = (ui.option_count > 0) ? EventPhase::CHOICE : EventPhase::ANIM;
            if (ui.phase == EventPhase::ANIM) ui.timer = 0.8f;
            else ui.selected = 0;
        } else if (ui.phase == EventPhase::CHOICE) {
            ui.selected = 0;
            ui.phase = EventPhase::ANIM;
            ui.timer = 0.8f;
        }
        return;   // ANIM/RESULT 由 _update 的 timer 自行推进, 无需输入
    }

    if (_s._is_action_just_pressed(input,"cancel")) {
        if (ui.phase == EventPhase::DESC || ui.phase == EventPhase::CHOICE) {
            ui.active = false;
            ui.phase = EventPhase::INACTIVE;
            _s._presentation.room_msg = "你离开了这里。";
            _s._presentation.room_msg_timer = 1.5f;
        }
        return;
    }

    if (ui.phase == EventPhase::DESC) {
        if (_s._is_action_just_pressed(input,"confirm")) {
            if (ui.option_count > 0) {
                ui.phase = EventPhase::CHOICE;
                ui.selected = 0;
            } else {
                ui.phase = EventPhase::ANIM;
                ui.timer = 0.8f;
                _s._presentation.trigger_shake(3.0f);
            }
        }
    } else if (ui.phase == EventPhase::CHOICE) {
        if (_s._is_action_just_pressed(input,"move_left"))
            ui.selected = (ui.selected - 1 + ui.option_count) % ui.option_count;
        if (_s._is_action_just_pressed(input,"move_right"))
            ui.selected = (ui.selected + 1) % ui.option_count;
        if (_s._is_action_just_pressed(input,"confirm")) {
            ui.phase = EventPhase::ANIM;
            ui.timer = 0.8f;
            _s._presentation.trigger_shake(4.0f);
        }
    }
}

// ============================================================
// Dialogue input handler
// ============================================================
void GameSceneInput::handle_dialogue_input(const InputMap& input) {
    auto& d = _s._dialogue;
    if (!d.active) return;

    // P1-A4-fix: sim 模式对话自动翻页 — AI 无 confirm 词汇, 对话无限等待同样锁死攻击链.
    if (_s._sim_mode) {
        d.page++;
        d.timer = 0.0f;
        // 翻到底的收尾逻辑与真实按键路径一致 (走 confirm 分支的简化: 每次进一格)
        if (d.page >= (int)d.pages.size()) {
            _s._dialogue.active = false;   // 直接关闭, NPC 状态由真实路径维护
            return;
        }
        return;
    }

    if (_s._is_action_just_pressed(input,"confirm")) {
        d.page++;
        d.timer = 0.0f;
        if (d.page >= (int)d.pages.size()) {
            if (d.target_npc) {
                d.target_npc->finished = true;
                int npc_floor = d.target_npc->id / 10;
                if (npc_floor == 2) {
                    _s._gameplay.world_state.set(WorldFlag::Saved_Prisoner);
                    _s._gameplay.rels.apply_reward(RR_SAVE_PRISONER);
                } else if (npc_floor == 7) {
                    _s._gameplay.world_state.set(WorldFlag::Saved_Priest);
                    _s._gameplay.rels.apply_reward(RR_SAVE_PRIEST);
                } else if (npc_floor == 9) {
                    _s._gameplay.rels.add_respect(90, 1);
                } else if (npc_floor == 14) {
                    _s._gameplay.rels.add_trust(140, 1);
                    _s._gameplay.rels.add_respect(140, 2);
                }
                const NPCData* cfg = get_npc_config(d.target_npc->id / 10, d.target_npc->id % 10);
                if (cfg && cfg->farewell) {
                    _s._presentation.room_msg = cfg->farewell;
                    _s._presentation.room_msg_timer = 2.0f;
                }
            }
            d.active = false;
            d.target_npc = nullptr;
        }
    }
    if (_s._is_action_just_pressed(input,"cancel")) {
        d.active = false; d.target_npc = nullptr;
        _s._presentation.room_msg = "你转身离开了。";
        _s._presentation.room_msg_timer = 1.5f;
    }
}

// ============================================================
// Debug key handler (F1-F12, only in _DEBUG builds)
// ============================================================
void GameSceneInput::handle_debug_keys() {
    if (!_s.player) return;
    if (IsKeyPressed(KEY_F7))
        apply_buff(_s.player.get(), "attack_up", 1);
    if (IsKeyPressed(KEY_F2))
        apply_buff(_s.player.get(), "slow", 1);
    if (IsKeyPressed(KEY_F3) && !_s.monsters.empty())
        apply_buff(_s.monsters[0].get(), "poison", 2);
    if (IsKeyPressed(KEY_F4)) {
        auto dump = [](const std::vector<BuffInstance>& buffs, const char* who) {
            if (buffs.empty()) { LOG_INFO("[F4] %s buffs: (none)", who); return; }
            for (auto& b : buffs)
                LOG_INFO("[F4] %s buff: %s stacks=%d rem=%.2f tick=%.2f",
                    who, b.id.c_str(), b.stacks, b.remaining, b.tick_timer);
        };
        dump(_s.player->active_buffs, "Player");
        if (!_s.monsters.empty()) dump(_s.monsters[0]->active_buffs, "Monster[0]");
    }
    if (IsKeyPressed(KEY_F5) && !_s.monsters.empty())
        apply_buff(_s.monsters[0].get(), "slow", 2);
    if (IsKeyPressed(KEY_F6) && !_s.monsters.empty())
        apply_buff(_s.monsters[0].get(), "attack_up", 2);
    if (IsKeyPressed(KEY_F8))
        _s._presentation.show_growth_debug = !_s._presentation.show_growth_debug;
    if (IsKeyPressed(KEY_F9))
        _s._presentation.combat_juice_on = !_s._presentation.combat_juice_on;
    // M6-i.1: 游戏内截图 (调试键 F7 — raylib TakeScreenshot 直接读 GL
    // 帧缓冲, 3D/HDR 内容零失真; 截到工作目录 screenshot.png)
    if (IsKeyPressed(KEY_F7)) {
        TakeScreenshot("screenshot.png");
        LOG_INFO("[截图] screenshot.png 已保存 (工作目录)");
    }
    if (IsKeyPressed(KEY_F10))
        _s._presentation.show_flow_debug = !_s._presentation.show_flow_debug;
    if (IsKeyPressed(KEY_F11))
        _s._presentation.show_boss_behavior = !_s._presentation.show_boss_behavior;
    if (IsKeyPressed(KEY_F12))
        _s._presentation.show_boss_cmd = !_s._presentation.show_boss_cmd;
    if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_F12))
        _s._presentation.show_boss_report = !_s._presentation.show_boss_report;
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_F12)) {
        static int ending_idx = 0;
        EndingType all[] = {EndingType::BAD_END, EndingType::NORMAL_END,
                            EndingType::GOOD_END, EndingType::TRUE_END, EndingType::SECRET_END};
        ending_idx = (ending_idx + 1) % 5;
        _s._gameplay.ending_dir.debug_override(all[ending_idx]);
        _s._presentation.room_msg = std::string("[Preview] ") + _s._gameplay.ending_dir.ending_name();
        _s._presentation.room_msg_timer = 2.0f;
    }
}
