#include "monster.h"
#include "ai.h"
#include "player.h"
#include "combat_system.h"
#include "core/logger.h"
#include "data/enemy_defs.h"   // G1 Step5
#include "resource_manager.h"                 // M4f.2
#include "game/rendering/sprite_renderer.h"   // M4f.2
#include "game/animation/skeleton_avatar.h"   // A6-S1: 骨骼皮肤绘制

// M4f.3: MonsterType → 精灵体型 (0人形 1史莱姆 2Boss 3箭 4甲 5炸弹 6帽)
static int _sprite_variant_for(bool is_boss, MonsterType type,
                               const std::string& name) {
    if (is_boss) return 2;
    switch (type) {
        case MonsterType::CHARGER:      return 3;
        case MonsterType::TANK:         return 4;
        case MonsterType::BOMBER:       return 5;
        case MonsterType::SUMMONER:
        case MonsterType::SHAMAN:       return 6;
        default:                        break;
    }
    return (name.find("史莱姆") != std::string::npos) ? 1 : 0;
}

// M4f.4: MonsterType/名字 → 素材精灵 key; 空 = 程序化占位
// G10.3-B1: 名字规则扩展 — 骨系→mon_skeleton, 萨满→mon_shaman
// M5-B: 兜底怪专属图 — 潜行者/火魔/精英兽人/刺客 (原先全落 mon_orc)
static const char* _monster_sprite_key(MonsterType type,
                                       const std::string& name, bool is_boss) {
    if (is_boss) return nullptr;
    switch (type) {
        case MonsterType::BOMBER:      return "mon_bomber";
        case MonsterType::TANK:        return "mon_tank";
        case MonsterType::CHARGER:     return "mon_charger";
        case MonsterType::SUMMONER:   return "mon_summoner";
        case MonsterType::SHAMAN:     return "mon_shaman";
        default:                       break;
    }
    if (name.find("史莱姆") != std::string::npos) return "mon_slime";
    if (name.find("骨") != std::string::npos)     return "mon_skeleton";
    if (name.find("骷髅") != std::string::npos)   return "mon_skeleton";
    if (name.find("萨满") != std::string::npos)   return "mon_shaman";
    if (name.find("法师") != std::string::npos)   return "mon_shaman";
    if (name.find("潜伏") != std::string::npos) return "mon_shadow_stalker";
    if (name.find("潜行者") != std::string::npos) return "mon_shadow_stalker";
    if (name.find("刺客") != std::string::npos)   return "mon_shadow_assassin";
    if (name.find("火魔") != std::string::npos)   return "mon_fire_imp";
    if (name.find("守卫") != std::string::npos)   return "mon_tank";
    if (name.find("兽人") != std::string::npos)
        return (name.find("精英") != std::string::npos) ? "mon_elite_orc" : "mon_orc";
    return "mon_orc";
}

// M4f.12: 怪物持械 — 按类型映射武器素材 (Boss持剑/冲锋持矛/弓手持弩/重装持剑)
static const char* _monster_weapon_key(MonsterType type, bool is_boss,
                                       const std::string& name) {
    if (is_boss) return "weapon_sword";
    if (name.find("兽人") != std::string::npos) return "weapon_sword";
    switch (type) {
        case MonsterType::CHARGER: return "weapon_spear";
        case MonsterType::TANK:    return "weapon_sword";
        case MonsterType::ARCHER:  return "weapon_crossbow";
        case MonsterType::ELITE:   return "weapon_sword";
        default: break;
    }
    return nullptr;
}

