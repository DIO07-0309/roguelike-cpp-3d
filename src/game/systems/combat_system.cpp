// ============================================================
// CombatSystem - Buff + find_attack_target implementation
// ============================================================
#include "combat_system.h"
#include "player.h"
#include "monster.h"
#include "ai.h"
#include "core/logger.h"
#include <algorithm>
#include <random>
#include <cmath>
#include <unordered_map>
#include <cstdio>
#include <cstring>
#include <ctime>

// D9: MinGW std::random_device 确定性修复 — 混合时间戳
CountingRng rng;  // Q3.9: CountingRng 带掷骰计数 (构造种子由首次 seed_rng 覆盖)

// G9.3 (RNG-001): 独立视觉流 — 固定初种, 与 gameplay 流严格隔离
CountingRng visual_rng;

// G4.5: deterministic RNG seed for replay
void seed_rng(uint32_t seed) { rng.seed(seed); }

// G9.3 (RNG-001): 视觉流播种 (enter_floor 按 dungeon seed 派生, replay 稳定)
void seed_visual_rng(uint32_t seed) { visual_rng.seed(seed); }

// ---- 伤害公式 ----
int calculate_damage(int atk, int def, AttackType type) {
    // G10.2: type-aware resistance
    float def_factor = 0.5f;
    if (type == AttackType::MAGICAL) def_factor = 0.6f;  // 魔抗折损更重; 但魔抗数值普遍低于物防 (30 怪 11 个 pdef>mdef, 6/6 boss 如此), 净效果仍偏利于魔法
    if (type == AttackType::TRUE)   def_factor = 0.0f;  // true damage ignores defense
    float base = std::max(1.0f, atk - def * def_factor);
    float variance = 0.8f + (float)(rng() % 401) / 1000.0f;
    return std::max(1, (int)(base * variance));
}

// ---- Buff config table (populated by load_buff_defs) ----
static std::unordered_map<std::string, BuffDef> g_buff_defs;
static bool g_buff_defs_loaded = false;

// ============================================================
// Minimal JSON loader for buffs.json
// ============================================================

static const char* _skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static std::string _read_string(const char*& p) {
    p = _skip_ws(p);
    if (*p != '"') return "";
    p++;
    std::string out;
    while (*p && *p != '"') { out += *p; p++; }
    if (*p == '"') p++;
    return out;
}

static float _read_float(const char*& p) {
    p = _skip_ws(p);
    char* end;
    float v = (float)strtod(p, &end);
    if (end > p) p = end;
    return v;
}

static int _read_int(const char*& p) {
    p = _skip_ws(p);
    char* end;
    int v = (int)strtol(p, &end, 10);
    if (end > p) p = end;
    return v;
}

static std::string _parse_buff_obj(const char*& p, BuffDef& out) {
    p = _skip_ws(p);
    if (*p != '{') return "expected '{'";
    p++;
    while (*p) {
        p = _skip_ws(p);
        if (*p == '}') { p++; return ""; }
        if (*p == ',') { p++; continue; }

        std::string key = _read_string(p);
        p = _skip_ws(p);
        if (*p != ':') return "expected ':' after " + key;
        p++;

        if (key == "id")              out.id = _read_string(p);
        else if (key == "display_name") out.display_name = _read_string(p);
        else if (key == "short_name")   out.short_name = _read_string(p);
        else if (key == "duration")     out.duration = _read_float(p);
        else if (key == "max_stacks")   out.max_stacks = _read_int(p);
        else if (key == "tick_interval") out.tick_interval = _read_float(p);
        else if (key == "tick_damage")   out.tick_damage = _read_int(p);
        else if (key == "hud_color") {
            p = _skip_ws(p);
            if (*p != '[') return "expected '['";
            p++;
            out.hud_color_r = (unsigned char)_read_int(p);
            p = _skip_ws(p); if (*p == ',') p++;
            out.hud_color_g = (unsigned char)_read_int(p);
            p = _skip_ws(p); if (*p == ',') p++;
            out.hud_color_b = (unsigned char)_read_int(p);
            p = _skip_ws(p);
            if (*p == ']') p++;
        } else {
            p = _skip_ws(p);
            if (*p == '"') { _read_string(p); }
            else if (*p == '[') { while (*p && *p != ']') p++; if (*p == ']') p++; }
            else if (*p == '{') { int d = 1; p++; while (*p && d) { if (*p=='{') d++; if (*p=='}') d--; p++; } }
            else { while (*p && *p != ',' && *p != '}' && *p != '\n') p++; }
        }
    }
    return "unterminated";
}

