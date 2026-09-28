#include "tutorial_scene.h"
#include "title_scene.h"
#include "scene_tree.h"
#include "combat_system.h"
#include "config.h"
#include "audio_server.h"
#include "rendering/sprite_renderer.h"
#include "rendering/effect_drawer.h"      // G10.11: 通用特效原语 (ring/spark)
#include "rendering3d/hd2d_renderer.h"    // G12-4: 3D 表现层
#include "rendering3d/hd2d_scene_view.h"  // G12-4: 只读场景视图
#include "systems/vfx_server.h"           // G10.11: 拾取 VFX
#include "resources/resource_manager.h"
#include "core/logger.h"
#include <algorithm>
#include <cmath>

extern Font g_font, g_font_small;
extern bool g_font_loaded;
extern bool g_hd2d_mode;   // G12-4: 3D 表现层开关

// G10.11: 交互提示气泡 — 黑底黄字小标签 (镜像 game_scene.cpp 同名实现)
static void _draw_interact_hint(const char* text, float cx, float cy) {
    if (!g_font_loaded) return;
    float tw = MeasureTextEx(g_font_small, text, 10, 1).x;
    DrawRectangle((int)(cx - tw/2 - 4), (int)(cy - 5), (int)(tw + 8), 17,
                  {25, 25, 30, 210});
    DrawRectangleLines((int)(cx - tw/2 - 4), (int)(cy - 5), (int)(tw + 8), 17,
                       {255, 210, 60, 220});
    DrawTextEx(g_font_small, text, {cx - tw/2, cy - 3}, 10, 1,
               {255, 225, 110, 255});
}

// G10.11: 拾取距离判定 — 与 _input 的实际拾取半径同源, 提示不会撒谎
static bool _in_pickup_range(const Player* p, const DroppedItem& d) {
    float ix = p->entity.rect.x + p->entity.rect.width/2;
    float iy = p->entity.rect.y + p->entity.rect.height/2;
    float tx = d.tile_x * TILE_SIZE + TILE_SIZE/2;
    float ty = d.tile_y * TILE_SIZE + TILE_SIZE/2;
    return std::hypot(ix - tx, iy - ty) <= PICKUP_RANGE * TILE_SIZE;
}

void TutorialScene::_ready() {
    name = "TutorialScene";
    game_time = 0;
    inventory_open = false;
    inv_cursor = 0;
    gave_skill = false;

    game_map = build_tutorial_map();
    player = std::make_unique<Player>(2 * TILE_SIZE, 4 * TILE_SIZE, PLAYER_SPEED,
        50, PLAYER_ATTACK, PLAYER_PDEF, PLAYER_MDEF);  // 教程用50HP方便检测药水使用
    // G10.10: 不再预装备武器 — 玩家在流程中真实走一遍 捡剑→装备→连击 闭环

    monsters.clear();
    monsters.push_back(create_tutorial_dummy(8, 4));
    ground_items = create_tutorial_items(6, 5);
    effects.clear();          // G10.11: 重进教程不复用上次特效
    pickup_msg.clear();
    pickup_msg_timer = 0.0f;

    // G10.8-fix: 教程漆黑回归 — G10.4 后地图渲染依赖 is_explored,
    // 教程从未调用 update_fov → 全部 tile 被可见性剔除 → 黑屏
    auto [px, py] = game_map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width/2,
        player->entity.rect.y + player->entity.rect.height/2);
    game_map->update_fov(px, py, 12);   // 大半径: 小沙箱全亮, 教学无视野迷雾
}

