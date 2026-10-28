# A16-Player: 玩家完整骨骼动画低模 — 设计文档

日期：2026-10-28 · 状态：已确认（用户选方案 A：完整骨骼动画低模）

## 1. 目标

把玩家从 A13 的**自动体素部件**（`player_part_*.png` 经 `voxel::build_part_mesh` 体素化）
替换为 **`blender_lib` 手工低模部件**（6 个唯一部件：leg/torso/head/arm/weapon/cape），
**复用现有骨骼 + 动画系统**（`SkeletonAvatar` 骨骼 + `AvatarAnimator` idle/walk/attack/hit），
不推倒动画系统。元素染色（披风 `e_tint`）保留。

## 2. 现状（已探明）

### 2.1 玩家渲染链路（已非纯 2D）
- `hd2d_scene_builder.cpp:504 buildPlayerAvatar` → `avatar->worldParts(player)` → `appendAvatarParts`
  （`hd2d_part_geometry.cpp:11`）画 `AvatarPartDraw.voxel`（A13 体素 Mesh）+ `e_tint`（披风染色）。
- `--no-voxel-avatar`（`voxel::g_voxel_avatar`）才回退 2D 纸偶。

### 2.2 骨骼 + 动画（全复用）
- 骨骼：`resources/animations/player_skeleton.json` — 9 骨
  root/hips/torso/cape/head/arm_back/weapon/arm_front/leg_front/leg_back，`pixels_per_unit=0.8`。
- 部件（8 个，与骨骼一一对应）：`player_skeleton.json` `parts[]`：
  | # | bone | file | pivot | dx/dy | tint |
  |---|------|------|-------|-------|------|
  | 0 | leg_back | player_part_leg.png | [11,20] | - | - |
  | 1 | cape | player_part_cape.png | [13,25] | -4/9 | ✓ |
  | 2 | arm_back | player_part_arm.png | [5,16] | - | - |
  | 3 | weapon | player_part_weapon.png | [3,24] | - | - |
  | 4 | torso | player_part_torso.png | [13,10] | - | - |
  | 5 | head | player_part_head.png | [11,7] | - | - |
  | 6 | arm_front | player_part_arm.png | [5,16] | - | - |
  | 7 | leg_front | player_part_leg.png | [11,20] | - | - |
- 动画：`resources/animations/player_anim.json` 的 `animations`（idle/walk/attack/hit；
  `AvatarAnimator` 优先级 attack>hit>walk/idle，attack/hit 一次性）。

### 2.3 体素 Mesh 局部坐标约定（`part_voxelizer.cpp emit_cell`）
- `xa = x - pivot_x`（x=贴图横向，原点=骨骼锚点/pivot）
- `zb = (height - y) - pivot_y`（z 向上，原点=骨骼锚点；锚点以下为负）
- `y ∈ [-depth/2 + y_offset, depth/2 + y_offset]`（厚度，负=朝相机）

### 2.4 渲染矩阵（`hd2d_part_geometry.cpp partMeshMatrix`）
- mesh 局部轴 → 相机空间：`x→camera right`，`y→-camera normal`（负=朝相机），`z→camera up`；
  缩放 `kx = size/texture.width = ppu*sx`，`kz = height/texture.height = ppu*sy`；
  绕 pivot 在广告牌平面内做 `rot_deg`（骨骼 pose 旋转）+ offset（骨骼平移）。
- **关键**：当前"完整骨骼动画"已在广告牌平面内实现——每个部件 mesh 随骨骼 `rot_deg`/offset
  旋转平移。**低模只需替换 mesh 几何，骨骼+clip 全复用。**

### 2.5 部件贴图尺寸（低模 mesh 的尺度基准）
| 部件 | 贴图 px | pivot | mesh 局部范围（x, z） |
|------|--------|-------|----------------------|
| leg | 22x20 | [11,20] | x∈[-11,11], z∈[-20,0]（锚点在顶，向下） |
| cape | 30x26 | [13,25] | x∈[-13,17], z∈[-25,1] |
| arm | 11x16 | [5,16] | x∈[-5,6], z∈[-16,0] |
| weapon | 6x24 | [3,24] | x∈[-3,3], z∈[-24,0] |
| torso | 28x20 | [13,10] | x∈[-13,15], z∈[-10,10]（锚点在中间） |
| head | 22x20 | [11,7] | x∈[-11,11], z∈[-7,13]（锚点在颈） |

## 3. 方案：手工低模部件替换体素

### 3.1 建模（`art_src/scripts/blender_models_player.py`）
- 用 `blender_lib` 基元（`part_box`/`part_cylinder`/`part_sphere`/`part_prism`/`join_parts`）构建
  6 个唯一部件，**每个部件一个导出 .json**（`assets/models/player_part_<slot>.json`）。
