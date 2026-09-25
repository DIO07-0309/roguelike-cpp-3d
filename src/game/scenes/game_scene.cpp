#include "game_scene.h"
#include "game/animation/player_avatar.h"   // A5: 玩家骨骼形象 (渲染路径懒建)
#include "systems/hit_stop.h"               // A6-T2: HitStop 击杀顿帧
#include "data/camera_defs.h"               // A6-T1: 摄像机语言数据加载
#include "title_scene.h"
#include "death_scene.h"
#include "victory_scene.h"
#include "config.h"
#include "boss.h"
#include "skill.h"
#include "combat_system.h"
#include "combat_feel.h"   // G10.4-B Fix2: DMG_FLOAT_SCALE_CRIT
#include "dungeon_generator.h"
#include "world/biome.h"   // M4f: get_biome_for_floor
#include "scene_tree.h"
#include "input_map.h"
#include "core/logger.h"
#include "save/save_manager.h"
#include "audio_server.h"
#include "event_bus.h"     // Batch 2C: ROOM_LOCKED/ROOM_CLEAR
#include "floor_config.h"
#include "attack_evolution.h"   // G1
#include "skill_evolution.h"   // G1 Step3
#include "rule_chain.h"         // G1 Step4
#include "data/boss_defs.h"     // G1 Step6
#include "core/replay/state_hash.h"  // G4.5
#include "systems/weapon_executor.h"  // G9.1
#include "systems/first_hint.h"       // G10.8-B4: 首遇提示
#include "vfx_server.h"              // G9: spear lightning VFX
#include "data/weapon_defs.h"        // G9: Boss drop
#include "data/element_defs.h"       // G10: element select screen
#include "systems/collision_utils.h"
#include "core/sim/sim_ai.h"         // G5.6
#include "core/sim/sim_runner.h"     // G5.6
#include "ai/agents/bt_agent.h"      // G8.1
#include "event_system.h"
#include "event_bus.h"
#include "service_locator.h"
#include "ai/player_behavior/player_behavior_recorder.h" // F15.2
#include "ai/player_behavior/player_behavior_analyzer.h"  // F15.5 mirror UI
#include "ai/mirror/mirror_agent.h"                       // F15.5
#include "game/rendering/mirror_hud_panel.h"              // v1.6-B1.1
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <cstdio>
#include <cstring>
#include "resource_manager.h"                 // M4f: NPC 精灵加载
#include "game/rendering/sprite_renderer.h"   // M4f: NPC 精灵绘制
#include "game/rendering/door_renderer.h"     // Door sprites + anim
#include "game/rendering3d/hd2d_renderer.h"   // M6-HD2D: 3D 表现层切片

// 字体指针 (在 main.cpp 中初始化)
extern Font g_font;
extern Font g_font_small;
extern bool g_font_loaded;
extern int  g_goto_floor;                    // v1.5.0-P0: scene_tree.cpp 定义

// ═══ G4.5: Replay static config ═══
std::string GameScene::g_record_path;
std::string GameScene::g_replay_path;
bool GameScene::g_record_mode = false;
bool GameScene::g_replay_mode = false;
bool GameScene::g_sim_mode = false;
int  GameScene::g_sim_runs = 100;
bool GameScene::g_show_mirror_acc = false;   // 验收: F9 toggle MIRROR AI 统计
bool GameScene::g_sim_all_builds = false;
int  GameScene::g_sim_build_type = 0;
std::string GameScene::g_sim_ai_type = "decision";
int  GameScene::g_rl_test_episodes = 0;
int  GameScene::g_rl_train_episodes = 0;
int  GameScene::g_rl_mirror_episodes = 0;  // F15.4

// G8.1: defined here so unique_ptr<DecisionAgent/BTAgent> can be destroyed
// (complete type available via includes in this .cpp)
GameScene::GameScene() = default;
GameScene::~GameScene() {
    // 死亡/切场景后 EventBus 悬挂订阅 → 继续游戏 FLOOR_ENTER 崩溃
    _boss.unregister_events();
    _gameplay.unregister_events();
    _presentation.unregister_events();
}

// ============================================================
// C1: 体验打磨 — 伤害数字/震动/冻结 辅助函数
// ============================================================
// _dmg_color_for moved to PresentationSystemDirector (dmg_color_for)
static Color _olddmg_color_for(int dmg, bool is_magic, bool is_poison) {
    if (is_poison) return {40, 220, 80, 255};
    if (is_magic)  return {160, 120, 255, 255};
    if (dmg >= 50) return {255, 220, 50, 255};  // 大数字偏黄
    return {255, 255, 255, 255};
}

// D2: 弹体命中判定半径 (宽容判定, 大于玩家碰撞盒)
constexpr float kProjectileHitRadius = 16.0f;

// 收官: 木桶参数 (引信 / 爆炸半径, 伤害 3×arena_scale 与尖刺同级)
constexpr float kBarrelFuse = 0.6f;
constexpr float kBarrelRadius = 2.0f * TILE_SIZE;

