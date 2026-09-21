#pragma once
#include "raylib.h"

// ============================================================
// HitFlash - 命中闪白效果
// ============================================================

class HitFlash {
public:
    // 触发闪白
    static void trigger(float duration = 0.1f, Color color = WHITE);
    
    // 更新（每帧调用）
    static void update(float delta_time);
    
    // 绘制（叠加到实体上）
    static void draw(float entity_x, float entity_y, float radius);
    
    // 是否活跃
    static bool is_active();
    
    // 剩余时间
    static float remaining();
    
private:
    static float s_elapsed;
    static float s_duration;
    static Color s_color;
    static bool s_active;
};
