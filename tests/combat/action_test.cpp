// G14: 事件执行真实测试 — 替换假测试。
// 旧版手写了 parse_action_type 复刻 "encounter.cpp execute_action logic",
// 还自封 "not linked to actual code"。更糟的是它枚举的 action 类型
// (hp_loss / debuff / confuse / skill_level / set_meta_flag ...) 在生产代码里
// 一个都不存在 —— 整个测试测的是想象中的接口。
// 真实接口是 execute_event(DungeonEvent&, Player*, int) + EventType 枚举。
#include <gtest/gtest.h>

#include "world/event_system.h"
#include "systems/combat_system.h"
#include "entities/player.h"
#include "data/item_defs.h"
#include "data/weapon_defs.h"

#include <memory>
#include <string>
#include <vector>

// main.cpp 中的字体全局在测试中桩化 (与 floor_lifecycle_test.cpp 一致)
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

namespace {

// 前置: JSON 路径相对 CWD, 必须在仓库根目录运行 (ctest 已设 WORKING_DIRECTORY)。
// 若资源缺失会在此直接暴露, 而不是伪装成"效果没生效"。
void load_all_defs() {
    ASSERT_TRUE(load_buff_defs("resources/buffs.json"));
    ASSERT_TRUE(load_relic_defs("resources/relics.json"));
    ASSERT_TRUE(load_item_defs("resources/items.json"));
    ASSERT_TRUE(load_weapon_defs("resources/weapons.json"));
    ASSERT_NE(get_buff_def("poison"), nullptr);
    ASSERT_NE(get_relic_def("iron_ring"), nullptr);
}

// Player(x, y, spd, hp, atk, pdef, mdef)
std::unique_ptr<Player> make_player(int hp = 100, int atk = 10) {
    auto p = std::make_unique<Player>(0, 0, 200, hp, atk, 0, 0);
    p->combat.max_hp = hp;
    p->combat.current_hp = hp;
    return p;
}

bool has_buff(const Player* p, const char* id) {
    for (const auto& b : p->active_buffs)
        if (b.id == id) return true;
    return false;
}

int stacks_of(const Player* p, const char* id) {
    for (const auto& b : p->active_buffs)
        if (b.id == id) return b.stacks;
    return 0;
}

}  // namespace

// ── 契约 1: 空指针玩家安全返回, 不崩溃 ───────────────────────
TEST(EventAction, NullPlayerReturnsEmpty) {
    DungeonEvent ev;
    ev.type = EventType::AMBUSH;
    EXPECT_EQ(execute_event(ev, nullptr, 1), "");
}

// ── 契约 2: 已触发事件不可重复执行 (防重复发奖) ───────────────
TEST(EventAction, TriggeredEventIsNoOp) {
    load_all_defs();
    auto p = make_player();
    DungeonEvent ev;
    ev.type = EventType::BLOOD_RITUAL;
    ev.triggered = true;

    const std::string msg = execute_event(ev, p.get(), 1);

    EXPECT_EQ(msg, "");
    EXPECT_TRUE(p->active_buffs.empty()) << "已触发事件不得再施加 buff";
    EXPECT_EQ(p->combat.current_hp, 100) << "已触发事件不得再扣血";
}

// ── 契约 3: NONE 无效果 ──────────────────────────────────────
TEST(EventAction, NoneReturnsEmpty) {
    load_all_defs();
    auto p = make_player();
    DungeonEvent ev;
    ev.type = EventType::NONE;

    EXPECT_EQ(execute_event(ev, p.get(), 1), "");
}

// ── 契约 4: AMBUSH 必定施加 attack_up (无随机分支) ────────────
TEST(EventAction, AmbushAlwaysAppliesAttackUp) {
    load_all_defs();
    auto p = make_player();
    DungeonEvent ev;
    ev.type = EventType::AMBUSH;

    const std::string msg = execute_event(ev, p.get(), 1);

    EXPECT_EQ(has_buff(p.get(), "attack_up"), true);
    EXPECT_EQ(stacks_of(p.get(), "attack_up"), 1);
    EXPECT_EQ(ev.triggered, true);
    EXPECT_NE(msg.find("MSG:"), std::string::npos);
}

