#include "skill.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "combat_system.h"
#include "data/skill_defs.h"   // G3.2
#include <cmath>
#include <algorithm>

// ---- 从 skill.h 移出的实现 ----

// ── G3.2: String → BuildTag 辅助 ──
static BuildTag _tag_from_string(const std::string& s) {
    if (s == "melee")      return BuildTag::MELEE;
    if (s == "ranged")     return BuildTag::RANGED;
    if (s == "magic")      return BuildTag::MAGIC;
    if (s == "fire")       return BuildTag::FIRE;
    if (s == "ice")        return BuildTag::ICE;
    if (s == "lightning")  return BuildTag::LIGHTNING;
    if (s == "poison")     return BuildTag::POISON;
    if (s == "bleed")      return BuildTag::BLEED;
    if (s == "summon")     return BuildTag::SUMMON;
    if (s == "combo")      return BuildTag::COMBO;
    if (s == "heavy")      return BuildTag::HEAVY;
    if (s == "aoe")        return BuildTag::AOE;
    if (s == "projectile") return BuildTag::PROJECTILE;
    if (s == "heal")       return BuildTag::HEAL;
    if (s == "time")       return BuildTag::TIME;
    if (s == "defense")    return BuildTag::DEFENSE;
    if (s == "support")    return BuildTag::SUPPORT;
    if (s == "dot")        return BuildTag::DOT;
    if (s == "knockback")  return BuildTag::KNOCKBACK;
    return BuildTag::NONE;
}

// ── G3.2: 从 SkillDef 填充通用字段 ──
static void _fill_common(Skill* sk, const SkillDef& def) {
    sk->_skill_id = def.id;
    for (auto& t : def.tags) {
        BuildTag bt = _tag_from_string(t);
        if (bt != BuildTag::NONE) sk->tags.push_back(bt);
    }
    for (auto& ev : def.evolutions) {
        SkillEvolution se;
        se.level = ev.level;
        se.name = ev.name;
        se.desc = ev.desc;
        for (auto& rt : ev.required_tags) {
            BuildTag bt = _tag_from_string(rt);
            if (bt != BuildTag::NONE) se.required_tags.push_back(bt);
        }
        sk->evolutions.push_back(se);
    }
}

// ============================================================
// D3 Step2: Evolution 实现
// ============================================================
void Skill::add_evolution(int lv, const char* name, const char* desc,
                           std::initializer_list<BuildTag> req) {
    SkillEvolution ev;
    ev.level = lv; ev.name = name; ev.desc = desc;
    for (auto t : req) ev.required_tags.push_back(t);
    evolutions.push_back(ev);
}

bool Skill::can_evolve() const {
    return evolution_level < (int)evolutions.size();
}

std::string Skill::evolve() {
    if (!can_evolve()) return "";
    evolution_level++;
    const SkillEvolution* ev = current_evolution();
    return ev ? ev->name : "";
}

const SkillEvolution* Skill::current_evolution() const {
    if (evolution_level <= 0 || evolution_level > (int)evolutions.size()) return nullptr;
    return &evolutions[evolution_level - 1];
}

std::string Skill::get_evolution_text() const {
    if (evolution_level <= 0) return "";
    const SkillEvolution* ev = current_evolution();
    if (!ev) return "Evo" + std::to_string(evolution_level);
    return ev->name;
}

// Skill 基类
Skill::Skill(const std::string& n, float cd, int ml)
    : name(n), base_cooldown(cd), cooldown(cd), max_level(ml) {}
bool Skill::can_upgrade() const { return level < max_level; }

// SkillManager
bool SkillManager::can_learn() const { return (int)active_skills.size() < max_active; }

// IronSkinSkill
int IronSkinSkill::get_value() const {
    int v[] = {0, 3, 7, 12};
    return v[std::min(level, 3)];
}
std::string IronSkinSkill::get_level_text() const {
    return "Lv" + std::to_string(level) + " DEF+" + std::to_string(get_value());
}

IronSkinSkill::IronSkinSkill(const SkillDef& def)
    : PassiveSkill(def.name, def.modifier_key, def.base_value, def.max_level) {
    _fill_common(this, def);
}

// BerserkSkill
int BerserkSkill::get_value() const {
    int v[] = {0, 3, 7, 12};
    return v[std::min(level, 3)];
}
std::string BerserkSkill::get_level_text() const {
    return "Lv" + std::to_string(level) + " ATK+" + std::to_string(get_value());
}

BerserkSkill::BerserkSkill(const SkillDef& def)
    : PassiveSkill(def.name, def.modifier_key, def.base_value, def.max_level) {
    _fill_common(this, def);
}

// ---- Skill 基类 ----
bool Skill::can_use(double game_time) const {
    return (game_time - last_use_time) >= cooldown;
}
float Skill::remaining_cooldown(double game_time) const {
    return std::max(0.0f, (float)(cooldown - (game_time - last_use_time)));
}
void Skill::mark_used(double game_time) {
    last_use_time = (float)game_time;
    use_count++;  // G1: SkillEvolutionManager 驱动
}

bool Skill::upgrade() {
    if (!can_upgrade()) return false;
    level++;
    _on_level_up();
    return true;
}

float Skill::get_power_multiplier() const {
    float m[] = {1.0f, 1.0f, 1.4f, 2.0f};
    return m[std::min(level, 3)] * (1.0f + charm_power_bonus);
}

std::string Skill::get_level_text() const {
    return "Lv" + std::to_string(level) + " x" + std::to_string(get_power_multiplier());
}

void Skill::_on_level_up() {
    base_cooldown = std::max(0.15f, base_cooldown * 0.8f);
    _recalc_cooldown();
}

void Skill::_recalc_cooldown() {
    cooldown = std::max(0.1f, base_cooldown * (1.0f - charm_cd_bonus));
}

void Skill::apply_charm(float cd, float pw) {
    charm_cd_bonus = cd; charm_power_bonus = pw;
    _recalc_cooldown();
}

void Skill::remove_charm() {
    charm_cd_bonus = 0; charm_power_bonus = 0;
    _recalc_cooldown();
}

