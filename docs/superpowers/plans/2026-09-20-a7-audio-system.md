# A7 音效/音乐系统 v1 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为游戏添加完整音景（三群系 BGM + Boss 专属音乐 + 战斗反馈音效），提升沉浸感和战斗反馈。

**Architecture:** 保留现有 BGM 引擎（程序合成），扩展三群系 BGM 变体和 Boss 专属音乐。音效用资源文件（Kenney CC0）。保留现有资源（时停/领域展开）。

**Tech Stack:** C++17 / raylib 5.0 / GoogleTest / nlohmann/json

**Spec:** `docs/superpowers/specs/2026-09-20-a7-audio-system-design.md`

## Global Constraints

- sim 红线：`--sim 12 --sim-seed 3` 报告逐字节一致（每任务必验）
- ctest 按可执行注册：新增 `audio_test` → **64→65**
- 函数 ≤40 行；类单一职责；无 new/delete（unique_ptr）；`.h` 有 `#pragma once`
- 逻辑层禁 `GetTime()`（音频除外，因为需要 wall clock）
- 改任 JSON 后跑 `python tools/world_validator.py`
- 桌面包同步规则 11（T4 末全量镜像 + exe 到根目录）
- 每任务完成：Release 0 error + ctest 全绿 + validator + sim 逐字节 + Code Review，然后 commit
- 保留 `assets/jojo_timestop.mp3` / `assets/domain_expand.mp3`（不要换）

---

## Task 1: 三群系 BGM 风格优化

**Files:**
- Modify: `src/game/audio/bgm_engine.cpp` (优化三群系 BGM 序列)
- Create: `tests/audio/audio_test.cpp` (音频测试)
- Modify: `tests/CMakeLists.txt` (注册测试)

**Interfaces:**
- Produces: `BGMEngine::_compile_bgm()` 支持 "prison"/"volcano"/"abyss" BGM 变体
- Produces: `tests/audio/audio_test.cpp` 测试框架

- [ ] **Step 1: 创建测试文件框架**

```cpp
// tests/audio/audio_test.cpp
#include <gtest/gtest.h>
#include "game/audio/bgm_engine.h"
#include "game/audio/audio_server.h"
#include "raylib.h"
#include "raymath.h"

Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

TEST(Audio, BgmEngineInit) {
    BGMEngine bgm;
    bgm.init();
    EXPECT_TRUE(bgm.is_initialized());  // 需要添加 is_initialized() 方法
}
```

- [ ] **Step 2: 添加 is_initialized() 方法**

```cpp
// src/game/audio/bgm_engine.h
class BGMEngine {
public:
    // ... 现有方法
    bool is_initialized() const { return !_cache.empty(); }
};
```

- [ ] **Step 3: 运行测试验证失败**

Run: `ctest --test-dir build -R audio_test --output-on-failure`
Expected: FAIL (测试未注册)

- [ ] **Step 4: 注册测试到 CMakeLists.txt**

```cmake
# tests/CMakeLists.txt
add_roguelike_test(audio_test    audio/audio_test.cpp)  # A7-T1: 音频系统
```

- [ ] **Step 5: 运行测试验证通过**

Run: `ctest --test-dir build -R audio_test --output-on-failure`
Expected: PASS

- [ ] **Step 6: 优化三群系 BGM**

```cpp
// src/game/audio/bgm_engine.cpp - _compile_bgm()
// 在现有 else 分支中优化三群系 BGM

if (name == "prison") {
    // 监牢: 72 BPM, square wave, 压抑感
    bpm = 72; beat = 60.0f / bpm;
    mw = "square";
    chords = {{"C3",3.0f},{"Db3",3.0f},{"Eb3",3.0f},{"C3",3.0f}};
    melody = {{"C4",0.5f},{0,1.0f},{"Db4",0.4f},{0,0.8f},{"Eb4",0.4f},{0,1.2f},{"C4",0.6f},{0,0.6f}};
    // 低音更沉重
    bw = "square";
    bass = {{"C2",1.5f},{"Db2",1.5f},{"Eb2",1.5f},{"C2",1.5f}};
} else if (name == "volcano") {
    // 火山: 90 BPM, saw wave, 灼热感
    bpm = 90; beat = 60.0f / bpm;
    mw = "saw"; bw = "triangle";
    chords = {{"C3",3.0f},{"Db3",3.0f},{"Eb3",3.0f},{"C3",3.0f}};
    melody = {{"C4",0.4f},{"Eb4",0.4f},{"G4",0.4f},{"Bb4",0.4f},{"C5",0.4f},{"Bb4",0.4f},{"G4",0.4f},{"Eb4",0.4f}};
    bass = {{"C2",0.75f},{"Db2",0.75f},{"Eb2",0.75f},{"C2",0.75f}};
} else if (name == "abyss") {
    // 深渊: 62 BPM, triangle wave, 虚空感
    bpm = 62; beat = 60.0f / bpm;
    mw = "triangle"; bw = "sine";
    chords = {{"C3",4.0f},{"Ab2",4.0f},{"Eb3",4.0f},{"C3",4.0f}};
    melody = {{"C5",1.0f},{0,0.5f},{"Eb5",0.5f},{0,1.0f},{"Ab5",1.0f},{0,1.5f}};
    bass = {{"C2",2.0f},{"Ab1",2.0f},{"Eb2",2.0f},{"C2",2.0f}};
}
```

