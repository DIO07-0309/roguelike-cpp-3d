#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ============================================================
// D4 Step5.3: RelationshipSystem — 第3个全局驱动系统
// 所有NPC/Quest/Boss/Ending 的关系数据统一来源
// ============================================================

enum class RelationType { TRUST, RESPECT, FEAR, GRATITUDE, CORRUPTION };

struct NPCRelation {
    int npc_id;             // floor*10 + slot (与NPCState一致)
    int trust      = 0;     // 信任: 救人/帮助
    int respect    = 0;     // 尊敬: 击杀Boss/强大
    int fear       = 0;     // 恐惧: 血祭/诅咒
    int gratitude  = 0;     // 感恩: 接受帮助/接受礼物
    int corruption = 0;     // 腐化: 献祭/血浴/黑暗行为

    int total() const { return trust + respect + fear + gratitude + corruption; }
    int get(RelationType t) const;
};

struct RelationReward {
    int npc_id = 0;
    int trust = 0, respect = 0, fear = 0, gratitude = 0, corruption = 0;
};

class RelationshipSystem {
public:
    // 查找/创建 NPC 关系
    NPCRelation* find(int npc_id);
    NPCRelation* find_or_create(int npc_id);

    // 增减关系
    void add_trust(int npc_id, int v);
    void add_respect(int npc_id, int v);
    void add_fear(int npc_id, int v);
    void add_gratitude(int npc_id, int v);
    void add_corruption(int npc_id, int v);

    // 一键发放 RelationReward
    void apply_reward(const RelationReward& rr);

    // 查询阈值 (用于NPC对话分支 / Boss hook)
    bool check_relation(int npc_id, RelationType type, int threshold) const;

    // 所有NPC (UI用)
    const std::vector<NPCRelation>& all() const { return _relations; }

private:
    std::vector<NPCRelation> _relations;
};

// ---- 预设 RelationReward 常量 (Quest/Event 处引用) ----
extern const RelationReward RR_SAVE_PRISONER;   // +2 trust
extern const RelationReward RR_SAVE_PRIEST;     // +3 trust +1 respect
extern const RelationReward RR_BLOOD_RITUAL;    // +2 fear +1 corruption
extern const RelationReward RR_ACCEPT_CURSE;    // +1 corruption
extern const RelationReward RR_BOSS_KILLED;     // +1 respect (所有NPC)
