// 纯状态机回归 — tutorial 阶段链 / boss cinematic / boss timeline
// 全部为无 gameplay 依赖的纯逻辑, 不动 RNG / sim 基线.
#include <gtest/gtest.h>

#include <string>

#include "game/tutorial/tutorial_guide.h"
#include "game/boss/boss_cinematic.h"
#include "game/boss/boss_timeline.h"

TEST(TutorialGuide, AdvanceStageFullChain) {
    TutorialGuide g;
    EXPECT_EQ(g.stage, TutorialStage::WELCOME);
    const TutorialStage chain[] = {
        TutorialStage::ELEMENT, TutorialStage::MOVE, TutorialStage::PICKUP,
        TutorialStage::INVENTORY, TutorialStage::EQUIP, TutorialStage::ATTACK_COMBO,
        TutorialStage::SKILL, TutorialStage::COOLDOWN, TutorialStage::WEAPON_INFO,
        TutorialStage::COMPLETE
    };
    for (auto expected : chain) {
        g.advance_stage();
        EXPECT_EQ(g.stage, expected);
    }
    g.advance_stage();  // COMPLETE 之后不再前进
    EXPECT_EQ(g.stage, TutorialStage::COMPLETE);
}

TEST(TutorialGuide, InstructionsNonEmptyForEveryStage) {
    TutorialGuide g;
    while (g.stage != TutorialStage::COMPLETE) {
        EXPECT_FALSE(g.get_instructions().empty()) << "stage " << (int)g.stage;
        g.advance_stage();
    }
    EXPECT_FALSE(g.get_instructions().empty());  // COMPLETE 也有结语文案
}

TEST(BossCinematic, LifecyclePhases) {
    BossCinematicController c;
    EXPECT_FALSE(c.is_running());
    EXPECT_FALSE(c.should_draw_overlay());
    c.start_intro();
    EXPECT_TRUE(c.is_running());
    EXPECT_EQ(c.phase(), CinematicPhase::INTRO);
    EXPECT_TRUE(c.should_draw_overlay());
    EXPECT_TRUE(c.should_freeze_player());
    c.trigger_phase2();
    EXPECT_EQ(c.phase(), CinematicPhase::PHASE2_TRANSITION);
    c.trigger_last_stand();
    EXPECT_EQ(c.phase(), CinematicPhase::LAST_STAND);
    c.trigger_death();
    EXPECT_EQ(c.phase(), CinematicPhase::DEATH);
}

TEST(BossCinematic, TickCountDown) {
    BossCinematicController c;
    c.start_intro();
    float t0 = c.timer();
    c.tick(0.01f);
    EXPECT_FLOAT_EQ(c.timer(), t0 - 0.01f);  // 每帧扣减 dt
    EXPECT_STREQ(c.phase_name(), "INTRO");
}

TEST(BossTimeline, RecordClearLast) {
    BossTimeline tl;
    EXPECT_EQ(tl.count(), 0);
    EXPECT_STREQ(tl.last_label(), "NONE");  // 空时间线返回占位串
    tl.record(1.5f, "intro");
    tl.record(3.0f, "phase2");
    EXPECT_EQ(tl.count(), 2);
    ASSERT_NE(tl.last_label(), nullptr);
    EXPECT_STREQ(tl.last_label(), "phase2");
    ASSERT_EQ(tl.events().size(), 2u);
    EXPECT_FLOAT_EQ(tl.events()[0].time, 1.5f);
    EXPECT_STREQ(tl.events()[1].label, "phase2");
    tl.clear();
    EXPECT_EQ(tl.count(), 0);
    EXPECT_STREQ(tl.last_label(), "NONE");
}
