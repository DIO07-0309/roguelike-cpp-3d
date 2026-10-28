#include <gtest/gtest.h>
#include "npc_system.h"

// ============================================================
// A15-Guard: 守卫互动 NPC 接入不变量
//   F1 = 守卫楼层; 模型名按对话状态切 guard_alert/guard_interacted;
//   其他楼层模型名与素材映射不变; 守卫配置存在且仅 slot0。
// ============================================================

TEST(GuardNpc, GuardFloorIsF1) {
    EXPECT_TRUE(is_guard_floor(1));
    EXPECT_FALSE(is_guard_floor(2));
    EXPECT_FALSE(is_guard_floor(14));
    EXPECT_FALSE(is_guard_floor(0));
}

TEST(GuardNpc, ModelKeySwitchesOnDialogueState) {
    // 守卫楼层: 未对话 → 警戒, 已对话 → 休息
    EXPECT_STREQ(npc_model_key(1, false), "guard_alert");
    EXPECT_STREQ(npc_model_key(1, true), "guard_interacted");
}

TEST(GuardNpc, OtherFloorsUnchanged) {
    // 非守卫楼层: 模型名不受对话状态影响, 且与素材映射一致
    EXPECT_STREQ(npc_model_key(2, false), "npc_1");
    EXPECT_STREQ(npc_model_key(2, true), "npc_1");   // finished 不改变非守卫
    EXPECT_STREQ(npc_model_key(14, false), "npc_blacksmith");
    EXPECT_STREQ(npc_sprite_key(2), "npc_1");
    EXPECT_STREQ(npc_sprite_key(3), "npc_2");
}

TEST(GuardNpc, GuardConfigExistsAtSlot0Only) {
    const NPCData* cfg = get_npc_config(1, 0);
    ASSERT_NE(cfg, nullptr) << "守卫配置必须存在于 F1 slot0";
    EXPECT_NE(cfg->name, nullptr);
    EXPECT_GT(cfg->first_dialogue[0] ? std::char_traits<char>::length(cfg->first_dialogue[0]) : 0, 0);
    EXPECT_EQ(get_npc_config(1, 1), nullptr) << "守卫只占 slot0";
    EXPECT_EQ(get_npc_config(1, 2), nullptr);
}
