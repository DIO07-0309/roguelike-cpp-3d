# 汉中 3D · P7 照片级深化设计（玻璃 / 绿化带 / 树 / 街景细节）

日期：2026-10-07。状态：已获用户逐节批准。
前置：`docs/superpowers/specs/2026-10-06-hanzhong-v2-design.md`（v2 冲刺 + P1-P6 已交付）。

## 1. 目标与范围

目标：在 P1-P6 基础上继续照片级深化——**每一个建筑的玻璃**、**绿化带与树**、
以及关联街景要素，分两个检查点逐步执行、逐步目验。

用户拍板记录（2026-10-07 四问）：

| 决策点 | 结论 |
|---|---|
| 玻璃深度 | **材质级全城 + 近景几何**（全城 shader 升级；~100 栋近景楼真实窗洞） |
| 绿化带来源 | **道路推导 + OSM 绿地补抓**（路上绿带处处有 + 公园真实分布） |
| 树升级 | **形体 + 密度双升级**（簇冠分叉色变；1.3 万 → ~2.8 万棵） |
| 范围边界 | 含 **车辆升级、街景家具、屋顶细节**；**不含行人** |

执行策略（拍板）：**方案 C —— 分段流水线 + 两个目验检查点**。

## 2. 总体分段

```
检查点①（先交付目验）            检查点②（终版）
M0 数据补抓 ──┐                 M3 绿化带 ──┐
M1 玻璃系统 ──┼→ ★交付一版       M4 树密度 ──┼→ ★终版交付
M2 树形体   ──┘  (目验玻璃/树)    M5 车辆     │   (全量+report §9)
                               M6 街景家具 ─┤
                               M7 屋顶细节 ─┤
                               M8 近景窗几何─┘
```

检查点①通过前不启动 M3-M8；每里程碑固定流程：语法 + `hz_review_scan.py`
（函数 ≤40 行、文件 ≤300 行）+ 冒烟 → 下一个。

## 3. 检查点① 详设

### M0 数据补抓与预算基线

- 扩展 `hz_fetch_tags.py` 管线（union 括号 + kumi/de/osm.jp 三镜像 fallback），
  一次抓全后续所需，写回 `hanzhong_osm.json`（新增键 snake_case）：
  - `leisure=park`、`landuse=grass|recreation_ground` → `greenspaces[]`（多边形）
  - `highway=median` → `medians[]`（独立键，不并入 roads——避免
    road_width/车辆/标线消费方把绿化带当车行道）
  - `highway=bus_stop` → `bus_stops[]`（点）
  - 交通信号灯节点 `highway=traffic_signals` → `signals[]`（点）
- 抓取失败降级策略：绿地缺 → M3 仅做道路推导层；signals/bus 缺 → M6 按路网推导。
- 记录预算基线：当前 ~1.4M 面、EEVEE 0.78s/帧、Cycles 256spp ≈ 3min/帧。

### M1 玻璃系统（全城材质级，零面数增量）

模块：`hz_v2_glass.py`（玻璃 shader 组装；`make_facade_mat` 调用点接入）。

- 渲染开关：EEVEE `scene.eevee.use_raytracing = True`（天空反射；性能见 §6 风险 1）。
- 窗格升级（全部在 `make_facade_mat` 节点层，几何不动）：
  1. **玻璃面**：窗格内格 = 玻璃 BSDF 近似（Principled：Transmission/低 rough、
     淡蓝绿基色、Fresnel 随墙面法线反射 Sky Texture）。
  2. **窗框**：Brick Mortar 改金属灰框色、Mortar Size 对应 0.08m 框宽。
  3. **假室内**：窗格内叠加暗色渐变层（由格内 UV 纵向渐变 + 深色 interior 色），
     消除贴纸感。
  4. **朝向差**：四向墙反射色温微差（按墙面法线象限 Mix），避免全楼同脸。
- 夜景兼容：现有 40% 逐格点亮逻辑保留，**emission 优先于反射**（夜窗必须亮）。
- 产出**近景楼选型规则**（供 M8）：v1 相机轴线走廊 + 4 地标 150m 内 OSM 楼
  → 目标 ~100 栋，输出 `closeup_buildings.json` 清单（id/pts/h）。

### M2 树形体升级（位置不动）

模块：`hz_v2_tree.py`（从 `hz_v2_bio.py` 拆出树生成；bio 保留车/芦苇并调用）。

- **阔叶**：主干 beam + 2~3 分叉 beam + 3~4 球簇（半径 0.6~1.0×、高低错落）；
  **4 色变分桶**——canopy 拆 4 个对象、各配 leaf 材质色调微变。
