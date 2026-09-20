# A6 摄像机语言 v1 设计 spec

> 前置: A5 骨骼动画已完成 (玩家+8怪物), v1.7-B3 翻滚已交付
> 日期: 2026-09-20 · 状态: 待审阅

## 1. 目标与非目标

**目标**: 为 Boss 战和关键击杀增加摄像机运镜，提升演出张力。
- Boss 战：摄像机推拉/环绕，聚焦战斗
- 击杀顿帧：hit-stop 微暂停 + 屏幕震动
- 普通战斗：轻量跟随，不打断节奏

**非目标 (v1 明确排除)**:
- 不做自由摄像机/切换视角（保持固定 45° 俯视）
- 不做过场动画/镜头切换（保持游戏内实时）
- 不做 NPC 对话镜头（无 NPC 对话系统）
- 不改任何战斗逻辑/手感（sim 逐字节零变化）

## 2. 决策记录

| 决策点 | 结论 | 理由 |
|:--|:--|:--|
| 范围 | 仅 Boss 战 + 击杀顿帧 | 先做高影响场景，普通战斗不打扰 |
| 运镜方式 | 相机 focus 偏移 + FOV 缩放 | raylib 5.0 无独立相机动画系统，手动插值 |
| 顿帧时长 | 0.08s (约 5 帧 @60fps) | 短到不卡节奏，长到能感知 |
| 震动幅度 | 2-4px 随机抖动 | 和现有 hit shake 同源 |
| 触发条件 | Boss 出场/被击中/死亡 | 数据驱动 bosses.json 已有 boss 标记 |
| 降级策略 | 运镜失败静默回退当前相机 | 不崩溃，不打断游戏 |

## 3. 数据契约

### boss_camera.json (新增)
```json
{
  "boss_war": {
    "zoom_in": { "fov_scale": 0.75, "duration": 1.5 },   // Boss 出场时拉近
    "zoom_out": { "fov_scale": 1.0, "duration": 0.8 },    // 玩家靠近时拉远
    "lerp_speed": 2.0                                      // 插值速度
  },
  "kill_stun": {
    "duration": 0.08,                                      // hit-stop 时长
    "shake_amplitude": 3.0,                                // 震动幅度
    "shake_frequency": 20.0                                // 震动频率
  }
}
```

### 现有字段复用 (不改逻辑)
- `bosses.json`: Boss 标识（已有）
- `GameScene::player->entity.rect`: 玩家位置（已有）
- `HD2DRenderer::_camera_focus`: 相机焦点（已有）
- `GameScene::shake_intensity`: 震动强度（已有）

## 4. 引擎层 (3 新文件)

| 文件 | 职责 | 依赖 |
|:--|:--|:--|
| `src/data/camera_defs.h/.cpp` | JSON 加载，返回 std::optional | nlohmann/json |
| `src/game/director/camera_director.h/.cpp` | 相机状态机 + 插值 | 只读表现状态 |
| `src/game/systems/hit_stop.h/.cpp` | hit-stop 计时器 | 无渲染依赖 |

**CameraDirector 规则**:
- 状态: NORMAL / BOSS_WAR / KILL_STUN
- 优先: KILL_STUN > BOSS_WAR > NORMAL
- KILL_STUN 期间游戏暂停 (dt=0)，但相机震动继续
- BOSS_WAR 期间相机 focus 在 Boss 和玩家之间插值
- FOV 缩放通过 `camera.fovy` 修改，非屏幕缩放

**HitStop 规则**:
- 触发: 玩家击中 Boss / Boss 死亡 / 玩家死亡
- 计时: wall clock (非 game time)，避免累积
- 期间: `GameScene::update()` 跳过，但 `HD2DRenderer::render_frame()` 继续
- 最长: 0.15s (防止卡死)

## 5. 渲染层

- **2D**: `GameScene::_draw_camera()` 读取 CameraDirector 状态，修改相机变换
- **3D**: `HD2DRenderer::_camera_focus` 和 `_camera.fovy` 由 CameraDirector 写入
- 震动: 复用现有 `shake_intensity`，HitStop 期间额外叠加

## 6. 降级与隔离

- JSON 缺失/解析失败 → CameraDirector 不初始化，相机行为不变
- 运镜插值失败 → 每帧检查，失败回退 NORMAL
- 无头 sim 不初始化渲染 → 不实例化 CameraDirector → **逐字节零变化**

## 7. 任务切分

| 任务 | 内容 | 验收 |
|:--|:--|:--|
| T1 | camera_defs + JSON + 加载器 + validator | gtest 加载/失败路径 |
| T2 | HitStop 计时器 + 集成到 GameScene | gtest 触发/计时/上限 |
| T3 | CameraDirector 状态机 + 插值 | gtest 状态转换/插值 |
| T4 | 2D/3D 渲染接入 + Boss 触发 | 实机 Boss 战运镜 |
| T5 | 击杀顿帧接入 + 震动 | 实机击杀顿帧 |
| T6 | 文档/桌面包/tag | 全量门禁 |

**实机验收标准**: Boss 出场摄像机拉近，击杀有顿帧，普通战斗不受影响。

## 8. 风险

| 风险 | 缓解 |
|:--|:--|
| FOV 缩放影响 UI 位置 | 只改 fovy，不改视口；UI 走屏幕坐标 |
| HitStop 期间输入丢失 | 输入队列缓存，恢复后消费 |
| 相机插值抖动 | lerp_speed 可配置，失败回退 |
| 性能下降 | 插值计算 <1μs，可忽略 |

## 9. 通用红线

零新增逻辑字段 · 零 RNG · dt 定步长 · 不引第三方库 ·
CMakeLists 只加新源文件 · 桌面同步规则 11 · 每任务 Code Review + 全量门禁
