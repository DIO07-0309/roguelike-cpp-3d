#include "sim_outcome.h"
#include "game/config.h"  // MAX_FLOORS

bool is_sim_victory(int floor, bool alive,
                    bool game_timeout, bool wall_timeout, int stuck_watchdog) {
    if (!alive || floor < MAX_FLOORS) return false;
    // 排除三类强制结算: 900s 游戏超时 / 帧预算超时 / 卡死看门狗.
    if (game_timeout || wall_timeout || stuck_watchdog > 0) return false;
    return true;
}
