#pragma once
#include "node.h"
#include "input_map.h"

class FloorSelectScene : public Node {
public:
    int max_unlocked = 1;
    int cursor = 0;
    void _render() override;
    void _input(const InputMap& input) override;
    const char* get_bgm_name() const override { return "select"; }

private:
    // G13: 拆 _render, 满足函数 ≤40 行红线
    void _render_header(int sw);                 // 深空渐变底 + 标题带
    void _render_floor_grid(int sw);             // 5×3 楼层格
    void _draw_floor_cell(int cx, int cy, int floor_num);   // 单格 (格底+角标+数字+名)
    void _draw_floor_frame(int cx, int cy, Color bg, Color border, float line_w);
    void _draw_floor_number(int cx, int cy, int floor_num, bool unlocked, bool is_boss);
    void _draw_floor_label(int cx, int cy, int floor_num, Color accent, bool is_boss);
};
