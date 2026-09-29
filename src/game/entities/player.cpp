#include "player.h"
#include "skill.h"
#include "item.h"
#include "config.h"
#include "resource_manager.h"                 // M4f.2
#include "game/rendering/sprite_renderer.h"   // M4f.2
#include "systems/relic_effect_processor.h"
#include "world/game_map.h"                    // G10.6-B C1: 朝墙视觉查询
#include <cmath>
#include <algorithm>

// ============================================================
// D2: ComboState 实现
// ============================================================
float ComboState::multiplier() const {
    if (count == 4) return 2.2f;
    if (count == 3) return 1.4f;
    if (count == 2) return 1.15f;
    return 1.0f;
}

bool ComboState::is_heavy() const { return count == 4; }

void ComboState::hit(double game_time) {
    if (timer <= 0) count = 0;
    count++;
    if (count > 4) count = 1;
    last_hit_time = (float)game_time;
    timer = WINDOW;
}

void ComboState::tick(float dt) {
    if (timer > 0) timer -= dt;
    if (timer <= 0) { timer = 0; count = 0; }
}

// D2 Step2: 消耗重击 — 技能/Relic/Boss 统一接口
bool Player::consume_heavy_combo() {
    if (!combo.is_heavy()) return false;
    combo.count = 0;  // 重置连击 (已消耗)
    return true;
}

Player::Player(float x, float y, float spd, int hp, int atk, int pdef, int mdef)
    : entity(x, y, 32, 32, 28, 28), speed(spd), combat(hp, atk, pdef, mdef),
      inventory(INVENTORY_MAX) {}

// Q3.2 平衡: lvl²×20 → lvl×30+50 — 前期升级更快, 玩家到 F5 可达 Lv4-6 (原 Lv3)
int Player::calc_xp_for_level(int lvl) { return lvl * 30 + 50; }

int Player::attack_target(Player* target, double game_time) {
    (void)target; (void)game_time;
    return 0; // 玩家打怪物用 combat_system
}

void Player::render(Camera2D& cam) { (void)cam; }

void Player::reset_attack_timers() {
    _last_attack_time = -999.0f;
    _last_skill_time = -999.0f;   // M4e
    weapon.runtime().last_attack_time = -999.0f;  // G9: floor transition fix
    for (auto& s : skills.active_skills) {
        s->last_use_time = -999.0f;
    }
    for (auto& s : skills.passives) {
        s->last_use_time = -999.0f;
    }
    dodge.reset();  // B3: 换层/重开清翻滚态与冷却
}

// P1-C7-A: Player::can_attack 已删 — 空手/持械统一走 WeaponComponent::can_attack

Vector2 Player::handle_input(const InputMap& input) {
    Vector2 move = input.get_movement_axis();

    if (input.is_action_pressed("move_up") && !input.is_action_pressed("move_down"))
        direction = Direction::UP;
    if (input.is_action_pressed("move_down") && !input.is_action_pressed("move_up"))
        direction = Direction::DOWN;
    if (input.is_action_pressed("move_left") && !input.is_action_pressed("move_right"))
        direction = Direction::LEFT;
    if (input.is_action_pressed("move_right") && !input.is_action_pressed("move_left"))
        direction = Direction::RIGHT;

    is_moving = (move.x != 0 || move.y != 0);
    return move;
}

void Player::give_xp(int amount) { xp += amount; }

void Player::auto_level_to(int target) {
    while (level < target) {
        xp = xp_to_next;
        // give_xp 在 game_scene 中会触发升级
    }
}

