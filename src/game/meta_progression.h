#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

// ============================================================
// D4.6 Step5: Meta Progression — 永久成长系统
// 跨Run持久数据: 天赋树/货币/成就/解锁
// ============================================================

struct MetaNode {
    const char* id;         // e.g. "hp_bonus"
    const char* name;       // "生命力强化"
    const char* desc;       // "+2% 最大HP"
    int  max_level = 5;
    int  cost_base = 3;     // 第一级消耗 Soul Fragments
    int  cost_scale = 2;    // 每级递增
};

// ═══ G3.5: Meta Reward Audit — 奖励来源 + 审计记录 ═══
enum class MetaRewardSource { RUN_SUMMARY, ENDING, QUEST, BOSS, DEBUG };

struct MetaCurrency {
    int soul_fragments = 0;
    int knowledge = 0;
    int ancient_memory = 0;
};

struct MetaRewardRecord {
    MetaRewardSource source;
    std::string detail;          // "F10 Boss" | "Quest: 救出囚犯" | "Run结算"
    MetaCurrency amount;
};

struct RunSummary {
    int  floor_reached = 0;
    int  bosses_killed = 0;
    int  total_kills = 0;
    int  elite_kills = 0;
    int  relics_collected = 0;
    int  quests_done = 0;
    int  combo_max = 0;
    int  total_dmg = 0;
    int  build_type = 0;       // BuildType int
    float play_time = 0.0f;
    // computed rewards
    MetaCurrency reward;
};

struct MetaSave {
    MetaCurrency currency;
    int node_levels[10] = {0};  // 每个MetaNode的等级
    int total_runs = 0;
    // G10.8-B3: 首次事件提示标记 (hint_id → 已显示; 跨 run 持久化)
    std::unordered_map<std::string, bool> first_hints_shown;
    // G10.9-B2: 账号级数据 (按审计归属矩阵从 save.json 迁入)
    std::vector<int> unlocked_endings;   // EndingType 列表 (账号收集, 删档不丢)
    int best_floor = 1;                  // 账号历史最高层 (展示用; 选关仍读 Slot maxf)
    // B4-T9: 挑战房压轴保底计数 (账号级 — 局内计数会被 GameScene 重建清零)
    int challenge_pity_streak = 0;
    // v1.6-B2: 死因史 (最近 8 条, 环形覆盖; 账号级, 删档不丢)
    struct DeathRecord { int floor; std::string cause; };
    std::vector<DeathRecord> death_history;
};

// ---- 全局单例 ----
class MetaSystem {
public:
    MetaSystem();

    // 查询
    int   node_level(const char* id) const;
    float permanent_bonus(const char* id) const;  // 返回倍率
    const MetaCurrency& currency() const { return _save.currency; }
    int   total_runs() const { return _save.total_runs; }

    // 升级
    bool upgrade_node(const char* id, MetaCurrency& cost);

    // Run收尾
    MetaCurrency end_run(const RunSummary& summary);
    void add_currency(const MetaCurrency& c);

    // G3.5: 统一奖励入口 + 审计日志
    void reward_from_ending(const char* ending_name,
                            int soul, int knowledge, int ancient_memory = 2);
    void clear_reward_log();               // 新 Run 开始时调用
    const std::vector<MetaRewardRecord>& reward_log() const { return _reward_log; }

    // G3.1: 从 registry 重建 MetaNode (在 JSON 加载后调用)
    void load_from_defs();
    // 存档
    bool save() const;
    bool load();
    const MetaSave& data() const { return _save; }

    // Q3.1: --sim 模式禁止写 meta 存档
    static bool g_readonly;

    // G10.8-B3: First Encounter Hint — 返回 false 表示首次（调用方显示提示并标记）
    static bool hint_already_shown(const std::string& hint_id);
    static void mark_hint_shown(const std::string& hint_id);

    // G10.9-B2: 账号级结局收集 + 历史最高层
    static void unlock_ending(int ending_type);      // 幂等: 已解锁不重复计
    static bool ending_unlocked(int ending_type);
    static void record_floor_reached(int floor);     // best_floor 更新 (只升不降)

    // v1.6-B2: 死因史 — 最近 8 条环形记录 (sim readonly 模式不落盘)
    void record_death(int floor, const std::string& cause);
    // 死因谱 Top-N: 按次数降序 ("尖刺史莱姆" → 3)
    std::vector<std::pair<std::string, int>> top_death_causes(int top_n) const;

    // G10.9-B4: 测试钩子 — 清空账号收集 (仅测试清理用, 业务勿调)
    static void debug_reset_collection();
    // v1.6-B2: 测试钩子 — 清空死因史 (仅测试清理用, 业务勿调)
    void _clear_death_history_for_test() { _save.death_history.clear(); }

    // B4-T9: 挑战房压轴保底 — 账号级计数, 连续空手 N 次后下次必出.
    // 必须账号级: 玩家反复进出同一层会重建 GameScene, 局内计数随之清零.
    int  challenge_pity_streak() const { return _save.challenge_pity_streak; }
    void set_challenge_pity_streak(int n);

private:
    MetaSave _save;
    MetaNode _nodes[10];  // 10个节点
    int     _node_count = 0;
    // G3.1: string storage for MetaNode const char* fields (from registry)
    std::vector<std::string> _node_texts;
    // G3.5: 本次 run 的奖励审计日志
    std::vector<MetaRewardRecord> _reward_log;
};

extern MetaSystem g_meta;

// ---- 构造函数填充 RunSummary ----
MetaCurrency calc_run_reward(const RunSummary& rs);
