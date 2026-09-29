#include "save_manager.h"
#include "player.h"
#include "skill.h"
#include "item.h"
#include "combat_stats.h"
#include "core/logger.h"
#include "config.h"
#include "combat_system.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define mkdir_impl(p) _mkdir(p)
#else
#include <sys/stat.h>
#define mkdir_impl(p) mkdir(p, 0755)
#endif

std::string SaveManager::_save_dir() { return "saves"; }
// G10.9-B2: 槽位路径 saves/slot_N.json (N = 1..3)
std::string SaveManager::_slot_path(int slot_id) {
    return _save_dir() + "/slot_" + std::to_string(slot_id) + ".json";
}
// B3 迁移专用: 旧单槽路�?
std::string SaveManager::_legacy_save_path() { return _save_dir() + "/save.json"; }

bool SaveManager::g_sim_readonly = false;  // Q3.1: --sim 只读

// �?G10.9-B2: 活跃槽位 (进程内状�? 进游戏前由菜单设�? �?
static int g_active_slot = 1;
int  SaveManager::active_slot() { return g_active_slot; }
void SaveManager::set_active_slot(int slot_id) {
    if (slot_id >= 1 && slot_id <= SAVE_SLOT_COUNT) g_active_slot = slot_id;
}

bool SaveManager::slot_exists(int slot_id) {
    FILE* f = fopen(_slot_path(slot_id).c_str(), "rb");
    if (!f) return false; fclose(f); return true;
}

// 旧接口兼�? 探测活跃�?(迁移�?save.json 不再使用)
bool SaveManager::save_exists() { return slot_exists(active_slot()); }

// ---- 辅助: trim ----
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

// ---- M4e: float 列表解析 (前置声明, 定义在文件末) ----
static void _parse_float_list(const std::string& s, std::vector<float>& out);

// ---- B8: spr 序列化 (前置声明, 定义在文件末) ----
static std::string _encode_spr(const std::vector<bool>& v);
static std::vector<bool> _decode_spr(const std::string& s);

// ---- G13: load_game 拆分用的行级访问器 ("key:value" 格式) ----
struct SaveTokens {
    std::vector<std::string> lines;
    int getV(const char* key, int def = 0) const {
        const std::string prefix = std::string(key) + ":";
        for (const auto& l : lines)
            if (l.compare(0, prefix.size(), prefix) == 0)
                return atoi(l.c_str() + prefix.size());
        return def;
    }
    std::string getS(const char* key) const {
        const std::string prefix = std::string(key) + ":";
        for (const auto& l : lines)
            if (l.compare(0, prefix.size(), prefix) == 0) return l.substr(prefix.size());
        return "";
    }
};

// 读整个存档文件为行列表 (跳过空行)
static SaveTokens _read_save_lines(const std::string& path) {
    SaveTokens tk;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return tk;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        std::string line = trim(buf);
        if (!line.empty()) tk.lines.push_back(line);
    }
    fclose(f);
    return tk;
}

// "a,b,c" -> ["a","b","c"]
static void _split_csv(const std::string& s, std::vector<std::string>& out) {
    out.clear();
    for (size_t i = 0, last = 0; i <= s.size(); i++) {
        if (i == s.size() || s[i] == ',') {
            out.push_back(s.substr(last, i - last));
            last = i + 1;
        }
    }
}

// G3.2: 旧档 skill 名映射 ("Slash"->"slash" ...)
static std::string _map_old_skill_name(const std::string& nm) {
    if (nm == "Slash")     return "slash";
    if (nm == "Fireball")  return "fireball";
    if (nm == "SelfHeal")  return "self_heal";
    if (nm == "TheWorld")  return "the_world";
    if (nm == "IronSkin")  return "iron_skin";
    if (nm == "Berserk")   return "berserk";
    return nm;   // G3.2+ 新格式已是正确 id
}

// 单条 "name,lv,evo,use" -> Skill (格式不符/未知 id 返回 nullptr)
static std::unique_ptr<Skill> _parse_skill_token(const std::string& tok) {
    const size_t comma1 = tok.find(',');
    if (comma1 == std::string::npos) return nullptr;
    const std::string nm = _map_old_skill_name(tok.substr(0, comma1));
    const size_t comma2 = tok.find(',', comma1 + 1);
    const std::string lvStr = (comma2 != std::string::npos)
        ? tok.substr(comma1 + 1, comma2 - comma1 - 1) : tok.substr(comma1 + 1);
    int lvl = atoi(lvStr.c_str());
    int evo = 0, use = 0;
    int commas = 0;
    for (char c : tok) if (c == ',') commas++;
    if (commas >= 3 && comma2 != std::string::npos) {
        const size_t comma3 = tok.find(',', comma2 + 1);
        const std::string evoStr = (comma3 != std::string::npos)
            ? tok.substr(comma2 + 1, comma3 - comma2 - 1) : tok.substr(comma2 + 1);
        evo = atoi(evoStr.c_str());
        if (comma3 != std::string::npos) use = atoi(tok.substr(comma3 + 1).c_str());
    }
    std::unique_ptr<Skill> sk = skill_factory_create(nm);
    if (!sk) return nullptr;
    while (sk->level < lvl) sk->upgrade();
    sk->evolution_level = evo;
    sk->use_count = use;
    return sk;
}

