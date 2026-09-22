// G9.4: DoorState Truth Table — 审计 §9 语义表的回归锁定
// 事实源: game_map.h:30-35 注释 + game_map.cpp 实现 + PROJECT_TECHNICAL_AUDIT.md §9
//
// | State  | Walkable | BlocksSight | E 键开 | R1 接触开 |
// |--------|----------|-------------|--------|-----------|
// | OPEN   | ✓        | ✗           | —      | —(no-op)  |
// | CLOSED | ✗        | ✓           | ✓      | ✓         |
// | LOCKED | ✗        | ✓           | ✗      | ✗         |
// | SEALED | ✗        | ✓           | ✗      | ✗         |
//
// E 键路径 = player_controller.cpp:360-376 生产守卫原样复放
// R1 路径 = GameMap::try_open_door_toward (game_map.cpp:123-138)
// 说明: 特征化测试 — 锁定已审计(一致)的现有语义, 防止 DoorState 枚举/守卫漂移。
#include <gtest/gtest.h>

#include "game_map.h"
#include "dungeon_generator.h"
#include "config.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace {

// 6x3 全 FLOOR, (2,1) 为 DOOR
struct DoorFixture {
    GameMap m{6, 3, 32};
    explicit DoorFixture(DoorState st) {
        for (int y = 0; y < 3; y++)
            for (int x = 0; x < 6; x++)
                m.set_tile(x, y, TileType::FLOOR);
        m.set_tile(2, 1, TileType::DOOR);
        if (!m.set_door_state(2, 1, st)) {
            ADD_FAILURE() << "set_door_state failed";
        }
    }
};

constexpr int DX = 2, DY = 1;   // 门 tile
const char* name(DoorState s) {
    switch (s) {
        case DoorState::OPEN:   return "OPEN";
        case DoorState::CLOSED: return "CLOSED";
        case DoorState::LOCKED: return "LOCKED";
        case DoorState::SEALED: return "SEALED";
        default:                return "NONE";
    }
}

}  // namespace

// ── 行 1/2: Walkable + BlocksSight 矩阵 ──────────────────────
TEST(DoorTruthTable, WalkableMatrix) {
    const std::vector<std::pair<DoorState, bool>> table = {
        {DoorState::OPEN,   true },
        {DoorState::CLOSED, false},
        {DoorState::LOCKED, false},
        {DoorState::SEALED, false},
    };
    for (auto& [st, walkable] : table) {
        DoorFixture f(st);
        EXPECT_EQ(f.m.is_walkable(DX, DY), walkable) << "state=" << name(st);
    }
}

TEST(DoorTruthTable, BlocksSightMatrix) {
    const std::vector<std::pair<DoorState, bool>> table = {
        {DoorState::OPEN,   false},
        {DoorState::CLOSED, true },
        {DoorState::LOCKED, true },
        {DoorState::SEALED, true },
    };
    for (auto& [st, blocks] : table) {
        DoorFixture f(st);
        EXPECT_EQ(f.m.blocks_sight(DX, DY), blocks) << "state=" << name(st);
        // FOV 遮挡一致性: 从 (0,1) 看, 门后 (3,1) 仅在 OPEN 时可见
        f.m.update_fov(0, 1, 4);
        EXPECT_EQ(f.m.isVisible(3, 1), !blocks) << "state=" << name(st);
    }
}