static void _apply_defaults(BuffDef& d) {
    if (d.display_name.empty()) d.display_name = d.id;
    if (d.short_name.empty()) d.short_name = d.display_name;
}

bool load_buff_defs(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        LOG_ERROR("[BUFF] Cannot open %s", path.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string buf(sz + 1, '\0');
    fread(&buf[0], 1, sz, f);
    fclose(f);
    return load_buff_defs_from_json(buf.c_str(), MergeMode::Skip) > 0 || is_buff_defs_loaded();
}

const std::unordered_map<std::string, BuffDef>& get_all_buff_defs() { return g_buff_defs; }
bool is_buff_defs_loaded() { return g_buff_defs_loaded; }

// ── Post-fill BuffDef tags from id ──
static void _post_fill_buff_tags(BuffDef& def) {
    if (def.id == "poison") def.tags = {BuildTag::POISON, BuildTag::DOT};
    else if (def.id == "slow") def.tags = {BuildTag::ICE};
    else if (def.id == "attack_up") def.tags = {BuildTag::SUPPORT};
    else if (def.id == "bleed") def.tags = {BuildTag::BLEED, BuildTag::DOT};
    else if (def.id == "shield") def.tags = {BuildTag::DEFENSE};
    else if (def.id == "burn") def.tags = {BuildTag::FIRE, BuildTag::DOT};
    else if (def.id == "freeze") def.tags = {BuildTag::ICE};
    else if (def.id == "stun") def.tags = {BuildTag::LIGHTNING};
    else if (def.id == "fear") def.tags = {BuildTag::TIME};
    else if (def.id == "regen") def.tags = {BuildTag::HEAL};
    else if (def.id == "berserk") def.tags = {BuildTag::MELEE};
    else if (def.id == "stone_skin") def.tags = {BuildTag::DEFENSE};
    else if (def.id == "defense_up") def.tags = {BuildTag::DEFENSE, BuildTag::SUPPORT};
    else if (def.id == "lifesteal") def.tags = {BuildTag::BLEED, BuildTag::MELEE};
    else if (def.id == "adrenaline") def.tags = {BuildTag::MELEE, BuildTag::COMBO};
    else if (def.id == "curse") def.tags = {BuildTag::SUMMON, BuildTag::DOT};
    // ── G5.1: new buffs ──
    else if (def.id == "electrified") def.tags = {BuildTag::LIGHTNING, BuildTag::DOT};
    else if (def.id == "frostbite") def.tags = {BuildTag::ICE, BuildTag::DOT};
    else if (def.id == "deep_wound") def.tags = {BuildTag::BLEED, BuildTag::DOT, BuildTag::HEAVY};
    else if (def.id == "shadow_veil") def.tags = {BuildTag::TIME, BuildTag::DEFENSE};
    else if (def.id == "summon_bless") def.tags = {BuildTag::SUMMON, BuildTag::SUPPORT};
}

// ── G4.1: from JSON text (RegistryBuilder 驱动, 复用现有 hand-rolled 解析器) ──
int load_buff_defs_from_json(const char* json_text, MergeMode mode,
                               const char* id_namespace) {
    if (!json_text || !json_text[0]) return 0;
    std::string ns = id_namespace ? (std::string(id_namespace)+":") : "";
    const char* p = json_text;
    p = _skip_ws(p);
    if (*p != '[') return 0;
    p++;
    int count=0, skipped=0, dups=0, over=0;
    while (*p) {
        p = _skip_ws(p);
        if (*p == ']') { p++; break; }
        if (*p == ',') { p++; continue; }
        BuffDef def;
        std::string err = _parse_buff_obj(p, def);
        if (!err.empty() || def.id.empty()) { skipped++; continue; }
        if (!ns.empty()) def.id = ns + def.id;
        _apply_defaults(def);
        _post_fill_buff_tags(def);
        if (g_buff_defs.count(def.id)) {
            if (mode == MergeMode::Replace) { g_buff_defs[def.id] = def; over++; }
            else dups++;
            continue;
        }
        g_buff_defs[def.id] = def; count++;
    }
    g_buff_defs_loaded = true;
    printf("[BUFF] +%d", count); if(over>0)printf(" ~%d",over); printf("\n");
    return count+over;
}

const BuffDef* get_buff_def(const std::string& id) {
    if (g_buff_defs.empty()) return nullptr;
    auto it = g_buff_defs.find(id);
    return it != g_buff_defs.end() ? &it->second : nullptr;
}

Color get_buff_hud_color(const std::string& id) {
    const BuffDef* def = get_buff_def(id);
    if (def) return {def->hud_color_r, def->hud_color_g, def->hud_color_b, 255};
    return {200, 200, 200, 255};
}

// ---- find_attack_target ----
Monster* find_attack_target(Rectangle r, const std::vector<Monster*>& targets, float range) {
    float rpx = range * 32.0f;
    Monster* best = nullptr;
    float best_d = 1e9f;
    for (auto* t : targets) {
        if (!t || !t->combat.is_alive) continue;
        float dx = r.x + r.width/2  - t->entity.rect.x - t->entity.rect.width/2;
        float dy = r.y + r.height/2 - t->entity.rect.y - t->entity.rect.height/2;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist <= rpx && dist < best_d) { best = t; best_d = dist; }
    }
    return best;
}

