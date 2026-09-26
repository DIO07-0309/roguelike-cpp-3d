#include "sim_ai.h"
#include "player.h"
#include "monster.h"
#include "boss.h"           // Q3.2: BossAI windup 状态读取 (蓄力判断)
#include "game_map.h"
#include "item.h"           // P1-A3: ConsumableItem 药水判定
#include "combat_system.h"  // rng
#include "build_score.h"    // BuildType, calculate_build
#include "core/logger.h"
#include "ai/mcts/mcts_search.h"
#include "ai/mcts/action.h"
#include "sim_ai_teleport.h"      // P0-M2: room-domain teleport contract
#include "world/room_manager.h"   // P0-M2: room_at for teleport logging
#include <cmath>
#include <cstring>
#include <algorithm>
#include <queue>

bool DecisionAgent::g_use_mcts = false;
int  DecisionAgent::g_mcts_iters = 100;

// M2-C: sim 卡墙恢复诊断计数 (GameScene::_collect_sim_stats 快照; 单线程 sim)
int sim_stuck_teleports = 0;   // [PLAYER-FIX] 口袋传送次数
int sim_stuck_rotations = 0;   // 旋转脱困进入次数
int sim_stuck_loot_wd = 0;     // 搜刮看门狗强制下楼次数
int sim_stuck_watchdog = 0;    // G13: >0 = 本帧请求强制结算 (STUCK_RECOVERED)

// G13: 动作分布诊断 — 结算时打印, 定位 AI 实际行为
// 0=none 1=move_* 2=attack 3=skill_* 4=pickup 5=descend 6=use_potion 7=其他
int sim_action_counts[8] = {0};
// G14: 移动分支归因 — 0:recovery 1:loot 2:room 3:approach 4:stand(圈内站桩)
int sim_move_branch[5] = {0};
// G14: BFS/贪心全部失败次数 — appr 分支 step==-1 时自增
int sim_bfs_fail = 0;
// G14: stairs 分支归因 — 0:stairs_active总帧 1:descend 2:move(走楼梯/搜刮)
int sim_stairs[3] = {0};
// G14: _evaluate_move 返回 0.1 的无怪随机游走帧数
int sim_move_noenemy = 0;
// G14: 卡死采样 — ...(..., LOCKED门数, 怪4邻可走数)
int sim_stuck_sample[64][13] = {0};
int sim_stuck_sample_count = 0;
// G14: 旋转脱困被墙挡住的次数 (尝试移动但可走性失败)
int sim_rot_blocked = 0;
// G14: 卡死时 BFS 朝怪命中/失败次数
int sim_stuck_bfs_hit = 0, sim_stuck_bfs_fail = 0;
// G14: 传送尝试次数 (含失败) — 判断传送分支是否被走到
int sim_tp_attempts = 0;

static int _action_bucket(const std::string& a) {
    if (a.empty()) return 0;
    if (a.rfind("move_", 0) == 0) return 1;
    if (a == "attack") return 2;
    if (a.rfind("skill_", 0) == 0) return 3;
    if (a == "pickup") return 4;
    if (a == "descend") return 5;
    if (a == "use_potion") return 6;
    return 7;
}

// G13: 卡死看门狗阈值 — 本局累计卡死时长 (秒) / 传送尝试冷却 (秒)
// 36000 帧预算约 600s; 120s 卡死 = 1/5 预算耗在无进展, 判本局不可恢复。
// 实测: 240s 反而更差 (seed101 真实死亡 8→1), 局跑得越久越容易在卡死中耗光预算;
//       120s 在各种子下平衡最好。阈值可调, 但不应无脑拉高。
static const double kStuckTotalBudget = 120.0;
static const float  kTeleportCooldown = 1.0f;

// G8.3: Build SimulationState snapshot from live game state
mcts::SimulationState DecisionAgent::build_sim_state(
    const Player* player, const std::vector<Monster*>& monsters, double game_time) {
    mcts::SimulationState s;
    if (!player) return s;
    auto& p = s.player;
    p.hp = (float)player->combat.current_hp;
    p.max_hp = (float)player->combat.max_hp;
    p.x = player->entity.rect.x / 32.0f;
    p.y = player->entity.rect.y / 32.0f;
    p.attack = player->combat.attack;
    p.pdef = player->combat.physical_defense;
    p.mdef = player->combat.magical_defense;
    p.alive = player->combat.is_alive;
    // Cooldowns: real remaining time (Q3.15 A6 fix — was faked to constant
    // 0.5s/0s, which permanently disabled ATTACK at the MCTS root since
    // get_possible_actions requires attack_cooldown <= 0)
    // P1-C7-A: 空手已迁 executor 轨 — 感知源统一为 WeaponComponent::can_attack
    p.attack_cooldown = player->weapon.can_attack(game_time) ? 0.0f : 0.5f;
    for (int i = 0; i < 4; i++) {
        if (i < (int)player->skills.active_skills.size() && player->skills.active_skills[i])
            p.skill_cooldowns[i] = std::max(0.0f,
                (float)player->skills.active_skills[i]->remaining_cooldown(game_time));
        else
            p.skill_cooldowns[i] = 99.0f;   // 未拥有的技能 = 永不可用
    }
    // Buffs
    for (auto& b : player->active_buffs)
        if (b.stacks > 0)
            p.buffs.push_back({b.id, b.stacks, b.remaining});

    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        mcts::MonsterSnapshot ms;
        ms.type = m->name;
        ms.hp = (float)m->combat.current_hp;
        ms.max_hp = (float)m->combat.max_hp;
        ms.x = m->entity.rect.x / 32.0f;
        ms.y = m->entity.rect.y / 32.0f;
        ms.attack = m->combat.attack;
        ms.pdef = m->combat.physical_defense;
        ms.mdef = m->combat.magical_defense;
        ms.alive = true;
        ms.is_boss = m->is_boss;
        s.monsters.push_back(ms);
    }
    s.rng.seed = (uint32_t)(int)(player->entity.rect.x * 1000 + player->entity.rect.y);
    return s;
}

// ═══════════════════════════════════════════════════════════
//  G7.4: Build-aware behavioral profiles
// ═══════════════════════════════════════════════════════════

DecisionAgent::DecisionAgent() {}

void DecisionAgent::start(const Player* player) {
    _frame = 0; _dir_timer = 0; _current_dir = -1;
    _resolve_profile(player);
}

void DecisionAgent::_resolve_profile(const Player* player) {
    if (!player) return;
    _build_type = calculate_build(player).identify();

    // Default: balanced
    _prefer_range = 0.3f; _prefer_aoe = 0.2f;
    _prefer_skill = 0.4f; _aggro_bias = 0.5f;
    _prefer_heal = 0.35f;
    _skill_priority[0]=0; _skill_priority[1]=1;
    _skill_priority[2]=2; _skill_priority[3]=3;

    switch (_build_type) {
    case BuildType::ICE_MAGE:
        _prefer_range = 0.9f; _prefer_aoe = 0.8f;
        _prefer_skill = 0.7f; _aggro_bias = 0.2f; // kite
        break;
    case BuildType::FIRE_MAGE:
        _prefer_range = 0.7f; _prefer_aoe = 0.7f;
        _prefer_skill = 0.8f; _aggro_bias = 0.3f;
        break;
    case BuildType::LIGHTNING_MAGE:
        _prefer_range = 0.6f; _prefer_aoe = 0.6f;
        _prefer_skill = 0.7f; _aggro_bias = 0.4f;
        break;
    case BuildType::BERSERKER:
        _prefer_range = 0.0f; _prefer_aoe = 0.3f;
        _prefer_skill = 0.3f; _aggro_bias = 0.9f; // rush in
        break;
    case BuildType::BLEED_BLADE:
        _prefer_range = 0.1f; _prefer_aoe = 0.3f;
        _prefer_skill = 0.5f; _aggro_bias = 0.7f;
        break;
    case BuildType::SHADOW_STRIKER:
        _prefer_range = 0.0f; _prefer_aoe = 0.0f;
        _prefer_skill = 0.6f; _aggro_bias = 0.6f; // single target burst
        break;
    case BuildType::SUPPORT:
        _prefer_range = 0.4f; _aggro_bias = 0.3f;
        _prefer_heal = 0.50f; // heal early
        break;
    case BuildType::JUGGERNAUT:
        _prefer_range = 0.0f; _prefer_aoe = 0.4f;
        _prefer_skill = 0.2f; _aggro_bias = 0.8f; // tank
        break;
    case BuildType::SUMMON_LORD:
        _prefer_range = 0.6f; _prefer_skill = 0.8f;
        _aggro_bias = 0.2f; // stay back, let summons fight
        break;
    case BuildType::POISON_MASTER:
        _prefer_range = 0.5f; _prefer_aoe = 0.4f;
        _prefer_skill = 0.5f; _aggro_bias = 0.4f;
        break;
    case BuildType::TIME_MASTER:
        _prefer_range = 0.5f; _prefer_skill = 0.7f;
        _aggro_bias = 0.3f;
        break;
    default: break; // keep defaults
    }
}

void DecisionAgent::tick() {
    _frame++;
    _cached_frame = -1;  // Q3.1: 强制下一查询重算 (世界已变)
}

// ═══════════════════════════════════════════════════════════
//  G7.4: Action evaluators
// ═══════════════════════════════════════════════════════════

