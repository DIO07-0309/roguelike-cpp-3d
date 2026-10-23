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
#include "game/rendering3d/part_voxelizer.h" // A13: --voxel-avatar
#include "core/logger.h"
#include "scenes/game_scene.h"
#include "scenes/title_scene.h"
#include "save/save_manager.h"
#include "meta_progression.h"   // Q3.1: --sim 只读保护
#include "ai/mirror/tactical_policy.h"   // v0.9.36 G32-E: --tactical-mirror
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

// G41: 流派主题预览开关 (--theme-preview) — 定义在 game/director/theme_preview.cpp
// (不放 main.cpp: 测试目标 roguelike_lib 排除 main.cpp, 否则测试链接 undefined)
extern bool g_theme_preview;
extern int  g_hd2d_autoshot;          // M6-i.1: hd2d_renderer.cpp 定义
extern float g_anim_time_override;   // G43: 定义在 theme_preview.cpp (同 G41 理由)

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

// ═══════════════════════════════════════════════════════════════
// main 启动编排 (G44: 原 302 行拆为 4 个阶段函数 + 40 行调度骨架)
// 各阶段互相独立, main 只负责按顺序调用
// ═══════════════════════════════════════════════════════════════

// ── G44 阶段 1: 平台初始化 (工作目录/SEH/stdio/terminate) ──
static void _init_platform(int argc, char** argv) {
    Logger::inst().init();
#ifdef _WIN32
    _fix_working_dir(argc > 0 ? argv[0] : "");
    install_seh_handler();
    gui_sanitize_stdio();   // G10.7-B1: WIN32 GUI 子系统下 stdout/stderr 保活
#endif
    std::set_terminate(_terminate_handler);
    LOG_INFO("Roguelike C++ Edition 启动 %s %s", __DATE__, __TIME__);
    SetTraceLogLevel(LOG_WARNING);
}

// ── G44 阶段 2: CLI 解析 (仅 Win32 — 依赖 __argc/__argv) ──
// 把所有 flag 收进一个结构体返回, 避免 main 里散落十几个局部变量
struct CliFlags {
    bool replay_mode = false, record_mode = false, sim_mode = false;
    bool input_diag_requested = false;
    std::string replay_path, record_path;
    int sim_runs = 100;
    uint32_t sim_seed_start = 0;
    std::string sim_build;
};

// ── G44: CLI 解析按功能分三组 (原 65 行单函数) ──
// 每组只改自己关心的字段, 未识别的参数留给下一组

// 回放/录制/sim 相关
static bool _parse_sim_flags(const std::string& arg, int& i, CliFlags& f) {
    if (arg == "--replay" && i + 1 < __argc) {
        f.replay_mode = true;
        f.replay_path = __argv[++i];
    } else if (arg == "--record" && i + 1 < __argc) {
        f.record_mode = true;
        f.record_path = __argv[++i];
    } else if (arg == "--sim" && i + 1 < __argc) {
        f.sim_mode = true;
        f.sim_runs = atoi(__argv[++i]);
    } else if (arg == "--sim-seed" && i + 1 < __argc) {
        f.sim_seed_start = (uint32_t)atoi(__argv[++i]);
    } else if (arg == "--sim-build" && i + 1 < __argc) {
        f.sim_build = __argv[++i];
    } else if (arg == "--sim-all-builds") {
        f.sim_mode = true;
        GameScene::g_sim_all_builds = true;
    } else if (arg == "--sim-ai" && i + 1 < __argc) {
        GameScene::g_sim_ai_type = __argv[++i];
    } else if (arg == "--rl-test" && i + 1 < __argc) {
        GameScene::g_rl_test_episodes = atoi(__argv[++i]);
    } else if (arg == "--rl-train" && i + 1 < __argc) {
        GameScene::g_rl_train_episodes = atoi(__argv[++i]);
    } else if (arg == "--rl-mirror" && i + 1 < __argc) {
        GameScene::g_rl_mirror_episodes = atoi(__argv[++i]);
    } else if (arg == "--tactical-mirror") {
        mirror::tactical_mirror_enabled() = true;
    } else {
        return false;
    }
    return true;
}