// D2: 弹道预警预览线 — 沿飞行方向绘制, 遇墙截断
static void _draw_projectile_preview(float sx, float sy, const Projectile& p,
                                     Color wc, float fade, float pulse,
                                     const GameMap* map) {
    float speed = sqrtf(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
    if (speed < 1.0f) return;
    float ux = p.vel.x / speed, uy = p.vel.y / speed;
    float max_len = speed * p.lifetime;
    float len = max_len;
    if (map) {
        for (float d = TILE_SIZE; d < max_len; d += TILE_SIZE) {
            auto [tx, ty] = map->pixel_to_tile(p.pos.x + ux * d, p.pos.y + uy * d);
            if (!map->is_walkable(tx, ty)) { len = d; break; }
        }
    }
    float ex = sx + ux * len, ey = sy + uy * len;
    unsigned char alpha = (unsigned char)((float)wc.a * fade);
    Color lc = {wc.r, wc.g, wc.b, alpha};
    DrawLineEx({sx, sy}, {ex, ey}, 2.0f * pulse, lc);
    DrawCircle(ex, ey, 4.0f * pulse, lc);
}

// _trigger_shake / _trigger_freeze moved to PresentationSystemDirector

// ============================================================
// D4 Step2: Event Presentation impl
// ============================================================


void GameScene::_start_event_presentation(EventType et) { _interaction.start_event_presentation(et); }
void GameScene::_tick_event_ui(float dt)              { _interaction.tick_event_ui(dt); }

void GameScene::_draw_event_ui(int sw, int sh) { _interaction.draw_event_ui(sw, sh); }

// ============================================================
// GameScene 实现
// ============================================================
void GameScene::_ready() {
    // D6: 场景入树时绑定Director
    _boss.init_events();
    _gameplay.init_events();
    _presentation.init_events();
    _flow.bind(this);
    _player_ctrl.bind(this);

    // G1: 注册 AttackEvolutionManager EventBus 监听
    AttackEvolutionManager::register_listener();
    SkillEvolutionManager::register_listener();  // G1 Step3
    RuleChainManager::register_listener();        // G1 Step4

    // D7 Step6: 注册场景级服务
    ServiceLocator::provide(&_boss);
    ServiceLocator::provide(&_gameplay);
    ServiceLocator::provide(&_presentation);
    ServiceLocator::provide(&_flow);
    ServiceLocator::provide(&_renderer);
    ServiceLocator::provide(&_interact);

    // A6-T4: 加载摄像机语言配置
    {
        std::string err;
        auto cam_def = load_camera_file("resources/camera/boss_camera.json", err);
        if (cam_def) {
            _camera_director.try_init(*cam_def);
            _camera_def_loaded = true;
            // 设置视野半径 (tile * TILE_SIZE = px), 限制聚焦偏移范围
            _camera_director.set_fov_radius(_fov_radius * TILE_SIZE);
        } else {
            LOG_INFO("[A6] Camera config not loaded: %s", err.c_str());
        }
    }
}

// v1.5.0-P0: sim goto-floor 玩家强度对标 — bot 裸装 (120HP/12ATK) 进 F15
// 对 demon_lord (620HP/26ATK/15PDEF) 约 6 击即死, 取证不了镜像机制。
// 对标手算: 15 层自然成长的合理面板 (等级/装备/药水同步补), 只影响 sim 取证局。
void GameScene::_sim_goto_scale_player(int start_floor) {
    int target_level = 1 + (start_floor - 1) * 2 / 3;         // F15→Lv11
    player->level = target_level;
    player->xp = 0;
    player->xp_to_next = Player::calc_xp_for_level(target_level);
    int scale_hp = 120 + (start_floor - 1) * 60;                // F15→960
    player->combat.max_hp = scale_hp;
    player->combat.current_hp = scale_hp;
    player->combat.attack += (start_floor - 1) * 2;            // F15→40
    player->combat.physical_defense += (start_floor - 1);      // F15→+14
    player->combat.magical_defense += (start_floor - 1) / 2;   // F15→+7
    for (int potion_i = 0; potion_i < start_floor / 3; potion_i++)
        player->inventory.items.push_back(
            std::make_shared<ConsumableItem>("治疗药水", Rarity::COMMON, "heal", 30));
}

void GameScene::new_game() {
    // M1: 新对局清空行为录制 — 修复跨局流污染
    // (旧代码 g_behavior 从不清: 上一局的死亡记录混入本局镜像分析)
    g_behavior.clear();

    // P1-B-fix(C): NPC 状态跨局清零 — new_game 从不重置, 上一局 met/finished/
    // tile 残留进本局 (sim 100 局共享一个 GameScene 实例, 逐局累积错位)
    for (int i = 0; i < _npc_count; i++) _npc_state[i] = NPCState{};
    _npc_count = 0;
    _current_npc_index = -1;
    _dialogue = DialogueState{};

    // P1-C7-C: Boss 子系统跨局重置 — 原 reset() 全仓零调用, boss 状态跨局
    // 全残留 (evolution/encounter/battle_report 等)。mirror 跨局记忆走
    // export/inject 独立通道, reset_run 不清 (语义正确)
    _boss.reset_run();

    player = std::make_unique<Player>(TILE_SIZE * 2, TILE_SIZE * 2,
        PLAYER_SPEED, PLAYER_MAX_HP, PLAYER_ATTACK, PLAYER_PDEF, PLAYER_MDEF);

    // Q3.10: 初始携带治疗药水 — 无自愈系开局遇毒/环境伤害无解 (F1 掉血死)
    // P1-C1: 2瓶→3瓶 — P1-B 基线毒路死亡35%: 毒DOT单次24HP, 2瓶60HP不够对冲2-3次中毒
    for (int potion_i = 0; potion_i < 3; potion_i++)
        player->inventory.items.push_back(
            std::make_shared<ConsumableItem>("治疗药水", Rarity::COMMON, "heal", 30));

    // G10.1: Element select on first-ever game
    if (!player->element.initialized) {
        if (g_sim_mode) {
            // G5.6: sim 模式跳过元素选择, 固定火系
            player->element.select(ElementType::FIRE);
            element_select_active = false;
        } else {
            element_select_active = true;
            element_select_cursor = 0;
            state = GameState::TITLE;
            return;
        }
    }

    // Q3.5: sim run 1 在 enter_floor 前播种 rng — 否则楼层种子从
    // random_device^time 初始态抽取 → 同种子不可重放 (F5 boss 每进程随机)
    if (g_sim_mode) {
        _dungeon_seed = SimRunner::inst().current_seed();
        seed_rng(_dungeon_seed);
        // Q3.8: 清空上一局的脱卡状态 — static 改实例成员后跨局残留的指针键必须清零
        _unstuck_last_pos.clear();
        _unstuck_since.clear();
        _sim_wall_frames = 0;        // M2-E: 帧计数兜底 (替代 GetTime 墙钟, 确定性)
        _sim_wall_timeout = false;
        _sim_game_timeout = false;   // M2-A: 超时标记跨局清零
        sim_stuck_watchdog = 0;      // G13: 卡死看门狗标记跨局清零
    }

    auto sk = random_active_skill({}, true);  // G9: first skill always base 4
    player->skills.learn(std::move(sk));

    // D4.6 Step5: 加载Meta存档 + 重置本局统计
    g_meta.load();
    _gameplay.run_stats = RunSummary{};
    player->skills.apply_all_passives(player.get());
    // v1.5.0-P0: sim goto-floor 直达 (取证/冒烟深层的测试通道; 默认 0 时
    // 与原路径完全一致, RNG 基线不受影响; 仅 sim 模式生效)
    int start_floor = 1;
    if (g_sim_mode && g_goto_floor >= 1) {
        start_floor = (std::min)(g_goto_floor, 15);
        _sim_goto_scale_player(start_floor);
    }
    current_floor = start_floor;
    max_unlocked_floor = start_floor;
    enter_floor(start_floor);
    _presentation.set_build_theme(BuildType::BERSERKER);  // G5.8.2: default theme

    // ── G4.5: Replay/Record auto-start ──
    if (g_replay_mode && !g_replay_path.empty())
        start_replay(g_replay_path);
    else if (g_record_mode && !g_record_path.empty())
        start_recording(_dungeon_seed);

    // ── G5.6: Sim mode init ──
    if (g_sim_mode) {
        _sim_mode = true;
        // G8.1: choose AI agent type
        if (g_sim_ai_type == "bt") {
            _sim_bt = std::make_unique<BTAgent>();
            _sim_bt->build_tree();
            _sim_bt->start(player.get());
            _use_bt_agent = true;
        } else {
            // G8.3: MCTS flag on DecisionAgent
            DecisionAgent::g_use_mcts = (g_sim_ai_type == "mcts");
            _sim_ai = std::make_unique<DecisionAgent>();
            _sim_ai->start(player.get());
            _sim_ai->set_room_manager(&_room_mgr);   // P0-M2: room-domain teleport
            _use_bt_agent = false;
        }
        seed_rng(_dungeon_seed ? _dungeon_seed : (uint32_t)(SimRunner::inst().current_run() * 1234567));
    }
}

// ── M4e: 注入跨对局镜像记忆 ──
void GameScene::set_mirror_memory(const std::vector<float>& alpha,
                                  const std::vector<float>& beta) {
    _mirror_mem_alpha = alpha;
    _mirror_mem_beta = beta;
}

void GameScene::load_saved_game(int floor, int max_f, std::unique_ptr<Player> p,
                                 uint32_t seed,
                                 const std::vector<bool>& special_triggered,
                                 const std::vector<bool>& special_discovered,
                                 const std::unordered_map<std::string, int>& rule_counters,
                                 const std::unordered_map<int, int>& quest_states,
                                 float play_time) {
    player = std::move(p);
    current_floor = floor;
    max_unlocked_floor = max_f;

    // ── G1 Step7: 恢复 WorldState rule_* counters ──
    for (auto& kv : rule_counters)
        _gameplay.world_state.add_counter(kv.first, kv.second);

    // ── G2.4: 恢复 Quest states ──
    _gameplay.quest_mgr.restore_states(quest_states);

    // G10.9-B2: 结局解锁改为账号级 — 从 meta 恢复 (不再随档往返)
    _gameplay.ending_dir.restore_unlocked(g_meta.data().unlocked_endings);
    // G10.9-B2: 恢复本档累计时长 (进入楼层时钟的起点)
    game_time = play_time;

    enter_floor(floor, seed);

    // B8: 地图生成完成后恢复特殊房间触发状态
    if (game_map && !special_triggered.empty()) {
        auto& rooms = game_map->special_rooms;
        size_t n = std::min(rooms.size(), special_triggered.size());
        for (size_t i = 0; i < n; i++)
            rooms[i].triggered = special_triggered[i];
        LOG_INFO("[ROOM] 恢复触发状态: %zu/%zu", n, special_triggered.size());
    }
    // B10: 恢复发现状态
    if (game_map && !special_discovered.empty()) {
        auto& rooms = game_map->special_rooms;
        size_t n = std::min(rooms.size(), special_discovered.size());
        for (size_t i = 0; i < n; i++)
            rooms[i].discovered = special_discovered[i];
        LOG_INFO("[ROOM] 恢复发现状态: %zu/%zu", n, special_discovered.size());
    }
    // B13: Relic 不再跨层 — 读档不恢复圣物 (每层重新Build)
}

// G9.3 (RNG-001): 屏震偏移 — 视觉流唯一合法消费入口 (GREEN: 独立 visual_rng)
std::pair<float, float> GameScene::shake_offset(float intensity, float timer) {
    if (timer <= 0.0f) return {0.0f, 0.0f};
    float s = intensity * (timer / 0.12f);
    float ox = ((float)(visual_rng() % 100) / 100.0f - 0.5f) * s * 2;
    float oy = ((float)(visual_rng() % 100) / 100.0f - 0.5f) * s * 2;
    return {ox, oy};
}

void GameScene::enter_floor(int floor, uint32_t seed) {
    current_floor = floor;
    player->current_floor = floor;
    game_time = 0;
    ground_items.clear();
    inventory_open = false;
    inventory_cursor = 0;
    stairs_active = false;
    active_effects.clear();
    time_stop_remaining = 0;
    pending_damage.clear();
    monsters.clear();
    _presentation.room_msg.clear();
    _presentation.room_msg_timer = 0.0f;

    // Batch 3A: FLOOR relics removed on floor transition
    std::vector<std::string> floor_relic_ids;
    for (auto& r : player->relics) {
        if (r.scope == PersistenceScope::FLOOR) {
            floor_relic_ids.push_back(r.id);
        }
    }
    for (auto& id : floor_relic_ids) {
        player->remove_relic(id);
    }

    // D4 Step1: 重置事件状态
    if (game_map) {
        game_map->event_room_index = -1;
        game_map->event_triggered = false;
    }
    _presentation.boss_intro_text.clear();
    _presentation.boss_modifier_text.clear();
    // P1-C7-C: 补 Boss 子系统跨层重置 — 原单一 reset() 设计意图"新楼层开始时
    // 调用"但全仓零调用, evolution/encounter/cinematic/timeline/domain 等
    // 状态跨层残留 (仅 arena 在此单独清过)
    _boss.reset_floor();

    // B8: seed=0 → 新楼层随机生成; seed!=0 → 读档恢复
    // 换层清空脱卡状态 — Monster* 键在换层后地址可复用, 残留键污染新怪 (进程间不确定)
    _unstuck_last_pos.clear();
    _unstuck_since.clear();

    // G9.2 (audit LIFE-001/002): 换层统一重置跨层生命周期状态 —
    //   Challenge 相位禁止跨层残留 (防免钥匙挑战/幽灵 tick);
    //   RoomManager 房间数据禁止带入 Boss 层 (Boss 层不走 build 分支)。
    _challenge.reset();
    _room_mgr.reset();
    // B4-T9: 压轴保底是账号级记忆 — 每次进层从 meta 重新载入.
    // 只靠控制器自持会在 GameScene 重建时清零 (玩家反复进出同层 → 保底永远到不了阈值).
    _challenge_pity_streak = g_meta.challenge_pity_streak();

    if (seed != 0) {
        _dungeon_seed = seed;
    } else {
        _dungeon_seed = static_cast<uint32_t>(rng());
    }
    // G9.3 (RNG-001): 视觉流按层重播种 — 派生自 dungeon seed, 不触碰 gameplay 流
    seed_visual_rng(_dungeon_seed ^ 0x9E3779B9u);

    // D1: FloorConfig — 统一难度/敌人池/特殊房间/BGM/剧情
    const FloorConfig* fcfg = get_floor_config(floor);
    _fov_radius = (fcfg->fov_radius > 0) ? fcfg->fov_radius : FOV_RADIUS_DEFAULT;  // Batch 1 (D4)

    // 生成地牢 (B8: seed 驱动; D1: special_room_count 从配置读)
    DungeonGenerator gen(MAP_WIDTH, MAP_HEIGHT, TILE_SIZE);
    game_map = gen.generate(_dungeon_seed, fcfg->special_room_count, fcfg->arena_density);
    _setup_boss_arena_terrain(gen, floor);   // M4b: Boss 房机制地形
    game_map->reset_visibility();
    _last_player_tile_x = -1;
    _last_player_tile_y = -1;
    _boss_last_known = {-1, -1};   // Phase 3: 新楼层重置 Boss 最后已知位置
    auto rooms = gen.get_room_centers();

    // D9-rest: 休息层保证 1 个泉水房 — 50% landmark 替换可能吞掉治疗资源
    if (fcfg->is_rest_floor && !game_map->special_rooms.empty()) {
        bool has_fountain = false;
        for (auto& sr : game_map->special_rooms)
            if (sr.type == SpecialRoomType::FOUNTAIN) { has_fountain = true; break; }
        if (!has_fountain) game_map->special_rooms[0].type = SpecialRoomType::FOUNTAIN;
    }

    // M4f: biome palette → 地图 (程序化像素纹理基色)
    const BiomeDef* biome = get_biome_for_floor(floor);
    game_map->set_palette(biome ? &biome->palette : nullptr);
    game_map->set_biome_id(biome ? biome->id.c_str() : "");   // M5-A: 群系贴图选择
    // G11.2: 氛围层 — 本群系 ambient 粒子配置 (监狱尘埃/火山余烬/深渊幽光)
    _ambient.set_biome(biome);
    _ambient.set_mood(0.0f, 0.0f);          // 入层重置情绪
    _kill_streak_timer = 0;

    // Door sprites — load once, reuse across floors
    if (!DoorRenderer::inst().is_loaded())
        DoorRenderer::inst().init();

    // 放置玩家
    if (!rooms.empty()) {
        auto [tx, ty] = rooms[0];
        auto [px, py] = game_map->tile_to_pixel(tx, ty);
        player->entity.position = {px, py};
        player->entity.sync_rect();
    }

    // 放置怪物
    const BossDef* bdef = nullptr;  // G1 Step6: BossDef replaces BossTemplate
    if (is_boss_floor(floor)) {
        auto pos = rooms.back();
        boss_floor = floor;
        BossType btype = boss_type_for_floor(floor, _dungeon_seed);
        bdef = get_boss_def_for_type((int)btype);  // G1 Step6: BossType → BossDef
        boss_intro_title = bdef->title.c_str();
        boss_intro_lore = bdef->lore.c_str();
        boss_intro_skills = get_boss_skills_text(bdef);
        boss_intro_color = get_boss_visual_color(bdef->visual_id);
        boss_intro_visual = bdef->visual_id;   // M5-C: 立绘 key 派生用
        state = GameState::BOSS_INTRO;

        // D4 Step5.5: BossNarrative覆盖intro对话
        BuildType bt = calculate_build(player.get()).identify();
        const BossDialogue* bd = _boss.narrative.find_intro(
            floor, _gameplay.world_state, bt, _gameplay.rels, _gameplay.story);
        if (bd && bd->intro) {
            _presentation.boss_intro_text = bd->intro;
        }
    } else {
        FloorManager::spawn_floor_monsters(floor, game_map.get(), monsters, rooms);
        // G15: 第 1 层保底武器 — 出生房旁固定 1 把 common 武器 (空手死亡螺旋:
        //       sim 50 局 avg_damage 42 vs 承伤 231, 拿到武器局 kills 8 vs 空手 0-2)
        if (floor == 1 && !rooms.empty()) {
            const WeaponDef* wdef = get_weapon_def("sword_common");
            if (wdef) {
                auto weapon = std::make_shared<EquipmentItem>(
                    pick_weapon_name(wdef, 0), Rarity::COMMON, "weapon",
                    (int)wdef->base_damage);
                weapon->weapon_def_id = "sword_common";
                auto [wtx, wty] = rooms[0];
                ground_items.push_back({weapon, wtx + 2, wty});
            }
        }
        state = GameState::PLAYING;
        // Batch 2C: 进层构建 Room Encounter 映射 (房间矩形 + 门组, 一次性固化)
        RoomEncounterCallbacks cb;
        cb.on_locked = [this](int) {
            show_room_message("房间封锁了!");
            // G10.8-B4: 首次封门教学 — 新手最大断崖 (P0 调查证明连 SimAI 都会困住)
            first_hint(*this, "encounter_lock",
                       "房间已封锁!", "击败房内所有敌人后门自动打开");
            EventBus::inst().emit(GameEventType::ROOM_LOCKED, this, 0, 0.0f, nullptr);
        };
        cb.on_cleared = [this](int) {
            EventBus::inst().emit(GameEventType::ROOM_CLEAR, this, 0, 0.0f, nullptr);
            /* 掉落钩子 Batch 3 接 */
        };
        _room_mgr.set_callbacks(cb);
        _room_mgr.build(game_map.get(), gen.get_room_rects(), is_boss_floor(floor));
    }

    for (auto& sr : game_map->special_rooms) {
        if (sr.type == SpecialRoomType::CHALLENGE) {
            _challenge.setup_portal(sr.portal_tx, sr.portal_ty);
            _challenge.set_room_rect(sr.rx, sr.ry, sr.rw, sr.rh);
            break;
        }
    }

    stairs_pos = rooms.back();

    // ── G5.5: 非Boss层生成动态事件 (概率提升 + 深层多事件) ──
    if (!fcfg->is_boss && rooms.size() > 3) {
        float ev_chance = fcfg->is_rest_floor ? 0.70f : 0.40f; // G5.5: up from 0.50/0.25
        ChapterConfig ch = *get_chapter_config(fcfg->chapter);
        int ev_count = (fcfg->chapter >= 1) ? 2 : 1; // G5.5: ch2+ spawns 2 events
        for (int ei = 0; ei < ev_count; ei++) {
            if ((float)(rng() % 1000) / 1000.0f >= ev_chance) continue;
            DungeonEvent ev = generate_event(floor, ch, rng);
            int ev_room = 1 + (int)(rng() % ((uint32_t)rooms.size() - 2));
            auto [etx, ety] = rooms[ev_room];
            game_map->event_room_index = ev_room;
            game_map->event_tile_x = etx;
            game_map->event_tile_y = ety;
            game_map->event_triggered = false;
            game_map->event_type = ev.type;  // 存储事件类型
        }
    }

    // D4 Step4: NPC 生成 (配置中有NPC的楼层)
    if (!fcfg->is_boss) _spawn_floor_npcs(floor, rooms);

    // D4 Step5.1: StoryDirector 楼层推进
    _gameplay.story.enter_floor(floor);
    // F15.2: record floor enter
    g_behavior.on_floor_enter(game_time, floor);
    // D4 Step5.2: QuestManager 楼层推进
    _gameplay.quest_mgr.set_relationship_system(&_gameplay.rels);
    _gameplay.quest_mgr.update(_gameplay.world_state, _gameplay.story);

    // B11: blood_charm — 进入新楼层时使用有效最大生命
    player->combat.current_hp = get_effective_max_hp(player.get());
    if (_sim_mode) _sim_hp_prev = player->combat.current_hp; // Q3.2: 入场回满不计入治疗统计
    player->reset_attack_timers();

    // D8: soul_lantern — 进入新楼层获得 attack_up + heal 10
    if (player_has_relic(player.get(), "soul_lantern")) {
        apply_buff(player.get(), "attack_up", 1);
        heal_player(player.get(), 10);
    }

    // M1A.1: 新遗物系统 on_floor_enter
    _combat.relic_fx().on_floor_enter(player.get());

    // D4 Step3: 楼层入场演出 (非Boss层, 首次进入, new_game)
    if (!fcfg->is_boss && !_gameplay.narr_state.floor_intro_played[floor - 1]) {
        _gameplay.narr_state.floor_intro_played[floor - 1] = true;
        _presentation.floor_intro_active = true;
        _presentation.floor_intro_timer = 2.0f;
        _presentation.floor_intro_fade = 0.0f;
        _presentation.floor_intro_floor = floor;
        _gameplay.narr_state.narration_timer = 25.0f + (float)(rng() % 15);
    }
    // D4 Step3: 章节入场 (每新章节开始)
    if (!fcfg->is_boss && fcfg->chapter != _presentation.chapter_intro_ch) {
        _presentation.chapter_intro_active = true;
        _presentation.chapter_intro_timer = 3.0f;
        _presentation.chapter_intro_ch = fcfg->chapter;
        _presentation.floor_intro_active = false;  // chapter intro overrides floor intro
    }

    // D1: BGM + 剧情从 FloorConfig 读取
    _pending_bgm = fcfg->bgm;

    if (fcfg->story_msg && !_presentation.floor_intro_active && !_presentation.chapter_intro_active) {
        _presentation.room_msg = fcfg->story_msg;
        _presentation.room_msg_timer = 3.0f;
    }

    if (fcfg->is_boss) {
        LOG_INFO("进入第%d层 (Boss floor)", floor);
    } else {
        LOG_INFO("进入第%d层 [%s] - %d只怪物, HPx%.2f ATKx%.2f",
            floor, fcfg->chapter_label, (int)monsters.size(),
            fcfg->hp_mult, fcfg->atk_mult);
    }
    // G10.8-B4: 首层首遇提示 (元素已选 + HUD 元素速览; 跨 run 只弹一次)
    if (floor == 1 && !g_sim_mode && !element_select_active) {
        first_hint(*this, "element_select",
                   "你已选择元素!", "攻击会触发元素效果（暴击/冻结/毒伤）");
        first_hint(*this, "hud_intro",
                   "HUD: 左上 HP/XP | 技能栏有冷却 | 左下金币钥匙",
                   "按 R 看圣物 · M 看地图 · F1 看日志");
    }
    EventBus::inst().emit(GameEventType::FLOOR_ENTER, this, floor,
                           fcfg->is_boss ? 1.0f : 0.0f);
}

// ============================================================
// 主循环
// ============================================================
void GameScene::_process(double delta) {
    if (!player) return;
    float dt = (float)delta;

    // Q3.10: 兜底超时置顶 — 任何状态(BOSS_CINEMATIC/事件/对话/非PLAYING)都可能卡死
    // 原置尾 870 行: state!=PLAYING 提前 return → 10min 墙钟永不触发 (v11 r8 进F14后静默150min)
    // M2-E: GetTime()(真实墙钟, 慢机改变结果) → 帧计数 (600s×60fps=36000帧, 确定性)
    if (_sim_mode && ++_sim_wall_frames > 36000) {
        LOG_INFO("[SIM] 帧数兜底超时(36000f) 第%d层 — 强制结算", current_floor);
        // P1-C3 探针: 兜底结算现场快照 — 定位游走困死的末帧状态
        // (与 900s P0DIAG 同构: 玩家/怪/楼梯/门, headless 一次性打印)
        {
            float px = player->entity.rect.x + player->entity.rect.width/2;
            float py = player->entity.rect.y + player->entity.rect.height/2;
            LOG_INFO("[C3DIAG] stairs_active=%d monsters=%zu picks=%d gold=%d",
                     (int)stairs_active, monsters.size(), _sim_items_picked, player->gold);
            for (auto& m : monsters) {
                if (!m || !m->combat.is_alive) continue;
                float d = hypotf(m->entity.rect.x + m->entity.rect.width/2 - px,
                                 m->entity.rect.y + m->entity.rect.height/2 - py);
                LOG_INFO("[C3DIAG] mon '%s' hp=%d/%d dist=%.0f pos=(%.0f,%.0f) room=%d",
                         m->name.c_str(), m->combat.current_hp, m->combat.max_hp,
                         d, m->entity.rect.x, m->entity.rect.y,
                         _room_mgr.room_at(
                             (int)(m->entity.rect.x / TILE_SIZE),
                             (int)(m->entity.rect.y / TILE_SIZE)));
            }
            if (game_map) {
                auto [stx, sty] = game_map->pixel_to_tile(px, py);
                LOG_INFO("[C3DIAG] player tile=(%d,%d) walkable=%d stairs_pos=(%d,%d) on_stairs=%d",
                         stx, sty, (int)game_map->is_walkable(stx, sty),
                         stairs_pos.first, stairs_pos.second,
                         (std::make_pair(stx, sty) == stairs_pos) ? 1 : 0);
            }
        }
        _sim_wall_timeout = true;
        _collect_sim_stats();
        return;
    }

    // G13: 卡死看门狗 — 传送连续失败即判定本局不可恢复, 提前结算。
    // 不设 _sim_wall_timeout, 结算分类落到末尾 else 分支 → STUCK_RECOVERED。
    // (原实现烧满 36000 帧兜底, 卡死局全标 TIMEOUT_WALL, 污染 avg_floor/avg_turns)
    if (_sim_mode && sim_stuck_watchdog > 0) {
        LOG_INFO("[SIM] 卡死看门狗触发 — 强制结算 STUCK_RECOVERED 第%d层", current_floor);
        sim_stuck_watchdog = 0;
        _collect_sim_stats();
        return;
    }

    if (state == GameState::BOSS_CINEMATIC) {
        // B15: Boss登场 — 玩家冻结, Boss暂停, 2秒后启动
        if (_boss_entrance_timer > 0) {
            _boss_entrance_timer -= dt;
            if (_boss_entrance_timer <= 0) {
                _boss_entered = true;
                state = GameState::PLAYING;
                // A6: Boss 出场预渲染其所在房间 (解决镜头聚焦后视野虚空)
                Monster* boss_monster = _get_boss();
                if (boss_monster && game_map) {
                    int boss_tx = (int)(boss_monster->entity.rect.x / TILE_SIZE);
                    int boss_ty = (int)(boss_monster->entity.rect.y / TILE_SIZE);
                    game_map->mark_boss_room(boss_tx, boss_ty);
                }
                // A6-T4: Boss 出场触发运镜
                if (!_sim_mode && _camera_def_loaded) {
                    _camera_director.enter_boss_war();
                }
            }
        } else {
            boss_cinematic_timer -= dt;
            if (boss_cinematic_timer <= 0) {
                boss_cinematic_timer = 0;
                _boss_entered = true;
                state = GameState::PLAYING;
                // A6: Boss 出场预渲染其所在房间 (解决镜头聚焦后视野虚空)
                Monster* boss_monster = _get_boss();
                if (boss_monster && game_map) {
                    int boss_tx = (int)(boss_monster->entity.rect.x / TILE_SIZE);
                    int boss_ty = (int)(boss_monster->entity.rect.y / TILE_SIZE);
                    game_map->mark_boss_room(boss_tx, boss_ty);
                }
                // A6-T4: Boss 出场触发运镜
                if (!_sim_mode && _camera_def_loaded) {
                    _camera_director.enter_boss_war();
                }
            }
        }
    }

    if (state != GameState::PLAYING) return;

    // A6-T2: HitStop — 击杀顿帧 (wall clock, 独立于 PresentationSystem)
    // sim 模式跳过: 顿帧使 game_time 变慢, 影响 sim 确定性
    if (!_sim_mode) {
        _hit_stop.update(dt);
        if (_hit_stop.is_stunned()) {
            _presentation.tick(dt);
            return;
        }
    }

    // A6-T4: CameraDirector 更新 (wall clock, sim 模式跳过)
    if (!_sim_mode && _camera_def_loaded) {
        Vector2 player_pos = {player->entity.rect.x + player->entity.rect.width/2,
                              player->entity.rect.y + player->entity.rect.height/2};
        Vector2 boss_pos = {0, 0};
        Monster* boss_monster = _get_boss();
        if (boss_monster && boss_monster->combat.is_alive) {
            boss_pos.x = boss_monster->entity.rect.x + boss_monster->entity.rect.width/2;
            boss_pos.y = boss_monster->entity.rect.y + boss_monster->entity.rect.height/2;
        }
        _camera_director.update(dt, player_pos, boss_pos);
    }

    // Q4.1: HitStop — 冻结期间只推表现层, 世界模拟暂停 (打击感)
    // Q3.10: sim 模式跳过 — 表现层冻结使 game_time 变慢, 900s 超时被稀释成数十分钟
    if (!_sim_mode && _presentation.is_frozen()) {
        _presentation.tick(dt);
        return;
    }
    game_time += dt;
    if (_sim_ai) _sim_ai->set_time(game_time); // Q3.2: AI 技能冷却判定需要当前时间
    if (_sim_bt) _sim_bt->set_time(game_time); // Q3.15: BT agent 同样需要真实时间 (P0-2 fix)

    // P1-A2: 地面物品只读注入 — SimAI 感知掉落物 (归属仍在本场景, AI 不修改)
    if (_sim_ai) {
        std::vector<DecisionAgent::GroundSpot> spots;
        spots.reserve(ground_items.size());
        for (auto& d : ground_items) {
            bool is_potion = false;
            bool is_weapon = false;
            auto* c = dynamic_cast<ConsumableItem*>(d.item.get());
            if (c && c->effect_type == "heal") is_potion = true;
            // P1-C5: 武器掉落标记 — 空手 AI 优先追击 (EquipmentItem weapon 槽)
            auto* eq = dynamic_cast<EquipmentItem*>(d.item.get());
            if (eq && eq->slot == "weapon") is_weapon = true;
            spots.push_back({d.tile_x, d.tile_y, is_potion, is_weapon});
        }
        _sim_ai->set_ground_items(std::move(spots));
        // P1-C3: 楼梯位置注入 — AI 需导航到楼梯格才能按 E 下楼
        // (原只传 bool stairs_active, AI 返回 descend 但不走路 → 600s 站桩)
        _sim_ai->set_stairs_pos(stairs_active ? stairs_pos.first : -1,
                                stairs_active ? stairs_pos.second : -1);
    }

    // Q3.2: sim 真实伤害统计 — 玩家 HP 下降累计 (含毒池等环境伤害)
    if (_sim_mode && player) {
        int hp = player->combat.current_hp;
        if (_sim_hp_prev < 0) {
            _sim_hp_prev = hp;
        } else if (hp < _sim_hp_prev) {
            _sim_dmg_taken += _sim_hp_prev - hp;
        } else if (hp > _sim_hp_prev) {
            _sim_heal_total += hp - _sim_hp_prev; // 泉水/药水/吸血等所有治疗
        }
        _sim_hp_prev = hp;
        // Q3.9: 清除已不在场的怪记录 — instance_id 唯一且不复用, 仅做内存回收
        for (auto it = _sim_mon_hp.begin(); it != _sim_mon_hp.end(); ) {
            bool alive_now = false;
            for (auto& m : monsters)
                if (m->instance_id == it->first) { alive_now = true; break; }
            if (alive_now) ++it; else it = _sim_mon_hp.erase(it);
        }
        for (auto& m : monsters) {
            if (!m) continue;
            auto it = _sim_mon_hp.find(m->instance_id);
            if (it == _sim_mon_hp.end()) {
                _sim_mon_hp[m->instance_id] = m->combat.current_hp;
            } else if (m->combat.current_hp < it->second) {
                _sim_dmg_dealt += it->second - m->combat.current_hp;
                it->second = m->combat.current_hp;
            }
        }
    }

    // M4a-fix: 兜底受击日志 — 只报未记账来源的玩家掉血 (标签源已调 mark_damage_logged)
    {
        int hp_now = player->combat.current_hp;
        int logged = player->combat.logged_hp;
        if (logged >= 0 && hp_now < logged)
            LOG_INFO("[DMG] 未标注来源 玩家掉血 %d → HP:%d/%d",
                     logged - hp_now, hp_now, player->combat.max_hp);
        player->combat.logged_hp = hp_now;
    }

    // B15: Boss Phase2 提示
    if (!_boss_phase2_shown) {
        auto* boss = _get_boss();
        if (boss) {
            auto* bai = dynamic_cast<BossAI*>(boss->ai);
            if (bai && bai->phase2) {
                // D4 Step5.5: Boss Phase2对话
                BuildType bt = calculate_build(player.get()).identify();
                const BossDialogue* pd = _boss.narrative.find_phase2(
                    current_floor, _gameplay.world_state, bt);
                _presentation.room_msg = pd && pd->phase2 ? pd->phase2 : "BOSS 狂暴！";
                _presentation.room_msg_timer = 2.5f;
                _boss_phase2_shown = true;
                _presentation.trigger_shake(10.0f);
                _boss.notify_phase2();   // D5 Step6 (cinematic + timeline 收敛)
            }
        }
    }

    // D5 Step2: LastStand 检测  D5 Step3: BossBehavior 评估
    if (!_boss.evolution.last_stand_triggered) {
        auto* boss = _get_boss();
        if (boss && boss->is_boss && (float)boss->combat.current_hp / boss->combat.max_hp < 0.15f) {
            _boss.evolution.last_stand_triggered = true;
            _boss.notify_last_stand(boss);  // CD减半 + 范围提升 + cinematic + timeline
            _presentation.room_msg = _boss.evolution.evolution_name
                ? std::string(_boss.evolution.evolution_name) + std::string("!")
                : std::string("LAST STAND!");
            _presentation.room_msg_timer = 2.5f;
            _presentation.trigger_shake(14.0f);
        }
    }

    // D5 Step3: Boss Memory tick + Behavior 评估 (收敛至 BossSystemDirector::tick)
    {
        auto* boss = _get_boss();
        if (boss && boss->is_boss && time_stop_remaining <= 0) {
            _boss._weak_point_pool = &monsters;  // F10.2: pass pool for core spawn
            _boss.tick(dt, boss, player.get(), current_floor, game_time,
                       _gameplay.world_state,
                       _gameplay.rels, _gameplay.story.stage(), monsters,
                       &active_effects);  // F15-fix: 镜像战特效通道

            // F10.1: Domain state change VFX
            if (_boss._behavior_type == "domain"
                && _boss.arena_state != _boss_last_arena_state) {
                _boss_last_arena_state = _boss.arena_state;
                switch (_boss.arena_state) {
                case BossArenaState::DOMAIN_PHASE:
                    _presentation.show_message("【领域展开】Boss受到保护 — 寻找破绽!", 2.5f);
                    _presentation.trigger_shake(6.0f);
                    get_tree()->get_audio()->play_sfx("domain_expand", 1.0f);
                    break;
                case BossArenaState::ENRAGED_PHASE:
                    _presentation.show_message("【狂暴领域!】核心周期减半 — 最终回响!", 2.5f);
                    _presentation.trigger_shake(14.0f);
                    _presentation.trigger_freeze(0.10f);
                    break;
                case BossArenaState::VULNERABLE_PHASE:
                    _presentation.show_message("【弱点暴露】全力输出! 伤害 x2", 2.0f);
                    _presentation.trigger_shake(10.0f);
                    _presentation.trigger_freeze(0.08f);
                    break;
                case BossArenaState::MECHANIC_PHASE:
                    _presentation.show_message("【核心粉碎!】弹幕风暴降临 — 躲避!", 2.2f);
                    _presentation.trigger_shake(8.0f);
                    _presentation.trigger_freeze(0.06f);
                    break;
                default: break;
                }
            }
        }
    }

    // D3 Step4: Build Fusion 检测 — 构筑成型/切换
    {
        BuildScore bs = calculate_build(player.get());
        BuildType bt = bs.identify();
        if (bt != BuildType::NONE && bt != _gameplay.last_notified_build) {
            std::string msg;
            if (_gameplay.last_notified_build == BuildType::NONE)
                msg = std::string("BUILD COMPLETE! ") + bs.build_name();
            else
                msg = std::string("BUILD CHANGED: ") + bs.build_name();
            _presentation.room_msg = msg;
            _presentation.room_msg_timer = 2.0f;
            _gameplay.last_notified_build = bt;
            _presentation.set_build_theme(bt);  // G5.8.2: update HUD/VFX colors
            _presentation.trigger_shake(CombatFeelSystem::SHAKE_LIGHT);
            _presentation.trigger_freeze(CombatFeelSystem::BUILD_COMPLETE);
        }
    }

    // ── Buff 逐帧结算 ──
    std::vector<BuffEvent> buf_events;
    tick_buffs(player.get(), dt, &buf_events);
    // 时停期间世界冻结 — 敌方 buff (毒/DOT) 不结算 (玩家自身 buff 正常)
    if (time_stop_remaining <= 0)
        for (auto& m : monsters) tick_buffs(m.get(), dt, &buf_events, player.get()); // B11: venom_fang

    // M1A.1: 新遗物系统 PASSIVE 效果逐帧结算
    _combat.relic_fx().tick(player.get(), dt);

    // Buff 事件日志 + C1: poison tick 伤害数字
    for (auto& ev : buf_events) {
        if (ev.type == BuffEventType::APPLIED)
            LOG_INFO("[BUF] %s applied to %s (stacks=%d)", ev.buff_id.c_str(), ev.target.c_str(), ev.stacks);
        else if (ev.type == BuffEventType::TICK_DAMAGE) {
            LOG_INFO("[BUF] %s tick on %s: %d dmg (stacks=%d)", ev.buff_id.c_str(), ev.target.c_str(), ev.value, ev.stacks);
            // C1: Poison 浮动数字 (在怪物头顶上方)
            for (auto& m : monsters) {
                if (m->name == ev.target) {
                    _presentation.damage_floats.push_back({
                        m->entity.rect.x + m->entity.rect.width/2,
                        m->entity.rect.y - 8,
                        0.6f, 0.6f, ev.value,
                        dmg_color_for(ev.value, false, true)
                    });
                    // G10.3: poison tick VFX at monster position
                    EventBus::inst().emit(GameEventType::ELEMENT_POISON_TICK,
                        m.get(), ev.value, 0.0f, m->name.c_str());
                    break;
                }
            }
        }
        else if (ev.type == BuffEventType::EXPIRED)
            LOG_INFO("[BUF] %s expired from %s", ev.buff_id.c_str(), ev.target.c_str());
    }

    // 清理被毒死的怪物
    _cleanup_dead_monsters();

    // ═══════════════════════════════════════════════════
    // G10.3: Element VFX — process pending element events
    // ═══════════════════════════════════════════════════
    {
        static std::vector<GameEvent> _elem_events;
        // Events are emitted synchronously by ElementResolver during weapon_executor
        // We process them here by subscribing once and storing in a static list
        static bool _subscribed = false;
        if (!_subscribed) {
            EventBus::inst().subscribe(GameEventType::ELEMENT_FIRE_HIT,
                [](const GameEvent& e) { _elem_events.push_back(e); }, "_elem");
            EventBus::inst().subscribe(GameEventType::ELEMENT_FIRE_CRITICAL,
                [](const GameEvent& e) { _elem_events.push_back(e); }, "_elem");
            EventBus::inst().subscribe(GameEventType::ELEMENT_ICE_SLOW,
                [](const GameEvent& e) { _elem_events.push_back(e); }, "_elem");
            EventBus::inst().subscribe(GameEventType::ELEMENT_ICE_FREEZE,
                [](const GameEvent& e) { _elem_events.push_back(e); }, "_elem");
            EventBus::inst().subscribe(GameEventType::ELEMENT_POISON_APPLY,
                [](const GameEvent& e) { _elem_events.push_back(e); }, "_elem");
            EventBus::inst().subscribe(GameEventType::ELEMENT_POISON_TICK,
                [](const GameEvent& e) { _elem_events.push_back(e); }, "_elem");
            EventBus::inst().subscribe(GameEventType::ELEMENT_LEVEL_UP,
                [](const GameEvent& e) { _elem_events.push_back(e); }, "_elem");
            // Q4.4: 怪物攻击音效 (AI 层通过事件解耦音频访问)
            EventBus::inst().subscribe(GameEventType::MONSTER_ATTACK,
                [](const GameEvent& e) {
                    auto* tree = ServiceLocator::get<SceneTree>();
                    if (tree) tree->get_audio()->play_sfx("monster_atk", 0.45f);
                }, "_elem");
            _subscribed = true;
        }
        // Process queued element events this frame
        for (auto& ev : _elem_events) {
            // Find target monster by name
            Monster* target = nullptr;
            if (ev.str_val) {
                for (auto& m : monsters) {
                    if (m->name == ev.str_val) { target = m.get(); break; }
                }
            }
            float tx = target ? target->entity.rect.x + target->entity.rect.width/2 : 0;
            float ty = target ? target->entity.rect.y + target->entity.rect.height/2 : 0;
            float px = player->entity.rect.x + player->entity.rect.width/2;
            float py = player->entity.rect.y + player->entity.rect.height/2;
            VFXServer vfx;

            switch (ev.type) {
            case GameEventType::ELEMENT_FIRE_HIT:
                if (target) {
                    vfx.beam(px, py, tx, ty, {255,140,30,200}, 0.28f);
                    vfx.explosion(tx, ty, 16.0f, {255,150,40,230}, 8, 0.32f);
                    vfx.ring(tx, ty, 20.0f, {255,120,20,220}, 2, 0.28f);
                    vfx.spark_burst(tx, ty + 10, 6, {255,160,50,200}, 0.25f);
                    _presentation.spawn_label(tx, ty - 18, "[火]", {255,140,40,255}, 0.8f);
                }
                break;
            case GameEventType::ELEMENT_FIRE_CRITICAL:
                if (target) {
                    vfx.beam(px, py, tx, ty, {255,180,40,240}, 0.35f);
                    vfx.explosion(tx, ty, 32.0f, {255,180,50,255}, 16, 0.40f);
                    vfx.shockwave(tx, ty, 50.0f, {255,140,30,230}, 3, 0.35f);
                    vfx.ring(tx, ty, 24.0f, {255,200,60,230}, 3, 0.45f);
                    vfx.flash(tx, ty, 18.0f, {255,220,100,220}, 0.12f);
                    _presentation.trigger_shake(8.0f);
                    _presentation.trigger_freeze(0.05f);
                    _presentation.spawn_label(tx, ty - 24, "[暴击!]", {255,220,40,255}, 1.0f);
                }
                break;
            case GameEventType::ELEMENT_ICE_SLOW:
                if (target) {
                    vfx.beam(px, py, tx, ty, {80,180,255,220}, 0.35f);
                    vfx.ring(tx, ty, 22.0f, {80,180,255,200}, 3, 0.40f);
                    vfx.spark_burst(tx, ty, 8, {100,200,255,200}, 0.35f);
                    vfx.ring(tx, ty, 16.0f, {100,200,255,120}, 1, 0.50f);
                    _presentation.spawn_label(tx, ty - 18, "[缓]", {80,180,255,255}, 0.8f);
                }
                break;
            case GameEventType::ELEMENT_ICE_FREEZE:
                if (target) {
                    vfx.flash(tx, ty, 26.0f, {120,200,255,240}, 0.18f);
                    vfx.shockwave(tx, ty, 35.0f, {100,180,255,220}, 3, 0.40f);
                    vfx.ring(tx, ty, 18.0f, {180,220,255,230}, 3, 0.50f);
                    vfx.explosion(tx, ty, 20.0f, {120,200,255,200}, 12, 0.35f);
                    _presentation.trigger_freeze(0.08f);
                    _presentation.trigger_shake(4.0f);
                    _presentation.spawn_label(tx, ty - 24, "[冻!]", {120,200,255,255}, 1.2f);
                }
                break;
            case GameEventType::ELEMENT_POISON_APPLY:
                if (target) {
                    vfx.beam(px, py, tx, ty, {80,200,80,200}, 0.30f);
                    vfx.ring(tx, ty, 18.0f, {80,210,80,200}, 3, 0.40f);
                    vfx.smoke_puff(tx, ty, 14.0f, {60,190,60,160}, 6, 0.50f);
                    vfx.spark_burst(tx, ty + 8, 5, {100,220,80,180}, 0.30f);
                    vfx.ring(tx, ty, 22.0f, {60,180,60,100}, 1, 0.55f);
                    _presentation.spawn_label(tx, ty - 18, "[毒]", {80,210,80,255}, 0.8f);
                }
                break;
            case GameEventType::ELEMENT_POISON_TICK:
                if (target) {
                    vfx.beam(px, py, tx, ty, {100,200,60,150}, 0.15f);
                    vfx.ring(tx, ty + 8, 12.0f, {80,210,70,180}, 2, 0.25f);
                    vfx.spark_burst(tx, ty + 4, 4, {100,230,80,180}, 0.22f);
                }
                break;
            case GameEventType::ELEMENT_LEVEL_UP: {
                const char* ename = ev.str_val ? ev.str_val : "fire";
                Color lc = (ename[0] == 'f') ? Color{255,140,30,220}
                         : (ename[0] == 'i') ? Color{100,200,255,220}
                         : Color{80,220,80,220};
                vfx.ring(px, py, 50.0f, lc, 3, 0.60f);
                vfx.spark_burst(px, py, 12, lc, 0.50f);
                vfx.flash(px, py, 20.0f, {lc.r,lc.g,lc.b,(unsigned char)(lc.a/2)}, 0.15f);
                {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%s Core Lv%d", ename, ev.int_val);
                    _presentation.show_message(buf, 2.0f);
                }
                break;
            }
            default: break;
            }
            for (auto& e : vfx.effects) active_effects.push_back(e);
        }
        _elem_events.clear();
    }

    // D2 Step5: Arena 环境 tick (D4.6: arena_scale) — 时停期间世界冻结
    if (game_map && !game_map->arena_objects.empty() && time_stop_remaining <= 0) {
        float px = player->entity.rect.x + player->entity.rect.width/2;
        float py = player->entity.rect.y + player->entity.rect.height/2;
        float ascale = g_growth.arena_scale(current_floor);
        for (auto& ao : game_map->arena_objects) {
            if (!ao.active) continue;
            float ax = ao.tile_x * TILE_SIZE + TILE_SIZE/2;
            float ay = ao.tile_y * TILE_SIZE + TILE_SIZE/2;
            float dist = hypotf(px - ax, py - ay);
            switch (ao.type) {
            case ArenaObjectType::EXPLOSIVE_BARREL:
                // 收官: 点燃倒计时 → 到期爆炸销毁 (触发源: 玩家攻击/敌方投射物)
                if (ao.timer > 0.0f) {
                    ao.timer -= dt;
                    if (ao.timer <= 0.0f) { ao.timer = 0; _explode_barrel(ao); ao.active = false; }
                }
                break;
            case ArenaObjectType::HEALING_TOTEM:
                ao.timer += dt;
                if (ao.timer >= 2.0f) { ao.timer = 0;
                    if (dist < 2.5f * TILE_SIZE) heal_player(player.get(), (int)(5 * ascale));
                    for (auto& m : monsters)
                        if (m->combat.is_alive && hypotf(m->entity.rect.x + m->entity.rect.width/2 - ax,
                            m->entity.rect.y + m->entity.rect.height/2 - ay) < 2.5f * TILE_SIZE)
                            m->combat.heal((int)(m->combat.max_hp * 0.08f * ascale));
                }
                break;
            case ArenaObjectType::POISON_POOL:
                // M4a-fix: 0.5s 间隔叠层 (原每帧 1 层 — 踩毒 0.1s 即叠满 5 层,
                // 离开后 4s 内每跳 15 伤害 = "离老远莫名死亡" 元凶)
                ao.timer += dt;
                if (ao.timer >= 0.5f) {
                    ao.timer = 0;
                    if (dist < 1.2f * TILE_SIZE) apply_buff(player.get(), "pool_poison", 1);
                }
                break;
            case ArenaObjectType::SPIKE: {
                int sd = (int)(3 * ascale), md = (int)(4 * ascale);
                if (dist < 0.8f * TILE_SIZE) {
                    player->combat.take_damage(sd);
                    player->combat.mark_damage_logged();
                    player->combat.last_damage_source = "env:尖刺";   // M2-B
                    LOG_INFO("[DMG] 尖刺 造成 %d 伤害 → 玩家", sd);
                    _presentation.damage_floats.push_back({px, py-8, 0.4f, 0.4f, sd, {255, 60, 40, 255}});
                }
                for (auto& m : monsters)
                    if (m->combat.is_alive && hypotf(m->entity.rect.x + m->entity.rect.width/2 - ax,
                        m->entity.rect.y + m->entity.rect.height/2 - ay) < 0.8f * TILE_SIZE)
                        m->combat.take_damage(md);
                break;
            }
            default: break;
            }
        }
    }

    // M4b: 熔岩地砖灼烧 (0.5s 节拍, Boss 免疫, 时停冻结)
    if (game_map && time_stop_remaining <= 0) {
        float px = player->entity.rect.x + player->entity.rect.width/2;
        float py = player->entity.rect.y + player->entity.rect.height/2;
        _lava_tick_timer += dt;
        if (_lava_tick_timer >= 0.5f) {
            _lava_tick_timer = 0.0f;
            auto [tx, ty] = game_map->pixel_to_tile(px, py);
            if (game_map->tile_at(tx, ty) == TileType::LAVA) {
                int ld = (int)(3 * g_growth.arena_scale(current_floor));
                player->combat.take_damage(ld);
                player->combat.mark_damage_logged();
                player->combat.last_damage_source = "env:熔岩";   // M2-B
                LOG_INFO("[DMG] 熔岩灼烧 造成 %d 伤害 → 玩家", ld);
                _presentation.damage_floats.push_back({px, py-8, 0.4f, 0.4f, ld, {255, 90, 30, 255}});
            }
            for (auto& m : monsters) {
                if (!m->combat.is_alive || m->is_boss) continue;
                auto [mtx, mty] = game_map->pixel_to_tile(
                    m->entity.rect.x + m->entity.rect.width/2,
                    m->entity.rect.y + m->entity.rect.height/2);
                if (game_map->tile_at(mtx, mty) == TileType::LAVA)
                    m->combat.take_damage((int)(4 * g_growth.arena_scale(current_floor)));
            }
        }
    }

    // 延迟播放 BGM（此时 get_tree() 已可用）
    if (!_pending_bgm.empty() && get_tree()) {
        LOG_DEBUG("BGM延迟播放: %s", _pending_bgm.c_str());
        get_tree()->get_audio()->play_bgm(_pending_bgm, 0.38f);
        _pending_bgm.clear();
    }

    // 时停倒计时
    if (time_stop_remaining > 0) {
        time_stop_remaining -= dt;
        if (time_stop_remaining <= 0) {
            time_stop_remaining = 0;
            _apply_pending_damage();
            // D3 Step3: The World E2 — 结束时释放 Shockwave
            if (_tw_evo_level >= 2) {
                float cx = player->entity.rect.x + player->entity.rect.width/2;
                float cy = player->entity.rect.y + player->entity.rect.height/2;
                for (auto& m : monsters) {
                    if (!m->combat.is_alive) continue;
                    float d = hypotf(m->entity.rect.x + m->entity.rect.width/2 - cx,
                                     m->entity.rect.y + m->entity.rect.height/2 - cy);
                    if (d < 120) {
                        int sd = calculate_damage(get_effective_attack(player.get()),
                            m->combat.get_effective_defense(AttackType::PHYSICAL));
                        m->combat.take_damage(sd);
                        // 击退
                        float dx = m->entity.rect.x - cx, dy = m->entity.rect.y - cy;
                        float len = sqrtf(dx*dx + dy*dy);
                        if (len > 0) {
                            clamp_displacement(m->entity, dx / len * 30, dy / len * 30, game_map.get());
                        }
                    }
                }
                _presentation.trigger_shake(8.0f);
                _presentation.room_msg = "时停冲击!";
                _presentation.room_msg_timer = 1.0f;
            }
            // D3 Step3: The World E3 — 速度提升 5s
            if (_tw_evo_level >= 3) {
                _tw_speed_boost = 5.0f;
            }
            _tw_evo_level = 0;
        }
    }

    // D3 Step3: TW E3 speed boost tick
    if (_tw_speed_boost > 0) _tw_speed_boost -= dt;

    // Q3.2: sim 单局时间上限 — 防止罕见卡死拖死整个批量 (正常对局 ~50s, 上限 900s)
    if (_sim_mode && game_time > 900.0f) {
        LOG_INFO("[SIM] 单局超时 t=%.0f 第%d层 — 强制结算", game_time, current_floor);
        // P0-M1 诊断: 超时现场快照 (玩家位置/朝向/存活怪/最近怪距离/楼梯状态)
        {
            float px = player->entity.rect.x + player->entity.rect.width/2;
            float py = player->entity.rect.y + player->entity.rect.height/2;
            LOG_INFO("[P0DIAG] player@(%.0f,%.0f) dir=%d hp=%d/%d stairs=%d monsters=%zu",
                px, py, (int)player->direction, player->combat.current_hp,
                player->combat.max_hp, (int)stairs_active, monsters.size());
            for (auto& m : monsters) {
                if (!m || !m->combat.is_alive) continue;
                float d = hypotf(m->entity.rect.x + 14 - px, m->entity.rect.y + 14 - py);
                LOG_INFO("[P0DIAG] monster '%s' hp=%d/%d dist=%.0f pos=(%.0f,%.0f)",
                    m->name.c_str(), m->combat.current_hp, m->combat.max_hp,
                    d, m->entity.rect.x, m->entity.rect.y);
            }
            // P0-M1 诊断: 全部门 tile 状态 (经公共 API, 不触私有 _tiles)
            if (game_map) {
                for (int y = 0; y < game_map->height; y++)
                    for (int x = 0; x < game_map->width; x++)
                        if (game_map->is_door(x, y))
                            LOG_INFO("[P0DIAG] door@(%d,%d) state=%d walkable=%d",
                                x, y, (int)game_map->door_state_at(x, y),
                                (int)game_map->is_walkable(x, y));
                auto [stx, sty] = game_map->pixel_to_tile(px, py);
                LOG_INFO("[P0DIAG] player tile=(%d,%d) type=%d walkable=%d",
                    stx, sty, (int)game_map->tile_at(stx, sty),
                    (int)game_map->is_walkable(stx, sty));
            }
        }
        _sim_game_timeout = true;   // M2-A: 结算归 TIMEOUT_GAME
        _collect_sim_stats();
        return;
    }

    if (!player->combat.is_alive) {
        // G5.6: sim 模式短路死亡流程 — _collect_sim_stats 内部已处理重启/退出
        if (_sim_mode) {
            _collect_sim_stats();
            return;
        }
        LOG_INFO("玩家死亡! 第%d层 Lv%d - 存档已保留", current_floor, player->level);
        // M1-D1: 死亡也保存镜像学习 — "你怎么死的, 它都会记住"
        // (F15 镜像战死亡时 agent 仍在, 本局学习成果随死亡落盘)
        {
            std::vector<float> fresh_alpha, fresh_beta;
            _boss.export_mirror_memory(fresh_alpha, fresh_beta);
            if (!fresh_alpha.empty()) {
                _mirror_mem_alpha = fresh_alpha;
                _mirror_mem_beta = fresh_beta;
                std::vector<bool> spr, spd;
                if (game_map) for (auto& sr : game_map->special_rooms) {
                    spr.push_back(sr.triggered);
                    spd.push_back(sr.discovered);
                }
                std::unordered_map<std::string, int> rcm;
                SaveManager::save_game(SaveManager::active_slot(), player.get(),
                    current_floor, max_unlocked_floor, _dungeon_seed,
                    spr, spd, rcm, _gameplay.quest_mgr.export_states(),
                    _mirror_mem_alpha, _mirror_mem_beta, (float)game_time);
                LOG_INFO("[MIRROR] 死亡保留镜像记忆 (%zu 桶)", fresh_alpha.size());
            }
        }
        // G10.9-B1: end_run 只在 on_player_dead 内部执行一次
        // (旧代码这里再直接调 g_meta.end_run → runs 双计数/奖励双发)
        _gameplay.on_player_dead(current_floor, player->level, player.get());
        _flow.on_player_dead();
        return;
    }

    // Batch 3F: Challenge Room tick (runs in both DUNGEON and CHALLENGE_ARENA)
    if (player && game_map && _world_mode == WorldMode::DUNGEON &&
        _challenge.phase() != ChallengePhase::INACTIVE &&
        _challenge.phase() != ChallengePhase::CLEARED) {
        int room_idx = 0;
        for (int i = 0; i < (int)game_map->special_rooms.size(); i++) {
            if (game_map->special_rooms[i].type == SpecialRoomType::CHALLENGE)
                { room_idx = i; break; }
        }
        int pity_before = _challenge_pity_streak;
        _challenge.tick(dt, game_map.get(), player.get(), monsters,
                        current_floor, _dungeon_seed, room_idx, ground_items,
                        &_challenge_pity_streak);
        if (_challenge_pity_streak != pity_before)
            g_meta.set_challenge_pity_streak(_challenge_pity_streak);
        if (_challenge.phase() == ChallengePhase::ARMED) {
            auto& sr = game_map->special_rooms[room_idx];
            game_map->lock_room_doors({});
            _challenge.on_doors_locked();
        }
        if (_challenge.phase() == ChallengePhase::CLEARED) {
            game_map->open_room_doors({});
        }
    }

    if (_challenge.phase() == ChallengePhase::PORTAL_ACTIVE ||
        _challenge.phase() == ChallengePhase::CLEARED ||
        _world_mode == WorldMode::CHALLENGE_ARENA) {
        _portal_pulse_timer += dt;
    }
    if (_teleport_fade_timer > 0) {
        _teleport_fade_timer -= dt;
        if (_teleport_fade_timer <= 0) {
            _teleport_fade_timer = 0;
            if (_portal_fade_in && _world_mode == WorldMode::CHALLENGE_ARENA) {
                int cx = _challenge.room_rx() + _challenge.room_rw() / 2;
                int cy = _challenge.room_ry() + _challenge.room_rh() / 2;
                auto [px, py] = _arena_map->tile_to_pixel(cx, cy);
                player->entity.position = {(float)px, (float)py};
                player->entity.sync_rect();
                _arena_map->update_fov(cx, cy, _fov_radius);
                _challenge.set_phase_for_test(ChallengePhase::ARMED);
            }
        }
    }

    if (_world_mode == WorldMode::CHALLENGE_ARENA) {
        int pity_before = _challenge_pity_streak;
        _challenge.tick(dt, game_map.get(), player.get(), monsters,
                        current_floor, _dungeon_seed, 0, ground_items,
                        &_challenge_pity_streak);
        if (_challenge_pity_streak != pity_before)
            g_meta.set_challenge_pity_streak(_challenge_pity_streak);
        if (_challenge.phase() == ChallengePhase::ARMED) {
            _challenge.on_doors_locked();
        }
        if (_challenge.phase() == ChallengePhase::CLEARED) {
            _return_portal_tx_arena = _challenge.return_portal_tx();
            _return_portal_ty_arena = _challenge.return_portal_ty();
        }
        // Player movement/attack/input — monsters is already arena monsters
        _player_ctrl.tick(dt);  // includes monster AI (gated by time_stop_remaining)
        // Weapon cooldown tick + specials + projectiles
        if (player) {
            player->weapon.tick(dt);
            if (player->weapon.range_indicator_timer > 0.0f)
                player->weapon.range_indicator_timer -= dt;
            std::vector<Monster*> mlist;
            for (auto& m : monsters) if (m && m->combat.is_alive) mlist.push_back(m.get());
            auto spec_results = WeaponExecutor::tick_specials(player.get(), mlist, dt);
            auto proj_results = WeaponExecutor::tick_projectiles(projectiles, mlist, dt, game_map.get());
            for (auto& r : spec_results) {
                _boss.dmg_done += r.damage;
                _presentation.spawn_damage(r.hit_point.x, r.hit_point.y, r.damage,
                    r.is_crit ? Color{255,220,30,255} : Color{100,200,255,255}, 0.5f);
                // G10.5-B B4: 特殊段每击命中 VFX (原 5/10 连击零反馈)
                VFXServer svfx;
                svfx.hit_flash(r.hit_point.x, r.hit_point.y, 12.0f);
                for (auto& e : svfx.effects) active_effects.push_back(e);
            }
            for (auto& r : proj_results) {
                _boss.dmg_done += r.damage;
                _presentation.spawn_damage(r.hit_point.x, r.hit_point.y, r.damage,
                    Color{255,180,50,255}, 0.5f);
            }
        }
        _cleanup_dead_monsters();
        _apply_pending_damage();
        // VFX tick — must run in arena too or effects stick forever
        for (auto& fx : active_effects) fx.elapsed += dt;
        active_effects.erase(std::remove_if(active_effects.begin(), active_effects.end(),
            [](auto& fx) { return fx.elapsed >= fx.duration + fx.start_delay; }),
            active_effects.end());
        if (player && game_map) {
            auto [tx, ty] = game_map->pixel_to_tile(
                player->entity.rect.x + player->entity.rect.width / 2,
                player->entity.rect.y + player->entity.rect.height / 2);
            game_map->update_fov(tx, ty, _fov_radius);
        }
        player->combo.tick(dt);
        _presentation.tick(dt);
        return;
    }

    // D6 Step7: 玩家移动/交互/怪物AI — 委托给 PlayerController
    _player_ctrl.tick(dt);

    // Batch 2C: Room Encounter 状态机 (进有怪房→封门→清房→开门)
    if (player && game_map) _room_mgr.tick(game_map.get(), player.get(), monsters);

    // Door animation update
    DoorRenderer::inst().update(dt);

    // Phase 1: FOV — 玩家跨 tile 时更新
    if (player && game_map) {
        auto [tx, ty] = game_map->pixel_to_tile(
            player->entity.rect.x + player->entity.rect.width / 2,
            player->entity.rect.y + player->entity.rect.height / 2);
        // Batch 3F: detect player entering challenge room
        if (_challenge.phase() == ChallengePhase::ARMED) {
            SpecialRoom* ch = nullptr;
            for (auto& sr : game_map->special_rooms)
                if (sr.type == SpecialRoomType::CHALLENGE) { ch = &sr; break; }
            if (ch && tx >= ch->rx && tx < ch->rx + ch->rw &&
                ty >= ch->ry && ty < ch->ry + ch->rh) {
                _challenge.on_player_entered();
            }
        }
        if (tx != _last_player_tile_x || ty != _last_player_tile_y) {
            _last_player_tile_x = tx;
            _last_player_tile_y = ty;
            game_map->update_fov(tx, ty, _fov_radius);
            // G11.2: 探索足迹 — 跨 tile 即踩下一枚脚印 (纯渲染)
            if (game_map->is_walkable(tx, ty))
                game_map->mark_footstep(tx, ty);
            // G11.2: 情绪 vignette — 探索进度 (地板总数近似)
            int floor_tiles = game_map->width * game_map->height;
            _ambient.set_mood(
                (float)game_map->explored_tile_count() / (float)floor_tiles * 3.0f,
                _kill_streak_timer > 0 ? 1.0f : 0.0f);
        }
    }

    // G11.2: 氛围层 tick — 粒子飘动 + 足迹渐隐 + 击杀势头衰减
    if (game_map) {
        _ambient.update(dt, game_map->pixel_width, game_map->pixel_height,
                        _cam_x, _cam_y, 0, 0);
        game_map->tick_footsteps(dt);
        if (_kill_streak_timer > 0) _kill_streak_timer -= dt;
    }

    // Boss 位置 + FOV — 始终追踪
    if (player && game_map) {
        Monster* b = _get_boss();
        if (b) {
            auto [btx, bty] = game_map->pixel_to_tile(b->entity.position.x, b->entity.position.y);
            _boss_last_known = {btx, bty};
            game_map->update_boss_fov(btx, bty, FOV_RADIUS_DEFAULT);
        }
    }

    // Phase 3: M 键 — 小地图开关
    if (IsKeyPressed(KEY_M)) _show_minimap = !_show_minimap;

    // Q3.2: sim 自动装备 — 搜刮到的武器/护甲直接换上 (总值更高才换)
    if (_sim_mode && player && !player->inventory.items.empty()) {
        for (int i = 0; i < (int)player->inventory.items.size(); i++) {
            auto* eq = dynamic_cast<EquipmentItem*>(player->inventory.items[i].get());
            if (!eq) continue;
            auto cur = player->inventory.equipped.find(eq->slot);
            // Q3.2-fix: equipped 槽位可能为 nullptr (构造预置) — 空槽视为直接换上
            bool better = true;
            if (cur != player->inventory.equipped.end() && cur->second) {
                better = (eq->atk_bonus + eq->pdef_bonus + eq->mdef_bonus) >
                         (cur->second->atk_bonus + cur->second->pdef_bonus
                          + cur->second->mdef_bonus);
            }
            if (better) player->inventory.equip(i, player.get());
        }
    }

    // Q3.3: sim 喝药 — 决策为 use_potion 时消耗背包首个治疗药水 (真玩家战斗中喝药模拟)
    if (_sim_mode && player && _sim_ai &&
        _sim_ai->last_best_action() == "use_potion") {
        for (int i = 0; i < (int)player->inventory.items.size(); i++) {
            auto* c = dynamic_cast<ConsumableItem*>(player->inventory.items[i].get());
            if (c && c->effect_type == "heal") {
                player->inventory.use_item(i, player.get());
                break;
            }
        }
    }

    // VFX 更新
    for (auto& fx : active_effects) fx.elapsed += dt;
    active_effects.erase(std::remove_if(active_effects.begin(), active_effects.end(),
        [](auto& fx) { return fx.elapsed >= fx.duration + fx.start_delay; }),
        active_effects.end());

    // G9.1: weapon tick + specials + projectiles
    player->weapon.tick(dt);
    if (player->weapon.range_indicator_timer > 0.0f)
        player->weapon.range_indicator_timer -= dt;
    {
        std::vector<Monster*> mlist;
        for (auto& m : monsters) mlist.push_back(m.get());
        auto spec_results = WeaponExecutor::tick_specials(player.get(), mlist, dt);
        auto proj_results = WeaponExecutor::tick_projectiles(projectiles, mlist, dt, game_map.get());

        // G9: spear stage-3 lightning VFX on each rapid hit
        if (player->weapon.runtime().special.active
            && player->weapon.weapon_type() == WeaponType::SPEAR) {
            float px = player->entity.rect.x + player->entity.rect.width/2;
            float py = player->entity.rect.y + player->entity.rect.height/2;
            VFXServer svfx;
            for (auto& r : spec_results) {
                // Heavy zigzag lightning (5 branches, thicker, brighter)
                svfx.lightning(px, py, r.hit_point.x, r.hit_point.y, 5,
                    {80,160,255,255}, 0.30f);
                svfx.beam(px, py, r.hit_point.x, r.hit_point.y, {100,180,255,200}, 0.28f);
                svfx.ring(r.hit_point.x, r.hit_point.y, 20.0f,
                    {80,180,255,220}, 2, 0.25f);
                svfx.spark_burst(r.hit_point.x, r.hit_point.y, 5,
                    {100,200,255,220}, 0.22f);
                // G9: electrified debuff on spear rapid hits
                apply_buff(r.target, "electrified", 1);
            }
            for (auto& e : svfx.effects) active_effects.push_back(e);
        }

        // G10.5-B B4: 非长矛特殊段 (双节棍 5 连击) 每击命中 VFX
        if (player->weapon.runtime().special.active
            && player->weapon.weapon_type() == WeaponType::NUNCHAKU) {
            VFXServer nvfx;
            for (auto& r : spec_results) {
                nvfx.hit_flash(r.hit_point.x, r.hit_point.y, 12.0f);
                nvfx.spark_burst(r.hit_point.x, r.hit_point.y, 4, {255,200,80,220}, 0.20f);
            }
            for (auto& e : nvfx.effects) active_effects.push_back(e);
        }

        for (auto& r : spec_results) {
            _boss.dmg_done += r.damage;
            _presentation.spawn_damage(r.hit_point.x, r.hit_point.y, r.damage,
                r.is_crit ? Color{255,220,30,255} : Color{100,200,255,255}, 0.5f);
            // P0-A: 不在此处调用 on_monster_killed，委托下帧 cleanup_dead_monsters 统一处理
        }
        for (auto& r : proj_results) {
            _boss.dmg_done += r.damage;
            _presentation.spawn_damage(r.hit_point.x, r.hit_point.y, r.damage,
                Color{255,180,50,255}, 0.5f);
            // P0-A: 同上
        }
    }

    // D2: tick enemy projectiles (MONSTER/ENVIRONMENT owner → hit player)
    // 时停期间世界冻结 — 敌方弹体不飞行不结算
    projectiles.for_each([&](Projectile& p, int) {
        if (!p.alive) return;
        if (time_stop_remaining > 0) return;
        if (p.owner != (int)ProjectileOwner::MONSTER
            && p.owner != (int)ProjectileOwner::ENVIRONMENT) return;
        // Warning phase countdown only
        if (p.active_time < 0.0f) {
            p.active_time += dt;
            if (p.active_time >= 0.0f) p.active_time = 0.0f; // just became ACTIVE
            return;
        }
        // Active phase
        p.active_time += dt;
        if (p.active_time >= p.lifetime) { p.alive = false; return; }
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        // 墙体碰撞 — pierce_walls=false 的弹幕碰墙销毁
        if (!p.pierce_walls && game_map) {
            auto [wtx, wty] = game_map->pixel_to_tile(p.pos.x, p.pos.y);
            if (!game_map->is_walkable(wtx, wty)) { p.alive = false; return; }
        }
        // D2: AOE 用 warning_radius, 点弹用宽容半径
        float hit_radius = (p.owner == (int)ProjectileOwner::ENVIRONMENT)
            ? p.warning_radius : kProjectileHitRadius;
        // 收官: 敌方弹体命中木桶 → 点燃 (点弹销毁, AOE 弹体保留)
        if (game_map) {
            auto [btx, bty] = game_map->pixel_to_tile(p.pos.x, p.pos.y);
            if (auto* ao = game_map->get_arena_at(btx, bty)) {
                if (ao->active && ao->type == ArenaObjectType::EXPLOSIVE_BARREL && ao->timer <= 0.0f) {
                    ao->timer = kBarrelFuse;
                    if (!p.piercing) p.alive = false;
                    return;
                }
            }
        }
        if (CheckCollisionCircleRec(p.pos, hit_radius, player->entity.rect)) {
            int dmg = calculate_damage(p.damage,
                player->combat.get_effective_defense((AttackType)p.damage_type),
                (AttackType)p.damage_type);
            player->combat.take_damage(dmg);
            _presentation.damage_floats.push_back({
                player->entity.rect.x + player->entity.rect.width/2,
                player->entity.rect.y - 12, 0.6f, 0.6f, dmg,
                dmg_color_for(dmg, p.damage_type != 0, false)
            });
            _presentation.trigger_shake(dmg > 20 ? 8.0f : 3.0f);
            if (!p.piercing) p.alive = false;
        }
    });
    // G11: 池回收取代 erase-remove, 免每帧 O(n) 搬移
    projectiles.release_if([](const Projectile& p) { return !p.alive; });

    // D4.6 Step3: FlowDirector tick
    _gameplay.flow.tick(dt);

    // B10: 房间消息计时器
    if (_presentation.room_msg_timer > 0) _presentation.room_msg_timer -= dt;

    // v1.6-B1: 镜像晋升横幅计时器
    if (_mirror_banner_timer > 0) _mirror_banner_timer -= dt;

    // Batch 3H: Gamble result timer decay
    if (gamble_result_timer > 0) gamble_result_timer -= dt;

    // D4 Step2: 事件演出 tick
    _tick_event_ui(dt);
    // D4 Step4: 对话计时器
    _update_dialogue(dt);
    // D4 Step5.2: QuestManager 自动推进
    _gameplay.quest_mgr.update(_gameplay.world_state, _gameplay.story);

    // D4 Step3: 楼层入场计时器
    if (_presentation.floor_intro_active) {
        _presentation.floor_intro_timer -= dt;
        _presentation.floor_intro_fade = std::min(1.0f, _presentation.floor_intro_fade + dt * 2.5f);
        if (_presentation.floor_intro_timer <= 0) _presentation.floor_intro_active = false;
    }
    // D4 Step3: 章节入场计时器
    if (_presentation.chapter_intro_active) {
        _presentation.chapter_intro_timer -= dt;
        if (_presentation.chapter_intro_timer <= 0) _presentation.chapter_intro_active = false;
    }
    // D4 Step3: 随机旁白 (25-40秒间隔, 不在事件中)
    if (!_presentation.floor_intro_active && !_presentation.chapter_intro_active && !_is_event_running()
        && !inventory_open && state == GameState::PLAYING && !is_boss_floor(current_floor)) {
        _gameplay.narr_state.narration_timer -= dt;
        if (_gameplay.narr_state.narration_timer <= 0) {
            // D4 Step5.4: WorldReaction overlay 优先覆盖随机旁白
            const char* nar = StoryDirector::world_flag_narration(_gameplay.world_state);
            if (!nar) nar = pick_random_narration(current_floor, _gameplay.narr_state);
            if (nar) { _presentation.room_msg = nar; _presentation.room_msg_timer = 3.0f; }
            _gameplay.narr_state.narration_timer = 25.0f + (float)(rng() % 15);
        }
        // D4 Step5.1: StoryDirector update + 世界事件 (~30秒)
        _gameplay.story.update(dt);
        if (_gameplay.story.should_trigger_story()) {
            // D4 Step5.4: WorldReaction 世界事件优先
            const char* we = g_reactions.current_world_event(_gameplay.world_state, _gameplay.story.stage());
            if (!we) we = _gameplay.story.tick_world_event(_gameplay.world_state);
            if (we) { _presentation.room_msg = we; _presentation.room_msg_timer = 3.0f; }
        }

        // D4.6 Step3: FlowDirector自动补救 (超过阈值触发动态内容)
        const char* suggest = _gameplay.flow.auto_spawn_suggestion();
        if (suggest && game_map && !monsters.empty()) {
            if (strcmp(suggest, "AUTO_STORY") == 0) {
                const char* we = _gameplay.story.tick_world_event(_gameplay.world_state);
                if (we) { _presentation.room_msg = we; _presentation.room_msg_timer = 3.0f; _gameplay.flow.mark_story(); }
            } else if (strcmp(suggest, "AUTO_REWARD") == 0) {
                // 在玩家附近生成药水
                auto [tx, ty] = game_map->pixel_to_tile(
                    player->entity.rect.x + player->entity.rect.width/2,
                    player->entity.rect.y + player->entity.rect.height/2);
                auto p = std::make_shared<ConsumableItem>("探索发现", Rarity::RARE, "heal", 25);
                ground_items.push_back({p, tx + 2, ty + 1});
                _presentation.room_msg = "你在角落发现了一件物品。";
                _presentation.room_msg_timer = 2.0f;
                _gameplay.flow.mark_reward();
            } else if (strcmp(suggest, "AUTO_PATROL") == 0) {
                // 在玩家附近生成巡逻怪
                auto [tx, ty] = game_map->pixel_to_tile(
                    player->entity.rect.x + (rng()%10-5) * TILE_SIZE,
                    player->entity.rect.y + (rng()%10-5) * TILE_SIZE);
                auto [px, py] = game_map->tile_to_pixel(tx, ty);
                if (game_map->is_walkable(tx, ty)) {
                    auto* m = spawn_monster(px, py, (rng()%3==0)?"orc":"slime");
                    m->combat.max_hp = (int)(m->combat.max_hp * g_growth.hp_scale(current_floor));
                    m->combat.current_hp = m->combat.max_hp;
                    m->combat.attack = (int)(m->combat.attack * g_growth.atk_scale(current_floor));
                    monsters.emplace_back(m);
                    _gameplay.flow.mark_combat();
                }
            } else if (strcmp(suggest, "AUTO_ELITE") == 0) {
                auto [tx, ty] = game_map->pixel_to_tile(
                    player->entity.rect.x + (rng()%8-4) * TILE_SIZE,
                    player->entity.rect.y + (rng()%8-4) * TILE_SIZE);
                if (game_map->is_walkable(tx, ty)) {
                    auto [px, py] = game_map->tile_to_pixel(tx, ty);
                    auto* m = spawn_monster(px, py, "elite");
                    m->combat.max_hp = (int)(m->combat.max_hp * g_growth.elite_scale(current_floor));
                    m->combat.current_hp = m->combat.max_hp;
                    monsters.emplace_back(m);
                    _presentation.room_msg = "一支精英巡逻队出现了！";
                    _presentation.room_msg_timer = 2.0f;
                    _gameplay.flow.mark_combat();
                }
            }
        }
    }

    // D2: Combo 窗口衰减 — 超过 WINDOW 未命中则重置
    player->combo.tick(dt);

    // D6 Step5: Presentation tick (shake/freeze/damage/message/intro)
    _presentation.tick(dt);
}

// ============================================================
// 输入处理
// ============================================================
// ── Batch 2B: 接触开门回调 ──
void GameScene::on_door_opened() {
    if (player && game_map) {
        auto [tx, ty] = game_map->pixel_to_tile(
            player->entity.rect.x + player->entity.rect.width / 2,
            player->entity.rect.y + player->entity.rect.height / 2);
        _last_player_tile_x = tx;
        _last_player_tile_y = ty;
        game_map->update_fov(tx, ty, _fov_radius);
    }
}

// ── Batch 2C: Room Encounter 通知 ──
void GameScene::show_room_message(const char* msg) {
    if (msg) _presentation.show_message(msg, 1.5f);
}

// G10.8-B4: 首次提示转发 — 教学性提示用更长的 4.5s
void GameScene::show_hint(const char* msg, float duration) {
    if (msg) _presentation.show_message(msg, duration);
}


// ── G4.5: Replay recording control ──
void GameScene::start_recording(uint32_t seed) {
    std::vector<ModSnapshot> mods;
    _recorder.start(seed, "0.8.5", mods);
    seed_rng(seed);
}

void GameScene::start_replay(const std::string& path) {
    if (_replay_player.load(path)) {
        _replay_player.start();
        seed_rng(_replay_player.replay_seed());
    }
}

bool GameScene::_is_action_just_pressed(const InputMap& input, const char* name) {
    // G5.6/G8.1: SimAI / BTAgent drives the player
    if (_sim_mode) {
        // Q3.1: 模态 UI 优先 — 事件/对话由 AI 直接确认推进, 避免死锁
        if (_is_event_running())
            return strcmp(name, "confirm") == 0;
        if (_dialogue.active)
            return strcmp(name, "confirm") == 0 || strcmp(name, "attack") == 0;
        std::vector<Monster*> mlist;
        for (auto& m : monsters) mlist.push_back(m.get());
        bool boss_intro = (state == GameState::BOSS_INTRO);
        if (_use_bt_agent && _sim_bt)
            return _sim_bt->is_action_just_pressed(name, player.get(), mlist,
                game_map.get(), stairs_active, boss_intro);
        else if (_sim_ai)
            return _sim_ai->is_action_just_pressed(name, player.get(), mlist,
                game_map.get(), stairs_active, boss_intro);
    }
    if (_replay_player.is_active())
        return _replay_player.is_action_just_pressed(name);
    bool pressed = input.is_action_just_pressed(name);
    if (pressed && _recorder.is_active())
        _recorder.record(name);
    return pressed;
}

void GameScene::_tick_replay_hash() {
    if (_recorder.is_active() || _replay_player.is_active()) {
        uint64_t prev = _recorder.file().hash_chain.empty() ? 0
            : _recorder.file().hash_chain.back();
        uint64_t h = compute_state_hash(prev, *player, current_floor, monsters);
        if (_recorder.is_active()) _recorder.record_hash(h);
    }
}

// _input / debug / event / dialogue — delegated to GameSceneInput
void GameScene::_input(const InputMap& input) {    // G10.1: Element select input
    if (element_select_active) {
        if (input.is_action_just_pressed("move_left"))
            element_select_cursor = (element_select_cursor + 2) % 3;
        if (input.is_action_just_pressed("move_right"))
            element_select_cursor = (element_select_cursor + 1) % 3;
        if (input.is_action_just_pressed("attack") || input.is_action_just_pressed("pickup")) {
            static const ElementType choices[] = {
                ElementType::FIRE, ElementType::ICE, ElementType::POISON
            };
            player->element.select(choices[element_select_cursor]);
            element_select_active = false;
            // Continue new_game flow that was interrupted for element selection
            auto sk = random_active_skill({}, true);
            player->skills.learn(std::move(sk));
            g_meta.load();
            _gameplay.run_stats = RunSummary{};
            player->skills.apply_all_passives(player.get());
            current_floor = 1;
            max_unlocked_floor = 1;
            enter_floor(1);
            _presentation.set_build_theme(BuildType::BERSERKER);
            if (g_replay_mode && !g_replay_path.empty())
                start_replay(g_replay_path);
            else if (g_record_mode && !g_record_path.empty())
                start_recording(_dungeon_seed);
            return;
        }
    }

    // G4.5: frame tick
    if (_recorder.is_active()) _recorder.tick();
    if (_replay_player.is_active()) _replay_player.tick();
    // G5.6/G8.1: SimAI / BTAgent tick
    if (_sim_mode) {
        if (_use_bt_agent && _sim_bt) _sim_bt->tick();
        else if (_sim_ai) _sim_ai->tick();
    }

    if (!element_select_active) _input_handler.handle_input(input);

    // G4.5: hash computation
    _tick_replay_hash();
}

// ── G5.6: Simulation stats collection ──
void GameScene::_collect_sim_stats() {
    RunResult s;
    s.seed = _dungeon_seed;
    s.victory = (current_floor >= MAX_FLOORS) && player->combat.is_alive;  // Q3.7: 死在F15不算通关
    s.floor_reached = current_floor;
    s.turns = (int)(game_time * 60); // approximate frames → turns
    s.damage_dealt = (int)_sim_dmg_dealt;  // Q3.2: 真实累计 (替代 kills*10 估算)
    s.damage_taken = (int)_sim_dmg_taken;  // Q3.2: 真实累计 (含毒池环境伤害)
    s.heal_total = (int)_sim_heal_total;   // 泉水/药水/吸血等治疗量
    s.enemies_killed = _gameplay.run_stats.total_kills;
    s.elite_kills = _gameplay.run_stats.elite_kills;
    s.bosses_killed = _gameplay.run_stats.bosses_killed;
    s.relics_collected = (int)player->relics.size();
    s.equipment_count = (int)std::count_if(player->inventory.equipped.begin(),
        player->inventory.equipped.end(), [](const auto& kv) { return kv.second != nullptr; });
    s.build_type = (int)calculate_build(player.get()).identify();
    s.build_name = calculate_build(player.get()).build_name();
    for (auto& r : player->relics) s.relics_picked.push_back(r.id);

    // ── M2-A: 结果分类 — 不再所有失败混成一个 victory=false ──
    if (s.victory) {
        s.outcome = RunOutcome::VICTORY;
    } else if (_sim_game_timeout) {
        s.outcome = RunOutcome::TIMEOUT_GAME;
    } else if (_sim_wall_timeout) {
        s.outcome = RunOutcome::TIMEOUT_WALL;
    } else if (!player->combat.is_alive) {
        s.death_cause = player->combat.last_damage_source;
        s.death_floor = current_floor;
        // 死因分类: 源前缀/楼层语义 → 4 类
        if (s.death_cause.rfind("dot:", 0) == 0)
            s.outcome = RunOutcome::DEATH_DOT;
        else if (s.death_cause.rfind("env:", 0) == 0)
            s.outcome = RunOutcome::DEATH_ENVIRONMENT;
        else if (is_boss_floor(current_floor) && _get_boss())
            s.outcome = RunOutcome::DEATH_BOSS;
        else
            s.outcome = RunOutcome::DEATH_MONSTER;
    } else {
        s.outcome = RunOutcome::STUCK_RECOVERED;  // 存活但被强制结算
    }

    // ── M2-D: 构筑/状态快照 (死亡瞬间) ──
    if (player->weapon.current_def()) {
        s.weapon_type_final = (int)player->weapon.current_def()->type;
        s.weapon_id_final = player->weapon.current_weapon_id();
    } else {
        s.weapon_id_final = "fist_basic";
    }
    s.element_type = (int)player->element.type;
    s.level_final = player->level;
    s.relics_held = (int)player->relics.size();
    s.buffs_held = (int)player->active_buffs.size();
    s.gold_earned = player->gold;

    // ── M2-C: 行为指标 ──
    if (game_map) {
        for (auto& sr : game_map->special_rooms)
            if (sr.discovered) s.rooms_discovered++;
    }
    s.items_picked = _sim_items_picked;
    s.combat_frames = _sim_combat_frames;
    s.stuck_teleports = sim_stuck_teleports;   // M2-C: sim_ai 诊断计数 (读后清零)    s.stuck_rotations = sim_stuck_rotations;
    s.loot_watchdog_descends = sim_stuck_loot_wd;
    sim_stuck_teleports = 0; sim_stuck_rotations = 0; sim_stuck_loot_wd = 0;
    sim_stuck_watchdog = 0;
    // G13: 卡死累计诊断 — 看门狗阈值调参依据
    if (_sim_ai) LOG_INFO("[SIM-DIAG] 累计卡死=%.1fs", _sim_ai->stuck_total());
    // G13: 动作分布诊断 — 定位 AI 实际行为 (none/move/atk/skill/pick/desc/pot/oth)
    {
        int tot = 0; for (int i = 0; i < 8; i++) tot += sim_action_counts[i];
        if (tot > 0) LOG_INFO("[SIM-DIAG] 动作: none=%d%% move=%d%% atk=%d%% skill=%d%% pick=%d%% desc=%d%% pot=%d%% oth=%d%%",
            sim_action_counts[0]*100/tot, sim_action_counts[1]*100/tot, sim_action_counts[2]*100/tot,
            sim_action_counts[3]*100/tot, sim_action_counts[4]*100/tot, sim_action_counts[5]*100/tot,
            sim_action_counts[6]*100/tot, sim_action_counts[7]*100/tot);
        for (int i = 0; i < 8; i++) sim_action_counts[i] = 0;
    }
    // G14: 移动分支归因 — recovery/loot/room/approach/stand 占比
    {
        int tot = 0; for (int i = 0; i < 5; i++) tot += sim_move_branch[i];
        if (tot > 0) LOG_INFO("[SIM-DIAG] 移动分支: rec=%d%% loot=%d%% room=%d%% appr=%d%% stand=%d%%",
            sim_move_branch[0]*100/tot, sim_move_branch[1]*100/tot, sim_move_branch[2]*100/tot,
            sim_move_branch[3]*100/tot, sim_move_branch[4]*100/tot);
    }
    // G14: BFS 失败率 — appr 分支中 step==-1 的帧数
    {
        int appr_tot = sim_move_branch[3] + sim_bfs_fail;
        if (appr_tot > 0)
            LOG_INFO("[SIM-DIAG] bfs_fail=%d appr=%d fail=%.1f%%",
                sim_bfs_fail, appr_tot, 100.0f * sim_bfs_fail / appr_tot);
    }
    // G14: 无怪随机游走 — 解释 move 与 appr 的差额
    LOG_INFO("[SIM-DIAG] noenemy=%d appr=%d", sim_move_noenemy, sim_move_branch[3]);
    for (int i = 0; i < 5; i++) sim_move_branch[i] = 0;
    sim_bfs_fail = 0;
    // G14: stairs 分支归因 — 定位 AI 为何走楼梯却不 descend
    {
        int tot = sim_stairs[0];
        if (tot > 0) LOG_INFO("[SIM-DIAG] stairs: total=%d descend=%d%% move=%d%%",
            tot, sim_stairs[1]*100/tot, sim_stairs[2]*100/tot);
        for (int i = 0; i < 3; i++) sim_stairs[i] = 0;
    }
    sim_move_noenemy = 0;
    // G14: 卡死采样摘要 — 最近怪距离分布 + AI 位置轨迹
    if (sim_stuck_sample_count > 0) {
        int n = std::min(sim_stuck_sample_count, 8);
        for (int i = 0; i < n; i++) {
            int k = (sim_stuck_sample_count - 1 - i) % 64;
            LOG_INFO("[SIM-DIAG] stuck#%d: ai=(%d,%d) r=%d mdist=%dpx alive=%d open=%d hz=%d door=%d mon=(%d,%d) r=%d locked=%d monOpen=%d",
                i, sim_stuck_sample[k][0], sim_stuck_sample[k][1], sim_stuck_sample[k][9],
                sim_stuck_sample[k][2], sim_stuck_sample[k][3], sim_stuck_sample[k][4],
                sim_stuck_sample[k][5], sim_stuck_sample[k][6],
                sim_stuck_sample[k][7], sim_stuck_sample[k][8], sim_stuck_sample[k][10],
                sim_stuck_sample[k][11], sim_stuck_sample[k][12]);
        }
    }
    LOG_INFO("[SIM-DIAG] rot_blocked=%d bfs_stuck_hit=%d bfs_stuck_fail=%d",
        sim_rot_blocked, sim_stuck_bfs_hit, sim_stuck_bfs_fail);
    LOG_INFO("[SIM-DIAG] map=%dx%d stuck_tp=%d tp_attempts=%d", game_map->width, game_map->height,
        s.stuck_teleports, sim_tp_attempts);
    if (sim_stuck_sample_count > 0 && player) {
        // 连通性报告: 从玩家 tile flood fill, 统计可达域是否含怪/楼梯
        int w = game_map->width, h = game_map->height;
        std::vector<char> reach((size_t)w * h, 0);
        std::queue<int> q;
        auto [px0, py0] = game_map->pixel_to_tile(
            player->entity.rect.x + player->entity.rect.width/2,
            player->entity.rect.y + player->entity.rect.height/2);
        if (px0>=0 && px0<w && py0>=0 && py0<h) { reach[py0*w+px0]=1; q.push(py0*w+px0); }
        while (!q.empty()) {
            int cur = q.front(); q.pop();
            int cx = cur % w, cy = cur / w;
            for (int d = 0; d < 4; d++) {
                int ndx = cx + (d==2?-1:d==3?1:0), ndy = cy + (d==0?-1:d==1?1:0);
                if (ndx<0||ndx>=w||ndy<0||ndy>=h) continue;
                int ni = ndy*w+ndx;
                if (reach[ni]) continue;
                DoorState ds = game_map->door_state_at(ndx, ndy);
                if (ds == DoorState::LOCKED || ds == DoorState::SEALED) continue;
                if (ds == DoorState::CLOSED) { reach[ni]=1; q.push(ni); continue; }
                if (game_map->tile_at(ndx,ndy) != TileType::WALL) { reach[ni]=1; q.push(ni); }
            }
        }
        bool monOK = false, stairsOK = false;
        for (auto& m : monsters) {
            if (!m || !m->combat.is_alive) continue;
            auto [mtx2, mty2] = game_map->pixel_to_tile(
                m->entity.rect.x + m->entity.rect.width/2,
                m->entity.rect.y + m->entity.rect.height/2);
            if (mtx2>=0&&mtx2<w&&mty2>=0&&mty2<h && reach[mty2*w+mtx2]) monOK = true;
        }
        for (int y = 0; y < h && !stairsOK; y++)
            for (int x = 0; x < w; x++)
                if (game_map->tile_at(x,y)==TileType::STAIRS_DOWN && reach[y*w+x]) stairsOK = true;
        LOG_INFO("[SIM-DIAG] reach=%d 怪可达=%d 楼梯可达=%d",
            std::count(reach.begin(), reach.end(), (char)1), (int)monOK, (int)stairsOK);
        for (int y = 0; y < game_map->height; y++) {
            std::string row;
            for (int x = 0; x < game_map->width; x++) {
                char c = '.';
                TileType tt = game_map->tile_at(x, y);
                if (tt == TileType::WALL) c = '#';
                else if (tt == TileType::LAVA) c = '~';
                else if (tt == TileType::STAIRS_DOWN) c = '>';
                else if (tt == TileType::DOOR) {
                    DoorState ds = game_map->door_state_at(x, y);
                    c = (ds == DoorState::OPEN) ? 'D' : (ds == DoorState::CLOSED) ? 'd'
                       : (ds == DoorState::LOCKED) ? 'L' : 'S';
                }
                row.push_back(c);
            }
            LOG_INFO("[MAP] %s", row.c_str());
        }
    }
    sim_stuck_sample_count = 0;
    sim_rot_blocked = 0;
    sim_stuck_bfs_hit = 0; sim_stuck_bfs_fail = 0;

    // ── M2: enemies_fought 修复 — 字段一直存在但从未填充 (P1 审计) ──
    for (auto& m : monsters) {
        if (!m) continue;
        bool dup = false;
        for (auto& n : s.enemies_fought) if (n == m->name) { dup = true; break; }
        if (!dup) s.enemies_fought.push_back(m->name);
    }

    auto& sim = SimRunner::inst();
    sim.record_run(s);

    // Q3.2: 重置统计, 供下一局使用
    _sim_hp_prev = -1;
    _sim_dmg_taken = 0;
    _sim_dmg_dealt = 0;
    _sim_heal_total = 0;
    _sim_mon_hp.clear();
    _sim_items_picked = 0;
    _sim_combat_frames = 0;

    if (sim.should_restart()) {
        // G7.4: all-builds rotation
        if (sim.all_builds()) {
            g_sim_build_type = sim.next_build_type();
        }
        uint32_t next_seed = sim.next_seed();
        _dungeon_seed = next_seed;
        seed_rng(next_seed);
        new_game();
    } else {
        sim.finalize();
        if (get_tree()) get_tree()->quit();
    }
}

// ============================================================
// 战斗
// ============================================================
// _player_attack / _use_skill — delegated to PlayerController (D6 Step7)
void GameScene::_player_attack() { _player_ctrl.player_attack(); }
void GameScene::_use_skill(int index) { _player_ctrl.use_skill(index); }

// 收官: 木桶可交互闭环 — 攻击范围内未触发的桶 → 点燃 (倒计时由 arena tick 驱动)
void GameScene::_try_trigger_barrel_near(const Rectangle& rect) {
    if (!game_map) return;
    for (auto& ao : game_map->arena_objects) {
        if (!ao.active || ao.type != ArenaObjectType::EXPLOSIVE_BARREL) continue;
        if (ao.timer > 0.0f) continue;  // 已点燃
        Vector2 c = {(float)ao.tile_x * TILE_SIZE + TILE_SIZE / 2.0f,
                     (float)ao.tile_y * TILE_SIZE + TILE_SIZE / 2.0f};
        if (CheckCollisionCircleRec(c, TILE_SIZE, rect)) {
            ao.timer = kBarrelFuse;
            LOG_INFO("[BARREL] 点燃 (%d,%d)", ao.tile_x, ao.tile_y);
        }
    }
}

// 收官: 木桶爆炸 — AOE 伤害 (玩家 + 怪物) + 爆炸 VFX + 销毁
void GameScene::_explode_barrel(const ArenaObject& ao) {
    float cx = (float)ao.tile_x * TILE_SIZE + TILE_SIZE / 2.0f;
    float cy = (float)ao.tile_y * TILE_SIZE + TILE_SIZE / 2.0f;
    int dmg = (int)(3 * g_growth.arena_scale(current_floor));
    LOG_INFO("[BARREL] 爆炸 (%d,%d) 伤害 %d", ao.tile_x, ao.tile_y, dmg);
    VFXServer vfx;
    vfx.explosion(cx, cy, kBarrelRadius, {255, 140, 40, 255}, 18, 0.5f);
    vfx.shockwave(cx, cy, kBarrelRadius, {255, 180, 60, 255});
    for (auto& e : vfx.effects) active_effects.push_back(e);
    _presentation.trigger_shake(6.0f);
    float px = player->entity.rect.x + player->entity.rect.width / 2.0f;
    float py = player->entity.rect.y + player->entity.rect.height / 2.0f;
    if (hypotf(px - cx, py - cy) <= kBarrelRadius) {
        player->combat.take_damage(dmg);
        player->combat.mark_damage_logged();
        player->combat.last_damage_source = "env:木桶爆炸";   // M2-B
        _presentation.damage_floats.push_back({px, py - 8, 0.4f, 0.4f, dmg, {255, 60, 40, 255}});
    }
    for (auto& m : monsters) {
        if (!m->combat.is_alive) continue;
        float mx = m->entity.rect.x + m->entity.rect.width / 2.0f;
        float my = m->entity.rect.y + m->entity.rect.height / 2.0f;
        if (hypotf(mx - cx, my - cy) <= kBarrelRadius) {
            m->combat.take_damage(dmg);
            _presentation.damage_floats.push_back({mx, my - 8, 0.4f, 0.4f, dmg, {255, 140, 40, 255}});
        }
    }
}

void GameScene::_update_monsters(float dt) {
    int hp_before = player->combat.current_hp;
    // M2-C: 战斗帧累计 — 近距离交战 (2 tile 内, 与攻击判定同量级)
    // 注: 8 tile 感知半径会把"卡墙徘徊+远处怪"误计为战斗; 收紧到实际交战距离
    if (_sim_mode && !monsters.empty()) {
        float px = player->entity.rect.x + player->entity.rect.width / 2;
        float py = player->entity.rect.y + player->entity.rect.height / 2;
        for (auto& m : monsters) {
            if (!m || !m->combat.is_alive) continue;
            float d = hypotf(m->entity.rect.x - px, m->entity.rect.y - py);
            if (d < 2.5f * TILE_SIZE) { _sim_combat_frames++; break; }
        }
    }
    std::vector<Monster*> mlist;
    for (auto& m : monsters) mlist.push_back(m.get());
    // D2: Pass projectiles vector to monsters for ranged attacks
    for (auto& m : monsters) m->projectiles_ptr = &projectiles;

    int ptx = (int)(player->entity.rect.x + player->entity.rect.width / 2) / 32;
    int pty = (int)(player->entity.rect.y + player->entity.rect.height / 2) / 32;
    int player_room = _room_mgr.room_at(ptx, pty);

    _unstuck_wedged_monsters(game_time);
    for (auto& m : monsters) {
        if (!m->combat.is_alive) continue;
        int mtx = (int)(m->entity.rect.x + m->entity.rect.width / 2) / 32;
        int mty = (int)(m->entity.rect.y + m->entity.rect.height / 2) / 32;
        int monster_room = _room_mgr.room_at(mtx, mty);
        m->update_ai(player.get(), game_map.get(), dt, game_time, &mlist, &active_effects,
                     monster_room, player_room, &_room_mgr);
    }
}

// Q3.2: 怪物脱卡 — 存活非boss怪 ≥5s 未位移 → 拉到玩家周边可行走格 (消除贴墙/口袋钉子户软锁)
void GameScene::_unstuck_wedged_monsters(double gt) {
    auto& last_pos = _unstuck_last_pos;
    auto& stuck_since = _unstuck_since;
    for (auto& m : monsters) {
        if (!m || !m->combat.is_alive) { last_pos.erase(m->instance_id); stuck_since.erase(m->instance_id); continue; }
        int mt0 = (int)(m->entity.rect.x + m->entity.rect.width / 2) / 32;
        int mt1 = (int)(m->entity.rect.y + m->entity.rect.height / 2) / 32;
        auto it = last_pos.find(m->instance_id);
        if (it == last_pos.end() || it->second.first != mt0 || it->second.second != mt1) {
            last_pos[m->instance_id] = {mt0, mt1};
            stuck_since[m->instance_id] = gt;
            continue;
        }
        bool placed = false;
        int ptx = (int)(player->entity.rect.x + player->entity.rect.width / 2) / 32;
        int pty = (int)(player->entity.rect.y + player->entity.rect.height / 2) / 32;
        bool far_away = abs(mt0 - ptx) > 38 || abs(mt1 - pty) > 38;   // 远距怪: 强制吸引
        double idle_need = m->is_boss ? 12.0 : 5.0;
        if (!far_away) {
            // Q3.3: 战斗中不脱卡 — 怪在自身攻击射程内即换血/狙击 (传送会打断战局 → 无限循环)
            float mdx = m->entity.rect.x + m->entity.rect.width/2
                      - (player->entity.rect.x + player->entity.rect.width/2);
            float mdy = m->entity.rect.y + m->entity.rect.height/2
                      - (player->entity.rect.y + player->entity.rect.height/2);
            float atk_px = (m->ai ? m->ai->attack_range : 1.5f) * 32.0f;
            if (atk_px < 1.6f * 32.0f) atk_px = 1.6f * 32.0f;
            if (sqrtf(mdx*mdx + mdy*mdy) < atk_px) continue;
            if (gt - stuck_since[m->instance_id] < idle_need) continue;
        }
        // Q3.10: 近距环仅 1 格 — r=2(64px) 超出玩家近战48px, 怪仍够不着 → 追打循环拖死
        // 1 格(32px) 落点必在玩家近战内 → 战斗立即恢复 (Q3.2: 远距吸引放远环 8-12格 不变)
        int monster_room = _room_mgr.room_at(mt0, mt1);
        for (int r = (far_away ? 8 : 1); r <= (far_away ? 12 : 1) && !placed; r++)
            for (int a = 0; a < 8 && !placed; a++) {
                int tx = ptx + (int)(cosf(a * 0.785398f) * r);
                int ty = pty + (int)(sinf(a * 0.785398f) * r);
                // Q3.5: 禁止原地传送 — 环内首个可行走格常等于怪当前格 (堆叠/口袋)
                // → 每 5s 传送回原格 → 900s 死锁 (v38 F11 4怪同格循环)
                if (tx == mt0 && ty == mt1) continue;
                if (monster_room >= 0 && _room_mgr.room_at(tx, ty) != monster_room) continue;
                Rectangle rr = { (float)(tx * 32), (float)(ty * 32),
                                 m->entity.rect.width, m->entity.rect.height };
                if (game_map->is_rect_walkable(rr)) {
                    m->entity.position.x = tx * 32.0f;
                    m->entity.position.y = ty * 32.0f;
                    m->entity.sync_rect();
                    LOG_INFO("[FIX] 脱卡: %s → tile(%d,%d)", m->name.c_str(), tx, ty);
                    last_pos[m->instance_id] = {tx, ty};
                    stuck_since[m->instance_id] = gt;
                    placed = true;
                }
            }
        // Room boundary fallback: if no valid tile in room found, reset to home
        if (!placed && m->ai && m->ai->home_x >= 0 && m->ai->home_y >= 0) {
            int htx = (int)(m->ai->home_x) / 32;
            int hty = (int)(m->ai->home_y) / 32;
            if (monster_room < 0 || _room_mgr.room_at(htx, hty) == monster_room) {
                m->entity.position.x = m->ai->home_x - m->entity.rect.width / 2;
                m->entity.position.y = m->ai->home_y - m->entity.rect.height / 2;
                m->entity.sync_rect();
                last_pos[m->instance_id] = {htx, hty};
                stuck_since[m->instance_id] = gt;
            }
        }
    }
}

void GameScene::_on_monster_killed(Monster* m)  { _combat.on_monster_killed(m); }

// ── M4b: Boss 房机制地形 — F10 熔岩环带安全区 (茶杯头式竞技场) ──
void GameScene::_setup_boss_arena_terrain(const DungeonGenerator& gen, int floor) {
    if (!game_map || floor != 10) return;
    const BossDef* def = get_boss_def_for_floor(floor);
    if (!def || !def->arena.terrain.enabled) return;
    auto [rx, ry, rw, rh] = gen.get_boss_room_rect();
    if (rw <= 0 || rh <= 0) return;
    if (def->arena.terrain.clear_objects) {
        auto& objs = game_map->arena_objects;
        objs.erase(std::remove_if(objs.begin(), objs.end(),
            [rx, ry, rw, rh](const ArenaObject& o) {
                return o.tile_x >= rx && o.tile_x < rx + rw
                    && o.tile_y >= ry && o.tile_y < ry + rh;
            }), objs.end());
    }
    int cx = rx + rw / 2, cy = ry + rh / 2;
    int safe = def->arena.terrain.safe_radius;
    int band = def->arena.terrain.lava_band;
    int max_safe = std::min(rw, rh) / 2 - band - 1;
    if (safe > max_safe) safe = std::max(1, max_safe);
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++) {
            int d = (int)hypotf((float)(x - cx), (float)(y - cy));
            if (d > safe && d <= safe + band)
                game_map->set_tile(x, y, TileType::LAVA);
        }
    LOG_INFO("[M4b] F10 Boss 房地形: 安全区 r=%d 熔岩带 %d 格 (房间 %d,%d %dx%d)",
             safe, band, rx, ry, rw, rh);
}

