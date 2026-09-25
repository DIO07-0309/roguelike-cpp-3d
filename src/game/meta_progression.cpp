#include "meta_progression.h"
#include "core/logger.h"
#include "data/meta_node_defs.h"  // G3.1
#include <cstdio>
#include <cstring>
#include <algorithm>

// 与 save_manager.cpp / q_agent.cpp / sim_runner.cpp 同款: 写盘前确保目录存在。
// 首次运行目录若没有 saves/, fopen 静默失败 → meta 进度 (灵魂/知识/结局/保底计数)
// 全部丢档且不报错。save_manager 一直有这步, meta 一直缺。
#ifdef _WIN32
#include <direct.h>
#define meta_mkdir_impl(p) _mkdir(p)
#else
#include <sys/stat.h>
#define meta_mkdir_impl(p) mkdir(p, 0755)
#endif

MetaSystem g_meta;

// G3.1: 构造器空 — 在 JSON 加载后由 load_from_defs() 填充
MetaSystem::MetaSystem() {
    _node_count = 0;
}

// G3.1: 从 MetaNodeDef registry 构建 (替代硬编码 10 个 MetaNode)
void MetaSystem::load_from_defs() {
    _node_texts.clear();
    static const char* ORDER[] = {
        "hp_bonus","atk_bonus","gold_start","potion_start","relic_bonus",
        "exp_bonus","buff_dur","skill_cd","build_speed","crit_bonus",nullptr
    };

    int count = 0;
    for (int i = 0; ORDER[i] && count < 10; i++) {
        const MetaNodeDef* def = get_meta_node_def(ORDER[i]);
        if (!def) continue;
        _node_texts.push_back(def->id);
        _node_texts.push_back(def->name);
        _node_texts.push_back(def->description);
        MetaNode& n = _nodes[count];
        n.id         = _node_texts[_node_texts.size() - 3].c_str();
        n.name       = _node_texts[_node_texts.size() - 2].c_str();
        n.desc       = _node_texts[_node_texts.size() - 1].c_str();
        n.max_level  = def->max_level;
        n.cost_base  = def->cost_base;
        n.cost_scale = def->cost_scale;
        count++;
    }
    _node_count = count;
}

int MetaSystem::node_level(const char* id) const {
    for (int i = 0; i < _node_count; i++)
        if (strcmp(_nodes[i].id, id) == 0)
            return _save.node_levels[i];
    return 0;
}

float MetaSystem::permanent_bonus(const char* id) const {
    int lv = node_level(id);
    // 每个节点返回倍率: hp_bonus=0.02/级, atk=0.02/级, etc.
    if (strcmp(id, "hp_bonus") == 0)    return lv * 0.02f;
    if (strcmp(id, "atk_bonus") == 0)   return lv * 0.02f;
    if (strcmp(id, "gold_start") == 0)  return lv * 15.0f;
    if (strcmp(id, "potion_start") == 0) return (float)lv;
    if (strcmp(id, "relic_bonus") == 0) return lv * 0.02f;
    if (strcmp(id, "exp_bonus") == 0)   return lv * 0.03f;
    if (strcmp(id, "buff_dur") == 0)    return lv * 0.05f;
    if (strcmp(id, "skill_cd") == 0)    return lv * -0.02f;
    if (strcmp(id, "build_speed") == 0) return lv * 0.05f;
    if (strcmp(id, "crit_bonus") == 0)  return lv * 0.015f;
    return 0;
}

bool MetaSystem::upgrade_node(const char* id, MetaCurrency& cost) {
    for (int i = 0; i < _node_count; i++) {
        if (strcmp(_nodes[i].id, id) != 0) continue;
        int lv = _save.node_levels[i];
        if (lv >= _nodes[i].max_level) return false;
        int need = _nodes[i].cost_base + lv * _nodes[i].cost_scale;
        if (_save.currency.soul_fragments < need) return false;
        _save.currency.soul_fragments -= need;
        cost.soul_fragments += need;
        _save.node_levels[i]++;
        save();
        return true;
    }
    return false;
}