// M4f.12: 怪物武器绘制 — 身体右缘竖持, 攻击后 0.25s 内挥砍
static void _draw_monster_weapon(const Monster* m, const Rectangle& dr) {
    const char* wkey = _monster_weapon_key(m->monster_type, m->is_boss, m->name);
    if (!wkey) return;
    SpriteDef wdef;
    Texture2D wtex = ResourceManager::inst().sprite_by_key(wkey, wdef);
    if (wtex.id <= 0) return;
    float t = (float)GetTime();
    float since = t - m->last_attack_wall_time;
    float p = std::min(1.0f, since / 0.25f);
    const float kPi = 3.14159265f;
    float angle = since < 0.25f ? 70.0f * sinf(p * kPi)
                                : 4.0f * sinf(t * 2.5f);
    float w = 10.0f, h = 16.0f;
    float cx = dr.x + dr.width - 4, cy = dr.y + dr.height / 2 + 2;
    Vector2 origin{w / 2, 2};
    Rectangle src = SpriteRenderer::frame_rect(wdef, 0);
    Rectangle dst = {cx - w / 2 + origin.x, cy - h + origin.y, w, h};
    DrawTexturePro(wtex, src, dst, origin, angle, WHITE);
}

// M4f.2: 程序化怪物精灵, 纹理缺失时返回空矩形
static Rectangle _draw_monster_sprite(const Rectangle& dr, Color body_c,
                                      int variant, bool& ok) {
    char key[32];
    snprintf(key, sizeof(key), "mon_%02x%02x%02x_%d",
        body_c.r, body_c.g, body_c.b, variant);
    Texture2D tex = ResourceManager::inst().procedural_sprite(
        key, body_c, {255, 255, 255, 255}, variant, 0);
    ok = tex.id > 0;
    if (!ok) return dr;
    SpriteDef sd; sd.frame_w = 32; sd.frame_h = 32; sd.frame_count = 2;
    int frame = ((int)(GetTime() * 4)) & 1;   // M4f.3: 待机/呼吸轮换
    float inset = dr.width > 30 ? 0 : (dr.width < 22 ? 6 : 3);
    Rectangle dst = {dr.x + inset, dr.y + inset,
                     dr.width - inset * 2, dr.height - inset * 2};
    SpriteRenderer::draw_sprite(tex, sd, frame, dst);
    return dst;
}

// 素材/程序化双缺席时的几何回退 (历史绘制)
static void _draw_legacy_monster_body(const Rectangle& dr, Color color,
                                      const std::string& name) {
    DrawRectangleRounded(dr, 0.15f, 4, color);
    DrawRectangleRounded({dr.x + 3, dr.y + 2, dr.width - 6, 6}, 0.1f, 3,
        Color{(unsigned char)std::min(255u, (unsigned)color.r + 60),
              (unsigned char)std::min(255u, (unsigned)color.g + 60),
              (unsigned char)std::min(255u, (unsigned)color.b + 60), 255});
    float ey = dr.y + dr.height / 2 - 2;
    if (name.find("史莱姆") != std::string::npos) {
        DrawCircle(dr.x + dr.width / 3, ey, 5, WHITE);
        DrawCircle(dr.x + dr.width * 2 / 3, ey, 5, WHITE);
        DrawCircle(dr.x + dr.width / 3 + 1, ey + 1, 3, {20, 20, 20, 255});
        DrawCircle(dr.x + dr.width * 2 / 3 + 1, ey + 1, 3, {20, 20, 20, 255});
    } else if (name.find("兽人") != std::string::npos) {
        for (int sx : {int(dr.x + 5), int(dr.x + dr.width - 9)}) {
            DrawTriangle({(float)sx, ey}, {(float)sx + 5, ey + 2},
                        {(float)sx, ey + 5}, {255, 50, 50, 255});
        }
    }
}

// M4f.4: 怪物身体三态 fallback — 素材精灵 > 程序化占位 > 几何回退
static Rectangle _draw_monster_body(const Rectangle& dr, Color color,
                                    MonsterType type, const std::string& name,
                                    bool is_boss,
                                    const std::string& override_key) {
    const char* skey = !override_key.empty()
        ? override_key.c_str() : _monster_sprite_key(type, name, is_boss);
    SpriteDef sdef;
    Texture2D stex = skey
        ? ResourceManager::inst().sprite_by_key(skey, sdef)
        : Texture2D{0};
    if (stex.id > 0) {
        SpriteRenderer::draw_sprite(stex, sdef, 0, dr);
        return dr;
    }
    int variant = _sprite_variant_for(is_boss, type, name);
    bool spr_ok = false;
    Rectangle spr = _draw_monster_sprite(dr, color, variant, spr_ok);
    if (spr_ok) return spr;
    _draw_legacy_monster_body(dr, color, name);
    return dr;
}

