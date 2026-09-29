#include "gameplay_system_director.h"
#include "event_bus.h"
#include "player.h"
#include "core/logger.h"
#include "item.h"  // rng
#include "boss_replay.h"

void GameplaySystemDirector::on_player_dead(int floor, int level, const Player* player) {
    run_stats.floor_reached = floor;
    if (player) {
        run_stats.relics_collected = (int)player->relics.size();
        auto qs = quest_mgr.all_quests();
        for (auto& q : qs) if (q.state == QuestState::COMPLETED) run_stats.quests_done++;
        run_stats.build_type = (int)calculate_build(player).identify();
        if (player->combo.count > run_stats.combo_max)
            run_stats.combo_max = player->combo.count;
    }

    // 简易结局判定 (BAD END if didn't kill Boss3)
    if (!world_state.has(WorldFlag::Boss3_Defeated)) {
        ending_dir.begin(world_state, BossRank::C,
                         g_relic_archive.collection_pct(), rels, quest_mgr);
    }

    MetaCurrency earned = g_meta.end_run(run_stats);
    LOG_INFO("[META] 奖励: 魂片+%d 知识+%d 远古记忆+%d",
        earned.soul_fragments, earned.knowledge, earned.ancient_memory);
}

void GameplaySystemDirector::on_game_clear(int floor, int level, const Player* player,
    const BossBattleReport& boss_report, float collection_pct) {
    run_stats.floor_reached = floor;
    if (player) {
        run_stats.relics_collected = (int)player->relics.size();
        auto qs = quest_mgr.all_quests();
        for (auto& q : qs) if (q.state == QuestState::COMPLETED) run_stats.quests_done++;
        run_stats.build_type = (int)calculate_build(player).identify();
        if (player->combo.count > run_stats.combo_max)
            run_stats.combo_max = player->combo.count;
    }

    ending_dir.begin(world_state, boss_report.rank, collection_pct, rels, quest_mgr);

    // G3.5: 统一 Ending 奖励 (替代手写 MetaCurrency mc)
    g_meta.reward_from_ending(ending_dir.ending_name(),
        ending_dir.meta_reward_soul(),
        ending_dir.meta_reward_knowledge(), 2);
}

void GameplaySystemDirector::init_events() {
    EventBus::inst().subscribe(GameEventType::RELIC_GAIN,
        [this](const GameEvent& ev) {
            if (ev.str_val) run_stats.relics_collected++;
            (void)ev.int_val;
        }, "Gameplay", this);
    EventBus::inst().subscribe(GameEventType::FLOOR_ENTER,
        [this](const GameEvent& ev) {
            (void)ev;  // reserved: world state updates
        }, "Gameplay", this);
}

void GameplaySystemDirector::unregister_events() {
    EventBus::inst().unsubscribe(GameEventType::RELIC_GAIN, this);
    EventBus::inst().unsubscribe(GameEventType::FLOOR_ENTER, this);
}
