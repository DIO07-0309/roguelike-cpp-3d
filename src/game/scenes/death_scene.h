#pragma once
#include "node.h"
#include "input_map.h"
#include <string>

class DeathScene : public Node {
public:
    int final_floor = 1, final_level = 1;
    std::string ending_name;
    std::string final_line;
    int meta_soul = 0, meta_knowledge = 0;
    // v1.6-B1: 镜像复盘 (F15 死亡时才有; 空字符串 = 不显示)
    std::string mirror_verdict;      // 一句话: 它靠什么赢了你
    std::string mirror_habits;       // 多行: 被针对的 Top 习惯
    // v1.6-B2: 死因仪表盘 (本局死因 + 跨局死因谱)
    std::string death_cause;          // 本局死因 (last_damage_source)
    void _render() override;
    void _input(const InputMap& input) override;

private:
    // G13: 拆 _render, 满足函数 ≤40 行红线
    void _render_background(int sw);          // 血色渐晕底
    void _render_header(int sw);              // 大字 + 结局 + 死因 + 结算
    void _render_mirror_review(int sw, int sh);  // 镜像复盘 (F15)
    void _render_foot(int sw, int sh);        // 存档提示 + 死因谱 + 返回提示
};
