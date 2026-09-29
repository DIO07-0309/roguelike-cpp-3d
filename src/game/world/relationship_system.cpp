#include "relationship_system.h"
#include <algorithm>

// ============================================================
// NPCRelation 实现
// ============================================================
int NPCRelation::get(RelationType t) const {
    switch (t) {
        case RelationType::TRUST:      return trust;
        case RelationType::RESPECT:    return respect;
        case RelationType::FEAR:       return fear;
        case RelationType::GRATITUDE:  return gratitude;
        case RelationType::CORRUPTION: return corruption;
    }
    return 0;
}

// ============================================================
// RelationshipSystem 实现
// ============================================================
NPCRelation* RelationshipSystem::find(int npc_id) {
    for (auto& r : _relations)
        if (r.npc_id == npc_id) return &r;
    return nullptr;
}

NPCRelation* RelationshipSystem::find_or_create(int npc_id) {
    NPCRelation* r = find(npc_id);
    if (r) return r;
    NPCRelation nr;
    nr.npc_id = npc_id;
    _relations.push_back(nr);
    return &_relations.back();
}

void RelationshipSystem::add_trust(int npc_id, int v) {
    find_or_create(npc_id)->trust += v;
}
void RelationshipSystem::add_respect(int npc_id, int v) {
    find_or_create(npc_id)->respect += v;
}
void RelationshipSystem::add_fear(int npc_id, int v) {
    find_or_create(npc_id)->fear += v;
}
void RelationshipSystem::add_gratitude(int npc_id, int v) {
    find_or_create(npc_id)->gratitude += v;
}
void RelationshipSystem::add_corruption(int npc_id, int v) {
    find_or_create(npc_id)->corruption += v;
}

void RelationshipSystem::apply_reward(const RelationReward& rr) {
    add_trust(rr.npc_id, rr.trust);
    add_respect(rr.npc_id, rr.respect);
    add_fear(rr.npc_id, rr.fear);
    add_gratitude(rr.npc_id, rr.gratitude);
    add_corruption(rr.npc_id, rr.corruption);
}

bool RelationshipSystem::check_relation(int npc_id, RelationType type, int threshold) const {
    for (auto& r : _relations)
        if (r.npc_id == npc_id)
            return r.get(type) >= threshold;
    return false;
}

// ============================================================
// 预设 RelationReward 常量
// ============================================================
const RelationReward RR_SAVE_PRISONER = {20, 2, 0, 0, 0, 0};
const RelationReward RR_SAVE_PRIEST   = {70, 3, 1, 0, 0, 0};
const RelationReward RR_BLOOD_RITUAL  = {70, 0, 0, 2, 0, 1};
const RelationReward RR_ACCEPT_CURSE  = {110,0, 0, 1, 0, 1};
const RelationReward RR_BOSS_KILLED   = {0,  0, 1, 0, 0, 0};