// ---- PassiveSkill ----
int PassiveSkill::get_value() const {
    return (int)(base_value * get_power_multiplier());
}

void PassiveSkill::apply(Player* player) {
    int v = get_value();
    auto& mod = player->combat.modifiers;
    mod[modifier_key] = (mod.count(modifier_key) ? mod[modifier_key] : 0) + v;
}

void PassiveSkill::remove(Player* player) {
    auto& mod = player->combat.modifiers;
    mod[modifier_key] = mod[modifier_key] - get_value();
}

// ---- SlashSkill ----
SlashSkill::SlashSkill() : ActiveSkill("斩击", 2.0f, 3) {
    _skill_id = "slash";
    _cone_range = 1;
    triggers = {{"poison", 1, 0.30f, BuffTarget::ENEMY}};
    tags = {BuildTag::MELEE, BuildTag::COMBO};
    add_evolution(1, "广域斩", "攻击范围+30%", {BuildTag::MELEE});
    add_evolution(2, "流血斩", "100%附加1层Bleed", {BuildTag::MELEE, BuildTag::BLEED});
    add_evolution(3, "冲击斩", "Heavy: 冲击波向前延伸", {BuildTag::MELEE, BuildTag::COMBO, BuildTag::HEAVY});
}

// G3.2: 数据驱动构造器
SlashSkill::SlashSkill(const SkillDef& def) : ActiveSkill(def.name, def.cooldown, def.max_level) {
    _cone_range = 1;
    _fill_common(this, def);
    for (auto& tr : def.triggers) {
        BuffTarget bt = (tr.target == "self") ? BuffTarget::SELF : BuffTarget::ENEMY;
        triggers.push_back({tr.buff_id, tr.stacks, tr.chance, bt});
    }
}

void SlashSkill::_on_level_up() {
    if (level == 2) { base_cooldown = 1.6f; _cone_range = 2; }
    else if (level == 3) { base_cooldown = 1.2f; _cone_range = 2; }
    _recalc_cooldown();
}

std::string SlashSkill::execute(Player* caster, std::vector<Monster*>& targets,
                                 GameMap*, bool is_heavy) {
    int cr = _cone_range;
    float dmg_bonus = 1.0f;
    // D3 Step3 E1: 范围+30%
    if (evolution_level >= 1) cr = (int)(cr * 1.3f);
    // D2 Step2: Heavy → 范围+40%, 伤害+30%
    if (is_heavy) { cr = (int)(cr * 1.4f); if (cr < 2) cr = 2; dmg_bonus = 1.3f; }
    auto hit = get_targets_in_cone(caster, targets, cr);
    if (hit.empty()) return is_heavy ? "重·斩击挥空了" : "斩击挥空了（无目标）";

    std::string extra;
    for (auto* t : hit) {
        for (auto& tr : triggers) {
            if (tr.target == BuffTarget::ENEMY
                && (tr.chance >= 1.0f || (float)(rng() % 1000) / 1000.0f < tr.chance)) {
                int stacks = is_heavy ? (tr.stacks + 1) : tr.stacks;
                apply_buff(t, tr.buff_id, stacks);
                extra += " " + t->name + get_buff_display_name(tr.buff_id) + "!";
            }
        }
        // D3 Step3 E2: 100%附加 bleed
        if (evolution_level >= 2) { apply_buff(t, "bleed", 1); extra += " 流血!"; }
    }

    int total = 0;
    float base = get_effective_attack(caster) * 1.5f * dmg_bonus;
    float power = get_power_multiplier();
    int hits_per = (level >= 3) ? 2 : 1;
    for (auto* t : hit) {
        for (int i = 0; i < hits_per; i++) {
            int dmg = calculate_damage((int)(base * power),
                t->combat.get_effective_defense(AttackType::PHYSICAL));
            t->combat.take_damage(dmg);
            total += dmg;
        }
    }

    // D3 Step3 E3: Heavy冲击波 — 向前方直线敌人施加70%二次伤害
    if (evolution_level >= 3 && is_heavy) {
        float cx = caster->entity.rect.x + caster->entity.rect.width/2;
        float cy = caster->entity.rect.y + caster->entity.rect.height/2;
        for (auto* t : targets) {
            if (!t || !t->combat.is_alive) continue;
            float tx = t->entity.rect.x + t->entity.rect.width/2;
            float ty = t->entity.rect.y + t->entity.rect.height/2;
            float dx = tx - cx, dy = ty - cy; float dist = sqrtf(dx*dx+dy*dy);
            if (dist > cr * 48.0f) continue;
            // 检查是否在朝向方向
            bool in_dir = false;
            switch (caster->direction) {
                case Direction::DOWN:  in_dir = (dy > 0  && fabsf(dx) < fabsf(dy)); break;
                case Direction::UP:    in_dir = (dy < 0  && fabsf(dx) < fabsf(dy)); break;
                case Direction::RIGHT: in_dir = (dx > 0  && fabsf(dy) < fabsf(dx)); break;
                case Direction::LEFT:  in_dir = (dx < 0  && fabsf(dy) < fabsf(dx)); break;
            }
            if (in_dir) {
                int wave_dmg = (int)(calculate_damage((int)(base * power * 0.7f),
                    t->combat.get_effective_defense(AttackType::PHYSICAL)));
                t->combat.take_damage(wave_dmg);
                total += wave_dmg;
                extra += " 冲击波!";
            }
        }
    }

    std::string d = (is_heavy ? "重·" : "") + std::string("斩击Lv") + std::to_string(level)
                  + " 命中 " + std::to_string(hit.size()) + " 目标";
    if (hits_per > 1) d += "（二连击）";
    if (!extra.empty()) d += extra;
    return d + "，造成 " + std::to_string(total) + " 伤害";
}

std::string SlashSkill::get_level_text() const {
    return "Lv" + std::to_string(level) + " 范围" + std::to_string(_cone_range) + "格 " + std::to_string(cooldown).substr(0,3) + "s";
}

