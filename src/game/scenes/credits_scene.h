#pragma once
#include "node.h"
#include "input_map.h"
#include <string>
#include <vector>

// 前向声明 (避免依赖 GameScene)
struct TimelineEvent;
struct NPCEnding;
struct BossBattleReport;
struct RunSummary;

// ============================================================
// D6 Step2: CreditsScene — 片尾演出
// 纯数据驱动, 不依赖 GameScene/GameMap
// ============================================================
class CreditsScene : public Node {
public:
    // 数据入 (组合模式: 所有数据在切换场景前填充完毕)
    std::string ending_name;
    std::string ending_title;
    std::string sky_color;
    std::string final_line;
    int  meta_soul = 0, meta_knowledge = 0;

    // NPC 后日谈
    std::string npc_names[8];
    std::string npc_results[8];
    std::string npc_details[8];
    int  npc_count = 0;

    // Boss Timeline
    float timeline_times[16];
    std::string timeline_labels[16];
    int  timeline_count = 0;

    // Battle Report
    std::string boss_rank;
    int  boss_dmg_done = 0, boss_dmg_taken = 0;
    float boss_time = 0.0f;
    int  boss_arena_zones = 0;

    // Run Summary
    int  floor_reached = 0, bosses_killed = 0, total_kills = 0;
    int  elite_kills = 0, relics_collected = 0, quests_done = 0;
    int  combo_max = 0;
    float play_time = 0.0f;

    // Meta
    int  total_runs = 0, curr_soul = 0, curr_know = 0;

    // 生命周期
    void _ready() override;
    void _render() override;
    void _input(const InputMap& input) override;

private:
    int  _page = 0;           // 当前页码
    int  _npc_page = 0;       // NPC 子页
    int  _timeline_page = 0;  // Timeline 子页
    float _scroll_y = 0.0f;   // 滚动字幕位置

    enum class CreditsPhase { NPC_EPILOGUE, TIMELINE, REPORT, SUMMARY, CREDITS };
    CreditsPhase _phase() const;
    int  _total_pages() const;  // 字面数量(包括NPC分页)

    // G13: 每个 CreditsPhase 一页, 拆出后 _render() 只做分发
    void _render_npc_epilogue(int sw, int sh);
    void _render_timeline(int sw, int sh);
    void _render_report(int sw, int sh);
    void _render_summary(int sw, int sh);
    void _render_credits(int sw, int sh);
    void _draw_credits_npc_lines(float cx, float& sy, Color white);
    void _draw_credits_colophon(float cx, float& sy, Color gold, Color dim);
};
