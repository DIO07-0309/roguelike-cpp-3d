# P3-2 地标 / 护甲细节微调审查报告（2026-11-18）

> 只读审查，两路并行。结论：9 地标 + 8 护甲均健康，无缺口；
> 本轮是锦上添花，须明确"微调"边界与 bbox 契约。

## 1. 资产总览

| 类别 | 件数 | 面数区间 | 生成脚本 | 结构 |
|---|---:|---:|---|---|
| 地标 lm_* | 9 | 380–1688 | `blender_models_landmarks.py`（LANDMARK_BUILDERS，9 builder） | 各件独立造型 |
| 护甲 item_armor_* | 8 | 728–1310 | `blender_models_armors.py`（ARMOR_BUILDERS + 共用 `_base_chest`） | 7/8 共用骨架，robe 独立 |

## 2. 地标的造型短板与可加部位（9 件）

| 地标 | 面数 | 主要短板（docstring 承诺但未建模 / 素面部位） |
|---|---:|---|
| fire_pillar | 380 | 火焰=5 同轴锥、最简；岩座单圆柱，无岩块/火舌分层 |
| lava_rift | 900 | 地面/熔岩平直长条；仅 3 泡+3 尖柱，无龟裂/细脉 |
| forge_ruins | 912 | 炉体单 box、炉口平面；铁砧仅底座+顶板；无烟囱/散落工具 |
| floating_altar | 936 | docstring 的"幽光符文""空间扭曲环"未建模；石板顶面平 |
| void_crack | 848 | 裂口/辉光/碎片都是斜 box 平板，无锯齿/多层内辉 |
| collapsed_tunnel | 1248 | 门楣平直（"断裂拱门"未断）；落石仅 6 大球，无碎石 |
| broken_cell | 1228 | docstring"墙上抓痕"实际只有 1 条 crack；铁栏无锈环/端帽 |
| ancient_gate | 1416 | 6 符文只是小 box 无刻纹；双子/门楣平 box |
| torture_chamber | 1688 | 垂链用球串无链环；docstring"锈刑具"仅 1 把刀 |

**bbox 边界**：全部 y_min=0（贴地契约 ±0.1）。占地：六件地面 x±15/z±11（collapsed_tunnel 斜梁至 x=−16.41）、ancient_gate 26×11（z 非对称 −6..5）、floating_altar 19×19、fire_pillar 16×16。顶部 y_max 13.5–36.5。

## 3. 护甲的造型短板与可加部位（8 件）

| 护甲 | 面数 | 结构 | 主要短板 |
|---|---:|---|---|
| robe | 728 | 独立 | 兜帽无内衬/面影；前襟单条发光带；袖/下摆素面 |
| ice | 990 | 骨架 | 仅 2 肩刺+2 短霜纹；胸前素面；腰/领无垂冰 |
| plate | 1002 | 骨架 | 大胸甲板无刻槽/分块；金饰仅 1 横条+1 护心镜 |
| leather | 1058 | 骨架 | X 缝线与腰带间素面；领口下沿/肩/背面素 |
| iron | 1130 | 骨架 | 胸板整块无纹饰；铆钉仅 6 枚；肩无附加片 |
| rune | 1158 | 骨架 | 符文仅 3 发光短条无次级笔画；核心单色球 |
| chain | 1310 | 骨架 | 环阵只覆盖 x±4.5/z5..11；上胸/腹/肩/背无环 |
| scale | 1310 | 骨架 | 鳞阵宽仅 6.4、上缘 z≈12.3；领下/肋侧/肩/背无鳞 |

**bbox 边界**：多数 x±10~12、高度 16.2、深度 [-3.8,4.3]；ice 顶 20.0、plate 19.0、robe 19.1（robe y_min=−0.082 在容差内）。7/8 共用 `_base_chest`（chest/belt/buckle/ridge/collar + 双肩球），仅换色与 `pauldron_r`。

## 4. 契约（微调不能破坏）

- `mesh_asset_test.cpp`：AllModelsLoad（可加载/三角面>0/三数组等长）、FeetAtGroundLevel（y_min∈[−0.1,0.1]、height>1）、AllNineLandmarkModelsExist / AllEightArmorModelsExist（文件名+存在性）。
- 命名依赖：`kGroundItemModels` 映射 8 护甲 icon_key→同名模型；`item.cpp` 按 base_name 派生 icon_key。不改名即不受影响。
- 配色是脚本硬编码常量，与 landmarks.json 的 tile_color 仅注释级对应。

## 5. 与 P3 道具微调相同的关键经验（必须沿用）

协议 A 同样是**低段数采样 bbox**。提高半径敏感件（瓶身/圆盘/肩球）段数会采样到更精确极值、撑破原 bbox。对策：半径敏感件保持原段数，靠 bbox 内部装饰（浮雕/短条/小环/碎块）补细节，所有新增顶点落在原 bbox 内。

## 6. 边界判读（供立项拍板）

"微调"应明确定义：是**仅在原 bbox 内加细节、不改面数下限**（保守），还是**显著提升细节密度并设面数目标**（如地标/护甲各 +30~50%）。前者风险低、后者视觉收益大但需逐件设目标。
