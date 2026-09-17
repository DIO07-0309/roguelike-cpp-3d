# A5 玩家骨骼动画 v1 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 玩家四件套骨骼动画（idle/walk/attack/hit），自研 2.5D 分件骨骼，2D+HD-2D 双端，sim 零变化。

**Architecture:** 数据驱动（skeleton+anim JSON → 加载器 → 纯数学求值器 → animator）；渲染侧单点分叉（骨骼 all-or-nothing，失败整条回退静帧）；Player 实体零新成员。

**Tech Stack:** C++17 / raylib 5.0 (DrawTexturePro, DrawBillboardPro) / nlohmann/json / GoogleTest / Python PIL 工具。

**Spec:** `docs/superpowers/specs/2026-09-17-a5-skeletal-animation-design.md`（决策与 schema 以 spec 为准）

## Global Constraints

- sim 红线：`--sim 12 --sim-seed 3` → `reports/balance_report.json` 与 pre-A5 基线**逐字节一致**（每任务必验；先例基线法：实施前 `Copy-Item reports\balance_report.json $env:TEMP\opencode\pre_a5_report.json`）
- ctest 按可执行注册：新增 1 个 `animation_test` → **62→63/63**（T1/T2 用例同文件）
- 函数 ≤40 行；类单一职责；无 new/delete（unique_ptr）；`.h` 用 `#pragma once`；加载器返回 `std::optional`；PascalCase 类 / camelCase 方法 / snake_case 变量与 JSON 键
- 逻辑/数据层禁 `GetTime()`（pose 求值时间由调用方传入）；零 RNG；不引入第三方库
- CMakeLists 仅加源文件/测试注册，编译标志不动；改任何 JSON 后跑 `python tools/world_validator.py`
- 中文文件编辑**禁止** PowerShell `-replace`/`Get-Content -Raw` 回写（mojibake 事故先例），用 Write 工具或 `C:\Users\HP\anaconda3\python.exe` 脚本
- exe 链接前杀残留：`Get-Process roguelike_cpp -EA SilentlyContinue | Stop-Process -Force; Start-Sleep 2`
- 冒烟取图：`--autoshot 3600 --hidwin`（字体 atlas ~1s，帧数太少会早拍空屏）；截图判读用 PIL 像素统计
- 每任务完成：Release 0 error + ctest 全绿 + validator + sim 逐字节 + Code Review，然后 commit
- 桌面包同步规则 11（T5 末全量镜像 + exe 到根目录）

---

## Task 1: 数据层 — JSON schema + 加载器 + 占位件 + validator

**Files:**
- Create: `resources/animations/player_skeleton.json`、`resources/animations/player_anim.json`
- Create: `tools/gen_placeholder_parts.py`（生成 `assets/sprites/player_part_{head,torso,arm,leg,weapon}.png` 占位件）
- Create: `tools/anim_preview.py`（按 bind pose 合成预览图到 `reports/anim_preview.png`）
- Create: `src/data/animation_defs.h` / `src/data/animation_defs.cpp`
- Modify: `CMakeLists.txt`（src 源文件列表，参照 enemy_defs 注册处）
- Modify: `tools/world_validator.py`（动画交叉引用段）
- Create: `tests/animation/animation_test.cpp`
- Modify: `tests/CMakeLists.txt`（在 :221 后加 `add_roguelike_test(animation_test animation/animation_test.cpp)`）

