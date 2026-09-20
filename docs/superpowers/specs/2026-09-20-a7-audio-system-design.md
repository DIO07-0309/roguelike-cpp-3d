# A7 音效/音乐系统 v1 设计 spec

> 前置: A6 镜头语言已完成，v1.8-A6 tag 已打
> 日期: 2026-09-20 · 状态: 待审阅

## 1. 目标与非目标

**目标**: 为游戏添加完整音景，提升沉浸感和战斗反馈。
- 三群系 BGM 完整化（监牢/火山/深渊各有专属风格）
- Boss 专属音乐（F5/F10/F15 各有主题）
- 战斗反馈音效（命中/死亡/技能/UI）
- 保留现有资源（时停音乐/领域展开音效）

**非目标 (v1 明确排除)**:
- 不做动态音乐切换（保持静态 BGM 循环）
- 不做语音旁白（仅音效和音乐）
- 不做 3D 空间音频（保持 2D 立体声）
- 不改任何战斗逻辑/手感（sim 逐字节零变化）

## 2. 决策记录

| 决策点 | 结论 | 理由 |
|:--|:--|:--|
| 技术路线 | 混合方案 | 关键音效用资源，BGM 用程序合成 |
| 音源策略 | Kenney CC0 + 程序合成 | 音效用 CC0 素材，BGM 程序生成 |
| BGM 优先 | 先做 BGM 再补音效 | BGM 对沉浸感影响最大 |
| 资源保留 | 时停/领域展开不换 | 用户专门寻找的音效 |
| 降级策略 | 编译失败回退 dungeon BGM | 不崩溃，不打断游戏 |

## 3. 数据契约

### 现有 BGM (程序合成)
```cpp
// bgm_engine.cpp - _compile_bgm() 支持的 BGM 名称
"title"       // 标题音乐 (110 BPM)
"select"      // 选择界面 (85 BPM)
"dungeon"     // 地牢音乐 (75 BPM)
"boss"        // Boss 音乐 (150 BPM)
"victory"     // 通关音乐 (132 BPM)
"challenge"   // 挑战房 (160 BPM)
"prison"      // 监牢群系 (72 BPM)
"volcano"     // 火山群系 (90 BPM)
"abyss"       // 深渊群系 (62 BPM)
```

### 新增 BGM (T2)
```cpp
"boss_f5"     // F5 Boss 暗影骑士 (150 BPM, 紧张进行)
"boss_f10"    // F10 Boss 地狱火魔 (160 BPM, 灼热主题)
"boss_f15"    // F15 Boss 深渊之主 (140 BPM, 虚空主题)
```

### 现有资源 (保留)
```
assets/jojo_timestop.mp3    // 时停音乐 (不要换)
assets/domain_expand.mp3    // 领域展开音效 (不要换)
assets/jojo_timestop.wav
assets/domain_expand.wav
```

### 音效资源 (T4, 后补)
```
assets/sfx/hit_sword.wav    // 剑命中
assets/sfx/hit_spear.wav    // 矛命中
assets/sfx/hit_dagger.wav   // 双截棍命中
assets/sfx/hit_crossbow.wav // 连弩命中
assets/sfx/hit_staff.wav    // 法杖命中
assets/sfx/death_normal.wav // 普通怪死亡
assets/sfx/death_elite.wav  // 精英怪死亡
assets/sfx/death_boss.wav   // Boss 死亡
assets/sfx/skill_cast.wav   // 技能释放
assets/sfx/ui_click.wav     // UI 点击
assets/sfx/ui_select.wav    // UI 选择
```

## 4. 架构

```
AudioServer (管理层)
├── BGMEngine (程序合成 BGM)
│   ├── _compile_bgm() - 合成 BGM
│   ├── _cache - BGM 缓存
│   └── update() - 循环播放
└── _sfx - 音效缓存 (资源文件)
    ├── play_sfx() - 播放音效
    └── update() - 清理已释放音效
```

## 5. 组件

### T1: 三群系 BGM 完整化
- 监牢 (prison): 72 BPM, square wave, 压抑感
- 火山 (volcano): 90 BPM, saw wave, 灼热感
- 深渊 (abyss): 62 BPM, triangle wave, 虚空感

### T2: Boss 专属音乐
- F5 Boss (暗影骑士): 150 BPM, 紧张进行
- F10 Boss (地狱火魔): 160 BPM, 灼热主题
- F15 Boss (深渊之主): 140 BPM, 虚空主题

### T3: AudioServer 集成
- `play_bgm("prison")` / `play_bgm("volcano")` / `play_bgm("abyss")`
- `play_bgm("boss_f5")` / `play_bgm("boss_f10")` / `play_bgm("boss_f15")`
- 保留 `play_bgm("timestop")` / `play_bgm("domain_expand")` (资源文件)

### T4: 战斗反馈音效（后补）
- 武器命中音效 (5 类武器)
- 怪物死亡音效 (普通/精英/Boss)
- 技能释放音效
- UI 交互音效

## 6. 数据流

```
游戏事件 → AudioServer.play_bgm()
    → BGMEngine.play()
    → _compile_bgm() (首次调用)
    → PlaySound() + 循环播放

游戏事件 → AudioServer.play_sfx()
    → LoadSound() (首次调用)
    → PlaySound()
```

## 7. 错误处理

- BGM 编译失败：回退到 `dungeon` BGM
- 资源文件缺失：静默跳过，不崩溃
- sim 模式：`g_muted = true`，跳过所有音频

## 8. 测试

- **T1-T2**: BGM 编译测试（各群系/Boss 音乐可编译）
- **T3**: AudioServer 集成测试（播放/停止/切换）
- **T4**: 音效播放测试（资源加载/播放）

## 9. 影响面

### 修改文件
- `src/game/audio/bgm_engine.cpp` - 扩展 BGM 序列
- `src/game/audio/audio_server.h/.cpp` - 音效集成
- `src/game/scenes/game_scene.cpp` - BGM 触发点
- `src/game/scene/game_scene_combat.cpp` - 音效触发点

### 新增文件
- `tests/audio/audio_test.cpp` - 音频测试
- `tests/CMakeLists.txt` - 注册测试
- `resources/audio/` - 音效资源目录 (T4)

### 不改动的文件
- `assets/jojo_timestop.mp3` / `.wav` - 时停音乐
- `assets/domain_expand.mp3` / `.wav` - 领域展开音效
- 所有战斗逻辑代码 (sim 逐字节零变化)

## 10. 验证

### 自动化门禁
- Release 0 error
- ctest 65/65 (新增 audio_test)
- World Validator 0/0 (无 JSON 改动)
- sim 逐字节一致 (`--sim 12 --sim-seed 3`)

### 实机验收
- Boss 战音乐播放正常
- 三群系 BGM 风格区分明显
- 时停音乐和领域展开音效保留
- sim 模式无音频输出

## 11. 里程碑

- **T1**: 三群系 BGM 完整化 (1 天)
- **T2**: Boss 专属音乐 (1 天)
- **T3**: AudioServer 集成 (0.5 天)
- **T4**: 战斗反馈音效 (2 天, 后补)
- **总计**: 4.5 天 (T1-T3 优先, T4 后补)