// "name,lv,evo,use;..." (act / pas 同格式, 合并为一份解析)
static void _parse_skill_list(const SaveTokens& tk, const char* key, Player* p) {
    const std::string list = tk.getS(key);
    for (size_t pos = 0; pos < list.size(); ) {
        const size_t semi = list.find(';', pos);
        if (semi == std::string::npos) break;
        const std::string tok = list.substr(pos, semi - pos);
        pos = semi + 1;
        std::unique_ptr<Skill> sk = _parse_skill_token(tok);
        if (!sk) continue;
        p->skills.learn(std::move(sk));
    }
}

// Batch 3A: RUN relics "id,scope;..."
static void _parse_relics(const SaveTokens& tk, Player* p) {
    const std::string list = tk.getS("rlc");
    for (size_t pos = 0; pos < list.size(); ) {
        const size_t semi = list.find(';', pos);
        const std::string tok = list.substr(
            pos, (semi != std::string::npos ? semi - pos : std::string::npos));
        pos = (semi != std::string::npos) ? semi + 1 : std::string::npos;
        if (tok.empty()) break;
        const size_t comma = tok.find(',');
        if (comma == std::string::npos) continue;
        const std::string rid = tok.substr(0, comma);
        const int scope_val = std::atoi(tok.substr(comma + 1).c_str());
        if (scope_val == static_cast<int>(PersistenceScope::RUN) && !rid.empty())
            if (get_relic_def(rid)) p->add_relic(rid, PersistenceScope::RUN);
    }
}

// "name,RARITY,type,v1,v2,v3[,wpn_id];..."
static void _parse_inventory(const SaveTokens& tk, Player* p) {
    const std::string list = tk.getS("inv");
    for (size_t pos = 0; pos < list.size(); ) {
        const size_t semi = list.find(';', pos);
        if (semi == std::string::npos) break;
        const std::string tok = list.substr(pos, semi - pos);
        pos = semi + 1;

        std::vector<std::string> parts;
        _split_csv(tok, parts);
        if (parts.size() < 3) continue;
        const std::string nm = parts[0];
        const Rarity rar = (Rarity)atoi(parts[1].c_str());
        const std::string typ = parts[2];

        if ((typ == "heal" || typ == "buff") && parts.size() >= 4) {
            const std::string buf = (parts.size() >= 5) ? parts[4] : "";
            p->inventory.items.push_back(
                std::make_shared<ConsumableItem>(nm, rar, typ, atoi(parts[3].c_str()), buf));
        } else if (parts.size() >= 6) {
            auto ei = std::make_shared<EquipmentItem>(
                nm, rar, typ, atoi(parts[3].c_str()), atoi(parts[4].c_str()),
                atoi(parts[5].c_str()), false);
            if (parts.size() >= 7 && !parts[6].empty()) ei->weapon_def_id = parts[6];
            p->inventory.items.push_back(ei);
        }
    }
}

// "name,rarity,type,atk,pdef,mdef"
static std::shared_ptr<EquipmentItem> _parse_equip_line(const std::string& s) {
    if (s.empty()) return nullptr;
    std::vector<std::string> parts;
    _split_csv(s, parts);
    if (parts.size() < 6) return nullptr;
    return std::make_shared<EquipmentItem>(
        parts[0], (Rarity)atoi(parts[1].c_str()), parts[2],
        atoi(parts[3].c_str()), atoi(parts[4].c_str()), atoi(parts[5].c_str()),
        false);
}

// eqw / eqa / wpn 三行: 装备应用到玩家并回填 inventory.equipped
static void _parse_equipment(const SaveTokens& tk, Player* p) {
    auto eqw = _parse_equip_line(tk.getS("eqw"));
    if (eqw) { eqw->apply(p); p->inventory.equipped["weapon"] = eqw; }
    auto eqa = _parse_equip_line(tk.getS("eqa"));
    if (eqa) { eqa->apply(p); p->inventory.equipped["armor"] = eqa; }

    const std::string wpn_id = tk.getS("wpn");
    if (wpn_id.empty()) return;
    if (eqw) eqw->weapon_def_id = wpn_id;
    p->weapon.equip(wpn_id);
}

