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

## 9. P7 检查点② 交付报告（M3-M8 终版 · report §9 同文，2026-10-11）

§4 ★终检收口：全量重建 + 双 render + 转台 + 七视角 + 近景专项 + assert → 交付 5 件刷新
→ `report.md` §9 + `deliverables/README.md` + SDD 归档。M3-M8 六任务 + 终检 + 交付全部完成
（`$tmp` 提交链 `166172b..45a2e71`；正文与 `C:\Users\HP\Desktop\deliverables\hanzhong\report.md` §9 同文）。

### 9.1 各任务结果（提交 · Δ面数 · 门禁）

| 任务 | 里程碑 | 提交 | 关键数字 | Δ面数 / cap | 门禁 |
|---|---|---|---|---:|---|
| T1 | M3 绿化带 `hz_v2_green` | `166172b` | GREEN2 hedge=157,365m grass=558,627m parks=21 faces=91,039 | 91,039 / 100k | assert 双 PASS，阶梯未触发 |
| T2 | M4 树密度三级 | `94438b5` | trees 13,210→**27,724**（arr=3,963 offset=3,345 double=7,206，新增 14,514） | 289,129 / 330k | 面数门 + `cars=3764 reeds=366` 零漂双门 PASS |
| T3 | M5 车辆 `hz_v2_vehicle` | `11af753` | kind0=2056 kind1=1364 kind2=212 kind3=132（和=3,764，**4 车型**） | 22,876 / 80k | kind0/1 equal=True、A/B stable PASS |
| T4 | M6 街景家具 `hz_v2_furniture` | `7450315` | Furniture signals=87 bus=2 signs=40 faces=2,181 | 2,181 / 30k | `T4_ALL_PASS` |
| T5 | M7 屋顶细节 `hz_v2_roofdet` | `19a5efe`（修正 `4b07d33`） | RoofDet n=3,233 faces=57,436 | 57,436 / 70k | `T5_ALL_PASS`、dryrun match=True |
| T6 | M8 近景窗几何 `hz_v2_window_geo` | `877ffc4` | WindowGeo n=82 **matched=82/82** windows=4,383 faces=26,298 | 26,298 / 45k | `T6W_ALL_PASS`（缺楼 SystemExit 硬门） |
| T7 | 终检双构建+渲染 | `45a2e71` | 面数账本 / STATS 8 字段写回 / 探针统一重渲 | — | 断言 ×2 轮全绿、`MANIFEST_ALL_DONE` gate=PASS |

### 9.2 面数总账终表（`t7_ledger.py` 按 blend 前缀组实测，基线 `r_p7b2_faces.log` 独立复核）

基线 **1,924,385** → 终态 **2,413,344**，Δ **+488,959** ≤ 总 cap 2,575,000 / 红线 2,600,000 → **PASS ×2**。
running：T1 2,015,424 → T2 2,304,553 → T3 2,327,429 → T4 2,329,610 → T6 2,355,908 → T5 2,413,344（残差记账）。
日夜 parity：day content 2,413,344 = night content 2,413,344；夜景另含 191,716 夜灯具面
（NightLamps/LightPools/LandmarkFloods，按既有惯例不入日景账本，夜 scene_faces=2,605,060）。

### 9.3 断言双 PASS · 渲染参数与耗时

- 断言 ×2 轮（首轮 + 新鲜产物）：脚本层 + Blender 层均 **ASSERT_ALL_PASS**——
  `TURN frames=120 unique=120`、`CORNER DAY/NIGHT TL/TR span=2`（×4）、
  Terrain/Roads/Roads_Primary `mean_nz` = 0.99806 / 0.99869 / 0.99895 ≥ 0.997（×6）。
- STATS 写回（纯打印）：`green_hedge_m=157365 parks=21 signals=87 bus_stops=2
  vehicles=3764 roofdet=3233 wingeo_faces=26298 trees=27724` + 不变项
  （osm_bldg=143 infill=46676 riverfront=2273 roads=362 poke=0.2106% landmarks=4+1
  cars=3764 reeds=366 tex=8packed）。
- 渲染：日主帧 Cycles 256spp OPTIX **15.5s** / 夜 **36.2s**（≤300s 红线）；
  转台 EEVEE **1.43s/帧**（120 帧 171.6s，≤2s 红线）；探针 day=26 / night=26、
  turn=120 → `r_t7_manifest.log` **`MANIFEST_ALL_DONE day=26 night=26 turn=120 gate=PASS`**。
- 交付门禁复跑：`r_t8_syntax.log` exit 0、`r_t8_review.log` REVIEW_SCAN CLEAN。

### 9.4 专项结论（§6 采样）

- **玻璃**：反射对象 win_glass=4,383 / win_frame=17,532 / win_sill=4,383 / veh_glass=15,348
  （M1 shader 复用）；近景日/夜探针（`hz_close_street` 等）玻璃面、框带、朝向色差可见。
