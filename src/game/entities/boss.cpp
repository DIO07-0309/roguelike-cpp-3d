#include "boss.h"
#include "monster.h"
#include "player.h"
#include "game_map.h"
#include "systems/collision_utils.h"
#include "combat_system.h"
#include "vfx_server.h"
#include "config.h"
#include "growth_curve.h"
#include "resources/resource_manager.h"   // M5-C: visual_id 精灵探测
#include "core/logger.h"
#include "data/boss_defs.h"      // G1 Step6
#include <cmath>
#include <cstring>

// ---- BossSkill 基类 ----
BossSkill::BossSkill(const std::string& n, float cd) : name(n), cooldown(cd) {}
bool BossSkill::can_use(double t) const { return (t - last_use_time) >= cooldown; }
void BossSkill::mark_used(double t) { last_use_time = t; }

// ============================================================
// B15: ChargeSkill — 锁定玩家, 蓄力→冲刺
// ============================================================
ChargeSkill::ChargeSkill() : BossSkill("冲锋", 6.0f) {
    fx_kind = "cone"; fx_radius = 120; fx_color = {255, 80, 40, 255};
}

std::string ChargeSkill::execute(Monster* boss, Player* player,
                                 std::vector<Monster*>&, GameMap* map, double gt) {
    if (windup_left > 0) {
        // 蓄力中：身体变红预警
        boss->color = Color{255, 60, 40, 255};
        return "";
    }
    if (dash_duration > 0) {
        // 冲刺中
        float speed = 500.0f * 0.016f;  // per-frame-ish step
        for (int step = 0; step < 4; step++) {
            boss->entity.position.x += dash_dx * speed;
            boss->entity.position.y += dash_dy * speed;
            boss->entity.sync_rect();
            // 撞墙停止
            if (!map->is_rect_walkable(boss->entity.rect)) {
                boss->entity.position.x -= dash_dx * speed;
                boss->entity.position.y -= dash_dy * speed;
                boss->entity.sync_rect();
                dash_duration = 0;
                break;
            }
            // 撞到玩家
            Rectangle br = boss->entity.rect;
            Rectangle pr = player->entity.rect;
            if (CheckCollisionRecs(br, pr)) {
                int dmg = calculate_damage(
                    (int)(boss->combat.get_effective_attack() * damage_mult),
                    player->combat.get_effective_defense(AttackType::PHYSICAL));
                player->combat.take_damage(dmg);
                dash_duration = 0;
                mark_used(gt);
                boss->color = Color{200, 40, 40, 255};
                return "Boss 冲锋命中！造成 " + std::to_string(dmg) + " 伤害";
            }
        }
        return "";
    }
    return "";
}

// ============================================================
// B15: ShockwaveSkill — 蓄力→周围AOE
// ============================================================
ShockwaveSkill::ShockwaveSkill() : BossSkill("冲击波", 8.0f) {
    fx_kind = "circle"; fx_radius = 100; fx_color = {255, 180, 30, 255};
}

std::string ShockwaveSkill::execute(Monster* boss, Player* player,
                                    std::vector<Monster*>&, GameMap*, double gt) {
    if (windup_left > 0) {
        // 蓄力中：闪烁
        float flicker = sinf((float)GetTime() * 20);
        boss->color = Color{255, 200, (unsigned char)(60 + (int)(flicker * 60)), 255};
        return "";
    }
    // 释放
    float dx = (player->entity.rect.x + player->entity.rect.width/2)
             - (boss->entity.rect.x + boss->entity.rect.width/2);
    float dy = (player->entity.rect.y + player->entity.rect.height/2)
             - (boss->entity.rect.y + boss->entity.rect.height/2);
    float dist = sqrtf(dx*dx + dy*dy);
    mark_used(gt);
    boss->color = Color{200, 40, 40, 255};
    if (dist > fx_radius) return "Boss 释放冲击波，但你躲开了";
    int dmg = calculate_damage(
        (int)(boss->combat.get_effective_attack() * damage_mult),
        player->combat.get_effective_defense(AttackType::MAGICAL),
        AttackType::MAGICAL);
    player->combat.take_damage(dmg);
    return "Boss 冲击波！造成 " + std::to_string(dmg) + " 伤害";
}

// ============================================================
// SummonMinions (B7 保留, B15 加入固定循环)
// ============================================================
SummonMinions::SummonMinions() : BossSkill("召唤手下", 12.0f) {
    fx_kind = "circle"; fx_radius = 80; fx_color = {100, 200, 100, 255};
}

std::string SummonMinions::execute(Monster* boss, Player*,
                                    std::vector<Monster*>& monsters,
                                    GameMap* map, double gt) {
    int count = 0;
    for (int i = 0; i < 3; i++) {
        int off_x = (int)(rng() % 7) - 3;
        int off_y = (int)(rng() % 7) - 3;
        float sx = boss->entity.position.x + off_x * TILE_SIZE;
        float sy = boss->entity.position.y + off_y * TILE_SIZE;
        auto [tx, ty] = map->pixel_to_tile(sx, sy);
        if (map->is_walkable(tx, ty)) {
            const char* type = (rng() % 3 == 0) ? "orc" : "slime";
            auto* m = spawn_monster(sx, sy, type);
            if (m) { monsters.push_back(m); count++; }
        }
    }
    mark_used(gt);
    if (count > 0)
        return "Boss 召唤了 " + std::to_string(count) + " 只手下！";
    return "Boss 召唤失败（无空间）";
}

// ═══════════════════════════════════════════════════════════════
// G5.4: WhirlwindSkill — 360°旋转斩 (Shadow Knight Phase2)
// ═══════════════════════════════════════════════════════════════
WhirlwindSkill::WhirlwindSkill() : BossSkill("旋风斩", 10.0f) {
    fx_kind = "circle"; fx_radius = 140; fx_color = {180, 20, 200, 255};
}
std::string WhirlwindSkill::execute(Monster* boss, Player* player,
                                    std::vector<Monster*>&, GameMap*, double gt) {
    if (spin_duration <= 0) return "";
    // M4a-fix: 旋风斩仅近身命中 (原为全图必中 — 玩家逃离仍掉血)
    float bx = boss->entity.rect.x + boss->entity.rect.width/2;
    float by = boss->entity.rect.y + boss->entity.rect.height/2;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    float dist = sqrtf((px - bx) * (px - bx) + (py - by) * (py - by));
    if (dist <= fx_radius && gt - last_hit_time >= 0.5) {
        last_hit_time = (float)gt;
        int dmg = calculate_damage((int)(boss->combat.get_effective_attack() * 1.6),
            player->combat.get_effective_defense(AttackType::PHYSICAL));
        player->combat.take_damage(dmg);
        player->combat.mark_damage_logged();
        spin_hit_count++;
        player->combat.last_damage_source = boss->name + ":旋风斩";   // M2-B
        LOG_INFO("[DMG] 旋风斩命中玩家 造成 %d 伤害", dmg);
    }
    if (spin_duration <= 0.3f) {
        mark_used(gt);
        return "旋风斩结束! " + std::to_string(spin_hit_count) + " hits";
    }
    return "";
}

