# A5 角色骨骼动画 — 设计 spec (v1: 玩家四件套)

日期: 2026-09-17 · 状态: 待用户审阅
前置: v1.7.0 (B3 翻滚已验收发布) · 路线: docs/V1_6_ROADMAP.md Line A · A5

## 1. 目标与非目标

**目标**: 玩家角色获得"运动质感" — 待机呼吸 / 走路 / 攻击挥砍 / 受击后仰 四个动作,
2D 与 HD-2D 3D 双端同步生效, 自研轻量 2.5D 骨骼 (spine 式, 但不引入 spine 运行时)。

**非目标 (v1 明确排除)**:
- 不做小怪/Boss 动画 (机制数据驱动, 后续批次纯加数据+美术)
- 不做 A6 演出镜头 (本 spec 只保证动画可被外部指定播放)
- 不做 4 方向素材 (右侧视 + flip_x 镜像, 与现状一致)
- 不改任何战斗逻辑/手感数值 (sim 逐字节零变化是构造性保证)

## 2. 决策记录 (brainstorming 拍板)

| 决策点 | 结论 | 理由 |
|:--|:--|:--|
| 核心目的 | 运动质感 (非读招/非技术储备) | 用户选定 |
| 技术路线 | **A 分件骨骼** (否决 B 序列帧 / C 纯程序) | 动作间平滑过渡天然适合骨骼; AI 逐件生成无帧间一致性问题; 扩展 +1 角色 = +7 件美术+1 份数据 |
| 美术产能 | 可新增 1 角色份 | 用户选定; 引擎先用程序化占位件, AI 真素材 T5 换 |
| 范围 | 仅玩家 | 用户选定 |
| 动作集 | 基础四件套 | 用户选定; 重击复用 attack 轨时间缩放 |
| spine 格式 | 不采用 spine 导出格式, 自定最小 JSON | 项目 = 数据驱动 JSON 哲学; 避免外部编辑器工具链 |

## 3. 素材契约

- 7 件分体: 头 / 躯干 / 前臂 / 后臂 / 武器 / 前腿 / 后腿
- 统一**右侧视**姿态、透明背景 PNG、画布 64px 高, 存 `assets/sprites/`
  命名 `player_part_<部位>.png`
- 渲染近邻缩放到目标尺寸 (pixels_per_unit 在 skeleton.json 调, 默认 0.5)
- 朝向 = 仓库既有 flip_x 负宽源矩形惯例 → 素材量减半
- 占位件: T1-T4 期间用 `tools/` python PIL 生成纯色分件, 保证管线独立可测

## 4. 数据 schema (resources/animations/)

### player_skeleton.json
```json
{ "pixels_per_unit": 0.5,
  "anchor": [24, 62],
  "bones": [
    { "name": "root",      "parent": null },
    { "name": "hips",      "parent": "root",  "y": -28 },
    { "name": "torso",     "parent": "hips",  "y": 8 },
    { "name": "head",      "parent": "torso", "y": 16 },
    { "name": "arm_back",  "parent": "torso", "x": -3, "y": 13 },
    { "name": "weapon",    "parent": "arm_back", "y": -12 },
    { "name": "arm_front", "parent": "torso", "x": 4, "y": 13 },
    { "name": "leg_front", "parent": "hips", "x": 3 },
    { "name": "leg_back",  "parent": "hips", "x": -3 }
  ],
  "parts": [
    { "bone": "leg_back",  "file": "player_part_leg.png",   "pivot": [8, 30] },
    { "bone": "arm_back",  "file": "player_part_arm.png",   "pivot": [8, 2] },
    { "bone": "weapon",    "file": "player_part_weapon.png","pivot": [4, 28] },
    { "bone": "torso",     "file": "player_part_torso.png", "pivot": [16, 6] },
    { "bone": "head",      "file": "player_part_head.png",  "pivot": [16, 2] },
    { "bone": "arm_front", "file": "player_part_arm.png",   "pivot": [8, 2] },
    { "bone": "leg_front", "file": "player_part_leg.png",   "pivot": [8, 30] }
  ] }
```
- 骨骼局部坐标 **Y 向上**, 仅渲染层翻转一次; 未打键的骨 = bind pose
- parts 数组序 = 绘制序 (后→前); pivot = 贴图内锚点 (左下为原点的像素坐标)

### player_anim.json
- 4 个动作: `idle` (loop 2.4s) / `walk` (loop 0.7s, 腿反相 ±22°+手臂反摆+身体 bob)
  / `attack` (0.36s, 对齐武器 0.5s 攻击间隔; 重击按 recovery 比例拉长时间缩放)
  / `hit` (0.18s, 后仰+头抖)
- 键字段最简: `t / x / y / rot / sx / sy`, 线性插值, 无缓动曲线 (YAGNI)
- tracks 逐骨; 一个动作可多 track
- 重击复用 attack 轨: `播放时长 = anim_dur × clamp(实际recovery / 0.36, 1.0, 2.0)`
- gtest 归属: T1/T2 用例同注册进 `animation_test` 一个可执行 (ctest 62→63, 非 64)

### 校验 (world_validator.py 新段)
- parts[].bone ∈ bones; tracks[].bone ∈ bones (含所有动作)
- file 存在于 assets/sprites/ (占位件期跳过, 提供 --skip-art 开关)
- dur > 0, 首末键 t 覆盖 [0, dur] 或 loop 补值规则明确

