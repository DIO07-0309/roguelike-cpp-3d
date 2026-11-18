# P3 建模资产审查报告（地标 + 道具）（2026-11-18）

> 只读审查，两路并行。结论：9 地标 + 16 道具全部齐全、契约完备；
> 真正短板是**通用道具面数偏低**（204–592），以及一处 A22 遗留的脚本残留。

## 1. 资产总览

| 类别 | 件数 | 面数区间 | 均值 | 生成脚本 |
|---|---:|---:|---:|---|
| 地标 lm_* | 9 | 380–1688 | 1062 | `blender_models_landmarks.py`（LANDMARK_BUILDERS 字典，9 builder） |
| 护甲 item_armor_* | 8 | 728–1310 | 1060 | `blender_models_armors.py`（ARMOR_BUILDERS + 共用 `_base_chest`） |
| 通用道具 item_* | 8 | **204–592** | **362** | `blender_models_items.py`（由 `build_models_blender.py` BUILDERS 调用） |

合计 P3 范围 25 件（9 + 16）。

## 2. 现状事实

**地标**（`resources/landmarks.json` 9 id，三群系各 3）：
- id → `lm_<id>` 文件 → `landmark_preview_<id>.png` 一一对应，无缺项。
- 9 件均脚底 y=0、正面 -Y（导出后 +Z 朝相机）、平面占地多为 30×22。
- 代码 `_emit_landmark_prop`（`hd2d_scene_builder.cpp:1468-1477`）按 `lm_+landmark_id` 加载，**不读 triggered、无 used 态**——触发后保留遗迹，这与 A22 的"触发态返回未使用态"其他房间不同。
- 契约 `AllNineLandmarkModelsExist`（`mesh_asset_test.cpp:158-172`）钉死 9 件齐全。

**道具**：
- 8 护甲 A19 已做专属特征件（缝线/铆环/鳞片/尖肩/符文/霜纹/兜帽），面数健康，风格统一。
- 8 通用道具面数明显偏低：长枪 204、双截棍 296、药水 272×2、匕首 332、剑 440、护符 496、弩 592。造型是低段数基元（cylinder vertices=6/8/10、sphere 8×4~10×6、torus 8×5），功能可辨但细节层次弱。
- icon key → 模型映射在 `kGroundItemModels`（`hd2d_scene_builder.cpp:1103-1120`，16 条），已无 item_armor/item_robe 残留。
- 契约 `AllEightArmorModelsExist`（`mesh_asset_test.cpp:176-188`）钉死 8 护甲齐全。

## 3. 发现的问题（可在 P3 一并处理）

**A22 遗留脚本残留**：`build_models_blender.py:78-79` 的 BUILDERS 仍含
`"item_armor": item_models.build_armor` 与 `"item_robe": item_models.build_robe`，
而这两个 JSON 已在 A22 删除。重跑该脚本会重新生成已清理的孤儿资产，
使 A22 的资产卫生工作倒退。`blender_models_items.py:93-151` 的两个遗留 builder
同理。属于低风险卫生问题，建议 P3 批次 0 顺手清掉。

## 4. 与 A22 精修管线的关键差异（决定技术方案）

A22 怪物精修用的是**角色部件协议 B**（`export_part_json`，mesh=(x,-y,z)，正面 mesh.y<0）
+ per-monster refinement 挂点 + envelope clamp。

P3 的地标/道具用的是**整件场景协议 A**（`export_json`，(x,y,z)→(x,z,-y)，
正面 -Y→引擎 +Z），且是**单件完整模型**（不是 5 槽部件），没有骨骼锚点/pivot 概念。
因此：
- 不能复用 `_refine` 的 clamp（那是按部件 bbox 收敛）。
- 正面/包络契约要重新定义：地标/道具是整件摆放，envelope = 该件自身的 bbox，
  精修只能向内加细节或轻微抬高，不得放大占地（否则可能穿墙/盖过相邻格）。
- 闸门 `check_ground_item_assets.js` 只校验 mon_* 部件，P3 需要独立的地标/道具契约。

## 5. 结论

P3 的重点应是**8 件通用道具的细节层次提升**（面数 204→目标 600-1200，
与护甲/地标的 1000+ 看齐）；8 件护甲和 9 件地标本身质量健康，属于可选锦上添花。
批次 0 顺手清理 A22 的脚本残留。

优先级建议（供立项拍板）：
1. 批次 0：清理 `build_models_blender.py` 的 item_armor/item_robe 残留（低风险）。
2. 批次 1：8 件通用道具精修（收益最高，面数落差最大）。
3. 批次 2（可选）：9 地标 / 8 护甲的细节层次微调。