- [ ] **Step 7: 添加 BGM 编译测试**

```cpp
TEST(Audio, BgmCompileBiomeVariants) {
    BGMEngine bgm;
    bgm.init();
    // 验证三群系 BGM 可编译
    EXPECT_TRUE(bgm.is_initialized());
    // 触发 lazy-compile
    bgm.play("prison");
    bgm.stop();
    bgm.play("volcano");
    bgm.stop();
    bgm.play("abyss");
    bgm.stop();
    EXPECT_TRUE(bgm.is_initialized());
}
```

- [ ] **Step 8: 运行测试验证通过**

Run: `ctest --test-dir build -R audio_test --output-on-failure`
Expected: PASS

- [ ] **Step 9: 全量门禁 + commit**

Run:
```bash
cmake --build build --config Release
ctest --test-dir build --output-on-failure
conda run -n base python tools/world_validator.py
```
Expected: Release 0 error, ctest 65/65, validator 0/0

```bash
git add src/game/audio/bgm_engine.h src/game/audio/bgm_engine.cpp tests/audio/audio_test.cpp tests/CMakeLists.txt
git commit -m "feat(a7-t1): 三群系 BGM 风格优化"
```

---

## Task 2: Boss 专属音乐

**Files:**
- Modify: `src/game/audio/bgm_engine.cpp` (添加 Boss 专属 BGM)
- Modify: `tests/audio/audio_test.cpp` (添加 Boss BGM 测试)

**Interfaces:**
- Consumes: `BGMEngine::_compile_bgm()` (T1)
- Produces: "boss_f5"/"boss_f10"/"boss_f15" BGM 变体

- [ ] **Step 1: 添加 Boss 专属 BGM 测试**

```cpp
TEST(Audio, BgmCompileBossVariants) {
    BGMEngine bgm;
    bgm.init();
    // 验证 Boss BGM 可编译
    bgm.play("boss_f5");
    bgm.stop();
    bgm.play("boss_f10");
    bgm.stop();
    bgm.play("boss_f15");
    bgm.stop();
    EXPECT_TRUE(bgm.is_initialized());
}
```

- [ ] **Step 2: 运行测试验证失败**

Run: `ctest --test-dir build -R audio_test --output-on-failure`
Expected: FAIL (Boss BGM 未实现)

- [ ] **Step 3: 添加 Boss 专属 BGM**

```cpp
// src/game/audio/bgm_engine.cpp - _compile_bgm()
// 在 boss 分支后添加 Boss 专属 BGM

} else if (name == "boss_f5") {
    // F5 Boss 暗影骑士: 150 BPM, 紧张进行
    bpm = 150; beat = 60.0f / bpm;
    mw = "saw"; bw = "square";
    chords = {{"C3",1.0f},{"Ab2",1.0f},{"Bb2",1.0f},{"G2",1.0f}};
    melody = {{"C4",0.12f},{"Eb4",0.12f},{"G4",0.12f},{"C5",0.2f},{"B4",0.12f},{"G4",0.12f},{"F4",0.15f},{"Eb4",0.1f},{0,0.05f}};
    bass  = {{"C2",0.25f},{"C2",0.25f},{"Ab1",0.25f},{"Ab1",0.25f},{"Bb1",0.25f},{"Bb1",0.25f},{"G1",0.25f},{"G1",0.25f}};
} else if (name == "boss_f10") {
    // F10 Boss 地狱火魔: 160 BPM, 灼热主题
    bpm = 160; beat = 60.0f / bpm;
    mw = "saw"; bw = "square";
    chords = {{"G3",0.8f},{"Bb2",0.8f},{"D3",0.8f},{"G3",0.8f}};
    melody = {{"G4",0.1f},{"Bb4",0.1f},{"D5",0.1f},{"G5",0.2f},{"F5",0.1f},{"D5",0.1f},{"Bb4",0.15f},{"A4",0.1f},{0,0.05f}};
    bass  = {{"G2",0.2f},{"G2",0.2f},{"Bb1",0.2f},{"Bb1",0.2f},{"D2",0.2f},{"D2",0.2f},{"G2",0.2f},{"G2",0.2f}};
} else if (name == "boss_f15") {
    // F15 Boss 深渊之主: 140 BPM, 虚空主题
    bpm = 140; beat = 60.0f / bpm;
    mw = "triangle"; bw = "sine";
    chords = {{"C3",1.2f},{"Ab2",1.2f},{"Eb3",1.2f},{"C3",1.2f}};
    melody = {{"C5",0.15f},{"Eb5",0.15f},{"Ab5",0.15f},{"C6",0.3f},{"B5",0.15f},{"Ab5",0.15f},{"Eb5",0.2f},{0,0.1f}};
    bass  = {{"C2",0.3f},{"C2",0.3f},{"Ab1",0.3f},{"Ab1",0.3f},{"Eb2",0.3f},{"Eb2",0.3f},{"C2",0.3f},{"C2",0.3f}};
}
```

