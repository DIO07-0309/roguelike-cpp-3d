// G24: Quest reward 发放回归测试
//
// 背景: quests.json 有 8 条奖励配了 relic_id/buff_id, 但唯一会发放它们的
// QuestManager::grant_reward() 从没人调用; 活跃路径 _auto_complete() 只发
// set_flag/counter/relation, relic 与 buff 被静默丢弃。v1.14 已发布包同样如此。
// G23 清理死方法时删除了 grant_reward(), 未察觉这条奖励支路是活的配置 + 死代码。
// 本文件驱动真实 QuestManager::update() 全链路, 断言 relic 带 RUN scope、buff
// 正确施放、counter 累计, 且 player 为 nullptr 时只推进状态不发奖。
#include <gtest/gtest.h>

#include "world/quest_manager.h"
#include "world/world_state.h"
#include "entities/player.h"
#include "entities/combat_stats.h"
#include "systems/combat_system.h"
#include "data/quest_defs.h"

#include <memory>

// main.cpp 中的字体全局在测试中桩化 (与 buff_test.cpp 一致)
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

namespace {

std::unique_ptr<Player> make_player() {
    auto p = std::make_unique<Player>(0, 0, 200, 100, 10, 0, 0);
    p->combat.max_hp = 100;
    p->combat.current_hp = 100;
    return p;
}

int relic_count(const Player* p, const char* id, PersistenceScope* scope_out = nullptr) {
    int n = 0;
    for (const auto& r : p->relics) {
        if (r.id == id) {
            ++n;
            if (scope_out) *scope_out = r.scope;
        }
    }
    return n;
}

int buff_stacks(const Player* p, const char* id) {
    int s = 0;
    for (const auto& b : p->active_buffs)
        if (b.id == id) s += b.stacks;
    return s;
}

// 装载真实 JSON 定义, 数值口径以 resources/ 为 Single Source of Truth。
// gtest 同一进程顺序执行, 两个 loader 均拒绝重复装载 (已加载返回 false)。
void load_defs() {
    if (!is_quest_defs_loaded()) ASSERT_TRUE(load_quest_defs("resources/quests.json"));
    if (!is_buff_defs_loaded()) ASSERT_TRUE(load_buff_defs("resources/buffs.json"));
}

}  // namespace

// ── 契约 1: collector_items (#8) 完成 → golden_dice + collector_quest counter ──
TEST(QuestReward, CollectorQuestGrantsRareRelicAndCounter) {
    load_defs();
    auto p = make_player();
    WorldState ws;
    StoryDirector story;
    story.enter_floor(6);               // required_stage = chapter_2
    ws.set(WorldFlag::Collector_Met);   // required_flag
    ws.set(WorldFlag::Collect_3_Relics); // complete_flag

    QuestManager mgr;
    mgr.update(ws, story, p.get());

    Quest* q = mgr.find(8);
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->state, QuestState::COMPLETED);

    PersistenceScope scope = PersistenceScope::FLOOR;
    ASSERT_EQ(relic_count(p.get(), "golden_dice", &scope), 1);
    EXPECT_EQ(scope, PersistenceScope::RUN)
        << "剧情里程碑奖励换层不该丢失; FLOOR 会让传奇圣物白送一层就蒸发";
    EXPECT_EQ(ws.counter("collector_quest"), 1);
}

// ── 契约 2: dreamer_quest (#10) → legendary 圣物 + blessing buff x2 ──────────
TEST(QuestReward, DreamerQuestGrantsLegendaryRelicAndBuff) {
    load_defs();
    auto p = make_player();
    WorldState ws;
    StoryDirector story;
    story.enter_floor(15);                // required_stage = final
    ws.set(WorldFlag::Boss2_Defeated);    // required_flag
    ws.set(WorldFlag::Boss3_Defeated);    // complete_flag

    QuestManager mgr;
    mgr.update(ws, story, p.get());

    Quest* q = mgr.find(10);
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->state, QuestState::COMPLETED);

    PersistenceScope scope = PersistenceScope::FLOOR;
    ASSERT_EQ(relic_count(p.get(), "infinity_orb", &scope), 1);
    EXPECT_EQ(scope, PersistenceScope::RUN);
    EXPECT_EQ(buff_stacks(p.get(), "blessing"), 2);
}

// ── 契约 3: 完成判定与奖励发放解耦 —— nullptr 玩家不崩, 状态照常推进 ──────────
TEST(QuestReward, NullPlayerAdvancesStateWithoutGranting) {
    load_defs();
    WorldState ws;
    StoryDirector story;
    story.enter_floor(15);
    ws.set(WorldFlag::Boss2_Defeated);
    ws.set(WorldFlag::Boss3_Defeated);

    QuestManager mgr;
    mgr.update(ws, story, nullptr);

    Quest* q = mgr.find(10);
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->state, QuestState::COMPLETED);
    EXPECT_EQ(ws.counter("dreamer_quest"), 1);
}

// ── 契约 4: 幂等 —— 重复 update 不得重复发放圣物 ───────────────────────────
TEST(QuestReward, RepeatedUpdateDoesNotDuplicateRelic) {
    load_defs();
    auto p = make_player();
    WorldState ws;
    StoryDirector story;
    story.enter_floor(6);
    ws.set(WorldFlag::Collector_Met);
    ws.set(WorldFlag::Collect_3_Relics);

    QuestManager mgr;
    mgr.update(ws, story, p.get());
    mgr.update(ws, story, p.get());
    mgr.update(ws, story, p.get());

    EXPECT_EQ(relic_count(p.get(), "golden_dice"), 1);
}
