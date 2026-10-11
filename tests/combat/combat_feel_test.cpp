#include <gtest/gtest.h>
#include <cstring>
#include <set>
#include <string>
#include "game/combat_feel.h"
#include "game/build_score.h"                    // BuildType
#include "game/director/presentation_system_director.h"  // BuildTheme

// ============================================================
// G39: 打击感分级 + 流派差异 — "是否真的生效"的永久性客观断言
//
// 背景: G39 之前普攻命中只调 trigger_freeze, 屏震恒为 0 ——
//   CombatFeelSystem 里的 SHAKE_LIGHT/COMBO/CRIT/MEDIUM 四个常量
//   从未被任何代码读取 (死数据), BuildTheme::camera.shake_scale /
//   dash_offset 同样只写不读。这些测试把"数值非零 + 按分级递增 +
//   流派之间有差异"锁死, 防止再次静默退化成死数据。
// ============================================================

TEST(CombatFeel, LightHitShakeIsNonZero) {
    // G39 之前: 轻击 shake 恒为 0 (只有顿帧)。这是本测试的核心断言。
    float freeze_s = 0.0f, shake = 0.0f;
    CombatFeelSystem::hit_feel_for(false, false, 0, freeze_s, shake);
    EXPECT_GT(shake, 0.0f) << "轻击屏震为 0 — G39 改动被回退?";
    EXPECT_GT(freeze_s, 0.0f);
}

TEST(CombatFeel, ShakeIncreasesWithTier) {
    // 轻 → 三段重击 → 暴击 → 击杀, 震幅必须单调递增 (手感层级的可感知基础)
    float f = 0.0f, s_light = 0.0f, s_combo = 0.0f, s_crit = 0.0f, s_kill = 0.0f;

    CombatFeelSystem::hit_feel_for(false, false, 0, f, s_light);
    CombatFeelSystem::hit_feel_for(false, false, 2, f, s_combo);
    CombatFeelSystem::hit_feel_for(false, true,  0, f, s_crit);
    CombatFeelSystem::hit_feel_for(true,  false, 0, f, s_kill);

    EXPECT_LT(s_light, s_combo) << "三段重击震幅应大于轻击";
    EXPECT_LT(s_combo, s_crit) << "暴击震幅应大于三段重击";
    EXPECT_LT(s_crit, s_kill) << "击杀震幅应大于暴击";
}

TEST(CombatFeel, KillBeatsEverything) {
    // 聚合取最高等级: 同时有 kill + crit 时, 要以 kill 为准 (不叠加)
    float f_kill = 0.0f, s_kill = 0.0f;
    float f_crit = 0.0f, s_crit = 0.0f;
    CombatFeelSystem::hit_feel_for(true, true, 2, f_kill, s_kill);
    CombatFeelSystem::hit_feel_for(false, true, 2, f_crit, s_crit);
    EXPECT_NEAR(f_kill, CombatFeelSystem::KILL_SLOWMO, 1e-5f);
    EXPECT_GT(f_kill, f_crit);
    EXPECT_GT(s_kill, s_crit);
}

TEST(CombatFeel, ExactValuesPerTier) {
    // 钉住具体数值, 改手感时这里是唯一需要同步改的地方
    float f = 0.0f, s = 0.0f;

    CombatFeelSystem::hit_feel_for(false, false, 0, f, s);
    EXPECT_NEAR(f, CombatFeelSystem::LIGHT_HIT, 1e-6f);
    EXPECT_NEAR(s, CombatFeelSystem::SHAKE_LIGHT, 1e-6f);

    CombatFeelSystem::hit_feel_for(false, false, 2, f, s);
    EXPECT_NEAR(f, CombatFeelSystem::HEAVY_HIT, 1e-6f);
    EXPECT_NEAR(s, CombatFeelSystem::SHAKE_COMBO, 1e-6f);

    CombatFeelSystem::hit_feel_for(false, true, 0, f, s);
    EXPECT_NEAR(f, CombatFeelSystem::CRITICAL_HIT, 1e-6f);
    EXPECT_NEAR(s, CombatFeelSystem::SHAKE_CRIT, 1e-6f);

    CombatFeelSystem::hit_feel_for(true, false, 0, f, s);
    EXPECT_NEAR(f, CombatFeelSystem::KILL_SLOWMO, 1e-6f);
    EXPECT_NEAR(s, CombatFeelSystem::SHAKE_MEDIUM, 1e-6f);
}

