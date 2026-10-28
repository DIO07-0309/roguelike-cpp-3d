# A16-Player: 玩家完整骨骼动画低模 — 实现计划

日期：2026-10-28 · 设计：`2026-10-28-player-lowpoly-design.md`

## 阶段 ①：低模部件建模导出（Blender 无头）
- [ ] 1a. `art_src/scripts/blender_models_player.py`：`export_part_json`（mesh 局部映射 `(x,-y,z)` + 明暗表）+ 6 个 `build_player_part_<slot>()`
- [ ] 1b. `art_src/scripts/build_player_parts.py`：构建 6 件 → 导出 `assets/models/player_part_<slot>.json` + 装配预览渲染
- [ ] 1c. 运行 `"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup -P build_player_parts.py`
- [ ] 1d. 校验 6 个 JSON：verts>0、faces>0、位置在预期边界内（对照设计 §2.5）

**门 G1**：6 个 `player_part_*.json` 生成，格式与 mesh_asset 加载器兼容（`load_mesh_asset_file` 无 err）

## 阶段 ②：C++ 接入替换体素
- [ ] 2a. `model_cache.h/.cpp` 暴露 `Mesh build_gpu_mesh_asset(const MeshAsset&)`（复用 build_gpu_mesh）
- [ ] 2b. `player_avatar.h` 声明 `bool _setup_lowpoly_parts()`
- [ ] 2c. `player_avatar.cpp`：`try_init` 优先低模回退体素；实现 `_setup_lowpoly_parts`（按 slot 载 6 件→8 mesh，失败释放回退）

**门 G2**：构建通过；`buildPlayerAvatar` 走低模路径（mesh vertexCount>0）

## 阶段 ③：测试 + 静态检查
- [ ] 3a. `tests/rendering/player_part_asset_test.cpp`（6 件 load + 三角面 + 无退化 + 尺寸边界）
- [ ] 3b. `tests/CMakeLists.txt` 注册 `player_part_asset_test`
- [ ] 3c. 构建 + `ctest`（88 + 1 全绿，golden_baseline 不变）
- [ ] 3d. `py -3 tools/fn_length_scan.py` 0 超规；`world_validator` 0/0

**门 G3**：ctest 全绿、fn_length 0、validator 0/0

## 阶段 ④：交付 + 推送
- [ ] 4a. `README.md` 记录 A16 玩家低模
- [ ] 4b. 桌面同步（src/assets/tests/docs/art_src/README + exe）
- [ ] 4c. 冒烟（`--hd2d` 12s 存活 0 失败）
- [ ] 4d. commit + `git push origin master`（`$env:GIT_TERMINAL_PROMPT="0"; $env:GIT_ASKPASS="echo"`）
