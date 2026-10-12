#include "game/rendering/sprite_renderer.h"
#include "combat_system.h"   // visual_rng (RNG-001)
#include <algorithm>
#include <cmath>
#include <cstring>

Rectangle SpriteRenderer::frame_rect(const SpriteDef& def, int frame) {
    int col = frame % std::max(1, def.frame_count);
    int row = frame / std::max(1, def.frame_count);
    return {(float)(col * def.frame_w), (float)(row * def.frame_h),
            (float)def.frame_w, (float)def.frame_h};
}

void SpriteRenderer::draw_sprite(Texture2D tex, const SpriteDef& def,
                                 int frame, Rectangle dst, Color tint) {
    if (tex.id <= 0) return;
    Rectangle src = frame_rect(def, frame);
    DrawTexturePro(tex, src, dst, {0, 0}, 0.0f, tint);
}

// ── 程序化像素纹理辅助 (≤40 行/函数) ──
// M5-fix(RNG-001): 噪声吃独立 visual_rng — 原用 gameplay rng() 使纹理
// 生成/缓存时序变化 (如素材命中与否) 污染 gameplay 随机流 → 同 seed 分岔
static void _add_noise(Image* img, Color base) {
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            if (visual_rng() % 100 < 8) {
                int d = (int)(visual_rng() % 36) - 18;
                ImageDrawPixel(img, x, y,
                    {(unsigned char)std::clamp((int)base.r + d, 0, 255),
                     (unsigned char)std::clamp((int)base.g + d, 0, 255),
                     (unsigned char)std::clamp((int)base.b + d, 0, 255), 255});
            }
        }
}

static Color _dim(Color c, float f) {
    return {(unsigned char)(c.r * f), (unsigned char)(c.g * f),
            (unsigned char)(c.b * f), 255};
}

// 亮度提升 (墙顶高光/呼吸帧/发带用)
static Color _brighten(Color c, int amt) {
    return {(unsigned char)std::min(255, (int)c.r + amt),
            (unsigned char)std::min(255, (int)c.g + amt),
            (unsigned char)std::min(255, (int)c.b + amt), c.a};
}

static void _draw_wall_details(Image* img, Color base) {
    Color joint = _dim(base, 0.5f);
    for (int y = 0; y < 32; y += 8)
        for (int x = 0; x < 32; x++)
            ImageDrawPixel(img, x, y, joint);
    for (int x = 0; x < 32; x += 8)
        for (int y = 0; y < 32; y++)
            ImageDrawPixel(img, x, y, joint);
    Color bright = {(unsigned char)std::clamp((int)base.r + 30, 0, 255),
                    (unsigned char)std::clamp((int)base.g + 30, 0, 255),
                    (unsigned char)std::clamp((int)base.b + 30, 0, 255), 255};
    for (int x = 0; x < 32; x++)
        for (int y = 0; y < 2; y++)
            ImageDrawPixel(img, x, y, bright);
    for (int x = 0; x < 32; x++)
        for (int y = 30; y < 32; y++)
            ImageDrawPixel(img, x, y, joint);
}