// M4a-fx: 旋风斩命中范围可视化 (蓄力白环 → 旋转紫圈)
void WhirlwindSkill::draw(Monster* boss, float cam_x, float cam_y) const {
    if (windup_left <= 0 && spin_duration <= 0) return;
    float bx = boss->entity.rect.x + boss->entity.rect.width/2 - cam_x;
    float by = boss->entity.rect.y + boss->entity.rect.height/2 - cam_y;
    if (windup_left > 0) {
        DrawRing({bx, by}, fx_radius - 6, fx_radius, 0, 360, 36,
                 {240, 195, 255, 200});
        DrawCircleLines(bx, by, fx_radius, {255, 230, 255, 120});
    } else {
        float pulse = 0.7f + 0.3f * sinf((float)GetTime() * 14.0f);
        DrawRing({bx, by}, fx_radius - 6, fx_radius, 0, 360, 36,
                 {190, 40, 220, (unsigned char)(120 * pulse)});
        DrawRing({bx, by}, fx_radius - 2, fx_radius, 0, 360, 36,
                 {230, 130, 255, (unsigned char)(90 * pulse)});
    }
}

// G5.4: LaserBarrageSkill — 3-way 远程贯穿弹 (Fire Demon Phase2)
LaserBarrageSkill::LaserBarrageSkill() : BossSkill("炼狱激光", 9.0f) {
    fx_kind = "cone"; fx_radius = 200; fx_color = {255, 100, 20, 255};
}
std::string LaserBarrageSkill::execute(Monster* boss, Player* player,
                                       std::vector<Monster*>&, GameMap*, double gt) {
    if (windup_left > 0) { return ""; } // 蓄力中
    float bx = boss->entity.rect.x + boss->entity.rect.width/2;
    float by = boss->entity.rect.y + boss->entity.rect.height/2;
    float dx = player->entity.rect.x + player->entity.rect.width/2 - bx;
    float dy = player->entity.rect.y + player->entity.rect.height/2 - by;
    float base_ang = atan2f(dy, dx);
    int total = 0;
    for (int i = -1; i <= 1; i++) {
        float ang = base_ang + (float)i * 0.25f; // ±15° spread
        float lx = cosf(ang), ly = sinf(ang);
        float px = player->entity.rect.x + player->entity.rect.width/2;
        float py = player->entity.rect.y + player->entity.rect.height/2;
        float dot = (px-bx)*lx + (py-by)*ly;
        float perp = fabsf((px-bx)*(-ly) + (py-by)*lx);
        if (perp < 60.0f && dot > 0 && dot < 300.0f) {
            int dmg = calculate_damage((int)(boss->combat.get_effective_attack() * 1.8),
                player->combat.get_effective_defense(AttackType::MAGICAL), AttackType::MAGICAL);
            player->combat.take_damage(dmg); total += dmg;
        }
    }
    mark_used(gt);
    return total > 0 ? "激光弹幕 造成 " + std::to_string(total) + " 伤害" : "激光未命中";
}

// ═══════════════════════════════════════════════════════════════
// M4a: BarrageSkill — 蓄力后扇形弹幕, 命中减速
// ═══════════════════════════════════════════════════════════════
BarrageSkill::BarrageSkill() : BossSkill("弹幕", 7.0f) {
    fx_kind = "cone"; fx_radius = 200; fx_color = {150, 80, 255, 255};
}
std::string BarrageSkill::execute(Monster* boss, Player* player,
                                  std::vector<Monster*>&, GameMap* map, double gt) {
    if (windup_left > 0) {
        boss->color = Color{180, 60, 60, 255};
        return "";
    }
    float dt = 0.016f;
    if (!fired) {
        fired = true;
        _last_tick_time = gt;
    } else {
        dt = (float)(gt - _last_tick_time);
        _last_tick_time = gt;
        if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;  // 防抖兜底
    }
    std::string first_msg;
    if (_wave_fired < waves) {
        _wave_timer -= dt;
        if (_wave_fired == 0 || _wave_timer <= 0.0f) {
            _fire_wave(boss, player);
            _wave_fired++;
            _wave_timer = wave_interval;
            if (_wave_fired == 1) first_msg = "地狱火魔释放弹幕！";
        }
    }
    for (auto& s : shots) { s.x += s.vx * dt; s.y += s.vy * dt; s.life -= dt; }
    for (auto it = shots.begin(); it != shots.end();) {
        bool dead = it->life <= 0.0f;
        // M4a-fix: 弹丸撞墙消失 (原为穿墙 770px 追击)
        if (!dead && map) {
            auto [tx, ty] = map->pixel_to_tile(it->x, it->y);
            if (!map->is_walkable(tx, ty)) dead = true;
        }
        if (!dead && CheckCollisionCircleRec({it->x, it->y}, 8.0f, player->entity.rect)) {
            int dmg = calculate_damage(
                (int)(boss->combat.get_effective_attack() * damage_mult),
                player->combat.get_effective_defense(AttackType::PHYSICAL));
            player->combat.take_damage(dmg);
            player->combat.mark_damage_logged();
            apply_buff(player, "slow", 1);
            hit_fx.push_back({it->x, it->y});   // M4a-fx: 记录命中点
            player->combat.last_damage_source = boss->name + ":弹幕";   // M2-B
            LOG_INFO("[DMG] 弹幕命中玩家 造成 %d 伤害", dmg);
            dead = true;
        }
        if (dead) it = shots.erase(it); else ++it;
    }
    if (_wave_fired >= waves && shots.empty()) { finished = true; mark_used(gt); }
    return first_msg;
}

void BarrageSkill::_fire_wave(Monster* boss, Player* player) {
    float bx = boss->entity.rect.x + boss->entity.rect.width/2;
    float by = boss->entity.rect.y + boss->entity.rect.height/2;
    float tx = player->entity.rect.x + player->entity.rect.width/2;
    float ty = player->entity.rect.y + player->entity.rect.height/2;
    const float pi = 3.14159f;
    if (pattern == 1) {  // 环形 360° 等分
        float step = 2.0f * pi / (float)shot_count;
        for (int i = 0; i < shot_count; i++) {
            float ang = step * (float)i;
            shots.push_back({bx, by, cosf(ang) * speed, sinf(ang) * speed, 3.5f});
        }
        return;
    }
    float base = atan2f(ty - by, tx - bx);
    if (pattern == 2)  // 螺旋: 每波整环偏转
        base += (float)_wave_fired * spiral_turn_deg * pi / 180.0f;
    float total = spread_deg * pi / 180.0f;
    float half = total / 2.0f;
    float step = (shot_count > 1) ? total / (float)(shot_count - 1) : 0.0f;
    for (int i = 0; i < shot_count; i++) {
        float ang = base - half + step * (float)i;
        shots.push_back({bx, by, cosf(ang) * speed, sinf(ang) * speed, 3.5f});
    }
}

void BarrageSkill::draw(float cam_x, float cam_y) const {
    for (auto& s : shots) {
        float sx = s.x - cam_x, sy = s.y - cam_y;
        if (sx < -40 || sx > 1400 || sy < -40 || sy > 900) continue;
        // M4a-fx: 彗星拖尾 (沿速度方向 3 段递减半透明)
        float vlen = sqrtf(s.vx * s.vx + s.vy * s.vy);
        float nvx = (vlen > 1.0f) ? s.vx / vlen : 0.0f;
        float nvy = (vlen > 1.0f) ? s.vy / vlen : 0.0f;
        float trail[3] = {12.0f, 24.0f, 38.0f};
        for (int i = 0; i < 3; i++) {
            float tx = sx - nvx * trail[i], ty = sy - nvy * trail[i];
            DrawCircle(tx, ty, 7.0f - i * 1.6f,
                       {150, 80, 255, (unsigned char)(95 - i * 28)});
        }
        DrawCircle(sx, sy, 7.0f, {150, 80, 255, 220});
        DrawCircle(sx, sy, 3.5f, {220, 180, 255, 255});
    }
}