// 表现层开关 (hd2d / anim-time / voxel-avatar)
static bool _parse_render_flags(const std::string& arg, int& i) {
    if (arg == "--hd2d") {
        g_hd2d_mode = true;
    } else if (arg == "--voxel-avatar") {          // A13: 玩家部件走体素 3D 件 (默认已开, 兼容保留)
        voxel::g_voxel_avatar = true;
    } else if (arg == "--no-voxel-avatar") {       // A13: 关闭体素化身, 回退纸偶
        voxel::g_voxel_avatar = false;
    } else if (arg == "--anim-time" && i + 1 < __argc) {
        g_anim_time_override = (float)atof(__argv[++i]);
    } else {
        return false;
    }
    return true;
}

// 取证链路 (autoshot / hidwin / autocontinue / goto-floor / theme-preview)
static bool _parse_evidence_flags(const std::string& arg, int& i) {
    if (arg == "--autoshot" && i + 1 < __argc) {
        g_hd2d_autoshot = atoi(__argv[++i]);
    } else if (arg == "--hidwin") {
        g_window_hidden = true;
    } else if (arg == "--autocontinue") {
        g_autocontinue = true;
    } else if (arg == "--goto-floor" && i + 1 < __argc) {
        g_autocontinue = true;
        g_goto_floor = atoi(__argv[++i]);
    } else if (arg == "--theme-preview") {
        g_theme_preview = true;
        g_autocontinue = true;
    } else {
        return false;
    }
    return true;
}

static CliFlags _parse_cli() {
    CliFlags f;
#ifdef _WIN32
    for (int i = 1; i < __argc; i++) {
        const std::string arg = __argv[i];
        if (arg == "--input-diag") {
            f.input_diag_requested = true;   // P1-C9
            continue;
        }
        if (_parse_sim_flags(arg, i, f)) continue;
        if (_parse_render_flags(arg, i)) continue;
        if (_parse_evidence_flags(arg, i)) continue;
    }
#endif
    return f;
}

// ── G44: CLI 结果回填到 GameScene 静态配置 ──
static void _apply_cli_to_game(const CliFlags& flags) {
    GameScene::g_record_mode = flags.record_mode;
    GameScene::g_replay_mode = flags.replay_mode;
    GameScene::g_record_path = flags.record_path;
    GameScene::g_replay_path = flags.replay_path;
    GameScene::g_sim_mode  = flags.sim_mode;
    GameScene::g_sim_runs  = flags.sim_runs;
    if (flags.record_mode) LOG_INFO("Replay: recording to %s", flags.record_path.c_str());
    if (flags.replay_mode) LOG_INFO("Replay: playing from %s", flags.replay_path.c_str());
}

// ── G44 阶段 3: 配置回填 + sim headless 特判 ──
// 返回 true = 已进入 sim headless 路径 (无窗口, 调用方应走定步长驱动)
static bool _init_sim_headless(const CliFlags& f) {
    if (!f.sim_mode) return false;
    SimulationConfig sim_cfg;
    sim_cfg.runs = f.sim_runs;
    sim_cfg.seed_start = f.sim_seed_start;
    if (!f.sim_build.empty()) {
        sim_cfg.random_build = false;
        sim_cfg.fixed_build = true;
        sim_cfg.fixed_build_id = f.sim_build;
    }
    AudioServer::g_muted = true;            // Q3.1: headless 静音
    SaveManager::g_sim_readonly = true;     // Q3.1: sim 不覆盖玩家存档
    g_meta.g_readonly = true;               // Q3.1: sim 不写 meta 存档
    auto& sr = SimRunner::inst();
    if (GameScene::g_sim_all_builds) sr.set_all_builds(true);
    sr.begin(sim_cfg);
    LOG_INFO("Sim: %d runs", f.sim_runs);
    return true;
}