// ---- FireballSkill ----
FireballSkill::FireballSkill() : ActiveSkill("神罚", 5.0f, 3) {
    _skill_id = "fireball";
    triggers = {{"slow", 1, 0.25f, BuffTarget::ENEMY}};
    tags = {BuildTag::MAGIC, BuildTag::FIRE, BuildTag::PROJECTILE, BuildTag::AOE, BuildTag::RANGED};
    add_evolution(1, "爆炸火球", "半径+40%", {BuildTag::FIRE, BuildTag::AOE});
    add_evolution(2, "燃烧地面", "3秒燃烧区域持续DOT", {BuildTag::FIRE, BuildTag::DOT});
    add_evolution(3, "分裂火球", "分裂3颗小火球", {BuildTag::FIRE, BuildTag::PROJECTILE, BuildTag::AOE});
}

FireballSkill::FireballSkill(const SkillDef& def) : ActiveSkill(def.name, def.cooldown, def.max_level) {
    _fill_common(this, def);
    for (auto& tr : def.triggers) {
        BuffTarget bt = (tr.target == "self") ? BuffTarget::SELF : BuffTarget::ENEMY;
        triggers.push_back({tr.buff_id, tr.stacks, tr.chance, bt});
    }
}

void FireballSkill::_on_level_up() {
    if (level == 2) { base_cooldown = 4.0f; _target_count = 2; _range = 7.0f; }
    else if (level == 3) { base_cooldown = 3.0f; _target_count = 3; _range = 8.0f; }
    _recalc_cooldown();
}

std::string FireballSkill::execute(Player* caster, std::vector<Monster*>& targets,
                                    GameMap*, bool is_heavy) {
    std::vector<Monster*> alive;
    for (auto* t : targets) if (t && t->combat.is_alive) alive.push_back(t);

    float pcx = caster->entity.rect.x + caster->entity.rect.width/2;
    float pcy = caster->entity.rect.y + caster->entity.rect.height/2;
    std::sort(alive.begin(), alive.end(), [&](Monster* a, Monster* b) {
        float da = hypotf(a->entity.rect.x + a->entity.rect.width/2 - pcx,
                          a->entity.rect.y + a->entity.rect.height/2 - pcy);
        float db = hypotf(b->entity.rect.x + b->entity.rect.width/2 - pcx,
                          b->entity.rect.y + b->entity.rect.height/2 - pcy);
        return da < db;
    });

    float frng = is_heavy ? _range * 1.5f : _range;
    // D3 Step3 E1: 半径+40%
    if (evolution_level >= 1) frng *= 1.4f;
    float range_px = frng * 32.0f;
    std::vector<Monster*> chosen;
    for (auto* t : alive) {
        if ((int)chosen.size() >= _target_count) break;
        float d = hypotf(t->entity.rect.x + t->entity.rect.width/2 - pcx,
                         t->entity.rect.y + t->entity.rect.height/2 - pcy);
        if (d <= range_px) chosen.push_back(t);
    }
    if (chosen.empty()) return is_heavy ? "重·神罚没有击中任何目标" : "神罚没有击中任何目标";

    // G9.3: Crossbow MARKED synergy → +35% damage
    float synergy_mul = (caster->last_attack.primary_tag == AttackTag::MARKED) ? 1.35f : 1.0f;
    int total = 0;
    float base = get_effective_attack(caster) * 2.5f;
    float power = get_power_multiplier() * synergy_mul;
    for (auto* t : chosen) {
        int dmg = calculate_damage((int)(base * power),
            t->combat.get_effective_defense(AttackType::MAGICAL), AttackType::MAGICAL);
        t->combat.take_damage(dmg);
        total += dmg;
        if (is_heavy) apply_buff(t, "poison", 2);
        // D3 Step3 E2: 燃烧地面 → 命中目标+poison
        if (evolution_level >= 2) apply_buff(t, "poison", 1);
    }

    // D3 Step3 E3: 分裂火球 — 对所有非主目标敌人施加35%伤害
    if (evolution_level >= 3) {
        int split_count = 0;
        for (auto* t : alive) {
            if (split_count >= 3) break;
            bool already = false;
            for (auto* c : chosen) if (c == t) { already = true; break; }
            if (already) continue;
            float d = hypotf(t->entity.rect.x + t->entity.rect.width/2 - pcx,
                             t->entity.rect.y + t->entity.rect.height/2 - pcy);
            if (d <= range_px * 1.5f) {
                int s_dmg = (int)(calculate_damage((int)(base * power * 0.35f),
                    t->combat.get_effective_defense(AttackType::MAGICAL), AttackType::MAGICAL));
                t->combat.take_damage(s_dmg);
                total += s_dmg;
                split_count++;
            }
        }
    }

    // Buff 触发规则
    std::string extra;
    if (!chosen.empty()) {
        for (auto& tr : triggers) {
            if (tr.target == BuffTarget::ENEMY
                && (tr.chance >= 1.0f || (float)(rng() % 1000) / 1000.0f < tr.chance)) {
                int stacks = is_heavy ? (tr.stacks + 1) : tr.stacks;
                apply_buff(chosen[0], tr.buff_id, stacks);
                extra += " " + chosen[0]->name + get_buff_display_name(tr.buff_id) + "!";
            }
        }
    }
    std::string label = (is_heavy ? "重·" : "") + std::string("神罚Lv") + std::to_string(level);
    return label + " 命中 " + std::to_string(chosen.size())
           + " 目标，造成 " + std::to_string(total) + " 伤害" + extra
           + (is_heavy ? " +燃烧!" : "");
}

std::string FireballSkill::get_level_text() const {
    return "Lv" + std::to_string(level) + " " + std::to_string(_target_count) + "目标 " + std::to_string(cooldown).substr(0,3) + "s";
}

// ---- SelfHealSkill ----
SelfHealSkill::SelfHealSkill() : ActiveSkill("自愈", 8.0f, 3) {
    _skill_id = "self_heal";
    triggers = {{"attack_up", 1, 1.0f, BuffTarget::SELF}};
    tags = {BuildTag::HEAL, BuildTag::SUPPORT};
    add_evolution(1, "深度自愈", "回血量+50%", {BuildTag::HEAL});
    add_evolution(2, "战意自愈", "回血后attack_up×1", {BuildTag::HEAL, BuildTag::SUPPORT});
    add_evolution(3, "守护自愈", "回血后获得护盾", {BuildTag::HEAL, BuildTag::DEFENSE});
}