// Q3.9: 实例ID递增分配 — 同种子下创建顺序确定 → ID 序列确定
uint64_t Monster::_next_instance_id = 1;

Monster::Monster(float x, float y, const std::string& n, int hp, int atk,
                 int pdef, int mdef, Color c, MonsterAI* a)
    : entity(x, y, 28, 28), combat(hp, atk, pdef, mdef), name(n), color(c) {
    color.a = 255;
    instance_id = _next_instance_id++;
    ai = a ? a : new MonsterAI();
}

Monster::~Monster() { delete ai; }

// ── A6-S1: 渲染层信号映射 (纯表现, 不读写任何 gameplay 状态) ──
void Monster::set_skeleton_avatar(std::unique_ptr<SkeletonAvatar> avatar) {
    _skeleton_avatar = std::move(avatar);
}

std::string monster_actor_key(const Monster& m) {
    return !m.sprite_override.empty() ? m.sprite_override : m.name;
}

AnimInput monster_anim_input(const Monster& m, int& last_hp, float now_wall) {
    constexpr float kSwingWindowSec = 0.25f;   // 与 _draw_monster_weapon 挥砍窗口同款
    AnimInput in;
    in.attack_recovery_ratio = 1.f;            // 怪物无独立恢复计时字段 → 1.0
    if (!m.ai) {                               // ai 不可见 → 回退 idle
        last_hp = m.combat.current_hp;
        return in;
    }
    in.moving = (m.ai->state == AIState::CHASE);   // 实际移动态 (IDLE 巡逻视作站立)
    in.attacking = (now_wall - m.last_attack_wall_time) < kSwingWindowSec;
    in.hit_flash = (last_hp >= 0 && m.combat.current_hp < last_hp);   // hp 下降沿
    last_hp = m.combat.current_hp;
    return in;
}

bool Monster::can_attack(double gt) const {
    return (gt - last_attack_time) >= attack_cooldown;
}

int Monster::attack_target(Player* target, double gt) {
    int dmg = calculate_damage(get_effective_attack(this),
                                target->combat.get_effective_defense(attack_type),
                                attack_type);
    target->combat.take_damage(dmg);
    if (dmg > 0) {
        LOG_INFO("[DMG] %s普攻 造成 %d 伤害 → 玩家", name.c_str(), dmg);
        target->combat.mark_damage_logged();
        target->combat.last_damage_source = name;   // M2-B: 死因归因
    }
    last_attack_time = (float)gt;
    last_attack_wall_time = (float)GetTime();
    // 怪物命中附带 Buff (统一触发规则)
    for (auto& tr : on_hit_triggers) {
        if (tr.chance >= 1.0f || (float)(rng() % 1000) / 1000.0f < tr.chance)
            apply_buff(target, tr.buff_id, tr.stacks);
    }
    return dmg;
}

void Monster::update_ai(Player* player, GameMap* map, double dt, double gt,
                         std::vector<Monster*>* all, std::vector<Effect>* effects,
                         int monster_room, int player_room,
                         const RoomManager* room_mgr) {
    if (ai) ai->update(this, player, map, dt, gt, all, effects, monster_room, player_room, room_mgr);
}

