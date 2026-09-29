#pragma once
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include "raylib.h"
#include "entity.h"
#include "combat_system.h"
#include "build_tag.h"

// 前向声明
class Player;
class Monster;
class GameMap;
struct SkillDef;  // G3.2: SkillDef 来自 src/data/

// ============================================================
// D3 Step2: SkillEvolution — 技能进化数据
// 每级不提升数值，而是改变技能行为
// ============================================================
struct SkillEvolution {
    int level = 1;                         // 1-3 级
    std::string name;                       // 进化名称 e.g. "广域斩"
    std::string desc;                       // 效果描述
    std::vector<BuildTag> required_tags;    // 需要的构筑标签 (用于 Relic 联动)
};

// ============================================================
// Skill — 技能系统 (16主动 + 6被动 + 3级升级 + 冷却; 9个C++类: 4基础+5签名主动 + 2被动)
// ============================================================

class Skill {
public:
    std::string name;
    float base_cooldown;
    float cooldown;
    int level = 1;
    int max_level = 5;
    float last_use_time = -999.0f;

    float charm_cd_bonus = 0.0f;
    float charm_power_bonus = 0.0f;

    std::string _skill_id;     // G3.2: "slash"|"fireball"|... (替代 dynamic_cast 类型判断)

    Skill(const std::string& n, float cd, int ml = 5);
    virtual ~Skill() = default;

    // D3 Step1: 构筑标签查询 (替代 dynamic_cast / 硬编码类型判断)
    bool has_tag(BuildTag t) const     { return ::has_tag(tags, t); }
    bool has_any_tag(std::initializer_list<BuildTag> list) const { return ::has_any_tag(tags, list); }
    bool has_all_tags(std::initializer_list<BuildTag> list) const { return ::has_all_tags(tags, list); }

    std::vector<BuildTag> tags;   // D3 Step1: 构筑标签
    int use_count = 0;            // G1: 技能使用次数 — SkillEvolutionManager 驱动

    // D3 Step2: 技能进化 (Evolution 系统)
    std::vector<SkillEvolution> evolutions;    // 进化路径 (在构造器中填充)
    int evolution_level = 0;                    // 当前进化等级 0-3 (0=未进化)
    void add_evolution(int lv, const char* name, const char* desc,
                       std::initializer_list<BuildTag> req);
    bool can_evolve() const;
    std::string evolve();                        // 执行进化, 返回名称
    const SkillEvolution* current_evolution() const; // 当前进化等级的数据 (nullptr=无)
    std::string get_evolution_text() const;      // "广域斩 Lv2" 风格显示

    bool can_use(double game_time) const;
    float remaining_cooldown(double game_time) const;
    void mark_used(double game_time);
    void reset_cooldown() { last_use_time = -999.0f; }  // D8: time_fragment
    bool can_upgrade() const;
    bool upgrade();

    float get_power_multiplier() const;
    virtual std::string get_level_text() const;

    void apply_charm(float cd_bonus, float power_bonus);
    void remove_charm();

protected:
    virtual void _on_level_up();
    void _recalc_cooldown();
};

// 主动技能
class ActiveSkill : public Skill {
public:
    std::vector<struct BuffTrigger> triggers;   // 命中后触发的 Buff 规则

    ActiveSkill(const std::string& n, float cd, int ml = 5)
        : Skill(n, cd, ml) {}

    // D2 Step2: is_heavy — 重击强化 (仅 combo.is_heavy() 时为 true)
    virtual std::string execute(Player* caster, std::vector<Monster*>& targets,
                                GameMap* map, bool is_heavy = false) = 0;
};

// 被动技能
class PassiveSkill : public Skill {
public:
    std::string modifier_key;
    int base_value;

    PassiveSkill(const std::string& n, const std::string& key, int val, int ml = 3)
        : Skill(n, 0.0f, ml), modifier_key(key), base_value(val) {}

    virtual int get_value() const;
    void apply(Player* player);
    void remove(Player* player);
};

// ---- 具体主动技能 ----
class SlashSkill : public ActiveSkill {
public:
    SlashSkill();                                      // 旧构造器 (向后兼容)
    explicit SlashSkill(const SkillDef& def);          // G3.2: 数据驱动构造器
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map, bool is_heavy = false) override;
    std::string get_level_text() const override;
protected:
    void _on_level_up() override;
    int _cone_range = 1;
};

class FireballSkill : public ActiveSkill {
public:
    FireballSkill();
    explicit FireballSkill(const SkillDef& def);       // G3.2
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map, bool is_heavy = false) override;
    std::string get_level_text() const override;