**Interfaces (Produces):**
```cpp
// src/data/animation_defs.h
struct BoneDef  { std::string name; int parent = -1; float x = 0, y = 0; };       // bind 局部, Y 向上
struct PartDef  { int bone = -1; std::string file; float dx = 0, dy = 0; float pivot_x = 0, pivot_y = 0; };
struct SkeletonDef { float pixels_per_unit = 0.5f; float anchor_x = 0, anchor_y = 0;
                     std::vector<BoneDef> bones; std::vector<PartDef> parts; };   // parts 序 = 绘制序
struct KeyDef   { float t = 0, x = 0, y = 0, rot = 0, sx = 1, sy = 1; };
struct TrackDef { int bone = -1; std::vector<KeyDef> keys; };
struct AnimClipDef { bool loop = false; float dur = 0; std::vector<TrackDef> tracks; };
struct AnimSetDef { std::map<std::string, AnimClipDef> clips; };
std::optional<SkeletonDef> parse_skeleton(const nlohmann::json& j, std::string& err);
std::optional<AnimSetDef>  parse_anim(const nlohmann::json& j, const SkeletonDef& sk, std::string& err);
std::optional<SkeletonDef> load_skeleton_file(const std::string& path, std::string& err);
std::optional<AnimSetDef>  load_anim_file(const std::string& path, const SkeletonDef& sk, std::string& err);
```

- [ ] **Step 1: 写失败测试** `tests/animation/animation_test.cpp`
```cpp
#include <gtest/gtest.h>
#include "data/animation_defs.h"
using namespace roguelike; // 以仓库实际命名空间为准, 实施时核对 enemy_defs_test 的写法

TEST(AnimationDefs, ParsesMinimalSkeleton) {
    auto j = nlohmann::json::parse(R"({
      "pixels_per_unit":0.5,"anchor":[24,62],
      "bones":[{"name":"root"},{"name":"hips","parent":"root","y":-28}],
      "parts":[{"bone":"hips","file":"player_part_torso.png","pivot":[16,6]}]})");
    std::string err; auto sk = parse_skeleton(j, err);
    ASSERT_TRUE(sk.has_value()) << err;
    EXPECT_EQ(sk->bones.size(), 2u);
    EXPECT_EQ(sk->bones[1].parent, 0);
    EXPECT_FLOAT_EQ(sk->bones[1].y, -28.f);
    EXPECT_FLOAT_EQ(sk->pixels_per_unit, 0.5f);
}
TEST(AnimationDefs, RejectsUnknownBoneRef) {
    auto j = nlohmann::json::parse(R"({"bones":[{"name":"root"}],
      "parts":[{"bone":"ghost","file":"x.png"}]})");
    std::string err; EXPECT_FALSE(parse_skeleton(j, err).has_value());
    EXPECT_FALSE(err.empty());
}
TEST(AnimationDefs, ParsesAnimTracksAndKeys) {
    nlohmann::json sk_j = nlohmann::json::parse(R"({"bones":[{"name":"root"},{"name":"torso","parent":"root"}],"parts":[]})");
    std::string err; SkeletonDef sk = *parse_skeleton(sk_j, err);
    auto a = nlohmann::json::parse(R"({"animations":{"idle":{"loop":true,"dur":2.4,
      "tracks":[{"bone":"torso","keys":[{"t":0,"rot":0},{"t":2.4,"rot":0}]}]}}})");
    AnimSetDef set = *parse_anim(a, sk, err);
    ASSERT_TRUE(set.clips.count("idle"));
    EXPECT_TRUE(set.clips["idle"].loop);
    EXPECT_EQ(set.clips["idle"].tracks[0].bone, 1);
}
TEST(AnimationDefs, RejectsLoopMissingEndKey) {
    nlohmann::json sk_j = nlohmann::json::parse(R"({"bones":[{"name":"root"}],"parts":[]})");
    std::string err; SkeletonDef sk = *parse_skeleton(sk_j, err);
    auto a = nlohmann::json::parse(R"({"animations":{"idle":{"loop":true,"dur":2.4,
      "tracks":[{"bone":"root","keys":[{"t":0,"rot":0},{"t":2.0,"rot":0}]}]}}})");
    EXPECT_FALSE(parse_anim(a, sk, err).has_value());  // 末键 t=2.0 < dur=2.4
    EXPECT_FALSE(err.empty());
}
```

