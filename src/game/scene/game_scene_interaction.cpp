#include "game_scene_interaction.h"
#include "game_scene.h"
#include "raylib.h"
extern Font g_font_small;
extern bool g_font_loaded;
#include "event_system.h"
#include "npc_system.h"
#include "floor_config.h"
#include "world_state.h"
#include "relationship_system.h"
#include "quest_manager.h"
#include "game_renderer.h"
#include "gameplay_system_director.h"
#include "presentation_system_director.h"
#include "resource_manager.h"                 // G9.4: 对话头像
#include "game/rendering/sprite_renderer.h"  // G9.4
#include <cstdio>
#include <iterator>   // P1-B-fix(A): std::size 推导对话池容量

// ============================================================
// Event UI
// ============================================================
static int _event_option_count(EventType et);
static Color _event_anim_color(EventType et);

void GameSceneInteraction::start_event_presentation(EventType et) {
    _s._event_ui.active = true;
    _s._event_ui.phase = EventPhase::ENTER;
    _s._event_ui.current = et;
    _s._event_ui.timer = 1.2f;
    _s._event_ui.fade = 0.0f;
    _s._event_ui.selected = 0;
    _s._event_ui.option_count = _event_option_count(et);
    _s._event_ui.desc_text = "";
    _s._event_ui.reward_text = "";
    _s._event_ui.complete_text = "";
    _s._event_ui.anim_color = _event_anim_color(et);
}

// G53: 事件落定后的世界 flag / 关系副作用 (选 0 = 主动接受诅咒/仪式)
void GameSceneInteraction::_apply_event_reward_flags(const DungeonEvent& ev,
                                                     int selected) {
    if (selected != 0) return;
    if (ev.type == EventType::CURSED_ROOM) {
        _s._gameplay.world_state.set(WorldFlag::Accepted_Curse);
        _s._gameplay.rels.add_trust(70, -1);
        _s._gameplay.rels.add_fear(70, 1);
    } else if (ev.type == EventType::BLOOD_RITUAL) {
        _s._gameplay.world_state.set(WorldFlag::Blood_Ritual);
        _s._gameplay.rels.apply_reward(RR_BLOOD_RITUAL);
    }
}

// G53: execute_event 的返回值是带前缀的协议串, 这里归类成给玩家看的一句
void GameSceneInteraction::_set_event_complete_text(std::string& complete_text,
                                                    const std::string& reward_text) {
    if (reward_text.size() > 6 && reward_text.substr(0, 6) == "RELIC:")
        complete_text = "获得圣物:" + reward_text.substr(6);
    else if (reward_text.size() > 4 && reward_text.substr(0, 4) == "MSG:")
        complete_text = reward_text.substr(4);
    else
        complete_text = reward_text;
}

void GameSceneInteraction::tick_event_ui(float dt) {
    auto& ui = _s._event_ui;
    if (!ui.active) return;

    if (ui.fade < 1.0f) ui.fade = std::min(1.0f, ui.fade + dt * 3.0f);
    ui.timer -= dt;

    switch (ui.phase) {
    case EventPhase::ENTER:
        if (ui.timer <= 0) {
            ui.phase = EventPhase::DESC;
            ui.timer = 0;
            auto pool = event_text_pool((int)ui.current - 1);
            if (!pool.empty()) ui.desc_text = pool[rng() % pool.size()];
        }
        break;
    case EventPhase::ANIM:
        if (ui.timer <= 0) {
            ui.phase = EventPhase::REWARD;
            ui.timer = 1.5f;
            DungeonEvent ev; ev.type = ui.current; ev.triggered = false;
            ui.reward_text = execute_event(ev, _s.player.get(), _s.current_floor);
            _apply_event_reward_flags(ev, ui.selected);
            _set_event_complete_text(ui.complete_text, ui.reward_text);
        }
        break;
    case EventPhase::REWARD:
        if (ui.timer <= 0) {
            ui.phase = EventPhase::COMPLETE;
            ui.timer = 2.0f;
            ui.fade = 0.0f;
        }
        break;
    case EventPhase::COMPLETE:
        if (ui.timer <= 0) {
            ui.active = false;
            ui.phase = EventPhase::INACTIVE;
            _s.game_map->event_triggered = true;
        }
        break;
    default: break;
    }
}