MetaCurrency MetaSystem::end_run(const RunSummary& rs) {
    MetaCurrency earned = calc_run_reward(rs);
    _save.currency.soul_fragments += earned.soul_fragments;
    _save.currency.knowledge += earned.knowledge;
    _save.currency.ancient_memory += earned.ancient_memory;
    _save.total_runs++;
    // G3.5: 记录 RUN_SUMMARY 奖励
    _reward_log.push_back({MetaRewardSource::RUN_SUMMARY,
        "Run结算: F" + std::to_string(rs.floor_reached) + " "
        + std::to_string(rs.bosses_killed) + "Boss "
        + std::to_string(rs.quests_done) + "Quest", earned});
    save();
    return earned;
}

void MetaSystem::add_currency(const MetaCurrency& c) {
    _save.currency.soul_fragments += c.soul_fragments;
    _save.currency.knowledge += c.knowledge;
    _save.currency.ancient_memory += c.ancient_memory;
}

// G3.5: 统一 Ending 奖励入口
void MetaSystem::reward_from_ending(const char* name, int soul, int knowledge,
                                     int ancient_memory) {
    MetaCurrency mc{soul, knowledge, ancient_memory};
    add_currency(mc);
    _reward_log.push_back({MetaRewardSource::ENDING,
        std::string("Ending: ") + name, mc});
    save();
}

void MetaSystem::clear_reward_log() { _reward_log.clear(); }

// ---- JSON save ----
bool MetaSystem::g_readonly = false;  // Q3.1: --sim 只读

bool MetaSystem::save() const {
    if (g_readonly) return false;  // Q3.1: sim 模式不写 meta 存档
    meta_mkdir_impl("saves");     // 目录不存在则创建; 失败由 fopen 兜底
    FILE* f = fopen("saves/meta_save.json", "w");
    if (!f) return false;
    fprintf(f, "{\n");
    fprintf(f, "  \"runs\":%d,\n", _save.total_runs);
    fprintf(f, "  \"soul\":%d,\n", _save.currency.soul_fragments);
    fprintf(f, "  \"know\":%d,\n", _save.currency.knowledge);
    fprintf(f, "  \"memory\":%d,\n", _save.currency.ancient_memory);
    fprintf(f, "  \"nodes\":[");
    for (int i = 0; i < _node_count; i++) {
        fprintf(f, "%d%s", _save.node_levels[i], (i < _node_count-1) ? "," : "");
    }
    fprintf(f, "],\n");
    // G10.9-B2: 账号历史最高层
    fprintf(f, "  \"best_floor\":%d,\n", _save.best_floor);
    // B4-T9: 挑战房压轴保底计数 (账号级, 跨 run/slot/重启持久)
    fprintf(f, "  \"pity\":%d,\n", _save.challenge_pity_streak);
    // G10.9-B2: 账号级结局收集
    fprintf(f, "  \"endings\":[");
    for (size_t i = 0; i < _save.unlocked_endings.size(); i++)
        fprintf(f, "%d%s", _save.unlocked_endings[i],
                (i + 1 < _save.unlocked_endings.size()) ? "," : "");
    fprintf(f, "],\n");
    // G10.8-B3: 首次提示标记
    fprintf(f, "  \"hints\":[");
    bool first = true;
    for (auto& [id, shown] : _save.first_hints_shown) {
        if (!shown) continue;
        fprintf(f, "%s\"%s\"", first ? "" : ",", id.c_str());
        first = false;
    }
    fprintf(f, "],\n");
    // v1.6-B2: 死因史 (最近 8 条: "层|死因" — 死因源无引号, 免转义)
    fprintf(f, "  \"deaths\":[");
    for (size_t i = 0; i < _save.death_history.size(); i++) {
        fprintf(f, "%s\"%d|%s\"", i ? "," : "",
                _save.death_history[i].floor,
                _save.death_history[i].cause.c_str());
    }
    fprintf(f, "]\n}\n");
    fclose(f);
    return true;
}

