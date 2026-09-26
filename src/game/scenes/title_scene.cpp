#include "title_scene.h"
#include "floor_select_scene.h"
#include "tutorial_scene.h"
#include "game_scene.h"
#include "slot_select_scene.h"   // G10.9-C: 三档选择
#include "scene_tree.h"
#include "audio/audio_server.h"
#include "config.h"
#include "save/save_manager.h"
#include "core/logger.h"
#include "resources/resource_manager.h"     // G10.7-B2: 舞台层素材
#include "game/rendering/sprite_renderer.h"  // G10.7-B2: draw_sprite
#include <cmath>

extern Font g_font, g_font_small;
extern bool g_font_loaded;

// Q4.5: 键盘/鼠标共用动作分发 — 单一职责
bool TitleScene::_activate(const std::string& action) {
    auto* tree = get_tree();
    if (!tree) return false;
    tree->get_audio()->play_sfx("ui_confirm", 0.5f);

    if (action == "new") {
        // G10.9-C3: 新游戏 → 统一选档界面 (空档直选 / 满档删除流由场景内处理)
        auto sss = std::make_shared<SlotSelectScene>();
        sss->name = "SlotSelectScene";
        sss->mode = SlotSelectScene::Mode::NEW_GAME;
        tree->change_scene(sss);
        LOG_INFO("新游戏 → 选档");
        return true;
    }
    if (action == "continue" && has_save) {
        // G10.9-C2: 继续游戏 → 选已有档
        auto sss = std::make_shared<SlotSelectScene>();
        sss->name = "SlotSelectScene";
        sss->mode = SlotSelectScene::Mode::CONTINUE_GAME;
        tree->change_scene(sss);
        LOG_INFO("继续游戏 → 选档");
        return true;
    }
    if (action == "select") {
        // G10.9-C4: 选关 → 先选档 (读该档 maxf, 只解锁该档范围)
        auto sss = std::make_shared<SlotSelectScene>();
        sss->name = "SlotSelectScene";
        sss->mode = SlotSelectScene::Mode::SELECT_FLOOR;
        tree->change_scene(sss);
        LOG_INFO("选关 → 选档");
        return true;
    }
    if (action == "tutorial") {
        auto ts = std::make_shared<TutorialScene>();
        ts->name = "TutorialScene";
        tree->change_scene(ts);
        LOG_INFO("进入教程");
        return true;
    }
    if (action == "fullscreen") {
        // G10.7-B4: 修复鼠标点击"全屏切换"无反应 — _activate 缺此分支,
        // 只有全局 G 键响应 (审计发现的 bug #2)
        ToggleFullscreen();
        return true;
    }
    if (action == "quit") {
        tree->quit();
        return true;
    }
    return false;
}

void TitleScene::_enter_tree() {
    // 每次进入标题画面时重新检测存档状态
    // G10.9-B2: 任一槽有档即算 (选关用各槽自身 maxf; 此处 max_floor = 全槽最大)
    has_save = false;
    max_floor = 1;
    for (auto& s : SaveManager::get_all_slots()) {
        if (s.exists) {
            has_save = true;
            if (s.max_floor > max_floor) max_floor = s.max_floor;
        }
    }
    LOG_INFO("标题画面: has_save=%d max_floor=%d", has_save, max_floor);
}

void TitleScene::_ready() {
    name = "TitleScene";
    items = {
        {"N", "新游戏", "new", {255, 200, 50, 255}},
        {"C", "继续游戏", "continue", {100, 200, 100, 255}},
        {"F", "选关", "select", {100, 180, 255, 255}},
        {"T", "新手教程", "tutorial", {200, 180, 120, 255}},
        {"G", "全屏切换", "fullscreen", {160, 160, 200, 255}},
        {"Esc", "退出", "quit", {200, 100, 100, 255}},
    };
}

void TitleScene::_process(double delta) {
    anim_time += (float)delta;
}

