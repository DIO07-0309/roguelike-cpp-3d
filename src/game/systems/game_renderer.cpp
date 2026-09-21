#include "game_renderer.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "item.h"
#include "combat_system.h"
#include "boss.h"
#include "config.h"
#include "save/save_manager.h"   // G10.9-C5: HUD 槽位提示
#include "vfx_server.h"
#include "boss.h"
#include "build_score.h"
#include "relic_progression.h"
#include "attack_evolution.h"   // G1
#include "skill_evolution.h"   // G1 Step3
#include "resource_manager.h"                 // M4f.2
#include "game/rendering/sprite_renderer.h"   // M4f.2
#include "particle_system.h"                 // A9: 粒子系统
#include <cmath>
#include <algorithm>
#include <cstdio>

// 字体指针 (在 main.cpp 中初始化)
extern Font g_font;
extern Font g_font_small;
extern bool g_font_loaded;

// ============================================================
// A8: 技能图标纹理缓存
// ============================================================
static Texture2D g_skill_icons[4] = {};
static bool g_skill_icons_loaded = false;

static void load_skill_icons() {
    if (g_skill_icons_loaded) return;
    
    const char* paths[] = {
        "assets/icons/skills/skill_slash.png",      // 斩击
        "assets/icons/skills/skill_divine.png",     // 神罚
        "assets/icons/skills/skill_timestop.png",   // 时停
        "assets/icons/skills/skill_heal.png",       // 治愈
    };
    
    for (int i = 0; i < 4; i++) {
        if (FileExists(paths[i])) {
            g_skill_icons[i] = LoadTexture(paths[i]);
        }
    }
    g_skill_icons_loaded = true;
}

static int skill_icon_index(const Skill* skill) {
    if (!skill) return 0;
    // 根据技能名称或标签判断图标
    // 斩击 - 物理/近战
    if (skill->has_tag(BuildTag::MELEE) || skill->has_tag(BuildTag::COMBO)) return 0;
    // 神罚 - 魔法/AOE
    if (skill->has_tag(BuildTag::MAGIC) || skill->has_tag(BuildTag::AOE)) return 1;
    // 时停 - 时间
    if (skill->has_tag(BuildTag::TIME)) return 2;
    // 治愈 - 恢复
    if (skill->has_tag(BuildTag::HEAL)) return 3;
    return 0; // fallback: 斩击
}

// ============================================================
// 静态绘制工具
// ============================================================
void GameRenderer::draw_panel(Rectangle r, const char* title, Color bg) {
    DrawRectangleRounded(r, 0.08f, 8, bg);
    DrawRectangleRoundedLines(r, 0.08f, 8, 2, {100, 100, 180, 255});
    if (title && g_font_loaded)
        DrawTextEx(g_font_small, title, {r.x + 12, r.y + 8}, 18, 1, {200, 200, 255, 255});
}

void GameRenderer::draw_glow_text(const char* text, float x, float y, int size, Color c,
                                   bool center) {
    if (!g_font_loaded) return;
    float w = MeasureTextEx(g_font, text, (float)size, 1).x;
    if (center) x -= w / 2;
    DrawTextEx(g_font, text, {x + 1, y + 1}, size, 1, {0, 0, 0, 100});
    DrawTextEx(g_font, text, {x, y}, size, 1, c);
}

void GameRenderer::draw_progress_bar(Rectangle r, float ratio, Color fill, Color bg) {
    DrawRectangleRec(r, bg);
    DrawRectangleRec({r.x, r.y, r.width * ratio, r.height}, fill);
    DrawRectangleLinesEx(r, 1, {60, 60, 90, 255});
}

// G10.3-B3: HUD 像素图标 — 16px 网格风格代码绘制 (与 gen_pixel_blast 同管线风格)
static void _draw_gold_icon(float x, float y, float s) {
    // 金币: 外圈金 + 内圈亮金 + 阴影底
    DrawRectangleRec({x, y + s * 0.25f, s, s * 0.75f}, Color{0, 0, 0, 70});
    DrawCircle(x + s / 2, y + s / 2, s * 0.42f, Color{212, 160, 40, 255});
    DrawCircle(x + s / 2, y + s / 2, s * 0.28f, Color{255, 214, 90, 255});
    DrawCircle(x + s * 0.38f, y + s * 0.38f, s * 0.08f, Color{255, 240, 180, 255});
}

static void _draw_key_icon(float x, float y, float s) {
    // 钥匙: 铜色圆环头 + 柄 + 齿
    DrawRectangleRec({x, y + s * 0.25f, s, s * 0.75f}, Color{0, 0, 0, 70});
    DrawCircleLines((int)(x + s * 0.3f), (int)(y + s * 0.42f), s * 0.18f, Color{190, 160, 90, 255});
    DrawRectangleRec({x + s * 0.42f, y + s * 0.36f, s * 0.5f, s * 0.12f}, Color{190, 160, 90, 255});
    DrawRectangleRec({x + s * 0.72f, y + s * 0.48f, s * 0.1f, s * 0.16f}, Color{190, 160, 90, 255});
    DrawRectangleRec({x + s * 0.58f, y + s * 0.48f, s * 0.08f, s * 0.12f}, Color{190, 160, 90, 255});
}

// ============================================================
// 摄像机
// ============================================================
void GameRenderer::update_camera(float& cam_x, float& cam_y, const Player* player,
                                  const GameMap* map, int screen_w, int screen_h) {
    if (!player) return;
    cam_x = player->entity.rect.x + player->entity.rect.width / 2 - screen_w / 2;
    cam_y = player->entity.rect.y + player->entity.rect.height / 2 - screen_h / 2;
    if (map) {
        cam_x = std::max(0.0f, std::min(cam_x, (float)map->pixel_width - screen_w));
        cam_y = std::max(0.0f, std::min(cam_y, (float)map->pixel_height - screen_h));
    }
}

// ============================================================
// 特效
// ============================================================

// G5.8.8-fix: 方向斩弧（含刃线与散点）
// G10.5-B B2: 平分线=玩家朝向角 (原四朝向恒偏 30°, 与 SECTOR 判定角平分线不一致)
// 屏幕角: DOWN=90 UP=270 RIGHT=0 LEFT=180; 弧扫 facing±60°
static void _draw_slash_arc(const Effect& e, float sx, float sy,
                            float prog, Color c) {
    float arc_r = e.radius * (0.6f + 0.4f * prog);
    float facing = 90;   // DOWN
    switch (e.direction) {
        case Direction::DOWN:  facing = 90;  break;
        case Direction::UP:    facing = 270; break;
        case Direction::RIGHT: facing = 0;   break;
        case Direction::LEFT:  facing = 180; break;
    }
    float startAngle = facing - 60;
    float endAngle = facing + 60;
    // 贴图地板较亮: 先画深色厚底弧保证亮色弧可见
    DrawRing({sx, sy}, arc_r * 0.4f - 1.5f, arc_r + 1.5f, startAngle, endAngle, 12,
             {0, 0, 0, 130});
    DrawRing({sx, sy}, arc_r * 0.4f, arc_r, startAngle, endAngle, 12, c);
    DrawLineEx({sx, sy},
               {sx + cosf((startAngle + 60) * DEG2RAD) * arc_r,
                sy + sinf((startAngle + 60) * DEG2RAD) * arc_r}, 3, c);
    for (int i = 0; i < 5; i++) {
        float a = (startAngle + (float)(rand() % 120)) * DEG2RAD;
        float dist = arc_r * (0.5f + (float)(rand() % 50) / 100.0f);
        DrawCircle(sx + cosf(a) * dist, sy + sinf(a) * dist, 2.5f, Fade(c, 0.5f));
    }
}

