#pragma once
#include "attack_evolution_state.h"

class Player;
struct GameEvent;

// ============================================================
// G1 Step1: AttackEvolutionManager — 普攻进化管理
// 事件驱动: 监听 RELIC_GAIN, 条件满足时升级 attack_evo.level
// 不轮询、不依赖 Renderer、不修改攻击逻辑
// ============================================================

class AttackEvolutionManager {
public:
    // 注册到 EventBus (在 enter_floor 调用)。
    // EventBus 是进程级全局单例、回调幂等，故没有对应的注销接口。
    static void register_listener();

    // 条件检查 (由 EventBus 回调触发)
    static void check_unlock(Player* player);

    // 查询
    static const char* current_name(const Player* player);
    static int  current_level(const Player* player);

    // ATTACK_EVOLVED 事件 payload 解包 (val 中打包了 new_level|old_level<<16)
    static int event_new_level(int packed)  { return packed & 0xFFFF; }
    static int event_old_level(int packed)  { return (packed >> 16) & 0xFFFF; }

private:
    // RELIC_GAIN 事件回调
    static void _on_relic_gained(const GameEvent& ev);
};