void GameScene::_cleanup_dead_monsters() {
    _boss.on_core_maybe_erased();   // F10.2-fix: UAF guard — DOT 击杀核心先解除引用
    // G11.2: 击杀势头 — 有怪刚死则触发暖色 vignette 脉动 (3s 衰减)
    size_t alive_before = monsters.size();
    _combat.cleanup_dead_monsters();
    if (monsters.size() < alive_before) _kill_streak_timer = 3.0f;
}
void GameScene::_check_floor_clear() {
    if (FloorManager::is_floor_cleared(monsters) && !stairs_active) _activate_stairs();
}

void GameScene::_activate_stairs() {
    if (stairs_active) return;
    auto boss = _get_boss();
    if (boss && boss->combat.is_alive) return;
    game_map->set_tile(stairs_pos.first, stairs_pos.second, TileType::STAIRS_DOWN);
    stairs_active = true;
    max_unlocked_floor = std::max(max_unlocked_floor, current_floor);
    LOG_INFO("第%d层清空! 楼梯已激活, 自动存档", current_floor);
    std::vector<bool> spr, spd;
    if (game_map) for (auto& sr : game_map->special_rooms) {
        spr.push_back(sr.triggered);
        spd.push_back(sr.discovered);
    }
    // B13: Relic 不再跨层 (无需保存圣物)
    // ── G1 Step7: 收集 rule_* counters ──
    {
        std::unordered_map<std::string, int> rcm;
        static const char* ALL_RULES[] = {
            "rule_shadow_charge","rule_summon_priority","rule_arena_movement",
            "rule_shield_patience","rule_rule_override", nullptr
        };
        for (int i = 0; ALL_RULES[i]; i++) {
            int v = _gameplay.world_state.counter(ALL_RULES[i]);
            if (v > 0) rcm[ALL_RULES[i]] = v;
        }
        // M1-D1: last-known 镜像记忆 — Boss 存活时导出更新缓存; 否则透传上一已知值
        // (修复: F15 前任意存档曾用空导出覆盖档, 抹掉上一局学习成果)
        {
            std::vector<float> fresh_alpha, fresh_beta;
            _boss.export_mirror_memory(fresh_alpha, fresh_beta);
            if (!fresh_alpha.empty()) {
                _mirror_mem_alpha = fresh_alpha;
                _mirror_mem_beta = fresh_beta;
            }
        }
        // G10.9-B2: 槽位化保存 — endings 不再入档 (Meta 侧落盘); 补记账号最高层
        // play_time 源 = game_time (本 run 进度内累计的帧时钟, 换层不清零)
        SaveManager::save_game(SaveManager::active_slot(), player.get(),
                               current_floor, max_unlocked_floor,
                               _dungeon_seed, spr, spd, rcm,
                               _gameplay.quest_mgr.export_states(),
                               _mirror_mem_alpha, _mirror_mem_beta,
                               (float)game_time);
        MetaSystem::record_floor_reached(max_unlocked_floor);
    }
    on_floor_cleared.emit();
}