// ============================================================
// Internal helpers
// ============================================================

static int _count_stacks(const std::vector<BuffInstance>& buffs, const std::string& id) {
    for (auto& b : buffs) if (b.id == id) return b.stacks;
    return 0;
}

static void _apply_impl(std::vector<BuffInstance>& buffs,
    const std::string& id, int stacks, const char* target_name,
    std::vector<BuffEvent>* events) {
    const BuffDef* def = get_buff_def(id);
    if (!def) return;
    for (auto& b : buffs) {
        if (b.id == id) {
            b.stacks = std::min(b.stacks + stacks, def->max_stacks);
            b.remaining = def->duration;
            if (def->tick_interval <= 0.0f) b.tick_timer = 0.0f;
            if (events) events->push_back({BuffEventType::APPLIED, id, target_name, b.stacks, 0});
            return;
        }
    }
    BuffInstance bi;
    bi.id = id; bi.stacks = std::min(stacks, def->max_stacks);
    bi.remaining = def->duration;
    bi.tick_timer = (def->tick_interval > 0) ? def->tick_interval : 0.0f;
    buffs.push_back(bi);
    if (events) events->push_back({BuffEventType::APPLIED, id, target_name, bi.stacks, 0});
}

static void _tick_impl(std::vector<BuffInstance>& buffs,
    CombatStats* combat, float dt, const char* target_name,
    std::vector<BuffEvent>* events) {
    size_t i = 0;
    while (i < buffs.size()) {
        auto& b = buffs[i];
        const BuffDef* def = get_buff_def(b.id);
        bool removed = false;
        b.remaining -= dt;
        if (def && def->tick_interval > 0 && combat && combat->is_alive) {
            b.tick_timer -= dt;
            while (b.tick_timer <= 0 && b.remaining > 0 && combat->is_alive) {
                int dmg = def->tick_damage * b.stacks;
                // D8: regen heals instead of damaging
                if (def->tick_damage < 0) combat->heal(-dmg);
                else {
                    combat->take_damage(dmg);
                    // M4a-fix: 玩家 DOT 掉血日志 (定位"离老远死亡"来源)
                    if (strcmp(target_name, "Player") == 0) {
                        combat->mark_damage_logged();
                        combat->last_damage_source = "dot:" + b.id;   // M2-B: 死因归因
                        LOG_INFO("[DMG] %s DOT 造成 %d 伤害 → 玩家", b.id.c_str(), dmg);
                    }
                }
                if (events) events->push_back({BuffEventType::TICK_DAMAGE, b.id, target_name, b.stacks, dmg});
                b.tick_timer += def->tick_interval;
                if (!combat->is_alive) break;
            }
        }
        if (b.remaining <= 0) removed = true;
        if (removed) {
            if (events) events->push_back({BuffEventType::EXPIRED, b.id, target_name, b.stacks, 0});
            buffs[i] = buffs.back(); buffs.pop_back();
        } else { i++; }
    }
}

// ============================================================
// Public API
// ============================================================

void apply_buff(Player* p, const std::string& id, int stacks, std::vector<BuffEvent>* events) {
    if (p && p->combat.is_alive) _apply_impl(p->active_buffs, id, stacks, "Player", events);
}
void apply_buff(Monster* m, const std::string& id, int stacks, std::vector<BuffEvent>* events) {
    if (m && m->combat.is_alive) _apply_impl(m->active_buffs, id, stacks, m->name.c_str(), events);
}