// P1-C2: 玩家当前中毒层数 (0=无) — 毒 DOT 是基线最大单一死因 (33.6%),
// AI 必须感知毒状态: 中毒时提前喝药 + 优先击杀毒源
static int _player_poison_stacks(const Player* p) {
    if (!p) return 0;
    for (auto& b : p->active_buffs)
        if (b.id == "poison") return b.stacks;
    return 0;
}

// P1-C2: 怪物命中是否会上毒 (orc/elite_orc/poison_wyrm 等) — 击杀毒源
// 是切断再上毒的最直接手段 (数据来自 enemies.json on_hit, 只读不复制)
static bool _monster_applies_poison(const Monster* m) {
    if (!m) return false;
    for (auto& trig : m->on_hit_triggers)
        if (trig.buff_id == "poison") return true;
    return false;
}

// P1-C4 探针: 攻击评分命中时的最近怪距离分布 — 实体定义在 sim_runner.cpp
// (test 二进制链接 sim_runner 但不链接 sim_ai, 引用方 extern 即可)
extern int g_p1c4_probe_frame;
extern int g_p1c4_d_bucket[8];

// P1-C5: 决策攻击半径 (px) — FIST 走 legacy 48px; 持械取当前段 range×32.
// 病理 (C4PROBE): 写死 1.5 格判定使 dagger(32px) 在 32-48px 空挥、
// sword(64px) 在 64-80px 站桩 — d1 边缘圈占 82.2%, F1 围殴 91% 的直接死因
float DecisionAgent::_decision_attack_reach_px(const Player* p) const {
    if (!p) return 48.0f;
    // P1-C5: 判定线 = 武器第 0 段 range (数据驱动, 消除 dagger 32px 半径下
    // 32-48px 空挥圈). 固定取 stages[0] 而非 current_stage():
    // 段位动态版实测引入非确定性 (同帧同 rng 下 kill 目标选择分岔,
    // 3 形态结局; 根因未明, 记 P1-C6 专项 — 疑 combo_index 与
    // hit_detect 目标 tie-break 的交互). stages[0] 3/3 稳定且保留核心收益.
    const WeaponDef* def = p->weapon.current_def();
    if (!def || def->type == WeaponType::FIST) return 1.5f * 32.0f;
    return def->stages[0].range * 32.0f;
}

// P1-C5: 空手判定 — 掉落武器优先追击的触发条件
bool DecisionAgent::_is_bare_fisted(const Player* p) {
    return p && p->weapon.weapon_type() == WeaponType::FIST;
}

float DecisionAgent::_evaluate_attack(const Player* p,
    const std::vector<Monster*>& monsters) const {
    auto* t = _find_nearest(p, monsters);
    if (!t) return 0;
    float d = hypotf(t->entity.rect.x + t->entity.rect.width/2 - (p->entity.rect.x + p->entity.rect.width/2),
                     t->entity.rect.y + t->entity.rect.height/2 - (p->entity.rect.y + p->entity.rect.height/2));
    // G13: 用武器真实射程 — 原硬编码 48px 让 crossbow(10格/320px) 成近战武器,
    //       AI 判"出圈"→ 不攻击, 实测 atk=1%/move=98% 全程空转。
    //       max(,1.5) 保底 48px: P1-C5 实验收窄 dagger 致负回归(af 6.25→2.55),
    //       保底值与旧行为一致, 只修正射程被低估的远程武器。
    //       归一化分母随之从固定 96px 改为 reach×2 — 原 48px 时恰好相等(96px),
    //       远程按比例放大, 保持"贴脸 1.0 / 射程处 0.5"的相对曲线。
    float reach_px = std::max(p->weapon.current_range(), 1.5f) * 32.0f;
    if (d > reach_px) return 0; // out of range — no score
    // Melee builds score higher for attacking
    float base = 1.0f - _prefer_range; // range=0 → score 1.0
    float score = base * (1.0f - d / (reach_px * 2.0f)); // closer = better
    // P1-C2: 自身中毒时毒源怪 +0.25 — 斩断再上毒源头 (兽人族 25%/击 上毒)
    if (_player_poison_stacks(p) > 0 && _monster_applies_poison(t))
        score += 0.25f;
    // P1-C4 探针: 近距未出手采样 — 每 180 帧 (3s) 记录"距真实攻击半径的比值",
    // 用于量化攻击圈内"看得见打不着"的站桩时长 (相对化后跨武器可比)
    if (++g_p1c4_probe_frame % 180 == 0) {
        int b = (int)(d * 4.0f / reach_px);   // 0-4 格按 reach 四等分
        if (b > 7) b = 7;
        g_p1c4_d_bucket[b]++;
    }
    return score;
}

float DecisionAgent::_evaluate_skill(int slot, const Player* p,
    const std::vector<Monster*>& monsters) const {
    if (slot < 0 || slot >= 4) return 0;
    // Q3.2: 槽位越界/技能空/冷却中 → 不得给分 (否则站桩按CD技能挨打)
    if (slot >= (int)p->skills.active_skills.size()) return 0;
    auto& sk = p->skills.active_skills[slot];
    if (!sk || !sk->can_use(_game_time)) return 0;
    int n = _count_in_range(p, monsters, 5.0f * 32.0f);
    if (n <= 0) return 0;
    float aoe_bonus = _prefer_aoe * (n > 1 ? 1.0f : 0.3f);
    float score = _prefer_skill * (0.5f + aoe_bonus);
    // M4.4: 单体Boss战 — 伤害技能冷却好就放 (补足普攻DPS缺口, 对冲Boss自愈)
    auto* t = _find_nearest(p, monsters);
    if (t && t->is_boss && !dynamic_cast<SelfHealSkill*>(sk.get())) score += 0.9f;
    return score;
}

static const int kBfsDx[4] = {0, 0, -1, 1};  // up, down, left, right
static const int kBfsDy[4] = {-1, 1, 0, 0};

// Q3.10: 口袋兜底 — 卡死≥8s(四向逃脱均失败)时传送玩家至最近存活怪相邻可行走格
// 破口袋/48-51px 隔墙死局: 传送后玩家必能普攻到该怪, 战斗恢复, 楼层推进
static bool _teleport_player_to_nearest(Player* p,
    const std::vector<Monster*>& monsters, const GameMap* map,
    const RoomManager* rooms) {
    if (!p || !map) return false;
    const Monster* t = nullptr;
    float bd = 1e9f;
    for (const Monster* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        float d = hypotf(m->entity.rect.x - p->entity.rect.x,
                         m->entity.rect.y - p->entity.rect.y);
        if (d < bd) { bd = d; t = m; }
    }
    if (!t) return false;

    // P0-M2: room-domain deterministic target selection
    // (contract: see core/sim/sim_ai_teleport.h + tests/sim/p0_teleport_test.cpp)
    TeleportQuery q;
    q.player_rect = p->entity.rect;
    q.target = const_cast<Monster*>(t);
    q.map = map;
    q.rooms = rooms;
    for (const Monster* m : monsters)
        if (m && m->combat.is_alive && m != t)
            q.extra_monsters.push_back(const_cast<Monster*>(m));
    TeleportResult r = sim_ai_teleport_target(q);
    if (!r.found) return false;

    p->entity.position.x = (float)(r.tile_x * 32);
    p->entity.position.y = (float)(r.tile_y * 32);
    p->entity.sync_rect();
    // G14: 落点在 CLOSED 门 → 即时开启 (玩家已站门 tile, 需可走)
    if (const_cast<GameMap*>(map)->door_state_at(r.tile_x, r.tile_y) == DoorState::CLOSED)
        const_cast<GameMap*>(map)->set_door_state(r.tile_x, r.tile_y, DoorState::OPEN);
    LOG_INFO("[PLAYER-FIX] 口袋传送 → tile(%d,%d) room=%d",
             r.tile_x, r.tile_y, rooms ? rooms->room_at(r.tile_x, r.tile_y) : -1);
    return true;
}

// G14: 兜底拉怪 — 玩家到怪 BFS 不可达(孤岛怪/卡墙)时, 把最近存活怪拉到玩家
//       1-2 格环内可走 tile, 破除"怪杀不到 → 楼层永不清"死局. 仅卡死脱困调用.
static bool _teleport_monster_to_player(const Player* p,
    const std::vector<Monster*>& monsters, const GameMap* map) {
    if (!p || !map) return false;
    const Monster* t = nullptr;
    float bd = 1e9f;
    for (const Monster* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        float d = hypotf(m->entity.rect.x - p->entity.rect.x,
                         m->entity.rect.y - p->entity.rect.y);
        if (d < bd) { bd = d; t = m; }
    }
    if (!t) return false;
    int ptx = (int)(p->entity.rect.x + p->entity.rect.width/2) / 32;
    int pty = (int)(p->entity.rect.y + p->entity.rect.height/2) / 32;
    for (int r = 2; r >= 1; r--)
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dy) != r) continue;
                int nx = ptx + dx, ny = pty + dy;
                if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height) continue;
                Rectangle tr = { (float)(nx * 32), (float)(ny * 32), 32, 32 };
                // G14: 只排除 LOCKED/SEALED — 原 != NONE 会误跳过 OPEN/CLOSED 门,
                //      玩家在走廊/门节点时周围全被跳过 → 拉怪永败 (stuck_tp=0)
                DoorState nds = map->door_state_at(nx, ny);
                if (nds == DoorState::LOCKED || nds == DoorState::SEALED) continue;
                if (!map->is_rect_walkable(tr)) continue;
                Monster* mm = const_cast<Monster*>(t);
                mm->entity.position.x = (float)(nx * 32);
                mm->entity.position.y = (float)(ny * 32);
                mm->entity.sync_rect();
                LOG_INFO("[PLAYER-FIX] 兜底拉怪 → tile(%d,%d)", nx, ny);
                return true;
            }
    return false;
}

