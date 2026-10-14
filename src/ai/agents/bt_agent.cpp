#include "ai/agents/bt_agent.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "combat_system.h"
#include "build_score.h"
#include <cmath>

BTAgent::BTAgent()  {}
BTAgent::~BTAgent() {}

// ═══════════════════════════════════════════════════════════
//  Blackboard sync: game state → key-value pairs
// ═══════════════════════════════════════════════════════════

// 敌人态势: 存活数 / 最近敌人距离与名字 / AOE 内数量
void BTAgent::_sync_enemy_situation(const Player* player,
    const std::vector<Monster*>& monsters) {
    int alive = 0;
    float nearest_dist = 9999;
    std::string nearest_id = "none";
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        alive++;
        float d = hypotf(m->entity.rect.x - player->entity.rect.x,
                         m->entity.rect.y - player->entity.rect.y);
        if (d < nearest_dist) { nearest_dist = d; nearest_id = m->name; }
    }
    _board.set("enemy_count", alive);
    _board.set("enemy_near", nearest_dist < 3.0f * 32.0f);
    _board.set("nearest_dist", nearest_dist);
    _board.set("enemies_in_aoe",
               _count_enemies_in_range(player, monsters, 4.0f * 32.0f));
    _board.set("nearest_id", nearest_id);   // name 也进黑板, BT 节点可直接读
}

// Boss 是否在 5 格内 (首个即判定, 不需要统计全部)
void BTAgent::_sync_boss_proximity(const Player* player,
    const std::vector<Monster*>& monsters) {
    for (auto* m : monsters)
        if (m && m->is_boss && m->combat.is_alive) {
            float d = hypotf(m->entity.rect.x - player->entity.rect.x,
                             m->entity.rect.y - player->entity.rect.y);
            _board.set("boss_near", d < 5.0f * 32.0f);
            return;
        }
    _board.set("boss_near", false);
}

// 玩家是否站在某个特殊房间附近 (3 格内)
void BTAgent::_sync_room_proximity(const Player* player, const GameMap* map) {
    if (!map) { _board.set("near_room", false); return; }
    for (auto& sr : map->special_rooms) {
        float dx = player->entity.rect.x + player->entity.rect.width / 2
                   - (sr.cx * 32 + 16);
        float dy = player->entity.rect.y + player->entity.rect.height / 2
                   - (sr.cy * 32 + 16);
        if (sqrtf(dx * dx + dy * dy) < 3.0f * 32.0f) {
            _board.set("near_room", true);
            return;
        }
    }
    _board.set("near_room", false);
}

void BTAgent::_sync_state(const Player* player,
    const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player) return;

    _board.set("hp", (float)player->combat.current_hp);
    _board.set("max_hp", (float)player->combat.max_hp);
    _board.set("hp_ratio", _hp_ratio(player));

    _sync_enemy_situation(player, monsters);
    _sync_boss_proximity(player, monsters);
    _sync_room_proximity(player, map);

    _board.set("stairs_active", stairs_active);
    _board.set("boss_intro", boss_intro_active);

    // Skills: 任一主动技 ready 即可 (BT 只关心"能不能放", 不关心放哪个)
    bool any_ready = false;
    for (int i = 0; i < 4; i++)
        if (i < (int)player->skills.active_skills.size()
            && player->skills.active_skills[i]->can_use(_game_time)) {
            any_ready = true;
            break;
        }
    _board.set("skill_ready", any_ready);

    // Heal slot available
    // Heal skills are typically index 2 (self_heal). Check if ready.
    _board.set("heal_ready", player->skills.active_skills.size() > 2);

    // Build identity
    _board.set("prefer_range", _board.get("prefer_range", 0.3f));
    _board.set("prefer_aoe", _board.get("prefer_aoe", 0.2f));
}

// ═══════════════════════════════════════════════════════════
//  Tree builder
// ═══════════════════════════════════════════════════════════

// ── G46: 由若干节点克隆出一个 Sequence 子树 ──
// build_tree 里 6 个子树都是"条件 → 动作"的两/三段 Sequence, 逐个手写
// vector + push_back + clone 是纯样板。用变参 helper 一次成型。
static std::unique_ptr<bt::Sequence> make_seq(
        std::vector<std::unique_ptr<bt::Node>> children) {
    return std::make_unique<bt::Sequence>(std::move(children));
}

// 把若干节点指针克隆后收集成 vector (供 make_seq 使用)
static std::vector<std::unique_ptr<bt::Node>> clones_of(
        std::initializer_list<bt::Node*> nodes) {
    std::vector<std::unique_ptr<bt::Node>> v;
    for (bt::Node* n : nodes) v.push_back(n->clone());
    return v;
}