void GameScene::_check_floor_transition() {
    if (!stairs_active) return;
    // E 键同时用于交互 — 对话/事件/背包/日志打开时不得误下楼
    if (_dialogue.active || _is_event_running() || inventory_open || _quest_log_open) return;
    // Q3.1: sim 模式下楼梯判定走 SimAI (headless 无真实按键)
    InputMap& sim_in = get_tree()->get_input();
    if (_sim_mode) {
        if (!_is_action_just_pressed(sim_in, "descend")) return;
    } else {
        int next_manual = FloorManager::check_floor_transition(sim_in,
            current_floor, game_map.get(), player.get(), stairs_pos);
        if (next_manual < 0) return;  // 不下楼
    }
    int next = current_floor + 1;

    if (next > MAX_FLOORS) {
        // G5.6: sim stats on game clear
        if (_sim_mode) {
            // Q3.11: sim 通关 — _collect_sim_stats 内部已处理重启(new_game)/退出(finalize)
            // 不能再执行下面的 VictoryScene 流程, 否则批次挂在 VictoryScene 永不退出
            // (v13/v14 "批次冻结" 实为此 bug: 镜像削弱后出胜局 → 误入胜利画面 → 主循环不退出)
            _collect_sim_stats();
            return;
        }
        LOG_INFO("通关! 最终第%d层 Lv%d", current_floor, player->level);
        // D6 Step6: 通关 — 委托给 FlowDirector (ending → VictoryScene → Credits)
        _gameplay.on_game_clear(current_floor, player->level, player.get(),
                                 _boss.battle_report, g_relic_archive.collection_pct());
        _flow.on_game_clear();
    } else {
        enter_floor(next);
    }
}

