# G7.2: Test target — links game sources as library + GoogleTest
# Excludes main.cpp (defines main() which would clash with gtest_main)

set(TEST_SOURCES "")
file(GLOB_RECURSE SRC_ALL "${CMAKE_SOURCE_DIR}/src/*.cpp")
foreach(f ${SRC_ALL})
    if(NOT f MATCHES "main\.cpp$" AND NOT f MATCHES "seh_handler\.cpp$")
        list(APPEND TEST_SOURCES ${f})
    endif()
endforeach()

# Build game sources as a static library (tests link against it)
add_library(roguelike_lib STATIC ${TEST_SOURCES})
target_include_directories(roguelike_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/game
    ${CMAKE_SOURCE_DIR}/src/game/entities
    ${CMAKE_SOURCE_DIR}/src/game/systems
    ${CMAKE_SOURCE_DIR}/src/game/world
    ${CMAKE_SOURCE_DIR}/src/game/scenes
    ${CMAKE_SOURCE_DIR}/src/game/audio
    ${CMAKE_SOURCE_DIR}/src/game/save
    ${CMAKE_SOURCE_DIR}/src/game/scene
    ${CMAKE_SOURCE_DIR}/src/game/types
    ${CMAKE_SOURCE_DIR}/src/game/resources
    ${CMAKE_SOURCE_DIR}/src/game/core
    ${CMAKE_SOURCE_DIR}/src/game/boss
    ${CMAKE_SOURCE_DIR}/src/game/director
    ${CMAKE_SOURCE_DIR}/src/data
    ${CMAKE_SOURCE_DIR}/src/core/replay
    ${CMAKE_SOURCE_DIR}/src/core/sim
    ${JSON_DIR}
    ${RAYLIB_DIR}/include
)
if(NOT MSVC)
    target_compile_options(roguelike_lib PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# Helper: copy runtime DLLs (raylib + MinGW runtime) next to a test exe
function(copy_test_runtime_dlls target)
    if(WIN32)
        get_filename_component(MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RAYLIB_DLL}" "$<TARGET_FILE_DIR:${target}>/raylib.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN}/libgcc_s_seh-1.dll" "$<TARGET_FILE_DIR:${target}>/libgcc_s_seh-1.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN}/libstdc++-6.dll" "$<TARGET_FILE_DIR:${target}>/libstdc++-6.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN}/libwinpthread-1.dll" "$<TARGET_FILE_DIR:${target}>/libwinpthread-1.dll")
    endif()
endfunction()

# Helper: define a test executable
function(add_roguelike_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE roguelike_lib gtest_main gtest)
    if(MSVC)
        target_link_libraries(${name} PRIVATE "${RAYLIB_IMPLIB}" winmm opengl32)
    elseif(WIN32)
        target_link_libraries(${name} PRIVATE "${RAYLIB_LIB}" winmm gdi32 opengl32)
    else()
        target_link_libraries(${name} PRIVATE raylib m pthread dl)
    endif()
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/game
        ${CMAKE_SOURCE_DIR}/src/game/world
        ${CMAKE_SOURCE_DIR}/src/game/entities
        ${CMAKE_SOURCE_DIR}/src/game/systems
        ${CMAKE_SOURCE_DIR}/src/game/scenes
        ${CMAKE_SOURCE_DIR}/src/data
        ${JSON_DIR}
        ${RAYLIB_DIR}/include
    )
    copy_test_runtime_dlls(${name})
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

# ── Test targets ──────────────────────────────────────────────