// Q3.2: Boss 蓄力判定 — 任一技能处于 windup 阶段即视为"即将出招"
static bool _boss_winding_up(const Monster* m) {
    const auto* bai = dynamic_cast<const BossAI*>(m->ai);
    if (!bai) return false;
    if (bai->_charge    && bai->_charge->windup_left    > 0.0f) return true;
    if (bai->_shockwave && bai->_shockwave->windup_left > 0.0f) return true;
    if (bai->_whirlwind && bai->_whirlwind->windup_left > 0.0f) return true;
    if (bai->_laser     && bai->_laser->windup_left     > 0.0f) return true;
    if (bai->_cone      && bai->_cone->windup_left      > 0.0f) return true;
    if (bai->_blink     && bai->_blink->windup_left     > 0.0f) return true;
    if (bai->_barrage   && bai->_barrage->windup_left   > 0.0f) return true;
    return false;
}

// Q3.2: tile 级 rect 碰撞判定 — BFS 与真实移动(rect)对齐, 防 tile可行走但玩家进不去导致的卡墙
// P1-C7: Sim 单 tile 通行性已统一到 GameMap::is_passable_sim (唯一真源, 含
//        LOCKED/SEALED 门阻断); 本函数只做 null 保护。旧的双轨实现
//        _sim_tile_passable 已删除。矩形级 is_rect_walkable 仍走 tile 标志位、
//        与门状态不联动, 该边界见 player_controller.cpp:194 注释 (有意为之)。
static bool _tile_rect_walkable(const GameMap* map, int tx, int ty) {
    return map ? map->is_passable_sim(tx, ty) : false;
}

// Q3.2: 危险视野 — 活性毒池/尖刺圈/木桶 (伤害圈 1.2 格 + 缓冲 = 1.5 格)
bool DecisionAgent::_is_hazard_near(float px, float py, const GameMap* map) const {
    if (!map) return false;
    // M4b: 熔岩地砖 (脚下 + 邻格缓冲)
    auto [tx, ty] = map->pixel_to_tile(px, py);
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (map->tile_at(tx + dx, ty + dy) == TileType::LAVA) return true;
    for (auto& ao : map->arena_objects) {
        if (!ao.active) continue;
        if (ao.type != ArenaObjectType::POISON_POOL &&
            ao.type != ArenaObjectType::SPIKE &&
            ao.type != ArenaObjectType::EXPLOSIVE_BARREL) continue;  // 收官: 木桶可爆炸
        float ax = ao.tile_x * 32.0f + 16.0f;
        float ay = ao.tile_y * 32.0f + 16.0f;
        if (hypotf(px - ax, py - ay) <= 1.5f * 32.0f) return true;
    }
    return false;
}

// Q3.2: 残血且无可用自愈 → 需要找泉水/祭坛回血
bool DecisionAgent::_needs_recovery(const Player* p) const {
    if (!p) return false;
    if (_hp_ratio(p) >= 0.50f) return false;
    for (auto& s : p->skills.active_skills)
        if (dynamic_cast<SelfHealSkill*>(s.get()) && s->can_use(_game_time))
            return false;
    // P1-A3: 背包有治疗药水也不算危急 (AI 已有 use_potion 决策路径)
    for (const auto& it : p->inventory.items) {
        const auto* c = dynamic_cast<const ConsumableItem*>(it.get());
        if (c && c->effect_type == "heal") return false;
    }
    return true;
}

// Q3.2: BFS 至最近未触发的特殊房 — 战斗间隙搜刮资源 (圣物/装备/泉水)
// P1-A3-fix2: heal_only 模式 — 危急回血时只找回血房 (FOUNTAIN/ALTAR/SHRINE),
// 放弃宝箱/商店等 — 血线告急时多进一个房 = 多一分被围死的风险
int DecisionAgent::_bfs_toward_room(const Player* p, const GameMap* map,
                                    bool heal_only) const {
    if (!map || !p) return -1;
    int w = map->width, h = map->height;
    auto [sx, sy] = map->pixel_to_tile(
        p->entity.rect.x + p->entity.rect.width/2,
        p->entity.rect.y + p->entity.rect.height/2);
    // Q3.13: 钳制玩家瓦片 — 否则 first[] 越界写堆损坏
    if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
    if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
    const int N = w * h;
    std::vector<char> is_target((size_t)N, 0);
    size_t pending = 0;
    for (auto& sr : map->special_rooms) {
        if (sr.triggered) continue;
        // Q3.13: 房间坐标越界保护 (数据驱动异常时不得写堆)
        if (sr.cx < 0 || sr.cx >= w || sr.cy < 0 || sr.cy >= h) continue;
        if (heal_only) {
            bool heals = sr.type == SpecialRoomType::FOUNTAIN
                      || sr.type == SpecialRoomType::ALTAR
                      || sr.type == SpecialRoomType::SHRINE;
            if (!heals) continue;
        }
        is_target[sr.cy * w + sr.cx] = 1;
        pending++;
    }
    if (pending == 0) return -1;
    std::vector<int> first((size_t)N, -2);
    std::queue<int> q;
    first[sy * w + sx] = -1;
    q.push(sy * w + sx);
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        int cx = cur % w, cy = cur / w;
        if (is_target[cur]) return (first[cur] >= 0) ? first[cur] : -1;
        for (int d = 0; d < 4; d++) {
            int nx = cx + kBfsDx[d], ny = cy + kBfsDy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int ni = ny * w + nx;
            if (first[ni] != -2 || !_tile_rect_walkable(map, nx, ny)) continue;
            first[ni] = (cur == sy * w + sx) ? d : first[cur];
            q.push(ni);
        }
    }
    return -1;
}

// P1-A2: BFS 至最近地面物品 (注入的 _ground 快照), 返回第一步方向 (0-3, -1=无/不可达)
// 骨架复用 _bfs_toward_room: is_target = 注入物品格; 剩血时药水格优先由调用方排序
int DecisionAgent::_bfs_toward_loot(const Player* p, const GameMap* map) const {
    if (!map || !p || _ground.empty()) return -1;
    int w = map->width, h = map->height;
    auto [sx, sy] = map->pixel_to_tile(
        p->entity.rect.x + p->entity.rect.width/2,
        p->entity.rect.y + p->entity.rect.height/2);
    if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
    if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
    const int N = w * h;
    std::vector<char> is_target((size_t)N, 0);
    size_t pending = 0;
    for (auto& g : _ground) {
        if (g.tile_x < 0 || g.tile_x >= w || g.tile_y < 0 || g.tile_y >= h) continue;
        is_target[g.tile_y * w + g.tile_x] = 1;
        pending++;
    }
    if (pending == 0) return -1;
    std::vector<int> first((size_t)N, -2);
    std::queue<int> q;
    first[sy * w + sx] = -1;
    q.push(sy * w + sx);
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        int cx = cur % w, cy = cur / w;
        if (is_target[cur]) return (first[cur] >= 0) ? first[cur] : -1;
        for (int d = 0; d < 4; d++) {
            int nx = cx + kBfsDx[d], ny = cy + kBfsDy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int ni = ny * w + nx;
            if (first[ni] != -2 || !_tile_rect_walkable(map, nx, ny)) continue;
            first[ni] = (cur == sy * w + sx) ? d : first[cur];
            q.push(ni);
        }
    }
    return -1;
}

// P1-A2: 站位附近 (2.5 格拾取半径内) 最近地面物品距离 (px), -1=无
float DecisionAgent::_near_loot_dist(const Player* p) const {
    if (!p || _ground.empty()) return -1.0f;
    float px = p->entity.rect.x + p->entity.rect.width / 2;
    float py = p->entity.rect.y + p->entity.rect.height / 2;
    float best = -1.0f;
    for (auto& g : _ground) {
        float lx = g.tile_x * 32.0f + 16.0f;
        float ly = g.tile_y * 32.0f + 16.0f;
        float d = hypotf(lx - px, ly - py);
        if (best < 0 || d < best) best = d;
    }
    return best;
}

// P1-C5: 最近武器掉落距离 (px), -1=无 — 空手时武器是 DPS 跃迁点
// (基线数据: 空手局 avg_floor 1.26 vs 持械 5-12; F1 死 91% 的放大器)
float DecisionAgent::_near_weapon_loot_dist(const Player* p) const {
    if (!p || _ground.empty()) return -1.0f;
    float px = p->entity.rect.x + p->entity.rect.width / 2;
    float py = p->entity.rect.y + p->entity.rect.height / 2;
    float best = -1.0f;
    for (auto& g : _ground) {
        if (!g.is_weapon) continue;
        float lx = g.tile_x * 32.0f + 16.0f;
        float ly = g.tile_y * 32.0f + 16.0f;
        float d = hypotf(lx - px, ly - py);
        if (best < 0 || d < best) best = d;
    }
    return best;
}