SelfHealSkill::SelfHealSkill(const SkillDef& def) : ActiveSkill(def.name, def.cooldown, def.max_level) {
    _fill_common(this, def);
    for (auto& tr : def.triggers) {
        BuffTarget bt = (tr.target == "self") ? BuffTarget::SELF : BuffTarget::ENEMY;
        triggers.push_back({tr.buff_id, tr.stacks, tr.chance, bt});
    }
}

void SelfHealSkill::_on_level_up() {
    if (level == 2) base_cooldown = 6.5f;
    else if (level == 3) base_cooldown = 5.0f;
    _recalc_cooldown();
}

std::string SelfHealSkill::execute(Player* caster, std::vector<Monster*>&, GameMap*,
                                     bool is_heavy) {
    float pcts[] = {0, 0.20f, 0.35f, 0.35f};
    float pct = pcts[std::min(level, 3)];
    // D3 Step3 E1: 回血+50%
    if (evolution_level >= 1) pct *= 1.5f;
    float power = get_power_multiplier();
    int instant = (int)(caster->combat.max_hp * pct * power);
    int recovered = caster->combat.heal(instant);
    std::string d = "自愈Lv" + std::to_string(level) + " 瞬回 " + std::to_string(recovered) + " HP";
    if (level >= 3) { _regen_left = 4.0f; d += "（+4秒持续再生）"; }
    apply_triggers_self(triggers, caster);
    if (!triggers.empty()) d += " " + get_buff_display_name(triggers[0].buff_id) + "!";
    // D2 Step2: Heavy → 额外获得 attack_up
    if (is_heavy) { apply_buff(caster, "attack_up", 1); d += " +ATK↑!"; }
    // D3 Step3 E2: 回血后额外attack_up
    if (evolution_level >= 2) { apply_buff(caster, "attack_up", 1); d += " +战意!"; }
    // D3 Step3 E3: 护盾 (数值 = 20% max_hp)
    if (evolution_level >= 3) {
        apply_buff(caster, "shield", 1);
        caster->combat.shield_hp = (float)(caster->combat.max_hp * 0.20f);
        d += " +护盾!";
    }
    return d;
}

void SelfHealSkill::tick_regen(Player* caster, float dt) {
    if (_regen_left <= 0) return;
    _regen_left -= dt;
    int heal_per = (int)(caster->combat.max_hp * 0.03f * dt);
    if (heal_per > 0) caster->combat.heal(heal_per);
}

std::string SelfHealSkill::get_level_text() const {
    std::string s = "Lv" + std::to_string(level) + " " + std::to_string(cooldown).substr(0,3) + "s";
    if (level >= 3) s += "+再生";
    return s;
}

// ---- TheWorldSkill ----
TheWorldSkill::TheWorldSkill() : ActiveSkill("The World", 20.0f, 3) {
    _skill_id = "the_world";
    tags = {BuildTag::TIME, BuildTag::SUPPORT};
    add_evolution(1, "延长时间", "停止时间+20%", {BuildTag::TIME});
    add_evolution(2, "时停冲击", "结束时释放Shockwave", {BuildTag::TIME, BuildTag::AOE});
    add_evolution(3, "时间加速", "结束后移速+25% 3秒", {BuildTag::TIME, BuildTag::SUPPORT});
}

TheWorldSkill::TheWorldSkill(const SkillDef& def) : ActiveSkill(def.name, def.cooldown, def.max_level) {
    _fill_common(this, def);
}
void TheWorldSkill::_on_level_up() {
    if (level == 2) base_cooldown = 16.0f;
    else if (level == 3) base_cooldown = 12.0f;
    _recalc_cooldown();
}
float TheWorldSkill::get_stop_duration() const {
    return _stop_duration[std::min(level, 3)] * (1.0f + charm_power_bonus);
}
std::string TheWorldSkill::execute(Player*, std::vector<Monster*>&, GameMap*,
                                    bool is_heavy) {
    (void)is_heavy; // D2 Step2: The World 暂不做 heavy 变化 (已足够强大)
    return "__TIME_STOP__";
}
std::string TheWorldSkill::get_level_text() const {
    return "Lv" + std::to_string(level) + " 停" + std::to_string((int)get_stop_duration()) + "s " + std::to_string(cooldown).substr(0,4) + "s";
}

// ---- SkillManager ----
bool SkillManager::learn(std::unique_ptr<Skill> skill) {
    if (auto* p = dynamic_cast<PassiveSkill*>(skill.get())) {
        passives.push_back(std::unique_ptr<PassiveSkill>(p));
        skill.release();
        return true;
    }
    if (auto* a = dynamic_cast<ActiveSkill*>(skill.get())) {
        if (!can_learn()) return false;
        active_skills.push_back(std::unique_ptr<ActiveSkill>(a));
        skill.release();
        return true;
    }
    return false;
}

void SkillManager::apply_all_passives(Player* player) {
    for (auto& p : passives) p->apply(player);
}

// ═══════════════════════════════════════════════════════════════
// G5.2: Signature Skills — 5 new behavior classes
// ═══════════════════════════════════════════════════════════════