void GameSceneInteraction::draw_event_ui(int sw, int sh) {
    auto& ui = _s._event_ui;
    if (!ui.active) return;

    float alpha = ui.fade;
    auto ename = event_type_name(ui.current);
    Color gold = {255, 210, 50, (unsigned char)(255 * alpha)};
    Color white = {240, 240, 240, (unsigned char)(230 * alpha)};
    Color bg = {0, 0, 0, (unsigned char)(140 * alpha)};

    switch (ui.phase) {
    case EventPhase::ENTER: {
        DrawRectangle(0, 0, sw, sh, bg);
        GameRenderer::draw_glow_text("══════════════", sw/2.0f, sh/2.0f - 40, 22, gold, true);
        char title_buf[64]; snprintf(title_buf, sizeof(title_buf), "EVENT");
        GameRenderer::draw_glow_text(title_buf, sw/2.0f, sh/2.0f - 12, 36, gold, true);
        GameRenderer::draw_glow_text(ename, sw/2.0f, sh/2.0f + 28, 22, white, true);
        GameRenderer::draw_glow_text("══════════════", sw/2.0f, sh/2.0f + 52, 22, gold, true);
        break;
    }
    case EventPhase::DESC:
    case EventPhase::CHOICE: {
        float pw = 500, ph = 260;
        Rectangle pr = {sw/2.0f - pw/2, sh/2.0f - ph/2, pw, ph};
        GameRenderer::draw_panel(pr, ename, {15,15,35,(unsigned char)(220*alpha)});
        if (g_font_loaded && !ui.desc_text.empty())
            DrawTextEx(g_font_small, ui.desc_text.c_str(), {pr.x+30, pr.y+50}, 16, 1, white);
        if (ui.phase == EventPhase::CHOICE) {
            float oy = pr.y + 130;
            for (int i = 0; i < ui.option_count; i++) {
                const char* choice_text = "?";
                if (ui.current == EventType::BLOOD_RITUAL || ui.current == EventType::PRISONER
                    || ui.current == EventType::CURSED_ROOM)
                    choice_text = (i == 0) ? "→ YES" : "  NO";
                else if (ui.current == EventType::ALTAR_CHOICE) {
                    const char* opts[] = {"→ 攻击强化", "  生命恢复", "  技能进化"};
                    choice_text = (i < 3) ? opts[i] : "?";
                } else if (ui.current == EventType::MERCHANT) {
                    const char* opts[] = {"→ 购买药水", "  攻击祝福", "  随机圣物"};
                    choice_text = (i < 3) ? opts[i] : "?";
                }
                Color cc = (i == ui.selected) ? gold : Color{180,180,180,(unsigned char)(200*alpha)};
                DrawTextEx(g_font_small, choice_text, {pr.x+40, oy + i*28}, 16, 1, cc);
            }
        }
        const char* hint = (ui.phase == EventPhase::DESC)
            ? "[ENTER]继续  [ESC]离开" : "[←→]选择  [ENTER]确认  [ESC]取消";
        DrawTextEx(g_font_small, hint,
            {pr.x + (pw - MeasureTextEx(g_font_small,hint,14,1).x)/2, pr.y+ph-28},
            14, 1, Color{140,140,160,(unsigned char)(180*alpha)});
        break;
    }
    case EventPhase::ANIM: {
        DrawRectangle(0, 0, sw, sh, bg);
        float pulse = sinf(ui.timer * 8) * 30;
        float r = 80 + pulse;
        DrawRing({sw/2.0f, sh/2.0f}, r*0.7f, r, 0, 360, 24,
                 Color{ui.anim_color.r, ui.anim_color.g, ui.anim_color.b, (unsigned char)(180*alpha)});
        DrawRing({sw/2.0f, sh/2.0f}, r*0.4f, r*0.5f, 0, 360, 16,
                 Color{ui.anim_color.r, ui.anim_color.g, ui.anim_color.b, (unsigned char)(120*alpha)});
        break;
    }
    case EventPhase::REWARD: {
        DrawRectangle(0, 0, sw, sh, bg);
        GameRenderer::draw_glow_text("EVENT COMPLETE", sw/2.0f, sh/2.0f - 30, 28, gold, true);
        if (!ui.complete_text.empty())
            GameRenderer::draw_glow_text(ui.complete_text.c_str(), sw/2.0f, sh/2.0f + 20, 20, white, true);
        break;
    }
    case EventPhase::COMPLETE: {
        DrawRectangle(0, 0, sw, sh, {0,0,0,(unsigned char)(120*alpha)});
        GameRenderer::draw_glow_text("EVENT COMPLETE", sw/2.0f, sh/2.0f - 20, 24,
                                      {255,210,50,(unsigned char)(200*alpha)}, true);
        if (!ui.complete_text.empty())
            GameRenderer::draw_glow_text(ui.complete_text.c_str(), sw/2.0f, sh/2.0f + 18, 18,
                                          {240,240,240,(unsigned char)(180*alpha)}, true);
        break;
    }
    default: break;
    }
}