void TutorialScene::_process(double delta) {
    if (!player) return;
    const float dt = (float)delta;

    // G10.11: 特效计时与飘字 — 放在 HitStop 之前, 停顿期间视觉继续走完
    effect_drawer::update_effects(effects, dt);
    if (pickup_msg_timer > 0.0f) pickup_msg_timer -= dt;

    // G10.8-B1: HitStop 冻结期间暂停模拟 (表现层停顿)
    if (_tutorial_hitstop > 0.0f) { _tutorial_hitstop -= dt; return; }
    game_time += dt;

    // G10.8-fix: 空格连击无效根因 — 教程从不 tick WeaponComponent, recovery_timer 永不归零
    player->weapon.tick(dt);

    if (guide.stage == TutorialStage::WELCOME) return;

    if (!inventory_open) _tick_movement(dt);

    // 授予技能
    if (guide.stage == TutorialStage::SKILL && !gave_skill) {
        give_tutorial_skill(player.get());
        gave_skill = true;
    }

    // 阶段检测
    guide.check_and_advance(player.get(), inventory_open,
        reinterpret_cast<std::vector<Monster*>&>(monsters), ground_items);

    _update_camera_and_fov();
}

// G12-6: 移动 (从 _process 抽出, 满足函数长度红线)
void TutorialScene::_tick_movement(float dt) {
    const auto& input = get_tree()->get_input();
    const Vector2 move = input.get_movement_axis();
    if (input.is_action_pressed("move_up") && !input.is_action_pressed("move_down"))
        player->direction = Direction::UP;
    if (input.is_action_pressed("move_down") && !input.is_action_pressed("move_up"))
        player->direction = Direction::DOWN;
    if (input.is_action_pressed("move_left") && !input.is_action_pressed("move_right"))
        player->direction = Direction::LEFT;
    if (input.is_action_pressed("move_right") && !input.is_action_pressed("move_left"))
        player->direction = Direction::RIGHT;

    const float s = player->speed * dt;
    auto& e = player->entity;
    e.position.x += move.x * s; e.sync_rect();
    if (!game_map->is_rect_walkable(e.rect)) { e.position.x -= move.x * s; e.sync_rect(); }
    e.position.y += move.y * s; e.sync_rect();
    if (!game_map->is_rect_walkable(e.rect)) { e.position.y -= move.y * s; e.sync_rect(); }
}

// G12-6: 摄像机跟随 + 每帧同步 FOV (渲染依赖 is_explored/is_visible)
void TutorialScene::_update_camera_and_fov() {
    cam_x = player->entity.rect.x + player->entity.rect.width / 2 - get_tree()->get_width() / 2;
    cam_y = player->entity.rect.y + player->entity.rect.height / 2 - get_tree()->get_height() / 2;
    const auto [px, py] = game_map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width / 2,
        player->entity.rect.y + player->entity.rect.height / 2);
    game_map->update_fov(px, py, 12);
}

void TutorialScene::_render() {
    const int sw = get_tree()->get_width(), sh = get_tree()->get_height();

    // G12-4: 3D 表现层 —— 地形/实体/掉落/特效交给 hd2d 渲染器, 教程 UI 保持 2D 叠加
    if (!_try_render_hd2d(sw, sh)) _render_world_2d(sw, sh);

    _draw_pickup_msg(sw, sh);
    _draw_inventory_panel(sw, sh);
    _draw_element_select(sw, sh);
    _draw_tutorial_hints(sw, sh);
}

// G12-6: 2D 世界绘制 (从 _render 抽出, 满足函数长度红线)
void TutorialScene::_render_world_2d(int sw, int sh) {
    ClearBackground(BLACK);

    // 地图
    if (game_map) game_map->draw(cam_x, cam_y, sw, sh);

    // 实体
    for (auto& m : monsters) m->draw(cam_x, cam_y);
    if (player) player->draw_no_cam(cam_x, cam_y);
    _draw_monster_labels();   // G10.11: 怪物名条 (2D 坐标, 3D 下会错位故跳过)

    // 掉落物 (G10.11: 与主游戏 _draw_ground_items 同构)
    _draw_ground_items();

    // G10.11: 特效 (世界坐标, 盖在实体之上)
    for (auto& e : effects) {
        const float t = e.elapsed - e.start_delay;
        if (t < 0.0f) continue;
        effect_drawer::draw_generic_effect(e, e.world_x - cam_x,
                                           e.world_y - cam_y, t);
    }
}

