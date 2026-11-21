# 一怪一形（A22 深化）立项设计（2026-11-21）

状态：**已确认（用户拍板 3 项，全按推荐）+ builder 审查完成**
依赖调查：`docs/superpowers/specs/2026-11-17-modeling-audit.md` + 本轮只读盘点（§1）

### Builder 审查纳入的关键修正

- **gate 只比 head+签名槽，排除 arm/leg**（最高）：merge 保留 pilot 的 arm/leg 几何，全槽两两比必然碰撞永远 RED。head 13/13 全覆盖 + weapon/torso 按签名定义比。
- **坐标预算**：head y∈[3,5]（base ymax≈5.6）、torso y∈[4,6.5]（ymax≈6.8）、weapon y∈[5,8]。spec 原"y 4~10"会把 head/torso 细节 clamp 压成贴片。细节留 10-20% 余量，靠位置/数量/不对称/不同基元分段数制造哈希与视觉差异。
- **night_stalker 预期**：包络内做不到真匍匐兽形，下调为"兽首尖耳+尖牙+利爪"表面细节（骨架仍 humanoid），审图按此标准。
- **执行顺序**：merge 注入循环必须在 `_REFINE_FAMILY` 定义（:1522-1527）之后、族拷贝循环（:1528）之前，利用 `:1529` continue 跳过。
- **merge 深拷贝**：`{slot: list(details)}` 逐槽复制 list，签名槽整槽替换；绝不共享可变 list（否则污染 pilot 全族）。
- **--unique 哈希**：直接读 `assets/models/mon_*_part_*.json`（注入用 `REFINEMENTS[_mon]` 正则匹配不到）；量化坐标多重集 sha256（免疫 bmesh 面序非确定性）；先断言两侧顶点数相等避免假 RED。
- **范围诚实**：只解决 13 种；其余 17 只仍与 pilot 共享几何，验收说明写清，避免误读为"全 30 已去重"。

## 0. 用户拍板

| 决策点 | 选择 |
|---|---|
| 范围 | 第一批强身份 13 种 |
| 方案 | 包络内特征件（head/weapon 槽加独特造型，沿用 _refine clamp） |
| 闸门 | 加 --unique（顶点坐标多重集哈希防 cloning 回退） |

## 1. 根因与现状

`blender_models_monsters.py` 的 `_REFINE_FAMILY`（:1519-1533）用 `dict(REFINEMENTS[pilot])`
浅拷贝：5 个槽的 lambda 列表是同一批对象，只换 `_color(name)` 配色。
实测证实 26/30 怪与同族样板"几何语义等价"——同槽顶点坐标多重集哈希相同、面数相同、只有顶点色不同。
2D 层本来每怪一形（`tools/m5_sprite_gen.py` 逐怪有特征注释），是 3D 族拷贝把身份压平了。

4 族分组（30 种）：humanoid 19（orc 配方）、orb 4（fire_imp）、slime 5（slime）、construct 2（golem）。
4 个 pilot 已有独立配方；本轮给第一批 13 种写独立特征配方。

## 2. 第一批 13 种（强身份，按 2D 线索）

| id | 中文名 | 族 | 2D 独特造型 → 3D 特征件（包络内） |
|---|---|---|---|
| night_stalker | 夜行猎手 | humanoid | 匍匐兽形→兽耳+尖牙+兽目（head），利爪（weapon） |
| skeleton_archer | 骷髅弓手 | humanoid | 骷髅骨白+黑眼窝+额裂纹（head），骨弓（weapon） |
| bone_soldier | 骨兵 | humanoid | 破胸甲+红眼窝（head/torso），锈剑+盾（weapon） |
| shadow_assassin | 暗影刺客 | humanoid | 头巾斗篷+收窄身形+幽蓝双刃（head/weapon） |
| shadow_stalker | 暗影潜伏者 | humanoid | 幽灵披风+锯齿下摆+发光红眼（head/torso） |
| storm_elemental | 雷暴元素 | orb | 雷云体+云底锯齿+中央落雷（head/torso） |
| lightning_orb | 电光之核 | orb | 球体+交叉电弧+四向尖刺（head） |
| void_walker | 虚空行者 | orb | 体腔虚空+斜向亮紫裂隙+白瞳（head/torso） |
| blood_leech | 鲜血水蛭 | slime | 环节体+前端吸盘+湿润高光（head/torso） |
| poison_wyrm | 毒液蠕虫 | slime | 分节+抬起蛇头+毒牙+背毒泡（head/weapon） |
| stone_guardian | 石像守卫 | construct | 石甲块分割+发光符文眼+胸符（head/torso） |
| ice_warden | 冰狱守卫 | humanoid | 三峰冰晶冠+冰核+冰瞳（head/torso） |
| iron_sentinel | 铁卫 | humanoid | 头盔+一字蓝目镜+胸前中缝（head/torso） |

## 3. 约束与落地

- **不改机制**：利用 `:1529` 的 `if _mon in REFINEMENTS: continue`——在 `:1519` 前显式写 `REFINEMENTS[id]` 即跳过族拷贝。
- **包络内**：细节用 part_* 基元、Blender 局部 +Y（head 3~5 / torso 4~6.5 / weapon 5~8，保证导出 mesh y<0 正面且不被 clamp 压平），被 `_refine`（:1015-1026）clamp 进基础部件 bbox。不改 clamp、不改 FAMILIES/ANCHORS。
- **数据结构**：新增 `SIGNATURE_DETAILS = {id: {slot: [callables]}}` + merge 助手——先 `{s: list(d) for s,d in REFINEMENTS[pilot].items()}` **复制列表**，再对 head/weapon 换成新列表。绝不共享可变列表（否则改掉 pilot 全族）。
- **面数**：当前最大单槽 2176，cap 4000，余量充足。
- **不改清单**：_refine/build_parts 签名、协议 B、JSON schema、ground_item_baselines.json、resources、引擎。
- CLAUDE.md 规范：函数≤40 行、单一职责、语义命名、改前影响分析、改后 Code Review、单次≤300 行、可维护性优先。

## 4. 闸门 --unique

`tools/check_ground_item_assets.js` 加 `--unique`：直接读 JSON、对 **head（13/13）+ weapon/torso（按签名）** 逐槽比较
**顶点坐标多重集 sha256**（面索引哈希受 bmesh 非确定性影响，不用；**排除 arm/leg**——它们保留 pilot 几何必然相同）。
比较前先断言两侧顶点数相等（避免重三角化假 RED）。哈希对坐标量化后排序拼接，免疫 bmesh 面序非确定性。
RED：当前 26 只克隆 head 哈希=pilot → FAIL；GREEN：13 种签名后互不相同 → PASS。
baselines.json 不动（保持"原始 A17 基线"语义）。

## 5. 验收

- `node tools/check_ground_item_assets.js --all` → CHECKED=150 FAILED=0
- `node tools/check_ground_item_assets.js --unique` → 13 种特征槽哈希互不相同
- ctest 91/91（player_part_asset_test.All30MonsterPartsLoad）、fn_length 0、world_validator
- Blender MCP 逐批预览审图：13 种造型差异可辨、无穿模、正面细节朝相机
- 桌面包同步 + 最终审查

## 6. 任务拆分

1. --unique 闸门 + 唯一性契约测试（RED：当前 26 种 cloning 应失败）
2. SIGNATURE_DETAILS + merge 助手 + 13 种特征配方（包络内）
3. MCP 重导 13×5 JSON + 闸门全绿 + 审图
4. 全量回归 + CHANGELOG + 桌面同步 + 推送 + 最终审查
