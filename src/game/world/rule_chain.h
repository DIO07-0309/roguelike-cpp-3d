#pragma once
#include <string>
#include <vector>

class WorldState;
struct GameEvent;

// ============================================================
// G1 Step4: RuleChainManager — Boss 规则链管理
// 归属: WorldState (非 Player)
// 流程: Boss 死亡 → 激活规则 → WorldState 存储 → 后续楼层读取
// ============================================================

// ── 规则语义字典 ──
// Boss F5 (暗影骑士) 规则:
//   shadow_charge   — 后续楼层怪物获得 "暗影冲锋" 技能，CD 8s，冲刺后留下暗影路径
//   summon_priority — 后续楼层 Summoner 类型怪物召唤优先级 +1，召唤物数量 +1
//
// Boss F10 (地狱火魔) 规则:
//   arena_movement  — 后续楼层 Arena 房间增加移动火焰柱，周期性横扫 3x5 区域
//   shield_patience — 后续楼层 TANK 怪物获得护盾刷新 buff, 每 12s 获得 2 层护盾
//
// Boss F15 (深渊之主) 规则:
//   rule_override   — 所有已激活规则效果翻倍，且 BOSS 层额外生成迷你深渊裂隙

class RuleChainManager {
public:
    static void register_listener();
    static void unregister_listener();

    // Boss 死亡时激活对应规则 (由 BOSS_DEAD 事件触发)
    static void activate_for_boss(int boss_floor, WorldState& ws);

    // 按 Boss 楼层映射规则
    static std::vector<std::string> rules_for_boss(int boss_floor);

    // 规则人类可读名称 (供 HUD 展示)
    static const char* rule_display_name(const std::string& rule_id);

private:
    static void _on_boss_dead(const GameEvent& ev);
};