// P1-C3: BFS 至楼梯格 — stairs_active 后人必须站上楼梯才能按 E 下楼,
// 原 best_action 只返回 "descend" 不导航 → 站原地按 E 600s (探针 9/20 局)
int DecisionAgent::_bfs_to_stairs(const Player* p, const GameMap* map) const {
    if (!map || !p || _stairs_tx < 0) return -1;
    int w = map->width, h = map->height;
    if (_stairs_tx >= w || _stairs_ty >= h) return -1;
    auto [sx, sy] = map->pixel_to_tile(
        p->entity.rect.x + p->entity.rect.width/2,
        p->entity.rect.y + p->entity.rect.height/2);
    if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
    if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
    if (sx == _stairs_tx && sy == _stairs_ty) return -1;   // 已在格 → 调用方直接 descend
    const int N = w * h;
    std::vector<int> first((size_t)N, -2);
    std::queue<int> q;
    first[sy * w + sx] = -1;
    q.push(sy * w + sx);
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        int cx = cur % w, cy = cur / w;
        if (cx == _stairs_tx && cy == _stairs_ty)
            return (first[cur] >= 0) ? first[cur] : -1;
        for (int d = 0; d < 4; d++) {
            int nx = cx + kBfsDx[d], ny = cy + kBfsDy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int ni = ny * w + nx;
            if (first[ni] != -2 || !_tile_rect_walkable(map, nx, ny)) continue;
            first[ni] = (cur == sy * w + sx) ? d : first[cur];
            q.push(ni);
        }
    }
    return -1;
}

// Q3.2: BFS 寻路 — 从玩家所在格出发, 找最近可达的存活怪物, 返回第一步方向 (0-3, -1=不可达)
int DecisionAgent::_bfs_toward(const Player* p,
    const std::vector<Monster*>& monsters, const GameMap* map, bool avoid_hazard) const {
    if (!map || !p) return -1;
    int w = map->width, h = map->height;
    auto [sx, sy] = map->pixel_to_tile(
        p->entity.rect.x + p->entity.rect.width/2,
        p->entity.rect.y + p->entity.rect.height/2);
    // Q3.13: 钳制玩家瓦片 — 位置可能出图(边缘传送), 否则 first[] 越界写堆损坏
    if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
    if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
    const int N = w * h;
    std::vector<char> is_target((size_t)N, 0);
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        auto [tx, ty] = map->pixel_to_tile(
            m->entity.rect.x + m->entity.rect.width/2,
            m->entity.rect.y + m->entity.rect.height/2);
        // Q3.13: 越界怪跳过 — 击退/传送可使位置出图, 否则 is_target 越界写堆损坏
        if (tx < 0 || tx >= w || ty < 0 || ty >= h) continue;
        // G14: 怪 tile 不可走(卡墙/位置异常) → 标记其可走邻居为可达目标,
        //       否则 BFS 永远踏不上怪 tile → is_target 不命中 → 怪不可达 → 死局
        if (_tile_rect_walkable(map, tx, ty)) {
            is_target[ty * w + tx] = 1;
        } else {
            for (int d = 0; d < 4; d++) {
                int nx = tx + kBfsDx[d], ny = ty + kBfsDy[d];
                if (nx >= 0 && nx < w && ny >= 0 && ny < h &&
                    _tile_rect_walkable(map, nx, ny))
                    is_target[ny * w + nx] = 1;
            }
        }
    }
    std::vector<int> first((size_t)N, -2);  // 从起点出发的第一步方向, -1=起点, -2=未访问
    std::queue<int> q;
    first[sy * w + sx] = -1;
    q.push(sy * w + sx);
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        int cx = cur % w, cy = cur / w;
        if (is_target[cur]) return (first[cur] >= 0) ? first[cur] : -1;
        for (int d = 0; d < 4; d++) {
            int nx = cx + kBfsDx[d], ny = cy + kBfsDy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int ni = ny * w + nx;
            if (first[ni] != -2 || !_tile_rect_walkable(map, nx, ny)) continue;
            // Q3.2: 避开危险瓦片中心圈 (毒池/尖刺)
            if (avoid_hazard && _is_hazard_near(nx * 32.0f + 16.0f, ny * 32.0f + 16.0f, map)) continue;
            first[ni] = (cur == sy * w + sx) ? d : first[cur];
            q.push(ni);
        }
    }
    return -1;
}

// Q3.2: BFS 远离 — 从目标怪所在格 BFS 整图, 返回玩家 4 邻居中距怪最远的方向 (0-3, -1=全堵)
int DecisionAgent::_bfs_away(const Player* p, const Monster* t,
    const GameMap* map, bool avoid_hazard) const {
    if (!map || !p || !t) return -1;
    int w = map->width, h = map->height;
    auto [mx, my] = map->pixel_to_tile(
        t->entity.rect.x + t->entity.rect.width/2,
        t->entity.rect.y + t->entity.rect.height/2);
    auto [sx, sy] = map->pixel_to_tile(
        p->entity.rect.x + p->entity.rect.width/2,
        p->entity.rect.y + p->entity.rect.height/2);
    // Q3.13: 钳制怪物/玩家瓦片 — 否则 dist[] 越界写堆损坏
    if (mx < 0) mx = 0; else if (mx >= w) mx = w - 1;
    if (my < 0) my = 0; else if (my >= h) my = h - 1;
    if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
    if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
    const int N = w * h;
    std::vector<int> dist((size_t)N, -1);
    std::queue<int> q;
    dist[my * w + mx] = 0;
    q.push(my * w + mx);
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        int cx = cur % w, cy = cur / w;
        for (int d = 0; d < 4; d++) {
            int nx = cx + kBfsDx[d], ny = cy + kBfsDy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int ni = ny * w + nx;
            if (dist[ni] >= 0 || !_tile_rect_walkable(map, nx, ny)) continue;
            if (avoid_hazard && _is_hazard_near(nx * 32.0f + 16.0f, ny * 32.0f + 16.0f, map)) continue;
            dist[ni] = dist[cur] + 1;
            q.push(ni);
        }
    }
    int best = -1, best_dist = -1;
    for (int d = 0; d < 4; d++) {
        int nx = sx + kBfsDx[d], ny = sy + kBfsDy[d];
        if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
        if (!_tile_rect_walkable(map, nx, ny)) continue;
        if (avoid_hazard && _is_hazard_near(nx * 32.0f + 16.0f, ny * 32.0f + 16.0f, map)) continue;
        if (dist[ny * w + nx] > best_dist) { best_dist = dist[ny * w + nx]; best = d; }
    }
    return best;
}

// Q3.2: 轴贪心兜底 — BFS 无路时按主轴直行, 用真实rect校验, 每步避开毒池
int DecisionAgent::_greedy_step(const Player* p, const Monster* t,
                                const GameMap* map) const {
    if (!p || !t || !map) return -1;
    float px = p->entity.rect.x + p->entity.rect.width/2;
    float py = p->entity.rect.y + p->entity.rect.height/2;
    float tx = t->entity.rect.x + t->entity.rect.width/2;
    float ty = t->entity.rect.y + t->entity.rect.height/2;
    int dx = (tx > px) ? 3 : 2;
    int dy = (ty > py) ? 1 : 0;
    int cand[4] = {dx, dy, (dx == 2) ? 3 : 2, (dy == 0) ? 1 : 0};
    for (int i = 0; i < 4; i++) {
        float mdx = (cand[i] == 2) ? -32.0f : (cand[i] == 3) ? 32.0f : 0.0f;
        float mdy = (cand[i] == 0) ? -32.0f : (cand[i] == 1) ? 32.0f : 0.0f;
        Rectangle r = p->entity.rect;
        r.x += mdx; r.y += mdy;
        if (!map->is_rect_walkable(r)) continue;
        if (_is_hazard_near(r.x + r.width/2, r.y + r.height/2, map)) continue;
        return cand[i];
    }
    return -1;
}