- [ ] **Step 2: 跑红** — `cmake -B build -DENABLE_TESTS=ON; cmake --build build --target animation_test` → 编译失败（缺 animation_defs.h）即"失败"状态确认
- [ ] **Step 3: 实现** `animation_defs.h/.cpp`（解析函数各 ≤40 行：`parse_skeleton` 拆 `_parse_bones/_parse_parts` 私有静态函数；bone name→index 用 map；`parse_anim` 校验末键覆盖 dur：loop 必须 `|末t - dur| < 1e-3` 否则 nullopt）。注册进主 CMakeLists src 列表（grep `enemy_defs.cpp` 所在行同列）
- [ ] **Step 4:** 两份真 JSON 按 spec §4 落盘（9 骨 7 件 + 4 clip；walk/attack/hit 关键帧先给保守值：walk 腿 ±22°/±18° 反相 dur 0.7；attack 后臂+武器 3 键挥砍 dur 0.36；hit 躯干 -8° 头 -12° dur 0.18）
- [ ] **Step 5: 占位件与预览工具**（python+PIL；占位件尺寸：头 20×20 / 躯干 24×28 / 臂 8×22 / 腿 10×28 / 武器 6×40，纯色+深 1 像素边，不同色区分；`anim_preview.py` 读两份 JSON 按 bind 合成本地坐标到 128×128 画布输出 `reports/anim_preview.png`）
- [ ] **Step 6: validator 新段**（world_validator.py 在 audio 段前插入：`ANIM_DIR="resources/animations"`；加载两 JSON；bone 引用合法 + `file` 存在于 `assets/sprites/`；失败 `errors.append`，风格照 `check_ref`）
- [ ] **Step 7: 全门禁** — build 0 error · ctest **63/63** · validator `All checks passed` · sim 逐字节（无逻辑改动预期必过）
- [ ] **Step 8: Code Review** — 重点：函数行数 / optional+err / JSON 键 snake_case / 测试无磁盘依赖（parse 纯内存）
- [ ] **Step 9: Commit** `git commit -m "feat(A5-T1): animation JSON schema + defs loaders + placeholder parts + validator (ctest 62->63)"`（中文消息用 `-F` UTF-8 无 BOM 文件，PS 直传会 mojibake）

---

## Task 2: 求值器 — skeleton_pose（纯数学, gtest 驱动）

**Files:**
- Create: `src/game/animation/skeleton_pose.h` / `.cpp`
- Modify: `CMakeLists.txt`（src 列表）
- Modify: `tests/animation/animation_test.cpp`（追加 pose 用例，仍 1 个可执行）

**Interfaces:**
- Consumes: T1 的 `SkeletonDef/AnimClipDef/KeyDef/TrackDef`
- Produces:
```cpp
// src/game/animation/skeleton_pose.h
struct WorldBone { float x, y, rot_deg, sx, sy; };   // 骨空间(未翻转)世界姿态, root 合成后
struct OverlayTf { float x = 0, y = 0, rot_deg = 0, sx = 1, sy = 1; }; // B3/重击外部叠加
// clip==nullptr 或空 tracks 时全部 = bind pose × overlay(root)
std::vector<WorldBone> compute_pose(const SkeletonDef& sk, const AnimClipDef* clip,
                                    float t, const OverlayTf& overlay = {});
```