// B12: plague_mask — 玩家中毒 tick -1 (需 inline loop)
void tick_buffs(Player* p, float dt, std::vector<BuffEvent>* events) {
    if (!p || !p->combat.is_alive) return;
    bool has_plague = player_has_relic(p, "plague_mask");
    if (!has_plague) { _tick_impl(p->active_buffs, &p->combat, dt, "Player", events); return; }

    // Inline tick loop for plague_mask damage reduction + D8 regen heal
    size_t i = 0;
    auto& buffs = p->active_buffs;
    auto* combat = &p->combat;
    while (i < buffs.size()) {
        auto& b = buffs[i];
        const BuffDef* def = get_buff_def(b.id);
        bool removed = false;
        b.remaining -= dt;
        if (def && def->tick_interval > 0 && combat && combat->is_alive) {
            b.tick_timer -= dt;
            while (b.tick_timer <= 0 && b.remaining > 0 && combat->is_alive) {
                int dmg = def->tick_damage * b.stacks;
                if (b.id == "poison" || b.id == "pool_poison") { dmg -= 1; if (dmg < 0) dmg = 0; } // B12: plague_mask
                // D8: regen heals instead of damaging
                if (def->tick_damage < 0) combat->heal(-dmg);
                else combat->take_damage(dmg);
                if (events) events->push_back({BuffEventType::TICK_DAMAGE, b.id, "Player", b.stacks, dmg});
                b.tick_timer += def->tick_interval;
                if (!combat->is_alive) break;
            }
        }
        if (b.remaining <= 0) removed = true;
        if (removed) {
            if (events) events->push_back({BuffEventType::EXPIRED, b.id, "Player", b.stacks, 0});
            buffs[i] = buffs.back(); buffs.pop_back();
        } else { i++; }
    }
}
void tick_buffs(Monster* m, float dt, std::vector<BuffEvent>* events,
                const Player* relic_owner) {
    if (!m || !m->combat.is_alive) return;
    // B11: venom_fang — 玩家持有的 relic 增强怪物身上的 poison 每跳伤害
    if (relic_owner && player_has_relic(relic_owner, "venom_fang")) {
        // 手动结算 (不能直接调用 _tick_impl，需要在伤害计算处插入 venom_fang 加成)
        size_t i = 0;
        auto& buffs = m->active_buffs;
        auto* combat = &m->combat;
        while (i < buffs.size()) {
            auto& b = buffs[i];
            const BuffDef* def = get_buff_def(b.id);
            bool removed = false;
            b.remaining -= dt;
            if (def && def->tick_interval > 0 && combat && combat->is_alive) {
                b.tick_timer -= dt;
                while (b.tick_timer <= 0 && b.remaining > 0 && combat->is_alive) {
                    int dmg = def->tick_damage * b.stacks;
                    if (b.id == "poison") dmg += 1; // B11: venom_fang +1
                    combat->take_damage(dmg);
                    if (events) events->push_back({BuffEventType::TICK_DAMAGE, b.id, m->name.c_str(), b.stacks, dmg});
                    b.tick_timer += def->tick_interval;
                    if (!combat->is_alive) break;
                }
            }
            if (b.remaining <= 0) removed = true;
            if (removed) {
                if (events) events->push_back({BuffEventType::EXPIRED, b.id, m->name.c_str(), b.stacks, 0});
                buffs[i] = buffs.back(); buffs.pop_back();
            } else { i++; }
        }
    } else {
        _tick_impl(m->active_buffs, &m->combat, dt, m->name.c_str(), events);
    }
}