void Monster::draw(float cam_x, float cam_y) {
    Entity& e = entity;
    Rectangle dr = {e.position.x - cam_x, e.position.y - cam_y,
                    e.size.x, e.size.y};

    // Boss 光晕
    if (is_boss) {
        float pulse = 3 + fabs(sinf((float)GetTime() * 6)) * 5;
        DrawRectangleLinesEx({dr.x - pulse, dr.y - pulse,
                              dr.width + pulse*2, dr.height + pulse*2},
                             3, {60, 5, 5, 255});
    }

    // B14: Bomber 脉冲光晕
    if (monster_type == MonsterType::BOMBER) {
        float pulse = 2 + fabs(sinf((float)GetTime() * 10)) * 3;
        DrawRectangleLinesEx({dr.x - pulse, dr.y - pulse,
                              dr.width + pulse*2, dr.height + pulse*2},
                             2, Color{255, 200, 60, (unsigned char)(120 + pulse * 20)});
    }
    // B14: Tank 厚边框
    if (monster_type == MonsterType::TANK) {
        DrawRectangleLinesEx({dr.x - 2, dr.y - 2, dr.width + 4, dr.height + 4},
                             3, Color{180, 180, 200, 200});
    }
    // D8: Charger — 红色箭头方向指示
    if (monster_type == MonsterType::CHARGER) {
        float cx = dr.x + dr.width/2, cy = dr.y + dr.height/2;
        float arrow = 6 + sinf((float)GetTime() * 8) * 3;
        DrawTriangle({cx + arrow, cy}, {cx - arrow/2, cy - arrow/2},
                     {cx - arrow/2, cy + arrow/2}, Color{255, 100, 40, 255});
    }
    // D8: Summoner — 紫色魔法光环
    if (monster_type == MonsterType::SUMMONER) {
        float ring = 14 + sinf((float)GetTime() * 6) * 3;
        DrawRing({dr.x + dr.width/2, dr.y + dr.height/2}, ring - 2, ring,
                 0, 360, 16, Color{180, 120, 220, 160});
    }

    // 阴影
    DrawEllipse(dr.x + dr.width/2, dr.y + dr.height + 2, dr.width/2 - 2, 3,
                {0, 0, 0, 80});

    // A6-S1: 骨骼皮肤覆盖 — 仅 actor_avatars.json 白名单命中且懒建成功才激活
    // (白名单空 = 永不命中, 旧 sprite 路径原样); squash 类 B3 效果本阶段不做
    SkeletonAvatar* skeleton = _skeleton_avatar.get();
    if (skeleton && skeleton->active()) {
        skeleton->draw_at({dr.x + dr.width / 2, dr.y + dr.height},
                          skeleton->facing(), 255);
    } else {
        // M4f.4: 怪物身体三态 fallback — 素材精灵 > 程序化 > 几何
        Rectangle spr = _draw_monster_body(dr, color, monster_type, name, is_boss,
                                           sprite_override);

        // M4f.12: 怪物持械 (身体上层)
        _draw_monster_weapon(this, dr);

        // 边框 (纹理精灵带轮廓, 仍画描边强化辨识)
        Color bc = is_boss ? Color{255, 180, 30, 255} : Color{0, 0, 0, 255};
        DrawRectangleRoundedLines(spr, 0.15f, 4, is_boss ? 3 : 2, bc);
    }

    // 血条 (非Boss 且受伤)
    if (!is_boss && combat.current_hp < combat.max_hp) {
        float ratio = (float)combat.current_hp / combat.max_hp;
        DrawRectangle(dr.x, dr.y - 8, dr.width, 4, {40, 10, 10, 255});
        Color hc = ratio > 0.5f ? Color{200, 40, 40, 255} : Color{200, 20, 20, 255};
        DrawRectangle(dr.x, dr.y - 8, dr.width * ratio, 4, hc);
    }
}

// ============================================================
// String → Enum helpers (gameplay logic, stays in entity layer)
// ============================================================

static MonsterType _str_to_monster_type(const std::string& s) {
    if (s == "archer")   return MonsterType::ARCHER;
    if (s == "shaman")   return MonsterType::SHAMAN;
    if (s == "bomber")   return MonsterType::BOMBER;
    if (s == "tank")     return MonsterType::TANK;
    if (s == "elite")    return MonsterType::ELITE;
    if (s == "charger")  return MonsterType::CHARGER;
    if (s == "summoner") return MonsterType::SUMMONER;
    return MonsterType::NORMAL;
}

static TeamRole _str_to_team_role(const std::string& s) {
    if (s == "frontline") return TeamRole::FRONTLINE;
    if (s == "backline")  return TeamRole::BACKLINE;
    if (s == "support")   return TeamRole::SUPPORT;
    if (s == "flank")     return TeamRole::FLANK;
    if (s == "command")   return TeamRole::COMMAND;
    return TeamRole::NONE;
}

