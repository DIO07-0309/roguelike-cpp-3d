#include <gtest/gtest.h>
#include "data/animation_defs.h"

// A5-T1: animation JSON 加载器 (纯内存 parse, 无磁盘依赖)

TEST(AnimationDefs, ParsesMinimalSkeleton) {
    auto j = nlohmann::json::parse(R"({
      "pixels_per_unit":0.5,"anchor":[24,62],
      "bones":[{"name":"root"},{"name":"hips","parent":"root","y":-28}],
      "parts":[{"bone":"hips","file":"player_part_torso.png","pivot":[16,6]}]})");
    std::string err;
    auto sk = parse_skeleton(j, err);
    ASSERT_TRUE(sk.has_value()) << err;
    EXPECT_EQ(sk->bones.size(), 2u);
    EXPECT_EQ(sk->bones[1].parent, 0);
    EXPECT_FLOAT_EQ(sk->bones[1].y, -28.f);
    EXPECT_FLOAT_EQ(sk->pixels_per_unit, 0.5f);
    EXPECT_FLOAT_EQ(sk->anchor_x, 24.f);
    EXPECT_FLOAT_EQ(sk->anchor_y, 62.f);
    EXPECT_EQ(sk->parts[0].bone, 1);
    EXPECT_FLOAT_EQ(sk->parts[0].pivot_y, 6.f);
}

TEST(AnimationDefs, RejectsUnknownBoneRef) {
    auto j = nlohmann::json::parse(R"({"bones":[{"name":"root"}],
      "parts":[{"bone":"ghost","file":"x.png"}]})");
    std::string err;
    EXPECT_FALSE(parse_skeleton(j, err).has_value());
    EXPECT_FALSE(err.empty());
}

TEST(AnimationDefs, RejectsChildBeforeParentOrder) {
    auto j = nlohmann::json::parse(R"({"bones":[
      {"name":"hips","parent":"torso"},
      {"name":"torso","parent":"root"},
      {"name":"root"}],"parts":[]})");
    std::string err;
    EXPECT_FALSE(parse_skeleton(j, err).has_value());  // 链合成前提: 父先于子声明
}

TEST(AnimationDefs, ParsesAnimTracksAndKeys) {
    nlohmann::json sk_j = nlohmann::json::parse(
        R"({"bones":[{"name":"root"},{"name":"torso","parent":"root"}],"parts":[]})");
    std::string err;
    SkeletonDef sk = *parse_skeleton(sk_j, err);
    ASSERT_TRUE(!err.empty() || sk.bones.size() == 2);
    auto a = nlohmann::json::parse(R"({"animations":{"idle":{"loop":true,"dur":2.4,
      "tracks":[{"bone":"torso","keys":[{"t":0,"rot":0},{"t":2.4,"rot":0}]}]}}})");
    auto set = parse_anim(a, sk, err);
    ASSERT_TRUE(set.has_value()) << err;
    ASSERT_TRUE(set->clips.count("idle") == 1);
    EXPECT_TRUE(set->clips["idle"].loop);
    EXPECT_FLOAT_EQ(set->clips["idle"].dur, 2.4f);
    EXPECT_EQ(set->clips["idle"].tracks[0].bone, 1);
    EXPECT_EQ(set->clips["idle"].tracks[0].keys.size(), 2u);
}

TEST(AnimationDefs, RejectsLoopMissingEndKey) {
    nlohmann::json sk_j = nlohmann::json::parse(R"({"bones":[{"name":"root"}],"parts":[]})");
    std::string err;
    SkeletonDef sk = *parse_skeleton(sk_j, err);
    auto a = nlohmann::json::parse(R"({"animations":{"idle":{"loop":true,"dur":2.4,
      "tracks":[{"bone":"root","keys":[{"t":0,"rot":0},{"t":2.0,"rot":0}]}]}}})");
    EXPECT_FALSE(parse_anim(a, sk, err).has_value());  // 末键 t=2.0 < dur=2.4
    EXPECT_FALSE(err.empty());
}

TEST(AnimationDefs, RejectsUnknownTrackBone) {
    nlohmann::json sk_j = nlohmann::json::parse(R"({"bones":[{"name":"root"}],"parts":[]})");
    std::string err;
    SkeletonDef sk = *parse_skeleton(sk_j, err);
    auto a = nlohmann::json::parse(R"({"animations":{"idle":{"loop":false,"dur":1.0,
      "tracks":[{"bone":"nope","keys":[{"t":0},{"t":1.0}]}]}}})");
    EXPECT_FALSE(parse_anim(a, sk, err).has_value());
}