// ═══════════════════════════════════════════════════════════════
// M4a: ConeAttackSkill — 蓄力后扇形斩, 命中中毒 2s
// ═══════════════════════════════════════════════════════════════
ConeAttackSkill::ConeAttackSkill() : BossSkill("扇形斩", 6.0f) {
    fx_kind = "cone"; fx_radius = 96; fx_color = {140, 240, 80, 255};
}
std::string ConeAttackSkill::execute(Monster* boss, Player* player,
                                     std::vector<Monster*>&, GameMap*, double gt) {
    if (windup_left > 0) {
        boss->color = Color{160, 220, 90, 255};
        return "";
    }
    float bx = boss->entity.rect.x + boss->entity.rect.width/2;
    float by = boss->entity.rect.y + boss->entity.rect.height/2;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    float dx = px - bx, dy = py - by;
    float dist = sqrtf(dx * dx + dy * dy);
    mark_used(gt);
    boss->color = Color{200, 40, 40, 255};
    if (dist > reach) return "扇形斩落空";
    int dmg = calculate_damage(
        (int)(boss->combat.get_effective_attack() * damage_mult),
        player->combat.get_effective_defense(AttackType::PHYSICAL));
    player->combat.take_damage(dmg);
    player->combat.mark_damage_logged();
    apply_buff(player, "poison2s", 1);
    player->combat.last_damage_source = boss->name + ":扇形斩";   // M2-B
    LOG_INFO("[DMG] 扇形斩命中玩家 造成 %d 伤害 (中毒)", dmg);
    return "扇形斩命中！中毒 2 秒";
}

void ConeAttackSkill::draw(Monster* boss, Player* player,
                           float cam_x, float cam_y) const {
    if (windup_left <= 0) return;
    float bx = boss->entity.rect.x + boss->entity.rect.width/2 - cam_x;
    float by = boss->entity.rect.y + boss->entity.rect.height/2 - cam_y;
    float px = player->entity.rect.x + player->entity.rect.width/2 - cam_x;
    float py = player->entity.rect.y + player->entity.rect.height/2 - cam_y;
    float angle = atan2f(py - by, px - bx);
    float half = half_angle * 3.14159f / 180.0f;
    DrawCircleSector({bx, by}, reach,
        (angle - half) * 57.2958f, (angle + half) * 57.2958f, 28,
        {140, 240, 80, 70});
    DrawCircleSector({bx, by}, reach * 0.4f,
        (angle - half) * 57.2958f, (angle + half) * 57.2958f, 20,
        {255, 255, 255, 30});
}

// ═══════════════════════════════════════════════════════════════
// M4a: BlinkSkill — 瞬移至玩家侧翼 (CD 独立计时)
// ═══════════════════════════════════════════════════════════════
BlinkSkill::BlinkSkill() : BossSkill("瞬移", 12.0f) {
    fx_kind = "circle"; fx_radius = 70; fx_color = {120, 80, 255, 255};
}
void BlinkSkill::plan_destination(Monster* boss, Player* player, GameMap* map) {
    float bx = boss->entity.rect.x + boss->entity.rect.width/2;
    float by = boss->entity.rect.y + boss->entity.rect.height/2;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    float dx = px - bx, dy = py - by;
    float len = sqrtf(dx * dx + dy * dy);
    bool placed = false;
    if (len > 1.0f) {
        float nx = -dy / len, ny = dx / len;
        for (int side = 0; side < 2 && !placed; side++) {
            float s = (side == 0) ? 1.0f : -1.0f;
            float tx = px + nx * s * blink_dist;
            float ty = py + ny * s * blink_dist;
            auto [tile_x, tile_y] = map->pixel_to_tile(tx, ty);
            if (map->is_walkable(tile_x, tile_y)) {
                pending_x = tx; pending_y = ty;
                placed = true;
            }
        }
    }
    if (!placed) { pending_x = px; pending_y = py; }
}
std::string BlinkSkill::execute(Monster* boss, Player* player,
                                std::vector<Monster*>&, GameMap*, double gt) {
    if (windup_left > 0) {
        boss->color = Color{170, 110, 255, 255};
        return "";
    }
    if (blinked) return "";
    boss->entity.position = {pending_x, pending_y};
    boss->entity.sync_rect();
    blinked = true;
    mark_used(gt);
    return "暗影骑士瞬移了！";
}
void BlinkSkill::draw(float cam_x, float cam_y) const {
    if (windup_left <= 0) return;
    float sx = pending_x - cam_x, sy = pending_y - cam_y;
    DrawRing({sx, sy}, 16, 26, 0, 360, 20, {150, 100, 255, 200});
    DrawRing({sx, sy}, 26, 32, 0, 360, 20, {150, 100, 255, 110});
    DrawCircle(sx, sy, 4.0f, {220, 190, 255, 255});
}

// ============================================================
// BossAI — B15: 技能循环状态机 + Phase 2
// ============================================================
BossAI::BossAI() : MonsterAI(10.0f, 60.0f, 3.0f, 2.0f) {
    _charge    = std::make_unique<ChargeSkill>();
    _shockwave = std::make_unique<ShockwaveSkill>();
    _summon    = std::make_unique<SummonMinions>();
    _whirlwind = std::make_unique<WhirlwindSkill>();  // G5.4
    _laser     = std::make_unique<LaserBarrageSkill>();// G5.4
    _barrage   = std::make_unique<BarrageSkill>();    // M4a
    _cone      = std::make_unique<ConeAttackSkill>(); // M4a
    _blink     = std::make_unique<BlinkSkill>();      // M4a
}

int BossAI::_next_cycle_skill() {
    // D8: skill_cycle_bias defines pattern: 6=normal, 4=summon-heavy
    int cycle_len = skill_cycle_bias;
    int idx = skill_cycle_index % cycle_len;
    skill_cycle_index++;
    if (idx == 0) return 0;       // Charge
    if (idx == 2) return 1;       // Shockwave
    if (cycle_len == 4 && idx == 3) return 2; // Necromancer: Summon at idx 3 (every 2nd after norm)
    if (cycle_len == 6 && idx == 4) return 2; // Normal: Summon at idx 4
    return -1;                     // 普攻
}

// ============================================================
// M4a: 连招驱动 — BossSkillQueue 执行器
// ============================================================

static void _spawn_boss_vfx(Monster* self, const std::string& kind,
                             std::vector<Effect>* effects);  // 前向声明 (定义在下方)

BossCommand BossAI::_command_from_str(const std::string& s) {
    if (s == "normal")    return BossCommand::NORMAL;
    if (s == "charge")    return BossCommand::CHARGE;
    if (s == "shockwave") return BossCommand::SHOCKWAVE;
    if (s == "summon")    return BossCommand::SUMMON;
    if (s == "defend")    return BossCommand::DEFEND;
    if (s == "barrage")   return BossCommand::RANGED;
    if (s == "cone")      return BossCommand::CONE;
    if (s == "blink")     return BossCommand::BLINK;
    if (s == "whirlwind") return BossCommand::WHIRLWIND;
    return BossCommand::NONE;
}

