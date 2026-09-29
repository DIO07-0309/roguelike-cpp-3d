#pragma once

class GameScene;
class InputMap;
class Player;
class Monster;
class Inventory;
class PresentationSystemDirector;
struct WeaponAttackResult;  // G9: defined in weapon_types.h

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

    // G9: weapon-driven attack helpers
    void _weapon_attack(GameScene& gs, Player& p);
    void _kill_target(GameScene& gs, Monster* target);
    void _process_weapon_result(GameScene& gs, Player& p,
                                const WeaponAttackResult& r);

    // B3: 翻滚 helper — 起翻(方向解析+采集) / 尘土 (T2 空壳, T3 实装)
    void _try_start_dodge(GameScene& gs, const InputMap& input);
    void _roll_dust(GameScene& gs, float cx, float cy);
};