TEST(CombatFeel, StageOneIsLightTier) {
    // 段位 1 (二段) 与 0 同为轻档 — 只有三段 (stage==2) 才是重击
    float f0 = 0.0f, s0 = 0.0f, f1 = 0.0f, s1 = 0.0f;
    CombatFeelSystem::hit_feel_for(false, false, 0, f0, s0);
    CombatFeelSystem::hit_feel_for(false, false, 1, f1, s1);
    EXPECT_NEAR(f0, f1, 1e-6f);
    EXPECT_NEAR(s0, s1, 1e-6f);
}

// ------------------------------------------------------------
// BuildTheme: 流派之间的手感差异必须真实存在
// (G39 之前 shake_scale / dash_offset 只写不读, 两个都是死数据)
// ------------------------------------------------------------

TEST(BuildTheme, ShakeScaleDiffersAcrossBuilds) {
    // 若全部相同, 说明 shake_scale 没被当成差异数据设计 — 也就调不出流派手感
    float fire  = BuildTheme::from_build_type(BuildType::FIRE_MAGE).camera.shake_scale;
    float ice   = BuildTheme::from_build_type(BuildType::ICE_MAGE).camera.shake_scale;
    float shadow= BuildTheme::from_build_type(BuildType::SHADOW_STRIKER).camera.shake_scale;
    EXPECT_GT(fire, ice) << "火应比冰更震";
    EXPECT_GT(ice, shadow) << "冰应比暗影更震";
    EXPECT_NEAR(fire, 1.30f, 1e-5f);
    EXPECT_NEAR(ice, 0.80f, 1e-5f);
    EXPECT_NEAR(shadow, 0.70f, 1e-5f);
}

TEST(BuildTheme, DashOffsetOnlyOnFastBuilds) {
    // 翻滚 FOV 拉伸: "快"流派有值, "稳"流派恒 0 (冰/辅助/召唤本就没有冲刺感)
    EXPECT_GT(BuildTheme::from_build_type(BuildType::LIGHTNING_MAGE).camera.dash_offset, 0.0f);
    EXPECT_GT(BuildTheme::from_build_type(BuildType::FIRE_MAGE).camera.dash_offset, 0.0f);
    EXPECT_GT(BuildTheme::from_build_type(BuildType::SHADOW_STRIKER).camera.dash_offset, 0.0f);
    EXPECT_FLOAT_EQ(BuildTheme::from_build_type(BuildType::ICE_MAGE).camera.dash_offset, 0.0f);
    EXPECT_FLOAT_EQ(BuildTheme::from_build_type(BuildType::SUPPORT).camera.dash_offset, 0.0f);
    EXPECT_FLOAT_EQ(BuildTheme::from_build_type(BuildType::TIME_MASTER).camera.dash_offset, 0.0f);
}

TEST(BuildTheme, EveryBuildHasUsableFeelData) {
    // 全枚举扫一遍: 不允许任何流派返回非法值 (缩放<=0 会让屏震反转/消失)
    const BuildType all[] = {
        BuildType::ICE_MAGE, BuildType::FIRE_MAGE, BuildType::POISON_MASTER,
        BuildType::LIGHTNING_MAGE, BuildType::TIME_MASTER, BuildType::BERSERKER,
        BuildType::BLEED_BLADE, BuildType::SHADOW_STRIKER, BuildType::JUGGERNAUT,
        BuildType::SUPPORT, BuildType::SUMMON_LORD, BuildType::PROJECTILE,
    };
    for (BuildType bt : all) {
        BuildTheme t = BuildTheme::from_build_type(bt);
        EXPECT_GT(t.camera.shake_scale, 0.0f) << "流派 shake_scale 非法: " << t.name;
        EXPECT_GT(t.camera.zoom_offset, -1.0f) << "流派 zoom_offset 非法: " << t.name;
        EXPECT_GE(t.camera.dash_offset, 0.0f) << "流派 dash_offset 不应为负: " << t.name;
    }
}