void BossAI::_select_combo() {
    if (!_combos || _combos->empty()) return;
    const ComboDef* target = nullptr;
    // M4b: 遭遇阶段驱动模板 — CONTROL→press, LAST_STAND→rage, 其余 probe/rage(phase2)
    const char* want = nullptr;
    switch (_encounter_phase) {
        case EncounterPhase::CONTROL:    want = "press"; break;
        case EncounterPhase::LAST_STAND: want = "rage";  break;
        default: want = phase2 ? "rage" : "probe"; break;
    }
    for (auto& c : *_combos) if (want && c.id == want) { target = &c; break; }
    if (!target) for (auto& c : *_combos)
        if ((phase2 && c.id == "rage") || (!phase2 && c.id == "probe")) { target = &c; break; }
    if (!target) target = &(*_combos)[0];
    _combo_id = target->id;
    _combo_current_end_delay = target->end_delay;
    _combo_queue.clear();
    for (auto& cmd : target->commands)
        _combo_queue.enqueue(_command_from_str(cmd));
    _combo_queue.start();
    _combo_timer = 0.0f;
    normal_attack_count = 0;
    LOG_INFO("[COMBO] BOSS 启动连招「%s」(%d 段) phase=%d", target->id.c_str(),
             (int)target->commands.size(), (int)_encounter_phase);
}

void BossAI::_combo_advance() {
    _combo_queue.advance();
    if (_combo_queue.active) {
        _combo_timer = 0.6f;
        if (_combos) for (auto& c : *_combos)
            if (c.id == _combo_id) { _combo_timer = c.interval; break; }
    } else {
        _combo_end_delay = _combo_current_end_delay;
    }
    normal_attack_count = 0;
}

void BossAI::_combo_on_skill_end() {
    if (_combo_queue.active) _combo_advance();
    else normal_attack_count = 0;
}

bool BossAI::_tick_combo_attack(Monster* self, Player* player, GameMap* map,
                                double dt, double gt, std::vector<Effect>* effects) {
    if (_combo_queue.active) {
        if (_combo_timer > 0) { _combo_timer -= (float)dt; return true; }
        _run_combo_command(_combo_queue.current_cmd(), self, player, map, gt, effects);
        return true;
    }
    if (_combo_end_delay > 0) { _combo_end_delay -= (float)dt; return true; }
    if (normal_attack_count >= 2) { _select_combo(); return true; }
    return false;
}

// 收官: 连招命令中技能冷却 → 该步退化为普攻 (保持连招节奏不空转)
void BossAI::_combo_fallback_melee(Monster* self, Player* player, double gt,
                                   std::vector<Effect>* effects) {
    if (self->can_attack(gt)) {
        self->attack_target(player, gt);
        _spawn_boss_vfx(self, "charge", effects);
    }
    _combo_advance();
}

void BossAI::_run_combo_command(BossCommand cmd, Monster* self, Player* player,
                                GameMap* map, double gt, std::vector<Effect>* effects) {
    switch (cmd) {
    case BossCommand::NORMAL:
        if (self->can_attack(gt)) {
            self->attack_target(player, gt);
            _spawn_boss_vfx(self, "charge", effects);
        }
        _combo_advance();
        break;
    case BossCommand::CHARGE:
        // 收官: 冷却判定 — 冷却中退普攻
        if (!_charge->can_use(gt)) { _combo_fallback_melee(self, player, gt, effects); break; }
        boss_state = BossState::CHARGE;
        _charge->windup_left = _charge->windup_time;
        _charge->dash_duration = 0.0f;
        _spawn_boss_vfx(self, "charge", effects);
        break;
    case BossCommand::SHOCKWAVE:
        if (!_shockwave->can_use(gt)) { _combo_fallback_melee(self, player, gt, effects); break; }
        boss_state = BossState::SHOCKWAVE;
        _shockwave->windup_left = _shockwave->windup_time;
        _spawn_boss_vfx(self, "shockwave", effects);
        break;
    case BossCommand::SUMMON:
        if (!_summon->can_use(gt)) { _combo_fallback_melee(self, player, gt, effects); break; }
        boss_state = BossState::SUMMON;
        _spawn_boss_vfx(self, "summon", effects);
        break;
    case BossCommand::DEFEND:
        boss_state = BossState::DEFEND;
        _spawn_boss_vfx(self, "shockwave", effects);
        break;
    case BossCommand::RANGED:
        if (!_barrage->can_use(gt)) { _combo_fallback_melee(self, player, gt, effects); break; }
        boss_state = BossState::RANGED_BARRAGE;
        _barrage->windup_left = _barrage->windup_time;
        _barrage->shots.clear();
        _barrage->fired = false;
        _barrage->finished = false;
        _barrage->reset_waves();      // M4b: 波次复位
        _spawn_boss_vfx(self, "charge", effects);
        break;
    case BossCommand::CONE:
        if (!_cone->can_use(gt)) { _combo_fallback_melee(self, player, gt, effects); break; }
        boss_state = BossState::CONE_ATTACK;
        _cone->windup_left = _cone->windup_time;
        _spawn_boss_vfx(self, "shockwave", effects);
        break;
    case BossCommand::BLINK:
        if (!_blink->can_use(gt)) { _combo_fallback_melee(self, player, gt, effects); break; }
        boss_state = BossState::BLINK;
        _blink->windup_left = _blink->windup_time;
        _blink->blinked = false;
        _blink->plan_destination(self, player, map);
        _spawn_boss_vfx(self, "summon", effects);
        break;
    case BossCommand::WHIRLWIND:
        if (!_whirlwind->can_use(gt)) { _combo_fallback_melee(self, player, gt, effects); break; }
        boss_state = BossState::WHIRLWIND;
        _whirlwind->spin_duration = 0.0f;
        _spawn_boss_vfx(self, "charge", effects);
        break;
    default:
        _combo_advance();
        break;
    }
}

void BossAI::update(Monster* self, Player* player, GameMap* map,
                     double dt, double gt,
                     std::vector<Monster*>* all, std::vector<Effect>* effects,
                     int monster_room, int player_room,
                     const RoomManager* room_mgr) {
    (void)monster_room; (void)player_room; (void)room_mgr;
    if (!self->combat.is_alive) return;

    // B15: Phase 2 检测 (仅触发一次, HP < 阈值来自 BossDef)
    if (!phase2 && _hp_ratio(self) < _phase2_hp_threshold) {
        _enter_phase2(self, effects);
        return; // 本帧暂停
    }

    // Phase 2 暂停计时
    if (phase2_pause > 0) {
        phase2_pause -= (float)dt;
        return; // 暂停中
    }

    // G5.5: 二阶段持续行为 (狂暴脉冲 + 背水一战) — 阶段变化增强
    if (phase2)
        _tick_phase2_behaviors(self, player, map, dt, effects);

    // ── B15: Boss 状态机 ──
    _tick_boss_state(self, player, map, dt, gt, all, effects);
}

// G5.5: 二阶段持续行为 — 背水一战(一次性) + 周期性狂暴脉冲
void BossAI::_tick_phase2_behaviors(Monster* self, Player* player, GameMap* map,
                                    double dt, std::vector<Effect>* effects) {
    _phase2_elapsed += (float)dt;
    // 背水一战: HP<25% 一次性终极强化 (当帧只演出, 不进状态机)
    if (!_last_stand && _hp_ratio(self) < 0.25f) {
        _enter_last_stand(self, effects);
        return;
    }
    // 狂暴脉冲: 每 6s 一次 — 近身玩家击退 + 真实伤害, 压迫近战位
    // 阶段升级: 二阶段越久脉冲越密 (20s 后由 6s 收敛到 4s 下限)
    _rage_pulse_cd -= (float)dt;
    if (_rage_pulse_cd > 0.0f) return;
    float pulse_cd = 6.0f - _phase2_elapsed * 0.1f;
    if (pulse_cd < 4.0f) pulse_cd = 4.0f;
    _rage_pulse_cd = pulse_cd;

    float bx = self->entity.rect.x + self->entity.rect.width/2;
    float by = self->entity.rect.y + self->entity.rect.height/2;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    float dx = px - bx, dy = py - by;
    float dist = sqrtf(dx*dx + dy*dy);
    if (effects && dist < 220.0f) {
        VFXServer v;
        v.ring(bx, by, 96.0f, {220, 60, 220, 200}, 2, 0.50f);
        for (auto& e : v.effects) effects->push_back(e);
    }
    if (dist >= 96.0f || dist < 1.0f) return;
    clamp_displacement(player->entity, -dx/dist * 30.0f, -dy/dist * 30.0f, map);
    int dmg = calculate_damage((int)(self->combat.get_effective_attack() * 0.5f),
                               player->combat.get_effective_defense(AttackType::TRUE),
                               AttackType::TRUE);
    player->combat.take_damage(dmg);
}

