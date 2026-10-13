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

MetaCurrency MetaSystem::end_run(const RunSummary& rs) {
    MetaCurrency earned = calc_run_reward(rs);
    _save.currency.soul_fragments += earned.soul_fragments;
    _save.currency.knowledge += earned.knowledge;
    _save.currency.ancient_memory += earned.ancient_memory;
    _save.total_runs++;
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
    save();
}

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
    // G10.9-B2: 账号历史最高层 / B4-T9: 挑战房压轴保底计数
    fprintf(f, "  \"best_floor\":%d,\n", _save.best_floor);
    fprintf(f, "  \"pity\":%d,\n", _save.challenge_pity_streak);
    _meta_save_arrays(f);
    fclose(f);
    return true;
}

// G53: 4 个数组字段的序列化 (nodes / endings / hints / deaths)
void MetaSystem::_meta_save_arrays(FILE* f) const {
    fprintf(f, "  \"nodes\":[");
    for (int i = 0; i < _node_count; i++)
        fprintf(f, "%d%s", _save.node_levels[i], (i < _node_count - 1) ? "," : "");
    fprintf(f, "],\n");

    // G10.9-B2: 账号级结局收集
    fprintf(f, "  \"endings\":[");
    for (size_t i = 0; i < _save.unlocked_endings.size(); i++)
        fprintf(f, "%d%s", _save.unlocked_endings[i],
                (i + 1 < _save.unlocked_endings.size()) ? "," : "");
    fprintf(f, "],\n");

    // G10.8-B3: 首次提示标记
    fprintf(f, "  \"hints\":[");
    bool first = true;
    for (auto& kv : _save.first_hints_shown) {
        if (!kv.second) continue;
        fprintf(f, "%s\"%s\"", first ? "" : ",", kv.first.c_str());
        first = false;
    }
    fprintf(f, "],\n");

    // v1.6-B2: 死因史 (最近 8 条: 层|死因 — 死因源无引号, 免转义)
    fprintf(f, "  \"deaths\":[");
    for (size_t i = 0; i < _save.death_history.size(); i++)
        fprintf(f, "%s\"%d|%s\"", i ? "," : "",
                _save.death_history[i].floor,
                _save.death_history[i].cause.c_str());
    fprintf(f, "]\n}\n");
}

// ── G49: meta_save.json 解析子步骤 (原 load 92 行, 4 个数组同构) ──

// 取 "key" 对应的数组内容起点 (跳过 '[' 后的第一个字符);
// 找不到 key 或缺 '[' 时返回 nullptr
static const char* meta_array_at(const char* buf, const char* key) {
    const char* p = strstr(buf, key);
    if (!p) return nullptr;
    p = strstr(p, "[");
    if (!p) return nullptr;
    return p + 1;
}

// 整型数组 ("endings":[1,2]) → append 到 out (不清空, 由调用方决定)
static void meta_parse_int_array(const char* buf, const char* key,
                                 std::vector<int>& out) {
    const char* p = meta_array_at(buf, key);
    if (!p) return;
    while (*p && *p != ']') {
        if (*p >= '0' && *p <= '9') out.push_back(atoi(p));
        while (*p && *p != ',' && *p != ']') p++;
        if (*p == ',') p++;
    }
}

// 定长整型数组 ("nodes":[...]) → 最多写 max_n 个到 out, 不适配越界文件
static void meta_parse_int_array_capped(const char* buf, const char* key,
                                        int* out, int max_n) {
    const char* p = meta_array_at(buf, key);
    if (!p) return;
    for (int i = 0; i < max_n && *p && *p != ']'; i++) {
        while (*p && *p != '-' && (*p < '0' || *p > '9')) p++;
        if (*p == ']') break;
        out[i] = atoi(p);
        while (*p && *p != ',' && *p != ']') p++;
        if (*p == ',') p++;
    }
}