float DecisionAgent::_evaluate_move(int dir, const Player* p,
    const std::vector<Monster*>& monsters, const GameMap* map) const {
    if (!p) return -999;
    float dx = (dir == 2) ? -1.0f : (dir == 3) ? 1.0f : 0.0f;
    float dy = (dir == 0) ? -1.0f : (dir == 1) ? 1.0f : 0.0f;
    // Check walkable (Q3.1: 用 rect 判定对齐真实移动, 避免 tile 级误判撞墙)
    Rectangle target_rect = p->entity.rect;
    target_rect.x += dx * 32.0f;
    target_rect.y += dy * 32.0f;
    if (map) {
        if (!map->is_rect_walkable(target_rect)) {
            // G14: CLOSED 门放行 — BFS(_tile_rect_walkable) 视为可走(Sim自动开),
            //       而 rect 级判定 is_walkable=false 令 AI 永不走门 → 开门逻辑
            //       (best_action CLOSED→pickup) 成死代码 → AI 困死房间.
            //       返回低分 0.05: 无更好选项时选门方向, best_action 触发 pickup 开门.
            auto [pcx, pcy] = map->pixel_to_tile(
                p->entity.rect.x + p->entity.rect.width/2,
                p->entity.rect.y + p->entity.rect.height/2);
            int ntx = pcx + (int)(dx * 1.0f), nty = pcy + (int)(dy * 1.0f);
            if (map->door_state_at(ntx, nty) == DoorState::CLOSED) return 0.05f;
            return -999; // 真正 blocked (墙/LOCKED/SEALED)
        }
    }

    float px = p->entity.rect.x + p->entity.rect.width/2;
    float py = p->entity.rect.y + p->entity.rect.height/2;
    // Q3.2: 落脚点进入毒池/尖刺圈 → 重罚 (Q3.10: -999→-1.0, 全图无安全路径时允许踩毒渡河)
    if (map && _is_hazard_near(target_rect.x + target_rect.width/2,
                               target_rect.y + target_rect.height/2, map))
        return -1.0f;

    auto* t = _find_nearest(p, monsters);
    if (!t) {
        // G15: 全图无怪 → 先搜 loot/房间, 否则打完怪就站桩, 尸体掉落全废
        //      → 永远空手 (50 局 avg_damage 42 vs weapon 全 fist_basic)
        if (map && !_ground.empty()) {
            float loot_d = _near_loot_dist(p);
            if (loot_d >= 0 && loot_d < 5.0f * 32.0f) {
                int ls = _bfs_toward_loot(p, map);
                if (ls >= 0) return (dir == ls) ? 0.7f : 0.0f;
            }
        }
        if (map) {
            int rs = _bfs_toward_room(p, map);
            if (rs >= 0) return (dir == rs) ? 0.6f : 0.0f;
        }
        sim_move_noenemy++; return 0.1f; // 全图无存活怪无资源 → 中性
    }

    float ex = t->entity.rect.x + t->entity.rect.width/2;
    float ey = t->entity.rect.y + t->entity.rect.height/2;
    float d = hypotf(ex - px, ey - py);

    // Q3.2: Boss 蓄力闪避 — 起手瞬间脱离 (1.4 > 攻击 1.0, 躲招优先于换血)
    if (t->is_boss && _boss_winding_up(t) && d < 220.0f) {
        int away = _bfs_away(p, t, map, true);
        if (away < 0) return 1.4f;  // 无安全路径 → 任意方向裸躲
        return (dir == away) ? 1.4f : 0.0f;
    }

    // Q3.2: 站在毒池里 → 任何安全方向优先逃离 (1.2 > 攻击上限 1.0)
    if (map && _is_hazard_near(px, py, map)) return 1.2f;

    // P1-A3: 危急回血 — 残血(<50%)且无自愈无药水时, 找泉水/祭坛优先于战斗 (1.3 > 攻击 1.0)
    // P1-A3-fix1: 无未触发房 (room_step<0) 时不得永续撤退 — 原实现 bfs_away 0.9 分
    // 持续压过攻击 → "只逃不打"死循环 (v3 冒烟: 19/20 局零输出, 怪追到墙角磨死).
    // P1-C4: 贴脸拉开分 0.9→0.6 — 0.9 曾压过攻击(0.67)使围殴局零输出全程逃命
    // (P1-C3 数据: 270 局 F1 围殴死 100% 零杀, avg 118s 仅 1.65dps 被追着咬).
    // 0.6 保留撤离意图但让贴脸攻击(d0≈0.67)反超 → "逃一步打一下" 轮换
    if (map && _needs_recovery(p) && t && !t->is_boss) {
        int room_step = _bfs_toward_room(p, map, true);   // P1-A3-fix2: 只找回血房
        if (room_step >= 0) { if (dir == room_step) sim_move_branch[0]++;
            return (dir == room_step) ? 1.3f : 0.0f; }
        // 无房可去 → 仅贴脸时拉开 (条件撤退, 血线安全或距离拉开即恢复战斗)
        if (d < 2.0f * 32.0f) {
            int away = _bfs_away(p, t, map, true);
            if (away >= 0) { if (dir == away) sim_move_branch[0]++;
                return (dir == away) ? 0.6f : 0.0f; }
        }
    }

    // P1-C5: 空手武器优先追击 — 空手是 F1 死亡放大器 (avg_floor 1.26 vs 持械 5-12),
    // 武器掉落 8 格内 0.9 分直奔 (压过普通拾取 0.7/搜刮 0.6; 近身怪 >3 格
    // 才去捡 — 不至于贴脸送死)。捡到武器后本分支自然失效 (不再空手)。
    // P1-C5: 空手武器追击 — 冒烟负回归 (af 6.25→2.15): F1 怪密度下 0.9 分
    // 穿怪奔武器 = 挨打送头. 保留 _near_weapon_loot_dist 供 P1-C6 重设计
    // (需带威胁回避的绕行路径而非直线追击).
    if (false && map && !_ground.empty() && _is_bare_fisted(p)) {
        float wloot_d = _near_weapon_loot_dist(p);
        if (wloot_d >= 0 && wloot_d < 8.0f * 32.0f && d > 3.0f * 32.0f) {
            int wloot_step = _bfs_toward_loot(p, map);
            if (wloot_step >= 0) return (dir == wloot_step) ? 0.9f : 0.0f;
        }
    }

    // P1-A2: 战斗间隙捡地面物品 — 比特殊房更近的直接资源, 优先级更高 (0.7 > 0.6)
    // G13: 间隙判定对齐真实射程 — 原 160px 硬编码, crossbow 射程 320px 时 AI 在
    //      5~10 格区间去捡破烂而非攻击 (loot 0.7 > 远程 attack 0.35)。
    float reach_px = std::max(p->weapon.current_range(), 1.5f) * 32.0f;
    if (d > reach_px + 32.0f && map && !_ground.empty()) {
        float loot_d = _near_loot_dist(p);
        // 只对 5 格内的近物品直奔; 更远的留给房间搜刮 (避免长途回头捡破烂)
        if (loot_d >= 0 && loot_d < 5.0f * 32.0f) {
            int loot_step = _bfs_toward_loot(p, map);
            if (loot_step >= 0) { if (dir == loot_step) sim_move_branch[1]++;
                return (dir == loot_step) ? 0.7f : 0.0f; }
        }
    }

    // Q3.2: 战斗间隙搜刮 — 最近怪超出射程时走向最近未触发特殊房 (圣物/装备/泉水)
    // 交战圈内(≤ideal)先打; rect级BFS保证路径真实可达, 不会卡墙
    if (d > reach_px + 32.0f && map) {
        int room_step = _bfs_toward_room(p, map);
        if (room_step >= 0) { if (dir == room_step) sim_move_branch[2]++;
            return (dir == room_step) ? 0.6f : 0.0f; }
    }

    // G13: ideal_dist 对齐真实射程 — 原硬编码 2.5/1.5 与 reach 脱节,
    //       crossbow 射程 10 格但 ideal 只 2.5 格, AI 在 2.5~10 格区间
    //       attack=0(旧 reach 48px 出圈) 且 move>0 → 追到 2.5 格停下干等。
    //       与 _evaluate_attack 的 reach_px 同源 (max(current_range,1.5)),
    //       保证 ideal_dist ≥ reach, 消除 attack=0/move=0 死区。
    float atk_range = std::max(p->weapon.current_range(), 1.5f);
    float ideal_dist = atk_range + _prefer_range * 2.0f;

    // Q3.2: 已到攻击圈内 → 站桩攻击/放技能, 不移动
    // Q3.15: 此处存在已知理论缺陷 — d ∈ (48px, ideal_dist] 区间 attack=0/move=0,
    // 远程 build 死区宽达 ~90px。曾尝试激活"拉开距离"分支消除死区, 实测 200 局
    // 胜率 10.0%→3.5%(风筝震荡破坏 Q3.12 数值平衡), 故回退保留站桩行为。
    // 后续若重调此段必须同步重跑 500 局平衡回归。
    // P1-A4-fix: 48px 判定线 = 攻击半径极限边缘 — AI 停在这里时怪挪 1px 即出圈,
    // attack 分归零 → 决策抖动 → 出手率暴跌 (探针: 60 冷却拦截 vs 3 命中).
    // 修复: 圈内不返回 0, 改为"贴脸步进" — 距离 >1 格时向最近怪靠近仍得 0.4 分,
    // 压过 0 分的站桩, 让 AI 站进 d≤32px 的稳定出手区. 只在近战时启用 (远程风筝已验证有害).
    if (d <= ideal_dist * 32.0f) {
        bool is_melee = (p->weapon.weapon_type() == WeaponType::FIST);
        if (is_melee && t && d > 32.0f) {
            int step = _bfs_toward(p, monsters, map, false);
            if (step >= 0) return (dir == step) ? 0.4f : 0.0f;
        }
        if (dir == 0) sim_move_branch[4]++;
        return 0;
    }

    // Q3.2: 太远 → BFS 寻路接近 (绕墙+绕毒, 无路时轴贪心兜底)
    int step = _bfs_toward(p, monsters, map, true);
    if (step < 0) step = _bfs_toward(p, monsters, map, false);
    if (step < 0) step = _greedy_step(p, t, map);
    if (step < 0) { sim_bfs_fail++; return 0.0f; }
    // P1-C7b: 统一稳定步 — 与 _stuck_escape 共用 _pick_stable_bfs_step (同 _mem_*),
    //          替代原 0.8 记忆块 (两套记忆交替写入互相污染 → stuck/常规切换震荡)
    step = _pick_stable_bfs_step(p, monsters, map, step);
    if (dir == step) sim_move_branch[3]++;
    return (dir == step) ? 0.6f : 0.0f;
}