// G5.5: 背水一战 — HP<25% 一次性强化 (攻速/移速/攻击全面提升, 红色演出)
void BossAI::_enter_last_stand(Monster* self, std::vector<Effect>* effects) {
    _last_stand = true;
    move_speed *= 1.20f;
    self->attack_cooldown *= 0.70f;
    self->combat.attack = (int)(self->combat.attack * 1.15f);
    self->entity.size = {56, 56};  // visually bigger
    self->entity.sync_rect();
    LOG_INFO("[BOSS] 背水一战! 攻速+43%% 移速+20%% 攻击+15%%");
    if (effects) {
        VFXServer v;
        float mx = self->entity.rect.x + self->entity.rect.width/2;
        float my = self->entity.rect.y + self->entity.rect.height/2;
        v.boss_phase2_flash(mx, my, {255, 60, 40, 255});
        v.ring(mx, my, 128, {255, 80, 40, 255}, 3, 0.8f);
        for (auto& e : v.effects) effects->push_back(e);
    }
}

void BossAI::_enter_phase2(Monster* self, std::vector<Effect>* effects) {
    phase2 = true;
    phase2_pause = _phase2_pause;                       // G1 Step6: from BossDef
    _rage_pulse_cd = 3.0f;                              // G5.5: 转场演出后再首发脉冲
    is_enraged = true;
    move_speed *= _phase2_speed_mult;                   // G1 Step6: from BossDef
    self->attack_cooldown *= _phase2_cd_mult;           // G1 Step6: from BossDef
    // M4d: Phase2 ATK boost capped — base is already scaled by boss_atk_scale
    float clamped_mul = _phase2_atk_mult > 1.2f ? 1.2f : _phase2_atk_mult;
    self->combat.attack = (int)(self->combat.attack * clamped_mul);
    self->entity.size = {52, 52};  // visually bigger
    self->entity.sync_rect();
    LOG_INFO("[BOSS] Phase 2 触发! 攻击+%.0f%% 移速+%.0f%%",
             (_phase2_atk_mult - 1.0f) * 100, (_phase2_speed_mult - 1.0f) * 100);
    // M4a-fx: 狂暴演出 (紫色闪光 + 扩散波纹)
    if (effects) {
        VFXServer v;
        float mx = self->entity.rect.x + self->entity.rect.width/2;
        float my = self->entity.rect.y + self->entity.rect.height/2;
        v.boss_phase2_flash(mx, my, {180, 60, 220, 255});
        v.ring(mx, my, 96, {170, 70, 230, 255}, 3, 0.6f);
        for (auto& e : v.effects) effects->push_back(e);
    }
}

static void _spawn_boss_vfx(Monster* self, const std::string& kind,
                             std::vector<Effect>* effects) {
    if (!effects) return;
    VFXServer v;
    float mx = self->entity.rect.x + self->entity.rect.width / 2;
    float my = self->entity.rect.y + self->entity.rect.height / 2;
    if (kind == "charge")       v.boss_cone(mx, my);
    else if (kind == "shockwave") v.boss_circle(mx, my);
    else if (kind == "summon")    v.boss_summon(mx, my);
    for (auto& e : v.effects) effects->push_back(e);
}

