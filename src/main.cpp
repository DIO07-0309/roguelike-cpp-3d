// ============================================================
// Roguelike C++ Edition
// 重庆大学大数据与软件学院 · 程序设计实训
// 开发者：ruozhiDIO
// ============================================================
#include "raylib.h"
#include "core/scene_tree.h"
#include "core/win_center.h"     // G10.7-B1: gui_sanitize_stdio
#include "game/audio/audio_server.h"   // Q3.1: --sim 静音
#include "game/rendering3d/hd2d_renderer.h"  // M6-HD2D: --hd2d
#include "core/logger.h"
#include "scenes/game_scene.h"
#include "scenes/title_scene.h"
#include "save/save_manager.h"
#include "meta_progression.h"   // Q3.1: --sim 只读保护
#include "systems/combat_system.h"
#include "resources/resource_manager.h"
#include "core/service_locator.h"
#include "core/event_bus.h"
#include "config.h"
#include "core/registry_builder.h"     // G4.1
#include "core/builtin_provider.h"     // G4.1
#include "core/mod_provider.h"         // G4.1
#include "core/mod_dependency.h"      // G4.2
// G6: World layer data
#include "world/biome.h"
#include "world/landmark.h"
#include "world/encounter.h"
#include "spawn_tables.h"      // A6-S2 批次9: 楼层槽位选怪表 + 挑战房刷怪池
#include "data/vfx_recipe.h"
#include "core/sim/sim_runner.h"      // G5.6
#include "core/mod_manager.h"         // G4.4
#include "data/enemy_defs.h"
#include "data/boss_defs.h"
#include "data/dialogue_defs.h"
#include "data/quest_defs.h"
#include "data/ending_defs.h"
#include "data/meta_node_defs.h"
#include "data/skill_defs.h"
#include "data/item_defs.h"
#include "data/weapon_defs.h"     // G9
#include "data/element_defs.h"    // G10
#include "data/vfx_recipe.h"      // G5.8.5
#include "systems/combat_system.h"
#include "ai/rl/environment.h"         // G8.4
#include "ai/rl/random_agent.h"        // G8.4
#include "ai/rl/q_agent.h"             // G8.4
#include "ai/mcts/simulation_state.h"   // G8.3
#include <cstdio>
#include <memory>
#include <exception>
#include <cstring>
#include <string>
#ifdef _WIN32
#include <direct.h>
#endif

// 全局字体 (向后兼容 — 由 ResourceManager 管理)
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

// ── G8.4: RL standalone runner (defined in ai/rl/rl_runner.cpp) ──
extern void run_rl_mode(int test_episodes, int train_episodes);
extern void run_rl_mirror_mode(int episodes);  // F15.4

static void load_fonts() {
    ResourceManager::inst().load_all();
    g_font = ResourceManager::inst().font(32);
    g_font_small = ResourceManager::inst().font_small();
    g_font_loaded = ResourceManager::inst().font_loaded();
}

static void _terminate_handler() {
    FILE* cf = fopen("crash.log", "w");
    if (cf) {
        time_t now = time(nullptr); struct tm tm_buf;
        localtime_s(&tm_buf, &now); char ts[32];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);
        fprintf(cf, "[%s] CRASH\nC++ std::terminate\n开发者：ruozhiDIO\n", ts);
        fclose(cf);
    }
    fprintf(stderr, "\n致命错误! 详见 crash.log\n按 Enter 退出...");
    getchar();
}

// M4f.6: 工作目录修复 — 兼容 exe 在 build/ 或项目根两种启动布局
// 资源全部使用相对路径 (resources/ assets/), 必须切到含 resources/ 的目录
#ifdef _WIN32
static void _fix_working_dir(const char* argv0) {
    if (!argv0 || !*argv0) return;
    std::string p(argv0);
    auto slash = p.find_last_of("\\/");
    std::string dir = (slash == std::string::npos) ? "."
                                                  : p.substr(0, slash);
    for (int i = 0; i < 2; i++) {
        std::string cand;
        if (i == 0) {
            cand = dir;
        } else {
            auto sl2 = dir.find_last_of("\\/");
            cand = (sl2 == std::string::npos) ? "." : dir.substr(0, sl2);
        }
        std::string probe = cand + "\\resources\\sprites.json";
        FILE* f = nullptr;
        if (fopen_s(&f, probe.c_str(), "rb") == 0) {
            fclose(f);
            _chdir(cand.c_str());
            return;
        }
    }
}
#endif

// M6-i.1: --hidwin 把窗口移到屏幕外 (取证静默; HWND 仍有效, PostMessage 可用)
extern bool g_window_hidden;
extern bool g_autocontinue;
extern int  g_goto_floor;

