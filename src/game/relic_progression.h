#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// ============================================================
// D4.6 Step4: Relic Progression — 圣物长期成长
// 永久追踪: 收藏/熟练度/解锁/套装加成
// ============================================================

struct RelicArchiveEntry {
    std::string id;
    int  obtained_count = 0;      // 累计获得次数
    int  highest_rarity = 0;      // 0=common,1=rare,2=epic,3=legendary
    int  mastery = 0;             // 熟练度 0-500 (每获得一次+50, match+30)
    bool permanent_unlock = false; // 首次获得后永久解锁
};

struct CollectionBonus {
    std::string tag;       // BuildTag name
    int required_count;    // 需要几件
    const char* bonus_desc;
    float value;           // 效果数值
};

class RelicArchiveSystem {
public:
    RelicArchiveSystem();

    // 获得 relic 时调用 (每局结束时也可调用)
    void mark_obtained(const std::string& relic_id, int rarity_level);
    // 查询
    bool is_unlocked(const std::string& relic_id) const;
    int  mastery_level(const std::string& relic_id) const;   // 0-5
    int  collected_count() const;                             // 永久解锁数
    int  total_relic_count() const;                            // 全部relic数
    float collection_pct() const;
    // 套装加成 (permanent stat bonus)
    float get_collection_bonus(const std::string& tag) const;
    const RelicArchiveEntry* entry(const std::string& relic_id) const;
    // 所有条目
    const std::vector<RelicArchiveEntry>& all_entries() const { return _entries; }
private:
    std::vector<RelicArchiveEntry> _entries;
    std::vector<CollectionBonus>   _bonuses;
};

// 全局单实例
extern RelicArchiveSystem g_relic_archive;
int rarity_level(const std::string& rarity_str);