// M4f.2: 纹理爆点 (VFX 接入管线; 缺纹理回退几何圆)
static void _draw_fx_blast(float sx, float sy, float base_r,
                           const Color& c, int alpha_scale) {
    char key[28];
    snprintf(key, sizeof(key), "fx_%02x%02x%02x", c.r, c.g, c.b);
    Texture2D tex = ResourceManager::inst().procedural_fx(
        key, (Color){c.r, c.g, c.b, 255});
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

// 环形脉冲 (pulse/ring/默认分支共用)
static void _draw_fx_ring(float sx, float sy, float radius, float prog,
                          const Color& c, int seg) {
    float r = radius * (0.5f + 0.5f * prog);
    DrawRing({sx, sy}, r * 0.6f, r, 0, 360, seg, c);
}

// ============================================================
// A9: 武器三连击特效
// ============================================================

// 剑（扇形斩）三连击
static void _draw_slash_arc_1(const Effect& e, float sx, float sy, float prog, Color c) {
    float radius = e.radius * (0.5f + 0.5f * prog);
    DrawRing({sx, sy}, radius * 0.8f, radius, -45, 90, 16, c);
}

static void _draw_slash_arc_2(const Effect& e, float sx, float sy, float prog, Color c) {
    float radius = e.radius * (0.5f + 0.5f * prog);
    DrawRing({sx, sy}, radius * 0.8f, radius, -45, 90, 16, c);
    DrawRing({sx, sy}, radius * 0.6f, radius * 0.8f, -30, 60, 16, Fade(c, 0.7f));
}

static void _draw_slash_arc_3(const Effect& e, float sx, float sy, float prog, Color c) {
    float radius = e.radius * (0.5f + 0.5f * prog);
    DrawRing({sx, sy}, radius * 0.8f, radius, -45, 90, 16, c);
    DrawRing({sx, sy}, radius * 0.6f, radius * 0.8f, -30, 60, 16, Fade(c, 0.7f));
    DrawRing({sx, sy}, radius * 0.4f, radius * 0.6f, -15, 30, 16, Fade(c, 0.5f));
    
    // 粒子拖尾
    EmitterConfig config;
    config.position = {e.world_x, e.world_y};
    config.velocity_min = {-20, -20};
    config.velocity_max = {20, 20};
    config.color = c;
    config.end_color = Color{255, 255, 255, 0};
    config.duration_min = 0.3f;
    config.duration_max = 0.5f;
    config.size_min = 2.0f;
    config.size_max = 4.0f;
    ParticleSystem::emit(config, 3);
}

// 矛（穿透）三连击
static void _draw_pierce_beam_1(const Effect& e, float sx, float sy, float prog, Color c) {
    float length = e.radius * 3.0f;
    float angle = atan2f(e.target_y - e.world_y, e.target_x - e.world_x) * 180.0f / 3.14159f;
    DrawLineEx({sx, sy}, {e.target_x - sx, e.target_y - sy}, 3, c);
    // 命中火花
    _draw_fx_blast(e.target_x, e.target_y, 16, c, 200);
}

static void _draw_pierce_beam_2(const Effect& e, float sx, float sy, float prog, Color c) {
    float length = e.radius * 3.0f;
    DrawLineEx({sx, sy}, {e.target_x - sx, e.target_y - sy}, 4, c);
    // 分裂光束
    Vector2 start = {sx, sy};
    Vector2 end = {e.target_x - sx, e.target_y - sy};
    Vector2 mid = {start.x + (end.x - start.x) * 0.5f, start.y + (end.y - start.y) * 0.5f};
    DrawLineEx(mid, {mid.x + 20, mid.y - 20}, 2, Fade(c, 0.7f));
    DrawLineEx(mid, {mid.x - 20, mid.y + 20}, 2, Fade(c, 0.7f));
    _draw_fx_blast(e.target_x, e.target_y, 20, c, 200);
}

static void _draw_pierce_beam_3(const Effect& e, float sx, float sy, float prog, Color c) {
    DrawLineEx({sx, sy}, {e.target_x - sx, e.target_y - sy}, 5, c);
    // 贯穿光束 + 命中火花
    _draw_fx_blast(e.target_x, e.target_y, 24, c, 255);
    
    // 粒子效果
    EmitterConfig config;
    config.position = {e.target_x, e.target_y};
    config.velocity_min = {-50, -50};
    config.velocity_max = {50, 50};
    config.color = c;
    config.end_color = Color{255, 255, 255, 0};
    config.duration_min = 0.2f;
    config.duration_max = 0.4f;
    config.size_min = 3.0f;
    config.size_max = 6.0f;
    ParticleSystem::emit(config, 5);
}

// 双截棍（追踪）三连击
static void _draw_whip_arc_1(const Effect& e, float sx, float sy, float prog, Color c) {
    float radius = e.radius * (0.5f + 0.5f * prog);
    DrawRing({sx, sy}, radius * 0.8f, radius, -60, 120, 24, c);
}

static void _draw_whip_arc_2(const Effect& e, float sx, float sy, float prog, Color c) {
    float radius = e.radius * (0.5f + 0.5f * prog);
    DrawRing({sx, sy}, radius * 0.8f, radius, -60, 120, 24, c);
    DrawRing({sx, sy}, radius * 0.6f, radius * 0.7f, -45, 90, 24, Fade(c, 0.7f));
}

static void _draw_whip_arc_3(const Effect& e, float sx, float sy, float prog, Color c) {
    float radius = e.radius * (0.5f + 0.5f * prog);
    DrawRing({sx, sy}, radius * 0.8f, radius, -60, 120, 24, c);
    DrawRing({sx, sy}, radius * 0.6f, radius * 0.7f, -45, 90, 24, Fade(c, 0.7f));
    DrawRing({sx, sy}, radius * 0.4f, radius * 0.4f, -30, 60, 24, Fade(c, 0.5f));
    
    // 残影
    for (int i = 0; i < 3; i++) {
        float offset = prog * 30.0f + i * 10.0f;
        DrawRing({sx + offset, sy}, radius * 0.3f, radius * 0.5f, -30, 60, 16, Fade(c, 0.3f - i * 0.1f));
    }
}

// 连弩（弹幕）三连击
static void _draw_bolt_spread_1(const Effect& e, float sx, float sy, float prog, Color c) {
    DrawLineEx({sx, sy}, {e.target_x - sx, e.target_y - sy}, 2, c);
    _draw_fx_blast(e.target_x, e.target_y, 12, c, 150);
}

static void _draw_bolt_spread_2(const Effect& e, float sx, float sy, float prog, Color c) {
    // 双箭
    DrawLineEx({sx, sy}, {e.target_x - sx, e.target_y - sy}, 2, c);
    DrawLineEx({sx, sy}, {e.target_x - sx - 20, e.target_y - sy}, 2, Fade(c, 0.7f));
    _draw_fx_blast(e.target_x, e.target_y, 16, c, 200);
}

static void _draw_bolt_spread_3(const Effect& e, float sx, float sy, float prog, Color c) {
    // 多箭
    DrawLineEx({sx, sy}, {e.target_x - sx, e.target_y - sy}, 2, c);
    DrawLineEx({sx, sy}, {e.target_x - sx - 20, e.target_y - sy}, 2, Fade(c, 0.7f));
    DrawLineEx({sx, sy}, {e.target_x - sx + 20, e.target_y - sy}, 2, Fade(c, 0.7f));
    
    // 爆炸
    _draw_fx_blast(e.target_x, e.target_y, 24, c, 255);
    
    // 粒子
    EmitterConfig config;
    config.position = {e.target_x, e.target_y};
    config.velocity_min = {-40, -40};
    config.velocity_max = {40, 40};
    config.color = c;
    config.end_color = Color{255, 255, 100, 0};
    config.duration_min = 0.3f;
    config.duration_max = 0.5f;
    config.size_min = 2.0f;
    config.size_max = 5.0f;
    ParticleSystem::emit(config, 8);
}

// 重锤（重击）三连击
static void _draw_smash_impact_1(const Effect& e, float sx, float sy, float prog, Color c) {
    float radius = e.radius * (0.5f + 0.5f * prog);
    DrawRing({sx, sy}, radius * 0.3f, radius, 0, 360, 16, c);
}

static void _draw_smash_impact_2(const Effect& e, float sx, float sy, float prog, Color c) {
    float radius = e.radius * (0.5f + 0.5f * prog);
    DrawRing({sx, sy}, radius * 0.3f, radius, 0, 360, 16, c);
    DrawRing({sx, sy}, radius * 0.6f, radius * 1.2f, 0, 360, 16, Fade(c, 0.7f));
}

static void _draw_smash_impact_3(const Effect& e, float sx, float sy, float prog, Color c) {
    float radius = e.radius * (0.5f + 0.5f * prog);
    DrawRing({sx, sy}, radius * 0.3f, radius, 0, 360, 16, c);
    DrawRing({sx, sy}, radius * 0.6f, radius * 1.2f, 0, 360, 16, Fade(c, 0.7f));
    DrawRing({sx, sy}, radius * 0.9f, radius * 1.5f, 0, 360, 16, Fade(c, 0.5f));
    
    // 碎石粒子
    EmitterConfig config;
    config.position = {e.world_x, e.world_y};
    config.velocity_min = {-60, -60};
    config.velocity_max = {60, 60};
    config.color = Color{150, 100, 80, 255};
    config.end_color = Color{80, 50, 30, 0};
    config.duration_min = 0.5f;
    config.duration_max = 0.8f;
    config.size_min = 3.0f;
    config.size_max = 8.0f;
    ParticleSystem::emit(config, 10);
}

// G5.8.8-fix: 单特效渲染 — 合并原 VFXServer::draw 全部 kind 分支
static void _draw_effect_body(const Effect& e, float sx, float sy,
                              float cam_x, float cam_y, float t) {
    float alpha = 1.0f - t / e.duration;
    if (alpha <= 0) return;
    Color c = e.color; c.a = (unsigned char)(c.a * alpha);
    float prog = t / e.duration;

    // 剑（扇形斩）三连击
    if (e.kind == "slash_arc_1") {
        _draw_slash_arc_1(e, sx, sy, prog, c);
    } else if (e.kind == "slash_arc_2") {
        _draw_slash_arc_2(e, sx, sy, prog, c);
    } else if (e.kind == "slash_arc_3") {
        _draw_slash_arc_3(e, sx, sy, prog, c);
    }
    // 矛（穿透）三连击
    else if (e.kind == "pierce_beam_1") {
        _draw_pierce_beam_1(e, sx, sy, prog, c);
    } else if (e.kind == "pierce_beam_2") {
        _draw_pierce_beam_2(e, sx, sy, prog, c);
    } else if (e.kind == "pierce_beam_3") {
        _draw_pierce_beam_3(e, sx, sy, prog, c);
    }
    // 双截棍（追踪）三连击
    else if (e.kind == "whip_arc_1") {
        _draw_whip_arc_1(e, sx, sy, prog, c);
    } else if (e.kind == "whip_arc_2") {
        _draw_whip_arc_2(e, sx, sy, prog, c);
    } else if (e.kind == "whip_arc_3") {
        _draw_whip_arc_3(e, sx, sy, prog, c);
    }
    // 连弩（弹幕）三连击
    else if (e.kind == "bolt_spread_1") {
        _draw_bolt_spread_1(e, sx, sy, prog, c);
    } else if (e.kind == "bolt_spread_2") {
        _draw_bolt_spread_2(e, sx, sy, prog, c);
    } else if (e.kind == "bolt_spread_3") {
        _draw_bolt_spread_3(e, sx, sy, prog, c);
    }
    // 重锤（重击）三连击
    else if (e.kind == "smash_impact_1") {
        _draw_smash_impact_1(e, sx, sy, prog, c);
    } else if (e.kind == "smash_impact_2") {
        _draw_smash_impact_2(e, sx, sy, prog, c);
    } else if (e.kind == "smash_impact_3") {
        _draw_smash_impact_3(e, sx, sy, prog, c);
    }
    // 现有特效类型（保留）
    else if (e.kind == "pulse" || e.kind == "ring") {
        _draw_fx_ring(sx, sy, e.radius, prog, c, 24);
    } else if (e.kind == "spark") {
        _draw_fx_blast(sx, sy, e.radius * (0.5f + 0.5f * prog), c,
                       (unsigned char)std::min(255, c.a * 2));
    } else if (e.kind == "bolt") {
        DrawLineEx({sx, sy}, {e.target_x - cam_x, e.target_y - cam_y}, 3, c);
    } else if (e.kind == "flash") {
        _draw_fx_blast(sx, sy, e.radius, c,
                       (unsigned char)std::min(255, c.a * 2));
        _draw_fx_blast(sx, sy, e.radius * 1.5f, c, c.a / 2);
    } else if (e.kind == "smoke") {
        float sr = e.radius * (0.3f + 0.7f * prog);
        DrawCircle(sx, sy, sr, Fade(c, 0.5f));
    } else if (e.kind == "shield_ring") {
        float pulse = 0.8f + 0.2f * sinf(t * 8.0f);
        DrawRing({sx, sy}, e.radius * 0.7f, e.radius, 0, 360, 16,
                 Color{(unsigned char)c.r, (unsigned char)c.g, (unsigned char)c.b,
                       (unsigned char)(c.a * pulse)});
    } else if (e.kind == "slash_arc") {
        _draw_slash_arc(e, sx, sy, prog, c);
    } else if (e.kind == "cone") {
        DrawRectangleLines(sx - e.radius / 2, sy - e.radius / 4,
                           e.radius, e.radius / 2, c);
    } else {
        _draw_fx_ring(sx, sy, e.radius, prog, c, 12);
    }
}

void GameRenderer::draw_effects(const std::vector<Effect>& effects, float cam_x, float cam_y) {
    for (auto& e : effects) {
        float t = e.elapsed - e.start_delay;
        if (t < 0) continue;
        float sx = e.world_x - cam_x, sy = e.world_y - cam_y;
        _draw_effect_body(e, sx, sy, cam_x, cam_y, t);
    }
}

// ============================================================
// 覆盖层
// ============================================================
void GameRenderer::draw_time_stop_overlay(int sw, int sh, float time_stop_remaining) {
    DrawRectangle(0, 0, sw, sh, {90, 90, 100, 130});
    int remain = (int)time_stop_remaining;
    char buf[4]; snprintf(buf, sizeof(buf), "%d", remain);
    draw_glow_text(buf, sw / 2.0f, sh / 2.0f, 80, WHITE, true);
    draw_glow_text("The World · 时停", sw / 2.0f, 60, 24, WHITE, true);
}

// M4.2: 镜像冻结 overlay — 红霜 + 剩余秒数 + 提示 (玩家被冻结, Echo 可行动)
void GameRenderer::draw_mirror_freeze_overlay(int sw, int sh, float freeze_remaining) {
    DrawRectangle(0, 0, sw, sh, {120, 30, 40, 90});
    DrawRectangle(0, sh - 64, sw, 64, {120, 30, 40, 140});
    char buf[64];
    snprintf(buf, sizeof(buf), "镜像时停冻结 · %.1fs", freeze_remaining);
    draw_glow_text(buf, sw / 2.0f, sh - 56, 26, {255, 120, 120, 255}, true);
    draw_glow_text("你被冻结了 — 镜像仍可行动！", sw / 2.0f, sh - 28, 18, {255, 200, 200, 255}, true);
}

void GameRenderer::draw_boss_cinematic_overlay(int sw, int sh) {
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 160});
    draw_glow_text("BOSS 来了！", sw / 2.0f, sh / 2.0f, 48, {230, 50, 50, 255}, true);
}