// ============================================================
// NPC / Dialogue
// ============================================================
void GameSceneInteraction::spawn_floor_npcs(int floor, const std::vector<std::pair<int,int>>& rooms) {
    for (int slot = 0; slot < 2; slot++) {
        const NPCData* cfg = get_npc_config(floor, slot);
        if (!cfg) continue;
        int npc_id = floor * 10 + slot;
        const int npc_index_before = _s._npc_count;
        NPCState* st = _s._find_or_create_npc_state(npc_id);
        if (!st || st->finished) continue;
        // P1-B-fix(B): 写"本 NPC 自己"的槽 — 原写 _npc_count-1 恰是"最后创建"的槽,
        // 已存在的 NPC (met 跨局残留) 会把位置写到别人槽上 → 落位错乱/越界触发对话
        int npc_index = (npc_index_before < _s._npc_count) ? _s._npc_count - 1 : -1;
        if (npc_index < 0) {
            for (int i = 0; i < _s._npc_count; i++)
                if (_s._npc_state[i].id == npc_id) { npc_index = i; break; }
        }
        if (npc_index < 0) continue;
        int r_idx = 2 + slot; if (r_idx >= (int)rooms.size() - 1) r_idx = (int)rooms.size()/2;
        auto [rx, ry] = rooms[r_idx];
        _s._npc_tile_x[npc_index] = rx;
        _s._npc_tile_y[npc_index] = ry;
    }
}

void GameSceneInteraction::start_dialogue(int npc_index) {
    if (npc_index < 0 || npc_index >= _s._npc_count) return;
    NPCState* st = &_s._npc_state[npc_index];
    const NPCData* cfg = get_npc_config(st->id / 10, st->id % 10);
    if (!cfg) return;

    _s._dialogue.active = true;
    _s._dialogue.page = 0;
    _s._dialogue.timer = 0.0f;
    _s._dialogue.pages.clear();
    _s._dialogue.target_npc = st;

    // P1-B-fix(A): 按池真实容量遍历 — first_dialogue[5] / repeat_dialogue[2] 尺寸不同,
    // 原 i<5 统一遍历对 repeat 池越界读 3 个野指针 → dp.text=pool[i] → strlen SIGSEGV
    // (gdb 实锤: seed 3 run 4, 二次对话必崩, 真玩家同样潜伏)
    const char* const* pool = st->met ? cfg->repeat_dialogue : cfg->first_dialogue;
    const size_t pool_size = st->met ? std::size(cfg->repeat_dialogue)
                                      : std::size(cfg->first_dialogue);
    for (size_t i = 0; i < pool_size; i++) {
        if (!pool[i]) break;
        DialoguePage dp;
        dp.speaker = cfg->name;
        dp.text = pool[i];
        _s._dialogue.pages.push_back(dp);
    }
    if (!st->met) st->met = true;

    Quest* npc_quest = _s._gameplay.quest_mgr.get_npc_quest(st->id / 10);
    if (npc_quest && npc_quest->state == QuestState::AVAILABLE)
        _s._gameplay.quest_mgr.accept(npc_quest->id, _s._gameplay.world_state);

    int npc_floor = st->id / 10;
    const char* adaptive = StoryDirector::npc_adaptive_text(npc_floor, _s._gameplay.world_state);
    if (adaptive && !st->finished) {
        DialoguePage ap;
        ap.speaker = cfg->name;
        ap.text = adaptive;
        _s._dialogue.pages.push_back(ap);
    }
}