// ══════════════════════════════════════════════════════════════
// G10.7-B2: 电影海报舞台层 — 先建舞台, 角色层 B3 再上
// 纵深渐变 → 透视地板 → 两侧石墙 → 尽头拱门 → 火光余烬 → 径向 vignette
// 全部使用游戏内素材与色板 (wall/floor/door 纹理 + kenney 色系)
// ══════════════════════════════════════════════════════════════
void TitleScene::_draw_stage() {
    auto* tree = get_tree();
    if (!tree) return;
    const int sw = tree->get_width(), sh = tree->get_height();

    // ── 懒加载舞台素材 (一次) ──
    if (!_stage_tex.loaded) {
        auto& rm = ResourceManager::inst();
        SpriteDef d;
        _stage_tex.wall  = rm.sprite_by_key("wall", d);
        _stage_tex.floor = rm.sprite_by_key("floor", d);
        _stage_tex.door  = rm.sprite_by_key("door", d);
        _stage_tex.loaded = true;
    }

    // ── 层1: 纵深渐变 — 顶黑 → 中部地牢暖暗 (蓝紫 15,12,24 → 34,26,48) ──
    const int GRAD_STEPS = 12;
    for (int i = 0; i < GRAD_STEPS; i++) {
        float t = (float)i / (GRAD_STEPS - 1);          // 0=顶 1=底
        float k = 0.35f + 0.65f * sinf(t * 3.14159f);   // 中部最亮 (bell)
        Color c = {
            (unsigned char)(14 + 24 * k),
            (unsigned char)(10 + 18 * k),
            (unsigned char)(22 + 32 * k), 255 };
        DrawRectangle(0, (int)(sh * t) - sh / GRAD_STEPS / 2,
                      sw, sh / GRAD_STEPS + 2, c);
    }

    // ── 层2: 透视地板 — 下半屏 floor 平铺, 行高逐行压缩 (近大远小) ──
    if (_stage_tex.floor.id > 0) {
        SpriteDef fd; fd.frame_w = 32; fd.frame_h = 32;
        float floor_top = sh * 0.52f;                    // 地平线
        int rows = 9;
        float y = floor_top;
        for (int r = 0; r < rows; r++) {
            float depth = (float)r / (rows - 1);          // 0=远 1=近
            float row_h = 22 + 46 * depth * depth;        // 行高: 远 22px → 近 68px
            float dim = 0.30f + 0.70f * depth;             // 远暗近亮
            int cols = (int)(sw / 44) + 2;
            for (int c = 0; c < cols; c++) {
                float x = c * 44.0f - (r % 2) * 22.0f;    // 交错砖缝
                SpriteRenderer::draw_sprite(_stage_tex.floor, fd, 0,
                    {x, y, 46.0f, row_h},
                    Color{255, 255, 255, (unsigned char)(dim * 235)});
            }
            y += row_h;
            if (y > sh) break;
        }
    }

    // ── 层3: 两侧石墙 — 竖排贴墙, 越靠外越暗 (对峙走廊感) ──
    if (_stage_tex.wall.id > 0) {
        SpriteDef wd; wd.frame_w = 32; wd.frame_h = 32;
        const int wall_cols = 3;                          // 每侧 3 列渐隐
        for (int side = 0; side < 2; side++)
            for (int col = 0; col < wall_cols; col++) {
                float dim = 0.85f - col * 0.28f;         // 0.85 → 0.29
                float col_w = 54 + col * 10;
                float x = (side == 0) ? col * col_w : sw - (col + 1) * col_w;
                for (int ry = 0; ry < sh; ry += 44)
                    SpriteRenderer::draw_sprite(_stage_tex.wall, wd, 0,
                        {x, (float)ry, col_w, 46.0f},
                        Color{255, 255, 255, (unsigned char)(dim * 240)});
            }
    }

    // ── 层4: 尽头拱门 — 中央地平线上, 门内深黑 + 微红光 ──
    if (_stage_tex.door.id > 0) {
        SpriteDef dd; dd.frame_w = 32; dd.frame_h = 32;
        float door_w = 96, door_h = 120;
        float dx = sw / 2.0f - door_w / 2, dy = sh * 0.52f - door_h;
        // 门洞深黑
        DrawRectangle((int)dx + 8, (int)dy + 10, (int)door_w - 16, (int)door_h - 10,
                      Color{8, 6, 12, 255});
        // 门框
        SpriteRenderer::draw_sprite(_stage_tex.door, dd, 0, {dx, dy, door_w, door_h},
                                   Color{210, 210, 210, 255});
        // 门内微红光 (深处岩浆/危险暗示)
        float glow = 0.5f + 0.5f * sinf(anim_time * 1.2f);
        DrawRectangle((int)dx + 14, (int)dy + 18, (int)door_w - 28, (int)door_h - 26,
                      Color{60, 16, 14, (unsigned char)(60 + 30 * glow)});
    }

    // ── 层5: 火光余烬 — 拱门两侧光晕 + 上升暖色微粒 ──
    {
        float glow = 0.6f + 0.4f * sinf(anim_time * 2.3f);
        float door_w = 96;
        float tx[2] = {sw / 2.0f - door_w / 2 - 26, sw / 2.0f + door_w / 2 + 26};
        for (int t = 0; t < 2; t++) {
            Vector2 c = {tx[t], sh * 0.52f - 34};
            for (int ring = 3; ring >= 1; ring--) {
                float rad = 10 + ring * 14;
                DrawCircleV(c, rad,
                    Color{255, 120, 40, (unsigned char)(26 * glow / ring)});
            }
            // 火芯
            DrawCircleV(c, 4.5f, Color{255, 200, 90, (unsigned char)(150 + 60 * glow)});
        }
        // 上升余烬粒子 (12 颗, 确定性伪随机 — 与原粒子同手法)
        for (int i = 0; i < 12; i++) {
            float px = fmodf((float)(i * 97 + 13) * 1.7f + anim_time * 6.0f, (float)sw);
            float py = sh * 0.55f - fmodf(anim_time * 26.0f + i * 53.0f, sh * 0.4f);
            DrawCircle(px, py, 1.2f + (i % 3) * 0.5f,
                       Color{255, 150, 60, (unsigned char)(70 + 40 * glow)});
        }
    }

    // ── 层6: 径向 vignette — 四边向中心暗化, 聚焦中央 ──
    {
        const int V_STEPS = 7;
        // 上/下边
        for (int i = 0; i < V_STEPS; i++) {
            float k = 1.0f - (float)i / V_STEPS;          // 越外越暗
            unsigned char a = (unsigned char)(120 * k * k);
            int band = sh / 14;
            DrawRectangle(0, i * band, sw, band + 1, Color{6, 5, 10, a});
            DrawRectangle(0, sh - (i + 1) * band, sw, band + 1, Color{6, 5, 10, a});
        }
        // 左/右边
        for (int i = 0; i < V_STEPS; i++) {
            float k = 1.0f - (float)i / V_STEPS;
            unsigned char a = (unsigned char)(120 * k * k);
            int band = sw / 12;
            DrawRectangle(i * band, 0, band + 1, sh, Color{6, 5, 10, a});
            DrawRectangle(sw - (i + 1) * band, 0, band + 1, sh, Color{6, 5, 10, a});
        }
    }
}

