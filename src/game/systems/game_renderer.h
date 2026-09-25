#pragma once
#include <vector>
#include <memory>
#include <string>
#include "raylib.h"
#include "entity.h"

// 前向声明
class Player;
class Monster;
class GameMap;
struct DroppedItem;
struct Effect;
class InputMap;

// ============================================================
// F15.5.1: Mirror HUD — 角色面板数据结构
// ============================================================
struct SkillDisplay {
    std::string name;
    float cooldown_ratio = 0.0f;  // 0.0=ready, 1.0=full CD
    bool  ready = true;
};

struct BuffDisplay {
    std::string icon;           // 单字图标 "攻""毒""缓"...
    std::string label;          // 显示名
    float remaining_ratio = 1.0f;
};

struct CharacterPanelData {
    const char* name = nullptr;       // "RUOZHI" / "ENDING ECHO"
    const char* sub_label = nullptr;  // Phase text or style label
    int hp = 0, max_hp = 1;
    int atk = 0, pdef = 0, mdef = 0;
    int level = 0, xp = 0, xp_to_next = 0;
    std::vector<SkillDisplay> skills;
    std::vector<BuffDisplay> buffs;
    bool mirror_mode = false;         // true = dark-red inverted theme
    int  mirror_phase = 0;            // 1=Observe, 2=Mirror, 3=Evolve
    // M4e: 在线学习 HUD (当前桶 4 臂胜率, -1 = 未决策)
    int mirror_last_action = -1;
    float mirror_arm_rates[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    // v1.6-B1: 镜像记忆可视化 — "它眼中的你" (Boss 层常驻)
    float mirror_accuracy = -1.0f;   // 预测准确率 [0,1], -1=数据不足
    int   mirror_observed = 0;        // 已观察动作数
    float mirror_drift = 0.0f;        // 本局风格漂移 [0,1]
    char  mirror_style[24] = "";      // 画像风格名 (激进/防御/狙击/法师/均衡)
    char  mirror_habits[3][48] = {"", "", ""};  // Top3 习惯中文短句
};

// ============================================================
// GameRenderer — 纯渲染类 (组合模式)
// 接收 GameScene 的状态数据，执行所有绘制
// 不持有可变状态，不修改游戏数据
// ============================================================
class GameRenderer {
public:
    // ---- 静态绘制工具 (无状态) ----
    static void draw_panel(Rectangle r, const char* title, Color bg = {20, 20, 40, 230});
    static void draw_glow_text(const char* text, float x, float y, int size, Color c,
                               bool center = true);
    static void draw_progress_bar(Rectangle r, float ratio, Color fill,
                                  Color bg = {30, 30, 60, 255});

    // ---- 摄像机 ----
    void update_camera(float& cam_x, float& cam_y, const Player* player,
                       const GameMap* map, int screen_w, int screen_h);

    // ---- 特效与覆盖层 ----
    void draw_effects(const std::vector<Effect>& effects, float cam_x, float cam_y);
    void draw_time_stop_overlay(int sw, int sh, float time_stop_remaining);
    // M4.2: 镜像专属冻结 overlay — 红色压制感, 与玩家时停(蓝/白)区分
    void draw_mirror_freeze_overlay(int sw, int sh, float freeze_remaining);
    void draw_boss_cinematic_overlay(int sw, int sh);
    void draw_boss_intro(int sw, int sh, const std::string& title, const std::string& lore,
                         const std::string& skills_text, Color color, int boss_floor,
                         const std::string& visual_id = "");
    void draw_room_message(int sw, int sh, const std::string& msg, float timer);

    // ---- F15.5.1: 角色面板 (可复用的玩家/Echo面板) ----
    static void draw_character_panel(const CharacterPanelData& d, float x, float y);

    // ---- v1.6-B1: 镜像阶段晋升横幅 (观察→镜像→进化) ----
    static void draw_phase_banner(int sw, int sh, int phase, float timer);

    // ---- HUD 渲染 ----
    void draw_hud(const Player* player, int current_floor, float game_time,
                  Monster* boss, bool show_relic_panel,
                  int inventory_open, int inventory_cursor,
                  const std::string& room_msg, float room_msg_timer,
                  int screen_w, int screen_h,
                  const CharacterPanelData* echo_panel = nullptr,
                  int challenge_wave = -1, int challenge_total = 0);
    void draw_skill_bar(const Player* player, float game_time);
    void draw_player_buffs(const Player* player);
    void draw_player_relics(const Player* player);
    void draw_monster_buffs(const Monster& m, float draw_x, float draw_y);
    void draw_inventory_panel(const Player* player, int cursor, int sw, int sh);
    void draw_relic_panel(const Player* player, int sw);
    // Batch 3H: Gamble Room UI panel
    void draw_gamble_panel(const Player* player, const std::string& result_msg,
                           float result_timer, int sw, int sh);
    void draw_challenge_portal(float cam_x, float cam_y, int portal_tx, int portal_ty,
                               float pulse_timer, bool is_entry);
    void draw_teleport_fade(int sw, int sh, float fade_timer, bool fading_in);
    void draw_challenge_choice(int sw, int sh, int cursor, const char* pity_hint);

private:
    static Color _relic_rarity_color(const std::string& rarity);
    static std::string _rarity_label_cn(const std::string& rarity);
    static void _draw_panel_skills(const std::vector<SkillDisplay>& skills,
                                   float x, float y, bool mirror);
    static void _draw_panel_buffs(const std::vector<BuffDisplay>& buffs,
                                  float x, float y, bool mirror);
    // M4e: 镜像在线学习 HUD
    static void _draw_mirror_learning(const CharacterPanelData& d,
                                      float px, float py, float panel_h);
};
