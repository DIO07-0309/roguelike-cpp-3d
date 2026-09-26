#pragma once
#include "node.h"
#include "input_map.h"
#include "slot_select_scene.h"   // G11.1: 选档跳转 (Mode)
#include "raylib.h"
#include <vector>
#include <string>

struct MenuItem { std::string key, label, action; Color color; };

class TitleScene : public Node {
public:
    bool has_save = false;
    int max_floor = 1;
    float anim_time = 0;
    int hover_index = -1;  // Q4.5: 鼠标悬停菜单项
    std::vector<MenuItem> items;

    void _enter_tree() override;
    void _ready() override;
    void _process(double delta) override;
    void _render() override;
    void _input(const InputMap& input) override;
    const char* get_bgm_name() const override { return "title"; }

private:
    bool _activate(const std::string& action);  // Q4.5: 键盘/鼠标共用动作分发
    bool _open_slot_select(SlotSelectScene::Mode mode, const char* log_msg);  // G11.1: 三个选档入口共用
    void _draw_stage();   // G10.7-B2: 电影海报舞台层 (渐变/透视地板/拱门/vignette)
    void _draw_characters();   // G10.7-B3: 左右对峙角色层 (近大远小)
    struct StageTex { Texture2D wall{}, floor{}, door{}; bool loaded = false; }
        _stage_tex;
    struct CharTex {
        Texture2D p_fire{}, p_ice{}, p_poison{}, blacksmith{};
        Texture2D boss_f10{}, boss_f5{}, shaman{}, skeleton{}, orc{}, slime{};
        Texture2D tank{}, summoner{}, bomber{}, charger{};
        Texture2D w_sword{}, w_spear{}, w_crossbow{}, w_dagger{};
        Texture2D armor{}, potion_red{}, potion_blue{}, charm{};
        bool loaded = false;
    } _char_tex;
};
