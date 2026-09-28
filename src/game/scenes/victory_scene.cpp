#include "victory_scene.h"
#include "credits_scene.h"
#include "title_scene.h"
#include "scene_tree.h"
#include "core/logger.h"
#include "meta_progression.h"
#include "rendering/centered_text.h"
#include <cmath>

extern Font g_font, g_font_small;
extern bool g_font_loaded;

Color VictoryScene::_ending_tint() const {
    if (ending_name == "TRUE END" || ending_name == "ABSOLUTE END")
        return {255, 220, 80, 255};
    if (ending_name == "GOOD END")
        return {100, 255, 120, 255};
    if (ending_name == "BAD END")
        return {220, 80, 60, 255};
    return {200, 200, 200, 255};
}

// G13: 结局色氛围渐变底 + 顶部放射细线 (胜利光感)
void VictoryScene::_render_background(int sw, int sh, Color ec) {
    ClearBackground({(unsigned char)(ec.r / 14), (unsigned char)(ec.g / 14),
                     (unsigned char)(ec.b / 14), 255});
    for (int i = 0; i < 80; i++) {
        const float t = i / 80.0f;
        DrawRectangle(0, i, sw, 1,
            {(unsigned char)(ec.r / 14 + ec.r / 10 * t),
             (unsigned char)(ec.g / 14 + ec.g / 10 * t),
             (unsigned char)(ec.b / 14 + ec.b / 10 * t), 255});
    }
    for (int i = 0; i < 24; i++) {
        const float a = i / 24.0f * 6.2832f;
        DrawLine(sw / 2, -20, sw / 2 + cosf(a) * 900, -20 + sinf(a) * 900,
                 {ec.r, ec.g, ec.b, 18});
    }
}

// G13: 结局名 + 台词 + Meta 结算
void VictoryScene::_render_ending_panel(int sw, int sh, Color ec) {
    // M3: 结局名投影
    const float w = MeasureTextEx(g_font, ending_name.c_str(), 48, 1).x;
    DrawTextEx(g_font, ending_name.c_str(), {sw / 2.0f - w / 2 + 3, 63}, 48, 1, {0, 0, 0, 150});
    DrawTextEx(g_font, ending_name.c_str(), {sw / 2.0f - w / 2, 60}, 48, 1, ec);
    DrawLine(sw / 2 - w / 2, 118, sw / 2 + w / 2, 118, {ec.r, ec.g, ec.b, 110});

    if (!final_line.empty())
        centered_text::draw_wrapped(final_line.c_str(), sw / 2.0f, 140, 18,
                                    {230, 230, 240, 255});

    char buf[128];
    snprintf(buf, sizeof(buf), "天空颜色: %s  |  Lv%d", sky_color.c_str(), final_level);
    centered_text::draw_small(buf, sw / 2.0f, 260, 14, {180, 200, 180, 255});

    snprintf(buf, sizeof(buf), "Meta奖励: Soul +%d  Knowledge +%d",
             meta_soul, meta_knowledge);
    centered_text::draw_small(buf, sw / 2.0f, 285, 15, {200, 220, 255, 255});

    const int total_runs = g_meta.total_runs();
    snprintf(buf, sizeof(buf), "累计 %d 局  |  Soul: %d  Knowledge: %d",
             total_runs, g_meta.currency().soul_fragments, g_meta.currency().knowledge);
    centered_text::draw_small(buf, sw / 2.0f, 315, 13, {160, 170, 200, 220});

    centered_text::draw_big("按 Enter 观看片尾 →", sw / 2.0f, (float)(sh - 60),
                            22, {255, 200, 50, 255});
}

void VictoryScene::_render() {
    const int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    const Color ec = _ending_tint();
    _render_background(sw, sh, ec);

    if (!g_font_loaded) {
        DrawText("VICTORY!", sw / 2 - 60, 110, 48, {50, 255, 100, 255});
        DrawText("Press Enter", sw / 2 - 50, 180, 18, {200, 240, 200, 255});
        return;
    }
    _render_ending_panel(sw, sh, ec);
}

// G13: 结局相关字段 (结局名/台词/天空色/Meta + NPC 尾声 + 时间线 + 战斗报告)
void VictoryScene::_copy_ending_to(std::shared_ptr<CreditsScene> cs) const {
    cs->ending_name    = ending_name;
    cs->ending_title   = ending_name;
    cs->sky_color      = sky_color;
    cs->final_line     = final_line;
    cs->meta_soul      = meta_soul;
    cs->meta_knowledge = meta_knowledge;

    // NPC epilogues
    cs->npc_count = npc_count;
    for (int i = 0; i < npc_count; i++) {
        cs->npc_names[i]   = npc_names[i];
        cs->npc_results[i] = npc_results[i];
        cs->npc_details[i] = npc_details[i];
    }
    // Timeline
    cs->timeline_count = timeline_count;
    for (int i = 0; i < timeline_count; i++) {
        cs->timeline_times[i]  = timeline_times[i];
        cs->timeline_labels[i] = timeline_labels[i];
    }
    // Battle report
    cs->boss_rank       = boss_rank;
    cs->boss_dmg_done   = boss_dmg_done;
    cs->boss_dmg_taken  = boss_dmg_taken;
    cs->boss_time       = boss_time;
    cs->boss_arena_zones = boss_arena;
}

// G13: 本局战报 + 跨局 Meta
void VictoryScene::_copy_run_summary_to(std::shared_ptr<CreditsScene> cs) const {
    cs->floor_reached    = run_floor;
    cs->bosses_killed    = run_bosses;
    cs->total_kills      = run_kills;
    cs->elite_kills      = run_elites;
    cs->relics_collected = run_relics;
    cs->quests_done      = run_quests;
    cs->combo_max        = run_combo;
    cs->play_time        = run_playtime;

    cs->total_runs = g_meta.total_runs();
    cs->curr_soul  = g_meta.currency().soul_fragments;
    cs->curr_know  = g_meta.currency().knowledge;
}

std::shared_ptr<CreditsScene> VictoryScene::_build_credits_scene() const {
    auto cs = std::make_shared<CreditsScene>();
    cs->name = "CreditsScene";
    _copy_ending_to(cs);
    _copy_run_summary_to(cs);
    return cs;
}

void VictoryScene::_input(const InputMap& input) {
    if (!input.is_action_just_pressed("confirm")) return;
    // D6 Step2: 转入 CreditsScene (片尾演出)
    get_tree()->change_scene(_build_credits_scene());
    LOG_INFO("通关→Credits");
}