static AttackType _str_to_attack_type(const std::string& s) {
    if (s == "magical") return AttackType::MAGICAL;
    if (s == "true")    return AttackType::TRUE;
    return AttackType::PHYSICAL;
}

static MonsterSkillType _str_to_skill_type(const std::string& s) {
    if (s == "rapid_shot")   return MonsterSkillType::RAPID_SHOT;
    if (s == "totem")        return MonsterSkillType::TOTEM;
    if (s == "leap")         return MonsterSkillType::LEAP;
    if (s == "shield")       return MonsterSkillType::SHIELD;
    if (s == "summon")       return MonsterSkillType::SUMMON;
    if (s == "charge")       return MonsterSkillType::CHARGE;
    if (s == "mass_summon")  return MonsterSkillType::MASS_SUMMON;
    if (s == "snipe")        return MonsterSkillType::SNIPE;
    if (s == "scatter")      return MonsterSkillType::SCATTER;
    if (s == "ambush_attack") return MonsterSkillType::AMBUSH_ATTACK;
    if (s == "guard_aura")   return MonsterSkillType::GUARD_AURA;
    return MonsterSkillType::NONE;
}

static AIArchetype _str_to_archetype(const std::string& s) {
    if (s == "bomber" || s == "charger")   // old monster_type mapping fallback
    { if (s == "bomber")   return AIArchetype::BOMBER;
      if (s == "charger")  return AIArchetype::CHARGER; }
    if (s == "shaman")    return AIArchetype::SHAMAN;
    if (s == "sniper")    return AIArchetype::SNIPER;
    if (s == "controller")return AIArchetype::CONTROLLER;
    if (s == "ambush")    return AIArchetype::AMBUSH;
    if (s == "guardian")  return AIArchetype::GUARDIAN;
    if (s == "summoner")  return AIArchetype::SUMMONER;
    return AIArchetype::DEFAULT;
}

// visual_id → Color (表现层映射, 未来替换为 texture/animation)
// M5-D: 新专属怪配色 (与 mon_<visual_id> 专属图同色系)
static Color _visual_to_color(const std::string& vid) {
    if (vid == "slime")       return {100, 180, 100, 255};
    if (vid == "orc")         return {200,  80,  80, 255};
    if (vid == "archer")      return { 80, 160,  80, 255};
    if (vid == "shaman")      return {160, 100, 200, 255};
    if (vid == "bomber")      return {255, 140,  40, 255};
    if (vid == "tank")        return {120, 120, 140, 255};
    if (vid == "elite_slime") return {100, 220, 100, 255};
    if (vid == "elite_orc")   return {240,  60,  60, 255};
    if (vid == "charger")     return {200, 140,  60, 255};
    if (vid == "summoner")    return {180, 120, 220, 255};
    // M5-D 专属怪 (visual_id 已回归自身 id)
    if (vid == "shadow_stalker")  return { 58,  48,  92, 255};
    if (vid == "fire_imp")        return {232,  96,  40, 255};
    if (vid == "shadow_assassin") return { 72,  60, 110, 255};
    if (vid == "dark_mage")       return { 70,  50, 110, 255};
    if (vid == "void_walker")     return { 60,  40,  90, 255};
    if (vid == "night_stalker")   return { 90,  70,  60, 255};
    if (vid == "ice_warden")      return {140, 200, 230, 255};
    if (vid == "blood_leech")     return {170,  40,  60, 255};
    if (vid == "bone_soldier")    return {220, 215, 190, 255};
    if (vid == "skeleton_archer") return {210, 205, 185, 255};
    if (vid == "goblin_hunter")   return {110, 150,  70, 255};
    if (vid == "frost_slime")     return {150, 210, 240, 255};
    if (vid == "lightning_orb")   return {250, 230, 110, 255};
    if (vid == "poison_wyrm")     return {130, 170,  70, 255};
    if (vid == "golem")           return {140, 130, 120, 255};
    if (vid == "necromancer")    return {100, 130,  85, 255};
    if (vid == "storm_elemental") return {120, 170, 230, 255};
    if (vid == "blood_priest")    return {190,  40,  80, 255};
    if (vid == "stone_guardian")  return {150, 140, 125, 255};
    if (vid == "iron_sentinel")   return {170, 175, 185, 255};
    return {100, 180, 100, 255}; // fallback: slime green
}

