#pragma once
#include <string>
#include <vector>
#include "raylib.h"

struct InputMap;
class GameScene;
enum class EventType;
struct DungeonEvent;
struct EventPresentation;

// 从 GameScene 提取的事件 + NPC/对话模块
class GameSceneInteraction {
public:
    explicit GameSceneInteraction(GameScene& scene) : _s(scene) {}

    // Event
    void start_event_presentation(EventType et);
    void tick_event_ui(float dt);
    void draw_event_ui(int sw, int sh);

    // NPC / Dialogue
    void spawn_floor_npcs(int floor, const std::vector<std::pair<int,int>>& rooms);
    void start_dialogue(int npc_index);
    void update_dialogue(float dt);
    void draw_dialogue(int sw, int sh);
    void draw_quest_log(int sw, int sh);

private:
    // G53: tick_event_ui 拆出的相位推进 (ENTER->DESC / ANIM->REWARD)
    void _event_ui_to_desc(EventPresentation& ui);
    void _event_ui_to_reward(EventPresentation& ui);
    // G53: tick_event_ui 拆出的两段 (事件副作用 / 结果文本归类)
    void _apply_event_reward_flags(const DungeonEvent& ev, int selected);
    void _set_event_complete_text(std::string& complete_text,
                                  const std::string& reward_text);
    // G53: draw_quest_log 拆出的两段 (任务列表 / NPC 关系栏)
    void _draw_quest_list(const Rectangle& pr);
    void _draw_npc_relations(const Rectangle& pr);
    // G53: draw_event_ui 拆出的四段面板 (标题 / 描述+选项 / 动画 / 结果)
    void _draw_event_enter_panel(int sw, int sh, const char* ename,
                                 const Color& gold, const Color& white,
                                 const Color& bg);
    void _draw_event_choice_panel(int sw, int sh, float alpha, const char* ename,
                                  const Color& gold, const Color& white);
    void _draw_event_anim_panel(int sw, int sh, float alpha, const Color& bg);
    void _draw_event_result_panel(int sw, int sh, float alpha, bool final);
    GameScene& _s;
};
