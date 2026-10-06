#pragma once
#include <string>
#include <vector>
#include "types/boss_types.h"

class WorldState;
class QuestManager;
class RelationshipSystem;
class StoryDirector;

// D4 Step5.5: BossNarrative — Boss读取世界的叙事层

class BossNarrative {
public:
    BossNarrative();
    const BossDialogue* find_intro(int boss_floor, const WorldState& ws, BuildType bt,
                                    const RelationshipSystem& rels,
                                    const StoryDirector& story) const;
    const BossDialogue* find_phase2(int boss_floor, const WorldState& ws, BuildType bt) const;
    const BossDialogue* find_death(int boss_floor, const WorldState& ws, BuildType bt) const;
    static BossModifierHook calc_modifier(int boss_floor, const WorldState& ws,
                                           BuildType bt, const RelationshipSystem& rels);

// G35-A: Boss floor modifier helpers (static)
    static void _modifier_f5(const WorldState& ws, BuildType bt, const RelationshipSystem& rels, BossModifierHook& m);
    static void _modifier_f10(const WorldState& ws, BuildType bt, const RelationshipSystem& rels, BossModifierHook& m);
    static void _modifier_f15(const WorldState& ws, BuildType bt, const RelationshipSystem& rels, BossModifierHook& m);
private:
    std::vector<BossDialogue> _dialogues;
    // G2.1: string storage — 避免 registry c_str() 悬挂
    struct _Texts { std::string intro, phase2, death; };
    std::vector<_Texts> _texts;
};