int get_effective_attack(const Player* p) {
    if (!p) return 1;
    int base = p->combat.get_effective_attack();
    // G9.4: buffs additive to base, relics multiply only base (not buff-inflated value)
    float buff_sum = 1.0f
        + 0.20f * _count_stacks(p->active_buffs, "attack_up")   // was 0.30
        + 0.15f * _count_stacks(p->active_buffs, "berserk")     // was 0.25
        + 0.08f * _count_stacks(p->active_buffs, "blessing")    // was 0.10
        + 0.06f * _count_stacks(p->active_buffs, "momentum")    // was 0.08
        + 0.10f * _count_stacks(p->active_buffs, "adrenaline")  // was 0.15
        - 0.12f * _count_stacks(p->active_buffs, "blind");      // was 0.15
    float atk = (float)base * buff_sum;
    // Relic multipliers (additive factor, applied to base)
    float relic_sum = 1.0f;
    if (player_has_relic(p, "war_drum")) {
        const RelicDef* def = get_relic_def("war_drum");
        relic_sum += (def ? def->param : 0.15f);  // M0.1A: JSON 为 Single Source of Truth
    }
    if (player_has_relic(p, "hunter_gloves")) relic_sum += 0.08f;
    if (player_has_relic(p, "ancient_crown")) relic_sum += 0.06f;
    if (player_has_relic(p, "dragon_heart"))  relic_sum += 0.10f;
    if (player_has_relic(p, "infinity_orb"))  relic_sum += 0.12f;
    if (player_has_relic(p, "blood_chalice")) {
        const RelicDef* def = get_relic_def("blood_chalice");
        float max_mult = def ? def->param : 0.30f;  // M0.1A: JSON 为 Single Source of Truth
        float hp_r = (float)p->combat.current_hp / get_effective_max_hp(p);
        relic_sum += (1.0f - hp_r) * max_mult;
    }
    atk *= relic_sum;
    return std::max(1, (int)atk);
}
int get_effective_attack(const Monster* m) {
    if (!m) return 1;
    int base = m->combat.get_effective_attack();
    return std::max(1, (int)(base * (1.0f + 0.3f * _count_stacks(m->active_buffs, "attack_up"))));
}

float get_effective_speed(const Player* p) {
    if (!p) return 200.0f;
    float mult = 1.0f;
    // B11: hunters_eye — 移速 +10%
    if (player_has_relic(p, "hunters_eye")) {
        const RelicDef* def = get_relic_def("hunters_eye");
        mult += (def ? def->param : 0.10f);
    }
    // D8: traveler_boots +8%, ancient_crown +5%, dragon_heart +8%
    if (player_has_relic(p, "traveler_boots")) mult += 0.08f;
    if (player_has_relic(p, "ancient_crown"))  mult += 0.05f;
    if (player_has_relic(p, "dragon_heart"))   mult += 0.08f;
    if (player_has_relic(p, "infinity_orb"))   mult += 0.10f;
    // D8: adrenaline +5%/stack
    mult += 0.05f * _count_stacks(p->active_buffs, "adrenaline");
    // D8: freeze → speed*0
    int freeze_stacks = _count_stacks(p->active_buffs, "freeze");
    if (freeze_stacks > 0) return 0.0f;
    // D8: stun/fear → speed*0.3
    if (_count_stacks(p->active_buffs, "stun") > 0 || _count_stacks(p->active_buffs, "fear") > 0)
        mult *= 0.30f;
    return p->speed * std::pow(0.7f, _count_stacks(p->active_buffs, "slow")) * mult;
}
float get_effective_speed(const Monster* m, float base_speed) {
    if (!m) return base_speed;
    return base_speed * std::pow(0.7f, _count_stacks(m->active_buffs, "slow"));
}

// B11: blood_charm — 玩家有效最大生命 = 基础值 + 20
// B12: iron_heart — +10
// D8: dragon_heart +30, ancient_crown +8
int get_effective_max_hp(const Player* p) {
    if (!p) return 0;
    int hp = p->combat.max_hp;
    if (player_has_relic(p, "blood_charm"))  hp += 20;
    if (player_has_relic(p, "iron_heart"))   hp += 10;
    if (player_has_relic(p, "dragon_heart")) hp += 30;
    if (player_has_relic(p, "ancient_crown")) hp += 8;
    if (player_has_relic(p, "infinity_orb"))  hp += 25;
    return hp;
}

// B11: 玩家侧治疗 helper — 按 effective max_hp clamp, 防止 blood_charm 时被 base max_hp 截断
void heal_player(Player* p, int amount) {
    if (!p || amount <= 0) return;
    p->combat.current_hp += amount;
    int cap = get_effective_max_hp(p);
    if (p->combat.current_hp > cap) p->combat.current_hp = cap;
}

std::string get_buff_display_name(const std::string& id) {
    const BuffDef* def = get_buff_def(id);
    return def ? def->display_name : id;
}
std::string get_buff_short_name(const std::string& id) {
    const BuffDef* def = get_buff_def(id);
    return def ? def->short_name : id;
}
std::string format_buff_time(float sec) {
    char buf[16]; snprintf(buf, sizeof(buf), "%.1fs", sec);
    return buf;
}

// ============================================================
// B11: Relic 配置表
// ============================================================
static std::unordered_map<std::string, RelicDef> g_relic_defs;
static bool g_relic_defs_loaded = false;

