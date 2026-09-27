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
#include "animation/skeleton_avatar.h"    // G12-4: 怪物骨骼形象
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
    float dt = (float)delta;

    // G10.11: 特效计时与飘字 — 放在 HitStop 之前, 停顿期间视觉继续走完
    effect_drawer::update_effects(effects, dt);
    if (pickup_msg_timer > 0.0f) pickup_msg_timer -= dt;

    // G10.8-B1: HitStop 冻结期间暂停模拟 (表现层停顿)
    if (_tutorial_hitstop > 0.0f) {
        _tutorial_hitstop -= dt;
        return;
    }
    game_time += dt;

    // G10.8-fix: 空格连击无效根因 — 教程从不 tick WeaponComponent,
    // recovery_timer 永不归零 → 首击后 can_act() 恒 false
    player->weapon.tick(dt);

    if (guide.stage == TutorialStage::WELCOME) return;

    auto& input = get_tree()->get_input();

    if (!inventory_open) {
        // 移动
        Vector2 move = input.get_movement_axis();
        if (input.is_action_pressed("move_up") && !input.is_action_pressed("move_down"))
            player->direction = Direction::UP;
        if (input.is_action_pressed("move_down") && !input.is_action_pressed("move_up"))
            player->direction = Direction::DOWN;
        if (input.is_action_pressed("move_left") && !input.is_action_pressed("move_right"))
            player->direction = Direction::LEFT;
        if (input.is_action_pressed("move_right") && !input.is_action_pressed("move_left"))
            player->direction = Direction::RIGHT;

        float s = player->speed * dt;
        auto& e = player->entity;
        e.position.x += move.x * s; e.sync_rect();
        if (!game_map->is_rect_walkable(e.rect)) { e.position.x -= move.x * s; e.sync_rect(); }
        e.position.y += move.y * s; e.sync_rect();
        if (!game_map->is_rect_walkable(e.rect)) { e.position.y -= move.y * s; e.sync_rect(); }
    }

    // 授予技能
    if (guide.stage == TutorialStage::SKILL && !gave_skill) {
        give_tutorial_skill(player.get());
        gave_skill = true;
    }

    // 阶段检测
    guide.check_and_advance(player.get(), inventory_open,
        reinterpret_cast<std::vector<Monster*>&>(monsters), ground_items);

    // 摄像机
    cam_x = player->entity.rect.x + player->entity.rect.width/2 - get_tree()->get_width()/2;
    cam_y = player->entity.rect.y + player->entity.rect.height/2 - get_tree()->get_height()/2;

    // G10.8-fix: 每帧同步 FOV (渲染依赖 is_explored/is_visible)
    if (game_map) {
        auto [px, py] = game_map->pixel_to_tile(
            player->entity.rect.x + player->entity.rect.width/2,
            player->entity.rect.y + player->entity.rect.height/2);
        game_map->update_fov(px, py, 12);
    }
}