// ── G46: 建 9 个条件节点 ──
// 从 build_tree 抽出 — 条件构造是纯样板 (名字 + lambda), 集中一处便于
// 对照 Blackboard 键名审阅 (键名拼错不会报错, 只会让该分支永不命中)
static void bt_build_conditions(
        std::vector<std::unique_ptr<bt::Condition>>& out) {
    using namespace bt;
    out.push_back(std::make_unique<Condition>("BossIntro",
        [](Blackboard& b) { return b.get("boss_intro", false); }));
    out.push_back(std::make_unique<Condition>("StairsActive",
        [](Blackboard& b) { return b.get("stairs_active", false); }));
    out.push_back(std::make_unique<Condition>("HPLow",
        [](Blackboard& b) { return b.get("hp_ratio", 1.0f) < 0.35f; }));
    out.push_back(std::make_unique<Condition>("HealReady",
        [](Blackboard& b) { return b.get("heal_ready", false); }));
    out.push_back(std::make_unique<Condition>("EnemyNear",
        [](Blackboard& b) { return b.get("enemy_near", false); }));
    out.push_back(std::make_unique<Condition>("BossNear",
        [](Blackboard& b) { return b.get("boss_near", false); }));
    out.push_back(std::make_unique<Condition>("SkillReady",
        [](Blackboard& b) { return b.get("skill_ready", false); }));
    out.push_back(std::make_unique<Condition>("EnemiesInAoE",
        [](Blackboard& b) { return b.get("enemies_in_aoe", 0) >= 2; }));
    out.push_back(std::make_unique<Condition>("NearRoom",
        [](Blackboard& b) { return b.get("near_room", false); }));
}

// ── G46: 建动作节点 (写 action 字符串进 Blackboard) ──
static void bt_build_actions(std::vector<std::unique_ptr<bt::Action>>& out) {
    using namespace bt;
    out.push_back(std::make_unique<Action>("Confirm",
        [](Blackboard& b) { b.set<std::string>("action", "confirm"); }));
    out.push_back(std::make_unique<Action>("Descend",
        [](Blackboard& b) { b.set<std::string>("action", "descend"); }));
    out.push_back(std::make_unique<Action>("Heal",
        [](Blackboard& b) { b.set<std::string>("action", "skill_3"); }));
    out.push_back(std::make_unique<Action>("Attack",
        [](Blackboard& b) { b.set<std::string>("action", "attack"); }));
    out.push_back(std::make_unique<Action>("AoESkill",
        [](Blackboard& b) { b.set<std::string>("action", "skill_2"); }));
    out.push_back(std::make_unique<Action>("Pickup",
        [](Blackboard& b) { b.set<std::string>("action", "pickup"); }));
    out.push_back(std::make_unique<Action>("Wander",
        [](Blackboard& b) { b.set<std::string>("action", "move_up"); }));
}

// ── G46: 子树 + root Selector 装配 ──
// 下标契约: conds/acts 的顺序由 bt_build_conditions / bt_build_actions 的
// push_back 顺序决定。用 enum 而非裸数字 — 顺序不一致时启动即崩, 不会静默
// 选错分支 (vector::size() 非 constexpr, 只能运行期断言)。
static std::unique_ptr<bt::Selector> bt_assemble_root(
        std::vector<std::unique_ptr<bt::Condition>>& conds,
        std::vector<std::unique_ptr<bt::Action>>& acts) {
    enum CondIdx { C_BOSS_INTRO, C_STAIRS, C_HP_LOW, C_HEAL, C_ENEMY,
                   C_BOSS_NEAR, C_SKILL, C_AOE, C_ROOM, C_COUNT };
    enum ActIdx { A_CONFIRM, A_DESCEND, A_HEAL, A_ATTACK, A_AOE,
                  A_PICKUP, A_WANDER, A_COUNT };
    if (conds.size() != C_COUNT || acts.size() != A_COUNT) {
        assert(false && "bt condition/action count mismatch index enum");
        return nullptr;
    }
    // P0-1 修复记录: intro / stairs 最初写成裸 Condition, 导致
    // actConfirm / actDescend 从未入树 (BT 无法关 intro、无法下楼)。
    auto introST = make_seq(clones_of({conds[C_BOSS_INTRO].get(), acts[A_CONFIRM].get()}));
    auto stairsST = make_seq(clones_of({conds[C_STAIRS].get(), acts[A_DESCEND].get()}));
    auto healST   = make_seq(clones_of({conds[C_HP_LOW].get(), conds[C_HEAL].get(),
                                        acts[A_HEAL].get()}));
    auto bossST   = make_seq(clones_of({conds[C_BOSS_NEAR].get(), acts[A_ATTACK].get()}));
    auto aoeST    = make_seq(clones_of({conds[C_AOE].get(), conds[C_SKILL].get(),
                                        acts[A_AOE].get()}));
    auto combatST = make_seq(clones_of({conds[C_ENEMY].get(), acts[A_ATTACK].get()}));
    auto pickupST = make_seq(clones_of({conds[C_ROOM].get(), acts[A_PICKUP].get()}));

    // root = Selector, 优先级从上到下; 末尾 Wander 恒成功保证总有落点
    std::vector<std::unique_ptr<bt::Node>> root_children;
    root_children.push_back(std::move(introST));    // 1. Boss intro → confirm
    root_children.push_back(std::move(stairsST));   // 2. Stairs → descend
    root_children.push_back(std::move(healST));     // 3. Heal if low
    root_children.push_back(std::move(bossST));     // 4. Boss fight
    root_children.push_back(std::move(aoeST));      // 5. AoE if multi-target
    root_children.push_back(std::move(combatST));   // 6. Attack nearest
    root_children.push_back(std::move(pickupST));   // 7. Pickup near room
    root_children.push_back(acts[A_WANDER]->clone());  // 8. Wander
    return std::make_unique<bt::Selector>(std::move(root_children));
}