void GameScene::_apply_pending_damage() { _combat.apply_pending_damage(); }

// _pickup, _interact_special, _check_special_room_discovery, _show_room_message
// 已迁移到 InteractionHandler

Monster* GameScene::_get_boss() const {
    for (auto& m : monsters)
        if (m->is_boss && m->combat.is_alive) return m.get();
    return nullptr;
}

void GameScene::_drop_boss_reward(Monster* boss) {
    auto [tx, ty] = game_map->pixel_to_tile(boss->entity.position.x, boss->entity.position.y);
    // G9: Each Boss drops a different legendary weapon
    static const char* boss_weapons[] = {
        "spear_legendary",   // F5 暗影骑士 → 惊破天
        "crossbow_legendary",// F10 地狱火魔 → 东风破
        "sword_legendary",   // F15 深渊之主 → 倚天剑
    };
    int bf_idx = (current_floor == 5) ? 0 : (current_floor == 10) ? 1 : 2;
    const WeaponDef* wdef = get_weapon_def(boss_weapons[bf_idx]);
    const char* wname = wdef ? pick_weapon_name(wdef, 3) : "魔渊之刃";
    float atk = wdef ? (float)wdef->base_damage : 18.0f;
    auto weapon = std::make_shared<EquipmentItem>(wname, Rarity::LEGENDARY, "weapon",
        (int)atk, 3, 0);
    weapon->weapon_def_id = boss_weapons[bf_idx];
    ground_items.push_back({weapon, tx, ty});
    auto potion = std::make_shared<ConsumableItem>("神谕药剂", Rarity::LEGENDARY, "heal", 80);
    ground_items.push_back({potion, tx + 2, ty});
}

