// G14: 圣物效果真实测试 — 替换假测试。
// 旧版在文件内重新定义了 RelicTrigger / RelicEffectType / RelicTarget / DamageType
// 四个枚举 + RelicEffectDef / RelicEffectRuntime / DamageContext 三个结构体,
// 203 行全部测自己的副本, 生产代码改了照样全绿。
// 本文件直连 RelicEffectProcessor + load_relic_defs, 数值取自 resources/relics.json。
#include <gtest/gtest.h>

#include "systems/relic_effect_processor.h"
#include "systems/combat_system.h"
#include "entities/player.h"
#include "entities/monster.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// main.cpp 中的字体全局在测试中桩化 (与 floor_lifecycle_test.cpp 一致)
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

namespace {

void load_defs() {
    load_buff_defs("resources/buffs.json");
    load_relic_defs("resources/relics.json");
}

std::unique_ptr<Player> make_player(int hp = 100, int atk = 10, int pdef = 0) {
    auto p = std::make_unique<Player>(0, 0, 200, hp, atk, pdef, 0);
    p->combat.max_hp = hp;
    p->combat.current_hp = hp;
    p->combat.physical_defense = pdef;
    return p;
}

std::unique_ptr<Monster> make_monster(int hp) {
    auto m = std::make_unique<Monster>(0, 0, "测试怪", hp, 5, 0, 0, RED);
    m->combat.current_hp = hp;
    m->combat.is_alive = true;
    return m;
}

int stacks_of(const Player* p, const char* id) {
    for (const auto& b : p->active_buffs)
        if (b.id == id) return b.stacks;
    return 0;
}

}  // namespace

// ── 契约 1: JSON 装载真实效果定义 ─────────────────────────────
// iron_ring: passive / modify_stat / physical_defense +5
TEST(RelicEffect, LoadsRealRelicEffectDefs) {
    load_defs();
    const RelicDef* ring = get_relic_def("iron_ring");
    ASSERT_NE(ring, nullptr);
    ASSERT_EQ(ring->effects.size(), 1u);
    EXPECT_EQ(ring->effects[0].trigger, RelicTrigger::PASSIVE);
    EXPECT_EQ(ring->effects[0].type, RelicEffectType::MODIFY_STAT);
    EXPECT_EQ(ring->effects[0].stat, "physical_defense");
    EXPECT_FLOAT_EQ(ring->effects[0].value, 5.0f);
}

// ── 契约 2: 静态被动 apply/remove 严格对称 ────────────────────
TEST(RelicEffect, StaticPassiveApplyAndRemoveAreSymmetric) {
    load_defs();
    auto p = make_player();
    const int base = p->combat.physical_defense;

    RelicEffectProcessor::apply_passive_for_relic(p.get(), "iron_ring");
    EXPECT_EQ(p->combat.physical_defense, base + 5);

    RelicEffectProcessor::remove_passive_for_relic(p.get(), "iron_ring");
    EXPECT_EQ(p->combat.physical_defense, base);
}

// ── 契约 3: 未 acquire 就 remove 必须无效 (防防御变负) ────────
TEST(RelicEffect, RemoveWithoutAcquireIsNoOp) {
    load_defs();
    RelicEffectProcessor proc;
    auto p = make_player(100, 10, 2);

    proc.on_relic_removed(p.get(), "iron_ring");

    EXPECT_EQ(p->combat.physical_defense, 2)
        << "从未施加过的被动不得被扣减";
}

// ── 契约 4: acquire 后 remove 只回滚一次 (重复 remove 不得再扣) ─
TEST(RelicEffect, RemoveAfterAcquireRollsBackExactlyOnce) {
    load_defs();
    RelicEffectProcessor proc;
    auto p = make_player(100, 10, 2);

    proc.on_relic_acquired(p.get(), "iron_ring");
    EXPECT_EQ(p->combat.physical_defense, 7);

    proc.on_relic_removed(p.get(), "iron_ring");
    EXPECT_EQ(p->combat.physical_defense, 2);

    proc.on_relic_removed(p.get(), "iron_ring");   // 第二次 remove 应无操作
    EXPECT_EQ(p->combat.physical_defense, 2)
        << "重复 remove 不得把物防扣成负数";
}

