#pragma once
#include "core/node.h"
#include "raylib.h"               // Rectangle
#include "save/save_manager.h"
#include <memory>
#include <vector>

class GameScene;

// ══ G10.9-C1: 统一三档选择场景 ══
// 复用于: 新游戏(空档直进/满档管理) · 继续(已有档) · 选关(已有档, 带该档maxf)
// 底层只调 Slot API: get_all_slots / set_active_slot / slot_exists / get_slot_summary
class SlotSelectScene : public Node {
public:
    enum class Mode { NEW_GAME, CONTINUE_GAME, SELECT_FLOOR };

    Mode mode = Mode::CONTINUE_GAME;

    void _ready() override;
    void _process(double delta) override;
    void _render() override;
    void _input(const class InputMap& input) override;

private:
    // 布局: 三张竖排大卡 (960x640 逻辑)
    static constexpr float CARD_W = 340.0f;
    static constexpr float CARD_H = 118.0f;
    static constexpr float CARD_GAP = 26.0f;
    static constexpr float CARD_X = 175.0f;   // 左列, 避右侧怪物
    static constexpr float CARD_Y0 = 205.0f;  // 标题带之下

    void _refresh_slots();
    Rectangle _card_rect(int i) const;
    bool _slot_clickable(int i) const;
    void _activate_slot(int i);
    void _confirm_delete(int i);
    void _enter_game(int i);
    void _draw_delete_confirm();

    // G13: 拆 _render / _input / _enter_game / _draw_delete_confirm, 满足函数 ≤40 行红线
    void _render_header(int sw, bool any_exists);        // 标题带 + 操作提示
    void _render_cards();                                 // 三张卡
    void _draw_slot_card(int i);                          // 单卡 (底/框/三行字/删除角标)
    void _draw_slot_summary(const SlotSummary& s, const Rectangle& r);  // 卡内摘要
    void _input_delete_confirm(const class InputMap& input);   // 二次确认框独占输入
    void _handle_mouse_select();                          // 鼠标悬停选中 + 左键确认
    void _new_game_in_slot(int i);                       // NEW_GAME 分派
    void _continue_game_in_slot(int i);                  // CONTINUE 分派
    void _confirm_delete_by_slot(int slot_id);           // 按 slot_id 删档 (解耦 _delete_target)

    std::vector<SlotSummary> _slots;
    int _cursor = 0;
    int _hover = -1;
    float _anim = 0.0f;

    // 满档删除流 (NEW_GAME 且三档全满)
    bool _delete_confirm_open = false;
    int _delete_target = -1;
};