// M4f.2: 纹理缺失时的几何回退
static void _draw_legacy_player_body(float hx, float hy, float hw, float hh,
                                     Color body_c, Direction dir) {
    Rectangle body_r = {hx + 3, hy + 2, hw - 6, hh - 4};
    DrawRectangleRounded(body_r, 0.2f, 4, body_c);
    DrawRectangleRounded({hx + 5, hy + 3, hw - 10, 8}, 0.15f, 3,
                          {80, 210, 80, 255});
    float cx = hx + hw / 2, cy = hy + hh / 2;
    DrawCircle(cx, cy, 5, WHITE);
    float ox = 0, oy = 3;
    switch (dir) {
        case Direction::UP:    oy = -3; break;
        case Direction::DOWN:  oy = 3;  break;
        case Direction::LEFT:  ox = -3; break;
        case Direction::RIGHT: ox = 3;  break;
    }
    DrawCircle(cx + ox, cy + oy, 3, {20, 20, 20, 255});
}

// M4f.4: 元素核心 → 玩家精灵 key (毒/冰/火三形象)
static const char* _player_sprite_key(ElementType e) {
    switch (e) {
        case ElementType::FIRE:   return "player_fire";
        case ElementType::ICE:    return "player_ice";
        case ElementType::POISON: return "player_poison";
        default:                  return "player_poison";
    }
}

// G9.4: 素材精灵无朝向 → 眼睛指示器 (瞳孔朝方向偏移, 远程瞄准可辨)
static void _draw_facing_eyes(float hx, float hy, float hw, float hh,
                              Direction dir) {
    float cx = hx + hw / 2, cy = hy + hh * 0.16f;
    float dx = 0.0f, dy = 0.0f;
    switch (dir) {
        case Direction::UP:    dy = -2.0f; break;
        case Direction::DOWN:  dy = 2.0f;  break;
        case Direction::LEFT:  dx = -2.0f; break;
        case Direction::RIGHT: dx = 2.0f;  break;
    }
    DrawCircle(cx - 2.5f, cy, 2.0f, {255, 255, 255, 235});
    DrawCircle(cx + 2.5f, cy, 2.0f, {255, 255, 255, 235});
    DrawCircle(cx - 2.5f + dx, cy + dy, 1.1f, {15, 15, 20, 255});
    DrawCircle(cx + 2.5f + dx, cy + dy, 1.1f, {15, 15, 20, 255});
}

// M4f.2/3: 程序化占位精灵 (四方向 + 呼吸帧), 素材未配置时使用
static void _draw_procedural_player(float hx, float hy, float hw, float hh,
                                    Color body_c, Direction dir) {
    int eye_dir = (int)dir;
    char key[32];
    snprintf(key, sizeof(key), "ply_%d_%02x%02x%02x",
        eye_dir, body_c.r, body_c.g, body_c.b);
    Texture2D tex = ResourceManager::inst().procedural_sprite(
        key, body_c, {80, 210, 80, 255}, 0, eye_dir);
    if (tex.id > 0) {
        SpriteDef sd; sd.frame_w = 32; sd.frame_h = 32; sd.frame_count = 2;
        int frame = ((int)(GetTime() * 4)) & 1;   // M4f.3: 待机/呼吸轮换
        SpriteRenderer::draw_sprite(tex, sd, frame, {hx, hy, hw, hh});
    } else {
        _draw_legacy_player_body(hx, hy, hw, hh, body_c, dir);
    }
}

// G9.4: 武器角度 — 每类武器独立待机姿态(摆动幅度/频率) 与攻击轨迹
// 姿态个性: 匕首敏捷快颤 / 剑沉稳 / 矛挺拔轻颤 / 弩瞄准微调 / 双节棍惯性晃
static float _weapon_angle(WeaponType wt, bool attacking, float p, Direction dir) {
    float t = GetTime();
    const float PI_F = 3.14159265f;
    float base = 0.0f;
    if (wt == WeaponType::NUNCHAKU)
        base = attacking ? 360.0f * p : 8.0f * sinf(t * 3.0f);
    else if (wt == WeaponType::CROSSBOW)
        base = attacking ? 15.0f * sinf(p * PI_F) : 3.0f * sinf(t * 1.5f);
    else if (wt == WeaponType::SPEAR)
        base = attacking ? 90.0f * sinf(p * PI_F) : 2.0f * sinf(t * 1.2f);
    else if (wt == WeaponType::DAGGER)
        base = attacking ? 90.0f * sinf(p * PI_F) : 4.0f * sinf(t * 4.0f);
    else
        base = attacking ? 90.0f * sinf(p * PI_F) : 3.0f * sinf(t * 2.0f);
    if (dir == Direction::LEFT || dir == Direction::UP) return -base;
    return base;
}