void GameSceneInteraction::update_dialogue(float dt) {
    if (_s._dialogue.active) _s._dialogue.timer += dt;
}

void GameSceneInteraction::draw_dialogue(int sw, int sh) {
    auto& d = _s._dialogue;
    if (!d.active || d.page >= (int)d.pages.size()) return;

    float pw = 600, ph = 260;
    Rectangle pr = {sw/2.0f - pw/2, sh/2.0f - ph/2, pw, ph};
    GameRenderer::draw_panel(pr, d.pages[d.page].speaker.c_str(), {20,20,45,240});

    // G9.4: NPC 对话头像 (target_npc->id → 楼层 → 精灵)
    if (d.target_npc) {
        SpriteDef sdef;
        Texture2D stex = ResourceManager::inst().sprite_by_key(
            npc_sprite_key(d.target_npc->id / 10), sdef);
        if (stex.id > 0)
            SpriteRenderer::draw_sprite(stex, sdef, 0,
                {pr.x + pw - 90, pr.y + 18, 56, 56});
    }

    if (g_font_loaded)
        DrawTextEx(g_font_small, d.pages[d.page].text.c_str(), {pr.x+30, pr.y+50}, 16, 1, {230,230,240,255});

    const char* hint = (d.page + 1 < (int)d.pages.size())
        ? "[ENTER]下一页  [ESC]跳过" : "[ENTER]告别  [ESC]离开";
    DrawTextEx(g_font_small, hint,
        {pr.x + (pw - MeasureTextEx(g_font_small,hint,14,1).x)/2, pr.y+ph-28},
        14, 1, {140,140,160,200});
    char pg[16]; snprintf(pg, sizeof(pg), "%d/%d", d.page+1, (int)d.pages.size());
    DrawTextEx(g_font_small, pg, {pr.x+16, pr.y+ph-30}, 13, 1, {180,180,200,200});
}

// G53: 任务列表 (跳过 hidden; y 越界即停, 不裁剪滚动)
// 注意: 关系栏沿用原逻辑 — 只在任务非空时才绘制 (空任务时整块留空)
void GameSceneInteraction::_draw_quest_list(const Rectangle& pr) {
    auto& quests = _s._gameplay.quest_mgr.all_quests();
    if (quests.empty()) {
        DrawTextEx(g_font_small, "暂无任务。", {pr.x+30, pr.y+60}, 16, 1, {160,160,180,200});
        return;
    }
    int done = _s._gameplay.quest_mgr.completed_count();
    char progress[32];
    snprintf(progress, sizeof(progress), "%d/%d 完成", done, (int)quests.size());
    DrawTextEx(g_font_small, progress, {pr.x+pr.width-100, pr.y+14}, 12, 1, {200,200,100,200});

    float y = pr.y + 45;
    for (auto& q : quests) {
        if (q.hidden) continue;
        const char* status; Color c;
        switch (q.state) {
            case QuestState::COMPLETED: status = "[?完成]"; c = {100,200,100,255}; break;
            case QuestState::ACCEPTED:  status = "→进行中!"; c = {255,200,50,255}; break;
            case QuestState::AVAILABLE: status = "○可接"; c = {180,180,220,255}; break;
            case QuestState::FAILED:    status = "[?失败]"; c = {200,100,100,255}; break;
            default: status = "[锁]"; c = {100,100,100,200}; break;
        }
        char buf[128];
        snprintf(buf, sizeof(buf), "%s %s", status, q.title.c_str());
        DrawTextEx(g_font_small, buf, {pr.x+25, y}, 16, 1, c);
        DrawTextEx(g_font_small, q.desc.c_str(), {pr.x+40, y+22}, 13, 1, {180,180,200,200});
        y += 44;
        // 与关系栏保持 100px 间距, 再高就截断
        if (y > pr.y + pr.height - 100) break;
    }
}