// ── 契约 5: CURSED_ROOM 必定施加 poison 2 层 ─────────────────
TEST(EventAction, CursedRoomAlwaysAppliesPoison) {
    load_all_defs();
    auto p = make_player();
    DungeonEvent ev;
    ev.type = EventType::CURSED_ROOM;

    const std::string msg = execute_event(ev, p.get(), 1);

    EXPECT_TRUE(has_buff(p.get(), "poison"));
    EXPECT_EQ(stacks_of(p.get(), "poison"), 2);
    // 有货时返回 RELIC: (发诅咒圣物), 缺货时返回 MSG: 兜底文案。
    ASSERT_FALSE(msg.empty());
    EXPECT_TRUE(msg.rfind("RELIC:", 0) == 0 || msg.rfind("MSG:", 0) == 0)
        << "意外返回: " << msg;
}

// ── 契约 6: BLOOD_RITUAL = 扣 20% 当前血 + attack_up 3 层 ────
TEST(EventAction, BloodRitualTradesHealthForStrength) {
    load_all_defs();
    auto p = make_player(/*hp=*/100);
    DungeonEvent ev;
    ev.type = EventType::BLOOD_RITUAL;

    execute_event(ev, p.get(), 1);

    EXPECT_EQ(p->combat.current_hp, 80) << "100 × 0.20 = 20 伤害";
    EXPECT_EQ(stacks_of(p.get(), "attack_up"), 3);
}

// ── 契约 7: LOST_CAMP 治疗 25% max_hp ───────────────────────
TEST(EventAction, LostCampHeals25PercentOfMaxHp) {
    load_all_defs();
    auto p = make_player(/*hp=*/100);
    p->combat.current_hp = 50;
    DungeonEvent ev;
    ev.type = EventType::LOST_CAMP;

    execute_event(ev, p.get(), 1);

    EXPECT_EQ(p->combat.current_hp, 75) << "50 + 100 × 0.25 = 75";
}

// ── 契约 8: NOTHING 固定文本 ─────────────────────────────────
TEST(EventAction, NothingReturnsFixedMessage) {
    load_all_defs();
    auto p = make_player();
    DungeonEvent ev;
    ev.type = EventType::NOTHING;

    EXPECT_EQ(execute_event(ev, p.get(), 1), "MSG:这里什么也没有发生。");
}

// ── 契约 9: 所有真实事件类型的返回都带 MSG:/RELIC: 前缀 ───────
TEST(EventAction, EveryRealEventTypeReturnsPrefixedMessage) {
    load_all_defs();
    const EventType types[] = {
        EventType::MERCHANT, EventType::AMBUSH, EventType::CURSED_ROOM,
        EventType::ALTAR_CHOICE, EventType::STATUE, EventType::PRISONER,
        EventType::LOST_CAMP, EventType::TREASURE_GUARD, EventType::BLOOD_RITUAL,
        EventType::NOTHING, EventType::TRAP, EventType::MYSTERY,
        EventType::BLESSING, EventType::CURSE, EventType::LORE,
        EventType::NPC_EVENT, EventType::RELIC_DROP, EventType::TREASURE_CACHE,
    };
    for (auto t : types) {
        // 每种类型跑 20 次以覆盖内部分支; 用足血玩家避免死在断言前
        for (int i = 0; i < 20; ++i) {
            auto p = make_player(/*hp=*/10000);
            DungeonEvent ev;
            ev.type = t;
            const std::string msg = execute_event(ev, p.get(), 1);
            const bool prefixed =
                msg.rfind("MSG:", 0) == 0 || msg.rfind("RELIC:", 0) == 0;
            EXPECT_TRUE(prefixed)
                << "type=" << static_cast<int>(t) << " iter=" << i
                << " 返回非 MSG:/RELIC: 前缀: '" << msg << "'";
        }
    }
}

// ── 契约 10: 同种子可复现 (replay 依赖的事件确定性) ───────────
TEST(EventAction, SameSeedReproducesSameOutcome) {
    load_all_defs();

    seed_rng(4242u);
    auto p1 = make_player();
    DungeonEvent e1;
    e1.type = EventType::MERCHANT;
    const std::string m1 = execute_event(e1, p1.get(), 1);

    seed_rng(4242u);
    auto p2 = make_player();
    DungeonEvent e2;
    e2.type = EventType::MERCHANT;
    const std::string m2 = execute_event(e2, p2.get(), 1);

    EXPECT_EQ(m1, m2) << "同种子必须复现同一事件结果: '" << m1 << "' vs '" << m2 << "'";
}
