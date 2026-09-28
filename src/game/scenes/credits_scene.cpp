#include "credits_scene.h"
#include "title_scene.h"
#include "scene_tree.h"
#include "core/logger.h"
#include "meta_progression.h"
#include "rendering/centered_text.h"
#include <cstdio>
#include <cmath>

extern Font g_font, g_font_small;
extern bool g_font_loaded;

void CreditsScene::_ready() {
    _page = 0; _npc_page = 0; _timeline_page = 0; _scroll_y = (float)get_tree()->get_height();
}

CreditsScene::CreditsPhase CreditsScene::_phase() const {
    // 阶段判定 (纯数据, 无外部依赖)
    int npc_total = npc_count * 2; // each NPC = name+detail 2 screens
    if (_page < npc_total) return CreditsPhase::NPC_EPILOGUE;
    if (_page == npc_total) return CreditsPhase::TIMELINE;
    if (_page == npc_total + 1) return CreditsPhase::REPORT;
    if (_page == npc_total + 2) return CreditsPhase::SUMMARY;
    return CreditsPhase::CREDITS;
}

// G13: 滚动字幕单行 (自增 sy, 行距 size * 1.6)
static void draw_credit_line(const char* text, float cx, float& sy, int sz, Color c) {
    const float w = MeasureTextEx(g_font_small, text, (float)sz, 1).x;
    DrawTextEx(g_font_small, text, {cx - w / 2, sy}, (float)sz, 1, c);
    sy += sz * 1.6f;
}

void CreditsScene::_render_npc_epilogue(int sw, int sh) {
    const int npc_idx = _page / 2;      // 0=name, 1=detail
    const bool is_name = (_page % 2 == 0);
    if (npc_idx < npc_count) {
        if (is_name) {
            centered_text::draw_big(npc_names[npc_idx].c_str(), sw / 2.0f,
                                    sh / 2.0f - 40, 32, {255, 220, 100, 255});
            centered_text::draw_small(npc_results[npc_idx].c_str(), sw / 2.0f,
                                      sh / 2.0f + 10, 22, {180, 220, 255, 255});
        } else {
            // detail: 最多3行
            centered_text::draw_wrapped(npc_details[npc_idx].c_str(), sw / 2.0f,
                                        sh / 2.0f - 30, 16, {230, 230, 240, 240}, 1.5f);
        }
    }
    centered_text::draw_small("[ENTER]继续", sw / 2.0f, (float)(sh - 50), 14,
                              {140, 140, 160, 200});
}

void CreditsScene::_render_timeline(int sw, int sh) {
    centered_text::draw_big("战斗时间线", sw / 2.0f, 60, 26, {255, 200, 100, 255});
    float y = 110;
    int show = 0;
    for (int i = 0; i < timeline_count && show < 10; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%5.1fs  %s", timeline_times[i],
                 timeline_labels[i].c_str());
        DrawTextEx(g_font_small, buf, {sw / 2.0f - 100, y}, 16, 1, {200, 220, 255, 255});
        y += 24;
        show++;
    }
    if (timeline_count > 10)
        DrawTextEx(g_font_small, "...", {sw / 2.0f - 10, y}, 16, 1, {120, 120, 150, 200});
    centered_text::draw_small("[ENTER]继续", sw / 2.0f, (float)(sh - 50), 14,
                              {140, 140, 160, 200});
}

void CreditsScene::_render_report(int sw, int sh) {
    centered_text::draw_big("战斗报告", sw / 2.0f, 60, 26, {255, 200, 100, 255});

    float y = 110;
    const auto line = [&](const char* fmt, float val) {
        char buf[32];
        snprintf(buf, sizeof(buf), fmt, val);
        centered_text::draw_small(buf, sw / 2.0f, y, 15, {220, 240, 220, 255});
        y += 26;
    };
    line("BossRank  %s", 0);
    // 评级大字压在 "BossRank" 那一行上
    centered_text::draw_small(boss_rank.c_str(), sw / 2.0f, y - 26, 22, {255, 220, 60, 255});
    line("Damage     %d", (float)boss_dmg_done);
    line("Damage再  %d", (float)boss_dmg_taken);
    line("战时长    %.1fs", boss_time);
    line("Arena区域  %d", (float)boss_arena_zones);

    centered_text::draw_small("[ENTER]继续", sw / 2.0f, (float)(sh - 50), 14,
                              {140, 140, 160, 200});
}