// 字符串数组 ("hints":["a","b"]) → 每项以 key 存入 set (只关心存在性)
static void meta_parse_string_array(const char* buf, const char* key,
                                    std::unordered_map<std::string, bool>& out) {
    const char* p = meta_array_at(buf, key);
    if (!p) return;
    while (*p && *p != ']') {
        if (*p == '"') {
            char id[32]; int k = 0;
            p++;
            while (*p && *p != '"' && k < 31) id[k++] = *p++;
            id[k] = 0;
            if (k > 0) out[id] = true;
        }
        p++;
    }
}

// 结构化字符串数组 ("deaths":["3|火焰陷阱"]) → (floor, cause) 对。
// 无 '|' 或 floor 非数字的项跳过 — 异常文件不应让读档失败
static void meta_parse_death_array(const char* buf, const char* key,
                                   std::vector<MetaSave::DeathRecord>& out) {
    const char* p = meta_array_at(buf, key);
    if (!p) return;
    while (*p && *p != ']') {
        if (*p == '"') {
            char rec[96]; int k = 0;
            p++;
            while (*p && *p != '"' && k < 95) rec[k++] = *p++;
            rec[k] = 0;
            if (k > 0) {
                int fl = atoi(rec);
                const char* bar = strchr(rec, '|');
                if (bar && *(bar + 1))
                    out.push_back({fl, std::string(bar + 1)});
            }
        }
        p++;
    }
}

bool MetaSystem::load() {
    FILE* f = fopen("saves/meta_save.json", "r");
    if (!f) return false;
    // 简易解析 (v1.6-B2: 2048->4096 容纳 hints + deaths 数组)
    char buf[4096];
    fread(buf, 1, sizeof(buf)-1, f); fclose(f); buf[sizeof(buf)-1]=0;

    _meta_load_scalars(buf);
    _meta_load_arrays(buf);

    LOG_INFO("[META] Loaded: runs=%d soul=%d hints=%zu deaths=%zu",
             _save.total_runs, _save.currency.soul_fragments,
             _save.first_hints_shown.size(), _save.death_history.size());
    return true;
}

// G53: 标量字段 (6 个 int)
void MetaSystem::_meta_load_scalars(const char* buf) {
    auto parse_int = [&](const char* key, int def) {
        const char* p = strstr(buf, key);
        if (!p) return def;
        p += strlen(key) + 1;   // skip past \":\'\
        while (*p && (*p == ' ' || *p == ':')) p++;
        return atoi(p);
    };
    _save.total_runs   = parse_int("\"runs\"", 0);
    _save.currency.soul_fragments = parse_int("\"soul\"", 0);
    _save.currency.knowledge      = parse_int("\"know\"", 0);
    _save.currency.ancient_memory = parse_int("\"memory\"", 0);
    _save.best_floor  = parse_int("\"best_floor\"", 1);
    _save.challenge_pity_streak = parse_int("\"pity\"", 0);   // B4-T9
}

// G53: 4 个数组字段 + death_history 的 8 条环形上限
void MetaSystem::_meta_load_arrays(const char* buf) {
    // G10.9-B2: endings 数组 ("endings":[1,2])
    _save.unlocked_endings.clear();
    meta_parse_int_array(buf, "\"endings\"", _save.unlocked_endings);

    // nodes 数组 (定长, 与 _node_count 对应) — helper 内部自行定位数组起点,
    // 必须传 buf 而非已定位到 [ 之后的指针 (那样会找不到 key)
    meta_parse_int_array_capped(buf, "\"nodes\"",
                                _save.node_levels, _node_count);

    // G10.8-B3: hints 数组 — 只关心存在性
    _save.first_hints_shown.clear();
    meta_parse_string_array(buf, "\"hints\"", _save.first_hints_shown);

    // v1.6-B2: deaths 数组 — 老档无此键 -> 空史, 天然兼容
    _save.death_history.clear();
    meta_parse_death_array(buf, "\"deaths\"", _save.death_history);

    // 与 record_death 的环形上限保持一致 (手写/异常文件: 丢最旧, 留最近 8 条)
    if (_save.death_history.size() > 8)
        _save.death_history.erase(
            _save.death_history.begin(),
            _save.death_history.end() - 8);
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