// ============================================================
// 渲染
// ============================================================
void GameScene::_render() {
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();

    // G10.1: Element Select Screen
    if (element_select_active) {
        ClearBackground({20, 15, 30, 255});
        const ElementDef* defs[3] = {
            get_element_def("fire"), get_element_def("ice"), get_element_def("poison")
        };
        const char* icons[] = {
            "[火] 火焰核心", "[冰] 冰霜核心", "[毒] 剧毒核心"
        };
        const char* long_desc[] = {
            "每次攻击有概率触发火焰暴击\n暴击伤害 x1.5\nLv1 暴击率 15%，Lv20 约 30%",
            "每击附加减速\n累计减速层数触发冻结(1秒)\nLv1 冻结率 10%，Lv20 约 100%",
            "每击附加持续毒伤\nDOT = 本次伤害 x 比例\nLv1 毒伤 5%，Lv20 约 15%"
        };
        const Color colors[] = {
            {255,120,30,255}, {100,200,255,255}, {80,220,80,255}
        };
        const char* title = "选择你的元素核心";
        float tw = MeasureTextEx(g_font_small, title, 28, 1).x;
        DrawTextEx(g_font_small, title, {sw/2.0f - tw/2, 40}, 28, 1, {255,220,180,255});

        float card_w = 280, card_h = 300, gap = 20;
        float start_x = sw/2.0f - (card_w * 3 + gap * 2)/2.0f;
        for (int i = 0; i < 3; i++) {
            float cx = start_x + i * (card_w + gap);
            float cy = (sh - card_h)/2.0f + 20;
            bool selected = (i == element_select_cursor);
            Color bg = selected ? Color{50,50,80,255} : Color{25,25,45,255};
            Color border = selected ? colors[i] : Color{50,50,75,220};

            DrawRectangleRounded({cx, cy, card_w, card_h}, 0.1f, 8, bg);
            DrawRectangleRoundedLines({cx-1, cy-1, card_w+2, card_h+2}, 0.1f, 8, 2.5f, border);

            float iw = MeasureTextEx(g_font_small, icons[i], 32, 1).x;
            DrawTextEx(g_font_small, icons[i], {cx + card_w/2 - iw/2, cy + 25}, 32, 1, colors[i]);

            // Multi-line description
            float dy = cy + 80;
            const char* desc = long_desc[i];
            std::string line;
            for (const char* p = desc; *p; p++) {
                if (*p == '\n') {
                    float lw = MeasureTextEx(g_font_small, line.c_str(), 13, 1).x;
                    DrawTextEx(g_font_small, line.c_str(),
                        {cx + card_w/2 - lw/2, dy}, 13, 1, {200,210,200,200});
                    dy += 22;
                    line.clear();
                } else {
                    line += *p;
                }
            }
            if (!line.empty()) {
                float lw = MeasureTextEx(g_font_small, line.c_str(), 13, 1).x;
                DrawTextEx(g_font_small, line.c_str(),
                    {cx + card_w/2 - lw/2, dy}, 13, 1, {200,210,200,200});
            }

            if (selected) {
                DrawTextEx(g_font_small, "[←/→选择] [空格/E 确认]",
                    {cx + card_w/2 - 110, cy + card_h - 35}, 14, 1, {255,255,180,220});
            }
        }
        const char* ft = "选择后永久绑定，本局及以后所有存档不可更改";
        float fw = MeasureTextEx(g_font_small, ft, 14, 1).x;
        DrawTextEx(g_font_small, ft, {sw/2.0f - fw/2, (float)(sh - 30)}, 14, 1, {150,150,150,180});
        return;
    }

    if (state == GameState::BOSS_INTRO) {
        // F15.5: Mirror analysis panel for Ending Echo
        if (boss_floor == 15 && _boss._behavior_type == "mirror") {
            _draw_mirror_analysis_panel(sw, sh);
        } else {
            _renderer.draw_boss_intro(sw, sh, boss_intro_title, boss_intro_lore,
                                       boss_intro_skills, boss_intro_color, boss_floor,
                                       boss_intro_visual);
        }
        // D4 Step5.5: BossNarrative覆盖对话 (显示在面板下方)
        if (!_presentation.boss_intro_text.empty() && g_font_loaded) {
            float tw = MeasureTextEx(g_font_small, _presentation.boss_intro_text.c_str(), 17, 1).x;
            DrawTextEx(g_font_small, _presentation.boss_intro_text.c_str(),
                       {sw/2.0f - tw/2, (float)(sh - 100)}, 17, 1, {255, 220, 100, 240});
        }
        // D5 Step1: BossModifier文字 (金色Warning风格)
        if (!_presentation.boss_modifier_text.empty() && g_font_loaded) {
            float mw = MeasureTextEx(g_font_small, _presentation.boss_modifier_text.c_str(), 15, 1).x;
            DrawRectangle(sw/2.0f - mw/2 - 12, (float)(sh - 72), mw + 24, 24,
                          {30, 15, 15, 200});
            DrawTextEx(g_font_small, _presentation.boss_modifier_text.c_str(),
                       {sw/2.0f - mw/2, (float)(sh - 68)}, 15, 1, {255, 80, 40, 240});
        }
        return;
    }

    ClearBackground(BLACK);
    _renderer.update_camera(_cam_x, _cam_y, player.get(), game_map.get(), sw, sh);

    // D4 Step5.4: WorldReaction tint (覆盖 CLEAR_BACKGROUND 之上)
    {
        unsigned char tr = 0, tg = 0, tb = 0;
        g_reactions.current_tint(_gameplay.world_state, _gameplay.story.stage(), tr, tg, tb);
        if (tr > 0 || tg > 0 || tb > 0) {
            DrawRectangle(0, 0, sw, sh, {tr, tg, tb, 18});  // 18 alpha = 7% overlay
        }
    }

    // C1: 屏幕震动 (相机偏移) — G9.3 (RNG-001): 走独立视觉流, 不消耗战斗 RNG
    auto [shake_ox, shake_oy] = shake_offset(_presentation.shake_intensity,
                                              _presentation.shake_timer);
    float saved_cx = _cam_x, saved_cy = _cam_y;
    
    // A6-T4: CameraDirector 焦点偏移 (Boss 战运镜)
    if (!_sim_mode && _camera_def_loaded) {
        Vector2 cam_offset = _camera_director.focus_offset();
        _cam_x += cam_offset.x;
        _cam_y += cam_offset.y;
    }
    
    _cam_x += shake_ox; _cam_y += shake_oy;

    // M6-HD2D: 3D 表现层分支 (--hd2d) — 世界层走 3D 渲染器, HUD/overlay 走 2D 桥
    // (方案一: 3D 世界 + 屏幕空间 UI)。逻辑层零改动; 初始化失败自动回退 2D
    if (g_hd2d_mode) {
        auto& hd2d = HD2DRenderer::inst();
        if (hd2d.ensure_init(sw, sh)) {
            _ensure_player_avatar();
            _player_avatar_tick();
            _monster_avatars_tick();
            _npc_avatars_tick();
            hd2d.set_camera_shake(shake_ox, shake_oy);
            hd2d.render_frame(*this);
            _render_hd2d_ui_bridge(sw, sh);   // M6-v2a: HUD + 全 overlay 桥
            return;
        }
        g_hd2d_mode = false;  // 初始化失败: 本次会话回退 2D
    }
    _ensure_player_avatar();
    _player_avatar_tick();
    _monster_avatars_tick();
    _npc_avatars_tick();
    _draw_map();
    _draw_ground_items();
    _draw_entities();
    _renderer.draw_effects(active_effects, _cam_x, _cam_y);
    // G11.2: 氛围粒子层 — 实体之上, HUD 之下 (世界空间)
    if (game_map) _ambient.draw(_cam_x, _cam_y, sw, sh);
    // Batch 3I: Challenge portal VFX (entry in DUNGEON, return in CHALLENGE_ARENA)
    if (_challenge.phase() == ChallengePhase::PORTAL_ACTIVE && game_map) {
        for (auto& sr : game_map->special_rooms) {
            if (sr.type == SpecialRoomType::CHALLENGE) {
                _renderer.draw_challenge_portal(_cam_x, _cam_y,
                    sr.portal_tx, sr.portal_ty, _portal_pulse_timer, true);
                break;
            }
        }
    }
    if (_challenge.phase() == ChallengePhase::CLEARED &&
        _world_mode == WorldMode::CHALLENGE_ARENA &&
        _challenge.return_portal_tx() >= 0) {
        _renderer.draw_challenge_portal(_cam_x, _cam_y,
            _challenge.return_portal_tx(), _challenge.return_portal_ty(),
            _portal_pulse_timer, false);
    }
    // D5 Step4: Boss战场绘制
    _boss.arena.draw(_cam_x, _cam_y);

    // D2: draw unified projectiles (PLAYER + MONSTER + ENVIRONMENT)
    projectiles.for_each([&](const Projectile& p, int) {
        if (!p.alive) return;
        float sx = p.pos.x - _cam_x, sy = p.pos.y - _cam_y;
        bool is_enemy = (p.owner != (int)ProjectileOwner::PLAYER);

        // WARNING phase: AOE → danger circle; point projectile → trajectory preview
        if (p.active_time < 0.0f) {
            float fade = 1.0f - (-p.active_time / p.warning_time);
            float pulse = 1.0f + sinf(p.active_time * 12.0f) * 0.12f;
            Color wc = (p.warning_level >= 2) ? Color{255,40,20,120}
                     : (p.warning_level >= 1) ? Color{255,160,30,120}
                     : Color{255,200,60,110};
            if (p.owner == (int)ProjectileOwner::ENVIRONMENT && p.warning_radius > 0.0f) {
                float wr = p.warning_radius * pulse;
                DrawRing({sx, sy}, wr - 3, wr + 3, 0, 360, 20,
                    {wc.r, wc.g, wc.b, (unsigned char)(wc.a * 2/3)});
                DrawCircleLines(sx, sy, wr, wc);
            } else {
                _draw_projectile_preview(sx, sy, p, wc, fade, pulse, game_map.get());
            }
        }
        // ACTIVE phase: draw the projectile itself
        else {
            if (p.piercing) {
                DrawCircle(sx, sy, 10.0f, {255,200,40,100});
                DrawCircle(sx, sy, 7.0f, {255,180,40,220});
                DrawCircle(sx, sy, 3.0f, {255,255,220,255});
                Vector2 back = { sx - p.vel.x * 0.03f, sy - p.vel.y * 0.03f };
                DrawLineEx({sx, sy}, back, 3.0f, {255,180,40,180});
            } else if (is_enemy) {
                Color ec = (p.element == 1) ? Color{255,80,30,255}
                         : (p.element == 2) ? Color{80,180,255,255}
                         : Color{255,60,40,220};
                DrawCircle(sx, sy, 6.0f, ec);
                DrawCircle(sx, sy, 3.0f, {255,255,200,200});
            } else {
                DrawCircle(sx, sy, 6.0f, {0, 0, 0, 150});
                DrawCircle(sx, sy, 4.0f, {200,160,100,255});
                DrawCircle(sx, sy, 2.0f, {255,255,220,180});
            }
        }
    });

    // G9: ranged weapon range indicator
    if (player->weapon.range_indicator_timer > 0.0f && player->weapon.current_def()) {
        WeaponType wt = player->weapon.weapon_type();
        if (wt == WeaponType::SPEAR || wt == WeaponType::CROSSBOW
            || wt == WeaponType::NUNCHAKU) {
            float sx = player->entity.rect.x + player->entity.rect.width/2 - _cam_x;
            float sy = player->entity.rect.y + player->entity.rect.height/2 - _cam_y;
            float fade = player->weapon.range_indicator_timer / 0.25f;
            float rpx = player->weapon.range_indicator_px;

            if (wt == WeaponType::NUNCHAKU) {
                const WeaponDef* ndef = player->weapon.current_def();
                float inner_r = (ndef ? ndef->min_range : 2.0f) * TILE_SIZE;
                float outer_r = (ndef ? ndef->max_range : 5.0f) * TILE_SIZE;
                // 贴图地板较亮: 深色厚底衬 + 亮色细环, 保证可见
                DrawRing({sx, sy}, inner_r - 3, outer_r + 3, 0, 360, 64,
                         {0, 0, 0, (unsigned char)(110.0f * fade)});
                Color nc  = {235,175,95,(unsigned char)(200.0f*fade)};
                Color fill = {220,160,60,(unsigned char)(45.0f*fade)};
                DrawRing({sx, sy}, inner_r, outer_r, 0, 360, 64, fill);
                DrawRing({sx, sy}, inner_r - 2, inner_r + 2, 0, 360, 64, nc);
                DrawRing({sx, sy}, outer_r - 2, outer_r + 2, 0, 360, 64, nc);
            } else {
                bool is_cb = (wt == WeaponType::CROSSBOW);
                Color oc = is_cb ? Color{255,175,55,(unsigned char)(210.0f*fade)}
                                 : Color{130,195,255,(unsigned char)(210.0f*fade)};
                Color ic = is_cb ? Color{255,120,20,(unsigned char)(70.0f*fade)}
                                 : Color{80,140,220,(unsigned char)(70.0f*fade)};
                DrawRing({sx, sy}, rpx - 3, rpx + 3, 0, 360, 64,
                         {0, 0, 0, (unsigned char)(130.0f * fade)});
                DrawRing({sx, sy}, rpx - 2, rpx + 2, 0, 360, 64, oc);
                DrawCircle(sx, sy, rpx, ic);
            }
        }
    }

    // C1: 伤害数字 (世界坐标→屏幕; M6-v2b 提取共用)
    _render_damage_floats_2d();

    _cam_x = saved_cx; _cam_y = saved_cy;  // 恢复

    // M6-v2a: UI tail extracted to shared method (2D/3D common)
    _render_ui_tail(sw, sh);
}


// M6-v2a: _render UI tail (2D/3D common) - hitflash/fade/panels/HUD/minimap/dialogue/event/freeze/cinematic
void GameScene::_render_ui_tail(int sw, int sh) {

    // Q4.7: 玩家受击红屏 — 全屏叠加主题 hit_flash_tint (alpha 随计时衰减)
    if (_presentation.hit_flash_timer > 0 && g_font_loaded) {
        Color ft = _presentation.get_theme().screen.hit_flash_tint;
        float fade = _presentation.hit_flash_timer / 0.18f;
        DrawRectangle(0, 0, sw, sh,
            {ft.r, ft.g, ft.b, (unsigned char)(ft.a * fade * 0.35f)});
    }
    // Batch 3I: Teleport fade overlay (full-screen black)
    _renderer.draw_teleport_fade(sw, sh, _teleport_fade_timer, _portal_fade_in);

    if (challenge_choice_active) {
        _renderer.draw_challenge_choice(sw, sh, challenge_choice_cursor,
            ChallengeRoomController::boss_wave_hint(_challenge_pity_streak).c_str());
    }

    // F15.5.1: Build echo mirror panel data (M6-v2a: 提取为共用方法)
    CharacterPanelData echo_panel_data;
    _build_echo_panel_data(echo_panel_data);

    // HUD (委托给 GameRenderer)
    // G11.2: AI 情绪 vignette — 地图之上, HUD 之下 (探索冷色/战斗暖色渐晕)
    _ambient.draw_vignette(sw, sh);
    int ch_wave = (_challenge.phase() == ChallengePhase::COMBAT ||
                   _challenge.phase() == ChallengePhase::WAVE_SPAWNING)
                  ? _challenge.current_wave() + 1 : -1;
    _renderer.draw_hud(player.get(), current_floor, game_time,
                        _get_boss(), _show_relic_panel,
                        inventory_open, inventory_cursor,
                        _presentation.room_msg, _presentation.room_msg_timer, sw, sh,
                        echo_panel_data.mirror_mode ? &echo_panel_data : nullptr,
                        ch_wave, _challenge.total_waves());
    _render_mirror_hud_overlay(sw, game_time);   // v1.6-B1.1: 顶部分析/观察卡

    // Phase 3: Minimap — 右下角常驻面板 (M 键开关)
    if (_show_minimap && game_map && player) {
        MinimapInput mm;
        auto [ptx, pty] = game_map->pixel_to_tile(
            player->entity.rect.x + player->entity.rect.width / 2,
            player->entity.rect.y + player->entity.rect.height / 2);
        mm.player_tx = ptx; mm.player_ty = pty;

        // Boss — 始终显示（让玩家知道 BOSS 方向）
        if (_boss_last_known.first >= 0) {
            mm.boss_marker.tx = _boss_last_known.first;
            mm.boss_marker.ty = _boss_last_known.second;
            mm.boss_marker.visible = true;
            mm.boss_marker.color = Color{220, 60, 50, 255};  // 红 — Boss
            mm.boss_marker.type = MinimapMarker::Type::BOSS;
        }
        // 楼梯（发现后永久地标）
        if (MinimapRenderer::should_show_stairs(*game_map, stairs_pos.first, stairs_pos.second)) {
            mm.stairs_marker.tx = stairs_pos.first;
            mm.stairs_marker.ty = stairs_pos.second;
            mm.stairs_marker.visible = true;
            mm.stairs_marker.color = Color{230, 210, 70, 255};  // 黄 — 楼梯
            mm.stairs_marker.type = MinimapMarker::Type::STAIRS;
        }
        // 当前可见的怪物/物品（仅当前 is_visible 才显示）
        for (auto& m : monsters) {
            if (!m) continue;
            auto [mtx, mty] = game_map->pixel_to_tile(m->entity.position.x, m->entity.position.y);
            if (!m->is_boss && MinimapRenderer::should_show_entity(*game_map, mtx, mty)) {
                MinimapMarker mark;
                mark.tx = mtx;
                mark.ty = mty;
                mark.visible = true;
                mark.color = Color{200, 80, 80, 255};  // 暗红 — 普通怪
                mark.type = MinimapMarker::Type::MONSTER;
                mm.markers.push_back(mark);
            }
        }
        for (auto& d : ground_items) {
            if (MinimapRenderer::should_show_entity(*game_map, d.tile_x, d.tile_y)) {
                MinimapMarker mark;
                mark.tx = d.tile_x;
                mark.ty = d.tile_y;
                mark.visible = true;
                mark.color = Color{80, 200, 120, 255};  // 绿 — 物品
                mark.type = MinimapMarker::Type::ITEM;
                mm.markers.push_back(mark);
            }
        }

        Rectangle panel = {(float)(sw - MINIMAP_WIDTH - 14), (float)(sh - MINIMAP_HEIGHT - 40),
                           (float)MINIMAP_WIDTH, (float)MINIMAP_HEIGHT};
        _minimap.draw(*game_map, mm, panel);
        // M 键提示 — 面板下方
        const char* hint = "[M] Map";
        int hint_w = MeasureText(hint, 14);
        DrawText(hint, (int)(panel.x + panel.width / 2 - hint_w / 2),
                 (int)(panel.y + panel.height + 4), 14, {160, 160, 200, 180});
    }

    // G9.1: Combo stage UI (bottom center)
    if (player->weapon.weapon_type() != WeaponType::FIST && player->weapon.combo_index() > 0) {
        int stage = player->weapon.combo_index() + 1;
        char mark[4]; snprintf(mark, sizeof(mark), "%d", stage);
        Color sc = stage >= 3 ? Color{255,200,40,240} : Color{220,220,220,200};
        int fs = stage >= 3 ? 32 : 24;
        if (g_font_loaded)
            DrawTextEx(g_font_small, mark, {(float)(sw/2 - 8), (float)(sh - 65)}, (float)fs, 1, sc);
        else
            DrawText(mark, sw/2 - 10, sh - 60, fs, sc);
    }

#ifdef _DEBUG
    // D9: Debug overlay (右上角, fps/floor/seed/build/monsters)
    if (g_font_loaded) {
        char dbg[256]; int fps = GetFPS();
        const char* bn = "无";
        BuildType bt = calculate_build(player.get()).identify();
        if (bt == BuildType::BERSERKER) bn = "狂战士";
        else if (bt == BuildType::FIRE_MAGE) bn = "火法师";
        else if (bt == BuildType::POISON_MASTER) bn = "毒术师";
        else if (bt == BuildType::TIME_MASTER) bn = "时间术士";
        else if (bt == BuildType::SUPPORT) bn = "辅助";
        snprintf(dbg, sizeof(dbg),
            "FPS:%d Fl:%d/%d Seed:%u Mon:%zu Buf:%zu Rel:%zu | %s",
            fps, current_floor, MAX_FLOORS, _dungeon_seed,
            monsters.size(), player->active_buffs.size(), player->relics.size(), bn);
        float dw = MeasureTextEx(g_font_small, dbg, 11, 1).x;
        DrawTextEx(g_font_small, dbg, {sw - dw - 10.0f, 4.0f}, 11, 1, Color{120, 200, 120, 180});
    }
#endif

    if (inventory_open) _renderer.draw_inventory_panel(player.get(), inventory_cursor, sw, sh);
    if (gamble_open) _renderer.draw_gamble_panel(player.get(), gamble_result_msg, gamble_result_timer, sw, sh);

    // D4.6 Step1: F8 Growth Curve debug
    // F15.2: F9 — print player behavior stats
    if (IsKeyPressed(KEY_F9)) {
        g_show_mirror_acc = !g_show_mirror_acc;   // 验收: toggle MIRROR AI 统计
        char dbg[256];
        g_behavior.print_debug(dbg, sizeof(dbg));
        _presentation.show_message(dbg, 4.0f);
    }

    // F9 toggle 且仅 Boss 层 (镜像 agent 存在) 才绘制
    if (g_show_mirror_acc && _boss._mirror_agent && g_font_loaded) {
        const MirrorDebugStats* st = _boss._mirror_agent->debug_stats();
        if (st) {
            const char* title = "MIRROR AI [F9]";
            DrawTextEx(g_font_small, title, {18, 58}, 18, 1, {255, 200, 80, 255});
            DrawTextEx(g_font_small, st->summary().c_str(), {18, 84}, 14, 1,
                       {200, 230, 255, 230});
            char drift_buf[64];
            snprintf(drift_buf, sizeof(drift_buf), "Drift:%d%% Bar:%.2f",
                (int)(_boss._mirror_agent->profile_drift() * 100),
                _boss._mirror_agent->clone_confidence_threshold());
            DrawTextEx(g_font_small, drift_buf, {18, 104}, 14, 1,
                       {230, 160, 120, 230});
        }
    }

    if (_presentation.show_growth_debug && g_font_loaded) {
        const GrowthCurve& gc = g_growth.curve(current_floor);
        float pw = 240, ph = 260;
        Rectangle pr = {sw - pw - 10.0f, 10.0f, pw, ph};
        GameRenderer::draw_panel(pr, "Growth Curve", {15,15,30,220});
        float y = pr.y + 36;
        auto line = [&](const char* fmt, float val) {
            char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
            DrawTextEx(g_font_small, buf, {pr.x+14, y}, 14, 1, {200,220,255,255}); y += 20;
        };
        line("Floor %d", (float)current_floor);
        line("Monster HP  x%.2f",  gc.monster_hp);
        line("Monster ATK x%.2f",  gc.monster_atk);
        line("Boss HP     x%.2f",  gc.boss_hp);
        line("Boss ATK    x%.2f",  gc.boss_atk);
        line("Elite Scale x%.2f",  gc.elite_scale);
        line("EXP Scale   x%.2f",  gc.exp_scale);
        line("Gold Scale  x%.2f",  gc.gold_scale);
        line("Relic Scale x%.2f",  gc.relic_scale);
        line("Arena Scale x%.2f",  gc.arena_scale);
    }

    // D4.6 Step3: F10 FlowDirector debug
    if (_presentation.show_flow_debug && g_font_loaded) {
        float pw = 220, ph = 210;
        Rectangle pr = {sw - pw - 10.0f, 280.0f, pw, ph};
        GameRenderer::draw_panel(pr, "Flow Director", {15,30,15,220});
        float y = pr.y + 36;
        auto line = [&](const char* fmt, float val) {
            char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
            DrawTextEx(g_font_small, buf, {pr.x+14, y}, 13, 1, {200,255,200,255}); y += 19;
        };
        const FlowTimer& ft = _gameplay.flow.timer();
        line("Combat  %.1fs", ft.combat);
        line("Reward  %.1fs", ft.reward);
        line("Story   %.1fs", ft.story);
        line("Event   %.1fs", ft.event);
        line("Explorer %d pts", (int)ft.explorer_score);
        line("Rooms   %d",     ft.rooms_explored);
        line("MaxGap  %.1fs",  _gameplay.flow.worst_gap());
    }

    // D5 Step3: F11 BossBehavior debug
    if (_presentation.show_boss_behavior && g_font_loaded) {
        auto* boss = _get_boss();
        float pw = 240, ph = boss ? 260.0f : 120.0f;
        Rectangle pr = {sw - pw - 10.0f, 500.0f, pw, ph};
        GameRenderer::draw_panel(pr, "Boss Behavior", {25,15,25,220});
        float y = pr.y + 36;
        auto line = [&](const char* fmt, float val) {
            char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
            DrawTextEx(g_font_small, buf, {pr.x+14, y}, 13, 1, {255,200,100,255}); y += 18;
        };
        auto& st = _boss.behavior;
        line("Decision %s",     0); // hack: print string via label
        DrawTextEx(g_font_small, st.decision_name ? st.decision_name : "IDLE",
                   {pr.x+100, y-18}, 13, 1, {255,255,100,255});
        auto* bl = _get_boss();
        if (bl) {
            line("Pers   %s", 0);
            DrawTextEx(g_font_small, st.personality_name ? st.personality_name : "?",
                       {pr.x+100, y-18}, 13, 1, {150,255,150,255});
            line("Mem Atk %d",  (float)st.memory.attacks);
            line("Mem Skl %d",  (float)st.memory.skills);
            line("Mem Cmb %d",  (float)st.memory.combos_max);
            line("Mem Dod %d",  (float)st.memory.dodges);
            for (int i = 0; i < 8; i++) {
                if (st.weights[i] == 0) continue;
                const char* dn[] = {"CHS","RET","CRG","SUM","DEF","RNG","MEL","SPC"};
                char wb[32]; snprintf(wb, sizeof(wb), "%s:%d", dn[i], st.weights[i]);
                DrawTextEx(g_font_small, wb, {pr.x+14 + (i%4)*56.0f, y + (i/4)*16.0f},
                           11, 1, Color{(unsigned char)(150+i*12),(unsigned char)(180+i*8),255,255});
            }
        }
    }

    // D5 Step4: F12 BossCombat debug
    if (_presentation.show_boss_cmd && g_font_loaded) {
        float pw = 220, ph = 200;
        Rectangle pr = {sw - pw - 10.0f, 510.0f, pw, ph};
        GameRenderer::draw_panel(pr, "Boss Cmd+Arena", {25,20,20,220});
        float y = pr.y + 36;
        auto line = [&](const char* fmt, float val) {
            char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
            DrawTextEx(g_font_small, buf, {pr.x+14, y}, 13, 1, {200,255,200,255}); y += 18;
        };
        DrawTextEx(g_font_small, boss_command_name(_boss.current_cmd),
                   {pr.x+14, y}, 14, 1, {255,255,100,255}); y += 20;
        line("Arena Zones %d", (float)_boss.arena.zones().size());
        line("SkillQ active %d", _boss.skill_queue.active ? 1.0f : 0.0f);
        line("SkillQ size  %d",  (float)_boss.skill_queue.queue.size());
        y += 10;
        for (auto& z : _boss.arena.zones()) {
            char zbuf[64];
            const char* tn = z.type == DangerType::LAVA ? "LAVA" :
                             z.type == DangerType::SHADOW_WALL ? "WALL" :
                             z.type == DangerType::VOID_CRACK ? "VOID" : "?";
            snprintf(zbuf, sizeof(zbuf), "%s %.1fs r=%.0f",
                     tn, z.remaining, z.radius);
            DrawTextEx(g_font_small, zbuf, {pr.x+14, y}, 11, 1,
                       z.is_warning() ? Color{255,200,100,180} : Color{255,60,40,200});
            y += 14; if (y > pr.y+ph-20) break;
        }
    }

    // D5 Step5: F13 BossReport debug
    if (_presentation.show_boss_report && g_font_loaded) {
        auto& r = _boss.battle_report;
        float pw = 230, ph = 290;
        Rectangle pr = {sw - pw - 10.0f, 480.0f, pw, ph};
        GameRenderer::draw_panel(pr, "Boss Report", {20,25,20,220});
        float y = pr.y + 36;
        auto line = [&](const char* fmt, float val) {
            char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
            DrawTextEx(g_font_small, buf, {pr.x+14, y}, 13, 1, {200,255,200,255}); y += 18;
        };
        line("%s %s", 0);
        DrawTextEx(g_font_small, _boss.encounter.phase_name(), {pr.x+14, y-18}, 13, 1, {255,200,100,255});
        DrawTextEx(g_font_small, r.rank_name(), {pr.x+120, y-18}, 16, 1, {255,255,60,255});
        line("Time     %.1fs", r.battle_time);
        line("Damage   %d", (float)r.total_damage);
        line("Taken    %d", (float)r.damage_taken);
        line("ComboMax %d",  (float)r.replay.combo_max);
        line("Strategy %s", 0);
        DrawTextEx(g_font_small, r.replay.strategy_name(), {pr.x+100, y-18}, 13, 1, {255,150,100,255});
        line("Rank    %s", 0);
        DrawTextEx(g_font_small, r.rank_name(), {pr.x+100, y-18}, 14, 1, {255,200,60,255});
        line("ArenaZns %d", (float)r.arena_zones_spawned);
        line("BloodRit %s", (float)r.replay.blood_ritual);
        line("Curse    %s", (float)r.replay.curse);
    }

    // D4 Step3: 章节入场 (全屏大标题, 3秒)
    if (_presentation.chapter_intro_active) {
        int ch = _presentation.chapter_intro_ch;
        float a = std::min(1.0f, _presentation.chapter_intro_timer);
        Color gold = {255, 200, 50, (unsigned char)(220 * a)};
        Color white = {230, 230, 240, (unsigned char)(200 * a)};
        DrawRectangle(0, 0, sw, sh, {0, 0, 0, (unsigned char)(150 * a)});
        GameRenderer::draw_glow_text(get_chapter_subtitle(ch), sw/2.0f, sh/2.0f - 35,
                                      36, gold, true);
        GameRenderer::draw_glow_text(get_chapter_title(ch), sw/2.0f, sh/2.0f + 15,
                                      28, white, true);
        auto* fn = get_floor_narrative(_presentation.floor_intro_floor > 0 ? _presentation.floor_intro_floor : current_floor);
        if (fn) GameRenderer::draw_glow_text(fn->subtitle, sw/2.0f, sh/2.0f + 50,
                                              18, Color{180,180,200,(unsigned char)(160*a)}, true);
    }

    // D4 Step3: 楼层入场演出 (非章节覆盖时)
    if (_presentation.floor_intro_active && !_presentation.chapter_intro_active) {
        auto* fn = get_floor_narrative(_presentation.floor_intro_floor);
        if (fn) {
            float a = _presentation.floor_intro_fade;
            Color gold = {255, 200, 50, (unsigned char)(200 * a)};
            Color white = {230, 230, 240, (unsigned char)(180 * a)};
            DrawRectangle(0, 0, sw, sh, {0, 0, 0, (unsigned char)(120 * a)});
            char buf[64];
            snprintf(buf, sizeof(buf), "Floor %d", _presentation.floor_intro_floor);
            GameRenderer::draw_glow_text(buf, sw/2.0f, sh/2.0f - 40, 20, gold, true);
            GameRenderer::draw_glow_text("══════════════", sw/2.0f, sh/2.0f - 20, 18, gold, true);
            GameRenderer::draw_glow_text(fn->title, sw/2.0f, sh/2.0f + 10, 28, white, true);
            GameRenderer::draw_glow_text(fn->subtitle, sw/2.0f, sh/2.0f + 42, 16,
                                          Color{180,180,200,(unsigned char)(140*a)}, true);
            GameRenderer::draw_glow_text("══════════════", sw/2.0f, sh/2.0f + 60, 18, gold, true);
        }
    }

    // D4 Step4: 对话UI (在HUD之上, 事件之下)
    _draw_dialogue(sw, sh);
    // D4 Step4: 任务日志
    if (_quest_log_open) _draw_quest_log(sw, sh);

    // D4 Step2: 事件演出 UI (在所有 HUD 之上)
    _draw_event_ui(sw, sh);

    // v1.6-B1: 镜像阶段晋升横幅 (最高优先级 — 招牌时刻)
    if (_mirror_banner_timer > 0)
        GameRenderer::draw_phase_banner(sw, sh, _mirror_banner_phase,
                                        _mirror_banner_timer);

    // M4.2: 镜像冻结 overlay (优先级高于玩家时停 — 显示红霜)
    if (player_frozen_by_mirror()) {
        _renderer.draw_mirror_freeze_overlay(sw, sh, _boss.mirror_freeze_remaining());
        // G10.7-fix: 冻结期间 Echo 常驻紫环 — 玩家看到"谁在维持时停"
        if (_get_boss() && _get_boss()->combat.is_alive) {
            auto* mb = _get_boss();
            float pulse = 34 + sinf((float)GetTime() * 8.0f) * 6;
            DrawRing({mb->entity.rect.x + mb->entity.rect.width/2 - _cam_x,
                      mb->entity.rect.y + mb->entity.rect.height/2 - _cam_y},
                     pulse, pulse + 5, 0, 360, 20, Color{130, 80, 220, 170});
        }
    } else if (time_stop_remaining > 0) {
        _renderer.draw_time_stop_overlay(sw, sh, time_stop_remaining);
    }
    if (state == GameState::BOSS_CINEMATIC && _boss_entrance_timer > 0) {
        // B15/F15.5: Boss entrance cinematic
        DrawRectangle(0, 0, sw, sh, {0, 0, 0, 160});
        bool is_echo = (boss_floor == 15 && _boss._behavior_type == "mirror");
        GameRenderer::draw_glow_text(is_echo ? "终焉回响" : "!!! BOSS 登场 !!!",
            sw / 2.0f, sh / 2.0f - 20,
            is_echo ? 36 : 40,
            is_echo ? Color{180, 40, 40, 255} : Color{255, 60, 40, 255}, true);
        if (is_echo) {
            GameRenderer::draw_glow_text("你面对的是过去的自己",
                sw / 2.0f, sh / 2.0f + 20, 22, {200, 160, 140, 220}, true);
        }
        auto* boss = _get_boss();
        if (boss) {
            GameRenderer::draw_glow_text(is_echo ? "ENDING ECHO" : boss->name.c_str(),
                sw / 2.0f, sh / 2.0f + 50, 28,
                is_echo ? Color{255, 80, 40, 255} : boss->color, true);
        }
    } else if (state == GameState::BOSS_CINEMATIC) {
        _renderer.draw_boss_cinematic_overlay(sw, sh);
    }

    // D5 Step6: BossCinematic overlay (Phase2/LastStand vignette)
    if (_boss.cinematic.is_running()) {
        float a = std::min(1.0f, _boss.cinematic.timer());
        Color edge = {0,0,0, (unsigned char)(80 * a)};
        // simple 3-bar vignette at screen edges (top/bottom)
        DrawRectangle(0, 0, sw, 40, edge);
        DrawRectangle(0, sh-40, sw, 40, edge);
        if (g_font_loaded) {
            const char* pname = _boss.cinematic.phase_name();
            if (pname && strcmp(pname, "NONE") != 0) {
                float tw = MeasureTextEx(g_font_small, pname, 22, 1).x;
                DrawTextEx(g_font_small, pname, {sw/2.0f - tw/2, sh/2.0f - 12},
                           22, 1, {255,200,50,(unsigned char)(220*a)});
            }
        }
    }
}
void GameScene::_draw_map() {
    if (game_map) game_map->draw(_cam_x, _cam_y, get_tree()->get_width(), get_tree()->get_height());
}