// 复用 combat_system.cpp 现有的 _skip_ws / _read_string / _read_float / _read_int
static std::string _parse_relic_obj(const char*& p, RelicDef& out) {
    p = _skip_ws(p);
    if (*p != '{') return "expected '{'";
    p++;
    while (*p) {
        p = _skip_ws(p);
        if (*p == '}') { p++; return ""; }
        if (*p == ',') { p++; continue; }

        std::string key = _read_string(p);
        p = _skip_ws(p);
        if (*p != ':') return "expected ':' after " + key;
        p++;

        if (key == "id")              out.id = _read_string(p);
        else if (key == "name")        out.name = _read_string(p);
        else if (key == "short_name")  out.short_name = _read_string(p);
        else if (key == "desc")        out.desc = _read_string(p);
        else if (key == "rarity")       out.rarity = _read_string(p);
        else if (key == "param")       out.param = _read_float(p);
        else if (key == "param2")      out.param2 = _read_int(p);
        else if (key == "hud_color") {
            p = _skip_ws(p); if (*p != '[') return "expected '['"; p++;
            out.hud_color_r = _read_int(p);
            p = _skip_ws(p); if (*p == ',') p++;
            out.hud_color_g = _read_int(p);
            p = _skip_ws(p); if (*p == ',') p++;
            out.hud_color_b = _read_int(p);
            p = _skip_ws(p); if (*p == ']') p++;
        } else if (key == "tags") {
            p = _skip_ws(p); if (*p == '[') p++;
            out.favorite_tags.clear();
            while (*p) {
                p = _skip_ws(p);
                if (*p == ']') { p++; break; }
                if (*p == ',') { p++; continue; }
                std::string tag = _read_string(p);
                if (tag == "melee") out.favorite_tags.push_back(BuildTag::MELEE);
                else if (tag == "ranged") out.favorite_tags.push_back(BuildTag::RANGED);
                else if (tag == "magic") out.favorite_tags.push_back(BuildTag::MAGIC);
                else if (tag == "fire") out.favorite_tags.push_back(BuildTag::FIRE);
                else if (tag == "ice") out.favorite_tags.push_back(BuildTag::ICE);
                else if (tag == "lightning") out.favorite_tags.push_back(BuildTag::LIGHTNING);
                else if (tag == "poison") out.favorite_tags.push_back(BuildTag::POISON);
                else if (tag == "bleed") out.favorite_tags.push_back(BuildTag::BLEED);
                else if (tag == "summon") out.favorite_tags.push_back(BuildTag::SUMMON);
                else if (tag == "combo") out.favorite_tags.push_back(BuildTag::COMBO);
                else if (tag == "heavy") out.favorite_tags.push_back(BuildTag::HEAVY);
                else if (tag == "aoe") out.favorite_tags.push_back(BuildTag::AOE);
                else if (tag == "projectile") out.favorite_tags.push_back(BuildTag::PROJECTILE);
                else if (tag == "heal") out.favorite_tags.push_back(BuildTag::HEAL);
                else if (tag == "time") out.favorite_tags.push_back(BuildTag::TIME);
                else if (tag == "defense") out.favorite_tags.push_back(BuildTag::DEFENSE);
                else if (tag == "support") out.favorite_tags.push_back(BuildTag::SUPPORT);
                else if (tag == "dot") out.favorite_tags.push_back(BuildTag::DOT);
            }
        } else if (key == "effects") {
            p = _skip_ws(p);
            if (*p == '[') {
                p++;
                while (*p) {
                    p = _skip_ws(p);
                    if (*p == ']') { p++; break; }
                    if (*p == ',') { p++; continue; }
                    if (*p != '{') break;
                    RelicEffectDef eff;
                    p++;
                    while (*p) {
                        p = _skip_ws(p);
                        if (*p == '}') { p++; break; }
                        if (*p == ',') { p++; continue; }
                        std::string ek = _read_string(p);
                        p = _skip_ws(p);
                        if (*p != ':') break;
                        p++;
                        if (ek == "trigger") {
                            std::string v = _read_string(p);
                            if (v == "passive") eff.trigger = RelicTrigger::PASSIVE;
                            else if (v == "on_hit") eff.trigger = RelicTrigger::ON_HIT;
                            else if (v == "on_kill") eff.trigger = RelicTrigger::ON_KILL;
                            else if (v == "pre_damage") eff.trigger = RelicTrigger::PRE_DAMAGE;
                            else if (v == "on_hurt") eff.trigger = RelicTrigger::ON_HURT;
                            else if (v == "on_floor_enter") eff.trigger = RelicTrigger::ON_FLOOR_ENTER;
                        } else if (ek == "type") {
                            std::string v = _read_string(p);
                            if (v == "modify_stat") eff.type = RelicEffectType::MODIFY_STAT;
                            else if (v == "add_buff") eff.type = RelicEffectType::ADD_BUFF;
                            else if (v == "heal") eff.type = RelicEffectType::HEAL;
                            else if (v == "deal_aoe_damage") eff.type = RelicEffectType::DEAL_AOE_DAMAGE;
                            else if (v == "damage_reduction") eff.type = RelicEffectType::DAMAGE_REDUCTION;
                        } else if (ek == "target") {
                            std::string v = _read_string(p);
                            if (v == "self") eff.target = RelicTarget::SELF;
                            else if (v == "all_enemies") eff.target = RelicTarget::ALL_ENEMIES;
                        } else if (ek == "stat") { eff.stat = _read_string(p);
                        } else if (ek == "value") { eff.value = _read_float(p);
                        } else if (ek == "value2") { eff.value2 = _read_int(p);
                        } else if (ek == "buff_id") { eff.buff_id = _read_string(p);
                        } else if (ek == "chance") { eff.chance = _read_float(p);
                        } else {
                            if (*p == '"') _read_string(p);
                            else if (*p == '[') { while (*p && *p != ']') p++; if (*p == ']') p++; }
                            else if (*p == '{') { int d=1; p++; while(*p&&d){if(*p=='{')d++;if(*p=='}')d--;p++;} }
                            else { while (*p && *p != ',' && *p != '}') p++; }
                        }
                    }
                    out.effects.push_back(eff);
                }
            }
        } else {
            p = _skip_ws(p);
            if (*p == '"') { _read_string(p); }
            else if (*p == '[') { while (*p && *p != ']') p++; if (*p == ']') p++; }
            else if (*p == '{') { int d = 1; p++; while (*p && d) { if (*p=='{') d++; if (*p=='}') d--; p++; } }
            else { while (*p && *p != ',' && *p != '}' && *p != '\n') p++; }
        }
    }
    return "unterminated";
}