// G11.1-A: 伪3D墙贴图 — 上 1/3 亮色"墙顶", 下 2/3 暗色"墙面"
// 砖缝只画在墙面段, 墙顶段横向拉丝营造石面质感
static void _draw_wall_3d(Image* img, Color base) {
    Color top_face = _brighten(base, 42);
    Color top_edge = _brighten(base, 72);
    // 上段: 墙顶 (受光面)
    ImageDrawRectangle(img, 0, 0, 32, 11, top_face);
    for (int x = 0; x < 32; x++)                       // 顶缘高光
        for (int y = 0; y < 2; y++)
            ImageDrawPixel(img, x, y, top_edge);
    // 顶-面交界阴影线
    Color joint = _dim(base, 0.42f);
    for (int x = 0; x < 32; x++)
        ImageDrawPixel(img, x, 11, joint);
    // 下段: 墙面 (背光) + 错缝砖
    ImageDrawRectangle(img, 0, 12, 32, 20, base);
    Color brick_joint = _dim(base, 0.5f);
    for (int by = 12; by < 32; by += 6) {
        for (int x = 0; x < 32; x++)
            ImageDrawPixel(img, x, by, brick_joint);
        int offset = ((by - 12) / 6) % 2 == 0 ? 4 : 20;   // 错缝
        for (int y = by; y < std::min(by + 6, 32); y++)
            ImageDrawPixel(img, offset, y, brick_joint);
    }
    // 底缘踢脚阴影
    Color foot = _dim(base, 0.36f);
    for (int x = 0; x < 32; x++)
        for (int y = 30; y < 32; y++)
            ImageDrawPixel(img, x, y, foot);
}

static void _draw_floor_details(Image* img, Color base) {
    Color joint = _dim(base, 0.6f);
    for (int x = 0; x < 32; x++) {
        ImageDrawPixel(img, x, 15, joint);
        ImageDrawPixel(img, x, 31, joint);
    }
    for (int y = 0; y < 32; y++)
        ImageDrawPixel(img, 15, y, joint);
}