// "id,stacks,remaining,tick_timer;..." (过期/空条目标记跳过并计入统计)
static void _parse_buffs(const SaveTokens& tk, Player* p) {
    const std::string list = tk.getS("buf");
    int restored = 0, skipped = 0;
    for (size_t pos = 0; pos < list.size(); ) {
        const size_t semi = list.find(';', pos);
        if (semi == std::string::npos) break;
        const std::string tok = list.substr(pos, semi - pos);
        pos = semi + 1;

        std::vector<std::string> parts;
        _split_csv(tok, parts);
        if (parts.size() < 4) {
            LOG_WARN("[BUF] 读档跳过坏条目: %s", tok.c_str());
            skipped++; continue;
        }
        const std::string id = parts[0];
        if (id.empty()) { skipped++; continue; }
        BuffInstance bi;
        bi.id = id;
        bi.stacks     = atoi(parts[1].c_str());
        bi.remaining  = (float)atof(parts[2].c_str());
        bi.tick_timer = (float)atof(parts[3].c_str());
        if (bi.stacks <= 0 || bi.remaining <= 0) { skipped++; continue; }
        p->active_buffs.push_back(bi);
        restored++;
    }
    if (restored > 0 || skipped > 0)
        LOG_INFO("读档Buff: 恢复%d 跳过%d", restored, skipped);
}

// "qid=state;qid=state;..."
static void _parse_quest_states(const SaveTokens& tk, SaveData* d) {
    const std::string list = tk.getS("qst");
    for (size_t pos = 0; pos < list.size(); ) {
        const size_t semi = list.find(';', pos);
        const std::string tok = list.substr(
            pos, (semi != std::string::npos ? semi - pos : std::string::npos));
        pos = (semi != std::string::npos ? semi + 1 : list.size());
        const size_t eq = tok.find('=');
        if (eq != std::string::npos)
            d->quest_states[atoi(tok.substr(0, eq).c_str())] =
                atoi(tok.substr(eq + 1).c_str());
    }
}

// "key=val;key=val;..."
static void _parse_rule_counters(const SaveTokens& tk, SaveData* d) {
    const std::string list = tk.getS("rul");
    for (size_t pos = 0; pos < list.size(); ) {
        const size_t semi = list.find(';', pos);
        const std::string tok = list.substr(
            pos, (semi != std::string::npos ? semi - pos : std::string::npos));
        pos = (semi != std::string::npos ? semi + 1 : list.size());
        const size_t eq = tok.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = tok.substr(0, eq);
        if (!key.empty()) d->rule_counters[key] = atoi(tok.substr(eq + 1).c_str());
    }
}

// G10.1: "type,level,exp,init" (Player 与 SaveData 双写)
static void _parse_element(const SaveTokens& tk, Player* p, SaveData* d) {
    const std::string elem = tk.getS("elem");
    if (elem.empty()) return;

    int parts[4] = {0, 1, 0, 0};
    for (int i = 0, pi = 0; i < (int)elem.size() && pi < 4; ) {
        const int comma = (int)elem.find(',', i);
        const std::string tok = elem.substr(i, (comma < 0 ? (int)elem.size() : comma) - i);
        if (!tok.empty()) parts[pi] = atoi(tok.c_str());
        i = (comma < 0 ? (int)elem.size() : comma + 1);
        pi++;
    }
    static const ElementType map[] = {
        ElementType::NONE, ElementType::FIRE, ElementType::ICE, ElementType::POISON
    };
    const int et = (parts[0] >= 0 && parts[0] < 4) ? parts[0] : 0;
    p->element.type        = map[et];
    p->element.level       = std::max(1, parts[1]);
    p->element.experience  = std::max(0, parts[2]);
    p->element.initialized = (parts[3] != 0);
    d->element_type        = parts[0];
    d->element_level       = parts[1];
    d->element_exp         = parts[2];
    d->element_initialized = (parts[3] != 0);
}