bool MetaSystem::load() {
    FILE* f = fopen("saves/meta_save.json", "r");
    if (!f) return false;
    // 简易解析 (v1.6-B2: 2048→4096 容纳 hints + deaths 数组)
    char buf[4096];
    fread(buf, 1, sizeof(buf)-1, f); fclose(f); buf[sizeof(buf)-1]=0;
    auto parse_int = [&](const char* key, int def) {
        const char* p = strstr(buf, key);
        if (!p) return def;
        p += strlen(key) + 1; // skip past '":'
        while (*p && (*p==' '||*p==':')) p++;
        return atoi(p);
    };
    _save.total_runs   = parse_int("\"runs\"", 0);
    _save.currency.soul_fragments = parse_int("\"soul\"", 0);
    _save.currency.knowledge      = parse_int("\"know\"", 0);
    _save.currency.ancient_memory = parse_int("\"memory\"", 0);
    _save.best_floor  = parse_int("\"best_floor\"", 1);
    _save.challenge_pity_streak = parse_int("\"pity\"", 0);   // B4-T9
    // G10.9-B2: 解析 endings 数组 ("endings":[1,2])
    _save.unlocked_endings.clear();
    if (const char* ep = strstr(buf, "\"endings\"")) {
        if ((ep = strstr(ep, "[")) != nullptr) {
            ep++;
            while (*ep && *ep != ']') {
                if (*ep >= '0' && *ep <= '9')
                    _save.unlocked_endings.push_back(atoi(ep));
                while (*ep && *ep != ',' && *ep != ']') ep++;
                if (*ep == ',') ep++;
            }
        }
    }
    // 解析 nodes 数组
    const char* np = strstr(buf, "\"nodes\"");
    if (np) { np = strstr(np, "["); if (np) { np++;
        for (int i = 0; i < _node_count && *np && *np != ']'; i++) {
            while (*np && *np != '-' && (*np < '0' || *np > '9')) np++;
            if (*np == ']') break;
            _save.node_levels[i] = atoi(np);
            while (*np && *np != ',' && *np != ']') np++;
            if (*np == ',') np++;
    }}}
    // G10.8-B3: 解析 hints 数组 ("hints":["a","b"] — 只关心存在性)
    _save.first_hints_shown.clear();
    if (const char* hp = strstr(buf, "\"hints\"")) {
        if ((hp = strstr(hp, "[")) != nullptr) {
            hp++;
            while (*hp && *hp != ']') {
                if (*hp == '"') {
                    char id[32]; int k = 0;
                    hp++;
                    while (*hp && *hp != '"' && k < 31) id[k++] = *hp++;
                    id[k] = 0;
                    if (k > 0) _save.first_hints_shown[id] = true;
                }
                hp++;
            }
        }
    }
    // v1.6-B2: 解析 deaths 数组 ("deaths":["3|火焰陷阱","5|兽人"]) — 老档无此键
    // → 死因史为空, 天然兼容
    _save.death_history.clear();
    if (const char* dp = strstr(buf, "\"deaths\"")) {
        if ((dp = strstr(dp, "[")) != nullptr) {
            dp++;
            while (*dp && *dp != ']') {
                if (*dp == '"') {
                    char rec[96]; int k = 0;
                    dp++;
                    while (*dp && *dp != '"' && k < 95) rec[k++] = *dp++;
                    rec[k] = 0;
                    if (k > 0) {
                        int fl = atoi(rec);
                        const char* bar = strchr(rec, '|');
                        if (bar && *(bar + 1))
                            _save.death_history.push_back({fl, std::string(bar + 1)});
                    }
                }
                dp++;
            }
        }
    }
    // 与 record_death 的环形上限保持一致 (手写/异常文件: 丢最旧, 留最近 8 条)
    if (_save.death_history.size() > 8)
        _save.death_history.erase(
            _save.death_history.begin(),
            _save.death_history.end() - 8);
    LOG_INFO("[META] Loaded: runs=%d soul=%d hints=%zu deaths=%zu",
             _save.total_runs, _save.currency.soul_fragments,
             _save.first_hints_shown.size(), _save.death_history.size());
    return true;
}