- **树簇**：4 色桶 verts [224,614 / 224,488 / 222,180 / 224,458]（差 1.09%）；新增 14,514 株
  全部 lite/median 档、289,129 面；中分树阵（`p8t2_median.png`）/ 双侧补种（`p8t2_double.png`）可读。
- **绿带覆盖率**：hedge 157,365m / 主干四级 206,186m = **76.32%**
  （`r_t7_numbers.log` NUMBERS_ALL_PASS，分母 = §4 主干四级口径）。

### 9.5 阶梯审计（引用 `task-2-report.md` L36-48，控制器 2026-10-09 追认）

- spec 字面满配预演 = 1,254,920 面（≈330k 预算 3.8×）→ 实际采用**阶梯外前置杠杆**
  （L42-45 常量表：`ARR_STEP` 20→40m、`ARR_MIN_WIDTH` ≥16→≥24、`OFF_STEP` 24→48m、
  `DBL_STEP` 15→45m），**等效阶梯 step1+2**；`LITE_CLUSTERS=1`（step1）已应用；
  落点 27,724 ∈ [24k, 28k]、Δ289,129 ≤ 330,000。
- **T1 stripped-only**（控制器裁定仅 4 级主干连续条带，阶梯未触发）；T3-T6 未降级。
- 机会性改动披露：`hz_v2_green.py:65` docstring 1 行（注释级，与 `ROAD_CLASS_PRIMARY` +
  `primary_only=True` 口径一致，非功能变更）。

### 9.6 豁免（waivers，控制器裁定）

| 豁免 | 内容 | 依据 / 代价 |
|---|---|---|
| T1 灌木球延后 | 绿篱仅连续条带，灌木球+草带球本期不建 | 控制器裁定；延后 ≈633k 面；§6「降绿篱密度」许可 |
| T4 平顶雨棚 | 雨棚 = 6 面平盒而非计划字面「斜面 4 面」（+8 面） | 控制器豁免、已披露；目验时用户判，不满意再改（代价 ~1 构建） |
| T5 16m 尺寸门 | 屋顶细节尺寸门 12m→16m（12m 投影 494,881 = 7× 超限） | 控制器授权；过门 39,965→3,233（−88%），概率 40/15/10 未动 |
| T6 浮雕非挖洞 | 窗 = 0.03 proud 浮雕 + 0.11m 框深差 ≈ brief 0.12m，非真内凹 | 墙不挖洞管线所限、代码已披露；无 z-fight（框带 +0.03→+0.14、窗台平出） |

### 9.7 遗留（deferred minors，详单见各 task-report / progress.md）

- T1：灌木球+草带球延后；`hz_v2_green.py:65` docstring 已于 T7 机会性修正。
- T2：offset/double 单一 lite 形态（无逐株树种抽值，已披露）；树与楼体重叠按既有口径接受
  （`valid_ground` 只拦水体/地标圈）。
- T3：lut json 缺 encoding 参数（报告级）。
- T4：雨棚平盒待目验；`r_t4_probe.log` 时间戳与 PNG 不同步（内容刷新仍有效）。
- T5：女儿墙 inset 为 bbox 缩（~112 栋不规则 OSM 楼近似）；地标 4+1 未入细分母；
  `mat_env` 双键同行（300 行帽下披露妥协）。
- T6：几何窗与 M1 砖纹假窗叠层近景可能「双窗」观感（目验判，不满意可做 closeup 楼砖窗抑制）；
  模块级 store 不 reset（单会话二次 build 潜伏）；`FRAME_W` 死常数。
- T7：`r_t7_t7compile.log` 0 字节（审查者 py_compile 复跑 exit 0 补证）。

### 9.8 交付 5 件实测字节（2026-10-11 检查点②刷新，覆盖 ckpt① 旧值）

| 文件 | 旧（ckpt①） | 新（ckpt②） | 来源 |
|---|---:|---:|---|
| `hanzhong.blend` | 77,781,214 | **95,285,816** | `hanzhong_v2.blend` 双 build（`r_p7c_day.log`） |
| `hanzhong_night.blend` | 82,236,660 | **99,742,148** | `hanzhong_v2_night.blend`（`r_p7c_night.log`） |
| `hanzhong_preview.png` | 3,218,789 | **3,234,200** | `t7_probes_day/hz_v2_day.png`（Cycles 256spp 日主帧） |
| `hanzhong_night.png` | 3,602,850 | **3,609,863** | `t7_probes_night/hz_v2_night.png`（Cycles 256spp 夜主帧） |
| `hanzhong_turntable.mp4` | 4,597,876 | **4,638,698** | `hz_turn` 120 帧 ffmpeg 合成；ffprobe h264 / 1920×1080 / 30 fps、nb_frames=120、duration=4.000000s |

`deliverables/README.md` 5 尺寸行 + CHANGELOG 已同步（规则 7）。
