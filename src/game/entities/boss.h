#pragma once
#include <vector>
#include <utility>
#include <memory>
#include <string>
#include "ai.h"
#include "types/boss_types.h"   // M4a: BossSkillQueue / BossCommand
#include "data/boss_defs.h"     // M4a: ComboDef

// 前向声明
class Monster;
class Player;
class GameMap;
struct Effect;

// ============================================================
// B15: BossState — Boss 行为状态机
// ============================================================
enum class BossState {
    IDLE,        // 空闲/追逐 (使用基础 AI)
    ATTACK,      // 普攻
    CHARGE,      // 冲锋: 蓄力→冲刺
    SHOCKWAVE,   // 冲击波: 蓄力→AOE
    SUMMON,      // 召唤: 召唤小怪
    DEFEND,      // D8: 防御: 举盾减伤 (Golem)
    WHIRLWIND,   // G5.4: 旋风斩 (Shadow Knight Phase2)
    LASER_BARRAGE,// G5.4: 激光弹幕 (Fire Demon Phase2)
    GRAVITY_PULL,// G5.4: 引力拉扯 (Demon Lord Phase2)
    RANGED_BARRAGE,// M4a: 弹幕 (蓄力→扇形弹)
    CONE_ATTACK,   // M4a: 扇形斩 (蓄力→扇形挥击)
    BLINK,         // M4a: 瞬移 (闪烁至侧翼)
};

// ============================================================
// BossSkill — Boss 专属技能 (保留 B7 兼容)
// ============================================================
struct BossSkill {
    std::string name;
    float cooldown;
    float last_use_time = -999.0f;
    float damage_mult = 1.0f;    // 数据驱动伤害倍率 (来自 bosses.json)
    std::string fx_kind = "circle";
    float fx_radius = 60.0f;
    Color fx_color{200, 40, 40, 255};

    BossSkill(const std::string& n, float cd);

    bool can_use(double t) const;
    void mark_used(double t);

    virtual std::string execute(Monster* boss, Player* player,
                                std::vector<Monster*>& monsters,
                                GameMap* map, double game_time) = 0;
};

// B15: 冲锋技能 — 锁定玩家方向, 蓄力0.6s后高速冲刺
class ChargeSkill : public BossSkill {
public:
    ChargeSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    // 蓄力/冲锋阶段变量
    float windup_left = 0.0f;
    float windup_time = 0.6f;    // 数据驱动蓄力时长
    float dash_duration = 0.0f;
    float dash_dx = 0.0f, dash_dy = 0.0f;
};

// B15: 冲击波 — 蓄力后向四周释放 AOE
class ShockwaveSkill : public BossSkill {
public:
    ShockwaveSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    float windup_left = 0.0f;
    float windup_time = 0.7f;    // 数据驱动蓄力时长
};

// 召唤 (保留 B7, B15 增强为固定循环)
class SummonMinions : public BossSkill {
public:
    SummonMinions();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
};

// G5.4: 旋风斩 (Shadow Knight Phase2) — 360°持续旋转+移动
class WhirlwindSkill : public BossSkill {
public:
    WhirlwindSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    // M4a-fx: 命中范围圈 (蓄力白环 / 旋转紫圈)
    void draw(Monster* boss, float cam_x, float cam_y) const;
    float windup_left = 0.0f;
    float spin_duration = 0.0f;
    int   spin_hit_count = 0;
    double last_hit_time = -10.0;  // 命中冷却 (0.5s), 避免每帧判伤
};

// G5.4: 激光弹幕 (Fire Demon Phase2) — 扇形3方向远程贯穿弹
class LaserBarrageSkill : public BossSkill {
public:
    LaserBarrageSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    float windup_left = 0.0f;
};

// M4a: 弹幕 — 蓄力后向玩家扇形发射多颗弹, 命中减速
// M4b: 图案化 — pattern 0=扇形 1=环形 2=螺旋多波, waves/wave_interval 波次发射
class BarrageSkill : public BossSkill {
public:
    BarrageSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    float windup_left = 0.0f;
    float windup_time = 0.5f;
    int   shot_count = 4;
    float spread_deg = 40.0f;   // 扇形总角度 (度)
    float speed = 220.0f;
    // M4b: 弹幕图案
    int   pattern = 0;          // 0=fan 扇形, 1=ring 环形 360°, 2=spiral 螺旋多波
    int   waves = 1;            // 总波数
    float wave_interval = 0.25f;// 波间隔 (s)
    float spiral_turn_deg = 25.0f; // 螺旋每波偏转角
    struct Shot { float x = 0, y = 0, vx = 0, vy = 0, life = 0; };
    std::vector<Shot> shots;
    // M4a-fx: 本帧命中位置 (场景读取后生成爆炸特效)
    std::vector<std::pair<float, float>> hit_fx;
    bool fired = false;
    bool finished = false;
    void reset_waves() { _wave_fired = 0; _wave_timer = 0.0f; _last_tick_time = 0.0; }
    void draw(float cam_x, float cam_y) const;
private:
    void _fire_wave(Monster* boss, Player* player);
    int   _wave_fired = 0;      // 已发射波数
    float _wave_timer = 0.0f;   // 下一波倒计时
    double _last_tick_time = 0.0; // 上一 tick 时间 (compute dt)
};

