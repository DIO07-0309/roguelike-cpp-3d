#include "game_scene_render_pass.h"
#include "scenes/game_scene.h"
#include "raylib.h"
#include <string>

extern Font g_font_small;
extern bool g_font_loaded;

// ============================================================
// GameSceneRenderPass — 全屏状态界面 (G12-2)
// 从 GameScene::_render 提取, 保持绘制顺序与视觉输出不变
// ============================================================

bool GameSceneRenderPass::draw_element_select(int sw, int sh) {
    if (!_s.element_select_active) return false;

    ClearBackground({20, 15, 30, 255});

    const char* title = "选择你的元素核心";
    float tw = MeasureTextEx(g_font_small, title, 28, 1).x;
    DrawTextEx(g_font_small, title, {sw/2.0f - tw/2, 40}, 28, 1, {255,220,180,255});

    float card_w = 280, card_h = 300, gap = 20;
    float start_x = sw/2.0f - (card_w * 3 + gap * 2)/2.0f;
    for (int i = 0; i < 3; i++) {
        draw_element_card(i, start_x + i * (card_w + gap), (sh - card_h)/2.0f + 20,
                          card_w, card_h, i == _s.element_select_cursor);
    }

    const char* ft = "选择后永久绑定，本局及以后所有存档不可更改";
    float fw = MeasureTextEx(g_font_small, ft, 14, 1).x;
    DrawTextEx(g_font_small, ft, {sw/2.0f - fw/2, (float)(sh - 30)}, 14, 1, {150,150,150,180});
    return true;
}

void GameSceneRenderPass::draw_element_card(int index, float cx, float cy,
                                            float card_w, float card_h, bool selected) {
    const char* icons[] = {
        "[火] 火焰核心", "[冰] 冰霜核心", "[毒] 剧毒核心"
    };
    const char* long_desc[] = {
        "每次攻击有概率触发火焰暴击\n暴击伤害 x1.5\nLv1 暴击率 15%，Lv20 约 30%",
        "每击附加减速\n累计减速层数触发冻结(1秒)\nLv1 冻结率 10%，Lv20 约 100%",
        "每击附加持续毒伤\nDOT = 本次伤害 x 比例\nLv1 毒伤 5%，Lv20 约 15%"
    };
    const Color colors[] = {
        {255,120,30,255}, {100,200,255,255}, {80,220,80,255}
    };

    Color bg = selected ? Color{50,50,80,255} : Color{25,25,45,255};
    Color border = selected ? colors[index] : Color{50,50,75,220};

    DrawRectangleRounded({cx, cy, card_w, card_h}, 0.1f, 8, bg);
    DrawRectangleRoundedLines({cx-1, cy-1, card_w+2, card_h+2}, 0.1f, 8, 2.5f, border);

    float iw = MeasureTextEx(g_font_small, icons[index], 32, 1).x;
    DrawTextEx(g_font_small, icons[index], {cx + card_w/2 - iw/2, cy + 25}, 32, 1, colors[index]);

    draw_card_description(long_desc[index], cx, cy + 80, card_w);

    if (selected) {
        DrawTextEx(g_font_small, "[←/→选择] [空格/E 确认]",
            {cx + card_w/2 - 110, cy + card_h - 35}, 14, 1, {255,255,180,220});
    }
}

void GameSceneRenderPass::draw_card_description(const char* text,
                                                float cx, float top_y, float card_w) {
    float dy = top_y;
    std::string line;
    for (const char* p = text; *p; p++) {
        if (*p == '\n') {
            float lw = MeasureTextEx(g_font_small, line.c_str(), 13, 1).x;
            DrawTextEx(g_font_small, line.c_str(),
                {cx + card_w/2 - lw/2, dy}, 13, 1, {200,210,200,200});
            dy += 22;
            line.clear();
        } else {
            line += *p;
        }
    }
    if (!line.empty()) {
        float lw = MeasureTextEx(g_font_small, line.c_str(), 13, 1).x;
        DrawTextEx(g_font_small, line.c_str(),
            {cx + card_w/2 - lw/2, dy}, 13, 1, {200,210,200,200});
    }
}

bool GameSceneRenderPass::draw_boss_intro_screen(int sw, int sh) {
    if (_s.state != GameState::BOSS_INTRO) return false;

    // F15.5: Mirror analysis panel for Ending Echo
    if (_s.boss_floor == 15 && _s._boss._behavior_type == "mirror") {
        _s._draw_mirror_analysis_panel(sw, sh);
    } else {
        _s._renderer.draw_boss_intro(sw, sh, _s.boss_intro_title, _s.boss_intro_lore,
                                     _s.boss_intro_skills, _s.boss_intro_color, _s.boss_floor,
                                     _s.boss_intro_visual);
    }
    // D4 Step5.5: BossNarrative覆盖对话 (显示在面板下方)
    if (!_s._presentation.boss_intro_text.empty() && g_font_loaded) {
        float tw = MeasureTextEx(g_font_small, _s._presentation.boss_intro_text.c_str(), 17, 1).x;
        DrawTextEx(g_font_small, _s._presentation.boss_intro_text.c_str(),
                   {sw/2.0f - tw/2, (float)(sh - 100)}, 17, 1, {255, 220, 100, 240});
    }
    // D5 Step1: BossModifier文字 (金色Warning风格)
    if (!_s._presentation.boss_modifier_text.empty() && g_font_loaded) {
        float mw = MeasureTextEx(g_font_small, _s._presentation.boss_modifier_text.c_str(), 15, 1).x;
        DrawRectangle(sw/2.0f - mw/2 - 12, (float)(sh - 72), mw + 24, 24,
                      {30, 15, 15, 200});
        DrawTextEx(g_font_small, _s._presentation.boss_modifier_text.c_str(),
                   {sw/2.0f - mw/2, (float)(sh - 68)}, 15, 1, {255, 80, 40, 240});
    }
    return true;
}