- [ ] **Step 1: 追加失败测试**
```cpp
static SkeletonDef make_two_bone() {   // root + hips(0,-28), pixels 1:1
    SkeletonDef sk; sk.pixels_per_unit = 1.f;
    sk.bones = { {"root", -1, 0, 0}, {"hips", 0, 0, -28} };
    return sk;
}
static AnimClipDef one_track(int bone, KeyDef a, KeyDef b, float dur, bool loop) {
    AnimClipDef c; c.loop = loop; c.dur = dur; c.tracks = { { bone, {a, b} } };
    return c;
}
TEST(SkeletonPose, BindPoseWhenNoClip) {
    auto p = compute_pose(make_two_bone(), nullptr, 0.f);
    EXPECT_FLOAT_EQ(p[1].y, -28.f); EXPECT_FLOAT_EQ(p[1].rot_deg, 0.f);
}
TEST(SkeletonPose, LinearInterpMidKey) {
    auto clip = one_track(1, {0,0,0,0,1,1}, {2,0,0,20,1,1}, 2.f, false);
    auto p = compute_pose(make_two_bone(), &clip, 1.f);
    EXPECT_FLOAT_EQ(p[1].rot_deg, 10.f);
}
TEST(SkeletonPose, LoopWrapsTime) {
    auto clip = one_track(1, {0,0,0,0,1,1}, {2,0,0,20,1,1}, 2.f, true);
    auto a = compute_pose(make_two_bone(), &clip, 2.5f);
    EXPECT_FLOAT_EQ(a[1].rot_deg, 5.f);            // ≡ t=0.5
}
TEST(SkeletonPose, ParentRotationMovesChild) {
    SkeletonDef sk = make_two_bone();
    sk.bones[1] = {"hips", 0, 0, -10};
    auto clip = one_track(0, {0,0,0,90,1,1}, {1,0,0,90,1,1}, 1.f, false);
    auto p = compute_pose(sk, &clip, 0.f);
    EXPECT_NEAR(p[1].x, 10.f, 1e-4);               // 逆时针: (0,-10)→(10,0)
    EXPECT_NEAR(p[1].y, 0.f, 1e-4);
}
TEST(SkeletonPose, OverlayOnRootScalesChain) {
    OverlayTf ov; ov.sx = 2.f; ov.sy = 2.f;
    auto p = compute_pose(make_two_bone(), nullptr, 0.f, ov);
    EXPECT_FLOAT_EQ(p[1].y, -56.f);                 // bind 链整体过 overlay
    EXPECT_FLOAT_EQ(p[1].sy, 2.f);
}
```

- [ ] **Step 2: 跑红** → 编译失败确认
- [ ] **Step 3: 实现** `skeleton_pose.cpp`：`compute_pose` 拆 3 个文件内静态小函数各 ≤40 行：`_sample_local(clip, bone_idx, t)`（二分/线性找区间键→线性插值→无轨返回 bind）→ `_chain_compose(sk, locals)`（按 bones 声明序合成，前提 **父先于子声明**，parse 时校验否则 nullopt）→ `compute_pose`（root 先乘 overlay 再链合成 → overlay 缩放作用全链）。角度制 sinf/cosf，`Vector2` 不引入（无渲染依赖）
- [ ] **Step 4:** ctest 63/63（animation_test 内全绿）+ Release 0 error
- [ ] **Step 5: sim 逐字节**（本任务不接触游戏循环，预期零 diff，仍走形式）
- [ ] **Step 6: Code Review + Commit** `"feat(A5-T2): skeleton pose evaluator — interp + chain compose + overlay (pure math gtest)"`

---

## Task 3: 动画选择 + 2D 接入（占位件可玩）

**Files:**
- Create: `src/game/animation/avatar_animator.h` / `.cpp`（纯逻辑：状态→clip + 计时）
- Create: `src/game/animation/player_avatar.h` / `.cpp`（聚合：懒加载/贴图缓存/draw_2d）
- Modify: `CMakeLists.txt`、`src/game/scenes/game_scene.h`（成员 `std::unique_ptr<PlayerAvatar> _player_avatar;` **仅在渲染路径首次懒建**，无头 sim 不实例化）、`src/game/scenes/game_scene.cpp:2816`（玩家绘制单点分叉）
- Modify: `tests/animation/animation_test.cpp`（animator 用例，仍同可执行）