// ══════════════════════════════════════════════════════════════
// G10.8-B3: First Encounter Hint — 跨 run 持久化的"首次提示"标记
// 用法: if (!MetaSystem::hint_already_shown("encounter_lock")) {
//           show_hint("房间已封锁..."); MetaSystem::mark_hint_shown("encounter_lock"); }
// ══════════════════════════════════════════════════════════════
static const char* HINT_IDS[] = {
    "element_select", "hud_intro", "combo_stage3", "encounter_lock",
    "skill_cooldown", "skill_evolution", "boss_hp_bar", "biome_shift",
    "challenge_portal", "gold_key", "relic_first", "stairs_descend",
};

bool MetaSystem::hint_already_shown(const std::string& hint_id) {
    // 未存盘的新档 — 内存表为空 = 全部视为未显示
    return g_meta.data().first_hints_shown.count(hint_id) > 0;
}

void MetaSystem::mark_hint_shown(const std::string& hint_id) {
    // Q3.1: sim 模式不标记（保持每次真实玩家首遇都触发）
    if (g_readonly) return;
    g_meta._save.first_hints_shown[hint_id] = true;
    g_meta.save();
}

// ═══ G10.9-B2: 账号级结局收集 + 历史最高层 ═══
void MetaSystem::unlock_ending(int ending_type) {
    if (g_readonly) return;
    for (int t : g_meta._save.unlocked_endings)
        if (t == ending_type) return;          // 幂等
    g_meta._save.unlocked_endings.push_back(ending_type);
    g_meta.save();                              // 解锁即落盘 — 修审计 bug#2
}

bool MetaSystem::ending_unlocked(int ending_type) {
    for (int t : g_meta.data().unlocked_endings)
        if (t == ending_type) return true;
    return false;
}

void MetaSystem::record_floor_reached(int floor) {
    if (floor > g_meta._save.best_floor) {
        g_meta._save.best_floor = floor;
        g_meta.save();
    }
}

// ═══ B4-T9: 挑战房压轴保底 (账号级) ═══
// 记账即落盘 — 挑战房清空不是存档点, 若只随档位存档写盘, 玩家死亡/退出后进度丢失.
void MetaSystem::set_challenge_pity_streak(int n) {
    if (n < 0) n = 0;
    if (n == g_meta._save.challenge_pity_streak) return;
    g_meta._save.challenge_pity_streak = n;
    g_meta.save();
}

// ── v1.6-B2: 死因史 (最近 8 条环形; 账号级) ──
void MetaSystem::record_death(int floor, const std::string& cause) {
    auto& hist = _save.death_history;
    MetaSave::DeathRecord rec{floor, cause};
    if (hist.size() >= 8) {
        // 环形: 移除最旧 (front), 追加最新 — vector 小, O(n) 可忽略
        hist.erase(hist.begin());
    }
    hist.push_back(rec);
    save();
}

std::vector<std::pair<std::string, int>> MetaSystem::top_death_causes(
    int top_n) const {
    std::unordered_map<std::string, int> counts;
    for (auto& d : _save.death_history) counts[d.cause]++;
    std::vector<std::pair<std::string, int>> list(counts.begin(), counts.end());
    std::sort(list.begin(), list.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    if ((int)list.size() > top_n) list.resize(top_n);
    return list;
}

// G10.9-B4: 测试清理钩子
void MetaSystem::debug_reset_collection() {
    g_meta._save.unlocked_endings.clear();
    g_meta._save.best_floor = 1;
}

// 计算本局奖励
MetaCurrency calc_run_reward(const RunSummary& rs) {
    MetaCurrency m;
    m.soul_fragments = rs.floor_reached * 1
                     + rs.bosses_killed * 5
                     + rs.quests_done * 3
                     + rs.elite_kills;
    m.knowledge      = rs.relics_collected / 2
                     + (rs.combo_max >= 30 ? 3 : rs.combo_max >= 10 ? 1 : 0);
    m.ancient_memory = rs.bosses_killed > 0 ? 1 : 0;
    if (rs.bosses_killed >= 3) m.ancient_memory += 2;
    if (rs.floor_reached >= 15) m.soul_fragments += 10;
    return m;
}
