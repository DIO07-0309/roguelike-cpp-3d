#pragma once
#include "game/build_score.h"     // BuildType
#include <functional>

// ═══════════════════════════════════════════════════════════════
// G41: 主题预览控制器 — 轮换全部流派, 每个流派产出一张预览图
//
// 动机: G39/G40 改的多是"看不出数值差异"的视觉/手感参数
// (屏震幅度 / FOV / 特效半径 / 音效), 逐项实机验证要进游戏找对应
// 怪物触发特效, 成本极高且难以横向对比。本控制器把 12 个流派放在
// 同一场景、同一组特效下各拍一张, 直接得到可对比的预览图。
//
// 红线: 本类只负责"何时切换/是否拍完", 不含任何渲染或场景知识。
// ═══════════════════════════════════════════════════════════════
class ThemePreviewController {
public:
    void enable() { _enabled = true; }
    bool active() const { return _enabled; }
    bool finished() const { return _finished; }
    int  shot_index() const { return _index; }

    // 到期切换到下一个流派并回调 on_switch(bt, index)。
    // on_switch 内由调用方负责: 应用主题 / 重建预览特效 / 请求截图。
    // dt 由控制器自取 (GetFrameTime) — 预览是纯表现层节奏, 不参与模拟，
    // 也让调用方不必为传 dt 把已超长的 _process 再撑一行。
    void tick(const std::function<void(BuildType, int)>& on_switch);

private:
    // 每个流派停留时长 — 须小于最短的特效存活期 (duration*life_scale,
    // 最短 雷系 4.0*0.85=3.4s), 保证截图时特效仍在场
    static constexpr float kHoldSeconds = 0.6f;

    bool  _enabled = false;
    bool  _finished = false;
    bool  _first_frame = true;
    int   _index = 0;
    float _timer = 0.0f;
};