void GameRenderer::draw_boss_intro(int sw, int sh, const std::string& title,
                                    const std::string& lore,
                                    const std::string& skills_text, Color color,
                                    int boss_floor, const std::string& visual_id) {
    ClearBackground(BLACK);
    float pw = 500, ph = 380;
    Rectangle pr = {sw / 2.0f - pw / 2, sh / 2.0f - ph / 2, pw, ph};
    draw_panel(pr, "! Boss 遭遇 !");

    // M5-C: Boss 立绘数据驱动 (visual_id 优先, 未注册时回退按层链)
    {
        auto& rm = ResourceManager::inst();
        const char* key = nullptr;
        SpriteDef probe;
        std::string vkey = "boss_" + visual_id;
        if (!visual_id.empty()
            && rm.sprite_by_key(vkey.c_str(), probe).id > 0) {
            key = vkey.c_str();
        } else {
            key = (boss_floor >= 15) ? "boss_self"
                 : (boss_floor >= 10) ? "boss_f10" : "boss_f5";
        }
        SpriteDef sd; sd.frame_w = 16; sd.frame_h = 16;
        Texture2D tex = rm.sprite_by_key(key, sd);
        if (tex.id > 0) {
            Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
            Rectangle dst = {pr.x + pr.width - 70, pr.y + 8, 48, 48};
            DrawTexturePro(tex, src, dst, {0, 0}, 0, WHITE);
            DrawRectangleLinesEx({dst.x - 2, dst.y - 2, dst.width + 4, dst.height + 4},
                                 1, {color.r, color.g, color.b, 160});
        }
    }

    draw_glow_text(title.c_str(), sw / 2.0f, pr.y + 45, 30, color, true);

    if (g_font_loaded) {
        // M3: 技能行自动换行 (原固定单行溢出面板)
        {
            std::string s(skills_text);
            float max_w = pw - 80;
            size_t pos = 0; float ly = pr.y + 160;
            while (pos < s.size() && ly < pr.y + ph - 130) {
                size_t take = s.size() - pos;
                while (take > 4 && MeasureTextEx(g_font_small,
                        s.substr(pos, take).c_str(), 16, 1).x > max_w)
                    take--;
                std::string line = s.substr(pos, take);
                // 尽量在逗号/句号处断行
                if (pos + take < s.size()) {
                    size_t cut = line.find_last_of("，。;；");
                    if (cut != std::string::npos && cut > 4) take = cut + 1;
                }
                DrawTextEx(g_font_small, line.c_str(), {pr.x + 40, ly}, 16, 1,
                           {180, 180, 180, 255});
                ly += 24;
                pos += take;
            }
        }
        // M3: 剧情文本换行 (原单行溢出)
        {
            std::string s(lore);
            float max_w = pw - 80;
            size_t pos = 0; float ly = pr.y + 210;
            while (pos < s.size() && ly < pr.y + ph - 60) {
                size_t take = s.size() - pos;
                while (take > 4 && MeasureTextEx(g_font,
                        s.substr(pos, take).c_str(), 18, 1).x > max_w)
                    take--;
                std::string line = s.substr(pos, take);
                if (pos + take < s.size()) {
                    size_t cut = line.find_last_of("，。；!？");
                    if (cut != std::string::npos && cut > 4) take = cut + 1;
                }
                DrawTextEx(g_font, line.c_str(), {pr.x + 40, ly}, 18, 1,
                           {160, 160, 180, 255});
                ly += 26;
                pos += take;
            }
        }
    }
    draw_glow_text("按 Enter 进入战斗...", sw / 2.0f, (float)(sh - 60), 20,
                   {140, 20, 20, 255}, true);
}