// ============================================================
// G1 Step5: spawn_monster — generic factory driven by EnemyDef
// 新增普通怪物只需修改 enemies.json, 无需改 C++ 代码
// ============================================================
Monster* spawn_monster(float px, float py, const std::string& type) {
    // Elite: 随机选择变体 (唯一的运行时分支逻辑)
    std::string lookup = type;
    if (type == "elite") {
        lookup = (rng() % 2 == 0) ? "elite_slime" : "elite_orc";
    }

    const EnemyDef* def = get_enemy_def(lookup);
    if (!def) {
        // fallback — 配置缺失时安全降级
        auto* m = new Monster(px, py, "史莱姆", 15, 3, 0, 1,
                              _visual_to_color("slime"));
        m->monster_type = MonsterType::NORMAL;
        m->team_role = TeamRole::NONE;
        return m;
    }

    // 创建 AI (使用嵌套 ai 配置)
    auto* ai = new MonsterAI(def->ai.sight, def->ai.speed,
                             def->ai.patrol, def->ai.attack_range);

    // 创建 Monster (visual_id → Color 映射在 _visual_to_color)
    auto* m = new Monster(px, py, def->name, def->hp, def->atk,
                          def->pdef, def->mdef,
                          _visual_to_color(def->visual_id), ai);

    // 枚举字段 (字符串 → enum 映射)
    m->monster_type    = _str_to_monster_type(def->type_str);
    m->team_role       = _str_to_team_role(def->role_str);
    m->attack_type     = _str_to_attack_type(def->attack_type_str);
    m->attack_cooldown = def->attack_cooldown;
    m->is_elite        = def->is_elite;

    // M5-D: visual_id 派生精灵 (mon_<visual_id>), 未注册时留空走名字规则链
    // — 消灭"名字规则不命中→共用 orc"整类 bug (如 潜伏者死规则)
    {
        std::string vkey = "mon_" + def->visual_id;
        SpriteDef probe;
        if (ResourceManager::inst().sprite_by_key(vkey.c_str(), probe).id > 0)
            m->sprite_override = vkey;
    }

    // G5.3: AI Archetype (行为原型, 与 MonsterType 外观解耦)
    if (ai) ai->archetype = _str_to_archetype(def->ai_archetype);

    // D2: Ranged monsters use projectile attacks (data-driven from enemies.json)
    bool is_ranged = (m->monster_type == MonsterType::ARCHER
        || m->monster_type == MonsterType::SHAMAN);
    m->uses_projectile = def->projectile.enabled || is_ranged;
    m->projectile_speed = def->projectile.speed;
    m->projectile_warning_time = def->projectile.warning_time;
    m->projectile_warning_level = def->projectile.warning_level;

    // on_hit 触发器 (直接拷贝)
    m->on_hit_triggers = def->on_hit;

    // 技能 (数据驱动, 每条 record → MonsterSkillState)
    for (auto& sk : def->skills) {
        MonsterSkillType st = _str_to_skill_type(sk.id);
        if (st == MonsterSkillType::NONE) continue;
        MonsterSkillState state = {st, sk.initial_cooldown, sk.max_cooldown};
        ai->_skills.push_back(state);
    }

    // 精英: 随机 Buff (从 buff_pool 中抽一条, 空 buff_id = 跳过)
    if (def->is_elite && !def->elite_buff_pool.empty()) {
        int roll = (int)(rng() % (uint32_t)def->elite_buff_pool.size());
        auto& eb = def->elite_buff_pool[roll];
        if (!eb.buff_id.empty())
            apply_buff(m, eb.buff_id, eb.stacks);
    }

    return m;
}