// G12-6: 拾取飘字 — 居中确认, 末尾 0.3s 淡出 (与主游戏 room_msg 同款)
void TutorialScene::_draw_pickup_msg(int sw, int sh) {
    if (!g_font_loaded || pickup_msg.empty() || pickup_msg_timer <= 0.0f) return;
    const std::string line = "+ " + pickup_msg;
    const float tw = MeasureTextEx(g_font_small, line.c_str(), 18, 1).x;
    const unsigned char a = (unsigned char)std::min(
        255.0f, pickup_msg_timer / 0.3f * 255.0f);
    DrawTextEx(g_font_small, line.c_str(),
        {(float)sw / 2 - tw / 2, (float)sh / 2 - 60}, 18, 1,
        Color{255, 215, 110, a});
}

// G12-6: 背包面板 (从 _render 抽出)
void TutorialScene::_draw_inventory_panel(int sw, int sh) {
    if (!inventory_open || !player) return;
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 180});
    auto& inv = player->inventory;
    if (!g_font_loaded) return;

    DrawTextEx(g_font_small, "背包 (B关闭)",
        {sw / 2.0f - 200, sh / 2.0f - 200}, 20, 1, {200, 200, 255, 255});
    for (int i = 0; i < (int)inv.items.size(); i++) {
        const std::string mk = (i == inv_cursor) ? ">" : " ";
        const std::string txt = mk + " " + inv.items[i]->get_description();
        // M4f.13: 物品图标 (16px 贴图)
        const char* ikey = item_icon_key(inv.items[i].get());
        if (ikey) {
            SpriteDef xd;
            const Texture2D itex = ResourceManager::inst().sprite_by_key(ikey, xd);
            if (itex.id > 0)
                SpriteRenderer::draw_sprite(itex, xd, 0,
                    {sw / 2.0f - 215, sh / 2.0f - 160 + (float)i * 26 + 2, 18, 18});
        }
        DrawTextEx(g_font_small, txt.c_str(),
            {sw / 2.0f - 192, sh / 2.0f - 160 + (float)i * 26}, 16, 1,
            inv.items[i]->color);
    }
}

// G12-6: 换行描述文本 (居中对齐) —— 返回实际占用高度
static float _draw_wrapped_desc(const char* text, float cx, float top_y,
                                float size, float line_h, Color col) {
    float dy = top_y;
    std::string line;
    for (const char* p = text; *p; p++) {
        if (*p == '\n') {
            const float lw = MeasureTextEx(g_font_small, line.c_str(), size, 1).x;
            DrawTextEx(g_font_small, line.c_str(), {cx - lw / 2, dy}, size, 1, col);
            dy += line_h;
            line.clear();
        } else {
            line += *p;
        }
    }
    if (!line.empty()) {
        const float lw = MeasureTextEx(g_font_small, line.c_str(), size, 1).x;
        DrawTextEx(g_font_small, line.c_str(), {cx - lw / 2, dy}, size, 1, col);
        dy += line_h;
    }
    return dy - top_y;
}

