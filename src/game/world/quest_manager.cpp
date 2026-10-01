#include "quest_manager.h"
#include "player.h"
#include "combat_system.h"
#include "relationship_system.h"
#include "core/logger.h"
#include "core/event_bus.h"        // G2.4: EventBus
#include "data/quest_defs.h"       // G2.4: QuestDef

// ── G2.4: String → enum 辅助映射 ──
static WorldFlag _str_to_flag(const std::string& s) {
    if (s == "saved_prisoner")   return WorldFlag::Saved_Prisoner;
    if (s == "saved_priest")     return WorldFlag::Saved_Priest;
    if (s == "saved_merchant")   return WorldFlag::Saved_Merchant;
    if (s == "accepted_curse")   return WorldFlag::Accepted_Curse;
    if (s == "blood_ritual")     return WorldFlag::Blood_Ritual;
    if (s == "ignored_npc")      return WorldFlag::Ignored_NPC;
    if (s == "merchant_killed")  return WorldFlag::Merchant_Killed;
    if (s == "boss1_defeated")   return WorldFlag::Boss1_Defeated;
    if (s == "boss2_defeated")   return WorldFlag::Boss2_Defeated;
    if (s == "boss3_defeated")   return WorldFlag::Boss3_Defeated;
    if (s == "all_boss_defeated") return WorldFlag::All_Boss_Defeated;
    if (s == "hunter_met")       return WorldFlag::Hunter_Met;
    if (s == "collector_met")    return WorldFlag::Collector_Met;
    if (s == "scout_met")        return WorldFlag::Scout_Met;
    if (s == "dreamer_met")      return WorldFlag::Dreamer_Met;
    if (s == "kill_10_monsters") return WorldFlag::Kill_10_Monsters;
    if (s == "reach_floor_10")   return WorldFlag::Reach_Floor_10;
    if (s == "collect_3_relics") return WorldFlag::Collect_3_Relics;
    return WorldFlag::NONE;
}

static StoryStage _str_to_stage(const std::string& s) {
    if (s == "chapter_1") return StoryStage::CHAPTER_1;
    if (s == "chapter_2") return StoryStage::CHAPTER_2;
    if (s == "final")     return StoryStage::FINAL;
    return StoryStage::INTRO;
}

// ============================================================
// G2.4: QuestManager 构造函数 — 从 QuestDef registry 构建
// ============================================================
QuestManager::QuestManager() {
    for (auto& kv : get_all_quest_defs()) {
        auto& d = kv.second;

        Quest q;
        q.id              = d.quest_id;
        q.title           = d.title;
        q.desc            = d.desc;
        q.state           = QuestState::LOCKED;  // all start locked
        q.chapter         = d.chapter;
        q.hidden          = d.hidden;
        q.auto_accept     = d.auto_accept;
        q.required_flag   = _str_to_flag(d.required_flag);
        q.required_stage  = _str_to_stage(d.required_stage);
        q.required_counter = d.required_counter;
        q.counter_key     = d.counter_key;
        q.complete_flag   = _str_to_flag(d.complete_flag);
        q.next_quest_id   = d.next_quest_id;

        q.reward.relic_id     = d.reward.relic_id;
        q.reward.buff_id      = d.reward.buff_id;
        q.reward.buff_stacks  = d.reward.buff_stacks;
        q.reward.set_flag     = _str_to_flag(d.reward.set_flag);
        q.reward.counter_name = d.reward.counter_name;
        q.reward.counter_add  = d.reward.counter_add;
        q.reward.story_msg    = d.reward.story_msg.empty() ? nullptr
                               : d.reward.story_msg.c_str();  // QuestDef 生命周期长于 QuestManager
        q.reward.relation.npc_id     = d.reward.npc_id;
        q.reward.relation.trust      = d.reward.trust;
        q.reward.relation.respect    = d.reward.respect;
        q.reward.relation.fear       = d.reward.fear;
        q.reward.relation.gratitude  = d.reward.gratitude;
        q.reward.relation.corruption = d.reward.corruption;

        _quests.push_back(q);
    }
}