// ---- G13: save_game 分段序列化 (key:value 行序即存档格式, 仅拆函数不改字节) ----
// G10.9-B2: v:5 是首个真正被读取的版本号; 新增 time: 字段
static void _write_player_base(FILE* f, Player* p, int floor, int max_f, float play_time) {
    auto& c = p->combat;
    fprintf(f, "v:5\n");
    fprintf(f, "floor:%d\n", floor);
    fprintf(f, "maxf:%d\n", max_f);
    fprintf(f, "lv:%d\n", p->level);
    fprintf(f, "xp:%d\n", p->xp);
    fprintf(f, "xpt:%d\n", p->xp_to_next);
    fprintf(f, "mhp:%d\n", c.max_hp);
    fprintf(f, "chp:%d\n", c.current_hp);
    fprintf(f, "atk:%d\n", c.attack);           // 基础值（每次升级+2）
    fprintf(f, "pd:%d\n", c.physical_defense);
    fprintf(f, "md:%d\n", c.magical_defense);
    fprintf(f, "gld:%d\n", p->gold);
    fprintf(f, "key:%d\n", p->key_count);
    fprintf(f, "time:%.0f\n", play_time);       // G10.9-B2: 本档累计时长(秒)
}

// 主动/被动技能 "id,lv,evo,use;..." (G3.2: _skill_id 替代 dynamic_cast)
static void _write_skills(FILE* f, Player* p) {
    fprintf(f, "act:");
    for (auto& s : p->skills.active_skills) {
        const char* nm = s->_skill_id.empty() ? "slash" : s->_skill_id.c_str();
        fprintf(f, "%s,%d,%d,%d;", nm, s->level, s->evolution_level, s->use_count);
    }
    fprintf(f, "\n");
    fprintf(f, "pas:");
    for (auto& s : p->skills.passives) {
        const char* nm = s->_skill_id.empty() ? "iron_skin" : s->_skill_id.c_str();
        fprintf(f, "%s,%d,%d,%d;", nm, s->level, s->evolution_level, s->use_count);
    }
    fprintf(f, "\n");
}

// 背包物品 "name,RARITY,type,val1,val2,val3[,wpn_id];..." (G9: wpn_id 追加在武器后)
static void _write_inventory(FILE* f, Inventory& inv) {
    fprintf(f, "inv:");
    for (auto& item : inv.items) {
        auto* eq = dynamic_cast<EquipmentItem*>(item.get());
        auto* cn = dynamic_cast<ConsumableItem*>(item.get());
        if (eq && eq->slot != "charm") {
            fprintf(f, "%s,%d,%s,%d,%d,%d",
                eq->base_name.c_str(), (int)eq->rarity, eq->slot.c_str(),
                eq->atk_bonus, eq->pdef_bonus, eq->mdef_bonus);
            if (!eq->weapon_def_id.empty())
                fprintf(f, ",%s", eq->weapon_def_id.c_str());
            fprintf(f, ";");
        } else if (eq) {   // charm
            fprintf(f, "%s,%d,charm,0,0,0;", eq->base_name.c_str(), (int)eq->rarity);
        } else if (cn) {
            fprintf(f, "%s,%d,%s,%d,%s;",
                cn->base_name.c_str(), (int)cn->rarity, cn->effect_type.c_str(),
                cn->effect_value, cn->buff_id.c_str());
        }
    }
    fprintf(f, "\n");
}

// 装备三行: eqw / wpn / eqa
static void _write_equipment(FILE* f, Inventory& inv) {
    fprintf(f, "eqw:");
    if (inv.equipped["weapon"]) {
        auto& eq = inv.equipped["weapon"];
        fprintf(f, "%s,%d,%s,%d,%d,%d", eq->base_name.c_str(), (int)eq->rarity,
                eq->slot.c_str(), eq->atk_bonus, eq->pdef_bonus, eq->mdef_bonus);
    }
    fprintf(f, "\n");
    fprintf(f, "wpn:%s\n",   // G9: weapon_def_id for equipped weapon
        inv.equipped["weapon"] && !inv.equipped["weapon"]->weapon_def_id.empty()
            ? inv.equipped["weapon"]->weapon_def_id.c_str() : "");
    fprintf(f, "eqa:");
    if (inv.equipped["armor"]) {
        auto& eq = inv.equipped["armor"];
        fprintf(f, "%s,%d,%s,%d,%d,%d", eq->base_name.c_str(), (int)eq->rarity,
                eq->slot.c_str(), eq->atk_bonus, eq->pdef_bonus, eq->mdef_bonus);
    }
    fprintf(f, "\n");
}

// Buff 状态 (玩家) "id,stacks,remaining,tick_timer;..."
static void _write_buffs(FILE* f, const std::vector<BuffInstance>& buffs) {
    fprintf(f, "buf:");
    for (auto& b : buffs)
        fprintf(f, "%s,%d,%.2f,%.2f;", b.id.c_str(), b.stacks, b.remaining, b.tick_timer);
    fprintf(f, "\n");
}