void GameRenderer::draw_room_message(int sw, int sh, const std::string& msg, float timer) {
    if (timer <= 0 || msg.empty() || !g_font_loaded) return;

    // C1: 平滑淡出 (ease-out: quadratic)
    float raw = std::min(1.0f, timer / 0.6f);
    float alpha = raw * raw;  // ease-out curve
    bool is_relic = (msg.size() > 6 && msg.substr(0, 6) == "RELIC:");
    std::string display = is_relic ? msg.substr(6) : msg;

    float tw = MeasureTextEx(g_font_small, display.c_str(), 18, 1).x;
    float px = sw / 2.0f - tw / 2;
    float py = (float)(sh - 70);

    // C1: Relic 获得 — 金色背景
    Color bg = is_relic
        ? Color{40, 30, 10, (unsigned char)(200 * alpha)}
        : Color{10, 10, 20, (unsigned char)(180 * alpha)};
    Color fg = is_relic
        ? Color{255, 220, 60, (unsigned char)(255 * alpha)}
        : Color{255, 255, 200, (unsigned char)(255 * alpha)};
    Color border = is_relic
        ? Color{255, 200, 50, (unsigned char)(180 * alpha)}
        : Color{80, 80, 120, (unsigned char)(160 * alpha)};

    DrawRectangleRounded({px - 16, py - 4, tw + 32, 28}, 0.15f, 6, bg);
    DrawRectangleRoundedLines({px - 16, py - 4, tw + 32, 28}, 0.15f, 6, 1, border);
    DrawTextEx(g_font_small, display.c_str(), {px, py}, 18, 1, fg);
}

// ── v1.6-B1: 镜像阶段晋升横幅 — "它学会了" 高优先级播报 ──
// phase: 2=镜像期(开始模仿你) 3=进化期(完整克制你); timer 3s 渐隐
void GameRenderer::draw_phase_banner(int sw, int sh, int phase, float timer) {
    if (timer <= 0 || !g_font_loaded) return;
    float alpha = std::min(1.0f, timer / 0.8f);        // 尾部 0.8s 淡出
    float slide = (1.0f - alpha) * 30.0f;             // 淡出时上滑
    const char* big = (phase >= 3) ? "它看穿了你的套路" : "它开始模仿你";
    const char* sub = (phase >= 3) ? "ECHO · 进化完成 — 完整克制策略上线"
                                   : "MIRROR · 镜像期 — 你的习惯正在被复刻";
    Color bigc = (phase >= 3) ? Color{255, 80, 60, 255} : Color{230, 120, 90, 255};
    // 全宽暗带 (Boss 战画面之上, 横幅可读性)
    DrawRectangle(0, (int)(sh * 0.30f), sw, 96,
        {10, 4, 4, (unsigned char)(150 * alpha)});
    float big_w = MeasureTextEx(g_font, big, 34, 1).x;
    DrawTextEx(g_font, big, {sw / 2.0f - big_w / 2, sh * 0.30f + 14 - slide},
        34, 1, {bigc.r, bigc.g, bigc.b, (unsigned char)(255 * alpha)});
    float sub_w = MeasureTextEx(g_font_small, sub, 15, 1).x;
    DrawTextEx(g_font_small, sub, {sw / 2.0f - sub_w / 2, sh * 0.30f + 60 - slide},
        15, 1, {220, 170, 150, (unsigned char)(220 * alpha)});
}

// ============================================================
// HUD 渲染
// ============================================================
Color GameRenderer::_relic_rarity_color(const std::string& rarity) {
    if (rarity == "rare")  return Color{100, 170, 255, 255};
    if (rarity == "epic") return Color{190, 100, 255, 255};
    return Color{255, 220, 100, 255}; // common
}

std::string GameRenderer::_rarity_label_cn(const std::string& rarity) {
    if (rarity == "rare")  return "稀有";
    if (rarity == "epic") return "史诗";
    return "普通";
}

void GameRenderer::draw_skill_bar(const Player* player, float game_time) {
    auto& active = player->skills.active_skills;
    if (active.empty() || !g_font_loaded) return;
    
    load_skill_icons();
    
    float x = 10.0f;
    float y = 56.0f;
    float skill_size = 40.0f;
    float spacing = 4.0f;
    
    for (int i = 0; i < (int)active.size(); i++) {
        const Skill* skill = active[i].get();
        if (!skill) continue;
        
        float ry = y + i * (skill_size + spacing);
        bool ready = skill->can_use(game_time);
        
        // 背景（半透明）
        DrawRectangleRounded(
            {x, ry, skill_size, skill_size},
            2.0f,
            3,
            Color{30, 30, 40, 180}
        );
        
        // 边框
        DrawRectangleRoundedLines(
            {x, ry, skill_size, skill_size},
            2.0f,
            3,
            1.0f,
            Color{80, 70, 90, 200}
        );
        
        // 技能图标贴图
        int icon_idx = skill_icon_index(skill);
        if (g_skill_icons[icon_idx].id > 0) {
            Rectangle src = {0, 0, (float)g_skill_icons[icon_idx].width, (float)g_skill_icons[icon_idx].height};
            Rectangle dst = {x + 4, ry + 4, skill_size - 8, skill_size - 8};
            Color tmod = ready ? WHITE : Color{100, 100, 100, 150};
            DrawTexturePro(g_skill_icons[icon_idx], src, dst, {0, 0}, 0, tmod);
        } else {
            // Fallback: 彩色方块
            Color skill_color;
            if (skill->has_tag(BuildTag::FIRE)) {
                skill_color = Color{200, 50, 50, 255};
            } else if (skill->has_tag(BuildTag::ICE)) {
                skill_color = Color{50, 150, 255, 255};
            } else if (skill->has_tag(BuildTag::POISON)) {
                skill_color = Color{100, 200, 50, 255};
            } else {
                skill_color = Color{200, 200, 200, 255};
            }
            DrawRectangleRec(
                {x + 8, ry + 8, skill_size - 16, skill_size - 16},
                skill_color
            );
        }
        
        // 技能编号
        char num_buf[4];
        snprintf(num_buf, sizeof(num_buf), "%d", i + 1);
        DrawTextEx(g_font_small, num_buf, {x + 14, ry + 14}, 14, 1, WHITE);
        
        // 技能名称（图标右侧）
        std::string label = skill->name + " " + skill->get_level_text();
        Color label_c = ready ? Color{180, 220, 255, 255} : Color{100, 100, 100, 255};
        if (skill->evolution_level > 0) label_c = ready ? Color{255, 200, 50, 255} : Color{140, 120, 50, 255};
        DrawTextEx(g_font_small, label.c_str(), {x + skill_size + 8, ry + 10}, 14, 1, label_c);
        
        // 冷却进度条（技能名称下方）
        float cd_r = 1.0f;
        if (skill->cooldown > 0.0f) {
            cd_r = 1.0f - skill->remaining_cooldown(game_time) / skill->cooldown;
        }
        draw_progress_bar({x + skill_size + 8, ry + 28, 90, 8}, cd_r,
                          ready ? Color{60, 180, 255, 255} : Color{70, 70, 70, 255});
        
        // 冷却数字倒计时（冷却时显示在图标上）
        if (skill->cooldown > 0.0f && !ready) {
            float cd_remaining = skill->remaining_cooldown(game_time);
            if (cd_remaining < 10.0f) {
                char cd_buf[16];
                snprintf(cd_buf, sizeof(cd_buf), "%.1f", cd_remaining);
                float text_w = MeasureTextEx(g_font_small, cd_buf, 14, 1).x;
                DrawTextEx(
                    g_font_small,
                    cd_buf,
                    {x + (skill_size - text_w) / 2, ry + skill_size / 2 - 7},
                    14, 1,
                    Color{255, 255, 255, 255}
                );
            }
        }
        
        // 升级标识（技能等级角标）
        if (skill->evolution_level > 0) {
            char level_buf[8];
            snprintf(level_buf, sizeof(level_buf), "E%d", skill->evolution_level);
            DrawTextEx(
                g_font_small,
                level_buf,
                {x + skill_size - 20, ry + skill_size - 14},
                10, 1,
                Color{255, 215, 0, 230}
            );
        }
    }
}

// 文本折行绘制: 超宽时按 UTF-8 码点折行, 返回行数
static int _draw_wrapped_text(const std::string& text, float x, float y,
                              float max_w, int font_size, float line_h, Color c) {
    if (MeasureTextEx(g_font_small, text.c_str(), font_size, 1).x <= max_w) {
        DrawTextEx(g_font_small, text.c_str(), {x, y}, font_size, 1, c);
        return 1;
    }
    std::string cur;
    int lines = 0;
    for (size_t i = 0; i < text.size(); ) {
        unsigned char ch = (unsigned char)text[i];
        size_t len = (ch < 0x80) ? 1 : (ch < 0xE0) ? 2 : (ch < 0xF0) ? 3 : 4;
        if (i + len > text.size()) break;
        std::string nxt = cur + text.substr(i, len);
        if (!cur.empty() && MeasureTextEx(g_font_small, nxt.c_str(), font_size, 1).x > max_w) {
            DrawTextEx(g_font_small, cur.c_str(), {x, y + lines * line_h}, font_size, 1, c);
            lines++;
            cur = text.substr(i, len);
        } else {
            cur = nxt;
        }
        i += len;
    }
    if (!cur.empty()) {
        DrawTextEx(g_font_small, cur.c_str(), {x, y + lines * line_h}, font_size, 1, c);
        lines++;
    }
    return lines;
}

