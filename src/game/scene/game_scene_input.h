#pragma once
#include "input_map.h"

class GameScene;
struct DialogueState;
struct NPCState;
struct EventPresentation;
enum class EventPhase;

// 从 GameScene 提取的输入处理模块
class GameSceneInput {
public:
    explicit GameSceneInput(GameScene& scene) : _s(scene) {}

    // 主入口: 替代 GameScene::_input()
    void handle_input(const InputMap& input);

    // Debug 按键 (F1-F12)
    void handle_debug_keys();

    // 子处理器 (由 handle_input 内部分发)
    void handle_event_input(const InputMap& input);
    void handle_dialogue_input(const InputMap& input);

private:
    // G53: handle_dialogue_input 拆出的两段 (确认翻页收尾 / 取消离场)
    void _finish_dialogue_page(DialogueState& d);
    void _apply_npc_farewell_rewards(NPCState* npc);
    void _leave_dialogue();
    // G53: handle_debug_keys 拆出的三段 (buff / 视觉覆盖层 / 截图+结局预览)
    void _handle_debug_buff_keys();
    void _handle_debug_overlay_keys();
    void _handle_debug_ending_preview();
    // G53: handle_event_input 拆出的三段 (sim 自动推进 / DESC 阶段 / CHOICE 阶段)
    void _sim_advance_event_ui(EventPresentation& ui);
    void _handle_event_desc_phase(const InputMap& input, EventPresentation& ui);
    void _handle_event_choice_phase(const InputMap& input, EventPresentation& ui);

private:
    GameScene& _s;
};