// M4a: 扇形斩 — 蓄力后向玩家方向扇形挥击, 命中中毒 2s
class ConeAttackSkill : public BossSkill {
public:
    ConeAttackSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    void draw(Monster* boss, Player* player, float cam_x, float cam_y) const;
    float windup_left = 0.0f;
    float windup_time = 0.4f;
    float half_angle = 45.0f;  // 半角 (度)
    float reach = 96.0f;       // 半径 (像素)
};

// M4a: 瞬移 — 蓄力闪烁后瞬移至玩家侧翼, CD 独立计时
class BlinkSkill : public BossSkill {
public:
    BlinkSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    void draw(float cam_x, float cam_y) const;
    void plan_destination(Monster* boss, Player* player, GameMap* map);
    float windup_left = 0.0f;
    float windup_time = 0.35f;
    bool blinked = false;
    float blink_dist = 150.0f;
    float pending_x = 0, pending_y = 0;  // 蓄力期预判落点 (draw 显示)
};

// ============================================================
// BossAI — 继承 MonsterAI，状态机驱动技能循环 + Phase2
// ============================================================
class BossAI : public MonsterAI {
public:
    BossAI();
    void update(Monster* self, Player* player, GameMap* map,
                double dt, double game_time,
                std::vector<Monster*>* all_monsters = nullptr,
                std::vector<Effect>* effects = nullptr,
                int monster_room = -1, int player_room = -1,
                const RoomManager* room_mgr = nullptr) override;

    // B15: 技能循环状态
    BossState boss_state = BossState::IDLE;
    int skill_cycle_index = 0;     // 0→Charge, 1→norm, 2→Shockwave, 3→norm, 4→Summon, repeat
    int normal_attack_count = 0;   // 当前已普攻次数

    bool is_enraged = false;
    bool phase2 = false;
    float phase2_pause = 0.0f;     // 进入二阶段暂停计时

    // G5.5: Phase2 阶段变化增强 — 狂暴脉冲 + 背水一战
    float _phase2_elapsed = 0.0f;  // 二阶段累计时间 (s)
    float _rage_pulse_cd = 0.0f;   // 狂暴脉冲冷却 (s)
    bool  _last_stand = false;     // 背水一战已触发 (HP<25%, 一次性)

    // D8 Step2: Boss-specific config
    int   skill_cycle_bias = 6;    // Necromancer: 改成更频繁的召唤循环
    float golem_shield_pct = 0.0f; // Golem: DEFEND 减伤比例

    // G1 Step6: Phase2 参数 (来自 BossDef, 替代硬编码)
    float _phase2_hp_threshold = 0.50f;
    float _phase2_pause = 0.50f;
    float _phase2_speed_mult = 1.50f;
    float _phase2_atk_mult = 1.25f;
    float _phase2_cd_mult = 0.70f;

    // G2.3: Arena 配置指针 (来自 BossDef, 在 boss_factory_create 中设置)
    const struct BossArenaDef* _arena_cfg = nullptr;

    // 三个技能实例 (B15 新)
    std::unique_ptr<ChargeSkill>    _charge;
    std::unique_ptr<ShockwaveSkill> _shockwave;
    std::unique_ptr<SummonMinions>  _summon;
    // G5.4: Phase2 signature skill instances
    std::unique_ptr<WhirlwindSkill>   _whirlwind;
    WhirlwindSkill* whirlwind_skill() { return _whirlwind.get(); }
    std::unique_ptr<LaserBarrageSkill> _laser;

    // M4a: 连招系统
    std::unique_ptr<BarrageSkill> _barrage;
    std::unique_ptr<ConeAttackSkill> _cone;
    std::unique_ptr<BlinkSkill> _blink;
    BarrageSkill* barrage_skill() { return _barrage.get(); }
    ConeAttackSkill* cone_skill() { return _cone.get(); }
    BlinkSkill* blink_skill() { return _blink.get(); }
    BossSkillQueue _combo_queue;
    float _combo_timer = 0.0f;
    float _combo_end_delay = 0.0f;
    float _combo_current_end_delay = 0.8f;
    std::string _combo_id;
    const std::vector<ComboDef>* _combos = nullptr;   // 来自 BossDef (factory 设置)
    // M4b: 遭遇阶段 (director 每帧注入) — 驱动连招模板切换
    EncounterPhase _encounter_phase = EncounterPhase::OPENING;
    void set_encounter_phase(EncounterPhase p) { _encounter_phase = p; }

