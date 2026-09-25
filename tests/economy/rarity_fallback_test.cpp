// 回归: items.json 未加载时 generate_random_item() 直接崩溃 (SIGFPE / 0xC000001C)。
//
// 根因: random_rarity() 以稀有度权重之和作除数 (rng() % total), 而 RarityConfig 原先
// 没有 in-class 默认值 → 全新进程里 g_rarity 被零初始化 → total == 0 → 除零。
// 附带: load_item_defs 的 rarity 块只写「数组里出现的下标」, 数组不足 4 项时未列出的
// 槽位同样靠零初始化兜底, 一个写短的 JSON 就会让游戏在第一次掉落处崩。
//
// 这个二进制是唯一「没有任何用例调用 load_item_defs」的测试, 因此是唯一能复现
// 空 registry 前置条件的地方。放进 challenge_room_test 会被 RewardDataJsonLoads 先加载
// items.json, 前置条件被掩盖 —— 这正是当年 tick 用例被迫排在文件靠后、--gtest_filter
// 单跑必崩的真因, 与用例顺序无关。
#include <gtest/gtest.h>

#include "item.h"
#include "data/item_defs.h"

namespace {

int rarity_weight_total() {
    const RarityConfig& rc = get_rarity_config();
    int total = 0;
    for (int i = 0; i < 4; i++) total += rc.weights[i];
    return total;
}

}  // namespace

TEST(RarityFallback, WeightTotalIsNeverZero) {
    EXPECT_GT(rarity_weight_total(), 0)
        << "weights 之和为 0 会让 random_rarity() 除零";
    const RarityConfig& rc = get_rarity_config();
    for (int i = 0; i < 4; i++)
        EXPECT_GE(rc.weights[i], 0) << "负权重会让 roll 落进错误的稀有度区间";
}

TEST(RarityFallback, ConfigDefaultsSurviveUnloadedJson) {
    // 多倍防御: 即使默认值被删掉, random_rarity() 的 total<=0 守卫也不得崩
    const RarityConfig& rc = get_rarity_config();
    EXPECT_GT(rc.mults[0], 0.0f) << "COMMON 倍率为 0 会让所有道具数值归零";
    for (int i = 0; i < 4; i++)
        EXPECT_GT(rc.mults[i], 0.0f) << "稀有度倍率为 0/负 属配置错误";
}

TEST(RarityFallback, RandomRaritySafeWithEmptyRegistry) {
    ASSERT_FALSE(is_item_defs_loaded())
        << "本二进制不得加载 items.json, 否则测不到空 registry 前置条件";
    for (int i = 0; i < 4096; i++) {
        Rarity r = random_rarity();
        EXPECT_GE((int)r, 0);
        EXPECT_LE((int)r, 3);
    }
}

TEST(RarityFallback, RandomRarityHonoursWeights) {
    // 60:25:12:3 → LEGENDARY 约 3%。区间放宽到 1%..6%, 远大于随机波动
    const int n = 200000;
    int legendary = 0;
    for (int i = 0; i < n; i++)
        if (random_rarity() == Rarity::LEGENDARY) legendary++;
    double rate = (double)legendary / (double)n;
    EXPECT_GT(rate, 0.01) << "LEGENDARY 完全不出 — 权重未生效";
    EXPECT_LT(rate, 0.06) << "LEGENDARY 过高 — 权重未生效";
}

TEST(RarityFallback, GenerateRandomItemNullWhenRegistryEmpty) {
    ASSERT_FALSE(is_item_defs_loaded());
    EXPECT_EQ(generate_random_item(), nullptr)
        << "空 registry 必须安全返回空, 而不是崩溃";
}