protected:
    void _on_level_up() override;
    int _target_count = 1;
    float _range = 6.0f;
};

class SelfHealSkill : public ActiveSkill {
public:
    SelfHealSkill();
    explicit SelfHealSkill(const SkillDef& def);       // G3.2
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map, bool is_heavy = false) override;
    std::string get_level_text() const override;
    void tick_regen(Player* caster, float delta);
protected:
    void _on_level_up() override;
    float _regen_left = 0.0f;
};

class TheWorldSkill : public ActiveSkill {
public:
    TheWorldSkill();
    explicit TheWorldSkill(const SkillDef& def);       // G3.2
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map, bool is_heavy = false) override;
    std::string get_level_text() const override;
    float get_stop_duration() const;
protected:
    void _on_level_up() override;
    float _stop_duration[4] = {0.0f, 5.0f, 7.0f, 10.0f};
};

// ---- 具体被动技能 ----
class IronSkinSkill : public PassiveSkill {
public:
    IronSkinSkill() : PassiveSkill("铁壁", "def_flat", 3, 3) { _skill_id = "iron_skin"; tags = {BuildTag::DEFENSE}; }
    explicit IronSkinSkill(const SkillDef& def);       // G3.2
    int get_value() const override;
    std::string get_level_text() const override;
};

class BerserkSkill : public PassiveSkill {
public:
    BerserkSkill() : PassiveSkill("狂暴", "atk_flat", 3, 3) { _skill_id = "berserk"; tags = {BuildTag::MELEE}; }
    explicit BerserkSkill(const SkillDef& def);       // G3.2
    int get_value() const override;
    std::string get_level_text() const override;
};

// ── G5.2: Signature Skills (新行为类) ──

// 冰爆: AOE冻结+对已冻结目标追加碎冰伤害
class IceNovaSkill : public ActiveSkill {
public:
    IceNovaSkill(); explicit IceNovaSkill(const SkillDef& def);
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map, bool is_heavy = false) override;
    std::string get_level_text() const override;
protected: void _on_level_up() override;
    float _radius = 4.0f; int _shatter_bonus = 30;  // 碎冰伤害%
};

// 连锁闪电: 主目标AOE→弹射链N次，递减伤害
class ChainLightningSkill : public ActiveSkill {
public:
    ChainLightningSkill(); explicit ChainLightningSkill(const SkillDef& def);
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map, bool is_heavy = false) override;
    std::string get_level_text() const override;
protected: void _on_level_up() override;
    int _bounces = 3; float _decay = 0.30f;  // 每次弹射衰减30%
};

// 暗影突袭: 瞬移至目标身后→背刺(方向判定+倍率)
class ShadowStrikeSkill : public ActiveSkill {
public:
    ShadowStrikeSkill(); explicit ShadowStrikeSkill(const SkillDef& def);
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map, bool is_heavy = false) override;
    std::string get_level_text() const override;
protected: void _on_level_up() override;
    float _backstab_mult = 2.5f;
};

// 血怒: 消耗自身HP→范围流血→根据命中数回血
class BloodFrenzySkill : public ActiveSkill {
public:
    BloodFrenzySkill(); explicit BloodFrenzySkill(const SkillDef& def);
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map, bool is_heavy = false) override;
    std::string get_level_text() const override;
protected: void _on_level_up() override;
    float _hp_cost_pct = 0.10f; float _heal_per_hit = 0.05f;
};

// 召唤英灵: spawn_monster→加入targets列表→AI驱动
class SummonSpiritSkill : public ActiveSkill {
public:
    SummonSpiritSkill(); explicit SummonSpiritSkill(const SkillDef& def);
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map, bool is_heavy = false) override;
    std::string get_level_text() const override;
protected: void _on_level_up() override;
    int _spirit_count = 1; int _spirit_hp = 30; int _spirit_atk = 8;
};

// ---- 技能管理器 ----
class SkillManager {
public:
    std::vector<std::unique_ptr<ActiveSkill>> active_skills;
    std::vector<std::unique_ptr<PassiveSkill>> passives;
    int max_active = 4;

    bool can_learn() const;
    bool learn(std::unique_ptr<Skill> skill);
    void apply_all_passives(Player* player);
};

// 工厂
std::unique_ptr<ActiveSkill> random_active_skill(const std::vector<std::string>& exclude = {},
                                                  bool base_only = false);
std::unique_ptr<Skill> random_skill(const std::vector<std::string>& exclude = {});
std::vector<std::string> get_learned_names(const SkillManager& mgr);

// G3.2: SkillFactory — 数据驱动创建
std::unique_ptr<Skill> skill_factory_create(const std::string& skill_id);
