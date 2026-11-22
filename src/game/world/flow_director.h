#pragma once

// ============================================================
// D4.6 Step3: FlowDirector — 探索节奏控制器
// 保证每30秒至少一种反馈 (战斗/奖励/剧情/事件)
// ============================================================

// FlowState 已删除: 全仓零引用, FlowDirector 只用 FlowTimer 字段与字面量

struct FlowTimer {
    float combat = 0.0f;    // 距上次战斗 (秒)
    float reward = 0.0f;    // 距上次奖励 (拾取/宝箱/Relic)
    float story  = 0.0f;    // 距上次剧情 (旁白/NPC)
    float event  = 0.0f;    // 距上次事件 (Event/特殊房间)
    int   rooms_explored = 0;
    int   explorer_score = 0;
};

class FlowDirector {
public:
    void tick(float dt);

    // 标记 (各系统在触发时调用)
    void mark_combat()  { _timer.combat = 0; }
    void mark_reward()  { _timer.reward = 0; }
    void mark_story()   { _timer.story  = 0; }
    void mark_event()   { _timer.event  = 0; }
    void mark_explore() { _timer.rooms_explored++; _timer.explorer_score += 5; }

    // 超过阈值时返回动作提示 (供 GameScene 使用)
    // nullptr = 无需自动触发
    const char* auto_spawn_suggestion();

    // 查询
    const FlowTimer& timer() const { return _timer; }
    float worst_gap() const;  // 最大空白值 (最长的未触发时间)

private:
    FlowTimer _timer;
    static constexpr float COMBAT_MAX = 35.0f;
    static constexpr float REWARD_MAX = 40.0f;
    static constexpr float STORY_MAX  = 40.0f;
    static constexpr float EVENT_MAX  = 30.0f;
};
