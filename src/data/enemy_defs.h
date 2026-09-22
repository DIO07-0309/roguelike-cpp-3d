#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "types/combat_types.h"
#include "core/registry_provider.h"  // G4.1: MergeMode

// ============================================================
// G1 Step5: EnemyDef — 敌人配置数据 (纯数据, 无 Gameplay/Render 依赖)
// 属于 Data 层，位于 src/data/
// 未来: boss_defs / skill_defs / attack_defs 统一放此目录
// ============================================================

// ── AI 参数 (嵌套对象, 未来可扩展 retreat_hp / kite_distance 等) ──
struct EnemyAIDef {
    float sight = 6.0f;
    float speed = 80.0f;
    float patrol = 2.0f;
    float attack_range = 1.5f;
};

// ── 技能配置 (对象数组, 非字符串数组; 未来轻松加 level/cooldown 字段) ──
struct EnemySkillDef {
    std::string id;               // "rapid_shot" | "totem" | "leap" | ...
    float max_cooldown = 5.0f;    // 基础冷却
    float initial_cooldown = 0.0f; // 初始冷却 (0=立即可用, >0=等待)
};

// ── 精英随机 Buff 条目 ──
struct EliteBuffEntry {
    std::string buff_id;          // 空字符串 = 不施放
    int stacks = 1;
};

// ── D2: 弹道攻击配置 (远程怪物普攻) ──
struct EnemyProjectileDef {
    bool  enabled = false;        // 是否使用弹道攻击
    float speed = 300.0f;         // 弹速 px/s
    float warning_time = 0.8f;    // 预警时长 s
    int   warning_level = 0;      // 0=NORMAL 1=DANGEROUS 2=DEADLY
};

// ── 敌人模板 (一条 JSON 记录) ──
struct EnemyDef {
    std::string id;               // "slime" | "orc" | "archer" | ...
    std::string name;             // 显示名 "史莱姆"
    std::string visual_id;        // 表现层标识 (非 Color, 未来映射到 texture/animation)

    // 基础数值
    int hp = 15, atk = 3, pdef = 0, mdef = 1;

    // 枚举字符串 (由 monster.cpp 转换为 C++ enum)
    std::string type_str;         // "normal" | "archer" | "shaman" | ...
    std::string role_str;         // "none" | "frontline" | "backline" | ...
    std::string attack_type_str;  // "physical" | "magical"
    std::string ai_archetype;     // G5.3: "default"|"bomber"|"shaman"|"sniper"|"controller"|"ambush"|"guardian"|"charger"|"summoner"
    // G5.5: 普攻模式 "basic"|"double_strike"|"cleave"|"lunge"|"spread" (空=按类型回退)
    std::string attack_pattern_str;

    float attack_cooldown = 1.5f;

    // 嵌套子对象
    EnemyAIDef ai;
    std::vector<EnemySkillDef> skills;
    std::vector<BuffTrigger> on_hit;

    // D2: 弹道攻击配置
    EnemyProjectileDef projectile;

    // 精英标记
    bool is_elite = false;
    std::vector<EliteBuffEntry> elite_buff_pool;
};

// ── 全局注册表 (与 load_buff_defs / load_relic_defs 模式一致) ──
bool load_enemy_defs(const std::string& json_path);
int  load_enemy_defs_from_json(const char* json_text, MergeMode mode,
                                const char* id_namespace = nullptr);  // G4.2: namespace
const EnemyDef* get_enemy_def(const std::string& id);
const std::unordered_map<std::string, EnemyDef>& get_all_enemy_defs();  // G2.0: 统一接口
bool is_enemy_defs_loaded();  // G2.0
