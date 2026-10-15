#include "boss_command.h"
#include "monster.h"
#include "player.h"
#include "game_map.h"
#include "combat_system.h"
#include "vfx_server.h"
#include <cmath>

// inline VFX helper (duplicated from boss.cpp static, for command layer independence)
static void _cmd_vfx(Monster* self, const std::string& kind, std::vector<Effect>* effects) {
    if (!effects) return;
    VFXServer v;
    float mx = self->entity.rect.x + self->entity.rect.width / 2;
    float my = self->entity.rect.y + self->entity.rect.height / 2;
    if (kind == "charge")       v.boss_cone(mx, my);
    else if (kind == "shockwave") v.boss_circle(mx, my);
    else if (kind == "summon")    v.boss_summon(mx, my);
    for (auto& e : v.effects) effects->push_back(e);
}

BossCommand boss_decision_to_command(int dec_enum) {
    switch (dec_enum) {
        case 0:  return BossCommand::MOVE;   // CHASE→MOVE
        case 1:  return BossCommand::RETREAT;
        case 2:  return BossCommand::CHARGE;
        case 3:  return BossCommand::SUMMON;
        case 4:  return BossCommand::DEFEND;
        case 5:  return BossCommand::CAST;   // RANGED→CAST
        case 6:  return BossCommand::CHARGE; // MELEE→CHARGE
        case 7:  return BossCommand::CAST;   // SPECIAL→CAST
        case 8:  return BossCommand::PHASE;
        case 9:  return BossCommand::LAST_STAND;
        default: return BossCommand::NONE;
    }
}

const char* boss_command_name(BossCommand c) {
    switch (c) {
        case BossCommand::MOVE:      return "MOVE";
        case BossCommand::CHARGE:    return "CHARGE";
        case BossCommand::SHOCKWAVE: return "SHOCKWAVE";
        case BossCommand::SUMMON:    return "SUMMON";
        case BossCommand::DEFEND:    return "DEFEND";
        case BossCommand::RETREAT:   return "RETREAT";
        case BossCommand::CAST:      return "CAST";
        case BossCommand::PHASE:     return "PHASE";
        case BossCommand::LAST_STAND: return "LAST_STAND";
        default: return "NONE";
    }
}

BossAction boss_default_action(BossCommand c) {
    BossAction a; a.command = c;
    switch (c) {
        case BossCommand::CHARGE:    a.windup = 0.6f; a.duration = 0.3f; a.cooldown = 5.0f; break;
        case BossCommand::SHOCKWAVE: a.windup = 0.7f; a.duration = 0.2f; a.cooldown = 7.0f; break;
        case BossCommand::SUMMON:    a.windup = 0.0f; a.duration = 0.5f; a.cooldown = 12.0f; break;
        case BossCommand::DEFEND:    a.duration = 3.0f; a.cooldown = 10.0f; break;
        case BossCommand::CAST:      a.windup = 0.5f; a.duration = 0.3f; a.cooldown = 6.0f; break;
        default: break;
    }
    return a;
}

// ── G53: boss_execute_command 拆出的 5 个命令实现 ────────────

// 冲撞: 朝玩家位移 4 帧量 + 命中伤害
static void _cmd_charge(Monster* self, Player* player,
                        std::vector<Effect>* effects) {
    float dx = player->entity.rect.x - self->entity.rect.x;
    float dy = player->entity.rect.y - self->entity.rect.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len > 0) {
        self->entity.position.x += dx / len * 120.0f * 0.016f * 4;
        self->entity.position.y += dy / len * 120.0f * 0.016f * 4;
        self->entity.sync_rect();
    }
    if (CheckCollisionRecs(self->entity.rect, player->entity.rect)) {
        int dmg = calculate_damage((int)(self->combat.get_effective_attack() * 1.8f),
            player->combat.get_effective_defense(AttackType::PHYSICAL));
        player->combat.take_damage(dmg);
    }
    _cmd_vfx(self, "charge", effects);
}

// 冲击波: 100px 内魔伤
static void _cmd_shockwave(Monster* self, Player* player,
                           std::vector<Effect>* effects) {
    float dist = hypotf(
        player->entity.rect.x - self->entity.rect.x,
        player->entity.rect.y - self->entity.rect.y);
    if (dist < 100) {
        int dmg = calculate_damage((int)(self->combat.get_effective_attack() * 1.5f),
            player->combat.get_effective_defense(AttackType::MAGICAL), AttackType::MAGICAL);
        player->combat.take_damage(dmg);
    }
    _cmd_vfx(self, "shockwave", effects);
}

// 召唤: boss 周围 ±3 格内可走点放 2 只小怪
static void _cmd_summon(Monster* self, Player*, GameMap* map,
                        std::vector<Monster*>* all, std::vector<Effect>* effects) {
    if (all && map) {
        for (int i = 0; i < 2; i++) {
            int off_x = (rng() % 7) - 3, off_y = (rng() % 7) - 3;
            float sx = self->entity.position.x + off_x * 32;
            float sy = self->entity.position.y + off_y * 32;
            auto [tx, ty] = map->pixel_to_tile(sx, sy);
            if (map->is_walkable(tx, ty)) {
                auto* m = spawn_monster(sx, sy, (rng()%3==0)?"orc":"slime");
                if (m) all->push_back(m);
            }
        }
    }
    _cmd_vfx(self, "summon", effects);
}

// 防御: def_pct +60% + 蓝色脉冲
static void _cmd_defend(Monster* self, Player*,
                        std::vector<Effect>* effects) {
    self->combat.modifiers["def_pct"] =
        (self->combat.modifiers.count("def_pct") ? self->combat.modifiers["def_pct"] : 0) + 0.6f;
    if (effects) {
        Effect s; s.kind="pulse";
        s.world_x=self->entity.rect.x+self->entity.rect.width/2;
        s.world_y=self->entity.rect.y+self->entity.rect.height/2;
        s.radius=35; s.duration=0.15f; s.elapsed=0; s.color={60,140,255,140};
        effects->push_back(s);
    }
}

// 后撤: 远离玩家 1 帧量
static void _cmd_retreat(Monster* self, Player* player) {
    float dx = self->entity.rect.x - player->entity.rect.x;
    float dy = self->entity.rect.y - player->entity.rect.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len > 1) {
        self->entity.position.x += dx/len * 120.0f * 0.016f;
        self->entity.position.y += dy/len * 120.0f * 0.016f;
        self->entity.sync_rect();
    }
}

void boss_execute_command(BossCommand cmd, Monster* self, Player* player,
    GameMap* map, std::vector<Monster*>* all, std::vector<Effect>* effects,
    double) {
    if (!self || !player) return;
    switch (cmd) {
    case BossCommand::CHARGE:
        _cmd_charge(self, player, effects);
        break;
    case BossCommand::SHOCKWAVE:
        _cmd_shockwave(self, player, effects);
        break;
    case BossCommand::SUMMON:
        _cmd_summon(self, player, map, all, effects);
        break;
    case BossCommand::DEFEND:
        _cmd_defend(self, player, effects);
        break;
    case BossCommand::RETREAT:
        _cmd_retreat(self, player);
        break;
    case BossCommand::PHASE:
    case BossCommand::LAST_STAND:
    case BossCommand::CAST:   // CAST → SHOCKWAVE as fallback
        boss_execute_command(BossCommand::SHOCKWAVE, self, player, map, all, effects, 0);
        break;
    default: break;
    }
}