// ══════════════════════════════════════════════════════════════
// G10.7-B3: 对峙角色层 — 左玩家阵营 / 右敌方阵营, 近大远小
// 中央留白给标题与菜单 (可读性优先); 全部现有 16×16 素材放大
// ══════════════════════════════════════════════════════════════
void TitleScene::_draw_characters() {
    auto* tree = get_tree();
    if (!tree) return;
    const int sw = tree->get_width(), sh = tree->get_height();

    if (!_char_tex.loaded) {
        auto& rm = ResourceManager::inst();
        SpriteDef d;
        _char_tex.p_fire  = rm.sprite_by_key("player_fire", d);
        _char_tex.p_ice   = rm.sprite_by_key("player_ice", d);
        _char_tex.p_poison = rm.sprite_by_key("player_poison", d);
        _char_tex.blacksmith = rm.sprite_by_key("npc_blacksmith", d);
        _char_tex.boss_f10 = rm.sprite_by_key("boss_f10", d);
        _char_tex.boss_f5  = rm.sprite_by_key("boss_f5", d);
        _char_tex.shaman  = rm.sprite_by_key("mon_shaman", d);
        _char_tex.skeleton = rm.sprite_by_key("mon_skeleton", d);
        _char_tex.orc     = rm.sprite_by_key("mon_orc", d);
        _char_tex.slime   = rm.sprite_by_key("mon_slime", d);
        _char_tex.tank    = rm.sprite_by_key("mon_tank", d);
        _char_tex.summoner = rm.sprite_by_key("mon_summoner", d);
        _char_tex.bomber  = rm.sprite_by_key("mon_bomber", d);
        _char_tex.charger = rm.sprite_by_key("mon_charger", d);
        _char_tex.w_sword = rm.sprite_by_key("weapon_sword", d);
        _char_tex.w_spear = rm.sprite_by_key("weapon_spear", d);
        _char_tex.w_crossbow = rm.sprite_by_key("weapon_crossbow", d);
        _char_tex.w_dagger = rm.sprite_by_key("weapon_dagger", d);
        _char_tex.armor   = rm.sprite_by_key("item_armor_iron", d);
        _char_tex.potion_red = rm.sprite_by_key("item_potion_red", d);
        _char_tex.potion_blue = rm.sprite_by_key("item_potion_blue", d);
        _char_tex.charm   = rm.sprite_by_key("item_charm", d);
        _char_tex.loaded = true;
    }

    // 单角色绘制: 阴影 + 精灵 (16×16 -> size px) + 轻微呼吸浮动
    auto draw_char = [&](Texture2D tex, float cx, float base_y, float size,
                         float dim, float bob_phase) {
        if (tex.id <= 0) return;
        float bob = sinf(anim_time * 1.4f + bob_phase) * size * 0.012f;
        float x = cx - size / 2, y = base_y - size + bob;
        // 脚下阴影
        DrawEllipse(cx, base_y + 4, size * 0.32f, size * 0.09f,
                    Color{0, 0, 0, (unsigned char)(70 * dim)});
        SpriteDef sd; sd.frame_w = 16; sd.frame_h = 16;
        unsigned char a = (unsigned char)(255 * dim);
        SpriteRenderer::draw_sprite(tex, sd, 0, {x, y, size, size},
                                   Color{255, 255, 255, a});
    };

    // ── 菜单安全区: 中央 340×450 (x 310-650, y 60-510) 保持净空 ──
    // 布局原则: 全画布散布 (海报式), 近大远小, 允许少量边缘覆盖

    // 左侧纵深队列 (老师反馈修: 拉开间距消除互相遮盖, 纵深阶梯)
    float floor_y = sh * 0.78f;
    draw_char(_char_tex.p_fire, 70, floor_y + 30, 128, 1.0f, 0.0f);      // 前景领队 (6-134)
    draw_char(_char_tex.p_poison, 190, floor_y - 14, 72, 0.72f, 0.7f);   // 中景 (154-226) 与 fire 净距 20
    draw_char(_char_tex.p_ice, 122, floor_y - 118, 56, 0.50f, 1.3f);     // 中远更高 (94-150) 与 poison 上错位
    draw_char(_char_tex.orc, 248, floor_y - 96, 52, 0.38f, 1.8f);        // 远景 (222-274) 独立位
    draw_char(_char_tex.blacksmith, 55, floor_y - 168, 40, 0.30f, 2.4f); // 左上远处独立 (35-75)

    // 右侧纵深队列 (镜像布局, 同样间距)
    draw_char(_char_tex.boss_f10, 890, floor_y + 30, 128, 1.0f, 0.0f);   // 前景领队 (826-954)
    draw_char(_char_tex.shaman, 770, floor_y - 14, 72, 0.72f, 2.6f);     // (734-806)
    draw_char(_char_tex.skeleton, 838, floor_y - 118, 56, 0.50f, 1.9f);  // (810-866)
    draw_char(_char_tex.tank, 712, floor_y - 96, 52, 0.38f, 2.2f);        // (686-738)
    draw_char(_char_tex.slime, 905, floor_y - 168, 40, 0.30f, 3.1f);      // 右上远处 (885-925)

    // 顶部两角: 中怪剪影带 (半透明, 不抢主)
    // 老师反馈修: 移至标题带之下的无字区 (y≈218-252), 更靠外避标题
    draw_char(_char_tex.summoner, 160, 252, 52, 0.30f, 3.6f);
    draw_char(_char_tex.charger, 800, 252, 52, 0.30f, 4.1f);
    draw_char(_char_tex.bomber, 122, 218, 40, 0.25f, 4.6f);
    draw_char(_char_tex.orc, 838, 218, 38, 0.25f, 5.0f);   // 右上角补一只暗兽人

    // 左上晕角: 暗影骑士 (boss_f5) 大剪影 — 第三个 Boss 也是素材
    if (_char_tex.boss_f5.id > 0) {
        SpriteDef sd; sd.frame_w = 16; sd.frame_h = 16;
        SpriteRenderer::draw_sprite(_char_tex.boss_f5, sd, 0,
            {-30.0f, -34.0f, 110.0f, 110.0f},
            Color{190, 190, 210, 60});   // 深蓝灰剪影, 出血裁切
    }

    // 右前景 Boss 眼部红光脉冲 (精灵已由 draw_char 绘制, 此处只补光效)
    if (_char_tex.boss_f10.id > 0) {
        float bob = sinf(anim_time * 1.1f) * 1.8f;
        float size = 128;
        float bx = 872 - size / 2, by = floor_y + 26 - size + bob;
        float glow = 0.4f + 0.6f * (0.5f + 0.5f * sinf(anim_time * 3.2f));
        DrawCircle(bx + size * 0.38f, by + size * 0.34f, 3.5f + glow * 2,
                   Color{255, 60, 40, (unsigned char)(140 * glow)});
        DrawCircle(bx + size * 0.62f, by + size * 0.34f, 3.5f + glow * 2,
                   Color{255, 60, 40, (unsigned char)(140 * glow)});
    }

    // ── 底部战利品带 (y ≈ 594+, 菜单面板下方, 不挡任何交互) ──
    {
        SpriteDef sd; sd.frame_w = 16; sd.frame_h = 16;
        auto draw_item = [&](Texture2D tex, float cx, float cy, float size) {
            if (tex.id <= 0) return;
            SpriteRenderer::draw_sprite(tex, sd, 0,
                {cx - size / 2, cy - size / 2, size, size},
                Color{255, 255, 255, 200});
        };
        float band_y = sh - 46.0f;
        // 左半: 武器列 (剑/矛/弩/匕首)
        draw_item(_char_tex.w_sword,    180, band_y, 56);
        draw_item(_char_tex.w_spear,    252, band_y - 6, 48);
        draw_item(_char_tex.w_crossbow, 322, band_y, 52);
        draw_item(_char_tex.w_dagger,   388, band_y + 2, 44);
        // 右半: 装备列 (护符/甲/双药水)
        draw_item(_char_tex.charm,       572, band_y + 2, 44);
        draw_item(_char_tex.armor,       640, band_y, 46);
        draw_item(_char_tex.potion_red,  706, band_y - 2, 42);
        draw_item(_char_tex.potion_blue, 768, band_y + 2, 42);
    }
}

