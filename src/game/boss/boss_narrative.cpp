#include "boss_narrative.h"
#include "relationship_system.h"
#include "quest_manager.h"
#include "data/dialogue_defs.h"   // G2.1

// ── String → enum 辅助映射 (查询时使用, 保持 find_* 纯 C++ 算法) ──
static StoryStage _str_to_stage(const std::string& s) {
    if (s == "chapter_1") return StoryStage::CHAPTER_1;
    if (s == "chapter_2") return StoryStage::CHAPTER_2;
    if (s == "final")     return StoryStage::FINAL;
    return StoryStage::INTRO;  // "" → 不参与 stage 匹配
}

static WorldFlag _str_to_flag(const std::string& s) {
    if (s == "saved_prisoner")   return WorldFlag::Saved_Prisoner;
    if (s == "saved_priest")     return WorldFlag::Saved_Priest;
    if (s == "saved_merchant")   return WorldFlag::Saved_Merchant;
    if (s == "accepted_curse")   return WorldFlag::Accepted_Curse;
    if (s == "blood_ritual")     return WorldFlag::Blood_Ritual;
    if (s == "ignored_npc")      return WorldFlag::Ignored_NPC;
    if (s == "merchant_killed")  return WorldFlag::Merchant_Killed;
    if (s == "boss1_defeated")   return WorldFlag::Boss1_Defeated;
    if (s == "boss2_defeated")   return WorldFlag::Boss2_Defeated;
    if (s == "boss3_defeated")   return WorldFlag::Boss3_Defeated;
    if (s == "all_boss_defeated") return WorldFlag::All_Boss_Defeated;
    return WorldFlag::NONE;
}

static BuildType _str_to_build(const std::string& s) {
    if (s == "berserker")      return BuildType::BERSERKER;
    if (s == "fire_mage")      return BuildType::FIRE_MAGE;
    if (s == "poison_master")  return BuildType::POISON_MASTER;
    if (s == "time_master")    return BuildType::TIME_MASTER;
    if (s == "support")        return BuildType::SUPPORT;
    if (s == "projectile")     return BuildType::PROJECTILE;
    return BuildType::NONE;
}

// ============================================================
// G2.1: BossNarrative 构造函数 — 从 DialogueDef registry 构建
// 一条 DialogueDef → 一条 BossDialogue, 1:1 映射, 不拆分
// ============================================================
BossNarrative::BossNarrative() {
    for (auto& kv : get_all_dialogue_defs()) {
        auto& d = kv.second;

        // 跳过非 boss 对话 (未来 NPC/商店/教程用 type 字段区分)
        // G2.1: 所有 dialogue 均视为 boss 对话

        BossDialogue bd;
        bd.stage      = _str_to_stage(d.stage);
        bd.boss_floor = d.boss_floor;
        bd.required_flag = _str_to_flag(d.required_flag);
        bd.build      = _str_to_build(d.required_build);
        bd.relation_threshold = d.relation_threshold;
        bd.npc_id     = d.npc_id;
        bd.once       = d.once;

        // string storage — 避免 registry c_str() 悬挂
        _Texts txt;
        txt.intro   = d.intro_text;
        txt.phase2  = d.phase2_text;
        txt.death   = d.death_text;
        bd.intro    = txt.intro.empty()   ? nullptr : txt.intro.c_str();
        bd.phase2   = txt.phase2.empty()  ? nullptr : txt.phase2.c_str();
        bd.death    = txt.death.empty()   ? nullptr : txt.death.c_str();
        _texts.push_back(std::move(txt));

        _dialogues.push_back(bd);
    }
}

// ---- 查询 ----
const BossDialogue* BossNarrative::find_intro(int boss_floor, const WorldState& ws,
                                                BuildType bt,
                                                const RelationshipSystem& rels,
                                                const StoryDirector& story) const {
    for (auto& d : _dialogues) {
        if (d.boss_floor != boss_floor) continue;
        if (d.stage != StoryStage::INTRO && d.stage != story.stage()) continue;
        if (d.required_flag != WorldFlag::NONE && !ws.has(d.required_flag)) continue;
        if (d.build != BuildType::NONE && d.build != bt) continue;
        if (d.relation_threshold > 0 && d.npc_id > 0) {
            if (!rels.check_relation(d.npc_id, RelationType::TRUST, d.relation_threshold))
                continue;
        }
        if (d.intro) return &d;
    }
    return nullptr;
}

const BossDialogue* BossNarrative::find_phase2(int boss_floor, const WorldState& ws,
                                                 BuildType bt) const {
    for (auto& d : _dialogues) {
        if (d.boss_floor != boss_floor) continue;
        if (d.required_flag != WorldFlag::NONE && !ws.has(d.required_flag)) continue;
        if (d.build != BuildType::NONE && d.build != bt) continue;
        if (d.phase2) return &d;
    }
    return nullptr;
}

const BossDialogue* BossNarrative::find_death(int boss_floor, const WorldState& ws,
                                                BuildType bt) const {
    for (auto& d : _dialogues) {
        if (d.boss_floor != boss_floor) continue;
        if (d.required_flag != WorldFlag::NONE && !ws.has(d.required_flag)) continue;
        if (d.build != BuildType::NONE && d.build != bt) continue;
        if (d.death) return &d;
    }
    return nullptr;
}