// G9.4: 交互提示气泡 — 黑底黄字小标签 (E 交互)
static void _draw_interact_hint(const char* text, float cx, float cy) {
    if (!g_font_loaded) return;
    float tw = MeasureTextEx(g_font_small, text, 10, 1).x;
    DrawRectangle((int)(cx - tw/2 - 4), (int)(cy - 5), (int)(tw + 8), 17,
                  {25, 25, 30, 210});
    DrawRectangleLines((int)(cx - tw/2 - 4), (int)(cy - 5), (int)(tw + 8), 17,
                       {255, 210, 60, 220});
    DrawTextEx(g_font_small, text, {cx - tw/2, cy - 3}, 10, 1,
               {255, 225, 110, 255});
}

void GameScene::_ensure_player_avatar() {
    if (!player || _player_avatar) return;
    auto avatar = std::make_unique<PlayerAvatar>();
    std::string avatar_err;
    if (avatar->try_init("resources/animations", avatar_err))
        LOG_INFO("A5: player avatar active (skeletal)");
    else
        LOG_WARN("A5: avatar inactive, fallback static (%s)", avatar_err.c_str());
    _player_avatar = std::move(avatar);
}

void GameScene::_player_avatar_tick() {
    if (_player_avatar && _player_avatar->active() && player)
        _player_avatar->update(GetFrameTime(), *player);
}

// A6-S1: 怪物骨骼皮肤 — 白名单命中懒建一次 (成败都缓存), 与玩家同款渲染驱动
void GameScene::_monster_avatars_tick() {
    if (!_actor_avatars_loaded) {
        _actor_avatars_loaded = true;
        std::string conf_err;
        auto conf = load_actor_avatars_file("resources/animations/actor_avatars.json", conf_err);
        if (conf) _actor_avatars = std::move(*conf);
        else LOG_WARN("A6: actor_avatars.json invalid, all fallback (%s)", conf_err.c_str());
    }
    if (_actor_avatars.empty()) return;    // 白名单空 = 零开销全回退
    const float dt = GetFrameTime();
    const float now_wall = (float)GetTime();
    for (auto& m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        if (!m->skeleton_avatar()) {
            auto it = _actor_avatars.find(monster_actor_key(*m));
            if (it == _actor_avatars.end()) continue;
            auto avatar = std::make_unique<SkeletonAvatar>();
            std::string avatar_err;
            if (avatar->try_init(it->second.skeleton, it->second.anim, avatar_err))
                LOG_INFO("A6: monster avatar active (%s)", it->first.c_str());
            else
                LOG_WARN("A6: monster avatar inactive (%s): %s",
                         it->first.c_str(), avatar_err.c_str());
            m->set_skeleton_avatar(std::move(avatar));   // 成功/失败都缓存不重试
        }
        auto* avatar = m->skeleton_avatar();
        if (!avatar || !avatar->active()) continue;
        avatar->advance(dt, monster_anim_input(*m, avatar->hp_state(), now_wall));
        avatar->track_facing(m->entity.position);        // 渲染层朝向镜像
    }
}

SkeletonAvatar* GameScene::npc_avatar(int npc_id) {
    auto it = _npc_avatars.find(npc_id);
    return it == _npc_avatars.end() ? nullptr : it->second.get();
}

// A6-S2 批次6: NPC 骨骼 — 白名单命中懒建一次 (成败都缓存), idle-only
void GameScene::_npc_avatars_tick() {
    if (!_actor_avatars_loaded) {
        _actor_avatars_loaded = true;
        std::string conf_err;
        auto conf = load_actor_avatars_file("resources/animations/actor_avatars.json",
                                            conf_err);
        if (conf) _actor_avatars = std::move(*conf);
        else LOG_WARN("A6: actor_avatars.json invalid, all fallback (%s)",
                      conf_err.c_str());
    }
    if (_actor_avatars.empty()) return;
    const float dt = GetFrameTime();
    for (int i = 0; i < _npc_count; i++) {
        if (_npc_state[i].finished) continue;
        const int id = _npc_state[i].id;
        auto found = _npc_avatars.find(id);
        if (found == _npc_avatars.end()) {
            const std::string key = "npc_" + std::to_string(id);
            auto it = _actor_avatars.find(key);
            std::unique_ptr<SkeletonAvatar> avatar;
            if (it != _actor_avatars.end()) {
                avatar = std::make_unique<SkeletonAvatar>();
                std::string avatar_err;
                if (avatar->try_init(it->second.skeleton, it->second.anim, avatar_err))
                    LOG_INFO("A6: npc avatar active (%s)", key.c_str());
                else {
                    LOG_WARN("A6: npc avatar inactive (%s): %s",
                             key.c_str(), avatar_err.c_str());
                    avatar.reset();
                }
            }
            found = _npc_avatars.emplace(id, std::move(avatar)).first;
        }
        auto* avatar = found->second.get();
        if (!avatar || !avatar->active()) continue;
        avatar->advance(dt, AnimInput{});   // NPC 静止 → 默认全 false = idle
    }
}

void GameScene::_draw_entities() {
    // G9.4: 玩家瓦片 (交互相邻判断)
    std::pair<int,int> ppl = player && game_map
        ? game_map->pixel_to_tile(
            player->entity.rect.x + player->entity.rect.width/2,
            player->entity.rect.y + player->entity.rect.height/2)
        : std::pair<int,int>{0, 0};
    int ptx = ppl.first, pty = ppl.second;
    for (auto& m : monsters) {
        // Phase 1: 实体中心 tile 不可见 → 跳过渲染
        bool label_visible = true;
        if (game_map) {
            auto [mtx, mty] = game_map->pixel_to_tile(
                m->entity.rect.x + m->entity.rect.width / 2,
                m->entity.rect.y + m->entity.rect.height / 2);
            if (!game_map->isVisible(mtx, mty)) continue;
            // M6-v2d: 墙后名条裁剪 — 玩家→怪无视线则隐藏名条 (本体仍画)
            label_visible = game_map->has_line_of_sight(ptx, pty, mtx, mty);
        }
        m->draw(_cam_x, _cam_y);
        // F10.2: Weak point glow
        if (m->is_weak_point && m->combat.is_alive) {
            float ex = m->entity.rect.x + m->entity.rect.width/2 - _cam_x;
            float ey = m->entity.rect.y + m->entity.rect.height/2 - _cam_y;
            float pulse = 14.0f + sinf((float)GetTime() * 8.0f) * 4.0f;
            DrawRing({ex, ey}, pulse - 3, pulse + 3, 0, 360, 16,
                {255, 120, 30, 180});
            DrawCircleLines(ex, ey, pulse, {255, 80, 20, 200});
        }
        float mx = m->entity.rect.x + m->entity.rect.width/2 - _cam_x;
        float my = m->entity.rect.y - 14 - _cam_y;
        // G9: name label above monster (small, tinted by type)
        Color nc = m->is_boss ? Color{255,80,40,200}
                 : m->is_elite ? Color{255,180,60,180}
                 : Color{200,200,200,140};
        if (g_font_loaded && !m->name.empty() && label_visible) {
            float tw = MeasureTextEx(g_font_small, m->name.c_str(), 10, 1).x;
            DrawTextEx(g_font_small, m->name.c_str(),
                {mx - tw/2, my - 4}, 10, 1, nc);
        }
        _renderer.draw_monster_buffs(*m,
            m->entity.position.x - _cam_x,
            m->entity.position.y - _cam_y);
        // M4a: Boss 连招技能渲染 (弹幕光球 + 扇形预警 + 瞬移落点)
        if (m->is_boss && m->ai) {
            if (auto* bai = dynamic_cast<BossAI*>(m->ai)) {
                if (bai->barrage_skill())
                    bai->barrage_skill()->draw(_cam_x, _cam_y);
                if (bai->cone_skill() && player)
                    bai->cone_skill()->draw(m.get(), player.get(), _cam_x, _cam_y);
                if (bai->blink_skill())
                    bai->blink_skill()->draw(_cam_x, _cam_y);
                if (bai->whirlwind_skill())
                    bai->whirlwind_skill()->draw(m.get(), _cam_x, _cam_y);
            }
        }
        // D2 Step4: Tank守护连线 (淡蓝色)
        if (m->ai && m->team_role == TeamRole::FRONTLINE && m->ai->_protect_target) {
            float x1 = m->entity.rect.x + m->entity.rect.width/2 - _cam_x;
            float y1 = m->entity.rect.y + m->entity.rect.height/2 - _cam_y;
            auto* t = m->ai->_protect_target;
            float x2 = t->entity.rect.x + t->entity.rect.width/2 - _cam_x;
            float y2 = t->entity.rect.y + t->entity.rect.height/2 - _cam_y;
            DrawLineEx({x1, y1}, {x2, y2}, 1.5f, {60, 140, 255, 100});
        }
    }
    if (player) {
        if (_player_avatar && _player_avatar->active()) {
            _player_avatar->draw(*player, _cam_x, _cam_y, game_map.get());
        } else {
            player->draw_no_cam(_cam_x, _cam_y, game_map.get());
        }
    }

    // D4 Step4: NPC — sprite (floor-based lookup) + name label
    static const struct { int floor; const char* name; } _npc_lookup[] = {
        {2,"埃德加"},{3,"瑞卡"},{4,"卡利安"},{6,"卡兹"},{7,"泰伦斯"},
        {8,"维拉"},{9,"索拉斯"},{11,"迷失灵魂"},{12,"眠者"},{14,"守望者"}
    };
    for (int i = 0; i < _npc_count; i++) {
        if (_npc_state[i].finished) continue;
        // Phase 1: NPC tile 不可见 → 跳过渲染
        if (game_map && !game_map->isVisible(_npc_tile_x[i], _npc_tile_y[i])) continue;
        float nx = _npc_tile_x[i] * TILE_SIZE + TILE_SIZE/2 - _cam_x;
        float ny = _npc_tile_y[i] * TILE_SIZE + TILE_SIZE/2 - _cam_y;
        const char* nn = "NPC";
        for (auto& lk : _npc_lookup) {
            if (lk.floor != current_floor) continue;
            nn = lk.name;
            break;
        }
        float s = TILE_SIZE - 4;
        float sx = nx - s/2, sy = ny - s/2;
        SkeletonAvatar* npc_sk = npc_avatar(_npc_state[i].id);
        if (npc_sk && npc_sk->active()) {
            npc_sk->draw_at({nx, ny + s * 0.5f}, 1.f, 255);
        } else {
            SpriteDef sdef;
            Texture2D stex = ResourceManager::inst().sprite_by_key(
                npc_sprite_key(current_floor), sdef);
            if (stex.id > 0) {
                SpriteRenderer::draw_sprite(stex, sdef, 0, {sx, sy, s, s});
            } else {
                float pulse = 4 + sinf((float)GetTime() * 4) * 2;
                DrawCircle(nx, ny - 10, pulse, {100, 220, 140, 180});
                DrawCircle(nx, ny - 10, 3, {60, 180, 80, 255});
            }
        }
        // G9: NPC name label
        if (g_font_loaded) {
            float tw = MeasureTextEx(g_font_small, nn, 10, 1).x;
            DrawTextEx(g_font_small, nn, {nx - tw/2, ny - 26}, 10, 1, {180,240,180,200});
            // G9.4: 玩家相邻 → E 对话提示
            if (abs(_npc_tile_x[i] - ptx) <= 1 && abs(_npc_tile_y[i] - pty) <= 1)
                _draw_interact_hint("E 对话", nx, ny - 42);
        }
    }

    // G9.4: 事件房间未触发 → E 调查
    if (game_map && game_map->event_room_index >= 0 && !game_map->event_triggered) {
        if (ptx == game_map->event_tile_x && pty == game_map->event_tile_y) {
            float evx = game_map->event_tile_x * TILE_SIZE + TILE_SIZE/2 - _cam_x;
            float evy = game_map->event_tile_y * TILE_SIZE + TILE_SIZE/2 - _cam_y;
            _draw_interact_hint("E 调查", evx, evy - 30);
        }
    }
}

// ── C1/M6-v2b: 伤害飘字核心样式 (2D/3D 共用; 坐标由调用方算) ──
void GameScene::_render_damage_text(
        const PresentationSystemDirector::DamageFloat& df, float sx, float sy) {
    float life_ratio = df.max_lifetime > 0.0f ? df.lifetime / df.max_lifetime : 0.0f;
    if (life_ratio < 0.0f) life_ratio = 0.0f;
    unsigned char a = (unsigned char)(df.color.a * life_ratio);
    Color c = df.color; c.a = a;
    if (df.label) {
        // G10: element effect label (缓/冻/毒/暴)
        GameRenderer::draw_glow_text(df.label, sx, sy, 18, c, true);
    } else {
        char buf[16]; snprintf(buf, sizeof(buf), "%d", df.value);
        // G10.4-B Fix2: 暴击/重击数字 1.6x 字号 (接线 DMG_FLOAT_SCALE_CRIT)
        float size = (16 + df.value / 10) * (df.max_lifetime > 0.7f
            ? CombatFeelSystem::DMG_FLOAT_SCALE_CRIT : 1.0f);
        GameRenderer::draw_glow_text(buf, sx, sy, size, c, true);
    }
}

// 2D 路径: 世界坐标 - 相机偏移 (含上浮)
void GameScene::_render_damage_floats_2d() {
    for (auto& df : _presentation.damage_floats) {
        float sx = df.x - _cam_x, sy = df.y - _cam_y
                 - (df.max_lifetime - df.lifetime) * 30;
        _render_damage_text(df, sx, sy);
    }
}

// 3D 路径 (M6-v2b): 世界坐标 → 3D 投影 (含上浮)
void GameScene::_render_damage_floats_3d() {
    auto& hd2d = HD2DRenderer::inst();
    if (!hd2d.is_ready()) return;
    for (auto& df : _presentation.damage_floats) {
        float rise = (df.max_lifetime - df.lifetime) * 30;
        Vector2 s = hd2d.world_to_screen({df.x, 0, df.y}, 30.0f - rise);
        if (s.x < 0) continue;
        _render_damage_text(df, s.x, s.y);
    }
}

// ── M6-v2a: 3D 分支 UI 桥 — HUD 完整参数 + 全 overlay (方案一: 屏幕空间 UI) ──
// 对应 2D 分支 _render_ui_tail; 世界坐标类 overlay (伤害飘字/E提示) 在 v2b 投影
void GameScene::_render_hd2d_ui_bridge(int sw, int sh) {
    _ambient.draw_vignette(sw, sh);
    _render_hd2d_world_labels();             // M6-v2a: 怪名条/NPC名/E 气泡 (投影)
    _render_damage_floats_3d();               // M6-v2b: 伤害飘字 (投影)
    CharacterPanelData echo_panel_data;
    _build_echo_panel_data(echo_panel_data);
    int ch_wave = (_challenge.phase() == ChallengePhase::COMBAT ||
                   _challenge.phase() == ChallengePhase::WAVE_SPAWNING)
                  ? _challenge.current_wave() + 1 : -1;
    _renderer.draw_hud(player.get(), current_floor, game_time,
                       _get_boss(), _show_relic_panel,
                       inventory_open, inventory_cursor,
                       _presentation.room_msg, _presentation.room_msg_timer,
                       sw, sh,
                       echo_panel_data.mirror_mode ? &echo_panel_data : nullptr,
                       ch_wave, _challenge.total_waves());
    _render_mirror_hud_overlay(sw, game_time);   // v1.6-B1.1: 顶部分析/观察卡
    _render_ui_tail(sw, sh);   // 红屏/黑屏/面板/小地图/对话/事件/冻结/演出 全套
}

