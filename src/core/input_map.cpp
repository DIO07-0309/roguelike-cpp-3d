#include "input_map.h"
#include <string>
#include <cmath>

void InputMap::add_action(const std::string& action, KeyboardKey key) {
    _bindings[action].push_back(key);
}

bool InputMap::is_action_pressed(const std::string& action) const {
    auto it = _bindings.find(action);
    if (it == _bindings.end()) return false;
    for (auto key : it->second) {
        if (IsKeyDown(key)) return true;
    }
    return false;
}

bool InputMap::is_action_just_pressed(const std::string& action) const {
    auto it = _bindings.find(action);
    if (it == _bindings.end()) return false;
    for (auto key : it->second) {
        if (IsKeyPressed(key)) return true;
    }
    return false;
}

Vector2 InputMap::get_movement_axis() const {
    float x = 0, y = 0;
    if (is_action_pressed("move_up"))    y -= 1;
    if (is_action_pressed("move_down"))  y += 1;
    if (is_action_pressed("move_left"))  x -= 1;
    if (is_action_pressed("move_right")) x += 1;
    if (x != 0 && y != 0) {
        float inv = 0.70710678f;  // 1/sqrt(2)
        x *= inv; y *= inv;
    }
    return {x, y};
}

std::string InputMap::get_key_name(const std::string& action) const {
    auto it = _bindings.find(action);
    if (it == _bindings.end() || it->second.empty()) return "?";
    return _key_name(it->second[0]);
}

void InputMap::setup_defaults() {
    add_action("move_up",    KEY_W);    add_action("move_up",    KEY_UP);
    add_action("move_down",  KEY_S);    add_action("move_down",  KEY_DOWN);
    add_action("move_left",  KEY_A);    add_action("move_left",  KEY_LEFT);
    add_action("move_right", KEY_D);    add_action("move_right", KEY_RIGHT);
    add_action("attack",     KEY_SPACE);
    add_action("pickup",     KEY_E);
    add_action("inventory",  KEY_B);
    add_action("skill_1",    KEY_ONE);  add_action("skill_2", KEY_TWO);
    add_action("skill_3",    KEY_THREE); add_action("skill_4", KEY_FOUR);
    add_action("skill_1",    KEY_KP_1); add_action("skill_2", KEY_KP_2);
    add_action("skill_3",    KEY_KP_3); add_action("skill_4", KEY_KP_4);
    add_action("descend",    KEY_E);
    add_action("confirm",    KEY_ENTER); add_action("confirm", KEY_KP_ENTER);
    add_action("cancel",     KEY_ESCAPE);
    add_action("restore_backup", KEY_R);   // G20b: 槽位还原上一代备份
    add_action("fullscreen", KEY_G);
    add_action("dodge",      KEY_LEFT_SHIFT);   // B3: 翻滚 — 任一 Shift
    add_action("dodge",      KEY_RIGHT_SHIFT);
}

std::string InputMap::_key_name(KeyboardKey k) {
    if (k >= KEY_A && k <= KEY_Z) return std::string(1, 'A' + (k - KEY_A));
    if (k >= KEY_ZERO && k <= KEY_NINE) return std::to_string(k - KEY_ZERO);
    if (k == KEY_SPACE) return "空格";
    if (k == KEY_ENTER) return "Enter";
    if (k == KEY_ESCAPE) return "Esc";
    if (k == KEY_UP) return "↑";
    if (k == KEY_DOWN) return "↓";
if (k == KEY_LEFT) return "←";
    if (k == KEY_DOWN) return "↓";
    if (k == KEY_RIGHT) return "→";
    return "?";
}
