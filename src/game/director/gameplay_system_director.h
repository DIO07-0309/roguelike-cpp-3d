#pragma once
#include "world_state.h"
#include "quest_manager.h"
#include "relationship_system.h"
#include "flow_director.h"
#include "ending_director.h"
#include "meta_progression.h"
#include "relic_progression.h"
#include "floor_narrative.h"
#include "build_score.h"

class Player;
class Monster;
class GameMap;

// ============================================================
// D6 Step4: GameplaySystemDirector — 收纳所有非Boss的Gameplay子系统
// 组合模式: 持有 Flow/Quest/Relationship/Story/Ending/Meta
// GameScene 仅通过此 Director 协调非Boss gameplay tick
// ============================================================
class GameplaySystemDirector {
public:
    // ── 聚合的子系统 (组合, 非继承) ──
    FlowDirector        flow;
    WorldState          world_state;
    StoryDirector       story;
    RelationshipSystem  rels;
    QuestManager        quest_mgr;
    EndingDirector      ending_dir;
    RunSummary          run_stats;
    NarrativeState      narr_state;
    BuildType           last_notified_build = BuildType::NONE;
    void on_player_dead(int floor, int level, const Player* player);
    void on_game_clear(int floor, int level, const Player* player,
                       const class BossBattleReport& boss_report,
                       float collection_pct);
    void init_events();  // D7 Step5: 订阅 EventBus
    void unregister_events();  // 场景析构时注销, 防止悬挂订阅
};
