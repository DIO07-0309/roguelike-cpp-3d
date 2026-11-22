#pragma once
#include <string>

// ============================================================
// D7 Step5: 全局事件类型 — 所有 EventBus 事件统一管理
// ============================================================

enum class GameEventType {
    NONE = 0,

    // ── 玩家 ──
    // PLAYER_ATTACK/PLAYER_DAMAGED 已删 (零 emit/订阅; 旧设计文档仍有提及, 未来如需按文档接事件再恢复)
    PLAYER_DEAD,
    PLAYER_LEVEL_UP,

    // ── 怪物 ──
    // MONSTER_SPAWN 已删
    MONSTER_DIED,
    MONSTER_ATTACK,  // Q4.4: 怪物攻击 (近战/远程) — 驱动攻击音效

    // ── Boss ──
    BOSS_SPAWN,
    BOSS_PHASE2,
    BOSS_LAST_STAND,
    BOSS_DEAD,

    // ── 物品 / Relic ──
    ITEM_PICKUP,   // 规划: BATCH_3A 经济设计, 保留
    RELIC_GAIN,

    // ── G1: Attack Evolution ──
    ATTACK_EVOLVED,

    // ── G1: Skill Evolution ──
    SKILL_EVOLVED,

    // ── G1: Rule Chain ──
    BOSS_RULE_ACTIVATED,

    // ── Buff ── (BUFF_APPLIED/BUFF_EXPIRED 已删: 零引用)
    // ── 特殊房间 ── (SPECIAL_ROOM_ENTER/TRIGGER 已删: 零引用)
    // ── NPC / 对话 ── (NPC_DIALOGUE_START/END 已删: 零引用)

    // ── 任务 ──
    QUEST_ACCEPT,
    QUEST_COMPLETE,

    // ── 楼层 ──
    FLOOR_ENTER,   // FLOOR_CLEAR 已删: 零引用

    // ── Batch 2C: Room Encounter ──
    ROOM_LOCKED,   // 房间封门 (进入有怪房)
    ROOM_CLEAR,    // 房间清空开门

    // ── G9: Weapon combo events ──
    WEAPON_STAGE_1,
    WEAPON_STAGE_2,
    WEAPON_SPECIAL,

    // ── G9.3: Weapon-skill synergy ──
    WEAPON_ATTACK_COMPLETE,

    // ── G10.3: Element VFX ──
    ELEMENT_FIRE_HIT,
    ELEMENT_FIRE_CRITICAL,
    ELEMENT_ICE_SLOW,
    ELEMENT_ICE_FREEZE,
    ELEMENT_POISON_APPLY,
    ELEMENT_POISON_TICK,
    ELEMENT_LEVEL_UP,

    // ── F10.1: Boss arena domain events ──
    BOSS_DOMAIN_ENTER,
    BOSS_VULNERABLE_ENTER,
    WEAK_POINT_BREAK,

    // ── 游戏 ──
    GAME_CLEAR,     // 结局流程契约, 保留
    GAME_OVER,      // 结局流程契约, 保留
    ENDING_REACHED,
    // META_GAIN 已删: 零引用

    COUNT
};

// 轻量事件数据体
struct GameEvent {
    GameEventType type = GameEventType::NONE;
    void* sender = nullptr;
    int   int_val = 0;           // floor / damage / level / rarity
    float float_val = 0.0f;      // timer / pct
    const char* str_val = nullptr;   // name / id / msg
};