void BossAI::_tick_boss_state(Monster* self, Player* player, GameMap* map,
                               double dt, double gt,
                               std::vector<Monster*>* all, std::vector<Effect>* effects) {
    // Q3.13: static→局部 — SummonMinions push 的裸指针跨帧累积成悬垂 (泄漏+定时炸弹)
    std::vector<Monster*> empty_slots;
    switch (boss_state) {
    case BossState::IDLE: {
        // 基础 AI (追逐/巡逻)
        MonsterAI::update(self, player, map, dt, gt, all, effects, _monster_room, _player_room, _room_mgr);
        // M4a-fix: 视野内即进入战斗姿态 (原 attack_range 48px —
        // 玩家拉扯>48px 时连招/召唤/瞬移永不触发, BOSS 只会追)
        float dist = _dist_to(self, player);
        if (dist <= sight_range * 32.0f) {
            boss_state = BossState::ATTACK;
            normal_attack_count = 0;
            skill_cycle_index = 0;
        }
        break;
    }
    case BossState::ATTACK: {
        float dist = _dist_to(self, player);
        // M4a-fix: 脱战距离 384px (原 192px — 拉扯进出视野导致连招反复中断)
        if (dist > sight_range * 64.0f) {
            boss_state = BossState::IDLE;
            break;
        }
        // M4a: 连招驱动 (优先于普攻/旧循环; 返回 true = 连招占用本帧)
        if (_combos && !_combos->empty()) {
            if (_tick_combo_attack(self, player, map, dt, gt, effects)) break;
        }
        // F15 Mirror: 战斗委托给 MirrorCombatDirector, BossAI 只做移动
        if (_is_mirror) {
            normal_attack_count = 0;  // 不让旧循环触发
        } else {
            // 标准普攻
            if (self->can_attack(gt)) {
                self->attack_target(player, gt);
                _spawn_boss_vfx(self, "charge", effects);
                normal_attack_count++;
            }
        }
        // 普攻2次后 → 使用下个技能 (镜像跳过)
        if (!_is_mirror && normal_attack_count >= 2) {
            int sk = _next_cycle_skill();
            // 收官: 冷却判定 — 冷却中的技能退回普攻
            if (sk == 0 && !_charge->can_use(gt)) sk = -1;
            else if (sk == 1 && !_shockwave->can_use(gt)) sk = -1;
            else if (sk == 2 && !_summon->can_use(gt)) sk = -1;
            else if (sk == 4 && !_whirlwind->can_use(gt)) sk = -1;
            else if (sk == 5 && !_barrage->can_use(gt)) sk = -1;
            else if (sk == 6 && !_shockwave->can_use(gt)) sk = -1;
            // ── G5.4: Phase2 signature skill injection ──
            if (phase2 && _boss_id) {
                // Shadow Knight: every 3rd cycle → Whirlwind
                if (strcmp(_boss_id,"shadow_knight")==0 && (skill_cycle_index%9)<3) sk=4;
                // Fire Demon: every 3rd cycle → Laser Barrage
                else if (strcmp(_boss_id,"fire_demon")==0 && (skill_cycle_index%9)<3) sk=5;
                // Demon Lord: every 4th cycle → Gravity Pull (shockwave with 2x range)
                else if (strcmp(_boss_id,"demon_lord")==0 && (skill_cycle_index%12)<3) sk=6;
                // Vampire: every cycle → faster charge+shockwave (lifesteal on hit)
                else if (strcmp(_boss_id,"vampire")==0) {
                    self->attack_cooldown *= 0.85f;
                    if (sk==0) _charge->windup_left *= 0.7f;
                }
                // Necromancer: summoned adds get Guardian buff in Phase2
                else if (strcmp(_boss_id,"necromancer")==0 && sk==2 && all) {
                    for (auto* m : *all) if (m && !m->is_boss && m->combat.is_alive)
                        apply_buff(m,"defense_up",1);
                }
                // Golem: Phase2 → consecutive shockwaves (3 waves)
                else if (strcmp(_boss_id,"golem")==0 && sk==1) {
                    _shockwave->windup_left = _shockwave->windup_time;
                    // spawn 2 more on next cycle via state
                }
            }
            // D8: Golem — every other cycle, use DEFEND instead of Shockwave
            if (golem_shield_pct > 0 && sk == 1 && (skill_cycle_index % 12) < 6) {
                sk = 3; // override: DEFEND instead of Shockwave
            }
            if (sk == 0) {
                boss_state = BossState::CHARGE;
                _charge->windup_left = _charge->windup_time;
                _charge->dash_duration = 0.0f;
                _spawn_boss_vfx(self, "charge", effects);
            } else if (sk == 1) {
                boss_state = BossState::SHOCKWAVE;
                _shockwave->windup_left = _shockwave->windup_time;
                _spawn_boss_vfx(self, "shockwave", effects);
            } else if (sk == 2) {
                boss_state = BossState::SUMMON;
                _spawn_boss_vfx(self, "summon", effects);
            } else if (sk == 3) {
                boss_state = BossState::DEFEND;
                _spawn_boss_vfx(self, "shockwave", effects); // reuse VFX
            } else if (sk == 4) {
                boss_state = BossState::WHIRLWIND;
                _spawn_boss_vfx(self, "charge", effects);
            } else if (sk == 5) {
                boss_state = BossState::LASER_BARRAGE;
                _laser->windup_left = 0.8f;
                _spawn_boss_vfx(self, "shockwave", effects);
            } else if (sk == 6) {
                boss_state = BossState::GRAVITY_PULL;
                _spawn_boss_vfx(self, "charge", effects);
            }
            normal_attack_count = 0;
        }
        // 仍然追向玩家
        if (dist > attack_range * 32.0f) {
            float dx = player->entity.rect.x + player->entity.rect.width/2
                     - self->entity.rect.x - self->entity.rect.width/2;
            float dy = player->entity.rect.y + player->entity.rect.height/2
                     - self->entity.rect.y - self->entity.rect.height/2;
            float len = sqrtf(dx*dx + dy*dy);
            if (len > 1) _apply_movement(self, map, dx/len, dy/len, dt);
        }
        break;
    }
    case BossState::CHARGE: {
        // 蓄力阶段 (C1: 红色预警圈)
        if (_charge->windup_left > 0) {
            _charge->windup_left -= (float)dt;
            if (effects) {
                // Pulsing ring at boss position
                Effect warn;
                warn.kind = "pulse"; warn.world_x = self->entity.rect.x + self->entity.rect.width/2;
                warn.world_y = self->entity.rect.y + self->entity.rect.height/2;
                warn.radius = 72; warn.duration = 0.35f; warn.elapsed = 0;
                warn.color = {255, 80, 30, 200};
                effects->push_back(warn);
                // M4d: Dash direction indicator line
                float ctx = self->entity.rect.x + self->entity.rect.width/2;
                float cty = self->entity.rect.y + self->entity.rect.height/2;
                float ptx = player->entity.rect.x + player->entity.rect.width/2;
                float pty = player->entity.rect.y + player->entity.rect.height/2;
                Effect line;
                line.kind = "bolt"; line.world_x = ctx; line.world_y = cty;
                line.target_x = ptx; line.target_y = pty;
                line.duration = 0.35f; line.elapsed = 0;
                line.color = {255, 60, 30, 200};
                effects->push_back(line);
            }
            { std::vector<Monster*> dummy; _charge->execute(self, player, dummy, map, gt); }
            if (_charge->windup_left <= 0) {
                // 蓄力结束 → 开始冲刺
                float dx = player->entity.rect.x + player->entity.rect.width/2
                         - self->entity.rect.x - self->entity.rect.width/2;
                float dy = player->entity.rect.y + player->entity.rect.height/2
                         - self->entity.rect.y - self->entity.rect.height/2;
                float len = sqrtf(dx*dx + dy*dy);
                if (len > 0) { _charge->dash_dx = dx/len; _charge->dash_dy = dy/len; }
                _charge->dash_duration = 0.3f;
            }
            break;
        }
        // 冲刺阶段
        if (_charge->dash_duration > 0) {
            _charge->dash_duration -= (float)dt;
            std::vector<Monster*> dm; (void)dm;
            std::string result = _charge->execute(self, player, dm, map, gt);
            if (!result.empty() && result.find("命中") != std::string::npos) {
                // 命中玩家, 恢复
                boss_state = BossState::ATTACK;
                _combo_on_skill_end();
            }
            if (_charge->dash_duration <= 0) {
                self->color = Color{200, 40, 40, 255};
                _charge->mark_used(gt);
                boss_state = BossState::ATTACK;
                _combo_on_skill_end();
            }
            break;
        }
        boss_state = BossState::ATTACK;
        _combo_on_skill_end();
        break;
    }
    case BossState::SHOCKWAVE: {
        // C1: 蓄力阶段显示半透明冲击波范围
        if (_shockwave->windup_left > 0) {
            _shockwave->windup_left -= (float)dt;
            if (effects) {
                Effect warn;
                warn.kind = "pulse";
                warn.world_x = self->entity.rect.x + self->entity.rect.width/2;
                warn.world_y = self->entity.rect.y + self->entity.rect.height/2;
                warn.radius = _shockwave->fx_radius; warn.duration = 0.35f; warn.elapsed = 0;
                warn.color = {255, 180, 40, 210};
                effects->push_back(warn);
                Effect flash;
                flash.kind = "flash"; flash.world_x = warn.world_x; flash.world_y = warn.world_y;
                flash.radius = _shockwave->fx_radius * 0.5f;
                flash.duration = 0.35f; flash.elapsed = 0;
                flash.color = {255, 120, 30, 180};
                effects->push_back(flash);
            }
            std::vector<Monster*> dm2;
            _shockwave->execute(self, player, dm2, map, gt);
            if (_shockwave->windup_left <= 0) {
                // 释放
                _spawn_boss_vfx(self, "shockwave", effects);
                { std::vector<Monster*> dm3; _shockwave->execute(self, player, dm3, map, gt); }
                self->color = Color{200, 40, 40, 255};
                _shockwave->mark_used(gt);
                boss_state = BossState::ATTACK;
                _combo_on_skill_end();
            }
            break;
        }
        boss_state = BossState::ATTACK;
        _combo_on_skill_end();
        break;
    }
    case BossState::SUMMON: {
        auto mlist = all ? *all : std::vector<Monster*>{};
        // D8: Necromancer Phase2 summons more
        int extra = (phase2 && skill_cycle_bias == 4) ? 2 : 0;
        for (int n = 0; n < 1 + extra; n++) {
            _summon->execute(self, player, mlist, map, gt);
        }
        _spawn_boss_vfx(self, "summon", effects);
        boss_state = BossState::ATTACK;
        _combo_on_skill_end();
        break;
    }
    case BossState::DEFEND: {
        // D8: Golem — 举盾减伤 3 秒
        auto& mod = self->combat.modifiers;
        float pct = golem_shield_pct > 0 ? golem_shield_pct : 0.70f;
        mod["def_pct"] = (mod.count("def_pct") ? mod["def_pct"] : 0) + pct;
        if (effects) {
            Effect s;
            s.kind = "pulse";
            s.world_x = self->entity.rect.x + self->entity.rect.width/2;
            s.world_y = self->entity.rect.y + self->entity.rect.height/2;
            s.radius = 56; s.duration = 0.5f; s.elapsed = 0;
            s.color = {60, 140, 255, 160};
            effects->push_back(s);
        }
        boss_state = BossState::ATTACK;
        _combo_on_skill_end();
        break;
    }
    // ── G5.4: Signature Phase2 skill states ──
    case BossState::WHIRLWIND: {
        if (_whirlwind->spin_duration <= 0) {
            _whirlwind->spin_duration = 1.2f;
            _whirlwind->spin_hit_count = 0;
            _whirlwind->windup_left = 0.4f; // 蓄力预警
        }
        if (_whirlwind->windup_left > 0) {
            _whirlwind->windup_left -= (float)dt;
            _spawn_boss_vfx(self, "charge", effects); // 预警特效
            break;
        }
        _whirlwind->spin_duration -= (float)dt;
        _whirlwind->execute(self, player, empty_slots, map, gt);
        // boss slowly moves toward player while spinning
        float dx = player->entity.rect.x + player->entity.rect.width/2
                 - self->entity.rect.x - self->entity.rect.width/2;
        float dy = player->entity.rect.y + player->entity.rect.height/2
                 - self->entity.rect.y - self->entity.rect.height/2;
        float len = sqrtf(dx*dx+dy*dy);
        if (len > 1) _apply_movement(self, map, dx/len, dy/len, dt * 0.6);
        if (_whirlwind->spin_duration <= 0) {
            boss_state = BossState::ATTACK;
            _combo_on_skill_end();
        }
        break;
    }
    case BossState::LASER_BARRAGE: {
        if (_laser->windup_left > 0) {
            _laser->windup_left -= (float)dt;
            _spawn_boss_vfx(self, "shockwave", effects); // 蓄力预警
            if (_laser->windup_left <= 0) {
                _laser->execute(self, player, empty_slots, map, gt);
                _spawn_boss_vfx(self, "shockwave", effects);
                boss_state = BossState::ATTACK;
                _combo_on_skill_end();
            }
        } else {
            _laser->windup_left = 0.8f;
        }
        break;
    }
    // ── M4a: 连招技能状态 ──
    case BossState::RANGED_BARRAGE: {
        if (_barrage->windup_left > 0) {
            _barrage->windup_left -= (float)dt;
            if (effects) {
                Effect warn;
                warn.kind = "pulse";
                warn.world_x = self->entity.rect.x + self->entity.rect.width/2;
                warn.world_y = self->entity.rect.y + self->entity.rect.height/2;
                warn.radius = 70; warn.duration = 0.25f; warn.elapsed = 0;
                warn.color = {150, 80, 255, 200};
                effects->push_back(warn);
            }
            break;
        }
        std::string msg = _barrage->execute(self, player, empty_slots, map, gt);
        if (!msg.empty() && effects) {
            // M4a-fx: 发射爆点 (紫色波纹 + 火花)
            VFXServer v;
            float bx = self->entity.rect.x + self->entity.rect.width/2;
            float by = self->entity.rect.y + self->entity.rect.height/2;
            v.ring(bx, by, 42, {170, 100, 255, 255}, 2, 0.30f);
            v.spark_burst(bx, by, 10, {220, 170, 255, 255}, 0.35f);
            for (auto& e : v.effects) effects->push_back(e);
        }
        if (effects && !_barrage->hit_fx.empty()) {
            // M4a-fx: 命中爆炸反馈 (爆炸 + 冲击波 + 闪光)
            VFXServer v;
            for (auto& [hx, hy] : _barrage->hit_fx) {
                v.explosion(hx, hy, 18, {210, 130, 255, 255}, 8, 0.35f);
                v.shockwave(hx, hy, 26, {170, 90, 255, 255}, 2, 0.40f);
                v.flash(hx, hy, 30, {230, 190, 255, 210}, 0.12f);
            }
            for (auto& e : v.effects) effects->push_back(e);
            _barrage->hit_fx.clear();
        }
        if (_barrage->finished) {
            boss_state = BossState::ATTACK;
            _combo_on_skill_end();
        }
        break;
    }
    case BossState::CONE_ATTACK: {
        if (_cone->windup_left > 0) {
            _cone->windup_left -= (float)dt;
            if (effects) {
                Effect warn;
                warn.kind = "pulse";
                warn.world_x = self->entity.rect.x + self->entity.rect.width/2;
                warn.world_y = self->entity.rect.y + self->entity.rect.height/2;
                warn.radius = _cone->reach; warn.duration = 0.14f; warn.elapsed = 0;
                warn.color = {140, 240, 80, 140};
                effects->push_back(warn);
            }
            break;
        }
        _cone->execute(self, player, empty_slots, map, gt);
        boss_state = BossState::ATTACK;
        _combo_on_skill_end();
        break;
    }
    case BossState::BLINK: {
        if (_blink->windup_left > 0) {
            _blink->windup_left -= (float)dt;
            if (effects) {
                Effect warn;
                warn.kind = "pulse";
                warn.world_x = _blink->pending_x;
                warn.world_y = _blink->pending_y;
                warn.radius = 26; warn.duration = 0.12f; warn.elapsed = 0;
                warn.color = {150, 100, 255, 180};
                effects->push_back(warn);
            }
            break;
        }
        _blink->execute(self, player, empty_slots, map, gt);
        if (_blink->blinked) {
            boss_state = BossState::ATTACK;
            _combo_on_skill_end();
        }
        break;
    }
    case BossState::GRAVITY_PULL: {
        // Demon Lord: pull player closer + delayed shockwave
        float bx = self->entity.rect.x + self->entity.rect.width/2;
        float by = self->entity.rect.y + self->entity.rect.height/2;
        float px = player->entity.rect.x + player->entity.rect.width/2;
        float py = player->entity.rect.y + player->entity.rect.height/2;
        float dx = bx - px, dy = by - py;
        float len = sqrtf(dx*dx+dy*dy);
        if (len > 1 && len < 300.0f) {
            clamp_displacement(player->entity, dx/len * 120.0f * (float)dt,
                               dy/len * 120.0f * (float)dt, map);
        }
        // 0.8s pull → shockwave with 1.5x range
        _gravity_timer += (float)dt;
        if (_gravity_timer > 0.8f) {
            _gravity_timer = 0;
            _shockwave->fx_radius *= 1.5f;
            _shockwave->execute(self, player, empty_slots, map, gt);
            _shockwave->fx_radius /= 1.5f;
            _shockwave->mark_used(gt);
            _spawn_boss_vfx(self, "shockwave", effects);
            boss_state = BossState::ATTACK;
            _combo_on_skill_end();
        }
        break;
    }
    default: break;
    }
}

