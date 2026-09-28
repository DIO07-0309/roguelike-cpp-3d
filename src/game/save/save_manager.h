#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>

class Player;

// G10.9-B2: 槽位摘要 — 菜单只读这个, 不解析存档格式
struct SlotSummary {
    bool exists = false;
    int slot_id = 0;
    int floor = 1;              // 当前层
    int max_floor = 1;          // 本档解锁层
    int level = 1;              // 玩家等级
    int element_type = 0;       // 0=NONE 1=FIRE 2=ICE 3=POISON (展示图标用)
    float play_time = 0.0f;     // 本档累计时长 (v:5+)
};

struct SaveData {
    std::unique_ptr<Player> player;
    int current_floor = 1;
    int max_unlocked_floor = 1;
    uint32_t dungeon_seed = 0;                  // B8: 当前楼层地牢种子
    std::vector<bool> special_triggered;         // B8: 特殊房间触发状态
    std::vector<bool> special_discovered;        // B10: 特殊房间发现状态
    // Batch 3A: Relic save/load via PersistenceScope (v4+)
    // ── G1 Step7: Save v2 新增 ──
    int attack_evo_level = 1;                    // 普攻进化等级
    std::unordered_map<std::string, int> rule_counters; // rule_* counter
    // ── G2.4: Save v3 新增 ──
    std::unordered_map<int, int> quest_states;   // quest_id → QuestState
    // ── G2.5: Save v3 新增 ──
    // G10.9-B2: unlocked_endings 迁移 Meta (账号级收集), 不再存档内往返
    // ── G10.1: Element Core ──
    int element_level = 1;
    int element_exp = 0;
    int element_type = 0;  // 0=NONE, 1=FIRE, 2=ICE, 3=POISON
    bool element_initialized = false;
    // ── M4e: 跨对局镜像 AI 记忆 (72 alpha + 72 beta, 桶-major) ──
    std::vector<float> mirror_prior_alpha;
    std::vector<float> mirror_prior_beta;
    // G10.9-B2: 槽位元数据
    float play_time = 0.0f;
};

// G10.9-B2: 槽位常量
constexpr int SAVE_SLOT_COUNT = 3;

class SaveManager {
public:
    // ═ G10.9-B2: Slot API — UI 只允许用这层, 不碰存档格式 ═
    static bool save_game(int slot_id, Player* player, int floor, int max_floor,
                          uint32_t dungeon_seed = 0,
                          const std::vector<bool>& special_triggered = {},
                          const std::vector<bool>& special_discovered = {},
                          const std::unordered_map<std::string, int>& rule_counters = {},
                          const std::unordered_map<int, int>& quest_states = {},
                          const std::vector<float>& mirror_prior_alpha = {},
                          const std::vector<float>& mirror_prior_beta = {},
                          float play_time = 0.0f);
    static SaveData* load_game(int slot_id);        // 堆分配, 调用方 delete
    static void delete_save(int slot_id = 1);
    static bool slot_exists(int slot_id);
    static SlotSummary get_slot_summary(int slot_id);          // 轻量只读 (不建 Player)
    static std::vector<SlotSummary> get_all_slots();
    static int  active_slot();                       // 当前游戏会话绑定的档
    static void set_active_slot(int slot_id);         // 进游戏时由菜单设置
    static bool migrate_legacy_save();               // B3: save.json → slot_1.json

    // 兼容旧接口 (内部转发到 active slot; 逐步淘汰)
    static bool save_exists();
    static SaveData* load_save();

    // Q3.1: --sim 模式禁止覆盖玩家存档
    static bool g_sim_readonly;

private:
    static std::string _slot_path(int slot_id);
    static std::string _save_dir();
    // 旧单槽路径 (仅迁移用)
    static std::string _legacy_save_path();
    // B8: spr 序列化辅助 (文件内 static 函数, 不占类接口)
};