// B8: 特殊房间状态 + Batch 3A: RUN relics
static void _write_special_and_relics(FILE* f, uint32_t dungeon_seed,
                                      const std::vector<bool>& special_triggered,
                                      const std::vector<bool>& special_discovered,
                                      const std::vector<RelicInstance>& relics) {
    fprintf(f, "seed:%u\n", dungeon_seed);
    fprintf(f, "spr:%s\n", _encode_spr(special_triggered).c_str());
    fprintf(f, "spd:%s\n", _encode_spr(special_discovered).c_str());
    fprintf(f, "rlc:");
    for (auto& r : relics)
        if (r.scope == PersistenceScope::RUN)
            fprintf(f, "%s,%d;", r.id.c_str(), static_cast<int>(r.scope));
    fprintf(f, "\n");
}

// G1 Step7: 普攻进化 + 规则计数 (rul:key=val;...) + G2.4: 任务状态 (qst:id=state;...)
static void _write_rules_and_quests(FILE* f, Player* p,
                                    const std::unordered_map<std::string, int>& rule_counters,
                                    const std::unordered_map<int, int>& quest_states) {
    fprintf(f, "atl:%d\n", p->attack_evo.level);
    fprintf(f, "rul:");
    if (!rule_counters.empty()) {
        bool first = true;
        for (auto& kv : rule_counters) {
            if (!first) fprintf(f, ";");
            fprintf(f, "%s=%d", kv.first.c_str(), kv.second);
            first = false;
        }
    }
    fprintf(f, "\n");
    fprintf(f, "qst:");
    if (!quest_states.empty()) {
        bool first = true;
        for (auto& kv : quest_states) {
            if (!first) fprintf(f, ";");
            fprintf(f, "%d=%d", kv.first, kv.second);
            first = false;
        }
    }
    fprintf(f, "\n");
}

// G10.1: 元素核心 + M4e: 跨对局镜像记忆
static void _write_element_and_mirror(FILE* f, Player* p,
                                      const std::vector<float>& mra,
                                      const std::vector<float>& mrb) {
    fprintf(f, "elem:%d,%d,%d,%d\n",   // M4b-fix: 存 int 而非名字
        (int)p->element.type, p->element.level, p->element.experience,
        p->element.initialized ? 1 : 0);

    // G10.9-B2: end: 行已移除 — unlocked_endings 迁移 meta_save.json (账号级),
    // 解锁时由 EndingDirector 调 MetaSystem::unlock_ending 立即落盘
    if (mra.empty() || mrb.empty()) return;
    fprintf(f, "mra:");
    for (size_t i = 0; i < mra.size(); i++) {
        if (i > 0) fprintf(f, ",");
        fprintf(f, "%.4f", mra[i]);
    }
    fprintf(f, "\n");
    fprintf(f, "mrb:");
    for (size_t i = 0; i < mrb.size(); i++) {
        if (i > 0) fprintf(f, ",");
        fprintf(f, "%.4f", mrb[i]);
    }
    fprintf(f, "\n");
}

// G20b: .tmp 写完之后收尾 —— 旧档备份成 .bak, 再把 .tmp 提升为正式槽。
// 任一步失败都回滚, 保证正式槽永远不是半截内容。
// 用 fopen 探测而非 slot_exists (本函数是文件内 free static, 不能碰成员函数)。
static bool _install_slot_write(int slot_id, const std::string& cur) {
    const std::string bak = cur + ".bak";
    const std::string tmp = cur + ".tmp";
    remove(bak.c_str());
    FILE* probe = fopen(cur.c_str(), "rb");
    const bool had_old = (probe != nullptr);
    if (probe) fclose(probe);
    if (had_old && rename(cur.c_str(), bak.c_str()) != 0) {
        remove(tmp.c_str());
        LOG_ERROR("存档备份失败 (slot %d), 放弃写入", slot_id);
        return false;
    }
    if (rename(tmp.c_str(), cur.c_str()) == 0) return true;
    remove(cur.c_str());
    if (had_old) (void)rename(bak.c_str(), cur.c_str());
    LOG_ERROR("存档写入失败 (slot %d), 已保留旧档", slot_id);
    return false;
}