// G12-6: ELEMENT 步骤 — 真实三卡片选择 UI (与正式游戏同款交互)
void TutorialScene::_draw_element_select(int sw, int sh) {
    if (guide.stage != TutorialStage::ELEMENT || !g_font_loaded) return;

    const char* names[] = {"[火] 火焰核心", "[冰] 冰霜核心", "[毒] 剧毒核心"};
    const char* descs[] = {"攻击概率火焰暴击\n暴击伤害 x1.5",
                           "每击附加减速\n累计触发冻结",
                           "每击附加持续毒伤\nDOT 按伤害比例"};
    const Color ecolors[] = {{255, 120, 30, 255}, {100, 200, 255, 255}, {80, 220, 80, 255}};
    const float cw = 240, chh = 190, gap = 16;
    const float sx = sw / 2.0f - (cw * 3 + gap * 2) / 2.0f;

    for (int i = 0; i < 3; i++) {
        const float cx = sx + i * (cw + gap);
        const float cy = sh * 0.36f;
        const bool sel = (element_cursor == i);
        DrawRectangleRounded({cx, cy, cw, chh}, 0.1f, 8,
            sel ? Color{50, 50, 80, 255} : Color{25, 25, 45, 255});
        DrawRectangleRoundedLines({cx - 1, cy - 1, cw + 2, chh + 2}, 0.1f, 8, 2.5f,
            sel ? ecolors[i] : Color{50, 50, 75, 220});
        const float nw = MeasureTextEx(g_font_small, names[i], 22, 1).x;
        DrawTextEx(g_font_small, names[i], {cx + cw / 2 - nw / 2, cy + 18}, 22, 1, ecolors[i]);
        _draw_wrapped_desc(descs[i], cx + cw / 2, cy + 58, 14, 22, {200, 210, 200, 220});
        if (sel) {
            DrawTextEx(g_font_small, "[A/D选择] [空格确认]",
                {cx + cw / 2 - 78, cy + chh - 26}, 13, 1, {255, 255, 180, 220});
        }
    }
}

// G12-6: 教程提示框 + 底部按键提示 (从 _render 抽出)
void TutorialScene::_draw_tutorial_hints(int sw, int sh) {
    if (!g_font_loaded) return;

    const auto lines = guide.get_instructions();
    if (!lines.empty()) {
        const float bw = 380, bh = (float)lines.size() * 24 + 30;
        const float bx = sw / 2.0f - bw / 2, by = 60;
        DrawRectangle(bx, by, bw, bh, {10, 10, 30, 220});
        DrawRectangleLines(bx, by, bw, bh, {80, 80, 150, 255});
        for (int i = 0; i < (int)lines.size(); i++) {
            DrawTextEx(g_font_small, lines[i].c_str(),
                {bx + 12, by + 16 + (float)i * 24}, 18, 1, WHITE);
        }
    }

    if (guide.stage != TutorialStage::WELCOME) {
        DrawTextEx(g_font_small, "WASD移动 | 空格攻击 | E交互 | B背包 | Shift翻滚 | P跳过本步 | T退出",
            {(float)sw / 2 - 260, (float)(sh - 24)}, 14, 1, {140, 140, 140, 255});
    }
}

// G12-4: 3D 表现层 —— 初始化成功则渲染世界返回 true; 失败则本次会话回退 2D
bool TutorialScene::_try_render_hd2d(int sw, int sh) {
    if (!g_hd2d_mode) return false;
    auto& hd2d = HD2DRenderer::inst();
    if (hd2d.ensure_init(sw, sh)) {
        // 必须先建骨骼形象再渲染 —— 没建的话渲染器会回退 2D 精灵贴图
        _avatars.ensure_player(player.get());
        _avatars.tick_player(player.get());
        _avatars.tick_monsters(monsters);
        hd2d.render_frame(hd2d_view());
        _draw_monster_labels_3d();   // G12-6: 3D 下补名条 (投影版, 非 2D 偏移版)
        return true;
    }
    g_hd2d_mode = false;   // 初始化失败: 回退 2D, 不再重试
    return false;
}

// G12-4: 教程的 3D 只读视图 —— 只填教程实际拥有的子系统, 其余留空
hd2d::SceneView TutorialScene::hd2d_view() const {
    hd2d::SceneView v;
    v.game_map = game_map.get();
    v.player = player.get();
    v.monsters = &monsters;
    v.effects = effects;
    v.dropped = ground_items;
    v.player_avatar_fn = [this] { return _avatars.player_avatar(); };
    return v;
}

// G12-6: 玩家/怪物骨骼懒建+驱动改用 AvatarDirector —— 与 game_scene.cpp 共用一份实现,
// 此前这里复制了一份 (见 git log 8e02573^), 两处容易漂移

