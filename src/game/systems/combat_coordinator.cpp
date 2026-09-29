#include "combat_coordinator.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "skill.h"
#include "item.h"
#include "combat_system.h"
#include "vfx_server.h"
#include "audio_server.h"
#include "config.h"
#include "core/logger.h"
#include "ai/player_behavior/player_behavior_recorder.h" // F15.1
#include <cmath>

void CombatCoordinator::skill_heavy_vfx(Player* player, const std::string&,
                                         std::vector<Effect>& effects) {
    VFXServer vfx;
    float cx = player->entity.rect.x + player->entity.rect.width / 2;
    float cy = player->entity.rect.y + player->entity.rect.height / 2;
    vfx.boss_circle(cx, cy);  // 复用 Boss AOE 特效作为 Heavy 技能反馈
    for (auto& e : vfx.effects) effects.push_back(e);
}

std::string CombatCoordinator::use_skill(int index, Player* player,
                                          std::vector<std::unique_ptr<Monster>>& monsters,
                                          GameMap* map, std::vector<Effect>& effects,
                                          AudioServer* audio, double game_time,
                                          float& time_stop_remaining,
                                          std::vector<std::pair<uint64_t, int>>& pending_damage,
                                          bool is_heavy) {
    if (!player || index >= (int)player->skills.active_skills.size()) return "";
    auto& sk = player->skills.active_skills[index];
    if (!sk->can_use(game_time)) return "";

    std::vector<Monster*> mlist;
    for (auto& m : monsters) mlist.push_back(m.get());

    // The World 时停 special-case (D2: Heavy 时停加长; D3 Step3: Evo)
    if (dynamic_cast<TheWorldSkill*>(sk.get())) {
        auto* tw = static_cast<TheWorldSkill*>(sk.get());
        sk->execute(player, mlist, map, is_heavy);
        sk->mark_used(game_time);
        float dur = tw->get_stop_duration();
        if (is_heavy) dur *= 1.3f;
        // D3 Step3 E1: 时停+20%
        if (sk->evolution_level >= 1) dur *= 1.2f;
        time_stop_remaining = dur;
        pending_damage.clear();
        if (audio) audio->play_sfx("timestop", 1.0f);
        VFXServer vfx;
        vfx.time_stop(player->entity.rect.x + player->entity.rect.width / 2,
                      player->entity.rect.y + player->entity.rect.height / 2);
        for (auto& e : vfx.effects) effects.push_back(e);
        return is_heavy ? "重·The World: 时间停止了!" : "The World: 时间停止了!";
    }

    // 时停期间技能：暂存伤害
    if (time_stop_remaining > 0) {
        std::unordered_map<Monster*, int> pre_hp;
        for (auto& m : monsters) pre_hp[m.get()] = m->combat.current_hp;
        sk->execute(player, mlist, map, is_heavy);
        sk->mark_used(game_time);
        for (auto& m : monsters) {
            int delta = pre_hp[m.get()] - m->combat.current_hp;
            if (delta > 0) {
                // P1-C4-fix(UAF): instance_id 代替裸指针 — 时停中目标可被释放
                pending_damage.emplace_back(m->instance_id, delta);
                m->combat.current_hp = pre_hp[m.get()];
                m->combat.is_alive = true;
            }
        }
        return "";
    }

    // 普通/Heavy 技能
    std::string result = sk->execute(player, mlist, map, is_heavy);
    sk->mark_used(game_time);
    // M4e: 记录技能施放时刻 (镜像 AI 观察窗口)
    player->_last_skill_time = (float)game_time;
    // F15.2: record skill usage with full context
    g_behavior.on_skill_use(sk->_skill_id.c_str(),
        (float)game_time, 0,  // floor set by game_scene
        player->entity.rect.x + player->entity.rect.width/2,
        player->entity.rect.y + player->entity.rect.height/2);

    // 技能 SFX (G3.2: _skill_id 替代 dynamic_cast)
    if (audio) {
        const std::string& sid = sk->_skill_id;
        if (sid == "slash" || sid == "shadow_strike" || sid == "blood_frenzy")
            audio->play_sfx("slash");
        else if (sid == "fireball" || sid == "ice_nova" || sid == "chain_lightning")
            audio->play_sfx("bolt");
        else if (sid == "self_heal") audio->play_sfx("heal");
        else if (sid == "summon_spirit") audio->play_sfx("bolt");
        if (is_heavy) audio->play_sfx("melee");
    }

    // ── G5.8.5: Recipe-based VFX dispatch ──
    VFXServer vfx;
    float cx = player->entity.rect.x + player->entity.rect.width / 2;
    float cy = player->entity.rect.y + player->entity.rect.height / 2;

    auto t = find_attack_target(player->entity.rect,
        reinterpret_cast<const std::vector<Monster*>&>(monsters), 10.0f);
    float tx = t ? t->entity.rect.x + t->entity.rect.width / 2 : cx + 100;
    float ty = t ? t->entity.rect.y + t->entity.rect.height / 2 : cy;
    int lvl = is_heavy ? sk->level + 1 : sk->level;

    // G5.8.5: Route through recipe dispatcher — no per-skill if-else needed
    vfx.play_recipe(sk->_skill_id.c_str(), cx, cy, player->direction, tx, ty, lvl);
    // D2: Heavy 额外特效环
    if (is_heavy) skill_heavy_vfx(player, sk->name, effects);
    for (auto& e : vfx.effects) effects.push_back(e);

    return result;
}

// G21: on_monster_killed / cleanup_dead_monsters 已删除 — 互相引用的死代码簇。
// cleanup_dead_monsters 全仓 0 个外部调用者, on_monster_killed 唯一调用点就在
// cleanup_dead_monsters 内部, 扫描器因此把两者都当成「有引用」放过 (与 tick/draw
// 这类重名盲区同类)。docs/RELIC_SYSTEM_AUDIT.md 早把它标为 G11 死代码却一直没清。
// 真实结算走 GameSceneCombat::on_monster_killed — XP 含 exp_scale(floor) 缩放,
// 发 on_player_leveled 与 EventBus PLAYER_LEVEL_UP。这份副本已跑偏 (无楼层 XP
// 缩放、不发任何升级信号), 留着只会让人哪天去「同步」两份升级实现。
// get_learned_names / random_active_skill 仍被 game_scene_combat.cpp 与
// game_scene.cpp 使用, 不在删除范围。

// P1-C4-fix: apply_pending_damage 已删除 — 零调用者的死路径, 且其裸指针
// 语义正是 UAF 根因 (真实结算走 GameSceneCombat::apply_pending_damage,
// 已改为 instance_id 查找).