// ── 契约 5: 禁用处理器后全部效果失效 ─────────────────────────
TEST(RelicEffect, DisabledProcessorIsNoOp) {
    load_defs();
    RelicEffectProcessor proc;
    proc.set_enabled(false);
    auto p = make_player(100, 10, 2);

    proc.on_relic_acquired(p.get(), "iron_ring");
    proc.on_floor_enter(p.get());

    EXPECT_EQ(p->combat.physical_defense, 2);
    EXPECT_TRUE(p->active_buffs.empty());
    EXPECT_FALSE(proc.is_enabled());
}

// ── 契约 6: 未知圣物 id 静默忽略 ──────────────────────────────
TEST(RelicEffect, UnknownRelicIdIsIgnored) {
    load_defs();
    auto p = make_player(100, 10, 2);

    RelicEffectProcessor::apply_passive_for_relic(p.get(), "no_such_relic");
    RelicEffectProcessor::remove_passive_for_relic(p.get(), "");

    EXPECT_EQ(p->combat.physical_defense, 2);
}

// ── 契约 7: emerald_heart 进入新层施加 regen 3 层 ────────────
TEST(RelicEffect, EmeraldHeartAppliesRegenOnFloorEnter) {
    load_defs();
    RelicEffectProcessor proc;
    auto p = make_player();
    p->add_relic("emerald_heart", PersistenceScope::RUN);

    proc.on_floor_enter(p.get());

    EXPECT_EQ(stacks_of(p.get(), "regen"), 3);
}

// ── 契约 8: thunder_orb 击杀 AOE 只伤旁观者, 绝不误伤被击杀者 ─
// 30% 概率触发, 跑 300 次确保至少命中一次; 每次伤害 = 有效攻击 × 1.0 = 10。
TEST(RelicEffect, OnKillAoEHitsBystandersNeverTheVictim) {
    load_defs();
    RelicEffectProcessor proc;
    auto player = make_player(/*atk=*/10);
    player->add_relic("thunder_orb", PersistenceScope::RUN);

    int total_bystander_damage = 0;
    for (int i = 0; i < 300; ++i) {
        auto victim = make_monster(100000);
        auto bystander = make_monster(100000);
        std::vector<Monster*> all{victim.get(), bystander.get()};

        proc.on_kill(player.get(), victim.get(), all);

        EXPECT_EQ(victim->combat.current_hp, 100000)
            << "被击杀者不得被自家 AOE 打中 (iter " << i << ")";
        total_bystander_damage += 100000 - bystander->combat.current_hp;
    }

    EXPECT_GT(total_bystander_damage, 0)
        << "300 次 × 30% 概率下旁观者从未受伤 — AOE 未生效";
    EXPECT_EQ(total_bystander_damage % 10, 0)
        << "单次 AOE 伤害应为 有效攻击×1.0 = 10 的整数倍";
}

// ── 契约 9: tiny_shield 减伤永不超过原值, 且确实会触发 ───────
// chance 0.1 / value 0.5 → 命中时 100 → 50; 1000 次内必然至少触发一次。
TEST(RelicEffect, PreDamageReductionNeverIncreasesDamage) {
    load_defs();
    auto player = make_player();
    player->add_relic("tiny_shield", PersistenceScope::RUN);

    int min_after = 1000000, max_after = 0;
    for (int i = 0; i < 1000; ++i) {
        DamageContext ctx;
        ctx.final_damage = 100;
        RelicEffectProcessor::static_on_pre_damage(ctx, player.get());
        min_after = std::min(min_after, ctx.final_damage);
        max_after = std::max(max_after, ctx.final_damage);
    }

    EXPECT_LE(max_after, 100) << "减伤不得放大伤害";
    EXPECT_EQ(min_after, 50) << "命中时应为 100 × (1 - 0.5)";
    EXPECT_LT(min_after, max_after) << "1000 次内减伤从未触发";
}