    const char* _boss_id = nullptr;   // G5.4: 当前 Boss ID 用于 phase2 行为分支
    float _gravity_timer = 0.0f;      // GRAVITY_PULL 拉拽计时 (成员, 原 static 跨实例共享)

    // F15 Mirror Boss: 镜像玩家武器/技能/装备
    bool _is_mirror = false;
    int  _mirror_weapon_type = 0;        // WeaponType int
    float _mirror_weapon_range = 1.5f;   // 基础攻击距离 (tiles)
    float _mirror_attack_mult = 1.2f;    // ATK = player_atk × 1.2
    int  _mirror_combo_stage = 0;        // 0-2, 类似玩家连招
    int  _mirror_max_stages = 1;         // 武器最大连招段数
    float _mirror_stage_mults[3] = {1.0f, 1.0f, 1.0f};  // 每段伤害倍率
    float _mirror_active_range = 0.0f;   // 主攻击范围 (pixels)
    float _mirror_width = 0.0f;          // 攻击宽度 (pixels) — 扇形角度/矩形宽
    int  _mirror_hit_shape = 0;          // HitShape int (0=circle, 1=sector, 2=rect, 3=capsule)

    // F15 Mirror Boss: 镜像玩家技能
    struct MirrorSkill {
        std::string name;           // 技能名 "Fire Slash"
        float cooldown = 2.0f;
        float damage_mult = 1.0f;
        float range = 80.0f;        // pixels
        int   skill_type = 0;       // 0=melee, 1=projectile, 2=self_buff, 3=aoe
        float last_used = -99.0f;
    };
    std::vector<MirrorSkill> _mirror_skills;
    int _mirror_skill_idx = 0;    // 当前镜像技能索引
    int _mirror_action_intent = -1;  // MirrorAgent 预测的下一动作 (-1=none, 0=attack, 1=skill, 2=heal)

private:
    void _enter_phase2(Monster* self, std::vector<Effect>* effects);
    // G5.5: Phase2 持续行为 (狂暴脉冲/背水一战) — 阶段变化增强
    void _tick_phase2_behaviors(Monster* self, Player* player, GameMap* map,
                                double dt, std::vector<Effect>* effects);
    void _enter_last_stand(Monster* self, std::vector<Effect>* effects);
    void _tick_boss_state(Monster* self, Player* player, GameMap* map,
                          double dt, double gt,
                          std::vector<Monster*>* all, std::vector<Effect>* effects);
    float _hp_ratio(Monster* self) const;
    int   _next_cycle_skill();  // 返回 -1=普攻, 0=Charge, 1=Shockwave, 2=Summon

    // M4a: 连招驱动
    bool _tick_combo_attack(Monster* self, Player* player, GameMap* map,
                            double dt, double gt, std::vector<Effect>* effects);
    void _run_combo_command(BossCommand cmd, Monster* self, Player* player,
                            GameMap* map, double gt, std::vector<Effect>* effects);
    void _select_combo();
    void _combo_advance();
    void _combo_on_skill_end();
    void _combo_fallback_melee(Monster* self, Player* player, double gt,
                               std::vector<Effect>* effects);  // 收官: 技能冷却中退普攻
    static BossCommand _command_from_str(const std::string& s);
};

// 工厂
Monster* spawn_boss(int tile_x, int tile_y, int floor);  // deprecated: use boss_factory_create

// ============================================================
// D8 Step2: BossType — Boss 类型枚举
// ============================================================
enum class BossType {
    SHADOW_KNIGHT,  // 暗影骑士 (F5)
    FIRE_DEMON,     // 地狱火魔 (F10)
    DEMON_LORD,     // 深渊之主 (F15)
    NECROMANCER,    // 亡灵法师 (F5)
    GOLEM,          // 远古魔像 (F10)
    VAMPIRE,        // G1 Step6: 血族伯爵 (F5)
};

// ── 前向声明 (boss_defs.h 在 boss.cpp 中引用) ──
struct BossDef;

// BossFactory: 数据驱动创建
Monster* boss_factory_create(BossType type, int tile_x, int tile_y, int floor,
                              std::vector<Monster*>* out_monsters = nullptr,
                              GameMap* map = nullptr);
BossType boss_type_for_floor(int floor, uint32_t seed);

// G1 Step6: visual_id → Color 映射 (表现层, 未来替换为 texture)
Color get_boss_visual_color(const std::string& visual_id);
const char* get_boss_skills_text(const BossDef* def);