// ---- 序列化 ----
// G13: 各段序列化拆到文件内 static 辅助, save_game 只做编排 (调用顺序即存档行序)
bool SaveManager::save_game(int slot_id, Player* player, int floor, int max_f,
                              uint32_t dungeon_seed,
                              const std::vector<bool>& special_triggered,
                              const std::vector<bool>& special_discovered,
                              const std::unordered_map<std::string, int>& rule_counters,
                              const std::unordered_map<int, int>& quest_states,
                              const std::vector<float>& mirror_prior_alpha,
                              const std::vector<float>& mirror_prior_beta,
                              float play_time) {
    if (g_sim_readonly) return false;  // Q3.1: sim 模式不写玩家存档
    if (slot_id < 1 || slot_id > SAVE_SLOT_COUNT) return false;
    mkdir_impl(_save_dir().c_str());
    // G20b: 先写 .tmp 暂存, 成功后由 _install_slot_write 做「旧档->.bak + .tmp->正式槽」。
    // 中途任一步失败都保留写前内容, 不再出现覆盖后无法回退的半截存档。
    const std::string tmp_path = _slot_path(slot_id) + ".tmp";
    FILE* f = fopen(tmp_path.c_str(), "wb");
    if (!f) { LOG_ERROR("存档无法写入 (slot %d)", slot_id); return false; }
    auto& inv = player->inventory;

    _write_player_base(f, player, floor, max_f, play_time);
    _write_skills(f, player);
    _write_inventory(f, inv);
    _write_equipment(f, inv);
    _write_buffs(f, player->active_buffs);
    _write_special_and_relics(f, dungeon_seed, special_triggered,
                              special_discovered, player->relics);
    _write_rules_and_quests(f, player, rule_counters, quest_states);
    _write_element_and_mirror(f, player, mirror_prior_alpha, mirror_prior_beta);

    bool write_ok = true;
    if (ferror(f)) write_ok = false;
    if (fclose(f) != 0) write_ok = false;
    if (!write_ok) {
        remove(tmp_path.c_str());
        LOG_ERROR("存档写入中断 (slot %d), 已保留旧档", slot_id);
        return false;
    }
    if (!_install_slot_write(slot_id, _slot_path(slot_id))) return false;
    LOG_INFO("存档: 第%d层 Lv%d HP:%d/%d %zu技能 %zu物品 %zuBuff seed:%u",
        floor, player->level, player->combat.current_hp, player->combat.max_hp,
        player->skills.active_skills.size(), inv.items.size(),
        player->active_buffs.size(), dungeon_seed);
    return true;
}

// G13: 基础数值 -> Player 骨架 (构造参数 6 个, 数值字段 6 个)
static std::unique_ptr<Player> _build_player_from(const SaveTokens& tk) {
    const int lv  = tk.getV("lv", 1);
    const int mhp = tk.getV("mhp", PLAYER_MAX_HP);
    auto p = std::make_unique<Player>(
        TILE_SIZE * 2, TILE_SIZE * 2, PLAYER_SPEED, mhp,
        tk.getV("atk", PLAYER_ATTACK), tk.getV("pd", PLAYER_PDEF),
        tk.getV("md", PLAYER_MDEF));
    p->combat.current_hp = tk.getV("chp", mhp);
    p->level = lv;
    p->xp = tk.getV("xp", 0);
    p->xp_to_next = tk.getV("xpt", Player::calc_xp_for_level(lv + 1));
    p->gold = tk.getV("gld", 0);
    p->key_count = tk.getV("key", 0);
    return p;
}

// G13: SaveData 元数据填充 (Player 由调用方挂载, element 单独解析)
static void _fill_save_meta(const SaveTokens& tk, SaveData* d, int floor, int maxf) {
    d->current_floor = floor;
    d->max_unlocked_floor = maxf;
    d->dungeon_seed = (uint32_t)tk.getV("seed", 0);
    d->special_triggered = _decode_spr(tk.getS("spr"));
    d->special_discovered = _decode_spr(tk.getS("spd"));
    _parse_quest_states(tk, d);
    _parse_rule_counters(tk, d);
    _parse_float_list(tk.getS("mra"), d->mirror_prior_alpha);
    _parse_float_list(tk.getS("mrb"), d->mirror_prior_beta);
    d->attack_evo_level = tk.getV("atl", 1);
    d->play_time = (float)tk.getV("time", 0);   // G10.9-B2: v<5 老档无此行 -> 0
}