// ── 行 4: R1 接触开门 — 仅 CLOSED 开门 ───────────────────────
TEST(DoorTruthTable, R1ContactOpen) {
    // 玩家 rect 位于 tile (1,1), 向右移动 → 前缘指向门 (2,1)
    Rectangle player = { 32.0f, 32.0f, 32.0f, 32.0f };
    const std::vector<std::pair<DoorState, bool>> table = {
        {DoorState::OPEN,   false},   // 已开, no-op
        {DoorState::CLOSED, true },   // R1 唯一开门态
        {DoorState::LOCKED, false},   // 封门不可接触开
        {DoorState::SEALED, false},   // 预留态不可接触开
    };
    for (auto& [st, should_open] : table) {
        DoorFixture f(st);
        bool opened = f.m.try_open_door_toward(player, 1.0f, 0.0f);
        EXPECT_EQ(opened, should_open) << "state=" << name(st);
        EXPECT_EQ(f.m.door_state_at(DX, DY),
                  should_open ? DoorState::OPEN : st)
            << "state=" << name(st);
    }
    // 静止不触发 (所有状态)
    for (DoorState st : {DoorState::OPEN, DoorState::CLOSED,
                         DoorState::LOCKED, DoorState::SEALED}) {
        DoorFixture f(st);
        EXPECT_FALSE(f.m.try_open_door_toward(player, 0.0f, 0.0f))
            << "state=" << name(st);
    }
}

// ── 行 3: E 键交互 — 生产守卫复放, 仅 CLOSED 开门 ─────────────
TEST(DoorTruthTable, EKeyOpen) {
    const std::vector<std::pair<DoorState, DoorState>> table = {
        {DoorState::OPEN,   DoorState::OPEN},    // no-op
        {DoorState::CLOSED, DoorState::OPEN},    // E 唯一开门态
        {DoorState::LOCKED, DoorState::LOCKED},  // E 不可开
        {DoorState::SEALED, DoorState::SEALED},  // E 不可开
    };
    for (auto& [st, expected] : table) {
        DoorFixture f(st);
        // player_controller.cpp:362-375 原样: 以"玩家所在格"为中心扫描 4 邻格,
        // 命中 CLOSED 才 set OPEN。玩家站在门旁 tile (1,1), 门在 (2,1)。
        constexpr int PX = 1, PY = 1;
        constexpr int dx4[] = {0, 0, -1, 1};
        constexpr int dy4[] = {-1, 1, 0, 0};
        for (int i = 0; i < 4; i++) {
            int nx = PX + dx4[i], ny = PY + dy4[i];
            if (f.m.door_state_at(nx, ny) == DoorState::CLOSED) {
                ASSERT_TRUE(f.m.set_door_state(nx, ny, DoorState::OPEN));
                break;
            }
        }
        EXPECT_EQ(f.m.door_state_at(DX, DY), expected) << "state=" << name(st);
    }
}

// ── RoomManager 门组 API 语义 (E3) ───────────────────────────
TEST(DoorTruthTable, DoorGroupOps) {
    // 房间门组: 上边 3 扇 + 右边 1 扇 (模拟 RoomManager build 采集)
    GameMap m{7, 5, 32};
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 7; x++)
            m.set_tile(x, y, TileType::FLOOR);
    const std::vector<std::pair<int,int>> doors = {
        {1, 0}, {3, 0}, {5, 0}, {6, 2},
    };
    for (auto& [tx, ty] : doors) m.set_tile(tx, ty, TileType::DOOR);

    EXPECT_TRUE(m.lock_room_doors(doors));
    for (auto& [tx, ty] : doors) {
        EXPECT_EQ(m.door_state_at(tx, ty), DoorState::LOCKED);
        EXPECT_FALSE(m.is_walkable(tx, ty));
        EXPECT_TRUE(m.blocks_sight(tx, ty));
    }
    // LOCKED 组内 E/接触均不开 (矩阵保证) → 解封必须走组 API
    EXPECT_TRUE(m.open_room_doors(doors));
    for (auto& [tx, ty] : doors) {
        EXPECT_EQ(m.door_state_at(tx, ty), DoorState::OPEN);
        EXPECT_TRUE(m.is_walkable(tx, ty));
    }
    EXPECT_TRUE(m.close_room_doors(doors));
    for (auto& [tx, ty] : doors) {
        EXPECT_EQ(m.door_state_at(tx, ty), DoorState::CLOSED);
        EXPECT_FALSE(m.is_walkable(tx, ty));
    }

    // 空组 no-op 返回 true (audit STATE-002: challenge ARMED 传空向量行为契约)
    EXPECT_TRUE(m.lock_room_doors({}));
    EXPECT_TRUE(m.open_room_doors({}));
    EXPECT_TRUE(m.close_room_doors({}));

    // 混入非 DOOR tile → false (调用方校验信号)
    std::vector<std::pair<int,int>> bad = doors;
    bad.push_back({0, 4});   // FLOOR tile
    EXPECT_FALSE(m.lock_room_doors(bad));
    EXPECT_FALSE(m.open_room_doors(bad));
}

