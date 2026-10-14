#pragma once
#include <string>
#include <vector>
#include "world_state.h"
#include "boss_replay.h"

// ============================================================
// D6 Step1: EndingDirector — 多结局系统
// ============================================================

enum class EndingType {
    BAD_END,      // 血祭/诅咒/杀商人
    NORMAL_END,   // Boss3 击杀
    GOOD_END,     // 全Boss + 救NPC
    TRUE_END,     // True_Ending_Ready + BossRank≥A
    SECRET_END,   // True + BossRank≥S + 全Collection
};

struct NPCEnding {
    int   npc_id;
    const char* name;
    const char* result;   // "幸存" / "死亡" / "离开" / etc.
    const char* detail;   // 1-2句结局文字
};

struct WorldEnding {
    EndingType type;
    const char* sky_color;     // "金色" / "深红" / "灰暗" / "纯白"
    const char* final_line;    // 最终台词
    const char* credits_title;
    int  meta_soul_bonus;
    int  meta_knowledge_bonus;
};

class EndingDirector {
public:
    void begin(const WorldState& ws, BossRank rank, float collection_pct,
               const class RelationshipSystem& rels,
               const class QuestManager& qm);
    EndingType current() const { return _type; }
    const char* ending_name() const;
    const char* ending_title() const;
    const char* final_line() const;
    const char* sky_color() const;
    int  meta_reward_soul() const;
    int  meta_reward_knowledge() const;
    const std::vector<NPCEnding>& npc_endings() const { return _npc_endings; }
    const WorldEnding& world_ending() const { return _world; }

    // G2.5: 已解锁结局追踪 (供 Save v3)
    const std::vector<int>& unlocked() const { return _unlocked; }
    void restore_unlocked(const std::vector<int>& list) { _unlocked = list; }

    // 调试: 强制设置EndingType
    void debug_override(EndingType et) { _type = et; }

private:
    EndingType _type = EndingType::NORMAL_END;
    std::vector<NPCEnding> _npc_endings;
    WorldEnding _world;
    void _build_npc_endings(const WorldState& ws, const RelationshipSystem& rels);
    // G53: begin 拆出的三段 (结局判定 / registry 填充 / 解锁记录)
    EndingType _determine_ending(const WorldState& ws, BossRank rank,
                                 float collection_pct) const;
    void _apply_ending_def();
    void _record_unlocked(int ending_type);
    // G2.5: string storage — 避免 registry c_str() 悬挂
    struct _EndTexts { std::string sky, line, title; };
    _EndTexts _texts;
    std::vector<int> _unlocked;  // G2.5: 已解锁结局类型列表
};