// C1: Buff icon mapping
static const char* _buff_icon(const std::string& id) {
    if (id == "attack_up") return "攻";
    if (id == "poison")    return "毒";
    if (id == "pool_poison" || id == "poison2s") return "毒";
    if (id == "slow")      return "缓";
    if (id == "freeze")    return "冻";
    if (id == "bleed")     return "血";
    if (id == "burn")      return "燃";
    if (id == "stun")      return "晕";
    if (id == "fear")      return "惧";
    if (id == "electrified") return "雷";
    if (id == "defense_up")  return "防";
    return "?";
}

void GameRenderer::draw_player_buffs(const Player* player) {
    if (!player || player->active_buffs.empty() || !g_font_loaded) return;
    auto& buffs = player->active_buffs;
    float x = 10;
    float y = 56.0f + player->skills.active_skills.size() * 28.0f + 4.0f;
    for (auto& b : buffs) {
        Color c = get_buff_hud_color(b.id);
        const BuffDef* def = get_buff_def(b.id);
        // D9: 高亮即将结束的 buff (<1.5s 闪烁)
        if (b.remaining < 1.5f) {
            float flicker = sinf((float)GetTime() * 10.0f);
            c.a = (unsigned char)(160 + (int)(flicker * 80));
        }
        // D9: 剩余时间进度条 (宽60px, 高4px)
        if (def && def->duration > 0) {
            float bar_w = 60.0f, bar_h = 4.0f;
            float ratio = b.remaining / def->duration;
            Color bar_c = ratio > 0.3f ? Color{100, 200, 100, 220}
                        : ratio > 0.1f ? Color{200, 200, 40, 220} : Color{200, 40, 40, 220};
            DrawRectangle(x + 26, y + 12, bar_w, bar_h, Color{30, 30, 30, 200});
            DrawRectangle(x + 26, y + 12, bar_w * ratio, bar_h, bar_c);
        }
        std::string line = std::string(_buff_icon(b.id)) + " "
            + get_buff_display_name(b.id)
            + " x" + std::to_string(b.stacks)
            + " " + format_buff_time(b.remaining);
        DrawTextEx(g_font_small, line.c_str(), {x, y}, 14, 1, c);
        y += 18;
    }
}

void GameRenderer::draw_player_relics(const Player* player) {
    if (!player || player->relics.empty() || !g_font_loaded) return;

    float x = 10;
    float base_y = 56.0f + player->skills.active_skills.size() * 28.0f + 4.0f;
    if (!player->active_buffs.empty())
        base_y += player->active_buffs.size() * 18.0f + 4.0f;

    DrawTextEx(g_font_small, "圣物:", {x, base_y}, 13, 1, Color{255, 220, 100, 255});
    float cx = x + MeasureTextEx(g_font_small, "圣物:", 13, 1).x + 4.0f;

    for (auto& r : player->relics) {
        const RelicDef* def = get_relic_def(r.id);
        if (!def) continue;
        Color rc = _relic_rarity_color(def->rarity);
        std::string token = def->short_name + std::string(" ");
        DrawTextEx(g_font_small, token.c_str(), {cx, base_y}, 13, 1, rc);
        cx += MeasureTextEx(g_font_small, token.c_str(), 13, 1).x;
    }
}

void GameRenderer::draw_relic_panel(const Player* player, int sw) {
    if (!player || !g_font_loaded) return;

    int count = (int)player->relics.size();
    float line_h = 24.0f;
    float panel_w = 370.0f;
    float panel_x = (float)sw - panel_w - 20.0f;
    float panel_y = 70.0f;
    // D4.6 Step4: 面板高度 + 收集率行
    float panel_h = 60.0f + (count > 0 ? count * line_h : line_h) + 22.0f;

    DrawRectangleRounded({panel_x, panel_y, panel_w, panel_h}, 0.08f, 8, Color{15, 15, 35, 220});
    DrawRectangleRoundedLines({panel_x, panel_y, panel_w, panel_h}, 0.08f, 8, 1.5f,
                              Color{100, 100, 160, 200});

    // D4.6 Step4: 标题行 + 收集进度
    int coll = g_relic_archive.collected_count();
    int total = g_relic_archive.total_relic_count();
    char title_buf[80];
    snprintf(title_buf, sizeof(title_buf), "圣物图鉴  %d/%d (%.0f%%)",
             coll, total, g_relic_archive.collection_pct() * 100.0f);
    DrawTextEx(g_font_small, title_buf, {panel_x + 14, panel_y + 10}, 16, 1,
               Color{255, 255, 200, 255});

    if (count == 0) {
        DrawTextEx(g_font_small, "本层尚未获得圣物。",
                   {panel_x + 14, panel_y + 40}, 14, 1, Color{160, 160, 180, 255});
        return;
    }

    float ly = panel_y + 38.0f;
    for (auto& r : player->relics) {
        const RelicDef* def = get_relic_def(r.id);
        if (!def) continue;

        Color rc = _relic_rarity_color(def->rarity);
        // D9: 稀有度描边色 (边框+左侧小条)
        Color border_c = rc;
        border_c.a = 120;
        DrawRectangleRoundedLines({panel_x + 8, ly - 2, panel_w - 24, line_h},
                                  0.10f, 3, 1, border_c);
        // rarity indicator bar
        DrawRectangle(panel_x + 14, ly + 2, 4, line_h - 8, rc);

        std::string label = "[" + _rarity_label_cn(def->rarity) + "]";
        int mlv = g_relic_archive.mastery_level(r.id);
        std::string mstars;
        for (int s = 0; s < mlv; s++) mstars += "★";
        char line[256];
        snprintf(line, sizeof(line), "%s %s %s - %s",
                 mstars.c_str(), label.c_str(), def->name.c_str(), def->desc.c_str());
        DrawTextEx(g_font_small, line, {panel_x + 24, ly}, 14, 1, rc);
        ly += line_h;
    }
}

void GameRenderer::draw_monster_buffs(const Monster& m, float draw_x, float draw_y) {
    if (m.active_buffs.empty() || !g_font_loaded) return;
    std::string label;
    int shown = 0;
    for (auto& b : m.active_buffs) {
        if (shown >= 4) break;
        if (!label.empty()) label += " ";
        // G10: show stacks for all buffs, bold prefix
        if (b.stacks > 1)
            label += std::string(_buff_icon(b.id)) + std::to_string(b.stacks);
        else
            label += std::string(_buff_icon(b.id));
        shown++;
    }
    float tw = MeasureTextEx(g_font_small, label.c_str(), 14, 1).x;
    float px = draw_x + (m.entity.size.x - tw) / 2;
    float py = draw_y - 26; // above name label
    // Shadow for readability
    DrawTextEx(g_font_small, label.c_str(), {px + 1, py + 1}, 14, 1, {0, 0, 0, 220});
    DrawTextEx(g_font_small, label.c_str(), {px - 1, py - 1}, 14, 1, {0, 0, 0, 220});
    // Colored label with bright tone
    Color c = get_buff_hud_color(m.active_buffs[0].id);
    c.a = 240;
    DrawTextEx(g_font_small, label.c_str(), {px, py}, 14, 1, c);
}