// ============================================================
// D5 Step1: calc_modifier() — 正式根据WorldState/Quest/Build/Relation修改Boss
// ============================================================
BossModifierHook BossNarrative::calc_modifier(int boss_floor, const WorldState& ws,
                                                BuildType bt,
                                                const RelationshipSystem& rels) {
    BossModifierHook m;
    if (boss_floor == 5) _modifier_f5(ws, bt, rels, m);
    else if (boss_floor == 10) _modifier_f10(ws, bt, rels, m);
    else if (boss_floor == 15) _modifier_f15(ws, bt, rels, m);
    if (m.hp_scale < 0.50f) m.hp_scale = 0.50f;
    if (m.hp_scale > 1.80f) m.hp_scale = 1.80f;
    if (m.damage_scale < 0.70f) m.damage_scale = 0.70f;
    if (m.damage_scale > 1.50f) m.damage_scale = 1.50f;
    return m;
}
void BossNarrative::_modifier_f5(const WorldState& ws, BuildType bt, const RelationshipSystem& rels, BossModifierHook& m) {
// Saved_Prisoner → HP-20%
if (ws.has(WorldFlag::Saved_Prisoner)) {
    m.hp_scale = 0.80f;
    m.modifier_text = "HP -20% (囚犯的祝福)";
}
// Blood_Ritual → HP+25%, 伤害+15%
if (ws.has(WorldFlag::Blood_Ritual)) {
    m.hp_scale = 1.25f; m.damage_scale = 1.15f;
    m.modifier_text = "HP+25%% ATK+15%% (血祭)";
}
// BuildType: BERSERKER → Boss 格挡意识提升 (disable_skill逻辑)
if (bt == BuildType::BERSERKER) {
    // 不影响 modifier (仅透传到AI), 标记为"对Boss近战格挡+"
}
// POISON → Boss中毒减半
if (bt == BuildType::POISON_MASTER) {
    m.hp_scale += 0.10f;  // slight HP boost
    if (!m.modifier_text) m.modifier_text = "毒抗+ (毒术大师)";
}
}

void BossNarrative::_modifier_f10(const WorldState& ws, BuildType bt, const RelationshipSystem& rels, BossModifierHook& m) {
// Saved_Priest → 禁用一次AOE技能
if (ws.has(WorldFlag::Saved_Priest)) {
    m.disable_skill = true;
    m.modifier_text = "AOE禁用 (神官祝福)";
}
// Blood_Ritual → HP+20%
if (ws.has(WorldFlag::Blood_Ritual)) {
    m.hp_scale = 1.20f;
    m.modifier_text = m.modifier_text ? " " : "";
    m.modifier_text = "HP+20%% (血祭)";
}
// Accepted_Curse → Boss获得新Debuff技能 (e.g. 上curse)
if (ws.has(WorldFlag::Accepted_Curse)) {
    m.damage_scale += 0.10f;
}
// BuildType: FIRE_MAGE → Boss火抗+ (HP+)
if (bt == BuildType::FIRE_MAGE) {
    m.hp_scale += 0.15f;
}
// Priest trust≥3 → 禁用一次技能
if (rels.check_relation(70, RelationType::TRUST, 3)) {
    m.disable_skill = true;
}
}

void BossNarrative::_modifier_f15(const WorldState& ws, BuildType bt, const RelationshipSystem& rels, BossModifierHook& m) {
// Blood_Ritual → HP+30%, 伤害+20%
if (ws.has(WorldFlag::Blood_Ritual)) {
    m.hp_scale = 1.30f; m.damage_scale = 1.20f;
    m.modifier_text = "HP+30%% ATK+20%% (血祭融合)";
}
// Saved_Priest → Phase2延迟 (降低HP间接影响)
if (ws.has(WorldFlag::Saved_Priest)) {
    m.hp_scale = 0.85f;
    m.modifier_text = !m.modifier_text ? "HP-15%% (神官祈祷)" : m.modifier_text;
}
// Watcher respect≥3 → Phase2时间减少
if (rels.check_relation(140, RelationType::RESPECT, 3)) {
    m.damage_scale -= 0.10f;
}
// Merchant_Killed → 额外精英
if (ws.has(WorldFlag::Merchant_Killed)) {
    m.add_minions = true;
    if (!m.modifier_text) m.modifier_text = "额外精英 (商人咒骂)";
}
// Build: POISON → Boss腐化免疫++
if (bt == BuildType::POISON_MASTER) {
    m.hp_scale += 0.15f;
}
// Build: TIME → Boss Phase切换加速
if (bt == BuildType::TIME_MASTER) {
    m.damage_scale += 0.10f;
}
// Priest trust≥5 → Boss禁用一次技能
if (rels.check_relation(70, RelationType::TRUST, 5)) {
    m.disable_skill = true;
}
// Fear高 (任意NPC fear≥5)
for (auto& r : rels.all()) {
    if (r.fear >= 5) { m.damage_scale += 0.08f; break; }
}
}