void TutorialScene::_render() {
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();

    // G12-4: 3D 表现层 —— 地形/实体/掉落/特效交给 hd2d 渲染器, 教程 UI 保持 2D 叠加
    bool use_3d = _try_render_hd2d(sw, sh);
    if (!use_3d) {
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
            float t = e.elapsed - e.start_delay;
            if (t < 0.0f) continue;
            effect_drawer::draw_generic_effect(e, e.world_x - cam_x,
                                               e.world_y - cam_y, t);
        }
    }

    // G10.11: 拾取飘字 — 居中确认, 末尾 0.3s 淡出 (与主游戏 room_msg 同款)
    if (g_font_loaded && !pickup_msg.empty() && pickup_msg_timer > 0.0f) {
        std::string line = "+ " + pickup_msg;
        float tw = MeasureTextEx(g_font_small, line.c_str(), 18, 1).x;
        unsigned char a = (unsigned char)std::min(255.0f,
                              pickup_msg_timer / 0.3f * 255.0f);
        DrawTextEx(g_font_small, line.c_str(),
            {(float)sw/2 - tw/2, (float)sh/2 - 60}, 18, 1,
            Color{255, 215, 110, a});
    }

    // 背包面板
    if (inventory_open && player) {
        DrawRectangle(0, 0, sw, sh, {0, 0, 0, 180});
        auto& inv = player->inventory;
        if (g_font_loaded) {
            DrawTextEx(g_font_small, "背包 (B关闭)", {sw/2.0f - 200, sh/2.0f - 200}, 20, 1, {200, 200, 255, 255});
            for (int i = 0; i < (int)inv.items.size(); i++) {
                std::string mk = (i == inv_cursor) ? ">" : " ";
                std::string txt = mk + " " + inv.items[i]->get_description();
                // M4f.13: 物品图标 (16px 贴图)
                const char* ikey = item_icon_key(inv.items[i].get());
                if (ikey) {
                    SpriteDef xd;
                    Texture2D itex = ResourceManager::inst().sprite_by_key(ikey, xd);
                    if (itex.id > 0)
                        SpriteRenderer::draw_sprite(itex, xd, 0,
                            {sw/2.0f - 215, sh/2.0f - 160 + (float)i * 26 + 2, 18, 18});
                }
                DrawTextEx(g_font_small, txt.c_str(),
                    {sw/2.0f - 192, sh/2.0f - 160 + (float)i * 26}, 16, 1, inv.items[i]->color);
            }
        }
    }

    // G10.8-B2: ELEMENT 步骤 — 真实三卡片选择 UI (与正式游戏同款交互)
    if (guide.stage == TutorialStage::ELEMENT) {
        const char* names[] = {"[火] 火焰核心", "[冰] 冰霜核心", "[毒] 剧毒核心"};
        const char* descs[] = {"攻击概率火焰暴击\n暴击伤害 x1.5",
                               "每击附加减速\n累计触发冻结",
                               "每击附加持续毒伤\nDOT 按伤害比例"};
        const Color ecolors[] = {{255,120,30,255},{100,200,255,255},{80,220,80,255}};
        float cw = 240, chh = 190, gap = 16;
        float sx = sw/2.0f - (cw*3 + gap*2)/2.0f;
        for (int i = 0; i < 3; i++) {
            float cx = sx + i * (cw + gap);
            float cy = sh * 0.36f;
            bool sel = (element_cursor == i);
            DrawRectangleRounded({cx, cy, cw, chh}, 0.1f, 8,
                sel ? Color{50,50,80,255} : Color{25,25,45,255});
            DrawRectangleRoundedLines({cx-1, cy-1, cw+2, chh+2}, 0.1f, 8, 2.5f,
                sel ? ecolors[i] : Color{50,50,75,220});
            float nw = MeasureTextEx(g_font_small, names[i], 22, 1).x;
            DrawTextEx(g_font_small, names[i], {cx + cw/2 - nw/2, cy + 18}, 22, 1, ecolors[i]);
            float dy = cy + 58;
            std::string line;
            for (const char* p = descs[i]; *p; p++) {
                if (*p == '\n') {
                    float lw = MeasureTextEx(g_font_small, line.c_str(), 14, 1).x;
                    DrawTextEx(g_font_small, line.c_str(), {cx + cw/2 - lw/2, dy}, 14, 1, {200,210,200,220});
                    dy += 22; line.clear();
                } else line += *p;
            }
            if (!line.empty()) {
                float lw = MeasureTextEx(g_font_small, line.c_str(), 14, 1).x;
                DrawTextEx(g_font_small, line.c_str(), {cx + cw/2 - lw/2, dy}, 14, 1, {200,210,200,220});
            }
            if (sel) {
                DrawTextEx(g_font_small, "[A/D选择] [空格确认]",
                    {cx + cw/2 - 78, cy + chh - 26}, 13, 1, {255,255,180,220});
            }
        }
    }

    // 教程提示框
    auto lines = guide.get_instructions();
    if (!lines.empty()) {
        float bw = 380, bh = (float)lines.size() * 24 + 30;
        float bx = sw/2.0f - bw/2, by = 60;
        DrawRectangle(bx, by, bw, bh, {10, 10, 30, 220});
        DrawRectangleLines(bx, by, bw, bh, {80, 80, 150, 255});
        if (g_font_loaded) {
            for (int i = 0; i < (int)lines.size(); i++) {
                DrawTextEx(g_font_small, lines[i].c_str(),
                    {bx + 12, by + 16 + (float)i * 24}, 18, 1, WHITE);
            }
        }
    }

    // 底部按键提示
    if (g_font_loaded && guide.stage != TutorialStage::WELCOME) {
        DrawTextEx(g_font_small, "WASD移动 | 空格攻击 | E交互 | B背包 | Shift翻滚 | P跳过本步 | T退出",
            {(float)sw/2 - 260, (float)(sh - 24)}, 14, 1, {140, 140, 140, 255});
    }
}