// ── M6-v2a: 3D 世界标签 — 怪名/NPC名/E 气泡 (投影到屏幕空间, 2D 同款样式) ──
// 红线: 只读状态; 投影失败(相机未就绪)静默跳过
void GameScene::_render_hd2d_world_labels() {
    _render_hd2d_monster_labels();
    _render_hd2d_interact_hints();
}

// 怪物名条 (2D 同款: Boss红/精英金/普通灰)
void GameScene::_render_hd2d_monster_labels() {
    auto& hd2d = HD2DRenderer::inst();
    if (!hd2d.is_ready() || !g_font_loaded) return;
    for (auto& m : monsters) {
        if (!m || !m->combat.is_alive || m->name.empty()) continue;
        int ptx = -99, pty = -99;
        if (player && game_map) {
            auto ppl = game_map->pixel_to_tile(
                player->entity.rect.x + player->entity.rect.width/2,
                player->entity.rect.y + player->entity.rect.height/2);
            ptx = ppl.first; pty = ppl.second;
        }
        if (game_map) {
            auto [mtx, mty] = game_map->pixel_to_tile(
                m->entity.rect.x + m->entity.rect.width / 2,
                m->entity.rect.y + m->entity.rect.height / 2);
            if (!game_map->isVisible(mtx, mty)) continue;
            // M6-v2d: 墙后名条裁剪 (与 2D 名条同条件 — has_line_of_sight)
            if (!game_map->has_line_of_sight(ptx, pty, mtx, mty)) continue;
        }
        Vector3 wpos = {m->entity.rect.x + m->entity.rect.width * 0.5f, 0,
                        m->entity.rect.y + m->entity.rect.height * 0.5f};
        Vector2 s = hd2d.world_to_screen(wpos, 58.0f);   // 头顶高度
        if (s.x < 0) continue;
        Color nc = m->is_boss ? Color{255,80,40,200}
                 : m->is_elite ? Color{255,180,60,180}
                 : Color{200,200,200,140};
        float tw = MeasureTextEx(g_font_small, m->name.c_str(), 10, 1).x;
        DrawTextEx(g_font_small, m->name.c_str(), {s.x - tw/2, s.y - 4}, 10, 1, nc);
    }
}

// E 交互气泡 (NPC 对话 / 地面物品拾取; 玩家相邻时显示)
void GameScene::_render_hd2d_interact_hints() {
    auto& hd2d = HD2DRenderer::inst();
    if (!hd2d.is_ready() || !g_font_loaded) return;
    int ptx = -99, pty = -99;
    if (player && game_map) {
        auto ppl = game_map->pixel_to_tile(
            player->entity.rect.x + player->entity.rect.width/2,
            player->entity.rect.y + player->entity.rect.height/2);
        ptx = ppl.first; pty = ppl.second;
    }
    for (auto& npc : npc_views()) {
        if (game_map && !game_map->isVisible(npc.tile_x, npc.tile_y)) continue;
        if (abs(npc.tile_x - ptx) > 1 || abs(npc.tile_y - pty) > 1) continue;
        Vector2 s = hd2d.world_to_screen(
            {(float)npc.tile_x * TILE_SIZE + TILE_SIZE * 0.5f, 0,
             (float)npc.tile_y * TILE_SIZE + TILE_SIZE * 0.5f}, 56.0f);
        if (s.x >= 0) _draw_interact_hint("E 对话", s.x, s.y - 16);
    }
    for (auto& d : ground_items) {
        if (abs(d.tile_x - ptx) > 1 || abs(d.tile_y - pty) > 1) continue;
        Vector2 s = hd2d.world_to_screen(
            {(float)d.tile_x * TILE_SIZE + TILE_SIZE * 0.5f, 0,
             (float)d.tile_y * TILE_SIZE + TILE_SIZE * 0.5f}, 20.0f);
        if (s.x >= 0) _draw_interact_hint("E 拾取", s.x, s.y - 4);
    }
}

// ── F15.5.1/M6-v2a: Echo 面板数据构建 (2D/3D 共用; 从 _render 提取防复制) ──
void GameScene::_build_echo_panel_data(CharacterPanelData& echo) const {
    bool is_echo = (boss_floor == 15 && _boss._behavior_type == "mirror");
    if (!is_echo) return;
    auto* boss = _get_boss();
    if (!boss) return;
    echo.name = "ENDING ECHO";
    echo.hp = boss->combat.current_hp;
    echo.max_hp = boss->combat.max_hp;
    echo.atk = boss->combat.get_effective_attack();
    echo.pdef = boss->combat.get_effective_defense(AttackType::PHYSICAL);
    echo.mdef = boss->combat.get_effective_defense(AttackType::MAGICAL);
    echo.mirror_mode = true;
    for (auto& sk : player->skills.active_skills) {
        SkillDisplay sd;
        sd.name = sk->name;
        sd.cooldown_ratio = sk->remaining_cooldown(game_time) / sk->cooldown;
        sd.ready = sk->can_use(game_time);
        echo.skills.push_back(sd);
    }
    _fill_echo_buffs(echo);
    if (!_boss._mirror_agent) return;
    echo.mirror_phase = _boss._mirror_agent->current_phase();
    echo.sub_label = _boss._mirror_agent->phase_name();
    echo.mirror_last_action = _boss._mirror_agent->last_action();
    int mb = _boss._mirror_agent->last_bucket();
    if (mb < 0) mb = 0;   // 观察期未决策 → 展示桶0
    for (int i = 0; i < 4; i++)
        echo.mirror_arm_rates[i] = _boss._mirror_agent->arm_win_rate(mb, i);
    _fill_mirror_learn_display(echo);
}

// ── v1.6-B1: "它眼中的你" 数据 (Boss 层常驻; 全部只读自画像/在线统计) ──
void GameScene::_fill_mirror_learn_display(CharacterPanelData& echo) const {
    const auto& agent = *_boss._mirror_agent;
    const PlayerHabitProfile& pf = agent.profile();
    // 在线统计 (观察期就有意义: 已观察数与准确率展示"它在学")
    echo.mirror_observed = agent.observed_actions();
    float acc = agent.prediction_accuracy();
    echo.mirror_accuracy = (echo.mirror_observed >= 5) ? acc : -1.0f;
    echo.mirror_drift = agent.profile_drift();
    snprintf(echo.mirror_style, sizeof(echo.mirror_style), "%s", pf.style_name());
    // Top3 习惯短句: 按画像字段生成 (后写覆盖前写, 手感优先级 < 条件维度)
    int n = 0;
    auto set_habit = [&](const char* fmt, float v) {
        if (n >= 3) return;
        snprintf(echo.mirror_habits[n], sizeof(echo.mirror_habits[0]), fmt, v);
        n++;
    };
    if (pf.predict_attack_heavy) set_habit("重攻轻守: 攻击占比%.0f%%", pf.aggression_score * 100);
    if (pf.predict_low_dodge)   set_habit("几乎不闪避 (%.0f%%)", pf.dodge_rate * 100);
    if (pf.predict_panic_heal)  set_habit("习惯提前治疗 (HP%.0f%%)", pf.hp_counter_threshold);
    if (pf.fight_back_rate > 0.6f) set_habit("受击后硬刚反击 (%.0f%%)", pf.fight_back_rate * 100);
    else if (pf.fight_back_rate < 0.3f && pf.total_actions > 40)
        set_habit("受击后倾向后撤 (%.0f%%)", (1.0f - pf.fight_back_rate) * 100);
    if (pf.attack_rhythm_var > 0 && pf.attack_rhythm_var < 0.25f)
        set_habit("固定攻击节奏 (方差%.2f)", pf.attack_rhythm_var);
    while (n < 3) { echo.mirror_habits[n][0] = '\0'; n++; }
}

// v1.6-B1.1: Mirror HUD overlay — 顶部 reveal 分析卡 + reveal 后接手观察卡
// 只读 MirrorAgent 缓存; 不参与战斗决策; 无 RNG
void GameScene::_render_mirror_hud_overlay(int sw, float game_time) {
    if (!_boss._mirror_agent) return;
    if (_boss._behavior_type != "mirror") return;
    const MirrorAgent& agent = *_boss._mirror_agent;
    MirrorHudPanel::render_analysis_card(agent, sw, game_time);
    // reveal 完成后 (>=5.5s) 由观察卡接手同一位置; 避免同帧两卡重叠
    if (agent.battle_seconds() >= 5.5f) {
        const float obs_w = 340.0f;
        float obs_x = (float)(sw - (int)obs_w) / 2.0f;
        MirrorHudPanel::render_observation_card(agent, obs_x, 4.0f, game_time);
    }
}

// Echo 面板 buff 腐化名映射 (攻/防/毒/缓/冻/血/燃/雷 → 黑化前缀)
void GameScene::_fill_echo_buffs(CharacterPanelData& echo) const {
    static const struct { const char* id, *icon, *label; } MAP[] = {
        {"attack_up", "攻", "Echo Atk"}, {"defense_up", "防", "腐化防御"},
        {"poison", "毒", "腐败毒"}, {"poison2s", "毒", "腐败毒"},
        {"slow", "缓", "暗影缓"}, {"freeze", "冻", "黑冰"},
        {"bleed", "血", "暗血"}, {"burn", "燃", "黑焰"},
        {"electrified", "雷", "暗雷"},
    };
    for (auto& b : player->active_buffs) {
        BuffDisplay bd;
        bd.icon = "?"; bd.label = b.id;
        for (auto& m : MAP)
            if (b.id == m.id) { bd.icon = m.icon; bd.label = m.label; break; }
        echo.buffs.push_back(bd);
    }
}

// ── M6-HD2D: 3D 表现层只读快照 (rendering3d 只读红线, 不给可变访问) ──
std::vector<GameScene::NpcView> GameScene::npc_views() const {
    std::vector<NpcView> out;
    for (int i = 0; i < _npc_count; i++) {
        if (_npc_state[i].finished) continue;
        out.push_back({_npc_tile_x[i], _npc_tile_y[i], false, _npc_state[i].id});
    }
    return out;
}

void GameScene::_draw_ground_items() {
    for (auto& d : ground_items) {
        // Phase 1: 物品 tile 不可见 → 跳过渲染
        if (game_map && !game_map->isVisible(d.tile_x, d.tile_y)) continue;
        float px = d.tile_x * TILE_SIZE - _cam_x;
        float py = d.tile_y * TILE_SIZE - _cam_y;
        float size = TILE_SIZE - 4;
        float cx = px + TILE_SIZE / 2, cy = py + TILE_SIZE / 2;

        // M4f.13: 物品图标 → 数据驱动贴图 (武器/装甲/药水/护符) + 稀有度光环
        const char* ikey = item_icon_key(d.item.get());
        if (ikey) {
            SpriteDef xd;
            Texture2D itex = ResourceManager::inst().sprite_by_key(ikey, xd);
            if (itex.id > 0) {
                float pulse = 6 + sinf((float)GetTime() * 5 + px * 0.1f) * 3;
                DrawRectangleLinesEx({cx - pulse, cy - pulse, pulse * 2, pulse * 2}, 1,
                                     rarity_color(d.item->rarity));
                SpriteRenderer::draw_sprite(itex, xd, 0,
                    {cx - size/2, cy - size/2, size, size});
                continue;
            }
        }

        float pulse = 6 + sinf((float)GetTime() * 5 + px * 0.1f) * 3;
        DrawRectangleLinesEx({cx - pulse, cy - pulse, pulse * 2, pulse * 2}, 1,
                             Color{(unsigned char)(d.item->color.r / 3),
                                   (unsigned char)(d.item->color.g / 3),
                                   (unsigned char)(d.item->color.b / 3), 200});
        DrawRectangleRounded({px + 2, py + 2, size, size}, 0.1f, 4, d.item->color);
        DrawRectangleRoundedLines({px + 2, py + 2, size, size}, 0.1f, 4, 1, BLACK);

        // G9.4: 玩家相邻 → E 拾取提示
        if (player && game_map) {
            auto [itx, ity] = game_map->pixel_to_tile(
                player->entity.rect.x + player->entity.rect.width/2,
                player->entity.rect.y + player->entity.rect.height/2);
            if (abs(d.tile_x - itx) <= 1 && abs(d.tile_y - ity) <= 1)
                _draw_interact_hint("E 拾取", cx, cy - TILE_SIZE/2 - 8);
        }
    }
}

void GameScene::_draw_arena_map() {
    if (!_arena_map) return;
    for (int y = 0; y < _arena_map->height; y++)
        for (int x = 0; x < _arena_map->width; x++) {
            float sx = x * TILE_SIZE - _cam_x;
            float sy = y * TILE_SIZE - _cam_y;
            if (sx + TILE_SIZE < 0 || sx > get_tree()->width() ||
                sy + TILE_SIZE < 0 || sy > get_tree()->height()) continue;
            TileType t = _arena_map->tile_at(x, y);
            if (t == TileType::WALL) DrawRectangle((int)sx, (int)sy, TILE_SIZE, TILE_SIZE, {40, 40, 60, 255});
            else if (t == TileType::FLOOR) DrawRectangle((int)sx, (int)sy, TILE_SIZE, TILE_SIZE, {80, 75, 65, 255});
        }
}

void GameScene::_draw_arena_entities() {
    if (!player || !_arena_map) return;
    player->draw_no_cam(_cam_x, _cam_y, game_map.get());
    for (auto& m : _arena_monsters) {
        if (m && m->combat.is_alive) m->draw(_cam_x, _cam_y);
    }
}

void GameScene::_cleanup_dead_arena_monsters() {
    _arena_monsters.erase(
        std::remove_if(_arena_monsters.begin(), _arena_monsters.end(),
            [](const std::unique_ptr<Monster>& m) { return !m || !m->combat.is_alive; }),
        _arena_monsters.end());
}

// _draw_hud 已迁移到 GameRenderer
// _draw_* methods migrated to GameRenderer

// ============================================================
// D4 Step4: NPC / Dialogue / Quest 实现
// ============================================================
NPCState* GameScene::_find_or_create_npc_state(int npc_id) {
    for (int i = 0; i < _npc_count; i++)
        if (_npc_state[i].id == npc_id) return &_npc_state[i];
    if (_npc_count < 10) {
        _npc_state[_npc_count].id = npc_id;
        return &_npc_state[_npc_count++];
    }
    return nullptr;
}

void GameScene::_spawn_floor_npcs(int floor, const std::vector<std::pair<int,int>>& rooms) { _interaction.spawn_floor_npcs(floor, rooms); }

void GameScene::_start_dialogue(int npc_index) { _interaction.start_dialogue(npc_index); }

void GameScene::_update_dialogue(float dt) { _interaction.update_dialogue(dt); }

void GameScene::_draw_dialogue(int sw, int sh) { _interaction.draw_dialogue(sw, sh); }

void GameScene::_draw_quest_log(int sw, int sh) { _interaction.draw_quest_log(sw, sh); }

// ============================================================
// F15.5: Mirror analysis panel — Ending Echo boss intro
// ============================================================
void GameScene::_draw_mirror_analysis_panel(int sw, int sh) {
    // Build a profile from the recorded action stream
    const auto& history = g_behavior.history();
    PlayerHabitProfile profile = history.empty()
        ? PlayerHabitProfile{}
        : PlayerBehaviorAnalyzer::analyze(history);

    // Background — dark void
    ClearBackground({10, 8, 20, 255});

    float pw = 500, ph = 380;
    float cx = sw/2.0f, cy = sh/2.0f + 10;
    DrawRectangleRounded({cx - pw/2, cy - ph/2, pw, ph}, 0.06f, 10, {18, 12, 28, 240});

    // Title
    const char* title = "终焉回响 · 人格解析";
    float tw = MeasureTextEx(g_font_small, title, 24, 1).x;
    DrawTextEx(g_font_small, title, {cx - tw/2, cy - ph/2 + 20}, 24, 1, {255, 80, 60, 255});

    // Divider
    DrawLine(cx - pw/2 + 30, cy - ph/2 + 55, cx + pw/2 - 30, cy - ph/2 + 55,
        {80, 30, 30, 200});
    DrawLine(cx - pw/2 + 32, cy - ph/2 + 56, cx + pw/2 - 28, cy - ph/2 + 56,
        {120, 20, 20, 100});

    // Style line
    float ly = cy - ph/2 + 75;
    char buf[128];
    snprintf(buf, sizeof(buf), "Combat Style:  %s", profile.style_name());
    DrawTextEx(g_font_small, buf, {cx - pw/2 + 40, ly}, 16, 1, {220, 200, 180, 240});

    // Favorite skill
    ly += 30;
    const char* skill_names[] = {"斩击","神罚","自愈","The World"};
    int fav = profile.predicted_fav_skill;
    const char* fav_name = (fav >= 0 && fav < 4) ? skill_names[fav] : "无偏好";
    snprintf(buf, sizeof(buf), "Fav Skill:  %s (%.0f%%)",
        fav_name, fav >= 0 ? profile.skill_preference[fav]*100 : 0);
    DrawTextEx(g_font_small, buf, {cx - pw/2 + 40, ly}, 16, 1, {220, 200, 180, 240});

    // Weakness
    ly += 30;
    if (profile.predict_attack_heavy)
        snprintf(buf, sizeof(buf), "Weakness: Attack Pattern Predictable");
    else if (profile.predict_low_dodge)
        snprintf(buf, sizeof(buf), "Weakness: Low Evasion");
    else if (profile.predict_panic_heal)
        snprintf(buf, sizeof(buf), "Weakness: Heal Timing Exploitable");
    else
        snprintf(buf, sizeof(buf), "Weakness: Balanced — No Obvious Pattern");
    DrawTextEx(g_font_small, buf, {cx - pw/2 + 40, ly}, 16, 1, {220, 200, 180, 240});

    // Counter strategy
    ly += 35;
    DrawTextEx(g_font_small, "─ Counter Strategy ─",
        {cx - pw/2 + 40, ly}, 14, 1, {180, 140, 60, 220});
    ly += 25;
    snprintf(buf, sizeof(buf), "%s", profile.counter_strategy_text());
    DrawTextEx(g_font_small, buf, {cx - pw/2 + 40, ly}, 13, 1, {200, 180, 160, 200});

    // M1: 实数回执 — "你做了什么, 它看到什么" (镜像分析的具体凭证)
    ly += 30;
    {
        const auto& bd = g_behavior.data();
        snprintf(buf, sizeof(buf),
                 "回执: 挥击x%d · 技能x%d · 闪避x%d · 累计承伤%d",
                 bd.weapon_attacks_total, bd.skill_uses_total,
                 bd.dodge_count, bd.total_damage_taken);
        DrawTextEx(g_font_small, buf, {cx - pw/2 + 40, ly}, 13, 1,
                   {255, 150, 130, 220});
        // 本命技能具体次数 (最爱技能在它手上的注脚)
        if (fav >= 0 && fav < 4) {
            static const char* skill_ids[] = {"slash", "fireball", "self_heal", "the_world"};
            auto it = bd.skill_uses.find(skill_ids[fav]);
            int uses = (it != bd.skill_uses.end()) ? it->second : 0;
            ly += 22;
            snprintf(buf, sizeof(buf), "「%s」共释放 %d 次 — 镜像已掌握",
                     fav_name, uses);
            DrawTextEx(g_font_small, buf, {cx - pw/2 + 40, ly}, 13, 1,
                       {255, 120, 100, 200});
        }
    }

    // Aggression gauge
    ly += 35;
    int ag_bar_w = (int)(pw - 80);
    DrawRectangle(cx - pw/2 + 40, ly, ag_bar_w, 8, {30, 30, 30, 200});
    int fill = (int)(ag_bar_w * profile.aggression_score);
    DrawRectangle(cx - pw/2 + 40, ly, fill, 8, {220, 60, 30, 240});
    snprintf(buf, sizeof(buf), "Aggression: %.0f%%", profile.aggression_score * 100);
    DrawTextEx(g_font_small, buf, {cx - pw/2 + 40, ly + 12}, 12, 1, {180, 180, 180, 180});

    // Floors recorded
    ly += 35;
    snprintf(buf, sizeof(buf), "Data: %d floors | %d actions recorded",
        g_behavior.data().floors_recorded, profile.total_actions);
    DrawTextEx(g_font_small, buf, {cx - pw/2 + 40, ly}, 11, 1, {120, 120, 140, 180});

    // Bottom message
    DrawTextEx(g_font_small, "[Enter] 开始战斗",
        {cx - 60, cy + ph/2 - 36}, 14, 1, {255, 200, 100, 220});
}

// _draw_player_buffs, _draw_player_relics, _draw_relic_panel,
// _draw_monster_buffs, _draw_inventory_panel 已迁移到 GameRenderer
// _draw_effects, _draw_time_stop_overlay, _draw_boss_cinematic_overlay, _draw_boss_intro
// 已迁移到 GameRenderer

// Batch 3I: WorldMode + Challenge Arena transition
void GameScene::enter_challenge_arena() {
    if (_world_mode != WorldMode::DUNGEON) return;
    _saved_player_x = player->entity.position.x;
    _saved_player_y = player->entity.position.y;

    // Save dungeon state
    _saved_dungeon_map = game_map;
    _saved_dungeon_monsters = std::move(monsters);
    _saved_dungeon_ground_items = std::move(ground_items);
    monsters.clear();
    ground_items.clear();

    // Create a 15x15 arena map
    const int AW = 15, AH = 15, TS = TILE_SIZE;
    _arena_map = std::make_shared<GameMap>(AW, AH, TS);
    for (int y = 0; y < AH; y++)
        for (int x = 0; x < AW; x++)
            _arena_map->set_tile(x, y, (x == 0 || x == AW-1 || y == 0 || y == AH-1)
                ? TileType::WALL : TileType::FLOOR);

    // Switch to arena map
    game_map = _arena_map;

    // Teleport player to center
    auto [px, py] = _arena_map->tile_to_pixel(AW/2, AH/2);
    player->entity.position = {(float)px, (float)py};
    player->entity.sync_rect();
    _arena_map->update_fov(AW/2, AH/2, _fov_radius);

    // Setup challenge controller for arena
    _challenge.set_room_rect(1, 1, AW-2, AH-2);
    _challenge.set_return_portal(AW/2, AH/2);

    _world_mode = WorldMode::CHALLENGE_ARENA;
    _teleport_fade_timer = 0.3f;
    _portal_fade_in = true;
    _portal_pulse_timer = 0.0f;

    // G10.7-B: 竞技场专属 BGM (160bpm 小调急促波次战斗曲)
    if (get_tree()) get_tree()->get_audio()->play_bgm("challenge", 0.42f);
}

void GameScene::exit_challenge_arena() {
    if (_world_mode != WorldMode::CHALLENGE_ARENA) return;
    player->entity.position = {_saved_player_x, _saved_player_y};
    player->entity.sync_rect();

    // Save arena monsters for potential re-entry
    _arena_monsters = std::move(monsters);
    monsters.clear();

    // Restore dungeon state
    game_map = _saved_dungeon_map;
    monsters = std::move(_saved_dungeon_monsters);
    _saved_dungeon_monsters.clear();
    ground_items = std::move(_saved_dungeon_ground_items);
    _saved_dungeon_ground_items.clear();
    _saved_dungeon_map.reset();

    // G9.2 (audit LIFE-003): 不调用 reset_visibility — 它会清空 is_explored,
    // 吞掉本层探索进度 (minimap/探索)。下方 update_fov 已负责重建当前帧可见性。
    auto [tx, ty] = game_map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width/2,
        player->entity.rect.y + player->entity.rect.height/2);
    game_map->update_fov(tx, ty, _fov_radius);
    _world_mode = WorldMode::DUNGEON;
    _teleport_fade_timer = 0.3f;
    _portal_fade_in = false;
    _challenge.reset();

    // G10.7-B: 退出竞技场 — 恢复当前楼层群系 BGM (走现有延迟播放管线)
    if (get_tree()) {
        const FloorConfig* fcfg = get_floor_config(current_floor);
        if (fcfg && fcfg->bgm && fcfg->bgm[0])
            _pending_bgm = fcfg->bgm;
    }
}

bool GameScene::is_save_blocked() const {
    auto ph = _challenge.phase();
    return ph == ChallengePhase::ARMED ||
           ph == ChallengePhase::WAVE_SPAWNING ||
           ph == ChallengePhase::COMBAT ||
           ph == ChallengePhase::WAIT_NEXT_WAVE;
}