// ---- 反序列化 ----
// G10.9-B2: 槽位化读取 — load_game(slot) 为新入口, load_save() 转发活跃槽
// G13: 各字段解析拆到文件内 static 辅助, load_game 只做编排 (顺序即语义)
SaveData* SaveManager::load_game(int slot_id) {
    if (!slot_exists(slot_id)) return nullptr;
    // G19-fix: 读档即绑定活跃槽。旧实现只在 NEW_GAME 分支调 set_active_slot,
    // CONTINUE / --autocontinue 两条读档路径漏了 → 读档 A 之后任何
    // save_game(active_slot()) 都写进另一个槽, 把它覆盖掉 (用户深档被清)。
    // 收口在 load_game 一处, 三条读档路径同时修齐, 后续新增路径不会漏。
    set_active_slot(slot_id);

    const SaveTokens tk = _read_save_lines(_slot_path(slot_id));

    // G10.9-B2: 版本号真正参与加载 — v<5 老档无 time: 行, getV 默认 0 兜底;
    // 后续 v6+ 迁移在此分支处理
    const int save_version = tk.getV("v", 4);
    (void)save_version;

    const int floor = tk.getV("floor", 1);
    const int maxf  = tk.getV("maxf", 1);
    const int lv    = tk.getV("lv", 1);
    const int mhp   = tk.getV("mhp", PLAYER_MAX_HP);
    const int chp   = tk.getV("chp", mhp);

    auto p = _build_player_from(tk);
    _parse_relics(tk, p.get());
    _parse_skill_list(tk, "act", p.get());   // 主动 (G3.2: SkillFactory)
    _parse_skill_list(tk, "pas", p.get());   // 被动 (G3.2: SkillFactory)
    p->attack_evo.level = tk.getV("atl", 1); // G1 Step7 (须早于 apply_all_passives)
    p->skills.apply_all_passives(p.get());
    _parse_inventory(tk, p.get());
    _parse_equipment(tk, p.get());
    _parse_buffs(tk, p.get());

    auto* d = new SaveData;
    _fill_save_meta(tk, d, floor, maxf);
    _parse_element(tk, p.get(), d);

    d->player = std::move(p);

    LOG_INFO("读档(slot %d v%d): 第%d层 Lv%d HP:%d/%d %zu技能 %zu物品 %zuBuff",
        slot_id, save_version, floor, lv, chp, mhp,
        d->player->skills.active_skills.size(), d->player->inventory.items.size(),
        d->player->active_buffs.size());
    return d;
}

// G10.9-B2: 旧接�?�?活跃槽转�?
SaveData* SaveManager::load_save() { return load_game(active_slot()); }

void SaveManager::delete_save(int slot_id) {
    remove(_slot_path(slot_id).c_str());
    remove((_slot_path(slot_id) + ".bak").c_str());   // G20b: 连带清备份
    LOG_INFO("存档已删�?(slot %d)", slot_id);
}

