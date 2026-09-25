#include <gtest/gtest.h>
#include "data/animation_defs.h"
#include "game/animation/skeleton_pose.h"
#include "game/animation/avatar_animator.h"
#include "game/rendering3d/hd2d_renderer.h"
#include "game/rendering3d/hd2d_part_geometry.h"
#include "game/animation/player_avatar.h"
#include "game/animation/skeleton_avatar.h"
#include "data/actor_avatar_defs.h"
#include "entities/monster.h"
#include "entities/ai.h"
#include "game/rendering3d/hd2d_scene_builder.h"
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <limits>
#include <set>
#include <string>
#include "raymath.h"
#include "rlgl.h"

namespace {
struct PartSubmission {
    bool depth_write = true;
    unsigned int texture = 0;
    Vector2 uv{};
    Color tint{};
    std::vector<Vector3> positions;
    std::vector<Vector2> uvs;
    std::vector<unsigned int> textures;
    std::vector<bool> vertex_depth_writes;
    std::vector<bool> flush_depth_writes;
    std::vector<size_t> flush_vertex_counts;
};
PartSubmission part_submission;
struct ShaderSubmission {
    int missing_location = -1;
    unsigned int active_shader = 0;
    std::vector<float> values = {-1, -1, -1};
};
ShaderSubmission shader_submission;
}

extern "C" {
void rlSetTexture(unsigned int texture) { part_submission.texture = texture; }
void rlBegin(int) {}
void rlEnd() {}
void rlNormal3f(float, float, float) {}
void rlColor4ub(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha) {
    part_submission.tint = {red, green, blue, alpha};
}
void rlTexCoord2f(float horizontal, float vertical) { part_submission.uv = {horizontal, vertical}; }
void rlVertex3f(float horizontal, float vertical, float depth) {
    part_submission.positions.push_back({horizontal, vertical, depth});
    part_submission.uvs.push_back(part_submission.uv);
    part_submission.textures.push_back(part_submission.texture);
    part_submission.vertex_depth_writes.push_back(part_submission.depth_write);
}
void rlDrawRenderBatchActive() {
    part_submission.flush_depth_writes.push_back(part_submission.depth_write);
    part_submission.flush_vertex_counts.push_back(part_submission.positions.size());
}
void rlDisableDepthMask() { part_submission.depth_write = false; }
void rlEnableDepthMask() { part_submission.depth_write = true; }
unsigned int rlGetShaderIdDefault() { return 1; }
int GetShaderLocation(Shader, const char* name) {
    const std::string uniform_name = name;
    const int location = uniform_name == "uTexelOffset" ? 0
        : uniform_name == "uAlphaThreshold" ? 1 : 2;
    return location == shader_submission.missing_location ? -1 : location;
}
void SetShaderValue(Shader, int location, const void* value, int) {
    shader_submission.values.at(location) = *static_cast<const float*>(value);
}
void BeginShaderMode(Shader shader) {
    rlDrawRenderBatchActive();
    shader_submission.active_shader = shader.id;
}
void EndShaderMode() {
    rlDrawRenderBatchActive();
    shader_submission.active_shader = 0;
}
}

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