static void _apply_relic_defaults(RelicDef& d) {
    if (d.name.empty())       d.name = d.id;
    if (d.short_name.empty()) d.short_name = d.name;
    if (d.rarity.empty())     d.rarity = "common";  // B12
}

bool load_relic_defs(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        LOG_ERROR("[RELIC] Cannot open %s", path.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string buf(sz + 1, '\0');
    fread(&buf[0], 1, sz, f);
    fclose(f);
    return load_relic_defs_from_json(buf.c_str(), MergeMode::Skip) > 0 || is_relic_defs_loaded();
}
#if 0
    p = _skip_ws(p);
    if (*p != '[') {
        LOG_ERROR("[RELIC] JSON root must be array");
        return false;
    }
    p++;

    int loaded = 0, skipped = 0, dups = 0;
    while (*p) {
        p = _skip_ws(p);
        if (*p == ']') { p++; break; }
        if (*p == ',') { p++; continue; }

        RelicDef def;
        std::string err = _parse_relic_obj(p, def);
        if (!err.empty()) {
            LOG_WARN("[RELIC] Parse error #%d: %s -- skip", loaded + skipped, err.c_str());
            int depth = 0;
            while (*p) {
                if (*p == '{' && !depth) depth = 1;
                else if (*p == '{') depth++;
                else if (*p == '}') { depth--; if (!depth) { p++; break; } }
                p++;
            }
            skipped++; continue;
        }
        if (def.id.empty()) {
            LOG_WARN("[RELIC] Missing id -- skip");
            skipped++; continue;
        }
        if (g_relic_defs.count(def.id)) {
            LOG_ERROR("[RELIC] Duplicate id '%s' -- skipped", def.id.c_str());
            dups++; continue;
        }
        _apply_relic_defaults(def);

        // D3 Step1: post-fill relic tags (from id)
        if (def.id == "blood_charm") def.favorite_tags = {BuildTag::DEFENSE};
        else if (def.id == "venom_fang") def.favorite_tags = {BuildTag::POISON};
        else if (def.id == "golden_dice") def.favorite_tags = {BuildTag::SUPPORT};
        else if (def.id == "hunters_eye") def.favorite_tags = {BuildTag::PROJECTILE};
        else if (def.id == "leech_blade") def.favorite_tags = {BuildTag::MELEE};
        else if (def.id == "war_drum") def.favorite_tags = {BuildTag::MELEE};
        else if (def.id == "battle_totem") def.favorite_tags = {BuildTag::MELEE};
        else if (def.id == "iron_heart") def.favorite_tags = {BuildTag::DEFENSE};
        else if (def.id == "sage_leaf") def.favorite_tags = {BuildTag::HEAL};
        else if (def.id == "merchant_coin") def.favorite_tags = {BuildTag::SUPPORT};
        else if (def.id == "plague_mask") def.favorite_tags = {BuildTag::POISON};

        LOG_INFO("[RELIC] Loaded '%s': %s param=%.2f param2=%d",
            def.id.c_str(), def.name.c_str(), def.param, def.param2);
        g_relic_defs[def.id] = def;
        loaded++;
    }

    g_relic_defs_loaded = true;
    LOG_INFO("[RELIC] Loaded %d relics, skipped %d", loaded, skipped);
    if (dups > 0) LOG_INFO("[RELIC]   (skipped %d duplicates)", dups);
    return loaded > 0;
}
#endif

