// G13: TeamCoordinator::evaluate() 拆成角色分支后的行为回归
// 目的: 拆分只搬移代码, 不改变任何决策结果
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include "systems/team_coordinator.h"
#include "monster.h"
#include "player.h"

namespace {

// 按"中心点"摆放 —— 怪物与玩家高度不同, 直接放 (x,y) 会让 Y 轴对不齐
std::unique_ptr<Monster> make_monster(float cx, float cy, TeamRole role) {
    auto m = std::make_unique<Monster>(0.0f, 0.0f, "test", 100, 10, 5, 5,
                                       Color{200, 80, 80, 255});
    m->entity.sync_rect();
    m->entity.position = {cx - m->entity.size.x / 2,
                          cy - m->entity.size.y / 2};
    m->entity.sync_rect();
    m->team_role = role;
    return m;
}

Player make_player(float cx, float cy) {
    Player p(0.0f, 0.0f, 200.0f, 120, 12, 4, 2);
    p.entity.sync_rect();
    p.entity.position = {cx - p.entity.size.x / 2,
                         cy - p.entity.size.y / 2};
    p.entity.sync_rect();
    return p;
}

}  // namespace

// FRONTLINE 无石柱 → 走"保护后排"分支: 朝 (玩家+后排)/2 归一化
TEST(TeamCoordinator, FrontlineMovesToProtectBackline) {
    Player player = make_player(300.0f, 300.0f);
    auto self = make_monster(200.0f, 300.0f, TeamRole::FRONTLINE);
    auto back = make_monster(380.0f, 300.0f, TeamRole::BACKLINE);

    std::vector<Monster*> all = {self.get(), back.get()};
    const TeamDecision dec = TeamCoordinator::evaluate(self.get(), player, all, nullptr);

    EXPECT_FLOAT_EQ(dec.move_x, 1.0f);
    EXPECT_FLOAT_EQ(dec.move_y, 0.0f);
    EXPECT_FALSE(dec.should_support);
    EXPECT_FALSE(dec.should_command);
}

// SUPPORT: 挑血量最低的盟友标记治疗, 并朝远离玩家方向退
TEST(TeamCoordinator, SupportTargetsWoundedAllyAndRetreats) {
    Player player = make_player(300.0f, 300.0f);
    auto self = make_monster(200.0f, 300.0f, TeamRole::SUPPORT);
    auto ally = make_monster(380.0f, 300.0f, TeamRole::BACKLINE);
    ally->combat.current_hp = 50;      // 50% → 治疗评分 50, 低于 0.85 阈值

    std::vector<Monster*> all = {self.get(), ally.get()};
    const TeamDecision dec = TeamCoordinator::evaluate(self.get(), player, all, nullptr);

    EXPECT_TRUE(dec.should_support);
    EXPECT_EQ(dec.support_target, ally.get());
    EXPECT_EQ(dec.support_type, 1);
    EXPECT_FLOAT_EQ(dec.move_x, -1.0f);
    EXPECT_FLOAT_EQ(dec.move_y, 0.0f);
}

// BACKLINE: Tank 已接敌时向后拉开距离
TEST(TeamCoordinator, BacklineRetreatsWhenTankIsEngaged) {
    Player player = make_player(300.0f, 300.0f);
    auto self = make_monster(200.0f, 300.0f, TeamRole::BACKLINE);
    auto tank = make_monster(340.0f, 300.0f, TeamRole::FRONTLINE);

    std::vector<Monster*> all = {self.get(), tank.get()};
    const TeamDecision dec = TeamCoordinator::evaluate(self.get(), player, all, nullptr);

    EXPECT_FLOAT_EQ(dec.move_x, -1.0f);
    EXPECT_FLOAT_EQ(dec.move_y, 0.0f);
}

// 无盟友 → 直接返回空决策 (不走任何分支)
TEST(TeamCoordinator, LoneMonsterGetsNoAdvice) {
    Player player = make_player(300.0f, 300.0f);
    auto self = make_monster(200.0f, 300.0f, TeamRole::FRONTLINE);

    std::vector<Monster*> all = {self.get()};
    const TeamDecision dec = TeamCoordinator::evaluate(self.get(), player, all, nullptr);

    EXPECT_FLOAT_EQ(dec.move_x, 0.0f);
    EXPECT_FLOAT_EQ(dec.move_y, 0.0f);
    EXPECT_FALSE(dec.should_support);
    EXPECT_FALSE(dec.should_command);
}

// COMMAND: 指挥光环
TEST(TeamCoordinator, CommandIssuesAuroraAdvice) {
    Player player = make_player(300.0f, 300.0f);
    auto self = make_monster(200.0f, 300.0f, TeamRole::COMMAND);
    auto ally = make_monster(380.0f, 300.0f, TeamRole::BACKLINE);

    std::vector<Monster*> all = {self.get(), ally.get()};
    const TeamDecision dec = TeamCoordinator::evaluate(self.get(), player, all, nullptr);

    EXPECT_TRUE(dec.should_command);
    EXPECT_FLOAT_EQ(dec.move_x, 0.0f);
}
