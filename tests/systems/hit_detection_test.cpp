// hit_detection 真实函数测试 — 替换 weapon_test.cpp 里只做手工数学的假测试.
// 直接调用 hit_detect_* 生产函数, 验证 circle/sector/rectangle/capsule 几何.
#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "systems/hit_detection.h"
#include "monster.h"

namespace {

// 把怪物中心放到 (cx,cy) — 生产函数用 entity.rect 中心判距离.
std::unique_ptr<Monster> monster_at(float cx, float cy) {
    auto m = std::make_unique<Monster>(0.0f, 0.0f, "靶子", 100, 5, 0, 0,
                                       Color{200, 80, 80, 255});
    m->entity.sync_rect();
    m->entity.position = {cx - m->entity.size.x / 2, cy - m->entity.size.y / 2};
    m->entity.sync_rect();
    m->combat.is_alive = true;
    return m;
}

}  // namespace

TEST(HitDetectionReal, CircleHitsInRangeMissesOutside) {
    auto near = monster_at(140, 100);
    auto far = monster_at(300, 100);
    std::vector<Monster*> targets = {near.get(), far.get()};
    auto hits = hit_detect_circle({100, 100}, 64.0f, targets);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].target, near.get());
}

TEST(HitDetectionReal, SectorRespectsDirection) {
    auto front = monster_at(160, 100);  // RIGHT 正前方
    auto behind = monster_at(40, 100);  // 背后
    std::vector<Monster*> targets = {front.get(), behind.get()};
    auto hits = hit_detect_sector({100, 100}, Direction::RIGHT, 100.0f, 45.0f, targets);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].target, front.get());
}

TEST(HitDetectionReal, RectangleForwardOnly) {
    auto front = monster_at(160, 100);
    auto behind = monster_at(40, 100);
    std::vector<Monster*> targets = {front.get(), behind.get()};
    auto hits = hit_detect_rectangle({100, 100}, Direction::RIGHT, 100.0f, 40.0f, targets);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].target, front.get());
}

TEST(HitDetectionReal, CapsuleSegmentRadius) {
    auto on_segment = monster_at(150, 105);  // 线段附近
    auto off_far = monster_at(150, 200);     // 远离线段
    std::vector<Monster*> targets = {on_segment.get(), off_far.get()};
    auto hits = hit_detect_capsule({100, 100}, Direction::RIGHT, 100.0f, 20.0f, targets);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].target, on_segment.get());
}

TEST(HitDetectionReal, EmptyTargetsReturnsEmpty) {
    std::vector<Monster*> none;
    EXPECT_TRUE(hit_detect_circle({0, 0}, 100.0f, none).empty());
    EXPECT_TRUE(hit_detect_sector({0, 0}, Direction::DOWN, 100.0f, 45.0f, none).empty());
}