void TutorialScene::_draw_ground_items() {
    for (auto& d : ground_items) {
        if (!game_map->isVisible(d.tile_x, d.tile_y)) continue;   // 可见性门控
        float px = d.tile_x * TILE_SIZE - cam_x;
        float py = d.tile_y * TILE_SIZE - cam_y;
        float size = TILE_SIZE - 4;
        float cx = px + TILE_SIZE/2, cy = py + TILE_SIZE/2;
        float pulse = 6 + sinf((float)GetTime() * 5 + px * 0.1f) * 3;
        bool drew = false;

        const char* ikey = item_icon_key(d.item.get());
        if (ikey) {
            SpriteDef xd;   // sprite_by_key 会从 sprites.json 覆写帧尺寸
            Texture2D itex = ResourceManager::inst().sprite_by_key(ikey, xd);
            if (itex.id > 0) {
                DrawRectangleLinesEx({cx - pulse, cy - pulse, pulse * 2, pulse * 2}, 1,
                                     rarity_color(d.item->rarity));
                SpriteRenderer::draw_sprite(itex, xd, 0,
                    {cx - size/2, cy - size/2, size, size});
                drew = true;
            }
        }
        if (!drew) {   // 贴图缺失回退: 圆角 + 描边 + 压暗光环 (与主游戏一致)
            DrawRectangleLinesEx({cx - pulse, cy - pulse, pulse * 2, pulse * 2}, 1,
                                 Color{(unsigned char)(d.item->color.r / 3),
                                        (unsigned char)(d.item->color.g / 3),
                                        (unsigned char)(d.item->color.b / 3), 200});
            DrawRectangleRounded({px + 2, py + 2, size, size}, 0.1f, 4, d.item->color);
            DrawRectangleRoundedLines({px + 2, py + 2, size, size}, 0.1f, 4, 1, BLACK);
        }
        if (player && _in_pickup_range(player.get(), d))
            _draw_interact_hint("E 拾取", cx, cy - TILE_SIZE/2 - 8);
    }
}

void TutorialScene::_draw_monster_labels() {
    if (!g_font_loaded) return;
    for (auto& m : monsters) {
        if (m->name.empty()) continue;
        if (!game_map->isVisible((int)(m->entity.rect.x / TILE_SIZE),
                                 (int)(m->entity.rect.y / TILE_SIZE))) continue;
        float mx = m->entity.rect.x + m->entity.rect.width/2 - cam_x;
        float my = m->entity.rect.y - 14 - cam_y;
        Color nc = m->is_boss  ? Color{255, 80, 40, 200}
                  : m->is_elite ? Color{255, 180, 60, 180}
                  : Color{200, 200, 200, 140};
        float tw = MeasureTextEx(g_font_small, m->name.c_str(), 10, 1).x;
        DrawTextEx(g_font_small, m->name.c_str(), {mx - tw/2, my - 4}, 10, 1, nc);
    }
}

// G12-6: 3D 怪物名条 —— 世界坐标投影到屏幕。2D 版用 cam_x/cam_y 平面偏移,
// 在 3D 透视相机下会错位, 所以 3D 路径单独走投影 (主游戏 _render_hd2d_monster_labels 同款)
void TutorialScene::_draw_monster_labels_3d() {
    auto& hd2d = HD2DRenderer::inst();
    if (!hd2d.is_ready() || !g_font_loaded) return;
    for (auto& m : monsters) {
        if (!m || !m->combat.is_alive || m->name.empty()) continue;
        if (!game_map->isVisible((int)(m->entity.rect.x / TILE_SIZE),
                                 (int)(m->entity.rect.y / TILE_SIZE))) continue;
        const Vector3 wpos = {m->entity.rect.x + m->entity.rect.width * 0.5f, 0,
                              m->entity.rect.y + m->entity.rect.height * 0.5f};
        const Vector2 s = hd2d.world_to_screen(wpos, 58.0f);   // 头顶高度
        if (s.x < 0) continue;
        const Color nc = m->is_boss  ? Color{255, 80, 40, 200}
                         : m->is_elite ? Color{255, 180, 60, 180}
                         : Color{200, 200, 200, 140};
        const float tw = MeasureTextEx(g_font_small, m->name.c_str(), 10, 1).x;
        DrawTextEx(g_font_small, m->name.c_str(), {s.x - tw/2, s.y - 4}, 10, 1, nc);
    }
}

