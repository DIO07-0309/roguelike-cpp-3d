#pragma once
// G8.1: BTAgent — Behavior Tree wrapper with same interface as DecisionAgent.
// Reads game state → writes to Blackboard → ticks BT → reads action from Blackboard.

#include "behavior_tree/behavior_tree.h"
#include <string>
#include <vector>
#include <memory>

class Player;
class Monster;
class GameMap;

class BTAgent {
public:
    BTAgent();
    ~BTAgent();

    // G8.1: Build the behavior tree (called once after construction)
    void build_tree();

    // Same interface as DecisionAgent::best_action()
    std::string best_action(const Player* player,
        const std::vector<Monster*>& monsters,
        const GameMap* map, bool stairs_active, bool boss_intro_active);

    // Event decision
    bool accept_event(float risk_pct, const std::string& effect_desc,
                      const Player* player) const;

    // Compat with GameScene._is_action_just_pressed
    bool is_action_just_pressed(const char* action_name,
        const Player* player,
        const std::vector<Monster*>& monsters,
        const GameMap* map,
        bool stairs_active, bool boss_intro_active);

    // G7.4 compat: start() for new run
    void start(const Player* player) { if (player) _resolve_profile(player); }

    // Tick frame counter (for rate-limiting)
    void tick() { _frame++; }

    // Q3.15: current game time — required for skill cooldown checks (P0-2 fix:
    // can_use(0) was always true at start and permanently false after first use)
    void set_time(double t) { _game_time = t; }

    // G8.1: access blackboard for testing
    const bt::Blackboard& board() const { return _board; }

private:
    bt::Blackboard _board;
    std::unique_ptr<bt::Node> _root;
    int _frame = 0;
    double _game_time = 0.0;

    void _sync_state(const Player* player,
        const std::vector<Monster*>& monsters,
        const GameMap* map, bool stairs_active, bool boss_intro_active);
    // G53: _sync_state 拆出的三段 (敌人态势 / Boss 贴近 / 特殊房间贴近)
    void _sync_enemy_situation(const Player* player,
        const std::vector<Monster*>& monsters);
    void _sync_boss_proximity(const Player* player,
        const std::vector<Monster*>& monsters);
    void _sync_room_proximity(const Player* player, const GameMap* map);
    void _resolve_profile(const Player* player);
    Monster* _find_nearest(const Player* player,
        const std::vector<Monster*>& monsters) const;
    float _hp_ratio(const Player* p) const;
    int _count_enemies_in_range(const Player* player,
        const std::vector<Monster*>& monsters, float range_px) const;
};