float DecisionAgent::_evaluate_pickup(const Player* p, const GameMap* map,
    const std::vector<Monster*>& monsters) const {
    if (!map) return 0;
    // G14b: 拾取冷却 + 放弃 — 冷却中不评估; 累计 3 次"真实尝试后物品仍在"
    //        (在下方 loot 命中处 ++) 才放弃, 避免冷却期间每帧误累加 (旧 bug
    //        1.5s 内 streak 涨到 90 → 永久放弃 → picks=0 全程空手)
    if (_game_time - _last_pickup_attempt < 1.5f) return 0;
    if (_pickup_fail_streak >= 3) return 0;
    float threat = 0.0f;
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        float md = hypotf(m->entity.rect.x + m->entity.rect.width/2 - (p->entity.rect.x + p->entity.rect.width/2),
                          m->entity.rect.y + m->entity.rect.height/2 - (p->entity.rect.y + p->entity.rect.height/2));
        threat = std::max(threat, 1.0f - md / (6.0f * 32.0f));
    }
    // P1-A2: 地面物品拾取判定 (原 AI 对 ground_items 全盲 → picks=0 → 无药无武器)
    // 拾取半径与 InteractionHandler::pickup_item 对齐 (PICKUP_RANGE=2.0 * TILE_SIZE)
    float loot_d = _near_loot_dist(p);
    if (loot_d >= 0 && loot_d < 2.0f * 32.0f) {
        _pickup_fail_streak++;   // G14b: 真实尝试计数 (冷却已过仍见物品 = 上次失败)
        // P1-A2-fix: 被围殴 (threat>0.55, 怪 <2 格) 时拾取消分 → 还手保命
        float loot_w = (threat > 0.55f) ? 0.0f : 1.6f;
        if (loot_w > 0 && _hp_ratio(p) < 0.5f) loot_w *= 1.8f;  // 残血药水加权
        if (loot_w > 0) return loot_w * (1.0f - 0.6f * threat);
    }
    for (auto& sr : map->special_rooms) {
        if (sr.triggered) continue; // Q3.2: 已拾取房间不再给分 — 否则 loot 完站桩等死
        float dx = p->entity.rect.x + p->entity.rect.width/2 - (sr.cx * 32 + 16);
        float dy = p->entity.rect.y + p->entity.rect.height/2 - (sr.cy * 32 + 16);
        // Q3.1: 只有站在房间上(≤1格)才给分 — 且随最近威胁衰减, 被围殴时不得锁死拾取
        // Q3.2: 残血时给 2.0 保底分 (泉水满血优先), 威胁权重减半
        if (sqrtf(dx*dx+dy*dy) < 1.0f * 32.0f) {
            // Q3.1: 只有站在房间上(≤1格)才给分 — 且随最近威胁衰减, 被围殴时不得锁死拾取
            float threat_w = threat;
            float base_w  = 1.5f;
            return base_w * (1.0f - threat_w);
        }
    }
    return 0;
}

// G13: 四方向轮换脱困 — 走不了或有危险就转下一向
std::string DecisionAgent::_rotation_escape(const Player* p, const GameMap* map) const {
    if (_escape_dir < 0) { sim_stuck_rotations++; _escape_dir = 0; }
    const char* esc[] = {"move_up", "move_down", "move_left", "move_right"};
    // G14: 撞墙统计 — 当前方向不可走/有危险时自增 (可判断 AI 是否困死)
    float mdx = (_escape_dir == 2) ? -1.0f : (_escape_dir == 3) ? 1.0f : 0.0f;
    float mdy = (_escape_dir == 0) ? -1.0f : (_escape_dir == 1) ? 1.0f : 0.0f;
    Rectangle er = p->entity.rect;
    er.x += mdx * 32; er.y += mdy * 32;
    // G14: rect 级整步判定失败(玩家可停半格位, 连续移动与 ±32 整步脱节)
    //      时, 回退 tile 级判定(_tile_rect_walkable, 与 BFS 同源) —
    //      否则 AI 永远认为 4 向全堵, 卡死在开放式空间 (stuck# open=4 door=0)
    if (!map->is_rect_walkable(er) ||
        _is_hazard_near(er.x + er.width / 2, er.y + er.height / 2, map)) {
        auto [pcx, pcy] = map->pixel_to_tile(
            p->entity.rect.x + p->entity.rect.width/2,
            p->entity.rect.y + p->entity.rect.height/2);
        int ntx = pcx + (int)mdx, nty = pcy + (int)mdy;
        sim_rot_blocked++;
        if (map->door_state_at(ntx, nty) == DoorState::CLOSED)
            return "pickup";   // G14: CLOSED 门 → E 键开门
        if (!_is_hazard_near(ntx * 32.0f + 16.0f, nty * 32.0f + 16.0f, map) &&
            _tile_rect_walkable(map, ntx, nty))
            return esc[_escape_dir];   // tile 级可走 → 放行 (执行层二分贴墙)
        _escape_dir = (_escape_dir + 1) % 4;
    }
    return esc[_escape_dir];
}

// G14b: 卡死进展信号 — 玩家移动>2格 / 怪HP总和变化(玩家输出或环境衰减) /
//       玩家HP变化 / 怪死亡, 任一为真即非卡死. 容差 0.01 过滤浮点等值噪声.
bool DecisionAgent::_stuck_progress(const Player* p,
    const std::vector<Monster*>& monsters, int tx, int ty) const {
    float php = (float)p->combat.current_hp;
    float mon = 0;
    int alive = 0;
    for (const Monster* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        mon += m->combat.current_hp; alive++;
    }
    bool progressed = hypotf((float)(tx - (int)_last_px), (float)(ty - (int)_last_py)) > 2.0f
                   || fabsf(mon - _last_mon_sum) > 0.01f
                   || fabsf(php - _last_hp_sum) > 0.01f
                   || alive != _last_alive_count;
    if (progressed && _stuck_since >= 0)
        _stuck_total += std::max(0.0f, (float)_game_time - _stuck_since);
    if (progressed) { _stuck_since = -1; _escape_dir = -1;
                      _last_px = (float)tx; _last_py = (float)ty; }
    _last_hp_sum = php; _last_mon_sum = mon; _last_alive_count = alive;
    return progressed;
}

// G14b: 卡死 ≥8s 兜底 — 传玩家到怪旁/反向拉怪/强开 3x3 CLOSED 门; 成功返回 "none"
std::string DecisionAgent::_stuck_tp_or_pull(const Player* p,
    const std::vector<Monster*>& monsters, const GameMap* map,
    int tx, int ty, float stuck_for) const {
    if (stuck_for <= 8.0f || (float)_game_time - _last_teleport_try < kTeleportCooldown)
        return "";
    _last_teleport_try = (float)_game_time;   // ≥8s → 兜底传送
    sim_tp_attempts++;
    auto* pm = const_cast<Player*>(p);
    if (_teleport_player_to_nearest(pm, monsters, map, _rooms)) {
        sim_stuck_teleports++;
        _stuck_since = -1; _escape_dir = -1; _last_px = -999; _last_py = -999;
        return "none";
    }
    // G14: 玩家传送失败(怪不可达) → 反向拉怪到玩家旁, 破除孤岛怪死局
    if (_teleport_monster_to_player(p, monsters, map)) {
        sim_stuck_teleports++;
        _stuck_since = -1; _escape_dir = -1; _last_px = -999; _last_py = -999;
        return "none";
    }
    // G14: 强开周围 3x3 CLOSED 门 — 玩家半格卡位使 try_open_door_toward 查错格
    auto* gm = const_cast<GameMap*>(map);
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int nx = tx + dx, ny = ty + dy;
            if (gm->door_state_at(nx, ny) == DoorState::CLOSED)
                gm->set_door_state(nx, ny, DoorState::OPEN);
        }
    return "";
}

// G13: 卡死脱困编排 — 返回脱困动作; "" = 未进入脱困 (交给常规评分)
std::string DecisionAgent::_stuck_escape(const Player* p,
    const std::vector<Monster*>& monsters, const GameMap* map) const {
    if (!p || !map) return "";
    int tx = (int)(p->entity.rect.x + p->entity.rect.width / 2) / 32;
    int ty = (int)(p->entity.rect.y + p->entity.rect.height / 2) / 32;
    _stuck_progress(p, monsters, tx, ty);   // 更新卡死状态
    if (tx != (int)_last_px || ty != (int)_last_py) _escape_dir = -1;
    // G14: 近身战豁免 — 2 格内有怪 = 真实战斗 (围殴/拉锯), 卡死机制让位
    if (_count_in_range(p, monsters, 2.0f * 32.0f) > 0) {
        _stuck_since = -1; _escape_dir = -1;
        return "";
    }
    if (_stuck_since < 0) {   // 锚定徘徊中心, 开始计时
        _stuck_since = (float)_game_time;
        _last_px = (float)tx; _last_py = (float)ty;
        return "";
    }
    float stuck_for = std::max(0.0f, (float)_game_time - _stuck_since);
    if (_stuck_total + stuck_for > kStuckTotalBudget) {   // 累计卡死超预算
        sim_stuck_watchdog = 1;
        LOG_INFO("[SIM] 卡死看门狗: 累计 %.0fs → 强制结算", _stuck_total + stuck_for);
        return "none";
    }
    std::string tp = _stuck_tp_or_pull(p, monsters, map, tx, ty, stuck_for);
    if (!tp.empty()) return tp;
    if (stuck_for <= 2.0f) return "";
    _stuck_sample(p, monsters, map, tx, ty);
    // G14: 怪就在 1.5 格内还卡死(被挤/地形) → 直接攻击
    if (_count_in_range(p, monsters, 1.5f * 32.0f) > 0) {
        sim_stuck_bfs_hit++;
        return "attack";
    }
    // G14: 卡死时优先 BFS 朝怪定向移动 — 原盲目旋转只让 AI 原地打转
    int bstep = _bfs_toward(p, monsters, map, true);
    if (bstep >= 0) {
        sim_stuck_bfs_hit++;
        bstep = _pick_stable_bfs_step(p, monsters, map, bstep);
        const char* dl[] = {"move_up", "move_down", "move_left", "move_right"};
        return dl[bstep];
    }
    sim_stuck_bfs_fail++;
    return _rotation_escape(p, map);
}

