// G14: Skill::execute 伤害数值基线 — 9 个 Skill::execute 此前零自动化覆盖
// (tests/ 里只测过 load_skill_defs 的 JSON 装载); 既有 damage_test.cpp 是复制公式的
// golden oracle, 不调用生产函数。本文件直接驱动真实 calculate_damage / SlashSkill::execute。
// 注意: calculate_damage 含方差 (variance ∈ [0.8, 1.2)), 断言一律用区间/均值而非等值。
#include <gtest/gtest.h>

#include "entities/skill.h"
#include "entities/player.h"
#include "entities/monster.h"
#include "systems/combat_system.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// main.cpp 中的字体全局在测试中桩化 (与 floor_lifecycle_test.cpp 一致)
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

namespace {

// Player(x, y, spd, hp, atk, pdef, mdef); direction 默认 DOWN → 锥体只看 dy > 0
std::unique_ptr<Player> make_caster() {
    auto p = std::make_unique<Player>(0, 0, 200, 200, 10, 5, 3);
    p->direction = Direction::DOWN;
    return p;
}

// caster 中心 (16,16); SlashSkill::_cone_range = 1 → cr = 48px
std::unique_ptr<Monster> make_target(int hp, int pdef, bool in_cone = true) {
    const float ty = in_cone ? 40.0f : -40.0f;   // 后方: 不在 DOWN 锥体内
    auto m = std::make_unique<Monster>(16, ty, "测试怪", hp, 5, pdef, 0, RED);
    m->combat.current_hp = hp;
    m->combat.is_alive = true;
    return m;
}

int damage_dealt(Player* p, SlashSkill* sk, Monster* m, bool heavy) {
    const int hp_before = m->combat.current_hp;
    std::vector<Monster*> targets{m};
    sk->execute(p, targets, nullptr, heavy);
    return hp_before - m->combat.current_hp;
}

// 采样 300 次, 返回 [min, max] — 用于锁定方差带而不依赖随机种子
std::pair<int, int> damage_range(int atk, int def, AttackType type) {
    int lo = 1000000, hi = -1000000;
    for (int i = 0; i < 300; ++i) {
        const int d = calculate_damage(atk, def, type);
        lo = std::min(lo, d);
        hi = std::max(hi, d);
    }
    return {lo, hi};
}

}  // namespace

// ── 基线 1: 真实 calculate_damage 方差带 [0.8x, 1.2x) ─────────
TEST(SkillExecute, CalculateDamageStaysWithinVarianceBand) {
    auto [lo, hi] = damage_range(100, 0, AttackType::PHYSICAL);
    EXPECT_GE(lo, (int)(100.0f * 0.8f) - 1) << "低于方差下界, lo=" << lo;
    EXPECT_LE(hi, (int)(100.0f * 1.2f) + 1) << "高于方差上界, hi=" << hi;
}

// ── 基线 2: 防御线性减免 (物理 def_factor = 0.5: 100-30=70) ──
TEST(SkillExecute, PhysicalDefenseReducesBaseLinearly) {
    auto [lo, hi] = damage_range(100, 60, AttackType::PHYSICAL);
    EXPECT_GE(lo, (int)(70.0f * 0.8f) - 1);
    EXPECT_LE(hi, (int)(70.0f * 1.2f) + 1);
}

// ── 基线 3: TRUE 无视防御 (def_factor = 0.0) ─────────────────
TEST(SkillExecute, TrueDamageIgnoresDefense) {
    auto [lo, hi] = damage_range(100, 5000, AttackType::TRUE);
    EXPECT_GE(lo, (int)(100.0f * 0.8f) - 1);
    EXPECT_LE(hi, (int)(100.0f * 1.2f) + 1);
}

// ── 基线 4: 伤害下限恒为 1 ──────────────────────────────────
TEST(SkillExecute, DamageNeverBelowOne) {
    for (int i = 0; i < 300; ++i)
        EXPECT_GE(calculate_damage(1, 10000, AttackType::PHYSICAL), 1);
}

// ── 基线 5: SlashSkill 命中锥内目标, 伤害落在 atk*1.5 的方差带内 ─
TEST(SkillExecute, SlashHitsTargetInCone) {
    ASSERT_TRUE(load_buff_defs("resources/buffs.json"));
    auto p = make_caster();
    auto m = make_target(100000, 0);
    SlashSkill sk;

    const int dmg = damage_dealt(p.get(), &sk, m.get(), false);
    const float base =
        (float)get_effective_attack(p.get()) * 1.5f * sk.get_power_multiplier();

    EXPECT_GT(dmg, 0) << "锥内目标必须受伤";
    EXPECT_GE(dmg, (int)(base * 0.8f) - 1)
        << "dmg=" << dmg << " base=" << base;
    EXPECT_LE(dmg, (int)(base * 1.2f) + 1)
        << "dmg=" << dmg << " base=" << base;
}

// ── 基线 6: 无目标 → 返回挥空提示, 不产生伤害 ─────────────────
TEST(SkillExecute, SlashMissesWhenNoTargets) {
    auto p = make_caster();
    SlashSkill sk;
    std::vector<Monster*> targets;

    const std::string msg = sk.execute(p.get(), targets, nullptr, false);
    EXPECT_NE(msg.find("挥空"), std::string::npos) << "无目标必须提示挥空: " << msg;
}

// ── 基线 7: 身后目标不在 DOWN 锥体内, 不得受伤 ─────────────────
TEST(SkillExecute, SlashMissesTargetBehindPlayer) {
    ASSERT_TRUE(load_buff_defs("resources/buffs.json"));
    auto p = make_caster();
    auto m = make_target(100000, 0, /*in_cone=*/false);
    SlashSkill sk;

    const int dmg = damage_dealt(p.get(), &sk, m.get(), false);
    EXPECT_EQ(dmg, 0) << "身后目标不在 DOWN 锥体内, 不得受伤";
    EXPECT_EQ(m->combat.current_hp, 100000);
}

// ── 基线 8: 重击伤害均值 = 普攻 1.3 倍 (D2 Step2) ────────────
// 单样本受方差影响会与普攻重叠, 故用 200 次均值比对。
TEST(SkillExecute, HeavyHitAverages130PercentOfNormal) {
    ASSERT_TRUE(load_buff_defs("resources/buffs.json"));
    auto p = make_caster();
    SlashSkill sk;

    const int N = 200;
    long normal_sum = 0, heavy_sum = 0;
    for (int i = 0; i < N; ++i) {
        auto a = make_target(100000, 0);
        normal_sum += damage_dealt(p.get(), &sk, a.get(), false);
        auto b = make_target(100000, 0);
        heavy_sum += damage_dealt(p.get(), &sk, b.get(), true);
    }

    ASSERT_GT(normal_sum, 0) << "普攻未产生任何伤害";
    const double ratio = (double)heavy_sum / (double)normal_sum;
    EXPECT_GT(ratio, 1.15) << "重击 +30% 未体现: ratio=" << ratio;
    EXPECT_LT(ratio, 1.45) << "重击倍率异常: ratio=" << ratio;
}
