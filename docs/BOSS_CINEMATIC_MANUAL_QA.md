# Boss 登场运镜 / 击杀顿帧 —— 手动验收清单

> 背景：G25-H 拆分后，BOSS_CINEMATIC / HitStop / 运镜三段 gate 一直在 sim 模式下被
> 结构性跳过（`sim` 只到 F1-2，不触发 F15 镜像 Boss），确定性测试覆盖不到。
> 2026-11-19 已为三段 gate 抽出可测纯函数 `GameScene::presentation_gate_allowed`
>（`camera_test.cpp` 的 `GameSceneGates.PresentationGateAllowedTruthTable` 锁定 sim-mode 跳过语义），
> 并把 gate 全部改走该函数（sim 报告 hash 逐字节未变，确定性零漂移）。
> 但**运镜观感本身仍需肉眼确认**——本清单即为该项验收依据。

## 如何到达 Boss 战

- 桌面包 `roguelike_cpp.exe`，进入 3D 模式。
- 最快路径：F5 / F10 / F15 是 Boss 层（`resources/floor_config.json`）。
  可用启动参数 `--goto-floor 5` 直接进第 5 层（见 `main.cpp` `_parse_evidence_flags`），
  `--autocontinue` 自动过标题页。

## 验收项（逐项勾选）

### 1. Boss 登场运镜（A6-T4 / `_begin_boss_battle` → `_camera_director.enter_boss_war`）
- [ ] 进入 Boss 房触发登场时，镜头出现 zoom-in（FOV scale ≈0.75，持续 ≈1.5s）。
- [ ] 焦点从玩家平滑移到 Boss（相机语言状态机 BOSS_WAR）。
- [ ] 登场结束后 FOV 平滑回落（≈0.8s 回到 1.0）。
- [ ] 镜头聚焦后 Boss 所在房间无"视野虚空"（`mark_boss_room` 预渲染生效）。

### 2. 击杀顿帧（A6-T2 / `_hit_stop_gate`）
- [ ] 击杀 Boss 瞬间有短暂停帧（kill_stun.duration ≈0.1s，shake amplitude ≈3.6）。
- [ ] 顿帧期间只推表现层、世界模拟暂停（打击感），之后正常恢复。
- [ ] 顿帧不导致 `game_time` 异常变慢 / 900s 超时被稀释（Q3.10 红线）。

### 3. sim 模式不受影响（确定性红线）
- [ ] `--sim 2 --sim-seed 1234` 报告 SHA256 = `F7AFD95FFF150CC78A45F7D46115BB4DF1CBFFB28E27B69F84E229087BB27470`
      （与基线一致，证明运镜/顿帧 gate 在 sim 下确实跳过）。

## 若不通过

- 运镜相关数值集中在 `resources/camera/boss_camera.json`（组件单测 `camera_test.cpp` 已钉死解析值）。
- 三段 gate 判定在 `game_scene.cpp` 的 `_begin_boss_battle` / `_hit_stop_gate` / `_tick_camera_director`，
  统一走 `GameScene::presentation_gate_allowed`。
- 状态机在 `src/game/director/camera_director.*`（`camera_test.cpp:108-300` 已覆盖进出 BOSS_WAR / KILL_STUN）。
