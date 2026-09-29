// G9.2: Floor transition lifecycle regression — audit LIFE-001/002/003
// 背景: PROJECT_TECHNICAL_AUDIT.md §L3 三项 P1 (enter_floor 重置不完整)
//   LIFE-001 Challenge 相位跨层残留 → 免钥匙挑战/幽灵 COMBAT
//   LIFE-002 上一层 room rect/door group 在 Boss 层继续 tick → 卡 ARMED/错误封门
//   LIFE-003 exit_challenge_arena 调 reset_visibility → 吞掉本层探索进度
// 驱动真实 GameScene::enter_floor / enter|exit_challenge_arena 调用侧路径
// (headless 安全: DoorRenderer::init 已有 IsWindowReady 守卫)
#include <gtest/gtest.h>

#include "scenes/game_scene.h"
#include "meta_progression.h"
#include "world/challenge_room.h"
#include "world/room_manager.h"
#include "world/game_map.h"
#include "player.h"
#include "data/boss_defs.h"
#include "data/enemy_defs.h"

#include <memory>

// 与 inventory_sell_ui_test.cpp 一致: main.cpp 中的字体全局在测试中桩化
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

static void load_registry_defs() {
    load_boss_defs("resources/bosses.json");
    load_enemy_defs("resources/enemies.json");
}

static std::unique_ptr<Player> make_unique_player() {
    return std::make_unique<Player>(0, 0, 200, 200, 10, 5, 3);
}

static std::unique_ptr<GameScene> make_scene() {
    load_registry_defs();
    auto s = std::make_unique<GameScene>();
    s->player = make_unique_player();
    return s;
}

// LIFE-007: 逐 tile 快照 —— 断言两次生成的地图是否同一张
static std::string map_layout(const std::shared_ptr<GameMap>& m) {
    std::string out;
    for (int y = 0; y < m->height; y++)
        for (int x = 0; x < m->width; x++)
            out.push_back(static_cast<char>(m->tile_at(x, y)));
    return out;
}

// ── LIFE-001: Challenge 相位禁止跨层残留 ─────────────────────
// 最危险残留: UNLOCKED (钥匙已消耗未进入) → 新层免钥匙挑战;
// COMBAT 残留 → is_save_blocked() 在新层幽灵拦截。
TEST(FloorLifecycle, ChallengePhaseDoesNotSurviveFloorTransition) {
    const ChallengePhase stale_phases[] = {
        ChallengePhase::UNLOCKED, ChallengePhase::CLEARED,
        ChallengePhase::COMBAT,   ChallengePhase::ARMED,
    };
    for (auto stale : stale_phases) {
        auto s = make_scene();
        s->enter_floor(1, 4242u);
        // 模拟玩家在挑战流程中途下楼 → 相位悬空跨层。
        // 用超出地图边界的哨兵值 — 无论新层是否恰好有挑战房, 残留均可暴露。
        s->challenge_ctrl().set_phase_for_test(stale);
        s->challenge_ctrl().set_room_rect(900, 900, 777, 666);
        s->challenge_ctrl().set_return_portal(900, 900);

        s->enter_floor(2, 4243u);

        const auto ph = s->challenge_ctrl().phase();
        EXPECT_TRUE(ph == ChallengePhase::INACTIVE ||
                    ph == ChallengePhase::PORTAL_ACTIVE)
            << "stale challenge phase survived floor transition";
        // 上一层房间矩形/返还传送门坐标在任何相位下都不得存活
        EXPECT_NE(s->challenge_ctrl().room_rw(), 777);
        EXPECT_NE(s->challenge_ctrl().room_rh(), 666);
        EXPECT_NE(s->challenge_ctrl().room_rx(), 900);
        EXPECT_NE(s->challenge_ctrl().return_portal_tx(), 900);
    }
}

// ── LIFE-002: 上一层 RoomManager 数据禁止带入 Boss 层 ────────
TEST(FloorLifecycle, RoomManagerNotReusedOnBossFloor) {
    auto s = make_scene();
    s->enter_floor(1, 99u);
    ASSERT_GT(s->room_mgr().room_count(), 0) << "normal floor must build rooms";

    s->enter_floor(5, 99u);   // Boss 层: 不走 build 分支
    EXPECT_EQ(s->state, GameState::BOSS_INTRO);
    ASSERT_EQ(s->room_mgr().room_count(), 0)
        << "previous floor room rects must not survive into boss floor";

    // Boss 层 tick 不得 ARM/封门 (模拟 intro 结束后的正常更新)
    s->state = GameState::PLAYING;
    s->room_mgr().tick(s->game_map.get(), s->player.get(), s->monsters);
    ASSERT_EQ(s->room_mgr().room_count(), 0);

    int locked_doors = 0;
    for (int y = 0; y < s->game_map->height; y++)
        for (int x = 0; x < s->game_map->width; x++)
            if (s->game_map->door_state_at(x, y) == DoorState::LOCKED)
                locked_doors++;
    EXPECT_EQ(locked_doors, 0)
        << "stale door-group logic must not lock doors on boss floor";
}