// ---- 自动解锁 & 自动完成 (算法不变) ----
void QuestManager::update(WorldState& ws, const StoryDirector& story) {
    for (auto& q : _quests) {
        _auto_unlock(q, ws, story);
        _auto_complete(q, ws);
    }
}

void QuestManager::_auto_unlock(Quest& q, const WorldState& ws, const StoryDirector& story) {
    if (q.state != QuestState::LOCKED) return;
    if (q.required_stage != StoryStage::INTRO && (int)story.stage() < (int)q.required_stage)
        return;
    if (q.required_flag != WorldFlag::NONE && !ws.has(q.required_flag))
        return;
    if (q.required_counter > 0 && !q.counter_key.empty()
        && ws.counter(q.counter_key) < q.required_counter)
        return;
    q.state = QuestState::AVAILABLE;
    if (q.auto_accept) q.state = QuestState::ACCEPTED;
    LOG_INFO("[QUEST] Unlocked #%d: %s", q.id, q.title.c_str());
}

void QuestManager::_auto_complete(Quest& q, WorldState& ws) {
    if (q.state != QuestState::ACCEPTED && q.state != QuestState::AVAILABLE) return;
    if (q.complete_flag == WorldFlag::NONE) return;
    if (!ws.has(q.complete_flag)) return;
    q.state = QuestState::COMPLETED;
    if (q.reward.set_flag != WorldFlag::NONE) ws.set(q.reward.set_flag);
    if (!q.reward.counter_name.empty()) ws.add_counter(q.reward.counter_name, q.reward.counter_add);
    if (_rels && q.reward.relation.npc_id != 0) _rels->apply_reward(q.reward.relation);

    // G2.4: EventBus emit
    EventBus::inst().emit(GameEventType::QUEST_COMPLETE, nullptr,
                          q.id, 0.0f, q.title.c_str());
    LOG_INFO("[QUEST] Completed #%d: %s", q.id, q.title.c_str());
}

// ---- 玩家操作 ----
bool QuestManager::accept(int quest_id, WorldState& ws) {
    Quest* q = find(quest_id);
    if (!q || q->state != QuestState::AVAILABLE) return false;
    q->state = QuestState::ACCEPTED;
    EventBus::inst().emit(GameEventType::QUEST_ACCEPT, nullptr,
                          quest_id, 0.0f, q->title.c_str());
    (void)ws;
    return true;
}

// ---- G2.4: Save v3 ----
void QuestManager::restore_states(const std::unordered_map<int, int>& states) {
    for (auto& q : _quests) {
        auto it = states.find(q.id);
        if (it != states.end())
            q.state = (QuestState)it->second;
    }
}

std::unordered_map<int, int> QuestManager::export_states() const {
    std::unordered_map<int, int> out;
    for (auto& q : _quests)
        if (q.state != QuestState::LOCKED)
            out[q.id] = (int)q.state;
    return out;
}

// ---- 查询 ----
Quest* QuestManager::find(int quest_id) {
    for (auto& q : _quests) if (q.id == quest_id) return &q;
    return nullptr;
}

// G2.4: 按 npc_floor 查询 (替代硬编码 floor→id 映射)
Quest* QuestManager::get_npc_quest(int npc_floor) {
    for (auto& q : _quests) {
        // 查 QuestDef 的 npc_floor
        const QuestDef* def = get_quest_def(q.id);
        if (def && def->npc_floor == npc_floor) return &q;
    }
    return nullptr;
}

int QuestManager::available_count() const {
    int n = 0;
    for (auto& q : _quests) if (q.state == QuestState::AVAILABLE) n++;
    return n;
}

int QuestManager::completed_count() const {
    int n = 0;
    for (auto& q : _quests) if (q.state == QuestState::COMPLETED) n++;
    return n;
}
