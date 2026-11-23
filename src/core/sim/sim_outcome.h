#pragma once

// P3-redesign 切片0: sim 胜利判定的纯函数.
// victory 必须来自正常清完 F15, 不能靠超时/看门狗强制结算时"F15 存活"造假.
// 纯值输入, 无副作用, 便于契约测试.
bool is_sim_victory(int floor, bool alive,
                    bool game_timeout, bool wall_timeout, int stuck_watchdog);
