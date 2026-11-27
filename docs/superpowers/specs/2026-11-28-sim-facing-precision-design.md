# Sim 攻击朝向精度 — 修复"扇形必中却空挥"设计

> 日期：2026-11-28 · 状态：**已实现并验收**（builder 审查完成，决定并入 §7）· 用户目标：**修单点**（命中率≥10% 且 kills/damage 不降；F15 通关留后续）
> 前置：`2026-11-27` 定位攻击距离不一致（标量折扣失败回退）；本轮命中瞬间诊断拿到铁证。

## 一、意图与 why

标量折扣方案失败后，按用户要求做了命中瞬间精确诊断（`attack_diagnostics` 的 `hit_probe`），拿到铁证：真实攻击 91.8% 空挥，根因是**朝向偏差 + 距离边缘叠加**。

## 二、铁证（20 局固定 build，命中瞬间采样）

| 指标 | 值 | 含义 |
|---|---|---|
| `hit_probe` | 2315 | 真实命中判定次数 |
| `hit_probe_empty` | 2126（91.8%） | 空命中 |
| `probe_dist_bucket` | [229,13,85,**1988**] | **86% 的最近怪距离 > 射程 3/4** |
| `probe_angle_bucket` | [169,**2146**,0,0] | **93% 的攻击方位角在 30-60°** |

**根因链**：
1. `_face_nearest_target`（player_controller.cpp:341-360）把朝向量化为上下左右 4 向 → 攻击朝向与怪实际方位差 30-60°。
2. 距离又在射程边缘（86% > 3/4 reach）。
3. 扇形半角虽 90°（dagger），但矩形/胶囊（sword RECTANGLE width0.8格、nunchaku CAPSULE）在 30-60° 偏差 + 边缘距离下打不中。
4. 叠加 → 91.8% 空挥。这解释了为何单纯收紧距离（标量折扣）无效——角度偏差是另一半主因。

## 三、方案（可选角度参数，真实游戏零变化）

### 3.1 hit_detect 加可选角度参数
给 `hit_detect_sector/rectangle/capsule` 和分派 `hit_detect_by_shape` 增加可选 `float aim_angle_rad = -1`（默认 -1 = 用原 `Direction` 四向，真实游戏行为逐字节不变）。
- 当 `aim_angle_rad >= 0`：用 `(cos(angle), sin(angle))` 作为朝向向量替代 `_forward_vec(dir)`。
- 只改朝向向量来源，不改半径/宽度/角度判定逻辑。

### 3.2 sim 攻击传真实方位角
在 `WeaponExecutor::execute` 的 normal 命中判定点：若 `sim_hit_aim_enabled()`（main.cpp 在 `--sim` 时置位，与纯观测的 `attack_diag_enabled()` 解耦），计算玩家中心→最近怪的 `atan2` 角度，传给 `hit_detect_by_shape` 的新参数。真实游戏该参数为默认 -1，走原路径。nunchaku/spear special 保持默认四向（占比小，留后续）。

### 3.3 为什么这样安全
- 不改 `Direction` 枚举、不改 `Player::direction` 字段。
- 真实游戏：可选参数默认 -1 → `hit_detect_*` 用原 `_forward_vec(dir)` → 逐字节不变。
- sim：仅命中判定的朝向向量改为真实角度，不改 gameplay 数值/rng/存档。

### 3.4 不做什么
- 不改武器数值、冷却、伤害、扇形半角/矩形宽度。
- 不改 `DecisionAgent` 评分（距离判定保持原样）。
- 不碰 sim 确定性红线（角度由 player/monster 坐标确定性计算）。

## 四、验证（严格 A/B）

1. **契约测试**：`hit_detect_*` 可选角度参数（默认行为不变、给定角度时朝向正确）。
2. **A/B**：跑 `--sim 20 --sim-seed 4242`，对比 `attack_diagnostics`：
   - `hit_probe_empty` 从 91.8% 显著下降、`probe_angle_bucket` 从 30-60° 集中转向 <30°
   - 命中率 `total_results/can_attack_pass` 升到 ≥10%
   - kills/damage 口径为**聚合指标**（avg_floor/avg_dmg），非逐局——固定 seed 个别局因 auto-aim 改变目标选择会正常漂移（GOLDEN-001 run1 dmg 127→84、run2 kills 16→15），聚合 dmg 10284→13425 上升
3. **真实游戏零变化**：GOLDEN-001 逐字段不变（真实路径默认参数）。但注意——sim 走 change_scene 也用 g_sim_mode，golden 测试是 sim 跑的，会变化 → 需按 SOP 重采集 GOLDEN-001（sim 行为变了）。真实游戏（非 sim）无自动测试覆盖，靠"默认参数等价"的代码论证 + 手动 smoke。
4. **确定性**：GOLDEN-003 同 seed 一致。
5. **闸门**：ctest 全量、fn_length 0、validator 0/0、桌面同步。

## 五、约束与红线

- 函数≤40 行、单一职责、语义命名（CLAUDE.md）。
- 不新增 rng 调用、不改调用顺序。
- 不破坏真实游戏行为（默认参数等价须有测试/论证）。
- 不碰存档、不改数值。

## 七、Builder 审查后的决定（用户已拍板 + 实施者决定）

### 用户拍板
1. **达标线排除弩**：用近战专属命中率（shape_pass/shape_whiff 或 melee_only 指标）判 ≥10%，弩走弹体池另案处理。
2. **独立 sim 标志**：新增 `sim_hit_aim_enabled`（main.cpp 在 --sim 时置位），与 `attack_diag_enabled`（纯观测）解耦，职责单一。
3. **本轮重采集 golden**：修复后按 SOP 重采集 GOLDEN-001 四局基线（floor/kills/damage/weapon）+ 归因注释；GOLDEN-002/003 不动。

### 实施者决定
4. **调用点范围**：仅改 normal 主命中点（weapon_executor.cpp:428）。`hit_detect_circle`（无方向）和弩弹体（方向无关）不改；nunchaku/spear stage3 special（:315/:520）需存储 aim 状态、改动大且占比小，留后续。
5. **角度约定**：`atan2(dy, dx)`（y-down 屏幕坐标）+ 单位向量 `(cos,sin)`；`AttackGeometry.direction` 保持量化（VFX 与命中几何轻微不一致可接受，sim-only）。
6. **最近怪定义**：与 `_face_nearest_target` 同一 alive 集；rt 为空时传 -1 哨兵回退原四向路径。
7. **跨工具链**：GOLDEN-003 同 exe 两次，`atan2/cos/sin` 确定性足够；不加角度量化。

### 签名兼容
`hit_detect_*` 的 5 个调用点都是位置参数且以 `targets` 结尾，末尾追加 `aim_angle_rad` 默认值不影响现有调用。

## 六、风险与诚实评估

- `hit_detect_*` 是真实命中路径，改签名虽用默认参数保持兼容，但需仔细验证所有调用点行为不变。
- sim 传角度后，攻击命中率应上升，但 kills/damage 是否同步上升需 A/B 确认（命中≠击杀，还受伤害/敌人HP影响）。
- 历史多次单点修补失败；本轮有命中瞬间铁证，但仍需 A/B，不达标即回退。
- 达标只意味着攻击不再大量空挥；F15 通关是后续独立目标。