int load_relic_defs_from_json(const char* json_text, MergeMode mode,
                                const char* id_namespace) {
    if (!json_text || !json_text[0]) return 0;
    std::string ns = id_namespace ? (std::string(id_namespace)+":") : "";
    const char* p = json_text;
    p = _skip_ws(p);
    if (*p != '[') return 0;
    p++;
    int count=0, skipped=0, dups=0, over=0;
    while (*p) {
        p = _skip_ws(p);
        if (*p == ']') { p++; break; }
        if (*p == ',') { p++; continue; }
        RelicDef def;
        std::string err = _parse_relic_obj(p, def);
        if (!err.empty() || def.id.empty()) { skipped++; continue; }
        if (!ns.empty()) def.id = ns + def.id;
        _apply_relic_defaults(def);
        if (g_relic_defs.count(def.id)) {
            if (mode == MergeMode::Replace) { g_relic_defs[def.id] = def; over++; }
            else dups++;
            continue;
        }
        g_relic_defs[def.id] = def; count++;
    }
    g_relic_defs_loaded = true;
    printf("[RELIC] +%d", count); if(over>0)printf(" ~%d",over); printf("\n");
    return count+over;
}

const std::unordered_map<std::string, RelicDef>& get_all_relic_defs() { return g_relic_defs; }
bool is_relic_defs_loaded() { return g_relic_defs_loaded; }

const RelicDef* get_relic_def(const std::string& id) {
    auto it = g_relic_defs.find(id);
    return it != g_relic_defs.end() ? &it->second : nullptr;
}

bool player_has_relic(const Player* p, const std::string& id) {
    if (!p) return false;
    for (auto& r : p->relics)
        if (r.id == id) return true;
    return false;
}

std::vector<std::string> get_all_relic_ids() {
    std::vector<std::string> ids;
    for (auto& kv : g_relic_defs) ids.push_back(kv.first);
    return ids;
}

std::string get_relic_display_name(const std::string& id) {
    const RelicDef* def = get_relic_def(id);
    return def ? def->name : id;
}

std::string get_relic_short_name(const std::string& id) {
    const RelicDef* def = get_relic_def(id);
    return def ? def->short_name : id;
}

Color get_relic_hud_color(const std::string& id) {
    const RelicDef* def = get_relic_def(id);
    if (def) return {(unsigned char)def->hud_color_r, (unsigned char)def->hud_color_g, (unsigned char)def->hud_color_b, 255};
    return {200, 200, 200, 255};
}

// ---- BuffTrigger helpers ----
void apply_triggers(const std::vector<BuffTrigger>& triggers, Player* self, Monster* enemy) {
    for (auto& tr : triggers) {
        if (tr.target != BuffTarget::ENEMY) continue;
        if (tr.chance < 1.0f && (float)(rng() % 1000) / 1000.0f >= tr.chance) continue;
        apply_buff(enemy, tr.buff_id, tr.stacks);
    }
}
void apply_triggers_self(const std::vector<BuffTrigger>& triggers, Player* self) {
    for (auto& tr : triggers) {
        if (tr.target == BuffTarget::ENEMY) continue;
        if (tr.chance < 1.0f && (float)(rng() % 1000) / 1000.0f >= tr.chance) continue;
        apply_buff(self, tr.buff_id, tr.stacks);
    }
}
