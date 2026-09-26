#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "raylib.h"
#include "types/combat_types.h"
#include "rendering/sprite_renderer.h"
#include "resources/resource_manager.h"

// ============================================================
// effect_drawer — 通用特效原语 (G10.11)
//
// 从 game_renderer.cpp 抽出, 让 TutorialScene 与主游戏共享同一套
// ring / spark 绘制与计时, 保证拾取、受击的手感一致。
// 武器三连击特效 (_draw_slash_arc_* 等) 仍留在 game_renderer.cpp。
// ============================================================
namespace effect_drawer {

// 纹理爆点 (VFX 接入管线; 缺纹理回退几何圆)
inline void draw_blast(float sx, float sy, float base_r,
                       const Color& c, int alpha_scale) {
    char key[28];
    snprintf(key, sizeof(key), "fx_%02x%02x%02x", c.r, c.g, c.b);
    Texture2D tex = ResourceManager::inst().procedural_fx(key,
        (Color){c.r, c.g, c.b, 255});
    if (tex.id > 0) {
        SpriteDef sd; sd.frame_w = 32; sd.frame_h = 32;
        float r = base_r * 2;
        SpriteRenderer::draw_sprite(tex, sd, 0,
            {sx - r, sy - r, r * 2, r * 2},
            Color{255, 255, 255, (unsigned char)alpha_scale});
    } else {
        DrawCircle(sx, sy, base_r, c);
    }
}

// 环形脉冲 (pulse / ring / 默认分支共用)
inline void draw_ring(float sx, float sy, float radius, float prog,
                      const Color& c, int seg = 24) {
    float r = radius * (0.5f + 0.5f * prog);
    DrawRing({sx, sy}, r * 0.6f, r, 0, 360, seg, c);
}

// 通用特效体 — 非武器三连击类型 (ring / pulse / spark / flash / smoke)
inline void draw_generic_effect(const Effect& e, float sx, float sy, float t) {
    float alpha = 1.0f - t / e.duration;
    if (alpha <= 0.0f) return;
    Color c = e.color;
    c.a = (unsigned char)(c.a * alpha);
    float prog = t / e.duration;

    if (e.kind == "spark") {
        draw_blast(sx, sy, e.radius * (0.5f + 0.5f * prog), c,
                   (unsigned char)std::min(255, c.a * 2));
        return;
    }
    if (e.kind == "flash") {
        draw_blast(sx, sy, e.radius, c, (unsigned char)std::min(255, c.a * 2));
        draw_blast(sx, sy, e.radius * 1.5f, c, c.a / 2);
        return;
    }
    if (e.kind == "smoke") {
        DrawCircle(sx, sy, e.radius * (0.3f + 0.7f * prog), Fade(c, 0.5f));
        return;
    }
    draw_ring(sx, sy, e.radius, prog, c, 24);
}

// 计时推进 + 过期剔除 (与 game_scene.cpp 的 active_effects tick 一致)
inline void update_effects(std::vector<Effect>& effects, float dt) {
    for (auto& e : effects) e.elapsed += dt;
    effects.erase(std::remove_if(effects.begin(), effects.end(),
        [](const Effect& e) { return e.elapsed >= e.duration + e.start_delay; }),
        effects.end());
}

}  // namespace effect_drawer
