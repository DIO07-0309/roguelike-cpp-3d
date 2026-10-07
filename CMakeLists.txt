cmake_minimum_required(VERSION 3.16)
project(roguelike_cpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 强制 UTF-8: MSVC 源文件编码 + 运行期字面量编码
if(MSVC)
    add_compile_options("/utf-8")
    add_compile_options("/Zc:__cplusplus")
endif()

# ---- Raylib (vendored) ----
set(RAYLIB_DIR "${CMAKE_SOURCE_DIR}/vendor/raylib")
set(JSON_DIR   "${CMAKE_SOURCE_DIR}/vendor/json/include")

include_directories("${RAYLIB_DIR}/include")
include_directories("${JSON_DIR}")
include_directories("${CMAKE_SOURCE_DIR}/src")
include_directories("${CMAKE_SOURCE_DIR}/src/core")
include_directories("${CMAKE_SOURCE_DIR}/src/game")
include_directories("${CMAKE_SOURCE_DIR}/src/game/entities")
include_directories("${CMAKE_SOURCE_DIR}/src/game/systems")
include_directories("${CMAKE_SOURCE_DIR}/src/game/world")
include_directories("${CMAKE_SOURCE_DIR}/src/game/scenes")
include_directories("${CMAKE_SOURCE_DIR}/src/game/rendering")
include_directories("${CMAKE_SOURCE_DIR}/src/game/audio")
include_directories("${CMAKE_SOURCE_DIR}/src/game/save")
include_directories("${CMAKE_SOURCE_DIR}/src/game/tutorial")
include_directories("${CMAKE_SOURCE_DIR}/src/game/scene")
include_directories("${CMAKE_SOURCE_DIR}/src/game/types")
include_directories("${CMAKE_SOURCE_DIR}/src/game/resources")
include_directories("${CMAKE_SOURCE_DIR}/src/game/core")
include_directories("${CMAKE_SOURCE_DIR}/src/game/ui")       # Phase 3 Minimap
include_directories("${CMAKE_SOURCE_DIR}/src/game/boss")
include_directories("${CMAKE_SOURCE_DIR}/src/game/director")
include_directories("${CMAKE_SOURCE_DIR}/src/data")       # G1 Step5
include_directories("${CMAKE_SOURCE_DIR}/src/core/replay") # G4.5
include_directories("${CMAKE_SOURCE_DIR}/src/core/sim")    # G5.6
include_directories("${CMAKE_SOURCE_DIR}/src/ai")          # G8.1

# ---- Sources ----
file(GLOB_RECURSE SOURCES src/*.cpp)

# ---- Executable ----
# G10.7-B1: WIN32 subsystem (no console window) + game icon/version resource
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} src/game_icon.rc)

# MSVC: vendored raylib dll/lib
if(MSVC)
    set(RAYLIB_IMPLIB "${RAYLIB_DIR}/lib/raylibdll.lib")
    set(RAYLIB_DLL    "${RAYLIB_DIR}/lib/raylib.dll")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${RAYLIB_IMPLIB}" winmm opengl32
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RAYLIB_DLL}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/raylib.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    )
elseif(MINGW)
    # MinGW: vendored raylib .a + .dll
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
    set(RAYLIB_LIB "${RAYLIB_DIR}/lib/libraylibdll.a")
    set(RAYLIB_DLL "${RAYLIB_DIR}/lib/raylib.dll")
    # G10.9-fix3: 全静态链接 MinGW 运行库 — exe 自包含, 不再依赖
    # libgcc_s_seh-1.dll / libstdc++-6.dll / libwinpthread-1.dll 在目标机器 PATH
    # (-static 覆盖 winpthread; raylib 走 import lib 不受影响, 仍需 raylib.dll 随包)
    target_link_options(${PROJECT_NAME} PRIVATE -static)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${RAYLIB_LIB}" winmm gdi32 opengl32 dbghelp   # P0-M1: dbghelp 崩溃栈符号化
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RAYLIB_DLL}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/raylib.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    )
else()
    # Linux / macOS: system-installed raylib (from CI: cmake --install)
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        raylib m pthread dl
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    )
endif()

# G7.2: ENABLE_TESTS option
option(ENABLE_TESTS "Build automated test suite" OFF)
if(ENABLE_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.14.0)
    FetchContent_MakeAvailable(googletest)
    add_subdirectory(tests)
    # v0.9.32 G32: DQN 手写推理验证目标放独立目录 — tests/CMakeLists.txt
    # 在并行开发中被反复还原, 改那里会被冲掉
    add_subdirectory(tests/dqn)
endif()
