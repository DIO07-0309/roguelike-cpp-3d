#pragma once
// A6-S2 批次9: Spawn tables — 楼层槽位选怪表 + 挑战房刷怪池 (数据驱动)
// 取代 floor_manager.cpp 的 12 case switch 与 challenge_room.cpp 的 9 个硬编码池。
// 楼层级槽位权重仍在 FloorConfig::enemy_weights[12] (编译期表), 本层只管 槽位序号 -> 具体怪 id。
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct SpawnCandidate {
    std::string id;
    int weight = 1;
    int floor_start = 1;   // inclusive, 缺省覆盖全部楼层
    int floor_end = 15;    // inclusive
};

struct SpawnSlot {
    std::string archetype;
    std::vector<SpawnCandidate> candidates;
};

struct ChallengeBiomePool {
    int floor_start = 1;
    int floor_end = 15;
    std::vector<std::vector<std::string>> waves;
};

// Registry
extern std::vector<SpawnSlot> g_spawn_slots;
extern std::vector<std::pair<std::string, std::vector<std::string>>> g_spawn_aliases;
extern std::string g_spawn_default;
extern std::vector<ChallengeBiomePool> g_challenge_pools;

bool load_spawn_slots(const char* json_path = "resources/enemy_slots.json");
bool load_challenge_pools(const char* json_path = "resources/challenge_pools.json");

const SpawnSlot* get_spawn_slot(int slot_index);

// 槽位内加权抽签。候选 <=1 时绝不消耗全局 rng() — 这是与旧 switch 逐位等价的
// 关键: 旧代码对单候选槽位(含槽位6越界被过滤后)直接返回字面量, 不掷骰。
const std::string* pick_slot_monster(int slot_index, int floor);

// 挑战房池为均匀抽签; rng 由调用方以 wave_seed 派生后传入, 不触碰全局 rng()。
const std::string* pick_challenge_monster(int floor, int wave, uint32_t rng);
