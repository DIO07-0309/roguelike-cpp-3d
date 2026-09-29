#pragma once
#include <string>

class Player;
struct GameEvent;

// ============================================================
// G1 Step3: SkillEvolutionManager — 技能进化管理
// 事件驱动: 监听 mark_used 后的阈值检测, 解锁进化等级
// 不轮询、不依赖 Renderer、不修改技能行为
// ============================================================

class SkillEvolutionManager {
public:
    static void register_listener();
    static void unregister_listener();

    // 检查所有技能的进化条件 (事件驱动, 每次技能使用后调用)
    static void check_unlock(Player* player);

    // 查询
    static std::string evo_name(const Player* player, int skill_index);

    // SKILL_EVOLVED payload unpacking
    static int  event_new_level(int packed)  { return packed & 0xFFFF; }
    static int  event_skill_index(int packed) { return (packed >> 16) & 0xFF; }

private:
    // 技能使用后的事件回调
    // 单技能检查
    static bool _try_evolve_skill(Player* player, int idx);
};
