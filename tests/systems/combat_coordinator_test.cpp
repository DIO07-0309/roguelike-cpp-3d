// CombatCoordinator 时停伤害队列回归 — 验证 instance_id 路径 (P1-C4 UAF 修复).
// 时停中技能命中后: 怪物 HP 回滚到原值, pending_damage 记 (instance_id, delta) 而非裸指针.
#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

#include "systems/combat_coordinator.h"
#include "entities/skill.h"
#include "entities/player.h"
#include "entities/monster.h"

namespace {

std::unique_ptr<Monster> make_target(float cx, float cy, int hp) {
    auto m = std::make_unique<Monster>(0, 0, "时停靶", hp, 5, 0, 0,
                                       Color{200, 80, 80, 255});
    m->entity.sync_rect();
    m->entity.position = {cx - m->entity.size.x / 2, cy - m->entity.size.y / 2};
    m->entity.sync_rect();
    m->combat.current_hp = hp;
    m->combat.is_alive = true;
    return m;
}

}  // namespace

TEST(CombatCoordinator, TimestopRollsBackHpAndQueuesInstanceId) {
    auto player = Player(0, 0, 200, 200, 30, 5, 3);  // 高攻确保能打出伤害
    player.direction = Direction::DOWN;
    auto monster = make_target(0, 40, 500);           // 正前方 (DOWN 锥体内)
    std::vector<Monster*> mlist = {monster.get()};
    auto skill = SlashSkill();
    int hp_before = monster->combat.current_hp;
    uint64_t iid_before = monster->instance_id;
    std::vector<std::pair<uint64_t, int>> pending;

    CombatCoordinator::cc_defer_in_timestop(
        &player, &skill, mlist, nullptr, false, 1.0, pending);

    // HP 回滚: 技能造成伤害后被恢复
    EXPECT_EQ(monster->combat.current_hp, hp_before);
    EXPECT_TRUE(monster->combat.is_alive);
    // 队列用 instance_id, 且 delta 为正
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0].first, iid_before);
    EXPECT_GT(pending[0].second, 0);
}

TEST(CombatCoordinator, TimestopNoHitQueuesNothing) {
    auto player = Player(0, 0, 200, 200, 10, 5, 3);
    player.direction = Direction::DOWN;
    auto monster = make_target(0, -400, 500);  // 正后方, 锥体内打不到
    std::vector<Monster*> mlist = {monster.get()};
    auto skill = SlashSkill();
    int hp_before = monster->combat.current_hp;
    std::vector<std::pair<uint64_t, int>> pending;

    CombatCoordinator::cc_defer_in_timestop(
        &player, &skill, mlist, nullptr, false, 1.0, pending);

    EXPECT_EQ(monster->combat.current_hp, hp_before);
    EXPECT_TRUE(pending.empty());
}