// G14: 卡死采样诊断 — AI tile + 最近怪距离 + 存活怪数 + 邻居统计 (环形缓冲 64)
void DecisionAgent::_stuck_sample(const Player* p,
    const std::vector<Monster*>& monsters, const GameMap* map,
    int tx, int ty) const {
    auto* nm = _find_nearest(p, monsters);
    int sIdx = sim_stuck_sample_count % 64;
    int sAlive = 0;
    for (const Monster* m : monsters) if (m && m->combat.is_alive) sAlive++;
    sim_stuck_sample[sIdx][0] = tx;
    sim_stuck_sample[sIdx][1] = ty;
    sim_stuck_sample[sIdx][2] = nm ? (int)hypotf(
        nm->entity.rect.x + nm->entity.rect.width/2 - (p->entity.rect.x + p->entity.rect.width/2),
        nm->entity.rect.y + nm->entity.rect.height/2 - (p->entity.rect.y + p->entity.rect.height/2)) : -1;
    sim_stuck_sample[sIdx][3] = sAlive;
    sim_stuck_sample[sIdx][5] = _is_hazard_near(p->entity.rect.x + p->entity.rect.width/2,
        p->entity.rect.y + p->entity.rect.height/2, map) ? 1 : 0;
    int stats[4];
    _stuck_neighbor_stats(map, nm, tx, ty, stats);
    sim_stuck_sample[sIdx][4] = stats[0];   // AI 4邻可走
    sim_stuck_sample[sIdx][6] = stats[1];   // AI 4邻DOOR
    sim_stuck_sample[sIdx][11] = stats[2];  // 全图 LOCKED
    sim_stuck_sample[sIdx][12] = stats[3];  // 怪 4邻可走
    sim_stuck_sample[sIdx][7] = nm ? (int)((nm->entity.rect.x + nm->entity.rect.width/2) / 32) : -1;
    sim_stuck_sample[sIdx][8] = nm ? (int)((nm->entity.rect.y + nm->entity.rect.height/2) / 32) : -1;
    sim_stuck_sample[sIdx][9] = _rooms ? _rooms->room_at(tx, ty) : -1;
    sim_stuck_sample[sIdx][10] = nm && _rooms
        ? _rooms->room_at((int)((nm->entity.rect.x + nm->entity.rect.width/2) / 32),
                          (int)((nm->entity.rect.y + nm->entity.rect.height/2) / 32)) : -1;
    sim_stuck_sample_count++;
}

// G14b: 卡死邻居统计 — [4邻可走, 4邻DOOR, 全图LOCKED, 怪4邻可走]
void DecisionAgent::_stuck_neighbor_stats(const GameMap* map, const Monster* nm,
    int tx, int ty, int out[4]) const {
    int sOpen = 0, sDoor = 0;
    for (int dd = 0; dd < 4; dd++) {
        int nx = tx + (dd == 2 ? -1 : dd == 3 ? 1 : 0);
        int ny = ty + (dd == 0 ? -1 : dd == 1 ? 1 : 0);
        if (_tile_rect_walkable(map, nx, ny)) sOpen++;
        if (map->door_state_at(nx, ny) != DoorState::NONE) sDoor++;
    }
    int sLocked = 0;
    for (int i = 0; i < map->width; i++)
        for (int j = 0; j < map->height; j++)
            if (map->door_state_at(i, j) == DoorState::LOCKED) sLocked++;
    int mOpen = -1;
    if (nm) {
        int mtx2 = (int)((nm->entity.rect.x + nm->entity.rect.width/2) / 32);
        int mty2 = (int)((nm->entity.rect.y + nm->entity.rect.height/2) / 32);
        mOpen = 0;
        for (int dd = 0; dd < 4; dd++)
            if (_tile_rect_walkable(map, mtx2 + (dd == 2 ? -1 : dd == 3 ? 1 : 0),
                                    mty2 + (dd == 0 ? -1 : dd == 1 ? 1 : 0))) mOpen++;
    }
    out[0] = sOpen; out[1] = sDoor; out[2] = sLocked; out[3] = mOpen;
}

// G14b: 卡死 BFS 步稳定化 — 路径记忆迟滞: 目标未变且上次方向仍缩短距离时,
//       沿用 _mem_step, 消除 BFS 等权震荡 (玩家在 2 个等距格间往返不止).
int DecisionAgent::_pick_stable_bfs_step(const Player* p,
    const std::vector<Monster*>& monsters, const GameMap* map, int bstep) const {
    auto* mem_t = _find_nearest(p, monsters);
    if (!mem_t || _mem_target != mem_t->instance_id || _mem_step < 0) {
        _mem_target = mem_t ? mem_t->instance_id : 0;
        _mem_step = bstep;
        return bstep;
    }
    float ex = mem_t->entity.rect.x + mem_t->entity.rect.width/2;
    float ey = mem_t->entity.rect.y + mem_t->entity.rect.height/2;
    float px2 = p->entity.rect.x + p->entity.rect.width/2;
    float py2 = p->entity.rect.y + p->entity.rect.height/2;
    float mdx = (_mem_step == 2) ? -32.0f : (_mem_step == 3) ? 32.0f : 0.0f;
    float mdy = (_mem_step == 0) ? -32.0f : (_mem_step == 1) ? 32.0f : 0.0f;
    Rectangle mr = p->entity.rect;
    mr.x += mdx; mr.y += mdy;
    float nd = hypotf(ex - (px2 + mdx), ey - (py2 + mdy));
    // G14b: 等距也保持记忆 (nd <= d+ε) — 对称双路径不震荡
    if (map->is_rect_walkable(mr) && nd <= hypotf(ex - px2, ey - py2) + 0.01f) {
        return _mem_step;
    }
    _mem_step = bstep;
    return bstep;
}

// ═══════════════════════════════════════════════════════════
//  G7.4: Best action selection (evaluate → pick max)
// ═══════════════════════════════════════════════════════════