// ── LIFE-003: 挑战竞技场往返不得清空本层探索进度 ──────────────
// 旧缺陷: exit_challenge_arena 调 reset_visibility 清空 is_explored,
// 返还后仅玩家周围一圈被 update_fov 重建 — 远处已探索区被吞。
TEST(FloorLifecycle, ArenaRoundTripPreservesExploration) {
    auto s = make_scene();
    s->enter_floor(1, 777u);

    // 在玩家出生点做一次 FOV → 区域 A
    auto [px, py] = s->game_map->pixel_to_tile(
        s->player->entity.position.x, s->player->entity.position.y);
    s->game_map->update_fov(px, py, 8);
    ASSERT_TRUE(s->game_map->isExplored(px, py));

    // 找一个远离出生点的可通行格, 传送过去再做 FOV → 区域 B
    int qx = -1, qy = -1;
    for (int y = 0; y < s->game_map->height && qx < 0; y++)
        for (int x = 0; x < s->game_map->width; x++) {
            if (s->game_map->is_walkable(x, y) &&
                std::abs(x - px) + std::abs(y - py) > 25) {
                qx = x; qy = y; break;
            }
        }
    ASSERT_GE(qx, 0) << "dungeon map too small for far-tile probe";
    auto [qxp, qyp] = s->game_map->tile_to_pixel(qx, qy);
    s->player->entity.position = {(float)qxp, (float)qyp};
    s->player->entity.sync_rect();
    s->game_map->update_fov(qx, qy, 8);
    // 区域 A 不在区域 B 的 FOV 内, 但仍应处于已探索状态
    ASSERT_TRUE(s->game_map->isExplored(px, py));

    // 真实路径: 进出挑战竞技场
    s->enter_challenge_arena();
    ASSERT_EQ(s->world_mode(), WorldMode::CHALLENGE_ARENA);
    s->exit_challenge_arena();
    ASSERT_EQ(s->world_mode(), WorldMode::DUNGEON);

    // 回归点: 返还后区域 A 不得被吞 (旧代码此处 is_explored == false)
    EXPECT_TRUE(s->game_map->isExplored(px, py))
        << "arena round trip must not wipe this floor's exploration";
    EXPECT_TRUE(s->game_map->isExplored(qx, qy));
}

// ── LIFE-004: 挑战压轴保底计数禁止随 GameScene 重建清零 ──────────
// 实机缺陷 (09-26): 4 次挑战房日志全部 "Boss wave roll: miss (pity 1/3)",
// 计数从未累积。根因: 计数原存于 ChallengeRoomController::_pity_miss_streak,
// 而该控制器是 GameScene 成员 — 每次选层新建 GameScene 即随实例销毁清零;
// 地牢每层仅 1 间挑战房, 单实例内本就攒不到上限。
// 修复: 计数改为账号级持久化 (saves/meta_save.json "pity"), enter_floor 载入。
// 本用例驱动 5 次真实「选层 → 进层 → 离场析构」重建, 断言计数跨实例存活。
// 注: 落盘往返本身由 save_test.ChallengePityStreakPersistsAcrossReload 覆盖,
//     此处只验 GameScene 生命周期这一环 (旧盲区, 单测原先结构上看不见)。
TEST(FloorLifecycle, ChallengePitySurvivesGameSceneRebuild) {
    const int floor = 12;  // 非 boss 层 (5/10/15 为 boss), 保证走正常建图分支

    // 先 load() 让内存与磁盘一致 — 否则 g_meta 内存态是默认值,
    // 结束时写盘会把仓库真实 meta 冲掉。测试跑完还原回原值。
    g_meta.load();
    const int restored_pity = g_meta.challenge_pity_streak();
    struct RestorePity {
        int value;
        ~RestorePity() { g_meta.set_challenge_pity_streak(value); }
    } restore{restored_pity};

    // 前 3 次挑战房空手 — 每次重新选层都是全新 GameScene 实例
    for (int miss = 1; miss <= 3; ++miss) {
        g_meta.set_challenge_pity_streak(miss);
        auto s = make_scene();
        s->enter_floor(floor, 4200u + (uint32_t)miss);
        ASSERT_EQ(s->challenge_pity_streak(), miss)
            << "fresh GameScene must load persisted pity, not restart from 0";
    }

    // 第 4 次到达保底上限: 与 seed/房间无关, 必出压轴
    g_meta.set_challenge_pity_streak(3);
    {
        auto s = make_scene();
        s->enter_floor(floor, 4203u);
        ASSERT_EQ(s->challenge_pity_streak(), 3);
        EXPECT_TRUE(s->challenge_ctrl().has_boss_wave(4203u, 0,
                                                      s->challenge_pity_streak()))
            << "pity cap must guarantee the boss wave";
    }

    // 见过压轴后计数归零, 新一轮从头计
    g_meta.set_challenge_pity_streak(0);
    auto s_last = make_scene();
    s_last->enter_floor(floor, 4204u);
    EXPECT_EQ(s_last->challenge_pity_streak(), 0);
}