// G9.4: 武器握持 — 手腕锚点 + 待机/挥砍角度 + 朝向镜像
// 素材朝向: 刀/剑尖朝下(柄在顶), 矛尖朝上(柄在底), 弩弓臂朝上(托在底)
static void _draw_player_weapon(Player* self, Direction dir,
                                float hx, float hy, float hw, float hh,
                                bool wall_ahead) {   // G10.6-B C1
    const char* wkey = weapon_sprite_key(self->weapon.weapon_type());
    if (!wkey) return;
    SpriteDef wdef;
    Texture2D wtex = ResourceManager::inst().sprite_by_key(wkey, wdef);
    if (wtex.id <= 0) return;

    // 挥砍动画: is_attacking 上升沿记录起始, 结束重置
    if (self->weapon.is_attacking()) {
        if (self->_swing_start < 0.0f) self->_swing_start = GetTime();
    } else {
        self->_swing_start = -1.0f;
    }

    WeaponType wt = self->weapon.weapon_type();
    bool is_spear = wt == WeaponType::SPEAR;
    bool is_crossbow = wt == WeaponType::CROSSBOW;
    bool is_nunchaku = wt == WeaponType::NUNCHAKU;
    bool bottom_grip = is_spear || is_crossbow;
    float w = is_spear ? 10.0f : (is_crossbow ? 16.0f : 12.0f);
    float h = is_spear ? 26.0f
             : (is_crossbow ? 13.0f
             : (wt == WeaponType::DAGGER ? 14.0f : 18.0f));

    // 手腕锚点 (身体中心 + 朝向偏移) — G10.6-B C1: 朝墙时横向偏移减半
    float cx = hx + hw / 2, cy = hy + hh * 0.55f;
    float ox = 0.0f;
    switch (dir) {
        case Direction::DOWN:  ox = hw * 0.30f;  break;
        case Direction::UP:    ox = -hw * 0.30f; break;
        case Direction::LEFT:  ox = -hw * 0.42f; break;
        case Direction::RIGHT: ox = hw * 0.42f;  break;
    }
    if (wall_ahead) ox *= 0.5f;

    // 角度: 每类武器独立待机姿态 + 攻击轨迹 (见 _weapon_angle)
    float p = 0.0f;
    if (self->_swing_start >= 0.0f)
        p = std::min(1.0f, (float)(GetTime() - self->_swing_start) / 0.35f);
    float angle = _weapon_angle(wt, self->_swing_start >= 0.0f, p, dir);

    // 柄部锚点: 刀/剑在顶部, 矛/弩在底部
    Vector2 origin = bottom_grip ? Vector2{w / 2, h - 2} : Vector2{w / 2, 2};
    float dst_y = bottom_grip ? cy - h + 2 : cy - 2;
    Rectangle src = SpriteRenderer::frame_rect(wdef, 0);
    // raylib: 绘制位置 = dest - origin → dest 补偿 origin 后, 主体归位且旋转真正绕柄部
    Rectangle dst = {cx + ox - w / 2 + origin.x, dst_y + origin.y, w, h};
    DrawTexturePro(wtex, src, dst, origin, angle, WHITE);
}

// G9.4: 攻击姿态 — 挥砍进度 p(0-1) → 朝向位移 (重击幅度更大)
static Rectangle _attack_pose_rect(Direction dir, Rectangle base, float p,
                                   bool heavy) {
    float amp = heavy ? 7.0f : 4.0f;
    float k = sinf(p * 3.14159265f);
    float dx = 0.0f, dy = 0.0f;
    switch (dir) {
        case Direction::DOWN:  dy = amp * k; break;
        case Direction::UP:    dy = -amp * k; break;
        case Direction::LEFT:  dx = -amp * k; break;
        case Direction::RIGHT: dx = amp * k; break;
    }
    return {base.x + dx, base.y + dy, base.width, base.height};
}