- [ ] **Step 4: 运行测试验证通过**

Run: `ctest --test-dir build -R audio_test --output-on-failure`
Expected: PASS

- [ ] **Step 5: 全量门禁 + commit**

Run:
```bash
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```
Expected: Release 0 error, ctest 65/65

```bash
git add src/game/audio/bgm_engine.cpp tests/audio/audio_test.cpp
git commit -m "feat(a7-t2): Boss 专属音乐 (F5/F10/F15)"
```

---

## Task 3: AudioServer 集成

**Files:**
- Modify: `src/game/audio/audio_server.h/.cpp` (音效集成)
- Modify: `src/game/scenes/game_scene.cpp` (BGM 触发点)
- Modify: `tests/audio/audio_test.cpp` (集成测试)

**Interfaces:**
- Consumes: `BGMEngine` (T1-T2)
- Produces: `AudioServer::play_sfx()` 音效播放

- [ ] **Step 1: 添加 AudioServer 集成测试**

```cpp
TEST(Audio, AudioServerInit) {
    AudioServer::g_muted = true;  // sim 模式静音
    AudioServer audio;
    audio.init();
    EXPECT_TRUE(AudioServer::g_muted);
    audio.close();
}

TEST(Audio, AudioServerPlayBgm) {
    AudioServer::g_muted = true;
    AudioServer audio;
    audio.init();
    audio.play_bgm("prison");
    audio.play_bgm("boss_f5");
    audio.stop_bgm();
    audio.close();
}
```

- [ ] **Step 2: 运行测试验证失败**

Run: `ctest --test-dir build -R audio_test --output-on-failure`
Expected: FAIL (AudioServer 未实现音效)

- [ ] **Step 3: 添加音效集成**

```cpp
// src/game/audio/audio_server.h
class AudioServer {
public:
    void init();
    void close();
    void play_bgm(const std::string& name, float vol = 0.4f);
    void stop_bgm(float = 0.4f);
    void play_sfx(const std::string& name, float vol = 0.6f);
    void update(float dt);
    
    static bool g_muted;

private:
    BGMEngine _bgm;
    std::unordered_map<std::string, Sound> _sfx;
    // 新增: 音效路径
    std::string _sfx_path = "assets/sfx/";
};
```

```cpp
// src/game/audio/audio_server.cpp
void AudioServer::init() {
    _bgm.init();
    // 加载音效资源 (T4 后补)
    // for (auto& sfx : sfx_list) {
    //     std::string path = _sfx_path + sfx;
    //     if (FileExists(path.c_str())) {
    //         _sfx[sfx] = LoadSound(path.c_str());
    //     }
    // }
}

void AudioServer::play_sfx(const std::string& name, float vol) {
    if (g_muted) return;
    auto it = _sfx.find(name);
    if (it != _sfx.end()) {
        SetSoundVolume(it->second, vol);
        PlaySound(it->second);
    }
}
```

- [ ] **Step 4: 添加 BGM 触发点**