void GameRenderer::draw_inventory_panel(const Player* player, int cursor, int sw, int sh) {
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 180});
    float pw = 500, ph = 480;
    Rectangle pr = {sw / 2.0f - pw / 2, sh / 2.0f - ph / 2, pw, ph};
    draw_panel(pr, "背包 B关闭");

    auto& inv = player->inventory;
    float x0 = pr.x + 30;
    float max_w = pw - 60.0f;
    float y = pr.y + 40;

    if (g_font_loaded) {
        std::string wdesc = "武器: " + (inv.equipped.at("weapon")
            ? inv.equipped.at("weapon")->get_description() : std::string("空"));
        y += _draw_wrapped_text(wdesc, x0, y, max_w, 18, 20.0f, {255, 200, 50, 255}) * 20.0f;
        std::string adesc = "防具: " + (inv.equipped.at("armor")
            ? inv.equipped.at("armor")->get_description() : std::string("空"));
        y += _draw_wrapped_text(adesc, x0, y, max_w, 18, 20.0f, {255, 200, 50, 255}) * 20.0f;
    } else {
        y += 40.0f;
    }
    y += 12.0f;
    DrawLine(x0, y, pr.x + pw - 30, y, {60, 60, 90, 255});

    const int kPage = Inventory::kPageSize;
    int item_count = (int)inv.items.size();
    int max_page = std::max(0, (item_count + kPage - 1) / kPage - 1);
    int page = cursor / kPage;
    int start = page * kPage;
    int end = std::min(start + kPage, item_count);
    float bottom_limit = pr.y + ph - 58.0f;
    float iy = y + 14.0f;
    for (int i = start; i < end; i++) {
        std::string mk = (i == cursor) ? ">" : " ";
        char idx[4]; snprintf(idx, sizeof(idx), "%2d", i + 1);
        std::string txt = mk + " [" + idx + "] " + inv.items[i]->get_description();
        // M4f.13: 物品图标 (16px 贴图)
        const char* ikey = item_icon_key(inv.items[i].get());
        if (ikey) {
            SpriteDef xd;
            Texture2D itex = ResourceManager::inst().sprite_by_key(ikey, xd);
            if (itex.id > 0)
                SpriteRenderer::draw_sprite(itex, xd, 0, {pr.x + 8, iy + 1, 20, 20});
        }
        if (g_font_loaded) {
            int n = _draw_wrapped_text(txt, pr.x + 34, iy, pw - 64.0f, 18, 22.0f, inv.items[i]->color);
            iy += n * 22.0f;
            if (iy > bottom_limit) break;
        } else {
            iy += 30.0f;
        }
    }
    if (g_font_loaded) {
        if (max_page > 0) {
            char page_buf[32];
            snprintf(page_buf, sizeof(page_buf), "第 %d/%d 页 (←→翻页)", page + 1, max_page + 1);
            DrawTextEx(g_font_small, page_buf,
                       {pr.x + 30, pr.y + ph - 52}, 14, 1, {160, 160, 200, 255});
        }
        // Batch 3A: Gold + Batch 3H: Key count (same line)
        char gold_buf[32];
        snprintf(gold_buf, sizeof(gold_buf), "金币:%d  钥匙:%d", player->gold, player->key_count);
        DrawTextEx(g_font_small, gold_buf,
                   {pr.x + pw - 160, pr.y + ph - 52}, 14, 1, Color{220, 200, 100, 220});
        // Show sell value of selected item
        if (cursor >= 0 && cursor < item_count) {
            int sv = get_sell_value(inv.items[cursor].get());
            char sv_buf[32];
            snprintf(sv_buf, sizeof(sv_buf), "售价: %d", sv);
            DrawTextEx(g_font_small, sv_buf,
                       {pr.x + pw - 120, pr.y + ph - 38}, 14, 1, Color{180, 160, 80, 200});
        }
        DrawTextEx(g_font_small, "^v选择 X装备 T出售 U使用 D丢弃 B关闭",
                   {pr.x + (pw - 260) / 2, pr.y + ph - 18}, 16, 1, {140, 140, 140, 255});
    }
}

// ============================================================
// Batch 3H: Gamble Room UI panel
// ============================================================
void GameRenderer::draw_gamble_panel(const Player* player, const std::string& result_msg,
                                     float result_timer, int sw, int sh) {
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 180});
    float pw = 420, ph = 340;
    Rectangle pr = {sw / 2.0f - pw / 2, sh / 2.0f - ph / 2, pw, ph};
    draw_panel(pr, "赌徒的轮盘");

    if (!g_font_loaded) return;

    float x0 = pr.x + 30;
    float y = pr.y + 50;
    float max_w = pw - 60;

    // Player gold
    char gold_buf[32];
    snprintf(gold_buf, sizeof(gold_buf), "金币: %d", player->gold);
    DrawTextEx(g_font_small, gold_buf, {x0, y}, 18, 1, Color{220, 200, 100, 255});
    y += 30;

    // Odds table
    DrawTextEx(g_font_small, "— 奖池概率 —", {x0, y}, 16, 1, {160, 160, 200, 255});
    y += 24;
    DrawTextEx(g_font_small, "65%  随机装备", {x0, y}, 14, 1, {200, 200, 200, 255});
    y += 18;
    DrawTextEx(g_font_small, "20%  钥匙 x1", {x0, y}, 14, 1, {200, 200, 200, 255});
    y += 18;
    DrawTextEx(g_font_small, "10%  金币 x10", {x0, y}, 14, 1, {200, 200, 200, 255});
    y += 18;
    DrawTextEx(g_font_small, " 5%  圣物", {x0, y}, 14, 1, {255, 220, 100, 255});
    y += 28;

    // Cost
    DrawTextEx(g_font_small, "每次抽奖: 20 金币", {x0, y}, 16, 1, {255, 255, 100, 255});
    y += 28;

    // Result message
    if (result_timer > 0 && !result_msg.empty()) {
        Color rc = (result_msg.find("RELIC:") == 0)
            ? Color{255, 220, 80, 255} : Color{180, 255, 180, 255};
        std::string display = result_msg;
        if (display.find("RELIC:") == 0)
            display = "圣物: " + display.substr(6);
        _draw_wrapped_text(display, x0, y, max_w, 18, 18.0f, rc);
        y += 36;
    }

    // Controls
    DrawTextEx(g_font_small, "[E] 抽奖   [B] 关闭",
               {pr.x + (pw - 180) / 2, pr.y + ph - 20}, 14, 1, {140, 140, 140, 255});
}

void GameRenderer::draw_challenge_portal(float cam_x, float cam_y,
    int portal_tx, int portal_ty, float pulse_timer, bool is_entry)
{
    float px = portal_tx * 32.0f + 16.0f - cam_x;
    float py = portal_ty * 32.0f + 16.0f - cam_y;
    float pulse = 0.8f + 0.2f * sinf(pulse_timer * 3.0f);
    Color outer = is_entry ? Color{80, 180, 255, (unsigned char)(200 * pulse)}
                           : Color{100, 255, 150, (unsigned char)(200 * pulse)};
    Color inner = is_entry ? Color{150, 220, 255, (unsigned char)(220 * pulse)}
                           : Color{180, 255, 200, (unsigned char)(220 * pulse)};
    DrawCircleV({px, py}, 12.0f, outer);
    DrawCircleV({px, py}, 8.0f, inner);
    if (g_font_loaded) {
        const char* label = is_entry ? "按 [E] 开始挑战" : "按 [E] 返回";
        float tw = MeasureTextEx(g_font_small, label, 12, 1).x;
        DrawTextEx(g_font_small, label, {px - tw / 2, py - 24}, 12, 1,
                   is_entry ? Color{200, 220, 255, 220} : Color{180, 255, 200, 220});
    }
}

void GameRenderer::draw_teleport_fade(int sw, int sh, float fade_timer, bool fading_in) {
    if (fade_timer <= 0) return;
    float alpha = fading_in ? (0.5f - fade_timer) / 0.5f : fade_timer / 0.5f;
    if (alpha < 0) alpha = 0;
    if (alpha > 1) alpha = 1;
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, (unsigned char)(255 * alpha)});
}

void GameRenderer::draw_challenge_choice(int sw, int sh, int cursor) {
    if (!g_font_loaded) return;
    float pw = 320, ph = 140;
    float cx = sw/2.0f - pw/2, cy = sh/2.0f - ph/2;
    DrawRectangleRounded({cx, cy, pw, ph}, 0.1f, 8, {20, 20, 40, 230});
    DrawRectangleRoundedLines({cx-1, cy-1, pw+2, ph+2}, 0.1f, 8, 2.0f, {80, 180, 255, 200});
    const char* title = "挑战房间";
    float tw = MeasureTextEx(g_font_small, title, 18, 1).x;
    DrawTextEx(g_font_small, title, {cx + pw/2 - tw/2, cy + 12}, 18, 1, {255, 220, 100, 255});
    const char* opts[] = {"开始挑战 (消耗1把钥匙)", "离开"};
    for (int i = 0; i < 2; i++) {
        float oy = cy + 50 + i * 36;
        Color c = (i == cursor) ? Color{255, 255, 200, 255} : Color{180, 180, 200, 200};
        if (i == cursor) DrawRectangleRounded({cx + 15, oy - 2, pw - 30, 30}, 0.1f, 4, {60, 60, 100, 120});
        DrawTextEx(g_font_small, opts[i], {cx + 30, oy + 4}, 14, 1, c);
    }
    const char* hint = "[↑↓选择] [E确认] [ESC离开]";
    float hw = MeasureTextEx(g_font_small, hint, 11, 1).x;
    DrawTextEx(g_font_small, hint, {cx + pw/2 - hw/2, cy + ph - 22}, 11, 1, {120, 120, 160, 180});
}

// ============================================================
// F15.5.1: Mirror HUD — character panel helpers
// ============================================================

void GameRenderer::_draw_panel_skills(const std::vector<SkillDisplay>& skills,
                                       float x, float y, bool mirror) {
    float ry = y;
    for (int i = 0; i < (int)skills.size(); i++) {
        auto& sk = skills[i];
        Color bg = sk.ready ? Color{50, 160, 50, 255} : Color{60, 60, 60, 255};
        if (mirror) bg = sk.ready ? Color{140, 40, 40, 255} : Color{50, 20, 20, 255};
        DrawRectangleRounded({x, ry, 22, 18}, 0.2f, 3, bg);
        DrawTextEx(g_font_small, std::to_string(i + 1).c_str(), {x + 7, ry + 1}, 14, 1, WHITE);
        Color lc = sk.ready ? Color{180, 220, 255, 255} : Color{100, 100, 100, 255};
        if (mirror) lc = sk.ready ? Color{200, 140, 140, 255} : Color{100, 60, 60, 255};
        DrawTextEx(g_font_small, sk.name.c_str(), {x + 26, ry + 2}, 14, 1, lc);
        if (mirror) {
            DrawLine(x + 4, ry + 4, x + 16, ry + 14, {80, 20, 20, 120});
            DrawLine(x + 8, ry + 2, x + 14, ry + 10, {80, 20, 20, 80});
        }
        draw_progress_bar({x + 26, ry + 16, 90, 8}, sk.cooldown_ratio,
                          sk.ready ? Color{60, 180, 255, 255} : Color{70, 70, 70, 255});
        ry += 28;
    }
}