// ── IceNovaSkill: AOE冻结+碎冰 ──
IceNovaSkill::IceNovaSkill() : ActiveSkill("冰爆", 6.0f, 3) {
    _skill_id = "ice_nova"; _radius = 4.0f; _shatter_bonus = 30;
    triggers = {{"freeze",1,0.30f,BuffTarget::ENEMY}};
    tags = {BuildTag::MAGIC,BuildTag::ICE,BuildTag::AOE,BuildTag::RANGED};
    add_evolution(1,"极寒","范围+30%",{BuildTag::ICE,BuildTag::AOE});
    add_evolution(2,"碎冰","对冻结目标追加伤害",{BuildTag::ICE,BuildTag::DOT});
    add_evolution(3,"暴风雪","多段AOE",{BuildTag::ICE,BuildTag::AOE,BuildTag::PROJECTILE});
}
IceNovaSkill::IceNovaSkill(const SkillDef& def) : ActiveSkill(def.name,def.cooldown,def.max_level) {
    _skill_id="ice_nova"; _radius=4.0f;_shatter_bonus=30;
    _fill_common(this,def);
    for(auto& tr:def.triggers){BuffTarget bt=(tr.target=="self")?BuffTarget::SELF:BuffTarget::ENEMY;triggers.push_back({tr.buff_id,tr.stacks,tr.chance,bt});}
}
void IceNovaSkill::_on_level_up() { if(level==2)base_cooldown=5.0f;else if(level==3)base_cooldown=4.0f;_recalc_cooldown(); }
std::string IceNovaSkill::execute(Player* caster,std::vector<Monster*>& targets,GameMap*,bool is_heavy){
    float pcx=caster->entity.rect.x+caster->entity.rect.width/2;
    float pcy=caster->entity.rect.y+caster->entity.rect.height/2;
    float radius_px=_radius*32.0f; if(evolution_level>=1)radius_px*=1.3f; if(is_heavy)radius_px*=1.5f;
    int total=0; std::string extra; float base=get_effective_attack(caster)*2.0f*get_power_multiplier();
    for(auto* t:targets){if(!t||!t->combat.is_alive)continue;
        float d=hypotf(t->entity.rect.x+t->entity.rect.width/2-pcx,t->entity.rect.y+t->entity.rect.height/2-pcy);
        if(d>radius_px)continue;
        int dmg=calculate_damage((int)base,t->combat.get_effective_defense(AttackType::MAGICAL),AttackType::MAGICAL);
        // G5.2: 碎冰 — 对已冻结目标追加伤害
        for(auto& b:t->active_buffs){if(b.id=="freeze"){dmg=dmg*(100+_shatter_bonus)/100;extra=" 碎冰!";break;}}
        t->combat.take_damage(dmg); total+=dmg;
        // G9.3: Sword BLUNT synergy → freeze guarantee
        bool sword_syn = caster->last_attack.primary_tag == AttackTag::BLUNT;
        if((rng()%1000)/1000.0f<0.30f || sword_syn) apply_buff(t,"freeze",1);
        if(is_heavy) apply_buff(t,"frostbite",1);
    }
    if(evolution_level>=3&&is_heavy){ // 暴风雪: 2nd wave
        for(auto* t:targets){if(!t||!t->combat.is_alive)continue;
            float d=hypotf(t->entity.rect.x+t->entity.rect.width/2-pcx,t->entity.rect.y+t->entity.rect.height/2-pcy);
            if(d>radius_px*1.3f)continue;
            int dmg2=calculate_damage((int)(base*0.5f),t->combat.get_effective_defense(AttackType::MAGICAL),AttackType::MAGICAL);
            t->combat.take_damage(dmg2);total+=dmg2;}}
    return "冰爆Lv"+std::to_string(level)+" 造成"+std::to_string(total)+"伤害"+extra;}
std::string IceNovaSkill::get_level_text() const { return "Lv"+std::to_string(level)+" R"+std::to_string((int)_radius)+" "+std::to_string(cooldown).substr(0,3)+"s"; }

// ── ChainLightningSkill: 弹射链 ──
ChainLightningSkill::ChainLightningSkill() : ActiveSkill("连锁闪电",4.0f,3) {
    _skill_id="chain_lightning";_bounces=3;_decay=0.30f;
    triggers={{ "electrified",1,0.25f,BuffTarget::ENEMY}};
    tags={BuildTag::MAGIC,BuildTag::LIGHTNING,BuildTag::PROJECTILE,BuildTag::AOE,BuildTag::RANGED};
    add_evolution(1,"过载","弹射+2",{BuildTag::LIGHTNING,BuildTag::AOE});
    add_evolution(2,"麻痹","首次命中+眩晕",{BuildTag::LIGHTNING,BuildTag::DOT});
    add_evolution(3,"雷暴","弹射至所有可见目标",{BuildTag::LIGHTNING,BuildTag::PROJECTILE,BuildTag::AOE});
}
ChainLightningSkill::ChainLightningSkill(const SkillDef& def):ActiveSkill(def.name,def.cooldown,def.max_level){_skill_id="chain_lightning";_bounces=3;_decay=0.30f;_fill_common(this,def);for(auto& tr:def.triggers){BuffTarget bt=(tr.target=="self")?BuffTarget::SELF:BuffTarget::ENEMY;triggers.push_back({tr.buff_id,tr.stacks,tr.chance,bt});}}
void ChainLightningSkill::_on_level_up(){if(level==2)base_cooldown=3.5f;else if(level==3)base_cooldown=2.8f;_recalc_cooldown();}
std::string ChainLightningSkill::execute(Player* caster,std::vector<Monster*>& targets,GameMap*,bool is_heavy){
    std::vector<Monster*> alive; for(auto* t:targets)if(t&&t->combat.is_alive)alive.push_back(t);
    if(alive.empty())return "连锁闪电: 无目标";
    float pcx=caster->entity.rect.x+caster->entity.rect.width/2; float pcy=caster->entity.rect.y+caster->entity.rect.height/2;
    // find closest
    Monster* closest=nullptr; float best_d=99999;
    for(auto* t:alive){float d=hypotf(t->entity.rect.x+t->entity.rect.width/2-pcx,t->entity.rect.y+t->entity.rect.height/2-pcy);if(d<best_d){best_d=d;closest=t;}}
    if(!closest)return"";
    int total=0,bounces=_bounces;if(evolution_level>=1)bounces+=2;if(is_heavy)bounces+=1;
    // G9.3: Nunchaku KNOCKBACK synergy → +2 bounces
    if(caster->last_attack.primary_tag == AttackTag::KNOCKBACK) bounces+=2;
    if(evolution_level>=3)bounces=(int)alive.size(); // 雷暴: all
    float mult=1.0f; Monster* prev=closest;
    float base=get_effective_attack(caster)*1.8f*get_power_multiplier();
    for(int i=0;i<=bounces&&prev&&prev->combat.is_alive;i++){
        int dmg=(int)(calculate_damage((int)(base*mult),prev->combat.get_effective_defense(AttackType::MAGICAL),AttackType::MAGICAL));
        prev->combat.take_damage(dmg);total+=dmg;
        if(i==0&&evolution_level>=2)apply_buff(prev,"stun",1);
        if((rng()%1000)/1000.0f<0.25f)apply_buff(prev,"electrified",1);
        mult*=(1.0f-_decay); // find next closest to prev
        Monster* next=nullptr; float nd=99999;
        for(auto* t:alive){if(t==prev)continue; float d=hypotf(t->entity.rect.x-prev->entity.rect.x,t->entity.rect.y-prev->entity.rect.y);if(d<nd){nd=d;next=t;}}
        prev=next;}return"连锁闪电Lv"+std::to_string(level)+" "+std::to_string(bounces)+"跳 造成"+std::to_string(total)+"伤害";}