// ── P1-C7: is_passable_sim 统一判定矩阵 ────────────────────────
// 语义: WALL/LOCKED/SEALED 不可走; FLOOR/STAIRS/LAVA/OPEN/CLOSED 门可走
// (CLOSED 视为可走 — Sim 自动开门, 执行层移动前 try_open_door_toward)
TEST(DoorTruthTable, PassableSimMatrix) {
    GameMap m{8, 3, 32};
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 8; x++)
            m.set_tile(x, y, TileType::FLOOR);
    m.set_tile(0, 1, TileType::WALL);
    m.set_tile(1, 1, TileType::STAIRS_DOWN);
    m.set_tile(2, 1, TileType::LAVA);
    m.set_tile(3, 1, TileType::DOOR);   // OPEN (set_tile 默认 OPEN)
    EXPECT_TRUE(m.is_passable_sim(0, 0));   // FLOOR
    EXPECT_FALSE(m.is_passable_sim(0, 1));  // WALL
    EXPECT_TRUE(m.is_passable_sim(1, 1));   // STAIRS
    EXPECT_TRUE(m.is_passable_sim(2, 1));   // LAVA
    EXPECT_TRUE(m.is_passable_sim(3, 1));   // DOOR OPEN
    EXPECT_FALSE(m.is_passable_sim(-1, 0)); // 越界
    EXPECT_FALSE(m.is_passable_sim(0, 99)); // 越界

    // 门的四态
    const std::vector<std::pair<DoorState, bool>> door_table = {
        {DoorState::OPEN,   true },
        {DoorState::CLOSED, true },   // Sim 可走 (自动开门语义)
        {DoorState::LOCKED, false},
        {DoorState::SEALED, false},
    };
    for (auto& [st, passable] : door_table) {
        GameMap m2{4, 3, 32};
        for (int y = 0; y < 3; y++)
            for (int x = 0; x < 4; x++)
                m2.set_tile(x, y, TileType::FLOOR);
        m2.set_tile(1, 1, TileType::DOOR);
        ASSERT_TRUE(m2.set_door_state(1, 1, st));
        EXPECT_EQ(m2.is_passable_sim(1, 1), passable) << "state=" << name(st);
    }
}

// ── 枚举值域不变量: 非 DOOR 恒 NONE, DOOR 恒在四态之内 ────────
TEST(DoorTruthTable, EnumDomainInvariant_AllSeeds) {
    DungeonGenerator gen(MAP_WIDTH, MAP_HEIGHT, TILE_SIZE);
    for (uint32_t seed : {1u, 42u, 777u, 20240801u}) {
        auto map = gen.generate(seed);
        for (int y = 0; y < MAP_HEIGHT; y++)
            for (int x = 0; x < MAP_WIDTH; x++) {
                DoorState s = map->door_state_at(x, y);
                if (map->tile_at(x, y) == TileType::DOOR) {
                    EXPECT_TRUE(s == DoorState::OPEN || s == DoorState::CLOSED ||
                                s == DoorState::LOCKED || s == DoorState::SEALED)
                        << "seed=" << seed << " door (" << x << "," << y << ")";
                } else {
                    EXPECT_EQ(s, DoorState::NONE)
                        << "seed=" << seed << " non-door (" << x << "," << y << ")";
                }
            }
    }
}