// 程序化像素纹理: 基色 + 确定性噪点 + 砖缝/接缝
// G11.1-A: wall=true 使用伪3D贴图 (墙顶亮面 + 墙面错缝砖)
Texture2D SpriteRenderer::gen_pixel_tile(Color base, bool wall) {
    Image img = GenImageColor(32, 32, base);
    _add_noise(&img, base);
    if (wall) _draw_wall_3d(&img, base);
    else      _draw_floor_details(&img, base);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

// ═══════════════ M6-i: 群系风格化程序材质 (per-biome 画法分歧) ═══════════════

// 监狱: 湿石砖 — 大块石板缝 + 底部水渍 + 零星苔斑 (accent=青苔色)
static void _biome_prison(Image* img, Color base, Color accent, bool wall) {
    Color joint = _dim(base, 0.45f);
    if (wall) {
        _draw_wall_3d(img, base);                    // 伪3D 亮顶/暗面骨架
        for (int x = 0; x < 32; x++)                 // 面上不规则石板缝
            for (int y = 13; y < 31; y++)
                if ((x + y * 3) % 13 == 0)
                    ImageDrawPixel(img, x, y, joint);
    } else {
        _draw_floor_details(img, base);             // 基础接缝
        for (int i = 0; i < 5; i++)                  // 湿渍块 (半透明深斑)
            ImageDrawRectangle(img, 2 + i * 6, 20 + (i % 3) * 4, 4, 3,
                                _dim(base, 0.72f));
    }
    // 零星苔斑 (accent; 视觉确定性循环覆盖, 不吃 RNG)
    for (int i = 0; i < 4; i++) {
        int mx = (i * 9 + 3) % 28, my = (i * 7 + 6) % 26;
        ImageDrawPixel(img, mx, my, accent);
        ImageDrawPixel(img, mx + 1, my, _dim(accent, 0.8f));
    }
}

// 火山: 玄武岩 — 不规则岩块分割 + 熔岩裂纹亮线 (accent=岩浆橙)
static void _biome_volcano(Image* img, Color base, Color accent, bool wall) {
    Color dark = _dim(base, 0.5f);
    if (wall) {
        _draw_wall_3d(img, base);
        for (int i = 0; i < 6; i++) {                // 岩块碎裂暗斑
            int bx = (i * 11 + 5) % 24, by = 13 + (i * 5) % 16;
            ImageDrawRectangle(img, bx, by, 4, 3, dark);
        }
    } else {
        _draw_floor_details(img, base);
        for (int i = 0; i < 4; i++)                  // 灰烬浅斑 (提亮)
            ImageDrawRectangle(img, (i * 8 + 2) % 26, (i * 6 + 4) % 24, 3, 2,
                                _brighten(base, 26));
    }
    // 熔岩裂纹 (accent 亮线, 之字形; 墙面+地板都画)
    int cy = wall ? 20 : 16;
    for (int x = 2; x < 30; x++) {
        int y = cy + ((x / 7) % 2 == 0 ? 0 : 2);
        ImageDrawPixel(img, x, y, accent);
        if (x % 9 == 4) ImageDrawPixel(img, x, y + 1, _dim(accent, 0.7f));
    }
}

// 深渊: 晶簇 — 竖晶柱亮棱 + 发光符文刻痕 (accent=幽紫光)
static void _biome_abyss(Image* img, Color base, Color accent, bool wall) {
    Color bright = _brighten(base, 36);
    if (wall) {
        _draw_wall_3d(img, base);
        for (int i = 0; i < 3; i++) {                // 竖晶柱棱 (高光)
            int cx = 5 + i * 10;
            for (int y = 13; y < 31; y++) {
                ImageDrawPixel(img, cx, y, bright);
                if (y % 5 == 0) ImageDrawPixel(img, cx + 1, y, _dim(bright, 0.8f));
            }
        }
    } else {
        _draw_floor_details(img, base);
        for (int i = 0; i < 3; i++)                  // 地面晶片碎斑
            ImageDrawRectangle(img, (i * 12 + 4) % 26, (i * 9 + 8) % 24, 2, 3,
                                bright);
    }
    // 发光符文刻痕 (accent; 两道折线符号)
    for (int i = 0; i < 2; i++) {
        int rx = 8 + i * 12, ry = wall ? 18 : 12;
        ImageDrawPixel(img, rx, ry, accent);
        ImageDrawPixel(img, rx + 1, ry + 2, accent);
        ImageDrawPixel(img, rx, ry + 4, _dim(accent, 0.75f));
        ImageDrawPixel(img, rx - 1, ry + 2, _dim(accent, 0.75f));
    }
}

Texture2D SpriteRenderer::gen_biome_tile(Color base, Color accent, Color joint_c,
                                         BiomeStyle style, bool wall) {
    // 通用回退 = 原画法 (保证既有视觉不回归)
    if (style == BiomeStyle::GENERIC)
        return gen_pixel_tile(base, wall);
    Image img = GenImageColor(32, 32, base);
    _add_noise(&img, base);
    switch (style) {
        case BiomeStyle::PRISON:  _biome_prison(&img, base, accent, wall);  break;
        case BiomeStyle::VOLCANO: _biome_volcano(&img, base, accent, wall); break;
        case BiomeStyle::ABYSS:   _biome_abyss(&img, base, accent, wall);   break;
        default: break;
    }
    (void)joint_c;                                  // 预留: 深缝色 (当前用 _dim)
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

// ═══════════════ M6-j: 程序化地板装饰片 (确定性 decal) ═══════════════

// ── G42: 地板装饰 32x32 程序化贴图, 每 kind 一个绘制函数 ──
// kind 与 scene_builder 的 decal_roll 区间对应 (0 裂缝 / 1 苔藓 / 2 符文 /
# // default 污渍)

static void _gen_decal_crack(Image* p, Color primary, Color secondary) {
    for (int s = 0; s < 3; s++) {                  // 三段折线 (墙缝深色)
        int x0 = 6 + s * 9, y0 = 8 + s * 4;
        for (int d = 0; d < 9; d++)
            ImageDrawPixel(p, x0 + d, y0 + (d % 3) - 1, primary);
    }
    ImageDrawPixel(p, 15, 16, secondary);
    ImageDrawPixel(p, 26, 20, secondary);
}

static void _gen_decal_moss(Image* p, Color primary, Color secondary) {
    ImageDrawCircle(p, 10, 22, 3, primary);        // 3+2+1 团簇
    ImageDrawCircle(p, 13, 24, 2, primary);
    ImageDrawCircle(p, 22, 12, 2, primary);
    ImageDrawPixel(p, 15, 20, secondary);
    ImageDrawPixel(p, 24, 15, secondary);
}

static void _gen_decal_rune(Image* p, Color primary, Color secondary) {
    for (int s = 0; s < 2; s++) {                  // 折线符文 + 中心点
        int x0 = 8 + s * 12, y0 = 8;
        for (int d = 0; d < 8; d++)
            ImageDrawPixel(p, x0 + (d % 3), y0 + d, primary);
        for (int d = 0; d < 8; d++)
            ImageDrawPixel(p, x0 + 3 - (d % 3), y0 + d, primary);
    }
    ImageDrawCircle(p, 16, 17, 2, secondary);
}

static void _gen_decal_stain(Image* p, Color primary, Color secondary) {
    for (int i = 0; i < 5; i++)                    // 不规则暗斑
        ImageDrawRectangle(p, 6 + i * 5, 10 + (i % 4) * 4, 4, 3, primary);
    ImageDrawPixel(p, 20, 20, secondary);
}

Texture2D SpriteRenderer::gen_floor_decal(int kind, Color primary, Color secondary) {
    Image img = GenImageColor(32, 32, BLANK);
    Image* p = &img;
    switch (kind) {
        case 0: _gen_decal_crack(p, primary, secondary); break;
        case 1: _gen_decal_moss(p, primary, secondary);  break;
        case 2: _gen_decal_rune(p, primary, secondary);  break;
        default: _gen_decal_stain(p, primary, secondary); break;
    }
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

// ═══════════════════════════════════════════════════════════════
// M4f.2: 程序化角色占位精灵
// ═══════════════════════════════════════════════════════════════

static void _draw_body_shadow(Image* img, int ox, int oy, int w, int h) {
    for (int y = oy + h; y < oy + h + 3; y++)
        for (int x = ox; x < ox + w; x++)
            ImageDrawPixel(img, x, y, {0, 0, 0, 70});
}

void SpriteRenderer::_draw_person_body(Image* img, Color body, bool big) {
    int ox = big ? 4 : 8, wd = big ? 24 : 16;
    int oy = big ? 5 : 4, ht = big ? 26 : 18;
    // 头 (顶部) + 躯干
    ImageDrawRectangle(img, ox + wd / 4, oy, wd / 2, wd / 2, body);
    ImageDrawRectangle(img, ox, oy + wd / 2, wd, ht - wd / 2, body);
    // 顶端 accent 发带
    Color cap = _brighten(body, 40);
    for (int x = ox + wd / 4; x < ox + 3 * wd / 4; x++)
        for (int y = oy; y < oy + 2; y++)
            ImageDrawPixel(img, x, y, cap);
    _draw_body_shadow(img, ox, oy, wd, ht);
}

void SpriteRenderer::_draw_circle_body(Image* img, Color body) {
    ImageDrawCircle(img, 16, 15, 13, body);
    _draw_body_shadow(img, 3, 2, 26, 26);
}

// M4f.3: 差异化体型 — 箭形(Charger) / 方甲(Tank) / 炸弹(Bomber) / 尖帽(法师)
static void _draw_arrow_body(Image* img, Color body) {
    for (int i = 0; i < 26; i++) {          // 三角: 顶在下 (冲刺指向)
        int y = 4 + i;
        int half = i < 24 ? 4 + i / 4 : 13;
        for (int x = 16 - half; x <= 16 + half; x++)
            ImageDrawPixel(img, x, y, body);
    }
    ImageDrawRectangle(img, 13, 6, 6, 4, {255, 60, 40, 255});   // 冲刺亮条
}

static void _draw_block_body(Image* img, Color body) {
    ImageDrawRectangle(img, 4, 6, 24, 22, body);                // 方甲
    ImageDrawRectangle(img, 6, 8, 20, 18, {50, 50, 70, 255});   // 内甲暗
    ImageDrawRectangle(img, 10, 2, 12, 4, body);                // 头盔
    for (int y = 10; y < 24; y += 6)                            // 甲缝
        for (int x = 6; x < 26; x++)
            ImageDrawPixel(img, x, y, {20, 20, 30, 255});
}

static void _draw_bomber_body(Image* img, Color body) {
    ImageDrawCircle(img, 16, 17, 12, body);                     // 圆身
    ImageDrawRectangle(img, 15, 2, 2, 8, {200, 120, 40, 255});  // 引信
    ImageDrawRectangle(img, 14, 0, 4, 2, {255, 220, 80, 255});  // 火花
}

static void _draw_hat_body(Image* img, Color body) {
    ImageDrawRectangle(img, 8, 12, 16, 14, body);               // 袍身
    for (int i = 0; i < 12; i++) {                              // 锥帽
        int y = i;
        int half = 10 - i / 2;
        for (int x = 16 - half; x <= 16 + half; x++)
            ImageDrawPixel(img, x, y, {150, 120, 200, 255});
    }
    ImageDrawCircle(img, 16, 16, 5, {240, 220, 255, 255});     // 水晶
}

void SpriteRenderer::_draw_shapes(Image* img, Color body, int variant) {
    switch (variant) {
        case 3: _draw_arrow_body(img, body); break;
        case 4: _draw_block_body(img, body); break;
        case 5: _draw_bomber_body(img, body); break;
        case 6: _draw_hat_body(img, body); break;
        default: break;
    }
}

void SpriteRenderer::_fill_body(Image* img, Color body, int variant) {
    if (variant == 1)      _draw_circle_body(img, body);
    else if (variant == 2) _draw_person_body(img, body, true);
    else if (variant >= 3) _draw_shapes(img, body, variant);
    else                   _draw_person_body(img, body, false);
}

void SpriteRenderer::_draw_eyes(Image* img, Color accent, int variant,
                                int eye_dir) {
    (void)accent;
    if (variant == 1) {   // 圆形: 双眼
        ImageDrawCircle(img, 11, 14, 4, WHITE);
        ImageDrawCircle(img, 21, 14, 4, WHITE);
        ImageDrawCircle(img, 12, 15, 2, {20, 20, 20, 255});
        ImageDrawCircle(img, 22, 15, 2, {20, 20, 20, 255});
        return;
    }
    // 人形: 单眼 + 朝向偏移
    int ex = 16, ey = 16;
    switch (eye_dir) {
        case 1: ey = 13; break;   // up
        case 2: ex = 13; break;   // left
        case 3: ex = 19; break;   // right
        default: ey = 19; break;  // down
    }
    ImageDrawRectangle(img, ex - 2, ey - 2, 4, 4, WHITE);
    ImageDrawRectangle(img, ex - 1, ey - 1, 2, 2, {20, 20, 20, 255});
}

// RGBA8 帧拷贝到 spritesheet (raylib 5.0 无 ImageDrawImage)
static void _blit_frame(Image* sheet, const Image& fr, int row) {
    for (int y = 0; y < fr.height; y++) {
        int dst = ((row * fr.height + y) * sheet->width + 0) * 4;
        int src = (y * fr.width + 0) * 4;
        memcpy((unsigned char*)sheet->data + dst,
               (unsigned char*)fr.data + src, (size_t)fr.width * 4);
    }
}

// M4f.3: 2 帧 spritesheet (32×64: 待机/呼吸) — 与 frame_rect 管线直通
Texture2D SpriteRenderer::gen_pixel_sprite(Color body, Color accent,
                                           int variant, int eye_dir) {
    Image sheet = GenImageColor(32, 64, {0, 0, 0, 0});
    for (int f = 0; f < 2; f++) {
        Image fr = GenImageColor(32, 32, {0, 0, 0, 0});
        Color c = f == 0 ? body : _brighten(body, 18);
        _fill_body(&fr, c, variant);
        _add_noise(&fr, c);
        _draw_eyes(&fr, accent, variant, eye_dir);
        _blit_frame(&sheet, fr, f);
        UnloadImage(fr);
    }
    Texture2D tex = LoadTextureFromImage(sheet);
    UnloadImage(sheet);
    return tex;
}

// ═══════════════════════════════════════════════════════════════
// M4f.2: 程序化 VFX 爆点
// ═══════════════════════════════════════════════════════════════

void SpriteRenderer::_draw_blast_lines(Image* img, Color c) {
    for (int i = 0; i < 8; i++) {
        float a = i * 45.0f * DEG2RAD;
        ImageDrawLine(img, 16, 16,
                      (int)(16 + cosf(a) * 12), (int)(16 + sinf(a) * 12), c);
    }
}

Texture2D SpriteRenderer::gen_pixel_blast(Color c) {
    Image img = GenImageColor(32, 32, {0, 0, 0, 0});
    _draw_blast_lines(&img, c);
    _add_noise(&img, c);
    ImageDrawRectangle(&img, 13, 13, 6, 6, WHITE);
    ImageDrawRectangle(&img, 14, 14, 4, 4, {255, 230, 160, 255});
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

// ── G42: 竞技场道具 32x32 程序化贴图, 每 type 一个绘制函数 ──
// type 序号来自 ArenaObjectType 枚举 (桶/图腾/毒池/岩石/尖刺)

static void _gen_arena_barrel(Image& img) {
    Color wood = {160, 100, 40, 255}, band = {40, 30, 20, 255};
    ImageDrawRectangle(&img, 8, 6, 16, 20, wood);
    ImageDrawRectangle(&img, 6, 8, 20, 3, band);
    ImageDrawRectangle(&img, 6, 20, 20, 3, band);
    ImageDrawRectangle(&img, 12, 4, 8, 4, {60, 40, 20, 255});
}

static void _gen_arena_totem(Image& img) {
    Color pole = {30, 120, 50, 255}, glow = {100, 255, 120, 200};
    ImageDrawRectangle(&img, 12, 8, 8, 18, pole);
    ImageDrawCircle(&img, 16, 10, 6, glow);
    ImageDrawCircle(&img, 16, 10, 4, {200, 255, 200, 255});
}

static void _gen_arena_pool(Image& img) {
    Color pool = {40, 150, 55, 180};
    ImageDrawRectangle(&img, 4, 10, 24, 12, pool);
    ImageDrawRectangle(&img, 8, 12, 16, 8, {60, 180, 70, 200});
}

static void _gen_arena_rock(Image& img) {
    Color rock = {100, 95, 100, 255};
    ImageDrawRectangle(&img, 6, 12, 20, 12, rock);
    ImageDrawRectangle(&img, 8, 10, 6, 4, {120, 115, 120, 255});
    ImageDrawRectangle(&img, 18, 8, 8, 6, {110, 105, 110, 255});
}

static void _gen_arena_spike(Image& img) {
    Color spike = {180, 50, 50, 255};
    for (int y = 4; y < 28; y++) {
        int w = (y - 4) * 2 / 3;
        ImageDrawLine(&img, 16 - w, y, 16 + w, y, spike);
    }
}

Texture2D SpriteRenderer::gen_arena_prop(int type, Color base) {
    Image img = GenImageColor(32, 32, {0, 0, 0, 0});
    switch (type) {
    case 0: _gen_arena_barrel(img); break;
    case 1: _gen_arena_totem(img);  break;
    case 2: _gen_arena_pool(img);   break;
    case 3: _gen_arena_rock(img);   break;
    case 4: _gen_arena_spike(img);  break;
    }
    _add_noise(&img, base);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}
