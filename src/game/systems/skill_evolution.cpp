#include "skill_evolution.h"
#include "player.h"
#include "skill.h"
#include "build_score.h"
#include "core/event_bus.h"
#include "core/logger.h"

// ============================================================
// G1 Step3: SkillEvolutionManager 实现
// ============================================================

static constexpr int USE_LV2 = 20;   // Lv1→Lv2: 20 次使用
static constexpr int USE_LV3 = 50;   // Lv2→Lv3: 50 次使用 + Build 确认

static bool _s_registered = false;

void SkillEvolutionManager::register_listener() {
    if (_s_registered) return;
    // 此处无需额外 EventBus 监听 — 进化检查直接在 mark_used() 中调用 check_unlock()
    // 保留接口供未来扩展
    _s_registered = true;
    LOG_INFO("[SKL_EVO] SkillEvolutionManager registered");
}

void SkillEvolutionManager::unregister_listener() {}

void SkillEvolutionManager::check_unlock(Player* player) {
    if (!player) return;
    auto& skills = player->skills.active_skills;
    for (int i = 0; i < (int)skills.size(); i++)
        _try_evolve_skill(player, i);
}

bool SkillEvolutionManager::_try_evolve_skill(Player* player, int idx) {
    auto& skills = player->skills.active_skills;
    if (idx < 0 || idx >= (int)skills.size()) return false;
    auto& sk = skills[idx];

    int uc = sk->use_count;
    // Lv1 → Lv2: 20 次使用
    if (sk->evolution_level == 0 && uc >= USE_LV2) {
        std::string name = sk->evolve();
        if (!name.empty()) {
            int packed = 1 | (idx << 16);
            EventBus::inst().emit(GameEventType::SKILL_EVOLVED, player, packed);
            LOG_INFO("[SKL_EVO] %s 进化: %s (Lv1→Lv2) uses=%d", sk->name.c_str(), name.c_str(), uc);
            return true;
        }
    }
    // Lv2 → Lv3: 50 次使用 + Build 确认
    if (sk->evolution_level == 1 && uc >= USE_LV3 && has_confirmed_build(player)) {
        std::string name = sk->evolve();
        if (!name.empty()) {
            int packed = 2 | (idx << 16);
            EventBus::inst().emit(GameEventType::SKILL_EVOLVED, player, packed);
            LOG_INFO("[SKL_EVO] %s 进化: %s (Lv2→Lv3) uses=%d", sk->name.c_str(), name.c_str(), uc);
            return true;
        }
    }
    return false;
}

std::string SkillEvolutionManager::evo_name(const Player* player, int idx) {
    if (!player || idx < 0 || idx >= (int)player->skills.active_skills.size()) return "";
    auto& sk = player->skills.active_skills[idx];
    return sk->get_evolution_text();
}