float BossAI::_hp_ratio(Monster* self) const {
    return (float)self->combat.current_hp / self->combat.max_hp;
}

// ============================================================
// G1 Step6: visual_id → Color 映射 (表现层, 未来替换为 texture)
// ============================================================
Color get_boss_visual_color(const std::string& vid) {
    if (vid == "shadow_knight") return {120, 20, 180, 255};
    if (vid == "necromancer")   return {80, 180, 80, 255};
    if (vid == "vampire")       return {180, 40, 60, 255};
    if (vid == "fire_demon")    return {240, 100, 20, 255};
    if (vid == "golem")         return {100, 100, 130, 255};
    if (vid == "demon_lord")    return {180, 20, 20, 255};
    return {180, 20, 20, 255};  // fallback: demon lord red
}

// ============================================================
// G1 Step6: BossType → BossDef::id 辅助映射
// ============================================================
static const char* _boss_type_to_id(BossType t) {
    switch (t) {
        case BossType::SHADOW_KNIGHT: return "shadow_knight";
        case BossType::NECROMANCER:   return "necromancer";
        case BossType::VAMPIRE:       return "vampire";
        case BossType::FIRE_DEMON:    return "fire_demon";
        case BossType::GOLEM:         return "golem";
        case BossType::DEMON_LORD:    return "demon_lord";
        default: return "shadow_knight";
    }
}