void TutorialScene::_on_pickup(const std::string& name) {
    get_tree()->get_audio()->play_sfx("pickup", 0.55f);   // 与主游戏音量对齐
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    VFXServer vfx;
    vfx.ring(px, py, 22.0f, Color{255, 200, 120, 200}, 2, 0.35f);
    vfx.spark_burst(px, py, 8, Color{255, 220, 160, 210}, 0.30f);
    for (auto& e : vfx.effects) effects.push_back(e);
    pickup_msg = "拾取: " + name;
    pickup_msg_timer = 2.5f;
}

void TutorialScene::_input(const InputMap& input) {
    if (!player) return;
    if (_input_quit_or_skip(input)) return;
    if (_input_gate_stage(input)) return;

    _tick_cooldown_stage();

    if (inventory_open) { _input_inventory(input); return; }

    _input_actions(input);
    _advance_stage_if_needed();
}

// G12-6: 退出 (Esc/T) + P 跳过当前阶段
bool TutorialScene::_input_quit_or_skip(const InputMap& input) {
    if (input.is_action_just_pressed("cancel") || IsKeyPressed(KEY_T)) {
        auto ts = std::make_shared<TitleScene>();
        ts->name = "TitleScene";
        get_tree()->change_scene(ts);
        LOG_INFO("退出教程");
        return true;
    }
    if (IsKeyPressed(KEY_P) && guide.stage != TutorialStage::WELCOME
        && guide.stage != TutorialStage::COMPLETE) {
        guide.advance_stage();
        LOG_INFO("跳过教程阶段");
        return true;
    }
    return false;
}

// G12-6: 阻挡型阶段 (这些阶段只吃自己的输入, 处理完即止)
bool TutorialScene::_input_gate_stage(const InputMap& input) {
    if (guide.stage == TutorialStage::WELCOME) {
        if (input.is_action_just_pressed("confirm")) guide.advance_stage();
        return true;
    }
    if (guide.stage == TutorialStage::COMPLETE) {
        if (input.is_action_just_pressed("confirm")) {
            auto ts = std::make_shared<TitleScene>();
            ts->name = "TitleScene";
            get_tree()->change_scene(ts);
        }
        return true;
    }
    if (guide.stage == TutorialStage::ELEMENT) { _input_element_select(input); return true; }
    if (guide.stage == TutorialStage::WEAPON_INFO) {
        if (input.is_action_just_pressed("confirm")) guide.advance_stage();
        return true;
    }
    return false;
}

// G12-6: ELEMENT 步骤 — 卡片导航 (与正式游戏同款 左右选+确认)
void TutorialScene::_input_element_select(const InputMap& input) {
    if (input.is_action_just_pressed("move_left"))
        element_cursor = (element_cursor + 2) % 3;
    if (input.is_action_just_pressed("move_right"))
        element_cursor = (element_cursor + 1) % 3;
    if (input.is_action_just_pressed("attack") || input.is_action_just_pressed("pickup")) {
        static const ElementType choices[] = {
            ElementType::FIRE, ElementType::ICE, ElementType::POISON };
        player->element.select(choices[element_cursor]);
        get_tree()->get_audio()->play_sfx("ui_confirm");
    }
}