// G53: NPC 关系栏 (跳过 total()==0 的关系; 名字是硬编码 id 映射)
void GameSceneInteraction::_draw_npc_relations(const Rectangle& pr) {
    auto& rels = _s._gameplay.rels.all();
    if (rels.empty()) return;
    DrawLine(pr.x+15, pr.y+pr.height-85, pr.x+pr.width-15, pr.y+pr.height-85, {60,60,90,180});
    float ry = pr.y+pr.height-78;
    DrawTextEx(g_font_small, "NPC关系:", {pr.x+20, ry}, 12, 1, {200,180,140,220});
    ry += 16;
    for (auto& r : rels) {
        if (r.total() == 0) continue;
        const char* npc_name = "???";
        if (r.npc_id == 20) npc_name = "埃德加";
        else if (r.npc_id == 70) npc_name = "泰伦斯";
        else if (r.npc_id == 90) npc_name = "索拉斯";
        else if (r.npc_id == 110) npc_name = "灵魂";
        else if (r.npc_id == 140) npc_name = "守望者";
        char rbuf[80];
        snprintf(rbuf, sizeof(rbuf), "%s  Tr:%d Rs:%d Fr:%d",
            npc_name, r.trust, r.respect, r.fear);
        DrawTextEx(g_font_small, rbuf, {pr.x+25, ry}, 12, 1, {180,180,200,200});
        ry += 14;
        if (ry > pr.y + pr.height - 10) break;
    }
}

void GameSceneInteraction::draw_quest_log(int sw, int sh) {
    float pw = 380, ph = 420;
    Rectangle pr = {sw/2.0f - pw/2, sh/2.0f - ph/2, pw, ph};
    GameRenderer::draw_panel(pr, "任务日志  F1关闭", {20,20,40,230});

    _draw_quest_list(pr);
    // 关系栏沿用原逻辑: 只在任务非空时才绘制 (无任务时只显示"暂无任务")
    if (!_s._gameplay.quest_mgr.all_quests().empty())
        _draw_npc_relations(pr);
}

// ============================================================
// Static helpers
// ============================================================
static int _event_option_count(EventType et) {
    switch (et) {
        case EventType::BLOOD_RITUAL: return 2;
        case EventType::PRISONER:     return 2;
        case EventType::CURSED_ROOM:  return 2;
        case EventType::ALTAR_CHOICE: return 3;
        case EventType::MERCHANT:     return 3;
        default: return 0;
    }
}

static Color _event_anim_color(EventType et) {
    switch (et) {
        case EventType::AMBUSH:         return {255, 40, 30, 255};
        case EventType::CURSED_ROOM:    return {140, 40, 200, 255};
        case EventType::BLOOD_RITUAL:   return {200, 30, 30, 255};
        case EventType::LOST_CAMP:      return {255, 200, 60, 255};
        case EventType::TREASURE_GUARD: return {255, 200, 30, 255};
        case EventType::MERCHANT:       return {255, 220, 100, 255};
        case EventType::ALTAR_CHOICE:   return {200, 200, 255, 255};
        case EventType::STATUE:         return {180, 160, 220, 255};
        default: return {255, 200, 50, 255};
    }
}