void GameRenderer::_draw_panel_buffs(const std::vector<BuffDisplay>& buffs,
                                      float x, float y, bool mirror) {
    float ry = y;
    for (auto& b : buffs) {
        Color ic = mirror ? Color{180, 100, 100, 255} : Color{255, 220, 100, 255};
        DrawTextEx(g_font_small, b.icon.c_str(), {x, ry}, 16, 1, ic);
        Color lc = mirror ? Color{160, 120, 120, 255} : Color{200, 220, 255, 255};
        DrawTextEx(g_font_small, b.label.c_str(), {x + 18, ry + 1}, 13, 1, lc);
        ry += 18;
    }
}

void GameRenderer::draw_character_panel(const CharacterPanelData& d, float px, float py) {
    if (!g_font_loaded) return;
    float pw = 240.0f;
    // Phase opacity
    float op = 1.0f;
    if (d.mirror_mode && d.mirror_phase == 1) op = 0.55f;
    else if (d.mirror_mode && d.mirror_phase == 3)
        op = 0.82f + 0.18f * sinf((float)GetTime() * 4.0f);
    // Panel frame
    Color pbg = d.mirror_mode ? Color{25, 8, 8, 230} : Color{15, 15, 35, 220};
    Color bc  = d.mirror_mode ? Color{120, 30, 30, 200} : Color{60, 60, 120, 180};
    pbg.a = (unsigned char)(pbg.a * op); bc.a = (unsigned char)(bc.a * op);
    int rows = 2 + (int)d.skills.size() + (int)d.buffs.size();
    if (!d.mirror_mode && d.level > 0) rows++;
    float ph = rows * 24.0f + 20.0f;
    DrawRectangleRounded({px, py, pw, ph}, 0.15f, 4, pbg);
    DrawRectangleRoundedLines({px, py, pw, ph}, 0.15f, 4, 1.5f, bc);
    // Name
    Color nc = d.mirror_mode ? Color{200, 60, 50, 255} : Color{220, 220, 255, 255};
    DrawTextEx(g_font_small, d.name, {px + 10, py + 4}, 16, 1, nc);
    float ly = py + 24;
    if (d.sub_label && d.sub_label[0]) {
        Color sc = d.mirror_mode ? Color{160, 90, 80, (unsigned char)(200*op)}
                                 : Color{160, 180, 200, 200};
        DrawTextEx(g_font_small, d.sub_label, {px + 10, ly}, 11, 1, sc);
        ly += 14;
    }
    // HP bar
    float hr = d.max_hp > 0 ? (float)d.hp / d.max_hp : 0.0f;
    if (hr > 1.0f) hr = 1.0f;
    Color hf = d.mirror_mode ? Color{160, 30, 30, 255} : Color{50, 200, 50, 255};
    Color hbg = d.mirror_mode ? Color{50, 10, 10, 255} : Color{40, 20, 20, 255};
    draw_progress_bar({px + 10, ly, pw - 20, 14}, hr, hf, hbg);
    ly += 18;
    // Stats
    char buf[96];
    snprintf(buf, sizeof(buf), "HP:%d/%d  ATK:%d", d.hp, d.max_hp, d.atk);
    Color stc = d.mirror_mode ? Color{180, 130, 130, 255} : Color{200, 200, 200, 255};
    DrawTextEx(g_font_small, buf, {px + 10, ly}, 12, 1, stc);
    ly += 16;
    // XP (player only)
    if (!d.mirror_mode && d.level > 0) {
        float xr = d.xp_to_next > 0 ? (float)d.xp / d.xp_to_next : 0.0f;
        draw_progress_bar({px + 10, ly, pw - 20, 8}, xr, {80, 120, 255, 255});
        snprintf(buf, sizeof(buf), "Lv%d", d.level);
        DrawTextEx(g_font_small, buf, {px + 12, ly - 2}, 10, 1, {180, 200, 255, 255});
        ly += 12;
    }
    ly += 2;
    // Skills + Buffs
    if (!d.skills.empty())
        _draw_panel_skills(d.skills, px + 10, ly, d.mirror_mode);
    ly += d.skills.size() * 28.0f + 4.0f;
    if (!d.buffs.empty())
        _draw_panel_buffs(d.buffs, px + 10, ly, d.mirror_mode);

    // M4e + v1.6-B1: 镜像学习区 — "它眼中的你" 常驻 (观察期起);
    // 4 臂胜率条仅决策后叠加
    if (d.mirror_mode)
        _draw_mirror_learning(d, px, py, ph);
}

// v1.6-B1: "它眼中的你" — 风格/Top3习惯/准确率 (Boss 层常驻, 观察期起)
// 布局: 紧贴 Echo 面板下方; 高度按习惯条数自适应 (54~116px)
void GameRenderer::_draw_mirror_learning(const CharacterPanelData& d,
                                         float px, float py, float panel_h) {
    float ly2 = py + panel_h + 8.0f;
    // ── 上段: 它眼中的你 (常驻) ──
    int habit_rows = 0;
    for (int i = 0; i < 3; i++)
        if (d.mirror_habits[i][0]) habit_rows++;
    float top_h = 46.0f + habit_rows * 15.0f;
    DrawRectangleRounded({px, ly2, 240.0f, top_h}, 0.12f, 4, {24, 10, 10, 225});
    DrawRectangleRoundedLines({px, ly2, 240.0f, top_h}, 0.12f, 4, 1.0f,
        {130, 40, 40, 190});
    char lbuf[64];
    snprintf(lbuf, sizeof(lbuf), "它眼中的你 · %s", d.mirror_style);
    DrawTextEx(g_font_small, lbuf, {px + 8, ly2 + 3}, 12, 1, {230, 120, 110, 255});
    float ry = ly2 + 20.0f;
    for (int i = 0; i < 3; i++) {
        if (!d.mirror_habits[i][0]) continue;
        DrawTextEx(g_font_small, d.mirror_habits[i], {px + 8, ry}, 11, 1,
            {195, 140, 135, 240});
        ry += 15.0f;
    }
    // 准确率行 (数据不足显示观察进度)
    if (d.mirror_accuracy >= 0.0f)
        snprintf(lbuf, sizeof(lbuf), "预测命中 %d%% (%d 招)",
            (int)(d.mirror_accuracy * 100), d.mirror_observed);
    else
        snprintf(lbuf, sizeof(lbuf), "观察中… 已收录 %d 招", d.mirror_observed);
    DrawTextEx(g_font_small, lbuf, {px + 8, ry}, 11, 1, {220, 170, 120, 255});
    // ── 下段: 4 臂胜率 (决策后) ──
    if (d.mirror_last_action < 0) return;
    static const char* ARM_NAMES[4] = {"近战压制", "后撤拉扯", "技能反制", "连招输出"};
    float ay = ly2 + top_h + 4.0f;
    float ah = 74.0f;
    DrawRectangleRounded({px, ay, 240.0f, ah}, 0.12f, 4, {20, 8, 8, 220});
    DrawRectangleRoundedLines({px, ay, 240.0f, ah}, 0.12f, 4, 1.0f,
        {90, 30, 30, 180});
    snprintf(lbuf, sizeof(lbuf), "应对策略 · 决策: %s",
        ARM_NAMES[d.mirror_last_action]);
    DrawTextEx(g_font_small, lbuf, {px + 8, ay + 3}, 12, 1,
        {220, 150, 140, 255});
    float ary = ay + 22.0f;
    for (int i = 0; i < 4; i++) {
        bool cur = (i == d.mirror_last_action);
        Color ac = cur ? Color{220, 90, 80, 255} : Color{170, 130, 130, 255};
        DrawTextEx(g_font_small, ARM_NAMES[i], {px + 8, ary}, 11, 1, ac);
        snprintf(lbuf, sizeof(lbuf), "%d%%",
            (int)(d.mirror_arm_rates[i] * 100.0f));
        DrawTextEx(g_font_small, lbuf, {px + 66, ary}, 11, 1, ac);
        draw_progress_bar({px + 104, ary + 3, 112, 8}, d.mirror_arm_rates[i],
            cur ? Color{220, 90, 80, 255} : Color{120, 60, 60, 255},
            {50, 20, 20, 255});
        ary += 13.0f;
    }
}

