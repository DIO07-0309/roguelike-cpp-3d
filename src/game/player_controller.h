#pragma once

#include "raylib.h"       // Vector2 (G31-A: _resolve_move_input 按值返回)

class GameScene;
class InputMap;
class Player;
class Monster;
class Inventory;
class PresentationSystemDirector;
struct WeaponAttackResult;  // G9: defined in weapon_types.h
struct WeaponDef;           // G31-D: defined in weapon_types.h
struct AttackGeometry;      // G31-D: defined in weapon_types.h
struct WeaponVFXContext;    // G31-D: defined in player_controller.cpp

// ============================================================
// D6 Step7: PlayerController — 玩家输入/攻击/技能/移动/交互
// 组合模式: 持有 GameScene 引用, 所有 player 行为集中于此
// GameScene 不再直接处理 WASD/攻击/技能/交互
// ============================================================
class Monster;

class PlayerController {
public:
    void bind(GameScene* gs) { _scene = gs; }

    // ── 每帧 tick (移动 + 房间发现 + 怪物AI触发) ──
    void tick(float dt);

    // G27: 层/局状态复位 — 清掉 tick() 内的跨层残留 (原为函数级 static)
    void reset_floor_state();

    // ── 输入处理 (攻击/技能/拾取/交互/背包/下楼) ──
    void handle_input(const InputMap& input);

    // ── 单一动作 ──
    void player_attack();
    void use_skill(int index);

    // Batch 3C: inventory sell helper (testable without Raylib input)
    static int sell_selected_item(Inventory& inv, Player* player,
                                  int& cursor,
                                  PresentationSystemDirector& pres);

private:
    GameScene* _scene = nullptr;

    // M5: 受击窗口追踪 — 上帧 HP 与窗口倒计时 (供受压反击学习)
    int   _last_seen_hp = -1;
    float _hit_window = 0.0f;   // >0 表示近 1s 内被打过

    // G27: 移动去抖/采样相位 — 原为 tick() 内函数级 static, 跨层跨局累积不重置
    int   _last_move_dir = -1;
    float _last_mx = 0.0f, _last_my = 0.0f;
    int   _loot_dbg = 0;
    // G27: sim 整格步进节流基准 — 与 game_time 比较, 每次进层须随其归零
    float _sim_last_step = -1.0f;

    // G9: weapon-driven attack helpers
    void _weapon_attack(GameScene& gs, Player& p);
    void _kill_target(GameScene& gs, Monster* target);
    void _process_weapon_result(GameScene& gs, Player& p,
                                const WeaponAttackResult& r);

    // B3: 翻滚 helper — 起翻(方向解析+采集) / 尘土 (T2 空壳, T3 实装)
    void _try_start_dodge(GameScene& gs, const InputMap& input);
    void _roll_dust(GameScene& gs, float cx, float cy);

    // G31-A: tick 子步骤 (纯结构搬运, 执行顺序零改动)
    void _init_behavior_context(float dt);
    void _tick_mirror_frozen(float dt);
    Vector2 _resolve_move_input();
    void _record_move_state(GameScene& gs, float mx, float my);
    void _apply_move_delta(GameScene& gs, bool is_x, float delta);
    void _move_normal(GameScene& gs, const Vector2& move, float dt);
    void _discover_rooms(GameScene& gs);
    void _update_monster_ai(float dt);
    void _self_heal_regen(float dt);

    // G31-B: handle_input 互斥 UI 面板 (纯结构搬运, 早退语义由调用方 return 保持)
    void _handle_inventory_ui(const InputMap& input);
    void _handle_gamble_ui(const InputMap& input);
    void _gamble_spin();
    void _gamble_relic_reward();
    void _handle_challenge_choice_ui(const InputMap& input);

    // G31-C: handle_input 动作链 (纯结构搬运, else-if 互斥语义零改动)
    // 4 个 bool helper 返回 true = 已消费该次交互 (门/NPC/事件/传送门), 上游据此短路
    void _face_nearest_target();
    void _handle_attack();
    void _handle_pickup();
    bool _try_pickup_door();
    bool _try_pickup_npc();
    bool _try_pickup_event();
    bool _try_pickup_portal();
    bool _try_arena_return_portal(int ptx, int pty);
    void _handle_interact_result();
    void _handle_inventory_open();

    // G31-D: _weapon_attack 子步骤 (纯结构搬运, ctx 参数束零逻辑改动)
    void _compute_stage_geometry(Player& p, const WeaponDef* def, AttackGeometry& geo);
    void _vfx_fist(const WeaponVFXContext& ctx);
    void _vfx_dagger(const WeaponVFXContext& ctx);
    void _vfx_sword(const WeaponVFXContext& ctx);
    void _vfx_nunchaku(const WeaponVFXContext& ctx);
    void _vfx_spear(const WeaponVFXContext& ctx);
    void _vfx_crossbow(const WeaponVFXContext& ctx);
    void _apply_post_hit(const WeaponVFXContext& ctx);
    // 聚合命中结果 → 顿帧秒数 + 屏震幅度 (输出参数, 与 ElementResolver 同款约定)
    float _hit_feel_seconds(const WeaponVFXContext& ctx, float& shake) const;
};