// G12-4: 3D 表现层 —— 初始化成功则渲染世界返回 true; 失败则本次会话回退 2D
bool TutorialScene::_try_render_hd2d(int sw, int sh) {
    if (!g_hd2d_mode) return false;
    auto& hd2d = HD2DRenderer::inst();
    if (hd2d.ensure_init(sw, sh)) {
        // 必须先建骨骼形象再渲染 —— 没建的话渲染器会回退 2D 精灵贴图
        _ensure_player_avatar();
        _player_avatar_tick();
        _monster_avatars_tick();
        hd2d.render_frame(hd2d_view());
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
    v.player_avatar_fn = [this] { return _player_avatar.get(); };
    return v;
}

// G12-4: 玩家骨骼形象 —— 与 game_scene.cpp 同名实现一致 (懒建一次)
void TutorialScene::_ensure_player_avatar() {
    if (!player || _player_avatar) return;
    auto avatar = std::make_unique<PlayerAvatar>();
    std::string avatar_err;
    if (avatar->try_init("resources/animations", avatar_err))
        LOG_INFO("G12-4: tutorial player avatar active (skeletal)");
    else
        LOG_WARN("G12-4: tutorial avatar inactive, fallback static (%s)", avatar_err.c_str());
    _player_avatar = std::move(avatar);
}

// G12-4: 玩家骨骼驱动
void TutorialScene::_player_avatar_tick() {
    if (_player_avatar && _player_avatar->active() && player)
        _player_avatar->update(GetFrameTime(), *player);
}

// G12-4: 怪物骨骼皮肤 —— 白名单命中懒建一次 (成败都缓存), 与玩家同款渲染驱动
void TutorialScene::_monster_avatars_tick() {
    if (!_actor_avatars_loaded) {
        _actor_avatars_loaded = true;
        std::string conf_err;
        auto conf = load_actor_avatars_file("resources/animations/actor_avatars.json", conf_err);
        if (conf) _actor_avatars = std::move(*conf);
        else LOG_WARN("G12-4: actor_avatars.json invalid, all fallback (%s)", conf_err.c_str());
    }
    if (_actor_avatars.empty()) return;    // 白名单空 = 零开销全回退
    const float dt = GetFrameTime();
    const float now_wall = (float)GetTime();
    for (auto& m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        if (!m->skeleton_avatar()) {
            auto it = _actor_avatars.find(monster_actor_key(*m));
            if (it == _actor_avatars.end()) continue;
            auto avatar = std::make_unique<SkeletonAvatar>();
            std::string avatar_err;
            if (avatar->try_init(it->second.skeleton, it->second.anim, avatar_err))
                LOG_INFO("G12-4: tutorial monster avatar active (%s)", it->first.c_str());
            else
                LOG_WARN("G12-4: tutorial monster avatar inactive (%s): %s",
                         it->first.c_str(), avatar_err.c_str());
            m->set_skeleton_avatar(std::move(avatar));   // 成功/失败都缓存不重试
        }
        auto* avatar = m->skeleton_avatar();
        if (!avatar || !avatar->active()) continue;
        avatar->advance(dt, monster_anim_input(*m, avatar->hp_state(), now_wall));
        avatar->track_facing(m->entity.position);        // 渲染层朝向镜像
    }
}

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

    if (input.is_action_just_pressed("cancel") || IsKeyPressed(KEY_T)) {
        auto ts = std::make_shared<TitleScene>();
        ts->name = "TitleScene";
        get_tree()->change_scene(ts);
        LOG_INFO("退出教程");
        return;
    }

    // P键跳过当前阶段
    if (IsKeyPressed(KEY_P) && guide.stage != TutorialStage::WELCOME
        && guide.stage != TutorialStage::COMPLETE) {
        guide.advance_stage();
        LOG_INFO("跳过教程阶段");
        return;
    }

    if (guide.stage == TutorialStage::WELCOME) {
        if (input.is_action_just_pressed("confirm")) {
            guide.advance_stage();
        }
        return;
    }

    if (guide.stage == TutorialStage::COMPLETE) {
        if (input.is_action_just_pressed("confirm")) {
            auto ts = std::make_shared<TitleScene>();
            ts->name = "TitleScene";
            get_tree()->change_scene(ts);
        }
        return;
    }

    // G10.8-B2: ELEMENT 步骤 — 卡片导航 (与正式游戏同款 左右选+确认)
    if (guide.stage == TutorialStage::ELEMENT) {
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
        return;
    }

    // G10.8-B2: COOLDOWN 步骤 — 1.5s 后自动通过（玩家观察蓝条变化）
    if (guide.stage == TutorialStage::COOLDOWN) {
        static float cd_timer = 0.0f;
        cd_timer += GetFrameTime();
        if (cd_timer > 1.5f) {
            guide.cooldown_waited = true;
            cd_timer = 0.0f;
        }
    } else {
        // 重置 static 计时器（离开步骤时）
    }

    // WEAPON_INFO 步骤 — Enter 进入 COMPLETE
    if (guide.stage == TutorialStage::WEAPON_INFO) {
        if (input.is_action_just_pressed("confirm")) {
            guide.advance_stage();
        }
        return;
    }

    // 背包模式
    if (inventory_open) {
        if (input.is_action_just_pressed("inventory") || input.is_action_just_pressed("cancel"))
            { inventory_open = false; return; }
        if (input.is_action_just_pressed("move_up"))    inv_cursor = std::max(0, inv_cursor - 1);
        if (input.is_action_just_pressed("move_down"))  inv_cursor = std::min((int)player->inventory.items.size() - 1, inv_cursor + 1);
        if (IsKeyPressed(KEY_X)) { player->inventory.equip(inv_cursor, player.get()); inv_cursor = std::min(inv_cursor, std::max(0, (int)player->inventory.items.size() - 1)); }
        if (IsKeyPressed(KEY_U)) {
            if (guide.stage == TutorialStage::INVENTORY) guide.item_used = true;
            player->inventory.use_item(inv_cursor, player.get());
            inv_cursor = std::min(inv_cursor, std::max(0, (int)player->inventory.items.size() - 1));
        }
        guide.check_and_advance(player.get(), inventory_open,
            reinterpret_cast<std::vector<Monster*>&>(monsters), ground_items);
        return;
    }

    // 游戏操作
    if (input.is_action_just_pressed("attack")) {
        // G10.8-B1: 接入 WeaponExecutor — 教程与正式游戏共享同一战斗链
        // (三段连击/HitShape/元素/HitStop 全一致, 消除 legacy fist 体验断层)
        if (player->weapon.can_attack(game_time)) {
            std::vector<Monster*> ml;
            for (auto& m : monsters) ml.push_back(m.get());
            auto results = WeaponExecutor::execute(
                player.get(), ml, game_time,
                get_tree()->get_audio(), nullptr, game_map.get());
            for (auto& r : results) {
                // 命中反馈: 伤害数字 + HitStop (走正式游戏的 PresentationDirector)
                get_tree()->get_audio()->play_sfx("hit");
            }
            if (!results.empty()) {
                // 轻量打击停顿 — 复用正式游戏 CombatFeelSystem 常量
                _tutorial_hitstop = CombatFeelSystem::LIGHT_HIT;
            }
        }
    }
    if (input.is_action_just_pressed("pickup")) {
        DroppedItem* best = nullptr;
        float bd = PICKUP_RANGE * TILE_SIZE;
        for (auto& d : ground_items) {
            float px = d.tile_x * TILE_SIZE + TILE_SIZE/2, py = d.tile_y * TILE_SIZE + TILE_SIZE/2;
            float dist = std::hypot(player->entity.rect.x + player->entity.rect.width/2 - px,
                                     player->entity.rect.y + player->entity.rect.height/2 - py);
            if (dist < bd) { bd = dist; best = &d; }
        }
        if (best) {
            std::string name = best->item->base_name;
            if (player->inventory.add(best->item, player.get())) {
                auto it = std::find_if(ground_items.begin(), ground_items.end(),
                    [&](auto& x) { return &x == best; });
                if (it != ground_items.end()) ground_items.erase(it);
                _on_pickup(name);   // G10.11: 音效 + VFX + 飘字
            }
        }
    }
    if (input.is_action_just_pressed("inventory")) {
        inventory_open = true; inv_cursor = 0;
    }
    if (input.is_action_just_pressed("skill_1") && guide.stage == TutorialStage::SKILL && !guide._skill_used) {
        guide.notify_skill_used();
        get_tree()->get_audio()->play_sfx("slash");
    }

    guide.check_and_advance(player.get(), inventory_open,
        reinterpret_cast<std::vector<Monster*>&>(monsters), ground_items);
}