// ------------------------------------------------------------
// G40: 打击感/听感四个新字段 — 必须有差异且取值合法,
// 否则说明设计数据没被认真填 (那就调不出流派手感)
// ------------------------------------------------------------

TEST(BuildTheme, FreezeBonusDifferentiatesBuilds) {
    // 时停系应当顿帧更长, 大多数流派无加成
    EXPECT_GT(BuildTheme::from_build_type(BuildType::TIME_MASTER).camera.freeze_bonus,
              BuildTheme::from_build_type(BuildType::BERSERKER).camera.freeze_bonus);
    EXPECT_GT(BuildTheme::from_build_type(BuildType::TIME_MASTER).camera.freeze_bonus, 0.0f);
    EXPECT_FLOAT_EQ(BuildTheme::from_build_type(BuildType::BERSERKER).camera.freeze_bonus, 0.0f);
}

TEST(BuildTheme, ZoomOffsetDifferentiatesBuilds) {
    // 暗影拉远视野 / 狂战拉近; 必须不同, 否则 zoom_offset 无意义
    float shadow = BuildTheme::from_build_type(BuildType::SHADOW_STRIKER).camera.zoom_offset;
    float berserk = BuildTheme::from_build_type(BuildType::BERSERKER).camera.zoom_offset;
    EXPECT_LT(shadow, berserk) << "暗影应比狂战视野更远";
    EXPECT_LT(shadow, 0.0f) << "暗影 zoom_offset 应为负(拉远)";
    EXPECT_GT(berserk, 0.0f) << "狂战 zoom_offset 应为正(拉近)";
}

TEST(BuildTheme, AudioSignaturesAreValidAndDistinct) {
    // hit_sfx / skill_sfx 必须是已注册的音效名 (未注册会静默回默认, 等于没接)
    const BuildType all[] = {
        BuildType::ICE_MAGE, BuildType::FIRE_MAGE, BuildType::POISON_MASTER,
        BuildType::LIGHTNING_MAGE, BuildType::TIME_MASTER, BuildType::BERSERKER,
        BuildType::BLEED_BLADE, BuildType::SHADOW_STRIKER, BuildType::JUGGERNAUT,
        BuildType::SUPPORT, BuildType::SUMMON_LORD, BuildType::PROJECTILE,
    };
    const char* kValidSfx[] = {"melee", "hit", "slash", "bolt"};
    for (BuildType bt : all) {
        const BuildTheme t = BuildTheme::from_build_type(bt);
        ASSERT_NE(t.audio.hit_sfx, nullptr) << "hit_sfx 为空: " << t.name;
        ASSERT_NE(t.audio.skill_sfx, nullptr) << "skill_sfx 为空: " << t.name;
        bool hit_ok = false, skill_ok = false;
        for (const char* v : kValidSfx) {
            if (std::strcmp(t.audio.hit_sfx, v) == 0) hit_ok = true;
            if (std::strcmp(t.audio.skill_sfx, v) == 0) skill_ok = true;
        }
        EXPECT_TRUE(hit_ok) << "hit_sfx 未注册: " << t.audio.hit_sfx << " (" << t.name << ")";
        EXPECT_TRUE(skill_ok) << "skill_sfx 未注册: " << t.audio.skill_sfx << " (" << t.name << ")";
    }
}

// ------------------------------------------------------------
// G40 第二批: VFX 速率/时长 — 同样必须真值表锁定
// (只断言"有快有慢"的弱断言在第一批已被变异测试证明锁不住退化)
// ------------------------------------------------------------

TEST(BuildTheme, VfxRateTruthTable) {
    struct Expect { BuildType bt; float speed; float life; };
    const Expect table[] = {
        {BuildType::ICE_MAGE,       0.70f, 1.05f},
        {BuildType::FIRE_MAGE,      1.30f, 0.90f},
        {BuildType::POISON_MASTER,  1.00f, 1.05f},
        {BuildType::LIGHTNING_MAGE, 1.40f, 0.85f},
        {BuildType::TIME_MASTER,    0.90f, 1.10f},
        {BuildType::BERSERKER,      1.10f, 0.90f},
        {BuildType::BLEED_BLADE,    1.05f, 0.95f},
        {BuildType::SHADOW_STRIKER, 0.85f, 1.15f},
        {BuildType::JUGGERNAUT,     0.85f, 1.10f},
        {BuildType::SUPPORT,        1.00f, 1.05f},
        {BuildType::SUMMON_LORD,    0.95f, 1.05f},
        {BuildType::PROJECTILE,     1.15f, 0.95f},
    };
    for (const Expect& e : table) {
        const BuildTheme t = BuildTheme::from_build_type(e.bt);
        EXPECT_NEAR(t.vfx.particle_speed, e.speed, 1e-5f)
            << "particle_speed 与真值表不符: " << t.name;
        EXPECT_NEAR(t.vfx.lifetime_scale, e.life, 1e-5f)
            << "lifetime_scale 与真值表不符: " << t.name;
    }
}