**Interfaces:**
- Consumes: T1 defs + T2 `compute_pose/OverlayTf`
- Produces:
```cpp
// avatar_animator.h — 时间由外部喂 dt 累计, 无 GetTime
struct AnimInput { bool attacking = false; bool hit_flash = false;
                   bool moving = false; float attack_recovery_ratio = 1.f; };
class AvatarAnimator {
public:
    void advance(float dt, const AnimInput& in);
    const std::string& current_name() const;                    // "idle|walk|attack|hit"
    float dur_scale() const;                                    // attack 时长缩放值
    const AnimClipDef* clip(const AnimSetDef& set) const;       // 名 → clip
    float time() const;                                         // 当前 clip 内时间
};
// player_avatar.h
class PlayerAvatar {
public:
    bool try_init(const std::string& base_dir, std::string& err);   // 全有才 true
    void update(float dt, const AnimInput& in, const OverlayTf& overlay);
    void draw_2d(const Player& pl);                                  // 7 件 + ghost
    bool active() const { return _active; }
};
```

- [ ] **Step 1: 失败测试（Animator 优先级与回落）**
```cpp
TEST(AvatarAnimator, AttackOverridesThenFallsBackToWalk) {
    AvatarAnimator an; AnimInput in; in.moving = true;
    an.advance(0.1f, in);
    EXPECT_EQ(an.current_name(), "walk");
    in.attacking = true;
    an.advance(0.05f, in);
    EXPECT_EQ(an.current_name(), "attack");
    EXPECT_FLOAT_EQ(an.time(), 0.05f);                       // 切换即从头计时
    in.attacking = false;
    for (int i = 0; i < 8; ++i) an.advance(0.05f, in);       // 0.4s > 0.36 播完
    EXPECT_EQ(an.current_name(), "walk");                    // 回落到移动态
}
TEST(AvatarAnimator, HitDuringAttackDoesNotInterrupt) {
    AvatarAnimator an; AnimInput in; in.attacking = true;
    an.advance(0.1f, in);
    in.hit_flash = true; an.advance(0.05f, in);
    EXPECT_EQ(an.current_name(), "attack");                  // attack > hit
    in.attacking = false; an.advance(0.5f, in);              // attack 播完
    EXPECT_EQ(an.current_name(), "hit");                     // hit 尚未触发过, 此时进入
}
TEST(AvatarAnimator, HitIsEdgeTriggered) {
    AvatarAnimator an; AnimInput in;
    in.hit_flash = true; an.advance(0.05f, in);
    for (int i = 0; i < 4; ++i) an.advance(0.05f, in);       // 0.2s ≥ 0.18 播完
    in.hit_flash = true; an.advance(0.05f, in);              // bool 持续为真 ≠ 重触发
    EXPECT_EQ(an.current_name(), "idle");                    // 只有 下降沿→上升沿 才进 hit
    in.hit_flash = false; an.advance(0.05f, in);
    in.hit_flash = true;  an.advance(0.01f, in);
    EXPECT_EQ(an.current_name(), "hit");                     // 新上升沿才触发
}
TEST(AvatarAnimator, HeavyScalesDuration) {
    AvatarAnimator an; AnimInput in;
    in.attacking = true; in.attack_recovery_ratio = 1.8f;
    an.advance(0.2f, in);
    EXPECT_FLOAT_EQ(an.dur_scale(), 1.8f);
    in.attacking = false;
    for (int i = 0; i < 8; ++i) an.advance(0.05f, in);       // 累计 0.6 < 0.36*1.8=0.648
    EXPECT_EQ(an.current_name(), "attack");                  // 仍在播 (基础时长早已"结束")
    for (int i = 0; i < 8; ++i) an.advance(0.05f, in);       // 0.98 > 0.648
    EXPECT_EQ(an.current_name(), "idle");
}
```
- [ ] **Step 2: 跑红**（编译失败）
- [ ] **Step 3: 实现 animator**（`_name/_t/_dur_scale/_prev_hit` 成员；优先级在 advance 顶部单函数 ≤30 行；一次性动作 t≥dur 后按 in 重选；hit 为**边沿触发**（`in.hit_flash && !_prev_hit` 才进）；attack 新上升沿 → 记 `_dur_scale = clamp(ratio, 1, 2)` 直至播完）
- [ ] **Step 4: 实现 player_avatar**
  - `try_init`：load 两 JSON + 逐 part `load_texture`，任一失败 → `_active=false` + 全部资源释放（**all-or-nothing**）
  - `draw_2d`：`compute_pose` → 逐 part：`k=pixels_per_unit`；骨空间→屏幕 `(x, -y)` 翻一次；`dst={feet + x*k, feet - y*k, ±tex.w*k, tex.h*k}`（sign=面朝），`origin={pivot.x*k, (tex.h-pivot.y)*k}`，`rot=-wb.rot_deg`（Y 翻转取反，实机校准符号）；ghost：`pl.dodge` 队列逐段重绘同 7 件，alpha=dodge ghost 老化值
  - overlay：调用方从 `pl.dodge.tilt_deg()/squash_scale()` + 重击 pose 数值构造 `OverlayTf` 传入
  - game_scene.cpp:2816 分叉：
