#pragma once
#include <string>
#include "raylib.h"

// ============================================================
// centered_text — 结算/结局类界面的居中排版原语 (G13)
//
// victory / death / credits / floor_select 等界面反复出现
// "MeasureTextEx 量宽 → 居中 DrawTextEx" 的模式, 抽成共用原语,
// 避免每个 _render() 手抄一遍 (同时也是把超长 _render 拆小的抓手)
// ============================================================

// 全局字体 (定义在字体加载处, 各 .cpp 各自 extern; 这里必须放全局作用域,
// 放进 namespace 会变成 centered_text::g_font 这个不存在的符号)
extern Font g_font, g_font_small;

namespace centered_text {

// 居中单行 (小字体)
inline void draw_small(const char* text, float cx, float y, int size, Color c) {
    const float w = MeasureTextEx(g_font_small, text, size, 1).x;
    DrawTextEx(g_font_small, text, {cx - w / 2, y}, size, 1, c);
}

// 居中单行 (主字体)
inline void draw_big(const char* text, float cx, float y, int size, Color c) {
    const float w = MeasureTextEx(g_font, text, size, 1).x;
    DrawTextEx(g_font, text, {cx - w / 2, y}, size, 1, c);
}

// 居中主字体 + 硬阴影 (静字变碑文; 阴影偏移固定 3px)
inline void draw_big_shadow(const char* text, float cx, float y, int size,
                            Color c, unsigned char shadow_alpha = 150) {
    const float w = MeasureTextEx(g_font, text, size, 1).x;
    DrawTextEx(g_font, text, {cx - w / 2 + 3, y + 3}, size, 1,
               Color{0, 0, 0, shadow_alpha});
    DrawTextEx(g_font, text, {cx - w / 2, y}, size, 1, c);
}

// 居中多行 ('\n' 分隔), 行距 = size * line_mul (默认 1.4)
inline void draw_wrapped(const char* text, float cx, float y, int size, Color c,
                         float line_mul = 1.4f) {
    std::string s(text);
    size_t pos = 0;
    float ly = y;
    while (pos < s.size()) {
        const size_t nl = s.find('\n', pos);
        const std::string line = (nl == std::string::npos) ? s.substr(pos)
                                                            : s.substr(pos, nl - pos);
        const float lw = MeasureTextEx(g_font_small, line.c_str(), size, 1).x;
        DrawTextEx(g_font_small, line.c_str(), {cx - lw / 2, ly}, size, 1, c);
        ly += size * line_mul;
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
}

}  // namespace centered_text