int main(int argc, char** argv) {
    Logger::inst().init();
#ifdef _WIN32
    _fix_working_dir(argc > 0 ? argv[0] : "");
    install_seh_handler();
    gui_sanitize_stdio();   // G10.7-B1: WIN32 GUI 子系统下 stdout/stderr 保活
#endif
    std::set_terminate(_terminate_handler);

    LOG_INFO("Roguelike C++ Edition 启动 %s %s", __DATE__, __TIME__);
    SetTraceLogLevel(LOG_WARNING);

    // ── G4.5/G7.3: CLI replay/record/sim flags ──
    bool replay_mode = false, record_mode = false, sim_mode = false;
    bool input_diag_requested = false;  // P1-C9: --input-diag
    std::string replay_path, record_path;
    int sim_runs = 100;
    uint32_t sim_seed_start = 0;
    std::string sim_build;
#ifdef _WIN32
    for (int i = 1; i < __argc; i++) {
        std::string arg = __argv[i];
        if (arg == "--replay" && i + 1 < __argc) {
            replay_mode = true;
            replay_path = __argv[++i];
        } else if (arg == "--record" && i + 1 < __argc) {
            record_mode = true;
            record_path = __argv[++i];
        } else if (arg == "--sim" && i + 1 < __argc) {
            sim_mode = true;
            sim_runs = atoi(__argv[++i]);
        } else if (arg == "--sim-seed" && i + 1 < __argc) {
            sim_seed_start = (uint32_t)atoi(__argv[++i]);
        } else if (arg == "--sim-build" && i + 1 < __argc) {
            sim_build = __argv[++i];
        } else if (arg == "--sim-all-builds") {
            sim_mode = true;
            GameScene::g_sim_all_builds = true;
        } else if (arg == "--sim-ai" && i + 1 < __argc) {
            GameScene::g_sim_ai_type = __argv[++i];
        } else if (arg == "--rl-test" && i + 1 < __argc) {
            GameScene::g_rl_test_episodes = atoi(__argv[++i]);
        } else if (arg == "--rl-train" && i + 1 < __argc) {
            GameScene::g_rl_train_episodes = atoi(__argv[++i]);
        } else if (arg == "--rl-mirror" && i + 1 < __argc) {
            GameScene::g_rl_mirror_episodes = atoi(__argv[++i]);
        } else if (arg == "--hd2d") {
            // M6-HD2D: 3D 表现层切片开关 (逻辑层不变; 默认 2D)
            g_hd2d_mode = true;
        } else if (arg == "--input-diag") {
            // P1-C9: 输入心跳诊断 (键盘失灵复发时定位用; 默认关)
            input_diag_requested = true;
        } else if (arg == "--autoshot" && i + 1 < __argc) {
            // M6-i.1: 3D 渲染第 N 帧自动截图 (视觉取证, 不依赖键盘)
            extern int g_hd2d_autoshot;
            g_hd2d_autoshot = atoi(__argv[++i]);
        } else if (arg == "--hidwin") {
            // M6-i.1: 窗口移出屏幕 (后台取证不弹窗干扰)
            g_window_hidden = true;
        } else if (arg == "--autocontinue") {
            // M6-i.1: 跳过标题/选档, 直接读 slot1 继续游戏 (取证免键入)
            g_autocontinue = true;
        } else if (arg == "--goto-floor" && i + 1 < __argc) {
            // M6-i.1: 覆盖读档楼层 (取证指定 F1/F6/F11)
            g_autocontinue = true;
            g_goto_floor = atoi(__argv[++i]);
        }
    }
#endif

    // ── G4.5/G7.3: Set replay/record/sim config ──
    GameScene::g_record_mode = record_mode;
    GameScene::g_replay_mode = replay_mode;
    GameScene::g_record_path = record_path;
    GameScene::g_replay_path = replay_path;
    GameScene::g_sim_mode  = sim_mode;    // G5.6
    GameScene::g_sim_runs  = sim_runs;    // G5.6
    if (record_mode) LOG_INFO("Replay: recording to %s", record_path.c_str());
    if (replay_mode) LOG_INFO("Replay: playing from %s", replay_path.c_str());
    if (g_window_hidden) {
        // M6-i.1: 后台取证静音 (不写 meta/存档以免污染)
        AudioServer::g_muted = true;
    }
    if (sim_mode) {
        SimulationConfig sim_cfg;
        sim_cfg.runs = sim_runs;
        sim_cfg.seed_start = sim_seed_start;
        if (!sim_build.empty()) {
            sim_cfg.random_build = false;
            sim_cfg.fixed_build = true;
            sim_cfg.fixed_build_id = sim_build;
        }
        AudioServer::g_muted = true;  // Q3.1: headless 静音
        SaveManager::g_sim_readonly = true;   // Q3.1: sim 不覆盖玩家存档
        g_meta.g_readonly = true;             // Q3.1: sim 不写 meta 存档
        auto& sr = SimRunner::inst();
        if (GameScene::g_sim_all_builds) sr.set_all_builds(true);
        sr.begin(sim_cfg);
        LOG_INFO("Sim: %d runs", sim_runs);
    }

    SceneTree tree(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);  // G10.7-B1: 统一 config.h 常量
    if (input_diag_requested) tree.set_input_diag(true);  // P1-C9
    ServiceLocator::provide(&tree);  // Q4.4: 事件回调访问音频
    LOG_INFO("窗口创建");

    // ═══ G4.1: RegistryBuilder — Provider 驱动加载 ─═
    LOG_INFO("── Registry Build ──");
    RegistryBuilder builder;
    builder.add_provider(std::make_unique<BuiltinProvider>());

    // ── 注册模块加载器 (各 Def 模块) ──
    builder.register_module("buff",     load_buff_defs_from_json);
    builder.register_module("relic",    load_relic_defs_from_json);
    builder.register_module("enemy",    load_enemy_defs_from_json);
    builder.register_module("boss",     load_boss_defs_from_json);
    builder.register_module("dialogue", load_dialogue_defs_from_json);
    builder.register_module("quest",    load_quest_defs_from_json);
    builder.register_module("ending",   load_ending_defs_from_json);
    builder.register_module("meta",     load_meta_node_defs_from_json);
    builder.register_module("skill",    load_skill_defs_from_json);
    builder.register_module("item",     load_item_defs_from_json);
    builder.register_module("weapon",   load_weapon_defs_from_json);
    builder.register_module("element",  load_element_defs_from_json);

    // ── G4.4: ModManager 驱动 + 依赖解析 ──
    {
        ModManager mod_mgr;
        mod_mgr.scan("mods");

        // Collect enabled mod providers
        std::vector<std::unique_ptr<ModProvider>> mods_temp;
        std::vector<ModDepInfo> dep_info;
        for (auto& mi : mod_mgr.all()) {
            if (!mi.enabled || !mi.valid) continue;
            auto mp = ModProvider::create(mi.dir_path);
            if (!mp) continue;
            ModDepInfo di;
            di.id = mp->manifest().id;
            di.requires_ids = mp->manifest().requires_ids;
            di.load_after = mp->manifest().load_after;
            dep_info.push_back(di);
            mods_temp.push_back(std::move(mp));
        }

        // Dependency resolve
        auto dep_result = DependencyResolver::resolve(dep_info);
        for (auto& id : dep_result.ordered_ids) {
            for (auto& mp : mods_temp) {
                if (mp && mp->manifest().id == id) {
                    builder.add_provider(std::move(mp));
                    break;
                }
            }
        }
        for (auto& s : dep_result.skipped)
            LOG_INFO("  [SKIP] Mod '%s': dependency missing", s.c_str());
        for (auto& c : dep_result.cycle_info)
            LOG_INFO("  [SKIP] Mod '%s': cyclic dependency", c.c_str());

        LOG_INFO("── Mods: %d/%d enabled ──",
                 mod_mgr.enabled_count(), mod_mgr.total_count());
    }

    if (!builder.build_all())
        LOG_ERROR("  [FAIL] Registry build");
    for (auto& rec : builder.log())
        LOG_INFO("  [%s] %s ← %s%s",
            rec.module.c_str(), rec.provider.c_str(), rec.source.c_str(),
            rec.is_merge ? " (merge)" : "");
    {
        auto& fs = builder.findings();
        if (!fs.empty()) {
            LOG_INFO("── Validator: %zu issues ──", fs.size());
            for (auto& f : fs)
                LOG_INFO("  [%s] %s/%s: %s",
                    f.severity == ValSeverity::Error ? "ERR" : "WARN",
                    f.module.c_str(), f.entry_id.c_str(), f.message.c_str());
        }
    }
    g_meta.load_from_defs();  // G3.1: MetaNode 需在 meta 模块加载后重建
    // G10.9-B1: 启动即加载 meta — 修复"继续游戏路径从不 load → mark_hint_shown
    // 用默认值整文件覆盖 meta_save.json (货币/runs 清零)" 的 Critical bug。
    // 此后任意路径 (new_game/continue/选关) 的 g_meta.save 都基于已加载的真实数据。
    g_meta.load();
    load_vfx_recipes("resources/vfx_recipes.json");  // G5.8.5: VFX recipe registry

    // G6.1-G6.7: World layer data loading
    load_biome_defs("resources/biomes.json");
    load_landmark_defs("resources/landmarks.json");
    load_encounter_defs("resources/encounters.json");

    // A6-S2 批次9: 刷怪表数据化 — 加载失败必须响亮, 否则全图静默刷 default 怪
    if (!load_spawn_slots("resources/enemy_slots.json") ||
        !load_challenge_pools("resources/challenge_pools.json")) {
        LOG_ERROR("Spawn tables 加载失败: 楼层刷怪将全部回退 default, 请检查 resources/enemy_slots.json 与 challenge_pools.json");
    }

    // Font 通过 ResourceManager 加载
    load_fonts();

    // D7 Step6: 注册全局服务
    ServiceLocator::provide(&ResourceManager::inst());
    ServiceLocator::provide(&EventBus::inst());
    LOG_INFO("ServiceLocator: 全局服务已注册");

    // G8.4/F15.4: RL train/test + mirror (standalone, before engine)
    if (GameScene::g_rl_test_episodes > 0 || GameScene::g_rl_train_episodes > 0) {
        run_rl_mode(GameScene::g_rl_test_episodes, GameScene::g_rl_train_episodes);
    }
    if (GameScene::g_rl_mirror_episodes > 0) {
        run_rl_mirror_mode(GameScene::g_rl_mirror_episodes);
    }
    if (GameScene::g_rl_test_episodes > 0 || GameScene::g_rl_train_episodes > 0 ||
        GameScene::g_rl_mirror_episodes > 0) {
        printf("[RL] Done. Exiting.\n");
        return 0;
    }

    // G10.9-B3: 旧档安全迁移 — save.json → slot_1.json (验证+备份, 不删源)
    SaveManager::migrate_legacy_save();
    // G10.9-B2: has_save 语义 = 任一槽位有档 (continue 入口; 细粒度选择 G10.9-C 做)
    bool has_save = false;
    for (auto& s : SaveManager::get_all_slots()) if (s.exists) { has_save = true; break; }
    LOG_INFO(has_save ? "存档存在" : "暂无存档");

    // G5.6: sim 模式直接进 GameScene, 跳过标题画面
    if (GameScene::g_sim_mode) {
        auto gs = std::make_shared<GameScene>();
        gs->name = "GameScene";
        gs->new_game();
        tree.change_scene(gs);
        // Q3.1: headless 加速 — 隐藏窗口 + 定步长驱动, 无渲染/输入开销
        SetWindowState(FLAG_WINDOW_HIDDEN);
        double dt = 1.0 / 60.0;
        while (tree.is_running() && !WindowShouldClose()) {
            tree.process_input();
            tree.process_frame(dt);
        }
        ServiceLocator::remove_all();
        ResourceManager::inst().unload_all();
        CloseAudioDevice();
        Logger::inst().close();
        return 0;
    }

    // M6-i.1: --autocontinue 直接读档进游戏 (跳过 Title/SlotSelect; 取证链路免键盘)
    if (g_autocontinue && has_save) {
        int slot_id = 1;
        for (int s = 1; s <= SAVE_SLOT_COUNT; ++s)
            if (SaveManager::get_slot_summary(s).exists) { slot_id = s; break; }
        auto gs = std::make_shared<GameScene>();
        gs->name = "GameScene";
        auto* data = SaveManager::load_game(slot_id);
        if (data) {
            int floor = data->current_floor, maxf = data->max_unlocked_floor;
            if (g_goto_floor >= 1) floor = (std::min)(g_goto_floor, maxf);
            if (!data->player) {
                auto p = std::make_unique<Player>(TILE_SIZE * 2, TILE_SIZE * 2,
                    PLAYER_SPEED, PLAYER_MAX_HP, PLAYER_ATTACK, PLAYER_PDEF, PLAYER_MDEF);
                data->player = std::move(p);
            }
            gs->load_saved_game(floor, maxf, std::move(data->player),
                                data->dungeon_seed, data->special_triggered,
                                data->special_discovered, data->rule_counters,
                                data->quest_states, data->play_time);
            gs->set_mirror_memory(data->mirror_prior_alpha, data->mirror_prior_beta);
            delete data;
            tree.change_scene(gs);
            LOG_INFO("autocontinue → slot %d 第%d层", slot_id, floor);
            tree.run();
            ServiceLocator::remove_all();
            ResourceManager::inst().unload_all();
            CloseAudioDevice();
            Logger::inst().close();
            return 0;
        }
        LOG_WARN("autocontinue: 读档失败 slot %d, 回退标题", slot_id);
    }

    auto title = std::make_shared<TitleScene>();
    title->name = "TitleScene";
    title->has_save = has_save;
    tree.change_scene(title);
    tree.run();

    ServiceLocator::remove_all();
    ResourceManager::inst().unload_all();
    CloseAudioDevice();
    Logger::inst().close();
    return 0;
}
