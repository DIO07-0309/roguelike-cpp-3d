#pragma once
#include <string>
#include <vector>
#include "raylib.h"

struct InputMap;
class GameScene;
enum class EventType;
struct DungeonEvent;

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
    // G53: tick_event_ui 拆出的两段 (事件副作用 / 结果文本归类)
    void _apply_event_reward_flags(const DungeonEvent& ev, int selected);
    void _set_event_complete_text(std::string& complete_text,
                                  const std::string& reward_text);
    // G53: draw_quest_log 拆出的两段 (任务列表 / NPC 关系栏)
    void _draw_quest_list(const Rectangle& pr);
    void _draw_npc_relations(const Rectangle& pr);
    GameScene& _s;
};