// G10.6-B C1: 朝墙检测 — facing 前方 tile 是否不可走 (纯视觉查询)
// 边界: 不阻止攻击/不改判定, 仅限制身体模型的视觉进入墙体
static bool _facing_tile_blocked(const struct GameMap* map, Vector2 player_center,
                                 Direction dir) {
    if (!map) return false;
    float probe_x = player_center.x;
    float probe_y = player_center.y;
    const float PROBE_DIST = 20.0f;   // 视觉盒半径 16 + 前倾峰值 7 缓冲
    switch (dir) {
        case Direction::UP:    probe_y -= PROBE_DIST; break;
        case Direction::DOWN: probe_y += PROBE_DIST; break;
        case Direction::LEFT:  probe_x -= PROBE_DIST; break;
        case Direction::RIGHT: probe_x += PROBE_DIST; break;
    }
    int tx = (int)(probe_x / 32);
    int ty = (int)(probe_y / 32);
    if (tx < 0 || ty < 0) return true;
    return !map->is_walkable(tx, ty);
}

// D2: 连击段数 → 身体颜色渐变 (程序化占位用)
static Color _combo_body_color(int count) {
    if (count >= 4) return Color{255, 200, 40, 255};
    if (count >= 3) return Color{80, 200, 80, 255};
    if (count >= 2) return Color{60, 180, 140, 255};
    return Color{40, 160, 40, 255};
}

// D2: Combo 指示器 (连击数显示在头顶)
static void _draw_combo_indicator(int count, float timer, float hx, float hy,
                                  float hw) {
    if (count < 2 || timer <= 0) return;
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", count);
    int fsize = count >= 4 ? 20 : 14;
    Color cc = count >= 4 ? Color{255, 200, 30, 255}
                          : Color{255, 255, 220, 200};
    DrawText(buf, (int)(hx + hw/2 - 4), (int)(hy - 18), fsize, cc);
}