std::string ChainLightningSkill::get_level_text() const { return "Lv"+std::to_string(level)+" B"+std::to_string(_bounces)+" "+std::to_string(cooldown).substr(0,3)+"s"; }

// ── ShadowStrikeSkill: 瞬移+背刺 ──
ShadowStrikeSkill::ShadowStrikeSkill() : ActiveSkill("暗影突刺",5.0f,3) {
    _skill_id="shadow_strike";_backstab_mult=2.5f;
    triggers={{ "fear",1,0.20f,BuffTarget::ENEMY}};
    tags={BuildTag::MELEE,BuildTag::TIME,BuildTag::COMBO,BuildTag::HEAVY};
    add_evolution(1,"潜行","确定瞬移后获得隐身0.5s",{BuildTag::TIME,BuildTag::MELEE});
    add_evolution(2,"暗杀术","背刺倍率+50%",{BuildTag::TIME,BuildTag::COMBO});
    add_evolution(3,"影分身","施放时留下一个分身诱饵",{BuildTag::TIME,BuildTag::SUMMON});
}
ShadowStrikeSkill::ShadowStrikeSkill(const SkillDef& def):ActiveSkill(def.name,def.cooldown,def.max_level){_skill_id="shadow_strike";_backstab_mult=2.5f;_fill_common(this,def);for(auto& tr:def.triggers){BuffTarget bt=(tr.target=="self")?BuffTarget::SELF:BuffTarget::ENEMY;triggers.push_back({tr.buff_id,tr.stacks,tr.chance,bt});}}
void ShadowStrikeSkill::_on_level_up(){if(level==2)base_cooldown=4.0f;else if(level==3)base_cooldown=3.2f;_recalc_cooldown();}
std::string ShadowStrikeSkill::execute(Player* caster,std::vector<Monster*>& targets,GameMap* map,bool is_heavy){
    std::vector<Monster*> alive;for(auto* t:targets)if(t&&t->combat.is_alive)alive.push_back(t);
    if(alive.empty())return"暗影突刺: 无目标";
    float pcx=caster->entity.rect.x+caster->entity.rect.width/2;float pcy=caster->entity.rect.y+caster->entity.rect.height/2;
    // find nearest
    Monster* t=nullptr;float bd=99999;
    for(auto* m:alive){float d=hypotf(m->entity.rect.x+m->entity.rect.width/2-pcx,m->entity.rect.y+m->entity.rect.height/2-pcy);if(d<bd){bd=d;t=m;}}
    if(!t)return"";
    // teleport behind target
    float tx=t->entity.rect.x+t->entity.rect.width/2; float ty=t->entity.rect.y+t->entity.rect.height/2;
    float dx=pcx-tx,dy=pcy-ty; float len=sqrtf(dx*dx+dy*dy);if(len<1)len=1;
    float behind_x=tx+dx/len*40.0f;float behind_y=ty+dy/len*40.0f;
    // clamp to walkable
    auto bpos=caster->entity.position;
    caster->entity.position={behind_x-caster->entity.rect.width/2,behind_y-caster->entity.rect.height/2};
    caster->entity.sync_rect();
    if(map&&!map->is_rect_walkable(caster->entity.rect)){caster->entity.position=bpos;caster->entity.sync_rect();}
    // backstab check: is player behind target relative to target's "facing"?
    // Simplified: always backstab if teleport succeeded (we teleported behind)
    // G9.3: Dagger PIERCE synergy → +30% backstab damage
    float mult=_backstab_mult;if(evolution_level>=2)mult*=1.5f;if(is_heavy)mult*=1.3f;
    if(caster->last_attack.primary_tag == AttackTag::PIERCE) mult*=1.3f;
    float base=get_effective_attack(caster)*1.8f*get_power_multiplier();
    int dmg=calculate_damage((int)(base*mult),t->combat.get_effective_defense(AttackType::PHYSICAL));
    t->combat.take_damage(dmg);
    if((rng()%1000)/1000.0f<0.20f)apply_buff(t,"fear",1);
    if(evolution_level>=1)apply_buff(caster,"shadow_veil",1);
    return"暗影突刺Lv"+std::to_string(level)+" 背刺! 造成"+std::to_string(dmg)+"伤害";}
std::string ShadowStrikeSkill::get_level_text() const { return "Lv"+std::to_string(level)+" x"+std::to_string((int)_backstab_mult)+" "+std::to_string(cooldown).substr(0,3)+"s"; }