- **Blender 构建坐标系**：X=宽度(右)，Y=深度(前面=+Y 朝相机)，Z=高度(上)；**部件锚点(骨骼 pivot)在原点**。
- **导出映射**（`export_part_json`）：`mesh = (co.x, -co.y, co.z)` → mesh 局部
  x=宽度、y=深度(负=朝相机)、z=高度；明暗表按 mesh 局部法线主轴烘进顶点色
  （front(-y)=1.0 / back(+y)=0.45 / sides(±x)=0.62 / top(+z)=1.55 / bottom(-z)=0.30）。
- 输出格式与 `mesh_asset` 加载器一致：`{"name", "vertices":[[x,y,z,r,g,b]...], "faces":[[i,j,k]...]}`。

### 3.2 调色板（采样自 `player_part_*.png`）
| 用途 | RGB |
|------|-----|
| 皮肤/亮甲 | 149,166,183 |
| 主甲(蓝灰) | 99,111,133 |
| 暗部/头发 | 63,38,49 |
| 深靴/手套 | 70,66,84 |
| 钢刃/亮缘 | 205,220,222 |
| 金饰 | 175,129,88 |
| 红宝石 | 154,48,62 |
| 披风基白(元素染色) | 226,228,234 |

### 3.3 部件几何（锚点在原点，Blender Z 上，单位=贴图 px）
- **leg**：大腿(锥台 上宽下窄) + 小腿 + 靴(前端前伸)，z∈[-20,0]，x∈[-11,11]。
- **torso**：胸甲(上宽锥台) + 腹/腰带 + 胸前饰板，z∈[-10,10]，x∈[-13,15]。
- **head**：颈(柱) + 头(椭球) + 头发(暗部)，z∈[-7,13]，x∈[-11,11]。
- **arm**：上臂(锥台) + 前臂 + 手(球)，z∈[-16,0]，x∈[-5,6]。
- **weapon**：剑 = 握柄(暗) + 护手(金) + 剑刃(钢/锥尖) + 红宝石，z∈[-24,0]，x∈[-3,3]。
- **cape**：梯形披风(上窄下宽，位于背后 y>0，bevel 圆边)，z∈[-25,1]，x∈[-13,17]，基白。

### 3.4 C++ 接入（`src/game/animation/player_avatar.cpp`）
```cpp
bool PlayerAvatar::try_init(...) {
    if (!_core.try_init(...)) return false;
    if (voxel::g_voxel_avatar) {
        if (!_setup_lowpoly_parts())   // A16: 优先手工低模 (6 件)
            _setup_voxel_parts();      // 回退 A13 体素 (原逻辑)
    }
    return true;
}
```
- `_setup_lowpoly_parts()`：按 `_core.part_defs()` 顺序（8 个），用 `voxel_slot_from_path(part.file)`
  取 slot → `load_mesh_asset_file("assets/models/player_part_"+slot+".json")` → `build_gpu_mesh_asset`
  上传 GPU Mesh；任一缺失/无效 → 释放已建并返回 false（走体素回退）。
- `model_cache` 暴露 `build_gpu_mesh_asset(const MeshAsset&)`（复用现有 `build_gpu_mesh` 逻辑）。

### 3.5 测试（`tests/rendering/player_part_asset_test.cpp`）
- 6 件 `player_part_leg/torso/head/arm/weapon/cape` 均 load、`triangle_count>0`、
  `positions.size()%3==0`、色/法线数与位置数一致、无退化面。
- **不进** `mesh_asset_test` 的 `kModelNames`（那里校验"脚底 y≈0 贴地"，而部件锚点在骨骼 pivot，
  不贴地，属不同约定）。

### 3.6 回归约束
- `golden_baseline`：玩家低模**只改渲染几何，不改 gameplay**——sim 走主循环但玩家 mesh 仅在
  渲染路径构建，sim 输出应不变；若漂移须归因（预期**不变**）。
- `ctest` 88/88 + 新增 `player_part_asset_test`；`fn_length` 0 超规；`world_validator` 0/0。
- 桌面同步 + 冒烟（`--hd2d` 12s 存活 0 失败）。

## 4. 非目标
- 不改骨骼/动画 JSON、不改 `AvatarAnimator`、不改 `partMeshMatrix`/`appendAvatarParts` 渲染路径。
- 不做 Blender 骨骼绑定/动画导出（现有骨骼动画已满足"完整骨骼动画"——广告牌平面内逐件动画）。
- 不改怪物/NPC 体素路径（仅玩家换低模）。

## 5. 交付物
1. `art_src/scripts/blender_models_player.py` + `art_src/scripts/build_player_parts.py`（建模+导出+预览）
2. `assets/models/player_part_{leg,torso,head,arm,weapon,cape}.json`（6 件）
3. `src/data/model_cache.h/.cpp` 暴露 `build_gpu_mesh_asset`
4. `src/game/animation/player_avatar.{h,cpp}` 接入 `_setup_lowpoly_parts`
5. `tests/rendering/player_part_asset_test.cpp` + `tests/CMakeLists.txt` 注册
6. `art_src/preview/blender_preview_player*.png`（装配预览）
7. `README.md` 更新