// ── G44 阶段 4: Registry + ModManager 构建 ──
// ServiceLocator::provide 也在这里 — sim headless 同样需要 (事件回调)
// ── G44: 收集并解析 mod 依赖, 按拓扑序加入 builder ──
static void _register_mods(RegistryBuilder& builder) {
    ModManager mod_mgr;
    mod_mgr.scan("mods");
    std::vector<std::unique_ptr<ModProvider>> mods;
    std::vector<ModDepInfo> deps;
    for (auto& mi : mod_mgr.all()) {
        if (!mi.enabled || !mi.valid) continue;
        auto mp = ModProvider::create(mi.dir_path);
        if (!mp) continue;
        ModDepInfo di;
        di.id = mp->manifest().id;
        di.requires_ids = mp->manifest().requires_ids;
        di.load_after = mp->manifest().load_after;
        deps.push_back(di);
        mods.push_back(std::move(mp));
    }
    auto resolved = DependencyResolver::resolve(deps);
    for (const auto& id : resolved.ordered_ids) {
        for (auto& mp : mods) {
            if (mp && mp->manifest().id == id) {
                builder.add_provider(std::move(mp));
                break;
            }
        }
    }
    for (const auto& skipped : resolved.skipped)
        LOG_INFO("  [SKIP] Mod '%s': dependency missing", skipped.c_str());
    for (const auto& cyclic : resolved.cycle_info)
        LOG_INFO("  [SKIP] Mod '%s': cyclic dependency", cyclic.c_str());
    LOG_INFO("── Mods: %d/%d enabled ──",
             mod_mgr.enabled_count(), mod_mgr.total_count());
}

// ── G44: 注册内置 12 个 Def 模块加载器 ──
static void _register_builtin_modules(RegistryBuilder& builder) {
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
}

// ── G44 阶段 5: Registry 真正构建 + 全局资源/字体/服务注册 ──
static void _init_resources(RegistryBuilder& builder) {
    if (!builder.build_all())
        LOG_ERROR("  [FAIL] Registry build");
    for (auto& rec : builder.log())
        LOG_INFO("  [%s] %s <- %s%s",
                 rec.module.c_str(), rec.provider.c_str(), rec.source.c_str(),
                 rec.is_merge ? " (merge)" : "");
    auto& findings = builder.findings();
    if (!findings.empty()) {
        LOG_INFO("── Validator: %zu issues ──", findings.size());
        for (auto& f : findings)
            LOG_INFO("  [%s] %s/%s: %s",
                     f.severity == ValSeverity::Error ? "ERR" : "WARN",
                     f.module.c_str(), f.entry_id.c_str(), f.message.c_str());
    }
    g_meta.load_from_defs();  // G3.1: MetaNode 需在 meta 模块加载后重建
    g_meta.load();
    load_vfx_recipes("resources/vfx_recipes.json");          // G5.8.5
    load_biome_defs("resources/biomes.json");
    load_landmark_defs("resources/landmarks.json");
    load_encounter_defs("resources/encounters.json");
    if (!load_spawn_slots("resources/enemy_slots.json") ||
        !load_challenge_pools("resources/challenge_pools.json")) {
        LOG_ERROR("Spawn tables 加载失败: 楼层刷怪将全部回退 default");
    }
    load_fonts();
    ServiceLocator::provide(&ResourceManager::inst());
    ServiceLocator::provide(&EventBus::inst());
    LOG_INFO("ServiceLocator: 全局服务已注册");
}

// ── G44 阶段 6: RL 独立模式 (跑完即退) ──
static bool _try_run_rl_headless() {
    if (GameScene::g_rl_test_episodes <= 0 && GameScene::g_rl_train_episodes <= 0
        && GameScene::g_rl_mirror_episodes <= 0)
        return false;
    if (GameScene::g_rl_test_episodes > 0 || GameScene::g_rl_train_episodes > 0)
        run_rl_mode(GameScene::g_rl_test_episodes, GameScene::g_rl_train_episodes);
    if (GameScene::g_rl_mirror_episodes > 0)
        run_rl_mirror_mode(GameScene::g_rl_mirror_episodes);
    printf("[RL] Done. Exiting.\n");
    return true;
}

