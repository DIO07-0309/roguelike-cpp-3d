#pragma once
#include "sim_redesign_flags.h"
#include "sim_tuning.h"
// ============================================================
// P6: CombatFocusTracker — 清怪僵持检测 (纯值, 无 game 依赖)
//
// Root cause (docs/SIM_STUCK_DIAGNOSIS.md): sim AI 杀掉近处怪后不追远处
// 满血怪 — 在 loot(0.7)/room(0.5) 与 approach(0.6) 间摇摆, 且"玩家移动"
// 被 _stuck_progress 当作战斗进展, 看门狗拖到 120s 才结算 (84.7% 卡死)。
//
// 职责 (单一): 根据"无怪掉血时长"判断该不该进入纯追怪模式 (hunting)。
// 不碰寻路 / 卡死判定 / 平衡参数 — 那些由 DecisionAgent 负责。
//
// 契约:
//   · 怪 HP 总和下降 或 存活数下降 → 有战斗进展, 退出 hunting。
//   · 无进展持续 >= kHuntThresholdSec 且仍有存活怪 → 进入 hunting。
//   · 无存活怪 → 不 hunting (交还清层/楼梯逻辑)。
//   · game_time 回退 (换层归零) → reset(), 否则跨层永久失效。
// ============================================================

class CombatFocusTracker {
public:
    static constexpr float kHuntThresholdSec = 8.0f;  // 原契约默认值
    float hunt_threshold() const {
        return (sim_redesign_active() && sim_redesign_flags().hunt_lock)
            ? sim_tuning::hunt_threshold() : kHuntThresholdSec;
    }

    // 每帧观测战斗状态。hp_sum/alive_count 口径必须与 _stuck_progress 一致:
    // 只统计 is_alive 怪的 current_hp (alive==0 时 hp_sum 视为 0)。
    void observe(float hp_sum, int alive_count, float game_time) {
        if (game_time < _last_time) reset();          // 换层归零 → 重置
        if (alive_count <= 0) {                        // 无怪 → 不追
            _hunting = false;
            _no_combat_since = -1.0f;
        } else if (_last_alive < 0 || alive_count < _last_alive ||
                   (hp_sum + 0.01f < _last_hp_sum)) {  // 怪掉血/死亡 → 进展
            _no_combat_since = game_time;
            _hunting = false;
        } else if (_no_combat_since < 0.0f) {          // 锚定起点
            _no_combat_since = game_time;
        } else if (game_time - _no_combat_since >= hunt_threshold()) {
            _hunting = true;                           // 僵持超时 → 纯追怪
        }
        _last_hp_sum = hp_sum;
        _last_alive = alive_count;
        _last_time = game_time;
        // P3-redesign change4: 清怪锁存 — 僵持触发后保持到清层/换层, 不因零星掉血解除.
        if (sim_redesign_active() && sim_redesign_flags().hunt_lock) {
            if (_hunting) _floor_locked = true;
            if (alive_count <= 0) _floor_locked = false;  // 清层解锁; 换层由 reset() 解锁
        }
    }

    bool hunting() const { return _hunting; }
    bool floor_hunting() const { return _floor_locked; }  // P3-redesign: 锁存态

    void reset() {
        _last_hp_sum = -1.0f;
        _last_time = -1.0f;
        _last_alive = -1;
        _no_combat_since = -1.0f;
        _hunting = false;
        _floor_locked = false;
    }

private:
    float _last_hp_sum = -1.0f;
    float _last_time = -1.0f;
    int   _last_alive = -1;
    float _no_combat_since = -1.0f;
    bool  _hunting = false;
    bool  _floor_locked = false;  // P3-redesign
};