// ============================================================
// G1 Step6: Boss 技能描述文字 (供 UI 展示)
// ============================================================
const char* get_boss_skills_text(const BossDef* def) {
    if (!def) return "冲锋·冲击波·召唤手下";
    if (def->is_summoner) return "召唤·尸爆·亡灵大军";
    if (def->is_defender) return "重锤·震地·石甲";
    return "冲锋·冲击波·召唤手下";
}

// ============================================================
// G1 Step6: spawn_boss — deprecated wrapper (委托给 BossFactory)
// ============================================================
Monster* spawn_boss(int tile_x, int tile_y, int floor) {
    BossType type = (floor == 5) ? BossType::SHADOW_KNIGHT
                   : (floor == 10) ? BossType::FIRE_DEMON
                   : BossType::DEMON_LORD;
    return boss_factory_create(type, tile_x, tile_y, floor);
}

// ============================================================
// D8 Step2: boss_type_for_floor — 随机Boss池
// ============================================================
BossType boss_type_for_floor(int floor, uint32_t seed) {
    // G9.3 (RNG-002): seed=0 → 固定兜底 1 (原 random_device 使 BossType 进程随机)
    auto rng_local = std::mt19937(seed ? seed : 1u);
    if (floor == 5) {
        // G1 Step6: 3 种 Boss 随机 (Shadow Knight / Necromancer / Vampire)
        int roll = (int)(rng_local() % 3);
        if (roll == 0) return BossType::SHADOW_KNIGHT;
        if (roll == 1) return BossType::NECROMANCER;
        return BossType::VAMPIRE;
    } else if (floor == 10) {
        return BossType::FIRE_DEMON; // G10: F10 is domain boss only
    }
    return BossType::DEMON_LORD; // F15 固定
}

// ============================================================
// G1 Step6: boss_factory_create — 数据驱动 Boss 创建
// BossDef (bosses.json) → BossAI 参数设置 → Monster
// ============================================================
Monster* boss_factory_create(BossType type, int tile_x, int tile_y, int floor,
                              std::vector<Monster*>* out_monsters, GameMap* map) {
    const BossDef* def = get_boss_def(_boss_type_to_id(type));
    if (!def) {
        // 安全降级: 配置缺失时使用默认 Shadow Knight
        def = get_boss_def("shadow_knight");
        if (!def) {
            auto* boss = new Monster((float)tile_x * TILE_SIZE, (float)tile_y * TILE_SIZE,
                "暗影骑士", 250, 15, 10, 4, get_boss_visual_color("shadow_knight"), new BossAI());
            boss->is_boss = true;
            boss->entity.size = {48, 48};
            boss->entity.rect = {boss->entity.position.x, boss->entity.position.y, 48, 48};
            boss->sprite_override = "boss_f5";   // M4f.5: 降级默认 F5 形象
            return boss;
        }
    }

    BossAI* ai = new BossAI();
    int scaled_hp  = (int)(def->hp  * g_growth.boss_hp_scale(floor));
    int scaled_atk = (int)(def->atk * g_growth.boss_atk_scale(floor));
    Monster* boss = new Monster(
        (float)tile_x * TILE_SIZE, (float)tile_y * TILE_SIZE,
        def->name, scaled_hp, scaled_atk, def->pdef, def->mdef,
        get_boss_visual_color(def->visual_id), ai);
    boss->is_boss = true;
    boss->entity.size = {48, 48};
    boss->entity.rect = {boss->entity.position.x, boss->entity.position.y, 48, 48};

    // ── M5-C: Boss 素材精灵数据驱动 (visual_id → boss_<id>, 缺失回退旧按层链) ──
    // 旧链保留兜底: F15 镜像有意用玩家形象 boss_self; F5/F10 未注册新图时落回旧 key
    {
        std::string vkey = "boss_" + def->visual_id;
        SpriteDef probe;
        if (ResourceManager::inst().sprite_by_key(vkey.c_str(), probe).id > 0) {
            boss->sprite_override = vkey;
        } else {
            boss->sprite_override = floor <= 5 ? "boss_f5"
                                  : floor <= 10 ? "boss_f10" : "boss_self";
        }
    }

    // ── G1 Step6: Phase2 参数 (替代硬编码) ──
    ai->_phase2_hp_threshold = def->phase2_hp_threshold;
    ai->_phase2_pause        = def->phase2_pause;
    ai->_phase2_speed_mult   = def->phase2_speed_mult;
    ai->_phase2_atk_mult     = def->phase2_atk_mult;
    ai->_phase2_cd_mult      = def->phase2_cd_mult;

    // ── G5.4: Boss ID 用于 Phase2 行为分支 ──
    ai->_boss_id = def->id.c_str();

    // ── G2.3: Arena 配置 ──
    ai->_arena_cfg = &def->arena;

    // ── 技能循环 bias ──
    ai->skill_cycle_bias = def->skill_cycle_bias;

    // ── 技能覆盖 (BossDef → BossSkill: 冷却/伤害倍率/蓄力/范围 全数据驱动) ──
    for (auto& sk : def->skill_overrides) {
        if (sk.id == "charge" || sk.id == "冲锋") {
            ai->_charge->cooldown    = sk.cooldown;
            ai->_charge->damage_mult = sk.damage_mult;
            ai->_charge->windup_time = sk.windup;
            ai->_charge->fx_radius   = sk.range;
        } else if (sk.id == "shockwave" || sk.id == "冲击波") {
            ai->_shockwave->cooldown    = sk.cooldown;
            ai->_shockwave->damage_mult = sk.damage_mult;
            ai->_shockwave->windup_time = sk.windup;
            ai->_shockwave->fx_radius   = sk.range;
        } else if (sk.id == "summon" || sk.id == "召唤") {
            ai->_summon->cooldown = sk.cooldown;
        } else if (sk.id == "barrage") {
            ai->_barrage->cooldown    = sk.cooldown;
            ai->_barrage->damage_mult = sk.damage_mult;
            ai->_barrage->windup_time = sk.windup;
            ai->_barrage->spread_deg  = sk.range;
            ai->_barrage->shot_count  = sk.shot_count;
            ai->_barrage->pattern     = sk.pattern;
            ai->_barrage->waves       = sk.waves;
            ai->_barrage->wave_interval = sk.wave_interval;
            ai->_barrage->spiral_turn_deg = sk.spiral_turn_deg;
        } else if (sk.id == "cone") {
            ai->_cone->cooldown    = sk.cooldown;
            ai->_cone->damage_mult = sk.damage_mult;
            ai->_cone->windup_time = sk.windup;
            ai->_cone->reach       = sk.range;
        } else if (sk.id == "blink") {
            ai->_blink->cooldown   = sk.cooldown;
            ai->_blink->blink_dist = sk.range;
        }
    }

    // ── M4a: 连招模板 (无配置则走旧循环) ──
    ai->_combos = &def->combos;

    // ── 特殊行为标志 ──
    if (def->is_summoner) {
        ai->skill_cycle_bias = 4;  // Necromancer 召唤特化
    }
    if (def->is_defender) {
        ai->golem_shield_pct = def->shield_pct;
    }

    (void)out_monsters; (void)map;
    return boss;
}
