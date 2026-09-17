#include <gtest/gtest.h>
#include "data/animation_defs.h"
#include "game/animation/skeleton_pose.h"

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

// ── A5-T2: skeleton_pose 求值器 ─────────────────────────────
namespace {
SkeletonDef make_two_bone() {   // root + hips(0,-28), pixels 1:1
    SkeletonDef sk; sk.pixels_per_unit = 1.f;
    sk.bones = { {"root", -1, 0, 0}, {"hips", 0, 0, -28} };
    return sk;
}
AnimClipDef one_track(int bone, KeyDef a, KeyDef b, float dur, bool loop) {
    AnimClipDef c; c.loop = loop; c.dur = dur; c.tracks = { { bone, {a, b} } };
    return c;
}
}  // namespace

TEST(SkeletonPose, BindPoseWhenNoClip) {
    auto p = compute_pose(make_two_bone(), nullptr, 0.f);
    EXPECT_FLOAT_EQ(p[1].y, -28.f); EXPECT_FLOAT_EQ(p[1].rot_deg, 0.f);
}
TEST(SkeletonPose, LinearInterpMidKey) {
    auto clip = one_track(1, {0,0,0,0,1,1}, {2,0,0,20,1,1}, 2.f, false);
    auto p = compute_pose(make_two_bone(), &clip, 1.f);
    EXPECT_FLOAT_EQ(p[1].rot_deg, 10.f);
}
TEST(SkeletonPose, LoopWrapsTime) {
    auto clip = one_track(1, {0,0,0,0,1,1}, {2,0,0,20,1,1}, 2.f, true);
    auto a = compute_pose(make_two_bone(), &clip, 2.5f);
    EXPECT_FLOAT_EQ(a[1].rot_deg, 5.f);            // t=0.5
}
TEST(SkeletonPose, ParentRotationMovesChild) {
    SkeletonDef sk = make_two_bone();
    sk.bones[1] = {"hips", 0, 0, -10};
    auto clip = one_track(0, {0,0,0,90,1,1}, {1,0,0,90,1,1}, 1.f, false);
    auto p = compute_pose(sk, &clip, 0.f);
    EXPECT_NEAR(p[1].x, 10.f, 1e-4);               // (0,-10)→(10,0)
    EXPECT_NEAR(p[1].y, 0.f, 1e-4);
}
TEST(SkeletonPose, OverlayOnRootScalesChain) {
    OverlayTf ov; ov.sx = 2.f; ov.sy = 2.f;
    auto p = compute_pose(make_two_bone(), nullptr, 0.f, ov);
    EXPECT_FLOAT_EQ(p[1].y, -56.f);
    EXPECT_FLOAT_EQ(p[1].sy, 2.f);
}

TEST(AnimationDefs, KeyXYDefaultsToBoneBind) {
    nlohmann::json sk_j = nlohmann::json::parse(
        R"({"bones":[{"name":"root"},{"name":"torso","parent":"root","y":8}],"parts":[]})");
    std::string err;
    SkeletonDef sk = *parse_skeleton(sk_j, err);
    auto a = nlohmann::json::parse(R"({"animations":{"idle":{"loop":true,"dur":2.4,
      "tracks":[{"bone":"torso","keys":[{"t":0,"rot":0},{"t":2.4,"rot":2}]}]}}})");
    auto set = parse_anim(a, sk, err);
    ASSERT_TRUE(set.has_value()) << err;
    const auto& ks = set->clips.at("idle").tracks[0].keys;
    EXPECT_FLOAT_EQ(ks[0].y, 8.f);   // 缺省 = bind, 非 0
    EXPECT_FLOAT_EQ(ks[1].y, 8.f);
}