std::string DecisionAgent::best_action(const Player* player,
    const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player) return "";

    if (boss_intro_active) return "confirm";
    if (stairs_active) {
        sim_stairs[0]++;
        // Q3.2: 清层后先搜刮未触发特殊房 (原逻辑直接下楼 → 整层资源全丢)
        // P1-C3-fix: 层级搜刮预算 15s — 超时放弃本层余下搜刮直奔楼梯.
        // 全量数据: 无限搜刮 → TWall 16.8% 爬不完; 15s 预算 → TWall 7.8%.
        // (25s 实验更差: 混沌重排下不单调 — 以结构指标定参: 死锁根除+节奏资源平衡)
        if (_stairs_since < 0) _stairs_since = (float)_game_time;
        bool budget_ok = (float)_game_time - _stairs_since < 15.0f;
        bool can_move = false;
        std::string move_act;
        if (map && !_loot_abandoned && budget_ok) {
            for (auto& sr : map->special_rooms) {
                if (sr.triggered) continue;
                float ddx = player->entity.rect.x + player->entity.rect.width/2 - (sr.cx * 32 + 16);
                float ddy = player->entity.rect.y + player->entity.rect.height/2 - (sr.cy * 32 + 16);
                if (sqrtf(ddx*ddx + ddy*ddy) < 1.0f * 32.0f) return "pickup";
            }
            int rs = _bfs_toward_room(player, map);
            if (rs >= 0) {
                const char* dl[] = {"move_up","move_down","move_left","move_right"};
                float mdxs[4] = {0,0,-32,32}, mdys[4] = {-32,32,0,0};
                for (int try_d = 0; try_d < 4; try_d++) {
                    int d2 = (try_d == 0) ? rs : (rs + try_d) % 4;
                    Rectangle er = player->entity.rect;
                    er.x += mdxs[d2]; er.y += mdys[d2];
                    if (map->is_rect_walkable(er)) { can_move = true; move_act = dl[d2]; break; }
                }
            }
        }
        if (can_move) {
            // Q3.2-fix: 搜刮被半格偏移卡死 → 原地 ≥2s 放弃搜刮直奔楼梯
            int ct0 = (int)(player->entity.rect.x + player->entity.rect.width / 2) / 32;
            int ct1 = (int)(player->entity.rect.y + player->entity.rect.height / 2) / 32;
            if (abs(ct0 - _loot_last_tx) + abs(ct1 - _loot_last_ty) >= 2) {
                _loot_last_tx = ct0; _loot_last_ty = ct1; _loot_stuck_since = -1;
            } else if (_loot_stuck_since < 0) {
                _loot_stuck_since = (float)_game_time;
            } else if ((float)_game_time - _loot_stuck_since > 2.0f) {
                sim_stuck_loot_wd++;   // M2-C: 搜刮看门狗强制下楼计数
                _loot_abandoned = true;  // P1-C3: 锁定放弃, 走楼梯不再回头
            }
            if (!_loot_abandoned) { sim_stairs[2]++; return move_act; }
        }
        // P1-C3: 搜刮完毕/放弃 → 人必须先走到楼梯格 (原直接 "descend" 但
        // _check_floor_transition 只认"站在楼梯上按 E" → 站原地按 E 600s)
        // 在格 → descend; 有路 → 迈向楼梯的第一步; 无路 → descend 碰运气
        {
            int sstep = _bfs_to_stairs(player, map);
            if (sstep >= 0) {
                const char* dl[] = {"move_up","move_down","move_left","move_right"};
                sim_stairs[2]++;
                return dl[sstep];
            }
        }
        sim_stairs[1]++;
        return "descend";
    }

    // ── G8.3: MCTS path (combat-only, enemies present) ──
    if (g_use_mcts && !monsters.empty()) {
        auto sim = build_sim_state(player, monsters, _game_time);
        if (sim.alive_monsters() > 0) {
            mcts::MCTS mcts(g_mcts_iters);
            mcts::CombatAction ca = mcts.search(sim);
            return mcts::action_name(ca);
        }
    }

    // Q3.2: 卡死逃脱 — 原地 ≥2s 且无近距怪 → 直线脱困 (先于一切评分)
    // Q3.10: 仅怪物血量总和变动视为战斗 (毒/环境只影响自身HP, 不得掩盖卡死)
    // P0-M2: 锚点半径卡死判定 — 原同 tile 判定被"两 tile 来回震荡"永久重置
    // (BFS 路径记忆在墙前 L/R 横跳 896s 不触发传送 → 900s 死局)
    // G13: 抽为 _stuck_escape — 原实现在此 return "none" 且无条件不重置计时,
    //      传送一旦失败即永久空转, 烧完 36000 帧 → TIMEOUT_WALL 假结果
    std::string stuck_action = _stuck_escape(player, monsters, map);
    if (!stuck_action.empty()) return stuck_action;

    float best_score = 0;
    std::string best = "";

    // Attack
    float atk_score = _evaluate_attack(player, monsters);
    if (atk_score > best_score) { best_score = atk_score; best = "attack"; }

    // Skills (priority-ordered)
    for (int si = 0; si < 4; si++) {
        int slot = _skill_priority[si];
        float sk_score = _evaluate_skill(slot, player, monsters);
        if (sk_score > best_score) {
            best_score = sk_score;
            best = "skill_" + std::to_string(slot + 1);
        }
    }

    // Heal decision (G7.4: HP below threshold → prioritize heal)
    if (_hp_ratio(player) < _prefer_heal) {
        for (int si = 0; si < 4; si++) {
            int slot = _skill_priority[si];
            // Q3.1: 只有真·治疗槽才触发 — 火系玩家初始无自愈时不得锁死 skill_3
            // Q3.2: 冷却中不得锁死 — 否则站桩等CD被环境伤害磨死
            if (slot < (int)player->skills.active_skills.size() &&
                dynamic_cast<SelfHealSkill*>(player->skills.active_skills[slot].get()) &&
                player->skills.active_skills[slot]->can_use(_game_time)) {
                best_score = 3.0f; // override other actions
                best = "skill_" + std::to_string(slot + 1);
                break; // Q3.15 (P1-2 fix): _skill_priority 已按优先序排列, 首个可用即最优 —
                       // 原 continue 遍历使最低优先级槽反向覆盖
            }
        }
    }

    // Q3.3: 药水 — 残血且本帧无可发自愈技能 → 喝治疗药水 (1s CD 防连灌)
    // M4.4: Boss战阈值 0.35→0.55 (冻结 1.5s 后血量会被秒杀, 必须提前喝)
    // M4.4: Boss蓄力瞬间不喝 — 优先 _evaluate_move 的躲招 (1.4 分 > 药水收益)
    // P1-C2: 中毒时阈值 0.35→0.55 — 毒 tick 3-6/0.5s, 35% 线才喝必然被追上
    // (基线 33.6% DOT 死亡: 喝 30HP 的同时毒继续吃血, 提前 20% 喝才有净回血)
    float potion_line = _prefer_heal;
    auto* boss = _find_nearest(player, monsters);
    if (boss && boss->is_boss) potion_line = 0.55f;
    else if (_player_poison_stacks(player) > 0) potion_line = 0.55f;
    if ((best.empty() || best[0] != 's') && _hp_ratio(player) < potion_line &&
        _game_time - _last_potion_time > 1.0f &&
        !(boss && boss->is_boss && _boss_winding_up(boss))) {
        for (const auto& it : player->inventory.items) {
            const auto* c = dynamic_cast<const ConsumableItem*>(it.get());
            if (c && c->effect_type == "heal") {
                _last_potion_time = _game_time;
                return "use_potion";
            }
        }
    }

    // Pickup
    float pu_score = _evaluate_pickup(player, map, monsters);
    if (pu_score > best_score) {
        best_score = pu_score; best = "pickup";
        _last_pickup_attempt = (float)_game_time;   // G14b: 记录拾取尝试时刻
    }

    // Movement (pick best direction)
    float move_scores[4];
    for (int d = 0; d < 4; d++)
        move_scores[d] = _evaluate_move(d, player, monsters, map);
    int best_dir = 0;
    for (int d = 1; d < 4; d++)
        if (move_scores[d] > move_scores[best_dir]) best_dir = d;
    // Q3.2: 方向迟滞 — 当前方向仍接近最优(±0.03)时保持, 避免对角逼近时逐帧翻转抖动
    if (_current_dir >= 0 && move_scores[_current_dir] >= move_scores[best_dir] - 0.03f)
        best_dir = _current_dir;
    if (move_scores[best_dir] > best_score) {
        const char* dirs[] = {"move_up","move_down","move_left","move_right"};
        best = dirs[best_dir];
        _current_dir = best_dir;
        _mem_step = best_dir;
        auto* mem_t = _find_nearest(player, monsters);
        _mem_target = mem_t ? mem_t->instance_id : 0;
        // Sim: 目标 tile 是 CLOSED 门 → 先开门再走
        if (map && best != "pickup") {
            float mdx[] = {0, 0, -1, 1}, mdy[] = {-1, 1, 0, 0};
            auto [cx, cy] = map->pixel_to_tile(
                player->entity.rect.x + player->entity.rect.width/2,
                player->entity.rect.y + player->entity.rect.height/2);
            int ntx = cx + (int)mdx[best_dir], nty = cy + (int)mdy[best_dir];
            if (map->door_state_at(ntx, nty) == DoorState::CLOSED)
                best = "pickup";
        }
    }

    // If nothing better — move randomly
    if (best.empty()) {
        const char* rand_dirs[] = {"move_up","move_down","move_left","move_right"};
        best = rand_dirs[rng() % 4];
    }
    return best;
}

// ═══════════════════════════════════════════════════════════
//  G7.4: Event decision
// ═══════════════════════════════════════════════════════════

bool DecisionAgent::accept_event(float risk_pct, const std::string& effect_desc,
                                  const Player* player) const {
    if (!player) return false;
    float hp = _hp_ratio(player);

    // Never suicide
    if (risk_pct > 0.40f && hp < 0.50f) return false;
    if (risk_pct > 0.25f && hp < 0.30f) return false;
    if (risk_pct > 0.10f && hp < 0.15f) return false;

    // High-value effects worth risking for
    bool high_value = (effect_desc.find("relic") != std::string::npos) ||
                      (effect_desc.find("skill_level") != std::string::npos) ||
                      (effect_desc.find("legendary") != std::string::npos);

    if (high_value && hp > 0.60f) return true;
    if (risk_pct == 0) return true;  // no risk → always accept

    // Moderate risk: accept if HP > risk*2 + buffer
    return hp > risk_pct * 2.0f + 0.25f;
}

// ═══════════════════════════════════════════════════════════
//  Backward compat: is_action_just_pressed gate
// ═══════════════════════════════════════════════════════════

bool DecisionAgent::is_action_just_pressed(const char* action_name,
    const Player* player, const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player) return false;

    // Q3.1: 同帧内所有动作名查询共享同一次 best_action 结果
    if (_cached_frame != _frame) {
        _cached_best = best_action(player, monsters, map, stairs_active, boss_intro_active);
        _cached_frame = _frame;
        sim_action_counts[_action_bucket(_cached_best)]++;   // G13: 动作分布诊断
    }
    return !_cached_best.empty() && _cached_best == action_name;
}

// ═══════════════════════════════════════════════════════════
//  Helpers (unchanged from G5.6)
// ═══════════════════════════════════════════════════════════

Monster* DecisionAgent::_find_nearest(const Player* player,
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

int DecisionAgent::_count_in_range(const Player* player,
    const std::vector<Monster*>& monsters, float range_px) const {
    int n = 0;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        float d = hypotf(m->entity.rect.x + m->entity.rect.width/2 - px,
                         m->entity.rect.y + m->entity.rect.height/2 - py);
        if (d < range_px) n++;
    }
    return n;
}

float DecisionAgent::_hp_ratio(const Player* p) const {
    if (!p || p->combat.max_hp <= 0) return 0;
    return (float)p->combat.current_hp / (float)p->combat.max_hp;
}

void DecisionAgent::_pick_direction(const Player* player,
    const std::vector<Monster*>& monsters) {
    Monster* t = _find_nearest(player, monsters);
    if (t) {
        float dx = t->entity.rect.x + t->entity.rect.width/2 -
                   (player->entity.rect.x + player->entity.rect.width/2);
        float dy = t->entity.rect.y + t->entity.rect.height/2 -
                   (player->entity.rect.y + player->entity.rect.height/2);
        if (fabsf(dx) > fabsf(dy))
            _current_dir = (dx > 0) ? 3 : 2;
        else
            _current_dir = (dy > 0) ? 1 : 0;
    } else {
        _current_dir = rng() % 4;
    }
}
