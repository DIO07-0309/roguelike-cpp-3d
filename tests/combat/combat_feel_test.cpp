#include <gtest/gtest.h>
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
        EXPECT_GT(t.camera.shake_scale, 0.0f) << "流派 shak_scale 非法: " << t.name;
        EXPECT_GT(t.camera.zoom_offset, -1.0f) << "流派 zoom_offset 非法: " << t.name;
        EXPECT_GE(t.camera.dash_offset, 0.0f) << "流派 dash_offset 不应为负: " << t.name;
    }
}