## 5. 引擎层 (4 新文件, 全表现层)

| 文件 | 职责 | 依赖 |
|:--|:--|:--|
| `src/data/animation_defs.h/.cpp` | 两份 JSON 加载, 返回 std::optional | nlohmann/json |
| `src/game/animation/skeleton_pose.h/.cpp` | 纯数学: 插值 + 父链合成 (LocalTf {pos, rot_deg, scale}), 手写合成 ~15 行 | 无渲染依赖, gtest |
| `src/game/animation/avatar_animator.h/.cpp` | 动作选择 + 计时 + overlay_tf | 只读表现状态 |

**animator 规则**:
- 优先级 `attack > hit > walk > idle`; attack 触发读 `weapon.is_busy()`,
  hit 读受击表现计时余量, walk 读移动轴 (均为现有字段, 零新增逻辑)
- 一次性动作播完自动回落; 循环动作时间无缝衔接
- **overlay_tf**: root 叠加外部变换 (B3 翻滚 tilt/squash、重击前倾作为渲染侧
  现有数值注入) → 已验收手感零损失
- 时间 t 由调用方传入, 内部禁止 GetTime()

**冲突裁决**: 骨骼模式下武器件由 weapon bone 驱动, 程序化武器挥摆
(player.cpp:162-180) **禁用**; 静帧回退模式保留原逻辑 (每帧二选一单点分叉)。

## 6. 渲染层

- **2D** (`Player::draw_no_cam` 新分支): 逐件 `DrawTexturePro`
  (原生旋转+缩放+origin 锚点), 7 次调用; tint 受击白闪继续走 VFXServer
  世界坐标 flash, 不碰件色
- **3D** (`hd2d_scene_builder` + `hd2d_renderer`): 用 raylib 5.0
  `DrawBillboardPro(camera, tex, src, position, up, size, origin, rotation, tint)`
  (仓库现 0 使用) — up 轴 + rotation 即骨变换落点; T4 先 spike 验证语义,
  失败退回 rlgl 手写 quad (墙面先例 hd2d_renderer.cpp:500-533)
- 附带收益: B3 遗留 "3D 端不做倾斜" 技术债由 Pro 旋转参数复活
- quad 预算: 玩家 7 件 × (阴影+描边+主) ≈ 28/帧, 可忽略; 阴影 pass 逐件
  (静帧图为整块, 分件后剪影更真)
- HD2DDrawItem 需扩展: 逐件提交 (新 kind 或 billboard 列表化), 设计细节 T4 spike 后定

## 7. 降级链 (all-or-nothing)

任一失败 → **整条静默回退现静帧路径** (像素级与 v1.7.0 一致):
JSON 缺失 / 解析失败 / 任一贴图加载失败 / 校验不通过。
判定一次性 (首次构建时), 不做逐帧抖动。

## 8. sim 隔离

- animator/pose 实例挂在表现侧容器 (GameScene 渲染路径构造, VFXServer 同款),
  Player 实体零新成员
- 无头 sim 根本不初始化渲染 → 不实例化 → **逐字节零变化由构造保证**
- 常规红线复验仍全跑 (不碰运气)

## 9. 任务切分与验收

| 任务 | 内容 | 验收 |
|:--|:--|:--|
| T1 | animation_defs + 两份 JSON 初稿 (占位件) | gtest 加载可选/失败路径, validator 新段, ctest 62→63 |
| T2 | skeleton_pose 求值器 | gtest: 插值/链合成/未打键=bind pose 确定性用例 |
| T3 | avatar_animator + 2D 接入 | hidwin 出图 4 动作, sim 逐字节 |
| T4 | DrawBillboardPro spike + 3D 接入 | 出图 + B3 3D tilt 复活验证, sim 逐字节 |
| T5 | AI 分件生图 (modelscope-image-gen) + 换件 + 调优 + 文档/桌面包/tag | 全量门禁 + 实机四件套目验 |

**实机验收标准**: 四件套自然; B3 翻滚/重击手感与 v1.7.0 无差异; 删骨骼 JSON
立即回退静帧无报错。
**工具**: `tools/anim_preview.py` — PIL 按 bind pose 合成分件输出预览图,
不启动游戏即可查件位/锚点。

## 10. 风险

| 风险 | 缓解 |
|:--|:--|
| DrawBillboardPro up 语义不明 | T4 独立 spike; 失败退 rlgl 手写 quad 先例 |
| AI 分件画风漂移 | 占位件保底; 同一参考描述前缀逐件生成; 单件重生成成本低 |
| 64px 件缩放到 16px 像素质感 | anim_preview + 实机定 pixels_per_unit; 不行整链降画布 32 |
| 双路径共存维护 | 每帧二选一单点分叉, 代码集中 draw 入口 |
| 函数 ≤40 行红线 (求值器易膨胀) | 插值/合成/取轨拆三个私有方法 |

## 11. 通用红线遵守

零新增逻辑字段 · 零 RNG · dt 定步长 · 无 GetTime 入逻辑 · 不引第三方库 ·
CMakeLists 只加新源文件 · 桌面同步规则 11 · 每任务 Code Review + 全量门禁。