// ── BloodFrenzySkill: 自伤→AOE流血→回血 ──
BloodFrenzySkill::BloodFrenzySkill() : ActiveSkill("血怒",7.0f,3) {
    _skill_id="blood_frenzy";_hp_cost_pct=0.10f;_heal_per_hit=0.05f;
    triggers={{ "bleed",2,0.60f,BuffTarget::ENEMY}};
    tags={BuildTag::MELEE,BuildTag::BLEED,BuildTag::AOE,BuildTag::HEAL};
    add_evolution(1,"深创","流血层数+1",{BuildTag::BLEED,BuildTag::MELEE});
    add_evolution(2,"血之收割","每命中一个目标额外回血",{BuildTag::BLEED,BuildTag::HEAL});
    add_evolution(3,"浴血","击杀流血目标→满血",{BuildTag::BLEED,BuildTag::HEAL,BuildTag::HEAVY});
}
BloodFrenzySkill::BloodFrenzySkill(const SkillDef& def):ActiveSkill(def.name,def.cooldown,def.max_level){_skill_id="blood_frenzy";_hp_cost_pct=0.10f;_heal_per_hit=0.05f;_fill_common(this,def);for(auto& tr:def.triggers){BuffTarget bt=(tr.target=="self")?BuffTarget::SELF:BuffTarget::ENEMY;triggers.push_back({tr.buff_id,tr.stacks,tr.chance,bt});}}
void BloodFrenzySkill::_on_level_up(){if(level==2)base_cooldown=5.5f;else if(level==3)base_cooldown=4.5f;_recalc_cooldown();}
std::string BloodFrenzySkill::execute(Player* caster,std::vector<Monster*>& targets,GameMap*,bool is_heavy){
    // self-damage
    int hp_cost=(int)(caster->combat.max_hp*_hp_cost_pct); if(is_heavy)hp_cost=(int)(hp_cost*1.5f);
    caster->combat.current_hp=std::max(1,caster->combat.current_hp-hp_cost);
    // AOE: all nearby enemies
    float pcx=caster->entity.rect.x+caster->entity.rect.width/2;float pcy=caster->entity.rect.y+caster->entity.rect.height/2;
    // G9.3: Spear PIERCE_STACK synergy → +50% AOE radius
    float radius_px=4.0f*32.0f; if(is_heavy)radius_px*=1.4f;
    if(caster->last_attack.primary_tag == AttackTag::PIERCE_STACK) radius_px*=1.5f;
    int total=0,hit_count=0;float base=get_effective_attack(caster)*1.5f*get_power_multiplier();
    for(auto* t:targets){if(!t||!t->combat.is_alive)continue;
        float d=hypotf(t->entity.rect.x+t->entity.rect.width/2-pcx,t->entity.rect.y+t->entity.rect.height/2-pcy);
        if(d>radius_px)continue;
        int dmg=calculate_damage((int)base,t->combat.get_effective_defense(AttackType::PHYSICAL));
        t->combat.take_damage(dmg);total+=dmg;hit_count++;
        int stacks=1;if(evolution_level>=1)stacks=2;if(is_heavy)stacks++;
        apply_buff(t,"bleed",stacks);
        if(evolution_level>=3&&t->combat.current_hp<=0)caster->combat.current_hp=caster->combat.max_hp; // 浴血
    }
    // heal per hit
    int heal_per=(int)(caster->combat.max_hp*_heal_per_hit);if(evolution_level>=2)heal_per*=2;
    int healed=caster->combat.heal(hp_cost+heal_per*hit_count);
    return"血怒Lv"+std::to_string(level)+" 消耗"+std::to_string(hp_cost)+"HP 命中"+std::to_string(hit_count)+"目标 造成"+std::to_string(total)+"伤害 回复"+std::to_string(healed)+"HP";}
std::string BloodFrenzySkill::get_level_text() const { return "Lv"+std::to_string(level)+" -"+std::to_string((int)(_hp_cost_pct*100))+"%HP "+std::to_string(cooldown).substr(0,3)+"s"; }

// ── SummonSpiritSkill: 召唤友军 ──
SummonSpiritSkill::SummonSpiritSkill() : ActiveSkill("召唤英灵",12.0f,3) {
    _skill_id="summon_spirit";_spirit_count=1;_spirit_hp=30;_spirit_atk=8;
    tags={BuildTag::SUMMON,BuildTag::SUPPORT};
    add_evolution(1,"英灵强化","召唤物HP+50% ATK+30%",{BuildTag::SUMMON,BuildTag::SUPPORT});
    add_evolution(2,"双灵","召唤2只",{BuildTag::SUMMON,BuildTag::AOE});
    add_evolution(3,"亡灵大军","召唤3只+死亡爆炸",{BuildTag::SUMMON,BuildTag::AOE,BuildTag::HEAVY});
}
SummonSpiritSkill::SummonSpiritSkill(const SkillDef& def):ActiveSkill(def.name,def.cooldown,def.max_level){_skill_id="summon_spirit";_spirit_count=1;_spirit_hp=30;_spirit_atk=8;_fill_common(this,def);}
void SummonSpiritSkill::_on_level_up(){if(level==2)base_cooldown=10.0f;else if(level==3)base_cooldown=8.0f;_recalc_cooldown();}
std::string SummonSpiritSkill::execute(Player* caster,std::vector<Monster*>& targets,GameMap* map,bool is_heavy){
    int count=_spirit_count;if(evolution_level>=2)count=2;if(evolution_level>=3)count=3;if(is_heavy)count++;
    float pcx=caster->entity.rect.x+caster->entity.rect.width/2;float pcy=caster->entity.rect.y+caster->entity.rect.height/2;
    int hp=_spirit_hp,atk=_spirit_atk;if(evolution_level>=1){hp=(int)(hp*1.5f);atk=(int)(atk*1.3f);}
    int spawned=0;
    for(int i=0;i<count;i++){
        float sx=pcx+(float)((int)rng()%80-40);float sy=pcy+(float)((int)rng()%80-40);
        auto [tx,ty]=map?map->pixel_to_tile(sx,sy):std::make_pair(0,0);
        if(map&&!map->is_walkable(tx,ty))continue;
        // Spawn a friendly monster using spawn_monster factory
        Monster* spirit=spawn_monster(sx,sy,"orc");
        if(!spirit)continue;
        spirit->combat.max_hp=hp;spirit->combat.current_hp=hp;
        spirit->combat.attack=atk;spirit->name="英灵";
        spirit->color={180,180,255,255};
        spirit->entity.sync_rect();
        targets.push_back(spirit);spawned++;
    }
    return"召唤英灵Lv"+std::to_string(level)+" 召唤"+std::to_string(spawned)+"只  HP"+std::to_string(hp)+" ATK"+std::to_string(atk);}
