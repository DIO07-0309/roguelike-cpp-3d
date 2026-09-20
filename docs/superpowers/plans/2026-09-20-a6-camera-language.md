# A6 摄像机语言 v1 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Boss 战运镜 + 击杀顿帧 + 屏幕震动，2D+HD-2D 双端，sim 零变化。
**Architecture:** 数据驱动（JSON → 加载器 → 状态机）；渲染侧单点分叉；Player/Boss 实体零新成员。
**Tech Stack:** C++17 / raylib 5.0 / nlohmann/json / GoogleTest
**Spec:** `docs/superpowers/specs/2026-09-20-a6-camera-language-design.md`

## Global Constraints

- sim 红线：`--sim 12 --sim-seed 3` 报告逐字节一致（每任务必验）
- ctest 按可执行注册：新增 1 个 `camera_test` → **63→64**
- 函数 ≤40 行；类单一职责；无 new/delete（unique_ptr）；`.h` 有 `#pragma once`
- 逻辑层禁 `GetTime()`（hit-stop 除外，因为需要 wall clock）
- 改任 JSON 后跑 `python tools/world_validator.py`
- 桌面包同步规则 11（T6 末全量镜像 + exe 到根目录）
- 每任务完成：Release 0 error + ctest 全绿 + validator + sim 逐字节 + Code Review，然后 commit

---

## Task 1: 数据层 — JSON schema + 加载器 + validator

**Files:**
- Create: `resources/camera/boss_camera.json`
- Create: `src/data/camera_defs.h` / `src/data/camera_defs.cpp`
- Modify: `CMakeLists.txt`（src 源文件列表）
- Modify: `tools/world_validator.py`（相机交叉引用段）
- Create: `tests/camera/camera_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces (Produces):**
```cpp
// src/data/camera_defs.h
struct ZoomDef { float fov_scale = 1.0f; float duration = 0.0f; };
struct BossWarDef { ZoomDef zoom_in; ZoomDef zoom_out; float lerp_speed = 2.0f; };
struct KillStunDef { float duration = 0.08f; float shake_amplitude = 3.0f; float shake_frequency = 20.0f; };
struct CameraDef { BossWarDef boss_war; KillStunDef kill_stun; };
std::optional<CameraDef> load_camera_file(const std::string& path, std::string& err);
```

- [ ] **Step 1: 写失败测试** `tests/camera/camera_test.cpp`
- [ ] **Step 2: 实现加载器** `src/data/camera_defs.h/.cpp`
- [ ] **Step 3: 创建 JSON** `resources/camera/boss_camera.json`
- [ ] **Step 4: 注册 CMake + validator**
- [ ] **Step 5: 全量门禁 + commit**

---

## Task 2: HitStop 计时器 + GameScene 集成

**Files:**
- Create: `src/game/systems/hit_stop.h` / `src/game/systems/hit_stop.cpp`
- Modify: `src/game/scene/game_scene.cpp` / `.h`
- Modify: `tests/camera/camera_test.cpp`

**Interfaces (Produces):**
```cpp
// src/game/systems/hit_stop.h
class HitStop {
public:
    void trigger(float duration);
    void update(float dt);          // wall clock
    bool active() const;
    float remaining() const;
    bool is_stunned() const;        // 游戏暂停信号
};
```

- [ ] **Step 1: 写失败测试**
- [ ] **Step 2: 实现 HitStop**
- [ ] **Step 3: 集成到 GameScene**
- [ ] **Step 4: 全量门禁 + commit**

---

## Task 3: CameraDirector 状态机 + 插值

**Files:**
- Create: `src/game/director/camera_director.h` / `.cpp`
- Modify: `tests/camera/camera_test.cpp`

**Interfaces (Produces):**
```cpp
// src/game/director/camera_director.h
enum class CameraState { NORMAL, BOSS_WAR, KILL_STUN };
class CameraDirector {
public:
    bool try_init(const CameraDef& def);
    void enter_boss_war();
    void exit_boss_war();
    void trigger_kill_stun();
    void update(float dt, const Vector2& player_pos, const Vector2& boss_pos);
    Vector2 focus_offset() const;    // 相对玩家的位置偏移
    float fov_scale() const;
    CameraState state() const;
};
```

- [ ] **Step 1: 写失败测试**
- [ ] **Step 2: 实现 CameraDirector**
- [ ] **Step 3: 全量门禁 + commit**

---

## Task 4: 2D/3D 渲染接入 + Boss 触发

**Files:**
- Modify: `src/game/scene/game_scene.cpp`（相机读取）
- Modify: `src/game/rendering3d/hd2d_renderer.cpp`（focus/fov 写入）
- Modify: `src/game/entities/boss.cpp`（出场触发）
- Modify: `tests/camera/camera_test.cpp`

- [ ] **Step 1: 写失败测试**
- [ ] **Step 2: 接入 2D 相机**
- [ ] **Step 3: 接入 3D 相机**
- [ ] **Step 4: Boss 出场触发**
- [ ] **Step 5: 全量门禁 + commit**

---

## Task 5: 击杀顿帧 + 震动

**Files:**
- Modify: `src/game/scene/game_scene.cpp`（击杀触发）
- Modify: `src/game/systems/combat_system.cpp`（hit-stop 信号）
- Modify: `tests/camera/camera_test.cpp`

- [ ] **Step 1: 写失败测试**
- [ ] **Step 2: 接入击杀顿帧**
- [ ] **Step 3: 震动叠加**
- [ ] **Step 4: 全量门禁 + commit**

---

## Task 6: 文档/桌面包/tag

- [ ] 更新 CHANGELOG.md
- [ ] 桌面包全量镜像 + exe 到根目录
- [ ] 跑全量门禁
- [ ] 打 v1.8-A6 tag
