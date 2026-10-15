#pragma once
#include "player_action.h"
#include "player_habit_profile.h"
#include <vector>

// ============================================================
// F15.3: PlayerBehaviorAnalyzer — read action stream → profile
// Pure data transformation. No state in the analyzer.
// Call after action stream is populated (e.g. entering F15).
// ============================================================

class PlayerBehaviorAnalyzer {
public:
    // Analyze the full action timeline → compact habit profile
    static PlayerHabitProfile analyze(const std::vector<PlayerAction>& history);

    // Generate counter-strategy hints based on profile
    static void generate_counter_hints(PlayerHabitProfile& profile);

private:
    static int count_type(const std::vector<PlayerAction>& h, PlayerActionType t);
    // M5: 条件维度统计 — 受压反击率/朝向稳定度/攻击节奏方差
    static void compute_condition_dimensions(
        const std::vector<PlayerAction>& history, PlayerHabitProfile& p);
    // G53: analyze 拆出的三段 (频率/移动/距离与伤害)
    static void _compute_frequencies(const std::vector<PlayerAction>& h,
                                     PlayerHabitProfile& p, float total_t);
    static void _compute_movement(const std::vector<PlayerAction>& h,
                                   PlayerHabitProfile& p);
    static void _compute_distance_damage(const std::vector<PlayerAction>& h,
                                         PlayerHabitProfile& p);
    // G53: 风格分类 (aggressive/defensive/sniper/mage 四档, 阈值原样)
    static void _classify_style(PlayerHabitProfile& p);
};
