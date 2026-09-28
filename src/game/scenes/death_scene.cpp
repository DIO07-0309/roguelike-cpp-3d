#include "death_scene.h"
#include "title_scene.h"
#include "scene_tree.h"
#include "core/logger.h"
#include "meta_progression.h"     // v1.6-B2: 死因谱 (g_meta.top_death_causes)
#include "rendering/centered_text.h"

extern Font g_font, g_font_small;
extern bool g_font_loaded;

// G13: 血色渐晕底 (脱离纯黑) — 上深下更暗, 中央微红
void DeathScene::_render_background(int sw) {
    ClearBackground({12, 6, 8, 255});
    for (int i = 0; i < 80; i++) {
        const float t = i / 80.0f;
        DrawRectangle(0, i, sw, 1,
            {(unsigned char)(12 + 26 * t), (unsigned char)(6 + 8 * t),
             (unsigned char)(8 + 10 * t), 255});
    }
}

// G13: 大字 + 结局名 + 本局死因 + 结算
void DeathScene::_render_header(int sw) {
    // M3: 大字投影 (静字变尸碑)
    centered_text::draw_big_shadow("你 死 了", sw / 2.0f, 60, 64,
                                   {220, 40, 40, 255}, 160);

    // D6: 结局信息
    if (!ending_name.empty()) {
        const Color ec = (ending_name == "BAD END")  ? Color{200, 80, 60, 255}
                       : (ending_name == "TRUE END") ? Color{255, 220, 80, 255}
                       : Color{200, 200, 200, 255};
        centered_text::draw_big(ending_name.c_str(), sw / 2.0f, 130, 36, ec);
    }
    if (!final_line.empty()) {
        // 取第一行 (避免多行长文本)
        const std::string firstline = final_line.substr(0, final_line.find('\n'));
        centered_text::draw_small(firstline.c_str(), sw / 2.0f, 175, 16,
                                  {220, 200, 180, 240});
    }
    // v1.6-B2: 本局死因 — 死亡界面第一要务 (红字醒目)
    if (!death_cause.empty()) {
        const std::string cause_line = "死于: " + death_cause;
        centered_text::draw_small(cause_line.c_str(), sw / 2.0f, 196, 17,
                                  {255, 90, 70, 245});
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "第%d层  Lv%d", final_floor, final_level);
    centered_text::draw_small(buf, sw / 2.0f, 222, 16, {180, 180, 180, 255});

    if (meta_soul > 0) {
        snprintf(buf, sizeof(buf), "Meta奖励: Soul +%d  Knowledge +%d",
                 meta_soul, meta_knowledge);
        centered_text::draw_small(buf, sw / 2.0f, 240, 14, {200, 220, 255, 200});
    }
}

// G13: 镜像复盘 (仅 F15 死亡有内容 — 死在"自己"手里的特别演出)
void DeathScene::_render_mirror_review(int sw, int sh) {
    if (mirror_verdict.empty()) return;

    float vy = 300.0f;
    centered_text::draw_small("— 镜像复盘 —", sw / 2.0f, vy, 16,
                              {255, 120, 100, 240});
    vy += 24;
    centered_text::draw_small(mirror_verdict.c_str(), sw / 2.0f, vy, 15,
                              {220, 160, 150, 235});
    vy += 26;

    // 逐行画习惯 (手动折行: 每行一个 '\n'; 上限避开死因谱区)
    size_t pos = 0;
    while (pos < mirror_habits.size() && vy < sh - 155) {
        size_t nl = mirror_habits.find('\n', pos);
        if (nl == std::string::npos) nl = mirror_habits.size();
        const std::string line = mirror_habits.substr(pos, nl - pos);
        if (!line.empty()) {
            centered_text::draw_small(line.c_str(), sw / 2.0f, vy, 13,
                                      {170, 140, 135, 225});
            vy += 19;
        }
        pos = nl + 1;
    }
}

// G13: 存档提示 + 死因谱 + 返回提示
void DeathScene::_render_foot(int sw, int sh) {
    centered_text::draw_small("存档已保留，可从选关界面继续挑战",
                              sw / 2.0f, 275, 16, {220, 180, 100, 255});

    // v1.6-B2: 死因谱 — 跨局 Top3 (死得多了才显示, 首死只看本局)
    const auto top_causes = g_meta.top_death_causes(3);
    if (top_causes.size() >= 2) {
        float py = (float)(sh - 160);
        centered_text::draw_small("— 死因谱 —", sw / 2.0f, py, 14,
                                  {200, 140, 120, 220});
        py += 18;
        for (auto& [cause, cnt] : top_causes) {
            char tb[96];
            snprintf(tb, sizeof(tb), "%s ×%d", cause.c_str(), cnt);
            centered_text::draw_small(tb, sw / 2.0f, py, 13,
                                      {170, 130, 120, 215});
            py += 17;
        }
    }
    centered_text::draw_big("按 Enter 返回标题", sw / 2.0f, (float)(sh - 70),
                            22, {200, 200, 200, 255});
}

void DeathScene::_render() {
    const int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    _render_background(sw);

    if (!g_font_loaded) {
        DrawText("YOU DIED", sw / 2 - 60, 130, 48, {220, 40, 40, 255});
        DrawText("Press Enter to return", sw / 2 - 80, 200, 18, {200, 200, 200, 255});
        return;
    }
    _render_header(sw);
    _render_mirror_review(sw, sh);
    _render_foot(sw, sh);
}

void DeathScene::_input(const InputMap& input) {
    if (input.is_action_just_pressed("confirm")) {
        auto ts = std::make_shared<TitleScene>();
        ts->name = "TitleScene";
        get_tree()->change_scene(ts);
        LOG_INFO("死亡→返回标题");
    }
}
