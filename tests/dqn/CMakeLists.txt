# v0.9.32 G32: DQN 手写推理验证目标
#
# 独立成文件的唯一原因: tests/CMakeLists.txt 在并行开发中被反复还原
# (实测 4 次), 改这里会被冲掉。放在独立文件由根 CMakeLists.txt add_subdirectory
# 引入, 完全避开冲突面。
#
# 全部为独立可执行文件, 只依赖 RL 三源文件; 不进主游戏目标
# (根 CMakeLists.txt 的 GLOB 只扫 src/, 抓不到 tools/), 与游戏逻辑零耦合。

add_executable(dqn_probe ${CMAKE_SOURCE_DIR}/tools/rl/dqn_probe.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/dqn_net.cpp)
target_include_directories(dqn_probe PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(dqn_policy_probe ${CMAKE_SOURCE_DIR}/tools/rl/dqn_policy_probe.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/dqn_policy.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/dqn_net.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_include_directories(dqn_policy_probe PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(dqn_net_test ${CMAKE_SOURCE_DIR}/tests/rl/dqn_net_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/dqn_net.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/observation.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_link_libraries(dqn_net_test PRIVATE gtest_main gtest)
target_include_directories(dqn_net_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME dqn_net_test COMMAND dqn_net_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# G32-B: DqnMirrorBridge 测试 — 含"默认关闭时仲裁链不变"的核心保证
# mirror_agent.cpp 依赖 Logger / combat_system::rng / QAgent 等, 手列源文件
# 会不断缺符号; 改用 add_roguelike_test(自动链 roguelike_lib, 与既有
# mirror_* 测试同构)。该函数由 tests/CMakeLists.txt 定义, 故本目录必须
# 在 add_subdirectory(tests) 之后引入(根 CMakeLists.txt 已保证顺序)。
add_roguelike_test(dqn_bridge_test  ${CMAKE_SOURCE_DIR}/tests/rl/dqn_mirror_bridge_test.cpp)

# 黄金轨迹导出器, 供 tools/rl/parity_test.py 比对 Python 移植。非测试目标。
add_executable(parity_dump ${CMAKE_SOURCE_DIR}/tools/rl/parity_dump.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/environment.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/observation.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_include_directories(parity_dump PRIVATE ${CMAKE_SOURCE_DIR}/src)