#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

class Player;
class GameMap;
class Monster;
struct DroppedItem;

// ============================================================
// Batch 3F: Challenge Room — 波次战斗挑战
// ============================================================

enum class ChallengePhase : uint8_t {
    INACTIVE,       // 门 CLOSED, 未交互
    UNLOCKED,       // Key 消耗, 门 OPEN, 玩家可进入
    ARMED,          // 玩家进入房间, RoomManager 即将 LOCKED
    WAVE_SPAWNING,  // 当前波次正在生成
    COMBAT,         // 等待当前波次怪物全部死亡
    WAIT_NEXT_WAVE, // 波次间隔计时 (3s)
    REWARD,         // 发放奖励
    CLEARED,        // 挑战完成, 门 OPEN
    PORTAL_ACTIVE   // Batch 3I: 传送门可见, 等待玩家交互
};

// 波次全灭后的下一步 (纯函数返回值, 便于全真值表测试)
enum class WaveAdvance : uint8_t { WAIT, BOSS_WAIT, REWARD };

struct ChallengeModifier {
    float hp_multiplier = 1.5f;
    float attack_multiplier = 1.3f;
    int extra_buffs = 1;
};

struct ChallengeWave {
    int monster_count = 4;
    float spawn_delay = 3.0f;
};

// 奖励结算计划 (纯数据, 由 decide_reward_plan 纯函数产出, 便于全真值表测试)
// *_rarity_floor 存 Rarity 秩 (int), 避免本头文件包含 item.h (后者牵连 raylib.h)
struct RewardPlan {
    int base_item_count = 0;
    int bonus_item_count = 0;
    int base_retry_cap = 0;
    int bonus_retry_cap = 0;
    int base_rarity_floor = 0;
    int bonus_rarity_floor = 0;
    int gold = 0;
};

class ChallengeRoomController {
public:
    void reset();

    // 旧接口: 保留兼容
    bool try_activate(Player& player);

    // Batch 3I: Portal interaction (GameScene 消费 Key, Controller 只记录状态)
    void setup_portal(int tx, int ty);
    bool consume_key_for_challenge(Player& player);
    void set_room_rect(int rx, int ry, int rw, int rh);
    void set_return_portal(int tx, int ty);
    void mark_cleared();

    // 每帧 tick
    void tick(float dt, GameMap* map, Player* player,
              std::vector<std::unique_ptr<Monster>>& monsters,
              int floor, uint32_t dungeon_seed, int room_index,
              std::vector<DroppedItem>& ground_items);

    void on_player_entered();
    void on_doors_locked();

    void set_phase_for_test(ChallengePhase p) { _phase = p; }
    ChallengePhase phase() const { return _phase; }
    bool is_cleared() const { return _phase == ChallengePhase::CLEARED; }
    int current_wave() const { return _current_wave; }
    int total_waves() const { return _total_waves; }

    // Batch B4: 隐藏压轴波 —— 确定性 25% 判定, 不消耗全局 rng
    bool has_boss_wave(uint32_t dungeon_seed, int room_index) const;
    bool boss_wave_pending() const { return _boss_wave_pending; }
    bool boss_wave_decided() const { return _boss_wave_decided; }
    static WaveAdvance decide_advance(int wave_after_increment, int total_waves,
                                      bool boss_pending);

    // Batch B4: 奖励隔离 —— 结算参数纯函数; boss_wave_pending=false 必须逐项等于现状契约
    static RewardPlan decide_reward_plan(int floor, bool boss_wave_pending);
    void grant_rewards_for_test(Player& player, GameMap* map, int floor,
                                std::vector<DroppedItem>& ground_items,
                                bool boss_wave_pending);

    // Batch 3I: Portal/room getters
    int portal_tx() const { return _portal_tx; }
    int portal_ty() const { return _portal_ty; }
    int return_portal_tx() const { return _return_portal_tx; }
    int return_portal_ty() const { return _return_portal_ty; }
    int room_rx() const { return _room_rx; }
    int room_ry() const { return _room_ry; }
    int room_rw() const { return _room_rw; }
    int room_rh() const { return _room_rh; }

private:
    ChallengePhase _phase = ChallengePhase::INACTIVE;
    int _current_wave = 0;
    int _total_waves = 3;
    float _wave_timer = 0.0f;
    int _monsters_alive_this_wave = 0;
    int _room_rx = 0, _room_ry = 0, _room_rw = 0, _room_rh = 0;
    int _portal_tx = -1, _portal_ty = -1;
    int _return_portal_tx = -1, _return_portal_ty = -1;

    // Batch B4: 压轴判定结果暂存位. 唯一写入点是 COMBAT 全灭分支 (_current_wave 首次
    // 抵达 _total_waves 时判定一次); 按房间清位在 on_doors_locked(), 按层兜底在 reset().
    // _grant_rewards 读 pending 以决定是否叠加压轴奖励.
    bool _boss_wave_decided = false;
    bool _boss_wave_pending = false;

    void _spawn_wave(int wave_index, GameMap* map,
                     std::vector<std::unique_ptr<Monster>>& monsters,
                     int floor, uint32_t seed, int room_idx);
    void _spawn_boss_wave(GameMap* map,
                          std::vector<std::unique_ptr<Monster>>& monsters,
                          int floor);
    void _grant_rewards(Player& player, GameMap* map, int floor,
                        std::vector<DroppedItem>& ground_items, bool boss_cleared);
    // rarity_floor 以 int 传 Rarity 秩, 避免本头文件包含 item.h (后者牵连 raylib.h)
    int _grant_items(Player& player, std::vector<DroppedItem>& ground_items,
                     int count, int rarity_floor, int retry_cap);
    bool _room_contains(int tx, int ty) const;
    uint32_t _deterministic_seed(uint32_t dungeon_seed, int room_index, int wave_index) const;
    static const char* _pick_monster_type(int floor, int wave, uint32_t rng);
};
