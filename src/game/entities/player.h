#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "raylib.h"
#include "entity.h"
#include "combat_stats.h"
#include "inventory.h"
#include "skill.h"
#include "input_map.h"
#include "attack_evolution_state.h"   // G1: 普攻进化
#include "systems/weapon_component.h"  // G9: weapon system
#include "components/element_component.h" // G10: element core
#include "systems/dodge_component.h"      // B3: 翻滚组件 (纯手感输入驱动)

// ============================================================
// D2: ComboState — 四段连击状态 (挂在 Player 上)
// 后续 D3~D5 的 Skill/Boss/Relic 全部可以查询此状态
// ============================================================
struct ComboState {
    int count = 0;           // 当前连击段数: 0=站立, 1-3=普通连击, 4=重击
    float timer = 0.0f;      // 连击窗口剩余时间
    float last_hit_time = 0;

    static constexpr float WINDOW = 0.85f;
    static constexpr float COOLDOWN = 0.35f;

    float multiplier() const;   // 当前段伤害倍率: 1.0/1.15/1.4/2.2
    bool  is_heavy() const;     // 是否为重击段 (count==4)
    void  hit(double game_time); // 命中: 推进连击段数 + 重置窗口
    void  tick(float dt);        // 逐帧衰减窗口
};

// ============================================================
// Player — 玩家实体
// ============================================================
class Player {
public:
    Entity entity;
    float speed = 200.0f;
    Direction direction = Direction::DOWN;
    bool is_moving = false;
    CombatStats combat;
    std::vector<BuffInstance> active_buffs;   // 当前施加的 buff
    std::vector<RelicInstance> relics;        // B11: 局内圣物 (跨楼层持续)
    Inventory inventory;
    SkillManager skills;
    AttackType attack_type = AttackType::PHYSICAL;

    int level = 1;
    int xp = 0;
    int xp_to_next = 80;

    int gold = 0;        // Batch 3A: 局内金币
    int key_count = 0;   // Batch 3A: 钥匙数量

    // Runtime context: current dungeon floor.
    // Used by floor-scaled interactions (e.g. Gamble Room cost).
    int current_floor = 1;

    // P1-C7-A: legacy 攻击冷却已删 — 空手统一走 WeaponComponent 轨 (weapons.json fist recovery=0.5)
    float _last_attack_time = -999.0f;   // 镜像 Boss 观察用 (executor 路径写入)
    float _last_skill_time = -999.0f;   // M4e: 最近技能施放时刻 (镜像观察)
    float _swing_start = -1.0f;         // G9.4: 武器挥砍动画起始时刻

    // 构造
    Player(float x, float y, float spd, int hp, int atk, int pdef, int mdef);

    void reset_attack_timers();
    int attack_target(Player* target, double game_time);

    // 移动
    Vector2 handle_input(const InputMap& input);

    // 等级
    static int calc_xp_for_level(int lvl);

    // Batch 3A: 经济 API
    void add_gold(int amount);
    bool spend_gold(int amount);
    int  get_gold() const;
    void add_key(int amount);
    bool spend_key(int amount);
    int  get_key_count() const;

    // Batch 3H: 统一圣物入口 (自动应用/移除 PASSIVE 加成)
    void add_relic(const std::string& id, PersistenceScope scope);
    void remove_relic(const std::string& id);

    // 渲染
    void render(Camera2D& cam);
    void draw_no_cam(float cam_x, float cam_y,
                     const struct GameMap* view_map = nullptr);  // G10.6-B C1: 朝墙姿态衰减用

    // D2: 连击状态
    ComboState combo;

    // G9: 武器组件 (equip时同步, 驱动普攻)
    WeaponComponent weapon;

    // B3: 翻滚组件 (独立冷却 0.7s, Shift 起翻, 不占武器 recovery)
    DodgeComponent dodge;

    // G1: 普攻进化状态
    AttackEvolutionState attack_evo;

    // G9.3: last weapon attack context (read by skills for synergy)
    AttackContext last_attack;

    // G10.1: element core (permanent, cross-save)
    ElementComponent element;

    // D2 Step2: 消耗重击标记 (技能/Relic/Boss 统一调用此接口)
    bool consume_heavy_combo();
};