```cpp
if (_player_avatar && _player_avatar->active()) _player_avatar->draw_2d(*gs.player);
else gs.player->draw_no_cam(camera);   // 原路径, 武器程序挥摆随静帧整体共存亡
```
  `_player_avatar` 首次渲染帧懒建（`if (!_player_avatar) { auto a = std::make_unique<PlayerAvatar>(); if (a->try_init(...)) ...; _player_avatar = std::move(a); }`——失败也缓存，不逐帧重试）
- [ ] **Step 5:** ctest 63/63 + Release 0 error
- [ ] **Step 6: 冒烟** — pre-A5 基线未存的话先补：`--sim 12 --sim-seed 3` 对 `pre_a5_report.json` 逐字节；`--autoshot 3600 --hidwin` 出图 PIL 判非黑 + 玩家在画（占位件大色块可像素级检出：统计画面中饱和色块面积 > 静帧基线）
- [ ] **Step 7: 实机自测点**（运行 `build/roguelike_cpp.exe`：走动腿摆/待机呼吸/攻击挥砍/受击后仰 + 删 player_anim.json 秒回退静帧 — 命令行 `--input-diag` 不需要）
- [ ] **Step 8: Code Review + Commit** `"feat(A5-T3): avatar animator + player 2D skeletal draw with all-or-nothing fallback"`

---

## Task 4: 3D 接入 — DrawBillboardPro spike + 分件 billboard + B3 tilt 复活

**Files:**
- Modify: `src/game/rendering3d/hd2d_renderer.h`（`HD2DDrawItem` 追加 `float rot_deg = 0; bool pro_mode = false;`，默认值=全存量行为不变）
- Modify: `src/game/rendering3d/hd2d_renderer.cpp:536-567`（`_draw_billboard` 加 pro 分支）
- Modify: `src/game/rendering3d/hd2d_scene_builder.cpp:401-454`（玩家块分叉：active → push 7 件 pro item；ghost 保留）
- Modify: `src/game/rendering3d/hd2d_shadow_caster.cpp`（pro item 是否进剪影——spike 结论定）

**Interfaces:** Consumes T3 `PlayerAvatar`（builder 经 GameScene 只读）+ T2 pose；Produces 渲染字段 `rot_deg/pro_mode`。

- [ ] **Step 1: spike（单件验证 Pro 语义，1 个 commit 内允许回退重做）**
  - raylib.h:1525 签名：`DrawBillboardPro(camera, texture, source, position, up, size, origin, rotation, tint)`
  - 实验 A：`up = {sinf(rot), cosf(rot), 0}`（屏幕内倾斜 = 世界 Y 轴绕视向 Z 旋转）、`rotation=0` → 截图看件是否"斜"
  - 实验 B：`up={0,1,0}`、`rotation=rot` → 对比语义（Pro 的 rotation 疑似绕 up 轴=水平自转，非屏幕倾斜）
  - 选定屏幕内倾斜的正确参数组合写入 `_draw_billboard` pro 分支；**两者都不成 → 退路：rlgl 手写顶点 quad（照抄 :500-533 墙面 `_wall_quad` 换 UV/顶点）**
  - 冒烟：`--hd2d --autoshot 3600 --hidwin` PIL 检出多色块（占位件不同色=可检出分件）
