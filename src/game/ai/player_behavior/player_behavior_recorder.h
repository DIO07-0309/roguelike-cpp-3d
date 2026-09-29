#pragma once
#include <vector>
#include "player_action.h"
#include "player_behavior_data.h"

// ============================================================
// F15.2: PlayerBehaviorRecorder — action stream recorder
// Records every player action as a PlayerAction struct.
// Debug: press F9 to print stats to game_scene message.
// Save: logs/player_behavior.json (temporary, not hooked to SaveManager).
// ============================================================

class PlayerBehaviorRecorder {
public:
    static PlayerBehaviorRecorder& inst();

    // ── Action stream ──
    void record(const PlayerAction& action);
    const std::vector<PlayerAction>& history() const { return _history; }
    void clear();

    // M1: per-frame context snapshot — attached to every recorded action
    void set_context(float hp_pct, float enemy_dist, int skill_ready_mask);
    // M5: 条件维度上下文 — 朝向 + 近1s受击计数 (供"受压反击"等模式学习)
    void set_battle_context(int facing_dir, int hit_in_1s);

    // ── Convenience hooks ──
void on_weapon_attack(const char* wt_name, float time, int floor,
        float px, float py, int weapon_type, int combo_stage);
    void on_skill_use(const char* skill_id, float time, int floor,
                      float px, float py);
    void on_move_state_change(float time, int floor, float px, float py,
                              int direction);
    void on_dodge(float time, int floor, float px, float py);
    void on_heal(float time, int floor, int amount);
    void on_floor_enter(float time, int floor);

    // ── Summary data (aggregated from action stream) ──
    PlayerBehaviorData& data() { return _data; }
    const PlayerBehaviorData& data() const { return _data; }

    // ── Debug ──
    void print_debug(char* buf, size_t buf_size) const;

    // ── Save/Load action stream ──
    void save_to_file(const char* path) const;

private:
    PlayerBehaviorRecorder() = default;
    std::vector<PlayerAction> _history;
    PlayerBehaviorData _data;
    float _ctx_hp = -1.0f;
    float _ctx_dist = -1.0f;
    int   _ctx_mask = 0;
    int   _ctx_facing = -1;   // M5
    int   _ctx_hits = 0;      // M5
};

#define g_behavior PlayerBehaviorRecorder::inst()