// ── LIFE-005: 挑战竞技场内禁止触发换层激活/自动存档 ──────────
// 实机缺陷 (09-29): enter_challenge_arena 里 monsters.clear(), 而
// FloorManager::is_floor_cleared(空列表) 返回 true —— for 循环一次都不执行
// 直接落空。竞技场内每次 cleanup_dead_monsters 都判成「本层已清空」, 从而在
// 15x15 竞技场里触发 _activate_stairs:
//   1) 往 stairs_pos (地牢坐标, 对竞技场越界) 写 STAIRS_DOWN
//   2) 置 stairs_active = true
//   3) 触发一次自动存档 (本层其实根本没清)
// 而 exit_challenge_arena 只恢复地图/怪物/掉落, 不恢复 stairs_active
// → 出了竞技场该标志一直是 true, 本层再也不会真正清空、再也不会自动存档。
// 表现: 玩家打到很多层, 存档却永远停在进竞技场之前的那一层。
TEST(FloorLifecycle, ActivateStairsBlockedInsideChallengeArena) {
    auto s = make_scene();
    s->enter_floor(3, 777u);
    ASSERT_FALSE(s->stairs_active);

    // 前置条件: 地牢模式下清完怪确实能激活楼梯
    s->monsters.clear();
    s->_activate_stairs();
    ASSERT_TRUE(s->stairs_active)
        << "precondition: a cleared dungeon floor must activate stairs";

    // 回滚标志再进竞技场 —— 复现「空怪列表被当成已清空」的陷阱
    s->stairs_active = false;
    s->enter_challenge_arena();
    ASSERT_EQ(s->world_mode(), WorldMode::CHALLENGE_ARENA);
    ASSERT_TRUE(s->monsters.empty())
        << "arena starts with no monsters — exactly what trips is_floor_cleared";

    s->_activate_stairs();

    EXPECT_FALSE(s->stairs_active)
        << "stairs must not activate from inside the challenge arena";

    // 也不得把 STAIRS_DOWN 写进 15x15 竞技场 (stairs_pos 是地牢坐标)
    int stairs_tiles = 0;
    for (int y = 0; y < s->game_map->height; y++)
        for (int x = 0; x < s->game_map->width; x++)
            if (s->game_map->tile_at(x, y) == TileType::STAIRS_DOWN)
                stairs_tiles++;
    EXPECT_EQ(stairs_tiles, 0)
        << "arena map must not receive a STAIRS_DOWN tile";
}

// ── LIFE-006: is_floor_cleared(空) 返回 true —— 记录该陷阱为何需要守卫 ──
// 不改 FloorManager 语义 (「没有怪 = 已清空」本身合理, 且被既有用例依赖),
// 而是让 _activate_stairs 按世界模式过滤。此用例把隐式假设写成显式断言,
// 防止有人日后删掉 _activate_stairs 的模式守卫。
TEST(FloorLifecycle, EmptyMonsterListCountsAsCleared) {
    std::vector<std::unique_ptr<Monster>> none;
    EXPECT_TRUE(FloorManager::is_floor_cleared(none));
}

// ── LIFE-007: 读档入口必须透传 dungeon_seed ──────────────────
// floor_select_scene 曾把 seed 写成默认值 0 → enter_floor 判定「seed=0 = 新楼层」
// 走 rng() 重滚地牢, 玩家进的是与存档不符的陌生地图; 随后任意一次自动存档又用
// 新 seed 覆盖旧档, 存档布局从此漂移 (与 G20 深档被覆盖是同一类静默损坏)。
// 这里锁住契约: 透传存档 seed 必复现同一张地图; 传 0 必得到不同地图。
TEST(FloorLifecycle, LoadSavedGameMustForwardDungeonSeed) {
    const int floor = 4;
    const uint32_t seed = 20260929u;

    auto a = make_scene();
    a->enter_floor(floor, seed);
    ASSERT_NE(a->game_map, nullptr);
    EXPECT_EQ(a->_dungeon_seed, seed);
    const std::string layout = map_layout(a->game_map);
    EXPECT_FALSE(layout.empty());

    // 正确路径: 透传存档 seed → 逐 tile 同一张地图
    auto b = make_scene();
    b->load_saved_game(floor, floor, make_unique_player(), seed, {}, {});
    ASSERT_NE(b->game_map, nullptr);
    EXPECT_EQ(b->_dungeon_seed, seed);
    EXPECT_EQ(map_layout(b->game_map), layout)
        << "透传存档 seed 必须复现存档当时的布局";

    // 旧写法 (传 0) → 布局漂移, 且新 seed 会在下次自动存档覆盖旧档
    auto c = make_scene();
    c->load_saved_game(floor, floor, make_unique_player(), 0, {}, {});
    ASSERT_NE(c->game_map, nullptr);
    EXPECT_NE(c->_dungeon_seed, seed);
    EXPECT_NE(map_layout(c->game_map), layout)
        << "seed=0 会重滚地牢 —— 读档入口不得漏传存档 seed";
}