TEST(BuildTheme, VfxRateIsThematicallySane) {
    // 冰系最慢 / 雷系最快; 雷系最短 / 暗影最长 — 主题自洽,
    // 若哪天数据被填反了(冰比雷快), 这里能抓住
    EXPECT_LT(BuildTheme::from_build_type(BuildType::ICE_MAGE).vfx.particle_speed,
              BuildTheme::from_build_type(BuildType::LIGHTNING_MAGE).vfx.particle_speed);
    EXPECT_LT(BuildTheme::from_build_type(BuildType::LIGHTNING_MAGE).vfx.lifetime_scale,
              BuildTheme::from_build_type(BuildType::SHADOW_STRIKER).vfx.lifetime_scale);
}

TEST(BuildTheme, VfxScalesAlwaysPositive) {
    // speed/life <= 0 会让特效永不移除或时间倒流 —
    // 实现里有防呆 clamp, 但数据层就该保证合法
    const BuildType all[] = {
        BuildType::ICE_MAGE, BuildType::FIRE_MAGE, BuildType::POISON_MASTER,
        BuildType::LIGHTNING_MAGE, BuildType::TIME_MASTER, BuildType::BERSERKER,
        BuildType::BLEED_BLADE, BuildType::SHADOW_STRIKER, BuildType::JUGGERNAUT,
        BuildType::SUPPORT, BuildType::SUMMON_LORD, BuildType::PROJECTILE,
    };
    for (BuildType bt : all) {
        const BuildTheme t = BuildTheme::from_build_type(bt);
        EXPECT_GT(t.vfx.particle_speed, 0.0f) << "particle_speed 非法: " << t.name;
        EXPECT_GT(t.vfx.lifetime_scale, 0.0f) << "lifetime_scale 非法: " << t.name;
    }
}

// ------------------------------------------------------------
// G40-3: explosion_scale / spark_scale 已接线 (VFXServer 装饰特效统一出口)
// 同样用真值表锁定 —— 只断言"有差异"的弱断言已被变异测试证明锁不住
// ------------------------------------------------------------

TEST(BuildTheme, ExplosionAndSparkTruthTable) {
    struct Expect { BuildType bt; float explosion; float spark; };
    // 值由运行时探针导出 (不是照源码猜的 — 猜错过两次)
    const Expect table[] = {
        {BuildType::BERSERKER,      1.05f, 1.00f},  // bt=1 blood
        {BuildType::FIRE_MAGE,      1.25f, 1.00f},  // bt=2 fire
        {BuildType::POISON_MASTER,  1.00f, 1.00f},  // bt=3 nature
        {BuildType::TIME_MASTER,    1.00f, 1.00f},  // bt=4 ice
        {BuildType::SUPPORT,        0.95f, 1.00f},  // bt=5 nature
        {BuildType::PROJECTILE,     1.05f, 1.00f},  // bt=6 gold
        {BuildType::ICE_MAGE,       1.15f, 1.00f},  // bt=7 ice
        {BuildType::LIGHTNING_MAGE, 1.10f, 1.10f},  // bt=8 lightning
        {BuildType::BLEED_BLADE,    1.10f, 1.05f},  // bt=9 blood
        {BuildType::SHADOW_STRIKER, 0.90f, 0.85f},  // bt=10 shadow
        {BuildType::JUGGERNAUT,     0.90f, 0.90f},  // bt=11 white
    };
    for (const Expect& e : table) {
        const BuildTheme t = BuildTheme::from_build_type(e.bt);
        EXPECT_NEAR(t.vfx.explosion_scale, e.explosion, 1e-5f)
            << "explosion_scale 与真值表不符: " << t.name;
        EXPECT_NEAR(t.vfx.spark_scale, e.spark, 1e-5f)
            << "spark_scale 与真值表不符: " << t.name;
    }
}