add_roguelike_test(registry_test    registry/registry_test.cpp)
add_roguelike_test(damage_test      combat/damage_test.cpp)
add_roguelike_test(damage_type_test combat/damage_type_test.cpp)
add_roguelike_test(projectile_test  combat/projectile_test.cpp)
add_roguelike_test(object_pool_test systems/object_pool_test.cpp)   # G11: 对象池槽位复用/回收/扩容安全
add_roguelike_test(buff_test        combat/buff_test.cpp)
add_roguelike_test(dodge_test       combat/dodge_test.cpp)   # B3: 翻滚纯逻辑组件
add_roguelike_test(animation_test  animation/animation_test.cpp)  # A5: 骨骼动画数据层+求值器
add_roguelike_test(biome_test       world/biome_test.cpp)
add_roguelike_test(spawn_tables_test spawn_tables_test.cpp)  # A6-S2 批次9: 刷怪表数据化 golden oracle
add_roguelike_test(encounter_test   world/encounter_test.cpp)
add_roguelike_test(condition_test   world/condition_test.cpp)
add_roguelike_test(fov_test         world/fov_test.cpp)
add_roguelike_test(dungeon_topology_test world/dungeon_topology_test.cpp)
add_roguelike_test(dungeon_verify_test world/dungeon_verify_test.cpp)
add_roguelike_test(door_seal_test world/door_seal_test.cpp)   # Batch 1: INVARIANT(seal) + 孔径完整性 + DoorState
add_roguelike_test(door_interact_test world/door_interact_test.cpp)  # Batch 2B: R1 接触开门 + CLOSED 语义
add_roguelike_test(room_encounter_test world/room_encounter_test.cpp) # Batch 2C: Room Encounter 状态机
add_roguelike_test(entity_center_test world/entity_center_test.cpp)  # Entity 视觉/碰撞中心不变量
add_roguelike_test(door_renderer_test world/door_renderer_test.cpp)  # Door sprites + anim
add_roguelike_test(minimap_test     ui/minimap_test.cpp)
add_roguelike_test(action_test      combat/action_test.cpp)
add_roguelike_test(relic_effect_test combat/relic_effect_test.cpp)
add_roguelike_test(save_test        save/save_test.cpp)
# G7.3: Simulation test (data-structure only, no game deps)
add_executable(simulation_test sim/simulation_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/sim/sim_runner.cpp)
target_link_libraries(simulation_test PRIVATE gtest_main gtest)
target_include_directories(simulation_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(simulation_test)
add_test(NAME simulation_test COMMAND simulation_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# P0-M2: SimAI teleport room-domain contract regression (locks F1 deadlock)
# sim_ai_teleport.cpp is auto-included via roguelike_lib GLOB
add_roguelike_test(p0_teleport_test sim/p0_teleport_test.cpp)

# G8.1: Behavior Tree test (pure C++, no game deps)
add_executable(behavior_tree_test ai/behavior_tree_test.cpp)
target_link_libraries(behavior_tree_test PRIVATE gtest_main gtest)
target_include_directories(behavior_tree_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(behavior_tree_test)
add_test(NAME behavior_tree_test COMMAND behavior_tree_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# G8.2: A* Pathfinder test (pure C++, no game deps)
add_executable(astar_test ai/astar_test.cpp ${CMAKE_SOURCE_DIR}/src/ai/navigation/pathfinder.cpp)
target_link_libraries(astar_test PRIVATE gtest_main gtest)
target_include_directories(astar_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(astar_test)
add_test(NAME astar_test COMMAND astar_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# G8.3: MCTS tests (pure C++, no game deps)
add_executable(sim_state_test mcts/simulation_state_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_link_libraries(sim_state_test PRIVATE gtest_main gtest)
target_include_directories(sim_state_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(sim_state_test)
add_test(NAME sim_state_test COMMAND sim_state_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(mcts_search_test mcts/mcts_search_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/mcts_search.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_link_libraries(mcts_search_test PRIVATE gtest_main gtest)
target_include_directories(mcts_search_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(mcts_search_test)
add_test(NAME mcts_search_test COMMAND mcts_search_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# G8.4: RL tests (pure C++, no game deps)
add_executable(env_test rl/environment_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/environment.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/observation.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_link_libraries(env_test PRIVATE gtest_main gtest)
target_include_directories(env_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(env_test)
add_test(NAME env_test COMMAND env_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(obs_test rl/observation_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/observation.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_link_libraries(obs_test PRIVATE gtest_main gtest)
target_include_directories(obs_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(obs_test)
add_test(NAME obs_test COMMAND obs_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(q_test rl/q_agent_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/q_agent.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/observation.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_link_libraries(q_test PRIVATE gtest_main gtest)
target_include_directories(q_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(q_test)
add_test(NAME q_test COMMAND q_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_roguelike_test(boot_test        integration/boot_test.cpp)

# G9: Weapon system tests
add_roguelike_test(weapon_test      weapon/weapon_test.cpp)
add_roguelike_test(weapon_special_test weapon/weapon_special_test.cpp)
add_roguelike_test(equipment_test   weapon/equipment_test.cpp)
add_roguelike_test(synergy_test     weapon/synergy_test.cpp)

# G10.1: Element Core tests
add_roguelike_test(element_test      element/element_test.cpp)
add_roguelike_test(element_combat    element/element_combat_test.cpp)
add_roguelike_test(element_vfx       element/element_vfx_test.cpp)

# M1: Behavior-clone table + MirrorAgent clone layer tests
add_roguelike_test(clone_test        mirror/behavior_clone_test.cpp)
add_roguelike_test(mirror_phase_test mirror/mirror_phase_test.cpp)
add_roguelike_test(mirror_arb_test    mirror/mirror_arbitration_test.cpp)
add_roguelike_test(mirror_acc_test    mirror/mirror_acc_test.cpp)
add_roguelike_test(analyzer_test      mirror/player_behavior_analyzer_test.cpp)
add_roguelike_test(mirror_tactic_test mirror/mirror_tactic_test.cpp)
add_roguelike_test(tactical_chain_test mirror/tactical_chain_test.cpp)
add_roguelike_test(sequence_e2e_test mirror/sequence_e2e_test.cpp)
# B3-M: CloneTable 跨局持久化闭环
add_roguelike_test(mirror_memory_store_test mirror/mirror_memory_store_test.cpp)

# Batch 3A: Economy & Persistence tests
add_roguelike_test(gold_test           economy/gold_test.cpp)
add_roguelike_test(key_test            economy/key_test.cpp)
add_roguelike_test(relic_persistence_test economy/relic_persistence_test.cpp)
add_roguelike_test(reward_manager_test economy/reward_manager_test.cpp)
add_roguelike_test(save_v4_test        economy/save_v4_test.cpp)
add_roguelike_test(gamble_room_test    economy/gamble_room_test.cpp)
add_roguelike_test(inventory_sell_ui_test economy/inventory_sell_ui_test.cpp)
add_roguelike_test(challenge_room_test    economy/challenge_room_test.cpp)
add_roguelike_test(challenge_portal_test  economy/challenge_portal_test.cpp)

add_roguelike_test(floor_lifecycle_test   world/floor_lifecycle_test.cpp)  # G9.2: 生命周期残留回归 (audit LIFE-001/002/003)
add_roguelike_test(rng_boundary_test      world/rng_boundary_test.cpp)   # G9.3: RNG 边界 (audit RNG-001/RNG-002)
add_roguelike_test(door_truth_table_test   world/door_truth_table_test.cpp)  # G9.4: DoorState Truth Table 回归锁定 (audit §9)
add_roguelike_test(asset_manifest_test   world/asset_manifest_test.cpp)   # G10.2-B1: Asset Manifest 校验 (ERROR/WARNING/INFO 分级)
add_roguelike_test(door_visual_mapping_test world/door_visual_mapping_test.cpp)  # G10.2-B2: 门视觉资产映射 (与 Truth Table 分层)

add_roguelike_test(audio_manifest_test    world/audio_manifest_test.cpp)  # G10.2-B3A: 音频清单化 + fallback 声明化

add_roguelike_test(camera_test            camera/camera_test.cpp)         # A6-T1: 摄像机语言数据层

add_roguelike_test(audio_test            audio/audio_test.cpp)            # A7-T1: 音频系统

add_roguelike_test(hud_test              hud/hud_test.cpp)                # A8-T1: 战斗 HUD 优化

add_roguelike_test(vfx_test              vfx/vfx_test.cpp)                # A9-T1: 攻击特效优化

# G10.2-B3B: font_manifest_test 处于 Review Gate (候选 A 缺 11 符号) — 未录用前不注册, 决策后启用或按裁决移除