void BTAgent::build_tree() {
    std::vector<std::unique_ptr<bt::Condition>> conds;
    std::vector<std::unique_ptr<bt::Action>> acts;
    bt_build_conditions(conds);
    bt_build_actions(acts);
    _root = bt_assemble_root(conds, acts);
}

// ═══════════════════════════════════════════════════════════
//  Interface: same as DecisionAgent
// ═══════════════════════════════════════════════════════════

std::string BTAgent::best_action(const Player* player,
    const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player || !_root) return "move_up";

    _sync_state(player, monsters, map, stairs_active, boss_intro_active);
    _board.set<std::string>("action", "move_up"); // default

    _root->tick(_board);
    return _board.get<std::string>("action", "move_up");
}

bool BTAgent::is_action_just_pressed(const char* action_name,
    const Player* player, const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player) return false;
    std::string best = best_action(player, monsters, map, stairs_active, boss_intro_active);
    return best == action_name;
}

bool BTAgent::accept_event(float risk_pct, const std::string& effect_desc,
                            const Player* player) const {
    if (!player) return false;
    float hp = _hp_ratio(player);
    if (risk_pct > 0.40f && hp < 0.50f) return false;
    if (risk_pct > 0.25f && hp < 0.30f) return false;
    if (risk_pct > 0.10f && hp < 0.15f) return false;
    bool high_value = (effect_desc.find("relic") != std::string::npos) ||
                      (effect_desc.find("skill") != std::string::npos) ||
                      (effect_desc.find("legendary") != std::string::npos);
    if (high_value && hp > 0.60f) return true;
    if (risk_pct == 0) return true;
    return hp > risk_pct * 2.0f + 0.25f;
}

// ═══════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════

void BTAgent::_resolve_profile(const Player* player) {
    auto bt = calculate_build(player).identify();
    float range = 0.3f, aoe = 0.2f;
    switch (bt) {
    case BuildType::ICE_MAGE:       range=0.9f; aoe=0.8f; break;
    case BuildType::FIRE_MAGE:      range=0.7f; aoe=0.7f; break;
    case BuildType::LIGHTNING_MAGE: range=0.6f; aoe=0.6f; break;
    case BuildType::BERSERKER:      range=0.0f; aoe=0.3f; break;
    case BuildType::SUMMON_LORD:    range=0.6f;         break;
    case BuildType::JUGGERNAUT:     range=0.0f; aoe=0.4f; break;
    default: break;
    }
    _board.set("prefer_range", range);
    _board.set("prefer_aoe", aoe);
}

Monster* BTAgent::_find_nearest(const Player* player,
    const std::vector<Monster*>& monsters) const {
    Monster* best = nullptr; float bd = 99999;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        float d = hypotf(m->entity.rect.x + m->entity.rect.width/2 - px,
                         m->entity.rect.y + m->entity.rect.height/2 - py);
        if (d < bd) { bd = d; best = m; }
    }
    return best;
}

float BTAgent::_hp_ratio(const Player* p) const {
    if (!p || p->combat.max_hp <= 0) return 0;
    return (float)p->combat.current_hp / (float)p->combat.max_hp;
}

int BTAgent::_count_enemies_in_range(const Player* player,
    const std::vector<Monster*>& monsters, float range_px) const {
    int n = 0;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        if (hypotf(m->entity.rect.x - px, m->entity.rect.y - py) < range_px) n++;
    }
    return n;
}