void CreditsScene::_render_summary(int sw, int sh) {
    centered_text::draw_big("旅程总结", sw / 2.0f, 60, 26, {255, 200, 100, 255});

    float y = 105;
    const auto sline = [&](const char* label, int val) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s  %d", label, val);
        centered_text::draw_small(buf, sw / 2.0f, y, 14, {200, 220, 255, 255});
        y += 24;
    };
    sline("到达楼层", floor_reached);
    sline("击杀Boss", bosses_killed);
    sline("击杀总数", total_kills);
    sline("击杀精英", elite_kills);
    sline("收集圣物", relics_collected);
    sline("完成任务", quests_done);
    sline("最高连击", combo_max);

    char tbuf[48];
    const int m = (int)play_time / 60, s = (int)play_time % 60;
    snprintf(tbuf, sizeof(tbuf), "游戏时间  %d:%02d", m, s);
    centered_text::draw_small(tbuf, sw / 2.0f, y, 14, {200, 220, 255, 255});

    centered_text::draw_small("[ENTER]进入片尾", sw / 2.0f, (float)(sh - 50), 14,
                              {140, 140, 160, 200});
}

void CreditsScene::_draw_credits_npc_lines(float cx, float& sy, Color white) {
    // NPC epilogue lines
    for (int i = 0; i < npc_count; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s — %s", npc_names[i].c_str(),
                 npc_results[i].c_str());
        draw_credit_line(buf, cx, sy, 15, white);
    }
    sy += 20;
}

void CreditsScene::_draw_credits_colophon(float cx, float& sy, Color gold, Color dim) {
    draw_credit_line("Developed by Zhou Yutong", cx, sy, 17, gold);
    draw_credit_line("C++ Raylib Roguelike", cx, sy, 14, dim);
    sy += 15;
    draw_credit_line("2026", cx, sy, 14, gold);
    sy += 30;
    draw_credit_line("Special thanks to", cx, sy, 13, dim);
    draw_credit_line("all who played and tested.", cx, sy, 13, dim);
    sy += 40;
    draw_credit_line("The End.", cx, sy, 20, gold);
}

void CreditsScene::_render_credits(int sw, int sh) {
    // 滚动字幕
    const float cx = sw / 2.0f;
    _scroll_y -= 30.0f * GetFrameTime();   // 30px/s
    float sy = _scroll_y;

    const Color gold  = {255, 220, 80, 255};
    const Color white = {220, 220, 240, 255};
    const Color dim   = {160, 160, 190, 200};

    draw_credit_line(ending_title.c_str(), cx, sy, 28, gold);
    sy += 20;
    draw_credit_line("─ ─ ─ ─ ─ ─ ─ ─ ─ ─", cx, sy, 16, dim);
    sy += 30;

    _draw_credits_npc_lines(cx, sy, white);

    draw_credit_line("─ ─ ─ ─ ─ ─ ─ ─ ─ ─", cx, sy, 16, dim);
    sy += 20;
    draw_credit_line(sky_color.c_str(), cx, sy, 20, gold);
    draw_credit_line(final_line.c_str(), cx, sy, 14, white);
    sy += 20;
    draw_credit_line("─ ─ ─ ─ ─ ─ ─ ─ ─ ─", cx, sy, 16, dim);
    sy += 30;

    _draw_credits_colophon(cx, sy, gold, dim);

    // 字幕结束后提示
    if (sy < 60)
        DrawTextEx(g_font_small, "[ENTER] 返回标题", {cx - 60, (float)(sh - 50)}, 16, 1,
                   {255, 200, 50, 255});
}

void CreditsScene::_render() {
    ClearBackground(BLACK);
    const int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    if (!g_font_loaded) return;

    switch (_phase()) {
    case CreditsPhase::NPC_EPILOGUE: _render_npc_epilogue(sw, sh); break;
    case CreditsPhase::TIMELINE:     _render_timeline(sw, sh); break;
    case CreditsPhase::REPORT:       _render_report(sw, sh); break;
    case CreditsPhase::SUMMARY:      _render_summary(sw, sh); break;
    case CreditsPhase::CREDITS:      _render_credits(sw, sh); break;
    }
}

void CreditsScene::_input(const InputMap& input) {
    if (!input.is_action_just_pressed("confirm")) return;

    CreditsPhase ph = _phase();
    if (ph == CreditsPhase::CREDITS) {
        if (_scroll_y > (float)(get_tree()->get_height())) return; // 还在滚动
        auto ts = std::make_shared<TitleScene>();
        ts->name = "TitleScene";
        get_tree()->change_scene(ts);
        LOG_INFO("Credits→返回标题");
        return;
    }

    // 推进页面
    _page++;
}