void Player::draw_no_cam(float cam_x, float cam_y, const GameMap* view_map) {
    Rectangle dr = entity.draw_rect(cam_x, cam_y);

    // G10.6-B C1: 朝墙姿态衰减 — 攻击照常, 仅限制视觉进入墙体
    // 朝墙时: 前倾 x0.3, 重击缩放 1.25 -> 1.08 (峰值 13px 嵌入 -> ~3px)
    Vector2 center = { dr.x + dr.width / 2, dr.y + dr.height / 2 };
    bool wall_ahead = _facing_tile_blocked(view_map, center, direction);

    // D2: 重击时身体变大
    float heavy_scale = combo.is_heavy() ? 1.25f : 1.0f;
    if (wall_ahead && heavy_scale > 1.0f) heavy_scale = 1.08f;
    Vector2 sq = dodge.squash_scale();   // B3: 翻滚压扁 (静止时恒 1,1 零影响)
    float hw = dr.width * heavy_scale * sq.x, hh = dr.height * heavy_scale * sq.y;
    float hx = dr.x - (hw - dr.width) / 2, hy = dr.y - (hh - dr.height) / 2;

    // G9.4: 攻击前倾位移 (p=挥砍进度) — 朝墙时衰减至 30%
    float p = 0.0f;
    if (_swing_start >= 0.0f)
        p = std::min(1.0f, (float)(GetTime() - _swing_start) / 0.35f);
    Rectangle pose = _attack_pose_rect(direction, {hx, hy, hw, hh}, p,
                                       combo.is_heavy());
    if (wall_ahead) {
        pose.x = hx + (pose.x - hx) * 0.3f;
        pose.y = hy + (pose.y - hy) * 0.3f;
    }
    hx = pose.x;
    hy = pose.y;

    // 阴影
    DrawEllipse(hx + hw/2, hy + hh + 2, hw/2 - 2, 3, {0, 0, 0, 100});

    // M4f.4: 数据驱动素材精灵 (元素形象), 未配置回退程序化占位
    SpriteDef fdef;
    Texture2D ftex = ResourceManager::inst().sprite_by_key(
        _player_sprite_key(element.type), fdef);
    if (ftex.id > 0) {
        // G9.4: 重击绕脚底回弹旋转  ·  B3: 翻滚倾斜叠加 (同一脚底 origin 路径)
        float rot = (combo.is_heavy() && p > 0.0f)
            ? 6.0f * sinf(p * 6.2831853f) : 0.0f;
        rot += dodge.tilt_deg();
        Rectangle src = SpriteRenderer::frame_rect(fdef, 0);
        // raylib: 绘制位置 = dest - origin, 旋转绕 dest 进行。
        // 平时 origin 归零 → 精灵贴满碰撞盒; 重击旋转时 dest 补偿 origin → 绕脚底旋转
        Vector2 origin = {hw / 2, hh};
        Rectangle dst = {hx + origin.x, hy + origin.y, hw, hh};
        if (rot == 0.0f) { dst.x -= origin.x; dst.y -= origin.y; origin = {0, 0}; }
        DrawTexturePro(ftex, src, dst, origin, rot, WHITE);
        if (rot == 0.0f) _draw_facing_eyes(hx, hy, hw, hh, direction);
        // B3: 残影 — 起翻时记录的 rect 左上偏移回贴同贴图, alpha 随龄衰减
        for (const auto& g : dodge.ghosts()) {
            float ga = 120.0f * (1.0f - g.age / DodgeComponent::kGhostLife);
            if (ga <= 0.0f) continue;
            Rectangle gd = {hx + (g.pos.x - entity.position.x),
                            hy + (g.pos.y - entity.position.y), hw, hh};
            DrawTexturePro(ftex, src, gd, {0, 0}, 0.0f,
                           {255, 255, 255, (unsigned char)ga});
        }
    } else {
        _draw_procedural_player(hx, hy, hw, hh,
                                _combo_body_color(combo.count), direction);
    }

    // G9.4: 装备武器握持效果 (徒手/无素材时静默)
    _draw_player_weapon(this, direction, hx, hy, hw, hh, wall_ahead);

    // D2: Combo 指示器 (连击数显示在头顶)
    _draw_combo_indicator(combo.count, combo.timer, hx, hy, hw);
}

// ============================================================
// Batch 3A: 经济 API
// ============================================================
void Player::add_gold(int amount) {
    if (amount <= 0) return;
    gold += amount;
}

bool Player::spend_gold(int amount) {
    if (amount <= 0 || gold < amount) return false;
    gold -= amount;
    return true;
}

int Player::get_gold() const { return gold; }

void Player::add_key(int amount) {
    if (amount <= 0) return;
    key_count += amount;
}

bool Player::spend_key(int amount) {
    if (amount <= 0 || key_count < amount) return false;
    key_count -= amount;
    return true;
}

int Player::get_key_count() const { return key_count; }

// 统一圣物入口
// G15: 防重 —— 原 apply-once 靠 RelicEffectProcessor::_passive_applied 跟踪,
// 但该表从未接线(见 relic_effect_processor.h 头部说明), 重复获得会二次叠加
// 被动 stat, 且 remove_relic 按 id 全量 erase 只会减一次造成 stat 泄漏。
void Player::add_relic(const std::string& id, PersistenceScope scope) {
    for (const auto& r : relics)
        if (r.id == id) return;
    relics.push_back({id, scope});
    RelicEffectProcessor::apply_passive_for_relic(this, id);
}

void Player::remove_relic(const std::string& id) {
    auto it = std::find_if(
        relics.begin(), relics.end(),
        [&](const RelicInstance& r) { return r.id == id; });
    if (it == relics.end()) return;   // 幂等: 不得扣减未持有的圣物被动
    RelicEffectProcessor::remove_passive_for_relic(this, id);
    relics.erase(
        std::remove_if(relics.begin(), relics.end(),
            [&](const RelicInstance& r) { return r.id == id; }),
        relics.end());
}
