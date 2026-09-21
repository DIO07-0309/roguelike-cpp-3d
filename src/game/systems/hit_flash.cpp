#include "hit_flash.h"

float HitFlash::s_elapsed = 0.0f;
float HitFlash::s_duration = 0.0f;
Color HitFlash::s_color = WHITE;
bool HitFlash::s_active = false;

void HitFlash::trigger(float duration, Color color) {
    s_elapsed = 0.0f;
    s_duration = duration;
    s_color = color;
    s_active = true;
}

void HitFlash::update(float delta_time) {
    if (!s_active) return;
    
    s_elapsed += delta_time;
    if (s_elapsed >= s_duration) {
        s_active = false;
    }
}

void HitFlash::draw(float entity_x, float entity_y, float radius) {
    if (!s_active) return;
    
    float t = s_elapsed / s_duration;
    Color c = s_color;
    c.a = (unsigned char)(c.a * (1.0f - t));
    
    // 绘制闪白（白色叠加）
    DrawCircle(entity_x, entity_y, radius, Fade(c, 0.5f));
}

bool HitFlash::is_active() {
    return s_active;
}

float HitFlash::remaining() {
    if (!s_active) return 0.0f;
    return s_duration - s_elapsed;
}
