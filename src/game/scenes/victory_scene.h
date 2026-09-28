#pragma once
#include "node.h"
#include "input_map.h"
#include <memory>
#include <string>
#include <vector>

class CreditsScene;   // G13: 前向声明, _build_credits_scene() 返回它的智能指针

// ============================================================
// D6 Step2: VictoryScene — 通关画面 (点击后转Credits)
// ============================================================
class VictoryScene : public Node {
public:
    int final_level = 1;
    std::string ending_name;
    std::string final_line;
    std::string sky_color;
    int meta_soul = 0, meta_knowledge = 0;

    // D6 Step2: 传给CreditsScene的数据 (纯数据组合, 无GameScene依赖)
    std::string npc_names[8], npc_results[8], npc_details[8];
    int  npc_count = 0;
    float timeline_times[16];
    std::string timeline_labels[16];
    int  timeline_count = 0;
    std::string boss_rank;
    int  boss_dmg_done = 0, boss_dmg_taken = 0;
    float boss_time = 0.0f;
    int  boss_arena = 0;
    int  run_floor = 0, run_bosses = 0, run_kills = 0, run_elites = 0;
    int  run_relics = 0, run_quests = 0, run_combo = 0;
    float run_playtime = 0.0f;

    void _render() override;
    void _input(const InputMap& input) override;
    // Q3.16: 通关动画专属欢快 BGM (change_scene 管线自动切换, 替代沿用的 Boss 曲)
    const char* get_bgm_name() const override { return "victory"; }

private:
    // G13: 拆 _render / _input, 满足函数 ≤40 行红线
    Color _ending_tint() const;                            // 结局名 → 氛围色
    void _render_background(int sw, int sh, Color ec);     // 渐变底 + 顶部放射线
    void _render_ending_panel(int sw, int sh, Color ec);   // 文字面板
    std::shared_ptr<CreditsScene> _build_credits_scene() const;  // 汇总数据转片尾
    void _copy_ending_to(std::shared_ptr<CreditsScene> cs) const;
    void _copy_run_summary_to(std::shared_ptr<CreditsScene> cs) const;
};
