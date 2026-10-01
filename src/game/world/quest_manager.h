#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "world_state.h"
#include "relationship_system.h"

class Player;

// ============================================================
// D4 Step5.2: QuestManager (G2.4: Data Driven)
// ============================================================

enum class QuestState { LOCKED, AVAILABLE, ACCEPTED, COMPLETED, FAILED };

struct QuestReward {
    std::string relic_id;
    std::string buff_id;
    int         buff_stacks = 1;
    WorldFlag   set_flag = WorldFlag::NONE;
    std::string counter_name;
    int         counter_add = 1;
    const char* story_msg = nullptr;
    RelationReward relation;
};

struct Quest {
    int         id = 0;
    std::string title;
    std::string desc;
    QuestState  state = QuestState::LOCKED;
    int         chapter = 0;
    bool        hidden = false;

    // 解锁条件
    WorldFlag    required_flag = WorldFlag::NONE;
    StoryStage   required_stage = StoryStage::INTRO;
    int          required_counter = 0;
    std::string  counter_key;

    // 完成条件
    WorldFlag    complete_flag = WorldFlag::NONE;

    // 奖励
    QuestReward reward;

    // 链式任务
    int         next_quest_id = 0;

    bool        auto_accept = false;
};

class QuestManager {
public:
    QuestManager();

    void set_relationship_system(RelationshipSystem* rels) { _rels = rels; }

    // 每帧更新: 自动解锁/完成 (根据 WorldState)
    void update(WorldState& ws, const StoryDirector& story);

    // 玩家操作。完成由 update() 自动判定 (_auto_complete),
    // 没有手工完成/失败入口; QuestState::FAILED 为存档兼容保留。
    bool accept(int quest_id, WorldState& ws);

    // G2.4: Save v3 读写
    void restore_states(const std::unordered_map<int, int>& states);
    std::unordered_map<int, int> export_states() const;

    // 查询
    Quest*       find(int quest_id);
    Quest*       get_npc_quest(int npc_floor);
    int          available_count() const;
    int          completed_count() const;
    const std::vector<Quest>& all_quests() const { return _quests; }

private:
    std::vector<Quest> _quests;
    RelationshipSystem* _rels = nullptr;
    void _auto_unlock(Quest& q, const WorldState& ws, const StoryDirector& story);
    void _auto_complete(Quest& q, WorldState& ws);
};