TEST(BuildTheme, ExplosionScalesThematicallySane) {
    // 火系爆炸最大 / 暗影最小 — 主题自洽, 填反了能抓住
    EXPECT_GT(BuildTheme::from_build_type(BuildType::FIRE_MAGE).vfx.explosion_scale,
              BuildTheme::from_build_type(BuildType::SHADOW_STRIKER).vfx.explosion_scale);
    EXPECT_GT(BuildTheme::from_build_type(BuildType::FIRE_MAGE).vfx.explosion_scale, 1.0f);
    EXPECT_LT(BuildTheme::from_build_type(BuildType::SHADOW_STRIKER).vfx.explosion_scale, 1.0f);
}

// ------------------------------------------------------------
// G40-3: 仍未接线的字段 — 显式标记, 防止误以为已接
// beam_width / ring_speed: 无对应 Effect 字段, 需改绘制路径
// edge_tint / vignette_strength: 需改 HUD/overlay 绘制路径
// ------------------------------------------------------------

TEST(BuildTheme, UnwiredVfxFieldsAreStillDocumented) {
    // 本测试不锁它们的值, 只确认字段存在 (编译期契约)。
    // 接线时须补: ① 真值表 ② 运行时取证 ③ 电报安全性论证
    const BuildTheme t = BuildTheme::from_build_type(BuildType::SHADOW_STRIKER);
    (void)t.vfx.beam_width;   (void)t.vfx.ring_speed;
    (void)t.screen.edge_tint; (void)t.screen.vignette_strength;
    SUCCEED() << "beam_width/ring_speed/edge_tint/vignette_strength 仍是死数据";
}
TEST(BuildTheme, AudioSignaturesDifferAcrossBuilds) {
    // 真值表: 每个流派的 (hit_sfx, skill_sfx) 逐一钉死。
    // ★ 早期版本只断言"存在 2 种以上", 变异测试证明其过弱 —
    //   把 2/6 个流派的 signature 抹平成常量后测试依然通过。
    //   故改成显式真值表: 要改流派音效必须来这里改, 不可能静默退化。
    struct Expect { BuildType bt; const char* hit; const char* skill; };
    const Expect table[] = {
        {BuildType::ICE_MAGE,        "slash", "bolt"},
        {BuildType::FIRE_MAGE,       "melee", "bolt"},
        {BuildType::POISON_MASTER,   "slash", "bolt"},
        {BuildType::LIGHTNING_MAGE,  "bolt",  "bolt"},
        {BuildType::TIME_MASTER,     "slash", "bolt"},
        {BuildType::BERSERKER,       "melee", "melee"},
        {BuildType::BLEED_BLADE,     "melee", "slash"},
        {BuildType::SHADOW_STRIKER,  "slash", "bolt"},
        {BuildType::JUGGERNAUT,      "melee", "melee"},
        {BuildType::SUPPORT,         "slash", "bolt"},
        {BuildType::SUMMON_LORD,     "bolt",  "bolt"},
        {BuildType::PROJECTILE,      "bolt",  "bolt"},
    };
    for (const Expect& e : table) {
        const BuildTheme t = BuildTheme::from_build_type(e.bt);
        EXPECT_STREQ(t.audio.hit_sfx, e.hit)
            << "hit_sfx 与真值表不符: " << t.name;
        EXPECT_STREQ(t.audio.skill_sfx, e.skill)
            << "skill_sfx 与真值表不符: " << t.name;
    }
    // 三类 signature 都必须真的出现, 否则等于有些流派没有自己的听感
    std::set<std::string> hit_set, skill_set;
    for (const Expect& e : table) { hit_set.insert(e.hit); skill_set.insert(e.skill); }
    EXPECT_EQ(hit_set.size(), 3u) << "hit_sfx 只应有三类: melee/slash/bolt";
    EXPECT_EQ(skill_set.size(), 3u) << "skill_sfx 只应有三类: melee/slash/bolt";
}