// ── A5-T3: AvatarAnimator 选择/计时 ─────────────────────────
TEST(AvatarAnimator, AttackOverridesThenFallsBackToWalk) {
    AvatarAnimator an; AnimInput in; in.moving = true;
    an.advance(0.1f, in);
    EXPECT_EQ(an.current_name(), "walk");
    in.attacking = true;
    an.advance(0.05f, in);
    EXPECT_EQ(an.current_name(), "attack");
    EXPECT_FLOAT_EQ(an.time(), 0.05f);                       // 切换即从头计时
    in.attacking = false;
    for (int i = 0; i < 8; ++i) an.advance(0.05f, in);       // 0.4s > 0.36 播完
    EXPECT_EQ(an.current_name(), "walk");                    // 回落到移动态
}
TEST(AvatarAnimator, HitDuringAttackDoesNotInterrupt) {
    AvatarAnimator an; AnimInput in; in.attacking = true;
    an.advance(0.1f, in);
    in.hit_flash = true; an.advance(0.05f, in);
    EXPECT_EQ(an.current_name(), "attack");                  // attack > hit
    in.attacking = false; an.advance(0.5f, in);              // attack 播完
    EXPECT_EQ(an.current_name(), "hit");                     // hit 锁定, 此时进入
}
TEST(AvatarAnimator, HitIsEdgeTriggered) {
    AvatarAnimator an; AnimInput in;
    in.hit_flash = true; an.advance(0.05f, in);
    for (int i = 0; i < 4; ++i) an.advance(0.05f, in);       // 0.25s ≥ 0.18 播完
    in.hit_flash = true; an.advance(0.05f, in);              // bool 持续为真 ≠ 重触发
    EXPECT_EQ(an.current_name(), "idle");
    in.hit_flash = false; an.advance(0.05f, in);
    in.hit_flash = true;  an.advance(0.01f, in);
    EXPECT_EQ(an.current_name(), "hit");                     // 新上升沿才触发
}
namespace {
Camera3D makePartCamera() {
    return {{0, 0, 10}, {0, 0, 0}, {0, 1, 0}, 50, CAMERA_PERSPECTIVE};
}

HD2DDrawItem makePartItem() {
    HD2DDrawItem item;
    item.pro_mode = true;
    item.world_pos = {10, 20, 30};
    item.part_offset = {3, 5};
    item.size = 4;
    item.height = 6;
    item.pivot_uv_px = {1, 2};
    item.texture = {1, 100, 200, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    item.tex_src = {10, 40, 20, 60};
    return item;
}

void expectPartPoint(Vector3 actual, Vector3 expected) {
    EXPECT_NEAR(actual.x, expected.x, 1e-4f);
    EXPECT_NEAR(actual.y, expected.y, 1e-4f);
    EXPECT_NEAR(actual.z, expected.z, 1e-4f);
}
}

TEST(HD2DPartGeometry, NoncentralPivotUsesFeetAndOffset) {
    const auto quad = hd2d::buildPartQuad(makePartItem(), makePartCamera());
    ASSERT_TRUE(quad.has_value());
    expectPartPoint(quad->positions[0], {12, 27, 30});
    expectPartPoint(quad->positions[1], {12, 21, 30});
    expectPartPoint(quad->positions[2], {16, 21, 30});
    expectPartPoint(quad->positions[3], {16, 27, 30});
}

TEST(HD2DPartGeometry, ClockwiseRotationDoesNotRotateOffset) {
    auto item = makePartItem();
    item.rot_deg = 90;
    const auto quad = hd2d::buildPartQuad(item, makePartCamera());
    ASSERT_TRUE(quad.has_value());
    expectPartPoint(quad->positions[0], {15, 26, 30});
    expectPartPoint(quad->positions[1], {9, 26, 30});
    expectPartPoint(quad->positions[2], {9, 22, 30});
    expectPartPoint(quad->positions[3], {15, 22, 30});
}

TEST(HD2DPartGeometry, AtlasFlipChangesOnlyU) {
    auto item = makePartItem();
    const auto normal = hd2d::buildPartQuad(item, makePartCamera());
    item.flip_x = true;
    const auto flipped = hd2d::buildPartQuad(item, makePartCamera());
    ASSERT_TRUE(normal.has_value());
    ASSERT_TRUE(flipped.has_value());
    const Vector2 expected_uv[] = {{0.1f, 0.2f}, {0.1f, 0.5f},
                                  {0.3f, 0.5f}, {0.3f, 0.2f}};
    for (int corner = 0; corner < 4; ++corner) {
        expectPartPoint(flipped->positions[corner], normal->positions[corner]);
        EXPECT_FLOAT_EQ(normal->uvs[corner].x, expected_uv[corner].x);
        EXPECT_FLOAT_EQ(normal->uvs[corner].y, expected_uv[corner].y);
        EXPECT_NEAR(flipped->uvs[corner].x, 0.4f - expected_uv[corner].x, 1e-6f);
        EXPECT_FLOAT_EQ(flipped->uvs[corner].y, expected_uv[corner].y);
    }
}

TEST(HD2DPartGeometry, MainCameraBasisIsSharedAcrossFeetAnchors) {
    auto camera = makePartCamera();
    camera.position = {10, 10, 0};
    auto item = makePartItem();
    item.pivot_uv_px = {0, 0};
    const auto first = hd2d::buildPartQuad(item, camera);
    ASSERT_TRUE(first.has_value());
    const float diagonal = std::sqrt(0.5f);
    expectPartPoint(first->positions[0], {10 - 5 * diagonal, 20 + 5 * diagonal, 27});
    item.world_pos = {-80, 50, 120};
    const auto second = hd2d::buildPartQuad(item, camera);
    ASSERT_TRUE(second.has_value());
    for (int corner = 0; corner < 4; ++corner) {
        auto expected = first->positions[corner];
        expected.x -= 90; expected.y += 30; expected.z += 90;
        expectPartPoint(second->positions[corner], expected);
    }
}

TEST(HD2DPartGeometry, CounterclockwiseForRotatedAndFlippedParts) {
    for (float angle : {0.f, 37.f, 90.f, -90.f, 180.f}) {
        auto item = makePartItem();
        item.rot_deg = angle;
        item.flip_x = true;
        const auto quad = hd2d::buildPartQuad(item, makePartCamera());
        ASSERT_TRUE(quad.has_value());
        const auto& positions = quad->positions;
        const float edge1_x = positions[1].x - positions[0].x;
        const float edge1_y = positions[1].y - positions[0].y;
        const float edge2_x = positions[2].x - positions[0].x;
        const float edge2_y = positions[2].y - positions[0].y;
        EXPECT_NEAR(edge1_x * edge2_y - edge1_y * edge2_x, 24.f, 1e-4f);
    }
}

TEST(HD2DPartGeometry, AlreadyScaledDimensionsAreNotScaledAgain) {
    auto item = makePartItem();
    item.scale_w = 3; item.scale_h = 7;
    const auto quad = hd2d::buildPartQuad(item, makePartCamera());
    ASSERT_TRUE(quad.has_value());
    expectPartPoint(quad->positions[0], {12, 27, 30});
    expectPartPoint(quad->positions[2], {16, 21, 30});
}

TEST(HD2DPartGeometry, EmptySourceUsesWholeTexture) {
    auto item = makePartItem();
    item.tex_src = {};
    const auto quad = hd2d::buildPartQuad(item, makePartCamera());
    ASSERT_TRUE(quad.has_value());
    EXPECT_FLOAT_EQ(quad->uvs[0].x, 0);
    EXPECT_FLOAT_EQ(quad->uvs[0].y, 0);
    EXPECT_FLOAT_EQ(quad->uvs[2].x, 1);
    EXPECT_FLOAT_EQ(quad->uvs[2].y, 1);
}

TEST(HD2DPartGeometry, RejectsMissingTextureAndDegenerateCamera) {
    auto item = makePartItem();
    item.texture.id = 0;
    EXPECT_FALSE(hd2d::buildPartQuad(item, makePartCamera()).has_value());
    item = makePartItem(); item.texture.width = 0;
    EXPECT_FALSE(hd2d::buildPartQuad(item, makePartCamera()).has_value());
    item = makePartItem(); item.height = 0;
    EXPECT_FALSE(hd2d::buildPartQuad(item, makePartCamera()).has_value());
    auto camera = makePartCamera(); camera.target = camera.position;
    EXPECT_FALSE(hd2d::buildPartQuad(makePartItem(), camera).has_value());
    camera = makePartCamera(); camera.up = {0, 0, 1};
    EXPECT_FALSE(hd2d::buildPartQuad(makePartItem(), camera).has_value());
}

TEST(AvatarPartGeometry, PoseScaleAndOffsetAreAppliedOnce) {
    PartDef part;
    part.dx = 2; part.dy = 3;
    part.pivot_x = 1; part.pivot_y = 2;
    const Texture2D texture{1, 10, 20, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    const WorldBone bone{7, 11, 90, 2, 3};
    const auto geometry = buildAvatarPart(part, bone, texture, 0.5f, false);
    EXPECT_NEAR(geometry.offset.x, -1.f, 1e-4f);
    EXPECT_NEAR(geometry.offset.y, 7.5f, 1e-4f);
    EXPECT_FLOAT_EQ(geometry.size.x, 10);
    EXPECT_FLOAT_EQ(geometry.size.y, 30);
    EXPECT_FLOAT_EQ(geometry.pivot.x, 1);
    EXPECT_FLOAT_EQ(geometry.pivot.y, 27);
    EXPECT_FLOAT_EQ(geometry.rot_deg, -90);
    EXPECT_FALSE(geometry.flip_x);
}

TEST(AvatarPartGeometry, FacingMirrorsBonesPivotRotationAndUV) {
    PartDef part;
    part.pivot_x = 1; part.pivot_y = 2;
    const Texture2D texture{1, 10, 20, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    const WorldBone bone{7, 11, 90, 2, 3};
    const auto right = buildAvatarPart(part, bone, texture, 0.5f, false);
    const auto left = buildAvatarPart(part, bone, texture, 0.5f, true);
    EXPECT_FLOAT_EQ(left.offset.x, -right.offset.x);
    EXPECT_FLOAT_EQ(left.offset.y, right.offset.y);
    EXPECT_FLOAT_EQ(left.pivot.x, right.size.x - right.pivot.x);
    EXPECT_FLOAT_EQ(left.pivot.y, right.pivot.y);
    EXPECT_FLOAT_EQ(left.rot_deg, -right.rot_deg);
    EXPECT_TRUE(left.flip_x);
}

TEST(AvatarPartGeometry, RootOverlayMovesChildAndScalesPart) {
    auto skeleton = make_two_bone();
    skeleton.bones[1] = {"hips", 0, 4, 10};
    OverlayTf overlay;
    overlay.sx = 2; overlay.sy = 3; overlay.rot_deg = 90;
    const auto pose = compute_pose(skeleton, nullptr, 0, overlay);
    const Texture2D texture{1, 10, 20, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    const auto part = buildAvatarPart({}, pose[1], texture, 0.5f, false);
    EXPECT_NEAR(part.offset.x, -15, 1e-4f);
    EXPECT_NEAR(part.offset.y, 4, 1e-4f);
    EXPECT_FLOAT_EQ(part.size.x, 10);
    EXPECT_FLOAT_EQ(part.size.y, 30);
    EXPECT_FLOAT_EQ(part.pivot.y, 30);
}

TEST(HD2DAvatarBuilder, PartsKeepFeetAnchorOrderAndSingleBlob) {
    AvatarPartDraw back, front;
    back.tex = {1, 10, 20, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    back.offset = {3, 5}; back.size = {10, 20}; back.pivot = {2, 7};
    back.rot_deg = 90; back.flip_x = true;
    front = back; front.tex.id = 2;
    std::vector<HD2DDrawItem> items;
    hd2d::appendAvatarParts({back, front}, {10, 0, 30}, 25, 255, 36, items);
    ASSERT_EQ(items.size(), 2u);
    for (const auto& item : items) {
        EXPECT_TRUE(item.pro_mode);
        EXPECT_FALSE(item.outline);
        EXPECT_TRUE(item.flip_x);
        EXPECT_EQ(item.kind, HD2DDrawItem::Kind::ENTITY_BILLBOARD);
        expectPartPoint(item.world_pos, {10, 0, 30});
        EXPECT_FLOAT_EQ(item.part_offset.x, 3);
        EXPECT_FLOAT_EQ(item.part_offset.y, 5);
        EXPECT_FLOAT_EQ(item.pivot_uv_px.x, 2);
        EXPECT_FLOAT_EQ(item.pivot_uv_px.y, 7);
        EXPECT_FLOAT_EQ(item.size, 10);
        EXPECT_FLOAT_EQ(item.height, 20);
        EXPECT_FLOAT_EQ(item.rot_deg, 90);
        EXPECT_FLOAT_EQ(item.sort_y, 25);
    }
    EXPECT_EQ(items[0].texture.id, 1u);
    EXPECT_EQ(items[1].texture.id, 2u);
    EXPECT_FLOAT_EQ(items[0].blob_width, 36);
    EXPECT_FLOAT_EQ(items[1].blob_width, 0);
}

TEST(HD2DAvatarBuilder, GhostPartsHaveOwnAnchorAlphaAndNoBlob) {
    AvatarPartDraw part;
    part.tex = {1, 10, 20, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    part.offset = {-3, 8}; part.size = {10, 20};
    std::vector<HD2DDrawItem> items;
    hd2d::appendAvatarParts({part, part}, {50, 0, 60}, 55, 80, 0, items);
    ASSERT_EQ(items.size(), 2u);
    for (const auto& item : items) {
        expectPartPoint(item.world_pos, {50, 0, 60});
        EXPECT_EQ(item.tint.a, 80);
        EXPECT_FLOAT_EQ(item.blob_width, 0);
        EXPECT_FLOAT_EQ(item.part_offset.y, 8);
    }
}

TEST(HD2DPartColor, MissingShaderOrUniformDisablesSubmission) {
    for (unsigned int shader_id : {0u, 1u, 7u}) {
        for (int missing_location : {-1, 0, 1, 2}) {
            shader_submission = {};
            part_submission = {};
            shader_submission.missing_location = missing_location;
            hd2d::PartColorShader shader;
            const bool valid = shader_id == 7 && missing_location == -1;
            EXPECT_EQ(shader.initialize({shader_id, nullptr}), valid);
            EXPECT_EQ(shader.ready(), valid);
            shader.draw(makePartItem(), makePartCamera());
            EXPECT_EQ(part_submission.positions.size(), valid ? 4u : 0u);
            EXPECT_TRUE(part_submission.depth_write);
            EXPECT_EQ(shader_submission.active_shader, 0u);
        }
    }
}

TEST(HD2DPartColor, ResetsOutlineAndShadowUniformsForEveryPart) {
    shader_submission = {};
    hd2d::PartColorShader shader;
    ASSERT_TRUE(shader.initialize({7, nullptr}));
    for (unsigned char alpha : {120, 255}) {
        part_submission = {};
        shader_submission.values = {0.02f, 0.1f, 1.f};
        auto item = makePartItem();
        item.tint.a = alpha;
        shader.draw(item, makePartCamera());
        EXPECT_FLOAT_EQ(shader_submission.values[0], 0.f);
        EXPECT_FLOAT_EQ(shader_submission.values[1], 0.5f);
        EXPECT_FLOAT_EQ(shader_submission.values[2], 0.f);
        ASSERT_EQ(part_submission.vertex_depth_writes.size(), 4u);
        for (bool depth_write : part_submission.vertex_depth_writes)
            EXPECT_EQ(depth_write, alpha == 255);
        EXPECT_TRUE(part_submission.depth_write);
        EXPECT_EQ(shader_submission.active_shader, 0u);
    }
}

TEST(HD2DPartColor, FailedReinitializationClearsPreviouslyValidShader) {
    shader_submission = {};
    part_submission = {};
    hd2d::PartColorShader shader;
    ASSERT_TRUE(shader.initialize({7, nullptr}));
    EXPECT_FALSE(shader.initialize({1, nullptr}));
    EXPECT_FALSE(shader.ready());
    shader.draw(makePartItem(), makePartCamera());
    EXPECT_TRUE(part_submission.positions.empty());
}

TEST(HD2DPartColor, GhostOrderIsBackToFrontAndPreservesCoplanarPartOrder) {
    std::vector<HD2DDrawItem> items(6, makePartItem());
    for (auto& item : items) {
        item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        item.tint.a = 120;
    }
    items[0].world_pos = {0, 0, 8};
    items[1].tint.a = 255;
    items[2].world_pos = {0, 0, -4};
    items[3].pro_mode = false;
    items[4].world_pos = {0, 0, -4};
    items[5].kind = HD2DDrawItem::Kind::FX_QUAD;
    const auto ghosts = hd2d::orderedGhostParts(items, makePartCamera());
    ASSERT_EQ(ghosts.size(), 3u);
    EXPECT_EQ(ghosts[0], &items[2]);
    EXPECT_EQ(ghosts[1], &items[4]);
    EXPECT_EQ(ghosts[2], &items[0]);
    EXPECT_FALSE(hd2d::isGhostPart(items[1]));
    EXPECT_FALSE(hd2d::isGhostPart(items[3]));
}

TEST(HD2DPartGeometry, OpaqueColorSubmissionWritesDepth) {
    part_submission = {};
    hd2d::drawPartQuad(makePartItem(), makePartCamera());
    ASSERT_EQ(part_submission.vertex_depth_writes.size(), 4u);
    for (bool depth_write : part_submission.vertex_depth_writes)
        EXPECT_TRUE(depth_write);
    EXPECT_TRUE(part_submission.depth_write);
    for (bool depth_write : part_submission.flush_depth_writes)
        EXPECT_TRUE(depth_write);
}

TEST(HD2DPartGeometry, GhostColorSubmissionDoesNotWriteDepthAndRestoresMask) {
    for (unsigned char alpha : {1, 120, 127, 128, 254}) {
        part_submission = {};
        auto item = makePartItem();
        item.tint.a = alpha;
        hd2d::drawPartQuad(item, makePartCamera());
        ASSERT_EQ(part_submission.vertex_depth_writes.size(), 4u);
        for (bool depth_write : part_submission.vertex_depth_writes)
            EXPECT_FALSE(depth_write);
        EXPECT_TRUE(part_submission.depth_write);
        ASSERT_EQ(part_submission.flush_depth_writes.size(), 2u);
        EXPECT_TRUE(part_submission.flush_depth_writes[0]);
        EXPECT_FALSE(part_submission.flush_depth_writes[1]);
        EXPECT_EQ(part_submission.flush_vertex_counts, (std::vector<size_t>{0, 4}));
    }
}

TEST(HD2DPartGeometry, ShadowSubmissionUsesTexturedTintedQuadAndSkipsGhosts) {
    part_submission = {};
    auto item = makePartItem();
    item.tint = {10, 20, 30, 255};
    item.flip_x = true;
    const auto quad = hd2d::buildPartQuad(item, makePartCamera());
    ASSERT_TRUE(quad);
    hd2d::drawPartQuad(item, makePartCamera(), hd2d::PartPass::Shadow);
    ASSERT_EQ(part_submission.positions.size(), 4u);
    for (size_t index = 0; index < 4; ++index) {
        expectPartPoint(part_submission.positions[index], quad->positions[index]);
        EXPECT_FLOAT_EQ(part_submission.uvs[index].x, quad->uvs[index].x);
        EXPECT_FLOAT_EQ(part_submission.uvs[index].y, quad->uvs[index].y);
        EXPECT_EQ(part_submission.textures[index], item.texture.id);
        EXPECT_TRUE(part_submission.vertex_depth_writes[index]);
    }
    EXPECT_EQ(part_submission.tint.r, 10);
    EXPECT_EQ(part_submission.tint.g, 20);
    EXPECT_EQ(part_submission.tint.b, 30);
    EXPECT_EQ(part_submission.tint.a, 255);
    EXPECT_EQ(part_submission.texture, 0u);
    for (unsigned char alpha : {0, 120, 127}) {
        part_submission = {};
        item.tint.a = alpha;
        hd2d::drawPartQuad(item, makePartCamera(), hd2d::PartPass::Shadow);
        EXPECT_TRUE(part_submission.positions.empty());
        EXPECT_TRUE(part_submission.depth_write);
    }
}

TEST(HD2DPartGeometry, RealIdlePartsStayAboveFeetWithoutUnitRescaling) {
    std::string error;
    const auto skeleton = load_skeleton_file("resources/animations/player_skeleton.json", error);
    ASSERT_TRUE(skeleton) << error;
    const auto animations = load_anim_file("resources/animations/player_anim.json", *skeleton, error);
    ASSERT_TRUE(animations) << error;
    const auto pose = compute_pose(*skeleton, &animations->clips.at("idle"), 0);
    const Vector3 feet{320, 0, 640};
    for (const auto& part : skeleton->parts) {
        Image image = LoadImage(part.file.c_str());
        ASSERT_NE(image.data, nullptr) << part.file;
        const Texture2D texture{1, image.width, image.height, 1, image.format};
        UnloadImage(image);
        const auto geometry = buildAvatarPart(part, pose[part.bone], texture,
                                               skeleton->pixels_per_unit, false);
        std::vector<HD2DDrawItem> items;
        hd2d::appendAvatarParts({geometry}, feet, 640, 255, 0, items);
        const auto quad = hd2d::buildPartQuad(items.front(), makePartCamera());
        ASSERT_TRUE(quad);
        float bottom = std::numeric_limits<float>::max(), top = -bottom;
        for (const auto& position : quad->positions) {
            bottom = std::min(bottom, position.y);
            top = std::max(top, position.y);
        }
        EXPECT_NEAR(top - bottom, texture.height * skeleton->pixels_per_unit, 1e-4f);
        const auto area = Vector3CrossProduct(Vector3Subtract(quad->positions[1], quad->positions[0]),
                                              Vector3Subtract(quad->positions[2], quad->positions[0]));
        EXPECT_GT(Vector3Length(area), 0.f);
        if (part.file.find("leg") != std::string::npos) {
            EXPECT_NEAR(bottom, -1.6, 1e-4f); EXPECT_NEAR(top, 14.4, 1e-4f);
        } else if (part.file.find("head") != std::string::npos) {
            EXPECT_NEAR(bottom, 17.6, 1e-4f); EXPECT_NEAR(top, 33.6, 1e-4f);
        }
    }
}

TEST(AvatarAnimator, HeavyScalesDuration) {
    AvatarAnimator an; AnimInput in;
    in.attacking = true; in.attack_recovery_ratio = 1.8f;
    an.advance(0.2f, in);
    EXPECT_FLOAT_EQ(an.dur_scale(), 1.8f);
    const auto clip = one_track(1, {0,0,0,0,1,1}, {0.36f,0,0,36,1,1}, 0.36f, false);
    const auto normal_pose = compute_pose(make_two_bone(), &clip, an.time());
    const auto heavy_pose = compute_pose(make_two_bone(), &clip, an.pose_time());
    EXPECT_NEAR(heavy_pose[1].rot_deg, normal_pose[1].rot_deg / 1.8f, 1e-4f);
    EXPECT_LT(heavy_pose[1].rot_deg, normal_pose[1].rot_deg);
    EXPECT_NEAR(an.pose_time(), 0.2f / 1.8f, 1e-6f);
    an.advance(0.2f, in);
    const auto late_pose = compute_pose(make_two_bone(), &clip, an.pose_time());
    EXPECT_NEAR(late_pose[1].rot_deg, 40.f / 1.8f, 1e-4f);
    in.attacking = false;
    for (int i = 0; i < 4; ++i) an.advance(0.05f, in);       // 累计 0.6 < 0.648
    EXPECT_EQ(an.current_name(), "attack");                  // 仍在播
    for (int i = 0; i < 8; ++i) an.advance(0.05f, in);       // 0.98 > 0.648
    EXPECT_EQ(an.current_name(), "idle");
}

// ���� A6-S1: actor_avatars.json ������ (���ڴ� parse, �� GL ����) ����������
TEST(ActorAvatarDefs, ParsesWhitelistEntries) {
    auto j = nlohmann::json::parse(R"({"actors":{"mon_fire_imp":{
      "skeleton":"resources/animations/imp_skeleton.json","anim":"resources/animations/imp_anim.json"}}})");
    std::string err;
    auto out = parse_actor_avatars(j, err);
    ASSERT_TRUE(out.has_value()) << err;
    ASSERT_EQ(out->size(), 1u);
    EXPECT_EQ((*out)["mon_fire_imp"].skeleton, "resources/animations/imp_skeleton.json");
    EXPECT_EQ((*out)["mon_fire_imp"].anim, "resources/animations/imp_anim.json");
}
TEST(ActorAvatarDefs, EmptyOrDefaultActorsMeanFullFallback) {
    std::string err;
    auto empty = parse_actor_avatars(nlohmann::json::parse(R"({"actors":{}})"), err);
    ASSERT_TRUE(empty.has_value()) << err;
    EXPECT_TRUE(empty->empty());
    auto absent = parse_actor_avatars(nlohmann::json::parse(R"({})"), err);
    ASSERT_TRUE(absent.has_value()) << err;   // ȱ actors �� = ȫ����, �Ǵ���
    EXPECT_TRUE(absent->empty());
}
TEST(ActorAvatarDefs, RejectsEntryMissingRequiredField) {
    auto j = nlohmann::json::parse(R"({"actors":{"mon_orc":{"skeleton":"a.json"}}})");
    std::string err;
    EXPECT_FALSE(parse_actor_avatars(j, err).has_value());
    EXPECT_FALSE(err.empty());
}
TEST(ActorAvatarDefs, RepoDefaultWhitelistCoversA6HumanoidFamily) {
    std::string err;
    auto out = load_actor_avatars_file("resources/animations/actor_avatars.json", err);
    ASSERT_TRUE(out.has_value()) << err;
    // A6-S2 批次1-7,8: 人形/软体/浮灵魔像/人形补充/影武者毒液蠕虫/5 Boss（NPC 骨骼化已回退，见 README）
    const std::set<std::string> expected = {"mon_orc", "mon_elite_orc", "mon_archer",
                                            "mon_shaman", "mon_goblin_hunter", "mon_tank",
                                            "mon_bone_soldier", "mon_skeleton_archer",
                                            "mon_slime", "mon_bomber", "mon_elite_slime",
                                            "mon_frost_slime", "mon_blood_leech",
                                            "mon_golem", "mon_stone_guardian",
                                            "mon_iron_sentinel", "mon_lightning_orb",
                                            "mon_fire_imp", "mon_storm_elemental",
                                            "mon_void_walker", "mon_charger",
                                            "mon_summoner", "mon_necromancer",
                                            "mon_ice_warden", "mon_blood_priest",
                                            "mon_dark_mage", "mon_shadow_stalker",
                                            "mon_shadow_assassin", "mon_night_stalker",
                                            "mon_poison_wyrm",
                                            "boss_shadow_knight", "boss_necromancer",
                                            "boss_vampire", "boss_fire_demon", "boss_golem",
                                            "boss_self"};
    ASSERT_EQ(out->size(), expected.size());
    for (const auto& key : expected) {
        auto it = out->find(key);
        ASSERT_NE(it, out->end()) << key;
        EXPECT_FALSE(it->second.skeleton.empty());
        EXPECT_FALSE(it->second.anim.empty());
        EXPECT_TRUE(std::filesystem::exists(it->second.skeleton)) << key;
        EXPECT_TRUE(std::filesystem::exists(it->second.anim)) << key;
        std::string parse_err;
        auto sk = load_skeleton_file(it->second.skeleton, parse_err);
        ASSERT_TRUE(sk.has_value()) << key << " " << parse_err;
        EXPECT_EQ(sk->bones.size(), 9u) << key;
        EXPECT_EQ(sk->parts.size(), 7u) << key;
        auto anim = load_anim_file(it->second.anim, *sk, parse_err);
        ASSERT_TRUE(anim.has_value()) << key << " " << parse_err;
    }
}

// ���� A6-S1: SkeletonAvatar ͨ�ú��� (hp ���ػ��� + ��Ⱦ�㳯��) ����������
TEST(SkeletonAvatarCore, HpFallEdgeSingleTrigger) {
    SkeletonAvatar sk;
    EXPECT_FALSE(sk.hp_hit_edge(10));        // �״ν���¼����
    EXPECT_FALSE(sk.hp_hit_edge(12));        // �����ز�����
    EXPECT_TRUE(sk.hp_hit_edge(8));          // �½��� = �ܻ�
    EXPECT_FALSE(sk.hp_hit_edge(8));         // ��ƽ���ش���
}
TEST(SkeletonAvatarCore, FacingTracksHorizontalMotion) {
    SkeletonAvatar sk;
    sk.track_facing({0, 0});
    sk.track_facing({-4, 2});                // x λ��Ϊ�� �� ����
    EXPECT_FLOAT_EQ(sk.facing(), -1.f);
    sk.track_facing({2, 2});                 // x λ��Ϊ�� �� ��ԭ
    EXPECT_FLOAT_EQ(sk.facing(), 1.f);
    sk.track_facing({2, 9});                 // �������ƶ����ĳ���
    EXPECT_FLOAT_EQ(sk.facing(), 1.f);
}

// ���� A6-S1: monster_anim_input �ź�ӳ�� (������, �� gameplay) ����������
namespace {
Monster make_signal_monster() {
    return Monster(0, 0, "����", 10, 1, 0, 0, {200, 80, 80, 255});
}
}
TEST(MonsterAnimInput, NullAiFallsBackToIdle) {
    Monster m = make_signal_monster();
    delete m.ai; m.ai = nullptr;
    int last_hp = -1;
    const auto in = monster_anim_input(m, last_hp, 5.f);
    EXPECT_FALSE(in.moving);
    EXPECT_FALSE(in.attacking);
    EXPECT_FALSE(in.hit_flash);
    EXPECT_FLOAT_EQ(in.attack_recovery_ratio, 1.f);
}
TEST(MonsterAnimInput, MovingReadsAiChaseState) {
    Monster m = make_signal_monster();
    int last_hp = -1;
    m.ai->state = AIState::CHASE;
    EXPECT_TRUE(monster_anim_input(m, last_hp, 5.f).moving);
    m.ai->state = AIState::IDLE;
    EXPECT_FALSE(monster_anim_input(m, last_hp, 5.f).moving);
    m.ai->state = AIState::ATTACK;
    EXPECT_FALSE(monster_anim_input(m, last_hp, 5.f).moving);
}
TEST(MonsterAnimInput, AttackUsesWallClockSwingWindow) {
    Monster m = make_signal_monster();
    int last_hp = -1;
    EXPECT_FALSE(monster_anim_input(m, last_hp, 5.f).attacking);   // Ĭ�� -10 Զ��
    m.last_attack_wall_time = 4.9f;
    EXPECT_TRUE(monster_anim_input(m, last_hp, 5.0f).attacking);   // 0.25s �ӿ�����
    EXPECT_FALSE(monster_anim_input(m, last_hp, 5.2f).attacking);  // �������
}
TEST(MonsterAnimInput, HitIsHpFallEdgeOnly) {
    Monster m = make_signal_monster();
    int last_hp = -1;
    EXPECT_FALSE(monster_anim_input(m, last_hp, 5.f).hit_flash);
    m.combat.current_hp = 7;
    EXPECT_TRUE(monster_anim_input(m, last_hp, 5.f).hit_flash);
    EXPECT_FALSE(monster_anim_input(m, last_hp, 5.f).hit_flash);
    m.combat.current_hp = 9;                                        // ��Ѫ�����ܻ�
    EXPECT_FALSE(monster_anim_input(m, last_hp, 5.f).hit_flash);
}
TEST(MonsterActorKey, PrefersSpriteOverrideThenName) {
    Monster m = make_signal_monster();
    EXPECT_EQ(monster_actor_key(m), "����");
    m.sprite_override = "mon_fire_imp";
    EXPECT_EQ(monster_actor_key(m), "mon_fire_imp");
}
