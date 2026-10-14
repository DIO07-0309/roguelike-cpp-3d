#include "ending_director.h"
#include "relationship_system.h"
#include "quest_manager.h"
#include "data/ending_defs.h"    // G2.5
#include "core/event_bus.h"      // G2.5: ENDING_REACHED
#include "meta_progression.h"    // G10.9-B1: unlock_ending 账号级落盘

// G53: 结局判定 — 按 True_End / Good / Normal / Bad 的优先级链
EndingType EndingDirector::_determine_ending(const WorldState& ws, BossRank rank,
                                             float collection_pct) const {
    if (ws.has(WorldFlag::True_Ending_Ready) && rank >= BossRank::S
        && collection_pct >= 0.90f)
        return EndingType::SECRET_END;
    if (ws.has(WorldFlag::True_Ending_Ready) && rank >= BossRank::A)
        return EndingType::TRUE_END;
    if (ws.has(WorldFlag::All_Boss_Defeated)
        && ws.has(WorldFlag::Saved_Prisoner)
        && ws.has(WorldFlag::Saved_Priest))
        return EndingType::GOOD_END;
    if (ws.has(WorldFlag::Boss3_Defeated))
        return EndingType::NORMAL_END;
    return EndingType::BAD_END;  // 死亡或Boss未全杀
}

// G53: 从 registry 取 flavor text / meta reward (替代旧的 WORLD_ENDINGS[5])
void EndingDirector::_apply_ending_def() {
    // G2.5: flavor text / meta reward 从 registry 读取
    const EndingDef* def = get_ending_def((int)_type);
    if (!def) {
        _world = { _type, "灰暗", "一切归于沉寂。", "END", 0, 0 };
        return;
    }
    _texts.sky   = def->sky_color;
    _texts.line  = def->final_line;
    _texts.title = def->title;
    _world = { _type,
               _texts.sky.c_str(),
               _texts.line.c_str(),
               _texts.title.c_str(),
               def->meta_soul_bonus,
               def->meta_knowledge_bonus };
}

// G53: 解锁记录 — 去重 + 账号级落盘 + 事件广播
void EndingDirector::_record_unlocked(int ending_type) {
    // G2.5: 记录已解锁结局 (去重)
    for (int v : _unlocked) if (v == ending_type) return;
    _unlocked.push_back(ending_type);
    // G10.9-B1: 账号级落盘 — 解锁即写 meta (修审计 bug#2: 此前从不持久化)
    MetaSystem::unlock_ending(ending_type);
    // EventBus emit (保持与 Quest/Attack/Skill Evolution 一致的风格)
    EventBus::inst().emit(GameEventType::ENDING_REACHED, nullptr, ending_type);
}

void EndingDirector::begin(const WorldState& ws, BossRank rank, float coll_pct,
                            const RelationshipSystem& rels,
                            const QuestManager& qm) {
    (void)qm;
    _npc_endings.clear();

    _type = _determine_ending(ws, rank, coll_pct);
    _apply_ending_def();
    _record_unlocked((int)_type);

    // NPC 结局
    _build_npc_endings(ws, rels);
}

void EndingDirector::_build_npc_endings(const WorldState& ws,
                                         const RelationshipSystem& rels) {
    // 埃德加 (F2)
    if (ws.has(WorldFlag::Saved_Prisoner))
        _npc_endings.push_back({20, "埃德加", "幸存",
            "他被关押三十年后终于重见天日。他的第一个自由日——也是地牢的第一个自由日。"});
    else
        _npc_endings.push_back({20, "埃德加", "继续等待",
            "铁门仍然紧锁着。他将继续独自等待——也许下一个冒险者会发现他。"});

    // 泰伦斯 (F7)
    if (ws.has(WorldFlag::Saved_Priest)) {
        _npc_endings.push_back({70, "泰伦斯", "希望复苏",
            "神官的祈祷终于得到了回应。虽然神明仍未归来——但至少，神殿不再黑暗。"});
    } else {
        _npc_endings.push_back({70, "泰伦斯", "最后的守望",
            "他继续在腐化的神殿里独自祈祷。终有一天——也许再也没有人去听。"});
    }

    // 朝圣者索拉斯 (F9)
    if (rels.check_relation(90, RelationType::RESPECT, 1))
        _npc_endings.push_back({90, "索拉斯", "继续前行",
            "他终于不再迷失——而是成为了后来的冒险者的指引者。"});
    else
        _npc_endings.push_back({90, "索拉斯", "永远迷失",
            "朝圣者之路的尽头——他还在那里。继续等待着永远不会到来的终点。"});

    // 守望者 (F14)
    if (rels.check_relation(140, RelationType::RESPECT, 2))
        _npc_endings.push_back({140, "守望者", "使命终结",
            "守望者推开了起源之门——这是三千年来第一次，也是最后一次。\n他终于可以休息了。"});
    else
        _npc_endings.push_back({140, "守望者", "独自留守",
            "三千年又三千年——他将继续守着那扇永远不会被打开的门。"});
}

const char* EndingDirector::ending_name() const {
    switch (_type) {
        case EndingType::BAD_END:    return "BAD END";
        case EndingType::NORMAL_END: return "NORMAL END";
        case EndingType::GOOD_END:   return "GOOD END";
        case EndingType::TRUE_END:   return "TRUE END";
        case EndingType::SECRET_END: return "ABSOLUTE END";
    }
    return "?";
}

const char* EndingDirector::ending_title() const {
    return _world.credits_title;
}

const char* EndingDirector::final_line() const {
    return _world.final_line;
}

const char* EndingDirector::sky_color() const {
    return _world.sky_color;
}

int EndingDirector::meta_reward_soul() const { return _world.meta_soul_bonus; }
int EndingDirector::meta_reward_knowledge() const { return _world.meta_knowledge_bonus; }
