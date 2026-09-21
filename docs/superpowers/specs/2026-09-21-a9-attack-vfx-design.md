# A9 — 攻击特效优化 v1 (2026-09-21)

## 目标

优化攻击特效（武器三连击 + 技能 + 元素效果），在 HD-2D 3D 模式下提升视觉冲击和打击感。

**成功标准：**
- 视觉冲击：每次攻击都有明显特效反馈（光效/粒子/形状）
- 打击感：震屏 + 顿帧 + 飘字 + 闪白组合
- 差异化：5 类武器 × 3 段连击各有独特视觉效果
- 性能优先：60FPS 稳定，不增加渲染负担

## 架构

保留现有 `Effect` 结构，逐步增强各组件：

1. **粒子系统** - 新增 `ParticleSystem` 类（发射器 + 粒子池）
2. **武器特效** - 扩展 `Effect.kind` 类型（`slash_1/2/3`, `pierce_1/2/3` 等）
3. **打击感组合** - 集成现有 `HitStop` + 震屏 + 飘字 + 闪白
4. **Shader 效果** - 新增 `VFXShader`（发光/模糊/扭曲）

**数据流不变** - `Effect` 每帧更新，`ParticleSystem` 独立运行

**渲染顺序** - 游戏场景 → 粒子层 → Effect 层 → UI

## 组件

### 1. ParticleSystem 类

**职责** - 管理粒子发射器和粒子池

**发射器：**
- 位置：世界坐标
- 速度：随机范围 + 方向
- 颜色：渐变（起始→结束）
- 生命周期：0.5-2.0 秒
- 发射率：每秒 N 个粒子

**粒子池：**
- 最大 512 粒子
- 对象池复用（避免频繁分配）
- 重力影响（可选）

**绘制：**
- 精灵/圆点混合
- 支持旋转
- alpha 淡出

### 2. 武器特效扩展

**5 类武器 × 3 段连击：**

| 武器 | 1 段 | 2 段 | 3 段 |
|------|------|------|------|
| 剑（扇形斩） | `slash_arc_1` 单弧 | `slash_arc_2` 双弧 | `slash_arc_3` 三连弧 + 粒子拖尾 |
| 矛（穿透） | `pierce_beam_1` 单光束 | `pierce_beam_2` 分裂光束 | `pierce_beam_3` 贯穿光束 + 命中火花 |
| 双截棍（追踪） | `whip_arc_1` 单段轨迹 | `whip_arc_2` 双段轨迹 | `whip_arc_3` 多段轨迹 + 残影 |
| 连弩（弹幕） | `bolt_spread_1` 单箭 | `bolt_spread_2` 双箭 | `bolt_spread_3` 多箭 + 爆炸 |
| 重锤（重击） | `smash_impact_1` 小冲击 | `smash_impact_2` 中冲击 | `smash_impact_3` 大冲击 + 碎石 |

**Effect.kind 扩展：**
- 现有：`pulse/ring`, `spark`, `bolt`, `flash`, `smoke`, `shield_ring`, `slash_arc`, `cone`
- 新增：`slash_arc_1/2/3`, `pierce_beam_1/2/3`, `whip_arc_1/2/3`, `bolt_spread_1/2/3`, `smash_impact_1/2/3`

### 3. 打击感组合

**震屏（ScreenShake）：**
- 现有，增强强度分级
- 普通怪：amplitude 4.0, frequency 20Hz
- 精英怪：amplitude 8.0, frequency 25Hz
- Boss：amplitude 16.0, frequency 30Hz

**顿帧（HitStop）：**
- 现有，0.08-0.12s
- 普通命中：0.08s
- 暴击：0.10s
- Boss 战击杀：0.12s

**飘字（DamageFloat）：**
- 现有，增加颜色分级
- 物理伤害：白色
- 魔法伤害：蓝色
- 元素伤害：元素颜色
- 暴击：金色 + 放大

**闪白（HitFlash）：**
- 新增，命中实体闪白 0.1s
- 白色叠加，alpha 100

### 4. Shader 效果

**VFXShader 类：**
- 发光（bloom）：Boss 战/终极技能
- 模糊（motion blur）：快速移动
- 扭曲（screen warp）：领域展开/时停

**启用时机：**
- Boss 战：发光 + 模糊
- 终极技能：全部效果
- 普通战斗：禁用（性能优先）

## 数据流

**保持不变，每帧从现有系统读取：**

1. **Effect 系统** - `std::vector<Effect>` 每帧更新 `elapsed`，过期移除
2. **ParticleSystem** - 独立更新，不依赖游戏状态
3. **打击感** - `HitStop`/`ScreenShake`/`DamageFloat` 每帧更新
4. **Shader** - 仅 Boss 战/终极技能时启用

**Effect 创建时机：**
- 武器命中：`WeaponExecutor::on_hit()` 创建 `Effect` + 触发粒子
- 技能释放：`SkillExecutor::on_cast()` 创建 `Effect` + 触发粒子
- 怪物死亡：`on_monster_killed()` 触发粒子 + 飘字

**不维护第二套状态** - 所有数据每帧实时读取

## 错误处理

1. **粒子池耗尽** - 丢弃新粒子，不崩溃
2. **Shader 未加载** - fallback 到程序绘制
3. **Effect 过期** - `elapsed > duration` 时移除
4. **边界检查** - 粒子位置不越界，Effect 坐标有效
5. **空指针保护** - `if (!effect) return;`

**降级策略** - 资源未加载时使用程序绘制替代

**调试输出** - 关键路径添加 `LOG_DEBUG`，不影响性能

## 测试策略

### 单元测试

每个组件独立测试：
- ParticleSystem: 粒子生成/更新/生命周期
- WeaponVFX: 5 类武器 × 3 段连击类型映射
- HitFlash: 闪白触发/持续时间
- VFXShader: 启用/禁用 fallback

### 集成测试

`ctest 66/66` 保持不变：
- 不破坏现有功能
- 新增测试用例验证新特效

### 视觉验收

实机运行检查：
- 5 类武器 × 3 段连击特效差异化
- 打击感组合（震屏 + 顿帧 + 飘字 + 闪白）
- 粒子系统性能（60FPS 稳定）

### 性能测试

60FPS 稳定：
- 粒子池最大 512，不超载
- Shader 仅 Boss 战启用

## 实施计划

### T1: 粒子系统
- 新建 `ParticleSystem` 类
- 粒子池 + 发射器
- 单元测试：粒子生成/更新/生命周期

### T2: 武器特效增强
- 扩展 `Effect.kind` 类型
- 5 类武器 × 3 段连击特效
- 单元测试：类型映射

### T3: 打击感组合
- 增强震屏强度分级
- 新增 `HitFlash` 类
- 飘字颜色分级
- 单元测试：闪白触发

### T4: Shader 效果
- 新建 `VFXShader` 类
- 发光/模糊/扭曲
- 单元测试：启用/禁用 fallback

## 门禁

- Release 0 error
- ctest 66/66（新增测试用例）
- World Validator 0/0
- 桌面包同步
- Tag: `v1.11-A9`