void TitleScene::_render() {
    ClearBackground(BLACK);
    auto* tree = get_tree();
    if (!tree) return;
    int sw = tree->get_width(), sh = tree->get_height();

    // G10.7-B2: 电影海报舞台层 (渐变/透视地板/石墙/拱门/火光/vignette)
    _draw_stage();
    // G10.7-B3: 左右对峙角色层 (近大远小, 中央留白给标题/菜单)
    _draw_characters();

    // 背景粒子 (舞台之上的飘浮尘埃)
    for (int i = 0; i < 20; i++) {
        float x = fmodf((float)(i * 127 + 31) * 1.3f + anim_time * 20 * (i % 3 + 1), (float)sw);
        float y = fmodf((float)(i * 53 + 17), (float)sh);
        DrawCircle(x, y, 1.5f + (i % 3), {60, 60, 100, 100});
    }

    // 标题 (G10.7-C: 定名「回响深渊」— 老师反馈: 去掉品类词大字 + 提高字号)
    if (g_font_loaded) {
        float pulse = 1.0f + sinf(anim_time * 2) * 0.02f;
        float title_size = 62 * pulse;
        float w = MeasureTextEx(g_font, "回响深渊", title_size, 2).x;
        float tx = sw/2.0f - w/2, ty = 96;
        DrawTextEx(g_font, "回响深渊", {tx + 3, ty + 3}, title_size, 2, {0, 0, 0, 160});
        DrawTextEx(g_font, "回响深渊", {tx, ty}, title_size, 2, {255, 214, 90, 255});
        w = MeasureTextEx(g_font_small, "ABYSSAL ECHO · 地牢肉鸽", 20, 1).x;
        DrawTextEx(g_font_small, "ABYSSAL ECHO · 地牢肉鸽",
                   {sw/2.0f - w/2, 168}, 20, 1, {200, 200, 220, 240});
    } else {
        DrawRectangle(sw/2 - 180, 90, 360, 60, {40, 40, 60, 255});
        DrawRectangleLines(sw/2 - 180, 90, 360, 60, {100, 100, 180, 255});
        DrawText("Abyssal Echo", sw/2 - 80, 105, 28, {255, 214, 90, 255});
    }

    // 面板 (老师反馈: 菜单字号 16→20, 面板加宽加高, 下移让出标题区)
    float pw = 360, ph = 320;
    Rectangle pr = {sw/2.0f - pw/2, 205, pw, ph};
    DrawRectangleRounded(pr, 0.08f, 8, {20, 20, 40, 230});
    DrawRectangleRoundedLines(pr, 0.08f, 8, 2, {100, 100, 180, 255});

    if (g_font_loaded)
        DrawTextEx(g_font_small, "选 单", {pr.x + 14, pr.y + 10}, 24, 1, {200, 200, 255, 255});
    else
        DrawText("Menu", (int)pr.x + 14, (int)pr.y + 12, 24, {200, 200, 255, 255});

    // 存档状态
    float y = pr.y + 44;
    if (g_font_loaded) {
        char buf[64];
        snprintf(buf, sizeof(buf), has_save ? "存档已存在（已解锁第%d层）" : "暂无存档", max_floor);
        float w = MeasureTextEx(g_font_small, buf, 16, 1).x;
        DrawTextEx(g_font_small, buf, {(float)(pr.x + (pw - w)/2), y}, 16, 1, {160, 160, 160, 255});
    } else {
        DrawText(has_save ? "Save exists" : "No save", (int)pr.x + 60, (int)y, 16, {160, 160, 160, 255});
    }
    y += 34;

    // 菜单项 (Q4.5: 鼠标悬停高亮 + hover 音效)
    // G10.9: 窗口可缩放后物理鼠标 ≠ 960×640 逻辑坐标, 经 blit 矩阵逆映射
    Vector2 mouse = get_tree()->get_mouse_logical();
    int new_hover = -1;
    for (int i = 0; i < (int)items.size(); i++) {
        auto& mi = items[i];
        Color c = mi.color;
        if (mi.action == "continue" && !has_save) c = {80, 80, 80, 255};
        if (mi.action == "select" && !has_save) c = {80, 80, 80, 255};

        Rectangle item_rect = {(float)(pr.x + 40), y, pw - 80, 38};
        if (CheckCollisionPointRec(mouse, item_rect)
            && !(mi.action == "continue" && !has_save)
            && !(mi.action == "select" && !has_save)) {
            new_hover = i;
            DrawRectangleRounded(item_rect, 0.2f, 6, {60, 60, 110, 140});
            DrawRectangleRoundedLines(item_rect, 0.2f, 6, 1, {160, 160, 220, 200});
            c = {255, 255, 255, 255};
        }

        if (g_font_loaded) {
            std::string txt = "[" + mi.key + "] " + mi.label;
            DrawTextEx(g_font_small, txt.c_str(), {(float)(pr.x + 66), y + 8}, 20, 1, c);
        } else {
            std::string txt = "[" + mi.key + "] " + mi.action;
            DrawText(txt.c_str(), (int)pr.x + 66, (int)y + 8, 20, c);
        }
        y += 42;
    }
    if (new_hover != hover_index) {
        if (new_hover >= 0 && get_tree())
            get_tree()->get_audio()->play_sfx("ui_click", 0.4f);
        hover_index = new_hover;
    }

    // B12.5: 操作说明 (右侧, 半透明方块)
    if (g_font_loaded) {
        const char* lines[] = {
            "操作说明",
            "WASD/方向键 - 移动",
            "空格 - 攻击   1~4 - 技能",
            "E - 交互   B - 背包   F1 - 日志",
            "Shift - 翻滚",
            "R - 圣物   M - 小地图   G - 全屏",
            "ESC - 保存并返回",
        };
        float guide_x = sw - 260.0f;
        float guide_y = 70.0f;    // B3: 7 行面板上移+19px 行距, 末行底 213 < 怪物队列遮挡线 ~218
        float guide_w = 240.0f;
        float guide_h = 155.0f;   // B3: 7 行 (原 6 行 165)
        DrawRectangleRounded({guide_x, guide_y, guide_w, guide_h}, 0.06f, 6, Color{15, 15, 30, 200});
        DrawRectangleRoundedLines({guide_x, guide_y, guide_w, guide_h}, 0.06f, 6, 1, Color{70, 70, 100, 180});
        for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++) {
            Color lc = (i == 0) ? Color{255, 210, 80, 255} : Color{190, 190, 210, 255};
            DrawTextEx(g_font_small, lines[i], {guide_x + 14, guide_y + 8 + i * 19.0f}, 15, 1, lc);
        }
    }

    // 底部版权 (老师反馈: 字号 12/14→14/16)
    if (g_font_loaded) {
        float w = MeasureTextEx(g_font_small, "重庆大学大数据与软件学院 · 程序设计实训", 14, 1).x;
        DrawTextEx(g_font_small, "重庆大学大数据与软件学院 · 程序设计实训",
                   {sw/2.0f - w/2, (float)(sh - 52)}, 14, 1, {100, 100, 100, 255});
        w = MeasureTextEx(g_font_small, "回响深渊 Abyssal Echo · 开发者：ruozhiDIO", 16, 1).x;
        DrawTextEx(g_font_small, "回响深渊 Abyssal Echo · 开发者：ruozhiDIO",
                   {sw/2.0f - w/2, (float)(sh - 30)}, 16, 1, {150, 150, 170, 255});
    } else {
        DrawText("Abyssal Echo | ruozhiDIO", sw/2 - 100, sh - 30, 16, {140, 140, 160, 255});
    }
}

void TitleScene::_input(const InputMap& input) {
    auto* tree = get_tree();
    if (!tree) return;

    if (input.is_action_just_pressed("cancel")) {
        tree->quit();  // 标题画面Esc = 退出游戏
        return;
    }
    // Q4.5: 键盘 → 动作分发 (鼠标点击复用同一路径)
    if (IsKeyPressed(KEY_N)) { _activate("new"); return; }
    if (IsKeyPressed(KEY_C)) { if (has_save) _activate("continue"); return; }
    if (IsKeyPressed(KEY_F)) { _activate("select"); return; }
    if (IsKeyPressed(KEY_T)) { _activate("tutorial"); return; }

    // Q4.5: 鼠标点击菜单项
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover_index >= 0
        && hover_index < (int)items.size()) {
        _activate(items[hover_index].action);
    }
}