void GameRenderer::draw_hud(const Player* player, int current_floor, float game_time,
                             Monster* boss, bool show_relic_panel,
                             int inventory_open, int inventory_cursor,
                             const std::string& room_msg, float room_msg_timer,
                             int screen_w, int screen_h,
                             const CharacterPanelData* echo_panel,
                             int challenge_wave, int challenge_total) {
    if (!player) return;
    auto& c = player->combat;

    // HP bar (G10.3-B3: 像素风双层边框 + 高光顶线)
    int eff_max_hp = get_effective_max_hp(player);
    float hp_r = eff_max_hp > 0 ? (float)c.current_hp / eff_max_hp : 0.0f;
    if (hp_r > 1.0f) hp_r = 1.0f;
    if (hp_r < 0.0f) hp_r = 0.0f;

    // 动态颜色：>50% 绿 / >25% 黄 / <25% 红
    Color hp_c;
    if (hp_r > 0.5f) {
        hp_c = Color{50, 200, 50, 255};
    } else if (hp_r > 0.25f) {
        hp_c = Color{200, 200, 50, 255};
    } else {
        hp_c = Color{200, 50, 50, 255};
    }
    DrawRectangleRec({10, 10, 200, 16}, {40, 20, 20, 255});
    DrawRectangleRec({10, 10, 200 * hp_r, 16}, hp_c);
    DrawRectangleRec({11, 11, 198 * hp_r, 2}, Color{255, 255, 255, 60});  // 高光
    DrawRectangleLinesEx({9, 9, 202, 18}, 1, {25, 20, 30, 255});         // 外框
    DrawRectangleLinesEx({10, 10, 200, 16}, 1, {80, 70, 90, 200});        // 内框

    if (g_font_loaded) {
        char buf[128];
        snprintf(buf, sizeof(buf), "HP:%d/%d ATK:%d PD:%d MD:%d",
            c.current_hp, eff_max_hp, c.get_effective_attack(),
            c.get_effective_defense(AttackType::PHYSICAL),
            c.get_effective_defense(AttackType::MAGICAL));
        DrawTextEx(g_font_small, buf, {215, 10}, 16, 1, {220, 220, 220, 255});
    }

    // XP bar
    float xp_r = (float)player->xp / player->xp_to_next;
    draw_progress_bar({10, 30, 200, 10}, xp_r, {80, 120, 255, 255});

    if (g_font_loaded) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Lv%d XP:%d/%d", player->level, player->xp, player->xp_to_next);
        DrawTextEx(g_font_small, buf, {12, 42}, 13, 1, {180, 200, 255, 255});
    }

    // Floor + G10.9-C5: 当前槽位轻量提示 (避免"我在玩哪个档?")
    if (g_font_loaded) {
        char buf[32];
        snprintf(buf, sizeof(buf), "第%d/%d层", current_floor, MAX_FLOORS);
        DrawTextEx(g_font_small, buf, {220, 42}, 16, 1, {200, 200, 50, 255});
        char slot_buf[16];
        snprintf(slot_buf, sizeof(slot_buf), "存档%d", SaveManager::active_slot());
        DrawTextEx(g_font_small, slot_buf, {315, 44}, 13, 1, {150, 160, 180, 200});
    }

    // Boss HP bar — F15.5.1: hide for echo boss (HP shown in mirror panel)
    if (boss && !echo_panel) {
        float bw = 500, bh = 24;
        float bx = screen_w / 2 - bw / 2, by = 4;
        Color bar_bg = {30, 5, 5, 255};
        Color bar_fg = {220, 100, 30, 255};
        auto* bai = dynamic_cast<const BossAI*>(boss->ai);
        if (bai && bai->phase2) {
            bar_fg = {255, 40, 20, 255};
            bar_bg = {50, 5, 5, 255};
        }
        float glow = 1.0f + sinf((float)GetTime() * 3) * 0.03f;
        DrawRectangleLinesEx({bx - 2, by - 2, bw + 4, bh + 4}, 1.5f,
                             Color{220, 100, 30, (unsigned char)(60 * glow)});
        draw_progress_bar({bx, by, bw, bh},
            (float)boss->combat.current_hp / boss->combat.max_hp,
            bar_fg, bar_bg);
        if (g_font_loaded) {
            char buf[80];
            const char* phase_tag = (bai && bai->phase2) ? "[狂暴] " : "";
            const char* defend_tag = (bai && bai->boss_state == BossState::DEFEND)
                ? "[护盾] " : "";
            const char* summon_tag = (bai && bai->boss_state == BossState::SUMMON)
                ? "[召唤] " : "";
            snprintf(buf, sizeof(buf), "%s%s%s%s HP:%d/%d",
                phase_tag, defend_tag, summon_tag,
                boss->name.c_str(), boss->combat.current_hp, boss->combat.max_hp);
            float tw = MeasureTextEx(g_font_small, buf, 18, 1).x;
            DrawTextEx(g_font_small, buf, {bx + (bw - tw) / 2, by + bh + 3}, 18, 1,
                       {255, 220, 100, 255});
        }
    }

    // F15.5.1: Echo mirror panel (right side)
    if (echo_panel)
        draw_character_panel(*echo_panel, (float)(screen_w - 260), 6.0f);

    draw_skill_bar(player, game_time);
    draw_player_buffs(player);
    draw_player_relics(player);
    // D3 Step4: Build 流派显示 (rellic下方)
    {
        BuildScore bs = calculate_build(player);
        BuildType bt = bs.identify();
        float y = 56.0f + player->skills.active_skills.size() * 28.0f + 4.0f;
        if (!player->active_buffs.empty())
            y += player->active_buffs.size() * 18.0f + 4.0f;
        if (!player->relics.empty())
            y += 22.0f;
        if (bt != BuildType::NONE && g_font_loaded) {
            char buf[64];
            snprintf(buf, sizeof(buf), "流派: %s", bs.build_name());
            DrawTextEx(g_font_small, buf, {12, y}, 12, 1, {255, 220, 100, 230});
            y += 16;
        }
        int atk_lv = AttackEvolutionManager::current_level(player);
        if (atk_lv >= 2 && g_font_loaded) {
            char buf[32];
            snprintf(buf, sizeof(buf), "普攻: %s", AttackEvolutionManager::current_name(player));
            DrawTextEx(g_font_small, buf, {12, y}, 12, 1, {255, 200, 60, 230});
            y += 16;
        }
        for (int si = 0; si < (int)player->skills.active_skills.size(); si++) {
            std::string ev = SkillEvolutionManager::evo_name(player, si);
            if (!ev.empty() && g_font_loaded) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%s: %s",
                         player->skills.active_skills[si]->name.c_str(), ev.c_str());
                DrawTextEx(g_font_small, buf, {12, y}, 11, 1, {180, 220, 255, 220});
                y += 14;
            }
        }
    }
    if (show_relic_panel) draw_relic_panel(player, screen_w);

    // Key hints
    if (g_font_loaded) {
        // Batch 3F: Challenge wave HUD (above gold/key)
        if (challenge_wave >= 0 && challenge_total > 0) {
            char cw[32];
            snprintf(cw, sizeof(cw), "Wave: %d/%d", challenge_wave, challenge_total);
            DrawTextEx(g_font_small, cw,
                       {14.0f, (float)screen_h - 48.0f},
                       12, 1, Color{255, 100, 100, 230});
        }
        // Batch 3A: Gold / Key HUD (bottom-left) — G10.3-B3: 像素图标替代纯文本
        if (player) {
            float icon_s = 14.0f;
            float base_x = 14.0f;
            float base_y = (float)screen_h - 27.0f;
            
            // 金币图标（黄色）
            _draw_gold_icon(base_x, base_y, icon_s);
            char gbuf[16];
            snprintf(gbuf, sizeof(gbuf), "%d", player->gold);
            DrawTextEx(g_font_small, gbuf,
                       {base_x + icon_s + 4.0f, base_y + 1.0f},
                       12, 1, Color{255, 214, 90, 230});
            
            // 计算金币文本宽度
            float gw = MeasureTextEx(g_font_small, gbuf, 12, 1).x;
            
            // 钥匙图标（金色）
            _draw_key_icon(base_x + icon_s + 12.0f + gw, base_y, icon_s);
            char kbuf[16];
            snprintf(kbuf, sizeof(kbuf), "%d", player->key_count);
            DrawTextEx(g_font_small, kbuf,
                       {base_x + icon_s * 2 + 16.0f + gw + 4.0f, base_y + 1.0f},
                       12, 1, Color{190, 160, 90, 230});
            
            // 圣物数量（圣物面板打开时）
            if (show_relic_panel && player->relics.size() > 0) {
                float relic_x = base_x + icon_s * 2 + 16.0f + gw + 4.0f + 
                               MeasureTextEx(g_font_small, kbuf, 12, 1).x + 12.0f;
                // 圣物图标（紫色）
                DrawRectangleRec(
                    {relic_x, base_y + 1.0f, icon_s, icon_s},
                    Color{180, 100, 255, 230}
                );
                char rbuf[16];
                snprintf(rbuf, sizeof(rbuf), "%d", (int)player->relics.size());
                DrawTextEx(g_font_small, rbuf,
                           {relic_x + icon_s + 4.0f, base_y + 1.0f},
                           12, 1, Color{200, 150, 255, 230});
            }
        }
        const char* hint = "[R]圣物  [B]背包  [F1]日志  [M]地图  [ESC]保存";
        float hw = MeasureTextEx(g_font_small, hint, 12, 1).x;
        DrawTextEx(g_font_small, hint,
                   {screen_w - hw - 14.0f, (float)screen_h - 26.0f},
                   12, 1, Color{140, 140, 160, 220});
    }

    draw_room_message(screen_w, screen_h, room_msg, room_msg_timer);
}