// G12-6: COOLDOWN 步骤 — 1.5s 后自动通过 (玩家观察蓝条变化)
void TutorialScene::_tick_cooldown_stage() {
    if (guide.stage != TutorialStage::COOLDOWN) return;
    static float cd_timer = 0.0f;
    cd_timer += GetFrameTime();
    if (cd_timer > 1.5f) {
        guide.cooldown_waited = true;
        cd_timer = 0.0f;
    }
}

// G12-6: 背包模式交互
void TutorialScene::_input_inventory(const InputMap& input) {
    const auto clamp_cursor = [&] {
        inv_cursor = std::min(inv_cursor,
            std::max(0, (int)player->inventory.items.size() - 1));
    };
    if (input.is_action_just_pressed("inventory") || input.is_action_just_pressed("cancel")) {
        inventory_open = false;
        return;
    }
    if (input.is_action_just_pressed("move_up"))
        inv_cursor = std::max(0, inv_cursor - 1);
    if (input.is_action_just_pressed("move_down"))
        inv_cursor = std::min((int)player->inventory.items.size() - 1, inv_cursor + 1);
    if (IsKeyPressed(KEY_X)) { player->inventory.equip(inv_cursor, player.get()); clamp_cursor(); }
    if (IsKeyPressed(KEY_U)) {
        if (guide.stage == TutorialStage::INVENTORY) guide.item_used = true;
        player->inventory.use_item(inv_cursor, player.get());
        clamp_cursor();
    }
    _advance_stage_if_needed();
}

// G12-6: 战斗 / 拾取 / 技能键
void TutorialScene::_input_actions(const InputMap& input) {
    // G10.8-B1: 接入 WeaponExecutor — 教程与正式游戏共享同一战斗链
    // (三段连击/HitShape/元素/HitStop 全一致, 消除 legacy fist 体验断层)
    if (input.is_action_just_pressed("attack") && player->weapon.can_attack(game_time)) {
        std::vector<Monster*> ml;
        for (auto& m : monsters) ml.push_back(m.get());
        const auto results = WeaponExecutor::execute(
            player.get(), ml, game_time,
            get_tree()->get_audio(), nullptr, game_map.get());
        for (auto& r : results)
            get_tree()->get_audio()->play_sfx("hit");
        if (!results.empty())
            _tutorial_hitstop = CombatFeelSystem::LIGHT_HIT;
    }

    if (input.is_action_just_pressed("pickup")) _try_pickup_nearest();
    if (input.is_action_just_pressed("inventory")) { inventory_open = true; inv_cursor = 0; }
    if (input.is_action_just_pressed("skill_1") && guide.stage == TutorialStage::SKILL
        && !guide._skill_used) {
        guide.notify_skill_used();
        get_tree()->get_audio()->play_sfx("slash");
    }
}

// G12-6: 就近拾取地面物品
void TutorialScene::_try_pickup_nearest() {
    DroppedItem* best = nullptr;
    float bd = PICKUP_RANGE * TILE_SIZE;
    for (auto& d : ground_items) {
        const float px = d.tile_x * TILE_SIZE + TILE_SIZE / 2;
        const float py = d.tile_y * TILE_SIZE + TILE_SIZE / 2;
        const float dist = std::hypot(player->entity.rect.x + player->entity.rect.width / 2 - px,
                                      player->entity.rect.y + player->entity.rect.height / 2 - py);
        if (dist < bd) { bd = dist; best = &d; }
    }
    if (!best) return;
    const std::string name = best->item->base_name;
    if (!player->inventory.add(best->item, player.get())) return;
    const auto it = std::find_if(ground_items.begin(), ground_items.end(),
        [&](auto& x) { return &x == best; });
    if (it != ground_items.end()) ground_items.erase(it);
    _on_pickup(name);   // G10.11: 音效 + VFX + 飘字
}

// G12-6: 阶段推进检测 (背包与战斗两条路径共用)
void TutorialScene::_advance_stage_if_needed() {
    guide.check_and_advance(player.get(), inventory_open,
        reinterpret_cast<std::vector<Monster*>&>(monsters), ground_items);
}
