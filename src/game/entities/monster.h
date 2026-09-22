#pragma once
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <random>
#include "raylib.h"
#include "entity.h"
#include "combat_stats.h"
#include "combat_system.h"
#include "types/weapon_types.h"               // Projectile (弹体池元素)
#include "object_pool.h"                      // Game::ObjectPool
#include "game/animation/avatar_animator.h"   // A6-S1: AnimInput 信号映射

// 前向声明
class Player;
class GameMap;
class MonsterAI;
class SkeletonAvatar;   // A6-S1: 通用骨骼形象 (渲染层专属)

// ============================================================
// D2 Step4: TeamRole — 怪物在队伍中的职责 (自动按 MonsterType 分配)
// ============================================================
enum class TeamRole { FRONTLINE, BACKLINE, SUPPORT, FLANK, COMMAND, NONE };

// ============================================================
// B14: MonsterType — 怪物生态定位
// ============================================================
enum class MonsterType {
    NORMAL,   // 普通怪 (slime/orc)
    ARCHER,   // 哥布林弓箭手: 远程, 玩家靠近后退
    SHAMAN,   // 萨满: 辅助附近怪物
    BOMBER,   // 爆炸怪: 靠近玩家自爆
    TANK,     // 重甲怪: 高HP低速
    ELITE,    // 精英怪: 强化属性+随机Buff
    CHARGER,  // D8: 冲锋怪: 中距蓄力冲刺撞击
    SUMMONER, // D8: 召唤师: 定期召唤小怪
};

// ============================================================
// Monster — 怪物实体
// ============================================================
class Monster {
public:
    Entity entity;
    CombatStats combat;
    // Q3.9: 稳定实例ID (构造时递增分配, 创建顺序确定性 → 同种子同ID)
    // 替代裸指针作为统计键 — 跨进程堆地址不同 → 指针键统计必然非确定性
    uint64_t instance_id = 0;
    static uint64_t _next_instance_id;
    std::vector<BuffInstance> active_buffs;   // 当前施加的 buff
    std::string name;
    Color color{200, 80, 80, 255};
    bool is_boss = false;
    MonsterType monster_type = MonsterType::NORMAL;  // B14
    // M4f.5: 素材精灵 key 覆盖 (Boss 按层指定, 空 = 按类型自动映射)
    std::string sprite_override = "";
    bool is_elite = false;                            // B14: Elite 标记
    TeamRole team_role = TeamRole::NONE;              // D2 Step4: 协同职责
    std::vector<BuffTrigger> on_hit_triggers;  // 命中玩家时触发的 Buff 规则
    AttackType attack_type = AttackType::PHYSICAL;
    float attack_cooldown = 1.5f;
    float last_attack_time = 0.0f;
    float last_attack_wall_time = -10.0f;   // M4f.12: 墙钟攻击时刻 (持械挥砍动画用)

    // B14: Bomber / Shaman 专用计时器
    float explode_timer = 0.0f;      // 自爆倒计时 (Bomber)
    float support_cooldown = 0.0f;   // 辅助冷却 (Shaman)

    // D2: Projectile attack config (data-driven from enemies.json)
    // F10.1: Domain boss invulnerability (set by BossSystemDirector)
    bool domain_invulnerable = false;

    // F10.2: Weak point (WeaponExecutor can target this)
    bool is_weak_point = false;
    int  weak_point_type = 0;            // WeakPointType as int (0=CORE)
    int  weak_point_state = 0;           // WeakPointState as int (0=SPAWNED)
    void* _weak_point_owner = nullptr;   // Boss* that spawned this core

    bool kill_processed = false;   // P0-A: 防止 on_monster_killed 重复触发 (defensive guard)
    bool uses_projectile = false;
    float projectile_speed = 300.0f;
    float projectile_warning_time = 0.8f;
    int   projectile_warning_level = 0;
    Game::ObjectPool<Projectile>* projectiles_ptr = nullptr;

    // AI 组件 (在 ai.h 中定义)
    class MonsterAI* ai = nullptr;

    Monster(float x, float y, const std::string& n, int hp, int atk,
            int pdef, int mdef, Color c, class MonsterAI* a = nullptr);
    ~Monster();

    void update_ai(Player* player, GameMap* map, double dt, double game_time,
                   std::vector<Monster*>* all_monsters = nullptr,
                   std::vector<struct Effect>* effects = nullptr,
                   int monster_room = -1, int player_room = -1,
                   const class RoomManager* room_mgr = nullptr);
    // G5.5: damage_mult 支持普攻模式倍率 (CLEAVE 1.5x); 默认 1.0 = 原行为
    int attack_target(Player* target, double game_time, float damage_mult = 1.0f);
    bool can_attack(double game_time) const;

    void draw(float cam_x, float cam_y);

    // A6-S1: 骨骼皮肤 (渲染层专属; 仅渲染路径由 GameScene 懒建/驱动, sim/无头永不触达)
    SkeletonAvatar* skeleton_avatar() { return _skeleton_avatar.get(); }
    void set_skeleton_avatar(std::unique_ptr<SkeletonAvatar> avatar);
    std::unique_ptr<SkeletonAvatar> _skeleton_avatar;
};

// 前置声明特效结构
struct Effect;

// A6-S1: 皮肤白名单查找 key — sprite_override 优先, 空则 name (数据驱动, 无 gameplay)
std::string monster_actor_key(const Monster& m);
// A6-S1: 动画信号映射 (可测纯函数; ai 不可见状态 → 全 false 回退 idle)
AnimInput monster_anim_input(const Monster& m, int& last_hp, float now_wall);

// 工厂
Monster* spawn_monster(float px, float py, const std::string& type);