- **塔柏**：主干 + 2~3 层叠窄锥（高度递减）。
- **垂柳**：簇冠 + 现有 4 垂枝 + 分叉干。
- 树型选择/河滩规则不变（M4 密度升级沿用）；局部 rng 派生规则不变。
- 预算：~90 面/棵（现 30）。超预算先砍 dome sides（7→5）不砍簇数。

### ★检查点① 交付与目验

双 build（日/夜）+ 日/夜主帧 Cycles 256spp + 转台 + 街景/楼群近景专项
+ `hz_v2_assert` 全跑 → 刷新交付 5 件 + report 增补 → **用户目验
玻璃反射与树簇效果**。不通过则回到 M1/M2 返工，不启动第二波。

## 4. 检查点② 详设

| 里程碑 | 内容 | 依赖 |
|---|---|---|
| **M3 绿化带** `hz_v2_green.py` | ① 主干/trunk/secondary 中央 **1.6m 绿篱条**（灌木链几何，宽路 16/24m 才有）② 人行道外侧 **3.5m 草坪带**（贴地 ribbon + 灌木球间隔）③ OSM `greenspaces[]` 多边形铺**公园草地**（逐顶点贴 dem、z+0.05，面数按多边形简化） | M0 |
| **M4 树密度** | ① 中分带树阵（20m 株距，仅有绿篱的宽路）② 路侧绿带乔木（与行道树错位）③ 次干/支路行道树双侧补种（15m 株距）→ 总数 **~2.8 万** | M2+M3 |
| **M5 车辆** | 车窗玻璃（复用 M1 玻璃 shader 独立小格）、尾灯 emission（夜景亮）、车型 2→4（+公交/货车几何） | M1 |
| **M6 街景家具** `hz_v2_furniture.py` | 主干路口信号灯（杆+三灯箱）、路名牌、公交站台（`bus_stops[]` 数据，缺失按主干 300m 推导）、井盖与路面修补贴图分块（`make_road_mat` 贴图层噪声门控） | M0 |
| **M7 屋顶细节** `hz_v2_roofdet.py` | 平顶楼 40% **太阳能板阵列**（朝南倾角、行列）、烟囱/水箱（15%）、女儿墙压顶环；坡顶屋脊压顶 + 烟囱（10%）。接入 P1 `attach_roof` 后置钩子 | P1 分类 |
| **M8 近景窗几何** `hz_v2_window_geo.py` | `closeup_buildings.json` ~100 栋：每窗内凹 0.12m 玻璃 quad + 4 框条 + 窗台（5~8 面/窗，预算 **3~5 万面**）；格尺寸与 M1 同契约（3m 层高、格宽高沿用 brick 参数） | M1 定稿 |

**★终检**：全量重建 + 双 render + 转台 + 七视角 + 近景专项 + assert →
交付 5 件刷新 → `report.md` §9 + `deliverables/README.md` + sdd 归档。

## 5. 架构与数据契约

- 新模块（各 ≤300 行、函数 ≤40 行、组合优于继承）：
  `hz_v2_glass / hz_v2_tree / hz_v2_green / hz_v2_vehicle /
  hz_v2_furniture / hz_v2_roofdet / hz_v2_window_geo`。
- 数据：`hanzhong_osm.json` 新增 `greenspaces[] / medians[] /
  bus_stops[] / signals[]`；贴图目录 `textures/` 扩展（草地/灌木 CC0，
  复用 `hz_fetch_textures` + `hz_v2_tex` 加载与 pack）。
- 材质：玻璃 shader 组装函数供 facade 与车窗共用；新增材质键进
  `make_simple_mats`（hedge/grass/leaf 变体等）。
- 抬升常量分级（z 冲突防线）：路面 0.6 / 人行道 0.72 / 草带 0.65 /
  绿篱 0.65 起 / 贴地物 0.05。

## 6. 预算红线与验收

- **红线**：总面数 ≤2.6M；EEVEE ≤2s/帧；Cycles 256spp ≤5min/帧。
  超线牺牲顺序：砍 dome/灌木 sides → 降绿篱密度 → 收窄 M8 范围。
- 每检查点验收 = 现有 `hz_v2_assert`（法线 ×6、天空角、帧唯一、STATS 更新）
  + 该波新增专项数字采样（玻璃反射可见、树簇计数、绿带覆盖率）。

## 7. 风险与对策

1. **raytracing 拖慢 EEVEE**：帧时 >2s 则转台关反射、静帧保留（静态收益为主）。
2. **OSM 绿地抓取失败**：降级纯道路推导层，公园缺口下轮补。
3. **夜窗 emission 与反射打架**：emission 优先，夜窗保持 40% 点亮。
4. **绿带与人行道/楼基 z 冲突**：逐顶点贴 dem + §5 分级抬升常量。
5. **M4 树数超面预算**：先降簇面（sides）、再降密度至 2.4 万保交付。