// G10.9-B2: 轻量槽位摘要 �?�?fopen 扫几�? 不构�?Player/不依�?Registry
// G20b: 上一代备份存在性 — 菜单据此决定是否给还原入口
bool SaveManager::backup_exists(int slot_id) {
    if (slot_id < 1 || slot_id > SAVE_SLOT_COUNT) return false;
    FILE* f = fopen((_slot_path(slot_id) + ".bak").c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

// G20b: 还原上一代备份。当前档不存在则直接提升备份; 存在则两代交换
// (用 .aside 中转, 不销毁任何一代, 可反复来回切换)。任一 rename 失败都回滚。
bool SaveManager::restore_backup(int slot_id) {
    if (slot_id < 1 || slot_id > SAVE_SLOT_COUNT) return false;
    if (g_sim_readonly) return false;
    const std::string cur = _slot_path(slot_id);
    const std::string bak = cur + ".bak";
    if (!backup_exists(slot_id)) {
        LOG_ERROR("还原失败: 无备份 (slot %d)", slot_id);
        return false;
    }
    if (!slot_exists(slot_id)) {
        if (rename(bak.c_str(), cur.c_str()) != 0) {
            LOG_ERROR("还原失败 (slot %d)", slot_id);
            return false;
        }
        LOG_INFO("还原备份: slot %d 无当前档, 备份直接提升", slot_id);
        return true;
    }
    const std::string aside = cur + ".aside";
    remove(aside.c_str());
    if (rename(bak.c_str(), aside.c_str()) != 0) {
        LOG_ERROR("还原失败: 备份无法挪走 (slot %d)", slot_id);
        return false;
    }
    if (rename(cur.c_str(), bak.c_str()) != 0) {
        remove(bak.c_str());
        (void)rename(aside.c_str(), bak.c_str());
        LOG_ERROR("还原失败: 当前档无法改名 (slot %d), 已回滚", slot_id);
        return false;
    }
    if (rename(aside.c_str(), cur.c_str()) != 0) {
        LOG_ERROR("还原失败: 备份无法提升 (slot %d), 手工检查 .aside", slot_id);
        return false;
    }
    LOG_INFO("还原备份: slot %d 已回到上一代", slot_id);
    return true;
}

// G20b: 行级摘要解析 — 正式槽与 .bak 共用, 保证两边字段口径一致
static bool _scan_summary(FILE* f, SlotSummary* s) {
    if (!f) return false;
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        if (strncmp(buf, "floor:", 6) == 0)      s->floor = atoi(buf + 6);
        else if (strncmp(buf, "maxf:", 5) == 0)  s->max_floor = atoi(buf + 5);
        else if (strncmp(buf, "lv:", 3) == 0)   s->level = atoi(buf + 3);
        else if (strncmp(buf, "time:", 5) == 0) s->play_time = (float)atof(buf + 5);
        else if (strncmp(buf, "elem:", 5) == 0) s->element_type = atoi(buf + 5);
    }
    s->exists = true;
    return true;
}

SlotSummary SaveManager::get_slot_summary(int slot_id) {
    SlotSummary s;
    s.slot_id = slot_id;
    FILE* f = fopen(_slot_path(slot_id).c_str(), "rb");
    if (!_scan_summary(f, &s)) return s;
    fclose(f);
    SlotSummary b;
    FILE* bf = fopen((_slot_path(slot_id) + ".bak").c_str(), "rb");
    if (_scan_summary(bf, &b)) {
        fclose(bf);
        s.has_backup = true;
        s.backup_floor = b.floor;
        s.backup_level = b.level;
    }
    return s;
}

std::vector<SlotSummary> SaveManager::get_all_slots() {
    std::vector<SlotSummary> v;
    v.reserve(SAVE_SLOT_COUNT);
    for (int i = 1; i <= SAVE_SLOT_COUNT; i++) v.push_back(get_slot_summary(i));
    return v;
}

// �?G10.9-B3: 旧档安全迁移 �?save.json �?slot_1.json �?
// 条件: 旧档存在 && slot_1 �?(不覆盖任何已有槽!)
// 流程: 复制旧档 �?验证新档可读 �?旧档改名 .bak (不删�? 迁移失败可手工恢�?
bool SaveManager::migrate_legacy_save() {
    FILE* legacy = fopen(_legacy_save_path().c_str(), "rb");
    if (!legacy) { fclose(legacy); return false; }          // 无旧�? 无事可做
    // 读完旧档内容到内�?(旧档单文�?< 10KB)
    std::string content;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), legacy)) > 0) content.append(buf, n);
    fclose(legacy);

    if (content.empty()) return false;
    if (slot_exists(1)) {
        LOG_INFO("[迁移] slot_1 已存�? 旧档保留不动");
        return false;                                       // 安全第一: 不覆�?
    }
    // 写入 slot_1
    mkdir_impl(_save_dir().c_str());
    FILE* out = fopen(_slot_path(1).c_str(), "wb");
    if (!out) { LOG_ERROR("[迁移] slot_1 写入失败"); return false; }
    fwrite(content.c_str(), 1, content.size(), out);
    fclose(out);
    // 验证: 新档存在且非�?
    FILE* verify = fopen(_slot_path(1).c_str(), "rb");
    if (!verify) { LOG_ERROR("[迁移] slot_1 验证失败(不可�?"); return false; }
    fseek(verify, 0, SEEK_END);
    bool ok = ftell(verify) == (long)content.size();
    fclose(verify);
    if (!ok) { remove(_slot_path(1).c_str()); LOG_ERROR("[迁移] slot_1 验证失败(大小不符)"); return false; }
    // 验证通过 �?旧档改名备份 (rename 同目录原子性足�? 失败也不丢数�?
    std::string bak = _legacy_save_path() + ".bak";
    if (rename(_legacy_save_path().c_str(), bak.c_str()) != 0)
        LOG_WARN("[迁移] 旧档改名失败 (数据已复制到 slot_1, 手动处理 save.json)");
    LOG_INFO("[迁移] 旧档 save.json �?slot_1.json 完成 (备份: save.json.bak)");
    return true;
}

// B8: spr 序列�?�?vector<bool> �?"1,0,1"
static std::string _encode_spr(const std::vector<bool>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++) {
        if (i > 0) out += ",";
        out += v[i] ? "1" : "0";
    }
    return out;
}

// B8: spr 反序列化 �?"1,0,1" �?vector<bool>
static std::vector<bool> _decode_spr(const std::string& s) {
    std::vector<bool> out;
    if (s.empty()) return out;
    for (size_t pos = 0; pos < s.size(); ) {
        size_t comma = s.find(',', pos);
        std::string tok = s.substr(pos, (comma == std::string::npos) ? std::string::npos : (comma - pos));
        out.push_back(tok == "1");  // 宽容: �?1"一律当 false
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return out;
}

// M4e: "1.0,2.0,..." �?vector<float> (空串忽略)
static void _parse_float_list(const std::string& s, std::vector<float>& out) {
    out.clear();
    if (s.empty()) return;
    for (size_t pos = 0; pos < s.size(); ) {
        size_t comma = s.find(',', pos);
        std::string tok = s.substr(pos, (comma == std::string::npos) ? std::string::npos : (comma - pos));
        if (!tok.empty()) out.push_back((float)atof(tok.c_str()));
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
}