std::string SummonSpiritSkill::get_level_text() const { return "Lv"+std::to_string(level)+" x"+std::to_string(_spirit_count)+" "+std::to_string(cooldown).substr(0,3)+"s"; }

// ---- 工厂 ----
// G3.2: SkillFactory — 数据驱动创建
std::unique_ptr<Skill> skill_factory_create(const std::string& skill_id) {
    const SkillDef* def = get_skill_def(skill_id);
    if (!def) return nullptr;

    if (def->class_type == "slash")
        return std::make_unique<SlashSkill>(*def);
    if (def->class_type == "fireball")
        return std::make_unique<FireballSkill>(*def);
    if (def->class_type == "self_heal")
        return std::make_unique<SelfHealSkill>(*def);
    if (def->class_type == "the_world")
        return std::make_unique<TheWorldSkill>(*def);
    if (def->class_type == "ice_nova")
        return std::make_unique<IceNovaSkill>(*def);
    if (def->class_type == "chain_lightning")
        return std::make_unique<ChainLightningSkill>(*def);
    if (def->class_type == "shadow_strike")
        return std::make_unique<ShadowStrikeSkill>(*def);
    if (def->class_type == "blood_frenzy")
        return std::make_unique<BloodFrenzySkill>(*def);
    if (def->class_type == "summon_spirit")
        return std::make_unique<SummonSpiritSkill>(*def);
    if (def->class_type == "iron_skin")
        return std::make_unique<IronSkinSkill>(*def);
    if (def->class_type == "berserk")
        return std::make_unique<BerserkSkill>(*def);
    return nullptr;
}

// G3.2: random_active_skill — 从 registry 随机
std::unique_ptr<ActiveSkill> random_active_skill(const std::vector<std::string>& exclude,
                                                  bool base_only) {
    std::vector<const SkillDef*> pool;
    static const char* base_ids[] = {"slash","fireball","self_heal","the_world"};
    for (auto& kv : get_all_skill_defs()) {
        auto& d = kv.second;
        // 只选主动技能
        if (d.class_type == "slash" || d.class_type == "fireball" ||
            d.class_type == "self_heal" || d.class_type == "the_world" ||
            d.class_type == "ice_nova" || d.class_type == "chain_lightning" ||
            d.class_type == "shadow_strike" || d.class_type == "blood_frenzy" ||
            d.class_type == "summon_spirit") {
            // G9: base_only mode restricts to original 4 skills
            if (base_only) {
                bool is_base = false;
                for (auto* bid : base_ids)
                    if (d.id == std::string(bid)) { is_base = true; break; }
                if (!is_base) continue;
            }
            bool ex = false;
            for (auto& e : exclude) if (e == d.id || e == d.class_type) { ex = true; break; }
            if (!ex) pool.push_back(&d);
        }
    }
    if (pool.empty()) {
        return std::make_unique<SlashSkill>();  // fallback
    }
    auto& chosen = pool[rng() % pool.size()];
    if (chosen->class_type == "slash")     return std::make_unique<SlashSkill>(*chosen);
    if (chosen->class_type == "fireball")  return std::make_unique<FireballSkill>(*chosen);
    if (chosen->class_type == "self_heal") return std::make_unique<SelfHealSkill>(*chosen);
    if (chosen->class_type == "the_world") return std::make_unique<TheWorldSkill>(*chosen);
    if (chosen->class_type == "ice_nova")  return std::make_unique<IceNovaSkill>(*chosen);
    if (chosen->class_type == "chain_lightning") return std::make_unique<ChainLightningSkill>(*chosen);
    if (chosen->class_type == "shadow_strike")   return std::make_unique<ShadowStrikeSkill>(*chosen);
    if (chosen->class_type == "blood_frenzy")    return std::make_unique<BloodFrenzySkill>(*chosen);
    if (chosen->class_type == "summon_spirit")   return std::make_unique<SummonSpiritSkill>(*chosen);
    return std::make_unique<SlashSkill>();  // fallback
}

std::unique_ptr<Skill> random_skill(const std::vector<std::string>& exclude) {
    if ((rng() % 100) < 70)
        return random_active_skill(exclude, false);
    // 30%: 被动技能
    if ((rng() % 2) == 0) {
        const SkillDef* def = get_skill_def("iron_skin");
        return def ? std::make_unique<IronSkinSkill>(*def)
                   : std::make_unique<IronSkinSkill>();
    }
    const SkillDef* def = get_skill_def("berserk");
    return def ? std::make_unique<BerserkSkill>(*def)
               : std::make_unique<BerserkSkill>();
}

// G3.2: get_learned_names — 使用 _skill_id (替代 dynamic_cast)
std::vector<std::string> get_learned_names(const SkillManager& mgr) {
    std::vector<std::string> names;
    for (auto& s : mgr.active_skills)
        if (!s->_skill_id.empty())
            names.push_back(s->_skill_id);
    return names;
}

// ---- 锥形目标判定 ----
std::vector<Monster*> get_targets_in_cone(Player* caster,
    const std::vector<Monster*>& targets, int cone_range) {
    std::vector<Monster*> result;
    float cx = caster->entity.rect.x + caster->entity.rect.width/2;
    float cy = caster->entity.rect.y + caster->entity.rect.height/2;
    float cr = cone_range * 48.0f;  // 1.5 tiles per range unit

    for (auto* t : targets) {
        if (!t || !t->combat.is_alive) continue;
        float tx = t->entity.rect.x + t->entity.rect.width/2;
        float ty = t->entity.rect.y + t->entity.rect.height/2;
        float dx = tx - cx, dy = ty - cy;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist > cr) continue;

        // 检查是否在朝向的锥形区域
        bool in_cone = false;
        switch (caster->direction) {
            case Direction::DOWN:  in_cone = (dy > 0); break;
            case Direction::UP:    in_cone = (dy < 0); break;
            case Direction::RIGHT: in_cone = (dx > 0); break;
            case Direction::LEFT:  in_cone = (dx < 0); break;
        }
        if (in_cone) result.push_back(t);
    }
    return result;
}