- [ ] **Step 2:** builder 玩家块：`pose_bones = avatar.current_world_bones()`（暴露只读接口）；逐 part 构造 item：`texture`、`tex_src` 全件、`world_pos={feet.x + x*k, y_world, feet.z}`、`size` 按件尺寸*k、`rot_deg`、`pro_mode=true`、`sort_y=entity.y`；Y 向上→世界高度换算在 builder 单点翻转
- [ ] **Step 3: B3 技术债复活**：翻滚时 `tilt_deg` 注入 root overlay → 玩家所有件 `rot_deg` 带倾斜（3D 端 tilt 从此可行）；squash 走既有 `scale_w/scale_h`（Pro 分支同样应用）
- [ ] **Step 4: 阴影/描边回归**：shadow_caster 对 pro item 的处理（剪影用整件纹理 → 天然支持，实施时验证 shader 采样同一 `tex_src`）；描边逐件后视觉是否毛刺 → 允许 pro 件 `outline=false`（占位件期先关，T5 真素材再开评估）
- [ ] **Step 5:** ctest 63/63 · Release · **sim 逐字节**（`--sim` 不初始化 3D 预期零 diff）· 3D hidwin 出图
- [ ] **Step 6: Code Review + Commit** `"feat(A5-T4): 3D skeletal billboards via DrawBillboardPro + B3 3D-tilt debt revived"`

---

## Task 5: 真素材 + 调优 + 文档收口

**Files:**
- Replace: `assets/sprites/player_part_*.png`（占位件 → AI 真件）
- Modify: `resources/animations/*.json`（anchor/pivot/pixels_per_unit 校准值）
- Modify: `README.md`（版本行 v1.7-A5）、`CHANGELOG.md`（v1.7-A5 条目）、`docs/V1_6_ROADMAP.md`（A5 → ✅ v1.7-A5 + 拍板问题 #1 结案注记）

- [ ] **Step 1: AI 分件生图** — 使用本地 `modelscope-image-gen` 技能；**同一风格前缀**逐件生成（例：`pixel art RPG hero side view, only <part>, flat colors, transparent/simple background, 64x64`）；SD 系不产透明底 → 白底生成后 `tools/part_cutout.py`（新建：PIL 白→alpha + 边缘 1px 收缩 + 最近邻缩到契约尺寸）
- [ ] **Step 2:** `anim_preview.py` 合成预览图 → 目测件位/锚点 → 改 JSON pivot/anchor（**只改数据不改代码**是本任务验收线）
- [ ] **Step 3:** 2D/3D hidwin 双端出图 + PIL 色块检出（真素材色板与占位件不同，更新判定脚本预期）
- [ ] **Step 4:** 全量门禁：Release · ctest 63/63 · validator · sim 逐字节 · 四件套实机录屏级目验（用户）
- [ ] **Step 5:** CHANGELOG/README/roadmap（中文文件改动一律 python 脚本精确替换，禁 PS 回写）
- [ ] **Step 6:** 桌面包全量同步（规则 11：目录镜像 + 根文件 + **exe 到根目录**，保留 saves/lnk）
- [ ] **Step 7: Code Review 全 diff 自查** → Commit → 向用户移交实机验收；**tag 待验收通过后另行打**

---

## 验收清单（整个 A5 v1）

- [ ] 四件套 2D/3D 均自然生效；B3 翻滚/重击手感与 v1.7.0 无差异（3D tilt 为纯增益）
- [ ] 删 `resources/animations/player_anim.json` → 静默整条回退静帧，像素级 = v1.7.0
- [ ] ctest 63/63 · validator All checks passed · sim 12×seed3 逐字节 = `pre_a5_report.json`
- [ ] 新增 JSON 全部 snake_case · 函数 ≤40 行 · 无 new · Player 实体零新成员