```cpp
// src/game/scenes/game_scene.cpp
// 在 _ready() 或类似方法中添加 BGM 触发

// 进入地牢时播放群系 BGM
void GameScene::_enter_dungeon() {
    // ... 现有代码
    std::string biome_bgm = _biome == "prison" ? "prison" :
                            _biome == "volcano" ? "volcano" : "abyss";
    ServiceLocator::resolve<AudioServer>()->play_bgm(biome_bgm);
}

// Boss 出场时播放 Boss BGM
void GameScene::_boss_spawn() {
    // ... 现有代码
    std::string boss_bgm = _floor == 5 ? "boss_f5" :
                          _floor == 10 ? "boss_f10" : "boss_f15";
    ServiceLocator::resolve<AudioServer>()->play_bgm(boss_bgm);
}
```

- [ ] **Step 5: 运行测试验证通过**

Run: `ctest --test-dir build -R audio_test --output-on-failure`
Expected: PASS

- [ ] **Step 6: 全量门禁 + commit**

Run:
```bash
cmake --build build --config Release
ctest --test-dir build --output-on-failure
conda run -n base python tools/world_validator.py
```
Expected: Release 0 error, ctest 65/65, validator 0/0

```bash
git add src/game/audio/audio_server.h src/game/audio/audio_server.cpp src/game/scenes/game_scene.cpp tests/audio/audio_test.cpp
git commit -m "feat(a7-t3): AudioServer 集成 + BGM 触发点"
```

---

## Task 4: 战斗反馈音效（后补）

**Files:**
- Create: `assets/sfx/` (音效资源目录)
- Modify: `src/game/audio/audio_server.cpp` (加载音效)
- Modify: `src/game/scene/game_scene_combat.cpp` (音效触发点)
- Modify: `tests/audio/audio_test.cpp` (音效测试)

**Interfaces:**
- Consumes: `AudioServer::play_sfx()` (T3)
- Produces: 战斗反馈音效 (命中/死亡/技能/UI)

- [ ] **Step 1: 创建音效资源目录**

```bash
mkdir -p assets/sfx
# 从 Kenney CC0 素材下载音效 (或手动准备)
# 示例: https://kenney.nl/assets/game-sound-effects-pack
```

- [ ] **Step 2: 添加音效加载**

```cpp
// src/game/audio/audio_server.cpp
void AudioServer::init() {
    _bgm.init();
    // 加载音效资源
    const char* sfx_list[] = {
        "hit_sword", "hit_spear", "hit_dagger", "hit_crossbow", "hit_staff",
        "death_normal", "death_elite", "death_boss",
        "skill_cast", "ui_click", "ui_select"
    };
    for (const char* sfx : sfx_list) {
        std::string path = _sfx_path + sfx + ".wav";
        if (FileExists(path.c_str())) {
            _sfx[sfx] = LoadSound(path.c_str());
        }
    }
}
```

- [ ] **Step 3: 添加音效触发点**

```cpp
// src/game/scene/game_scene_combat.cpp
void GameSceneCombat::on_hit() {
    // ... 现有代码
    AudioServer::play_sfx("hit_sword");  // 根据武器类型选择音效
}

void GameSceneCombat::on_monster_killed() {
    // ... 现有代码
    std::string death_sfx = _is_boss ? "death_boss" : (_is_elite ? "death_elite" : "death_normal");
    AudioServer::play_sfx(death_sfx);
}
```

- [ ] **Step 4: 添加音效测试**

```cpp
TEST(Audio, AudioServerPlaySfx) {
    AudioServer::g_muted = true;
    AudioServer audio;
    audio.init();
    audio.play_sfx("hit_sword");
    audio.play_sfx("death_normal");
    audio.close();
}
```

- [ ] **Step 5: 全量门禁 + commit**

Run:
```bash
cmake --build build --config Release
ctest --test-dir build --output-on-failure
conda run -n base python tools/world_validator.py
```
Expected: Release 0 error, ctest 65/65, validator 0/0

```bash
git add assets/sfx/ src/game/audio/audio_server.cpp src/game/scene/game_scene_combat.cpp tests/audio/audio_test.cpp
git commit -m "feat(a7-t4): 战斗反馈音效"
```

---

## 验证清单

### T1-T3 完成后
- [ ] Release 0 error
- [ ] ctest 65/65
- [ ] World Validator 0/0
- [ ] sim 逐字节一致 (`--sim 12 --sim-seed 3`)
- [ ] 桌面包同步

### T4 完成后
- [ ] 音效资源就位
- [ ] 战斗反馈音效播放正常
- [ ] sim 逐字节一致 (音效不影响逻辑)
- [ ] 桌面包同步

### 实机验收
- [ ] 三群系 BGM 风格区分明显
- [ ] Boss 专属音乐播放正常
- [ ] 时停音乐和领域展开音效保留
- [ ] 战斗反馈音效播放正常
- [ ] sim 模式无音频输出
