// G14: Buff 生命周期真实测试 — 替换假测试。
// 旧版复制了 SimpleBuffDef/SimpleBuff 两个结构体 + 手写 tick_buff 测自己的副本,
// 生产代码改了它照样全绿。本文件直连 load_buff_defs / apply_buff / tick_buffs /
// get_effective_attack, buff 数值取自 resources/buffs.json (Single Source of Truth)。
#include <gtest/gtest.h>

#include "systems/combat_system.h"
#include "entities/player.h"

#include <memory>
#include <vector>

// main.cpp 中的字体全局在测试中桩化 (与 floor_lifecycle_test.cpp 一致)
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

namespace {

std::unique_ptr<Player> make_player(int hp, int atk) {
    auto p = std::make_unique<Player>(0, 0, 200, hp, atk, 0, 0);
    p->combat.max_hp = hp;
    p->combat.current_hp = hp;
    return p;
}

int count_events(const std::vector<BuffEvent>& ev, BuffEventType t) {
    int n = 0;
    for (const auto& e : ev)
        if (e.type == t) ++n;
    return n;
}

int sum_value(const std::vector<BuffEvent>& ev, BuffEventType t) {
    int s = 0;
    for (const auto& e : ev)
        if (e.type == t) s += e.value;
    return s;
}

}  // namespace

// ── 契约 1: JSON 装载真实定义, 未知 id 返回 null ─────────────
TEST(BuffLifecycle, LoadsRealDefsFromJson) {
    ASSERT_TRUE(load_buff_defs("resources/buffs.json"));
    EXPECT_TRUE(is_buff_defs_loaded());

    const BuffDef* poison = get_buff_def("poison");
    ASSERT_NE(poison, nullptr);
    EXPECT_FLOAT_EQ(poison->duration, 4.0f);
    EXPECT_FLOAT_EQ(poison->tick_interval, 0.5f);
    EXPECT_EQ(poison->tick_damage, 3);
    EXPECT_EQ(poison->max_stacks, 5);
    EXPECT_EQ(get_buff_def("no_such_buff"), nullptr);
}

// ── 契约 2: 施加 buff → 生成实例 + APPLIED 事件 ───────────────
TEST(BuffLifecycle, ApplyCreatesInstance) {
    load_buff_defs("resources/buffs.json");
    auto p = make_player(100, 10);
    std::vector<BuffEvent> ev;

    apply_buff(p.get(), "poison", 2, &ev);

    ASSERT_EQ(p->active_buffs.size(), 1u);
    EXPECT_EQ(p->active_buffs[0].stacks, 2);
    EXPECT_FLOAT_EQ(p->active_buffs[0].remaining, 4.0f);
    EXPECT_EQ(count_events(ev, BuffEventType::APPLIED), 1);
}

// ── 契约 3: 重复施加 → 刷新持续时间, 层数按 max_stacks 封顶 ────
TEST(BuffLifecycle, ReapplyRefreshesDurationAndCapsStacks) {
    load_buff_defs("resources/buffs.json");
    auto p = make_player(100, 10);

    apply_buff(p.get(), "poison", 5);
    tick_buffs(p.get(), 2.0f);
    ASSERT_EQ(p->active_buffs.size(), 1u);
    EXPECT_FLOAT_EQ(p->active_buffs[0].remaining, 2.0f);

    apply_buff(p.get(), "poison", 5);   // 5+5 应封顶为 5, remaining 重置为 4.0
    ASSERT_EQ(p->active_buffs.size(), 1u);
    EXPECT_EQ(p->active_buffs[0].stacks, 5);
    EXPECT_FLOAT_EQ(p->active_buffs[0].remaining, 4.0f);
}

// ── 契约 4: 过期移除 + EXPIRED 事件 ─────────────────────────
TEST(BuffLifecycle, ExpiryRemovesBuffAndEmitsEvent) {
    load_buff_defs("resources/buffs.json");
    auto p = make_player(10000, 10);
    apply_buff(p.get(), "stun", 1);      // stun dur = 1.2s

    std::vector<BuffEvent> ev;
    tick_buffs(p.get(), 2.0f, &ev);

    EXPECT_TRUE(p->active_buffs.empty());
    EXPECT_EQ(count_events(ev, BuffEventType::EXPIRED), 1);
}

// ── 契约 5: DOT 伤害 = tick_damage × 层数, 逐跳结算 ──────────
// poison: dur 4.0 / interval 0.5 / dmg 3, 2 层 → 每跳 6。
// 8 × 0.5s 覆盖整个生命周期, 但最后一跳 remaining 已归零, 实际只打 7 跳。
TEST(BuffLifecycle, DotDamageScalesWithStacks) {
    load_buff_defs("resources/buffs.json");
    auto p = make_player(10000, 10);
    apply_buff(p.get(), "poison", 2);

    std::vector<BuffEvent> ev;
    for (int i = 0; i < 8; ++i)
        tick_buffs(p.get(), 0.5f, &ev);

    EXPECT_EQ(count_events(ev, BuffEventType::TICK_DAMAGE), 7);
    EXPECT_EQ(sum_value(ev, BuffEventType::TICK_DAMAGE), 7 * 6);
    EXPECT_TRUE(p->active_buffs.empty());
    EXPECT_EQ(p->combat.current_hp, 10000 - 42);
}

// ── 契约 6: tick_damage 为负 → 治疗而非伤害 (D8 regen) ────────
// regen: dur 4.0 / interval 1.0 / dmg -2, 2 层 → 每跳回 4; 3 跳共回 12。
TEST(BuffLifecycle, NegativeTickDamageHealsInsteadOfHurting) {
    load_buff_defs("resources/buffs.json");
    auto p = make_player(100, 10);
    p->combat.current_hp = 10;
    apply_buff(p.get(), "regen", 2);

    std::vector<BuffEvent> ev;
    for (int i = 0; i < 4; ++i)
        tick_buffs(p.get(), 1.0f, &ev);

    EXPECT_EQ(count_events(ev, BuffEventType::TICK_DAMAGE), 3);
    EXPECT_EQ(p->combat.current_hp, 10 + 12);
}

// ── 契约 7: attack_up 层数进入有效攻击 (每层 +20%) ────────────
TEST(BuffLifecycle, AttackUpStacksFeedEffectiveAttack) {
    load_buff_defs("resources/buffs.json");
    auto p = make_player(100, 10);

    EXPECT_EQ(get_effective_attack(p.get()), 10);
    apply_buff(p.get(), "attack_up", 2);   // 10 × 1.40
    EXPECT_EQ(get_effective_attack(p.get()), 14);
    apply_buff(p.get(), "attack_up", 1);   // 10 × 1.60
    EXPECT_EQ(get_effective_attack(p.get()), 16);
}

// ── 契约 8: 未知 buff id 静默忽略, 不生成实例 ─────────────────
TEST(BuffLifecycle, UnknownBuffIdIsIgnored) {
    load_buff_defs("resources/buffs.json");
    auto p = make_player(100, 10);

    apply_buff(p.get(), "no_such_buff", 3);

    EXPECT_TRUE(p->active_buffs.empty());
}