// ── G44: Registry + ModManager 构建 (返回 builder 供阶段 5 消费) ──
static RegistryBuilder _build_registry(SceneTree& tree) {
    ServiceLocator::provide(&tree);  // Q4.4: 事件回调访问音频
    LOG_INFO("── Registry Build ──");
    RegistryBuilder builder;
    builder.add_provider(std::make_unique<BuiltinProvider>());
    _register_builtin_modules(builder);
    _register_mods(builder);
    return builder;
}

// ── G44: 统一收尾 (原 4 处逐字重复的 5 行) ──
static void _shutdown_and_exit() {
    ServiceLocator::remove_all();
    ResourceManager::inst().unload_all();
    CloseAudioDevice();
    Logger::inst().close();
}

// ── G44: sim headless 主循环 — 建 GameScene + 定步长驱动 ──
static void _run_sim_headless(SceneTree& tree) {
    SetWindowState(FLAG_WINDOW_HIDDEN);
    auto gs = std::make_shared<GameScene>();
    gs->name = "GameScene";
    gs->new_game();
    tree.change_scene(gs);
    const double dt = 1.0 / 60.0;
    while (tree.is_running() && !WindowShouldClose()) {
        tree.process_input();
        tree.process_frame(dt);
    }
}

// ── G44: 是否有任何存档槽 (决定标题是否显示 Continue) ──
static bool _probe_has_save() {
    for (auto& slot : SaveManager::get_all_slots())
        if (slot.exists) return true;
    return false;
}

// ── G44: --autocontinue 直接读档进游戏 (跳过 Title/SlotSelect) ──
// 返回 true = 已成功启动; false = 读档失败, 调用方回退标题
static bool _start_autocontinue(SceneTree& tree) {
    int slot_id = 1;
    for (int s = 1; s <= SAVE_SLOT_COUNT; ++s)
        if (SaveManager::get_slot_summary(s).exists) { slot_id = s; break; }
    auto gs = std::make_shared<GameScene>();
    gs->name = "GameScene";
    auto* data = SaveManager::load_game(slot_id);
    if (!data) {
        LOG_WARN("autocontinue: 读档失败 slot %d, 回退标题", slot_id);
        return false;
    }
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
    return true;
}

// ── G44: 正常启动 (标题界面) ──
static void _start_title(SceneTree& tree, bool has_save) {
    auto title = std::make_shared<TitleScene>();
    title->name = "TitleScene";
    title->has_save = has_save;
    tree.change_scene(title);
    tree.run();
}

int main(int argc, char** argv) {
    _init_platform(argc, argv);

    CliFlags flags = _parse_cli();
    _apply_cli_to_game(flags);
    // M6-i.1: 后台取证静音
    if (g_window_hidden) AudioServer::g_muted = true;

    SceneTree tree(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    if (flags.input_diag_requested) tree.set_input_diag(true);
    LOG_INFO("窗口创建");

    RegistryBuilder builder = _build_registry(tree);
    _init_resources(builder);

    if (_try_run_rl_headless()) { _shutdown_and_exit(); return 0; }

    // Q3.1: sim headless — 建场景 + 定步长驱动, 无窗口/输入/渲染开销
    if (_init_sim_headless(flags)) {
        _run_sim_headless(tree);
        _shutdown_and_exit();
        return 0;
    }

    SaveManager::migrate_legacy_save();
    // G10.9-B2: has_save 决定是否显示 Continue (第一位; 细分槽选择见 G10.9-C)
    bool has_save = _probe_has_save();
    LOG_INFO(has_save ? "存档存在" : "暂无存档");

    // M6-i.1: --autocontinue 读档直进 (失败则回退标题)
    if (g_autocontinue && has_save && _start_autocontinue(tree))
        return 0;

    _start_title(tree, has_save);
    _shutdown_and_exit();
    return 0;
}
