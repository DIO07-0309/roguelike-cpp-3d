# 汉中 P7 检查点②（M3 绿化带 / M4 树密度 / M5 车辆 / M6 街景家具 / M7 屋顶 / M8 近景窗几何）Implementation Plan

日期：2026-10-09。状态：检查点① 8/8 已交付目验，用户指示继续执行。
前置：`docs/superpowers/specs/2026-10-07-hanzhong-p7-detail-design.md` §4/§5/§6/§7（逐节已批），
检查点① 计划 `2026-10-07-hanzhong-p7-checkpoint1.md`，SDD 台账
`.superpowers/sdd/2026-10-07-hanzhong-p7-checkpoint1/`。
工作区：`C:\Users\HP\AppData\Local\Temp\opencode`（$tmp，scratch git 仓，基线 HEAD `798b3fa`）。

## Global Constraints（每个任务的 brief 必须携带，红线不可协商）

1. **面数预算硬红线：总面数 ≤2.6M**（当前基线 1,924,385）。每任务面数增量上限（审查门槛，超限即
   触发本计划的降级阶梯，不得静默越线）：

   | 里程碑 | 面数上限 | 降级阶梯（超限按序执行） |
   |---|---|---|
   | M3 绿化带 | ≤100k | 灌木球间隔 6m→12m → 撤草带灌木球 → 绿篱 sides 降 |
   | M4 树密度 | ≤330k | ①新植树 sides/簇数降档 → ②密度 2.8万→2.4万 → ③次干双侧→单侧 |
   | M5 车辆 | ≤80k | 车窗简化为单面 windshield → 撤尾灯 emission 独立面 |
   | M6 家具 | ≤30k | 路名牌减点 → 公交站推导间隔 300m→600m |
   | M7 屋顶 | ≤70k | 撤 infill 女儿墙 → 撤 infill 太阳能 → 仅 OSM/riverfront 楼 |
   | M8 窗几何 | ≤45k | 每栋窗位 60→40 → 仅朝街两面 |
   | 合计 | ≤655k | 终版总面数 ≤2.575M，为终检余量留 25k |

   EEVEE ≤2s/帧、Cycles 256spp ≤300s（转台可按 spec §7.1 关反射保帧时）。
2. **RNG 契约（最高优先，来自检查点①教训）**：`build_biology` 用单一
   `random.Random(ECO_SEED)` 顺序消费 trees→traffic→reeds。**既有循环的 rng 消费序列必须逐位
   保持不变**——`cars=3764`、`reeds=366` 基线不许漂（reed 位置有效性依赖已消费随机数）。
   一切新增种植/类型选择/抖动**只准用位置哈希派生的 local rng**（范式：`hz_v2_bio.py:84`
   `local = random.Random(int(px*7.9+py*3.1) & 0xFFFF)`），或在既有循环**整体结束后**追加新循环。
   M5 换车型不得增加 eco rng 抽取次数（1 抽/车不变，车型用 local 哈希分流）。M7/M8 不得消耗
   传入的建筑 rng（屋顶/窗几何用 pts 哈希 local rng）。
3. **z 分级抬升常量（spec §5，防 z-fighting）**：路面 0.6 / 人行道 0.72 / 草带 0.65 /
   绿篱 0.65 起（顶再抬 1.0~1.4m 高）/ 贴地物 0.05。草带与人行道、绿篱与车道缘不得同高。
4. **模块边界**：新模块 `hz_v2_green / hz_v2_vehicle / hz_v2_furniture / hz_v2_roofdet /
   hz_v2_window_geo` 各 ≤300 行、函数 ≤40 行；既有文件被编辑后同样 ≤300 行
   （hz_v2_mat.py 现 198 行、hz_v2_bio.py 现 149 行、hz_v2_roof.py 现 149 行——编辑前先算余量）。
   组合优于继承；变量语义化；中文文件一律 Write/Edit 工具。
5. **环境与门禁**：Python 用 `$env:PYTHONIOENCODING='utf-8'; & C:\Users\HP\anaconda3\python.exe "<file>.py>"`
   （禁 python -c/内联）；Blender 长任务必须 `Start-Process ... -RedirectStandardOutput <log>
   -RedirectStandardError <err> -NoNewWindow -PassThru | ForEach-Object { $_.WaitForExit() }`；
   每次提交前跑 `hz_v2_syntax_check.py`（exit 0）+ `hz_review_scan.py`（CLEAN），
   验收输出重定向 `.log` 落盘。
6. **STATS/assert 基线**（`hz_v2_assert.py:105`，Task 7 统一更新实测值）：不变项
   `osm_bldg=143 infill=46676 riverfront=2273 roads=362 poke=0.2106% landmarks=4+1
   cars=3764 reeds=366 tex=8packed`；会变项 `trees=`（M4 上调）、`StreetTrees` 四桶 verts；
   新增字段 `green_hedge_m parks signals bus_stops vehicles roofdet wingeo_faces`
   （Task 7 实测后写死）。
7. **提交纪律**：只在 $tmp 提交
   （`git add -A; git -c user.name=opencode -c user.email=opencode@local commit -m "task-N: ..."`）；
   主仓与建模仓 `C:\Demo\hanzhong-city-3d` 由控制器管理；并行会话文件被 `git add -A`
   卷入无害，但审查 diff 的 base 必须选本计划提交链。
8. **同步（规则 11）**：docs 改动 → robocopy 主仓 `docs\` 到桌面 3D 包；建模仓 docs/plans
   同步本计划（Task 8 收口）；exe 未动不重拷；冻结 2D 包零触碰。
9. **降级决策权**：预算阶梯内自动执行并在报告记录；需要**改变 spec 设计**（如改 40% 概率、
   改树阵株距语义）时 BLOCKED 上报控制器，不擅自改。

---

### Task 1: M3-绿化带系统 `hz_v2_green.py`（中分绿篱 + 路侧草带 + 公园草地）

**目标**：spec §4-M3 三层绿地全部落地，面数 ≤100k，STATS 新增 `green_hedge_m parks`。

**集成点（先读再写）**：
- 数据：`hanzhong_osm.json` 的 `greenspaces[]`（21 多边形）；`medians[]` 为 **0 条**（M0 实测），
  中分绿篱改由道路推导（spec §4 已定道路推导+OSM 补抓双源，medians 空即纯推导）。
- 道路：`hz_v2_roads.py:91 road_width(cls)` + `hz_v2_data.py:36 ROAD_WIDTHS`
  （motorway/trunk/primary=24m、secondary=16m、ROAD_WIDTH_DEFAULT=10m）；
  `ROAD_CLASS_PRIMARY`。宽路定义：`road_width(cls) >= 16.0`。
- 走线：复用 `hz_v2_bio.py:19 iter_road_paths(osm, dem, primary_only)`；
  法线偏移范式 `hz_v2_bio.py:78 normal_at`；重采样 `hz_v2_roads.py:12 resample_polyline`。
- 人行道：`hz_v2_street.py:119 add_sidewalk`（外缘 = 半宽 + 人行道宽，先读确认实际偏移量），
  草带从人行道外缘起算 3.5m。
- 材质：`hz_v2_mat_env.py:196 make_simple_mats(night)` 增 `hedge/grass/park` 三键
  （草绿基调 + WhiteNoise 色斑，无新贴图依赖——`hz_fetch_textures` 不强制，避免网络风险）。
- 接线：`hanzhong_v2_build.py:71 build_streets` 之后调用
  `build_green(osm, dem, mats)`；生物之前（互不依赖）。

**接口与产出**（写进 `hz_v2_green.py`，头部注释声明契约）：
```python
def build_green(osm, dem, mats):
    """三层绿地：中分绿篱(推导)+路侧草带+公园草地。返回统计 dict：
    {"hedge_m": float, "grass_m": float, "parks": int, "faces": int}"""
# 内部层（各 ≤40 行）：
# _hedge_runs(osm, dem)      -> 绿篱条几何（仅 road_width>=16 的 run，中央 1.6m，株球间隔 6m）
# _grass_bands(osm, dem)     -> 人行道外侧 3.5m 贴地草带 ribbon + 灌木球（local rng）
# _park_polys(greens, dem)   -> greenspaces[] 三角扇，逐顶点 dem+0.05，>12 顶点抽稀
```
**几何要点**：绿篱=连续矮墙带（每 10m 段 4 面）+ 每 6m 一个 12 面灌木球；
草带=贴地 ribbon（沿走线 2 三角/段，z=dem+0.65 逐顶点）；灌木球间隔 6m、local rng 微半径。
公园草地用 fan 三角化遇凹多边形风险时按跨列对角线法（greenspaces 21 个已知较小，逐个验证无翻面）。

**步骤**（每步完成打勾）：
- [ ] 1.1 读 `add_sidewalk`/`iter_road_paths`/`greenspaces[]` 实际结构，记录偏移量与顶点格式到报告
- [ ] 1.2 `make_simple_mats` 增 hedge/grass/park 三材质键（夜景不发光）
- [ ] 1.3 写 `hz_v2_green.py`（三层 + `build_green` 汇总），行数 ≤300、函数 ≤40
- [ ] 1.4 `hanzhong_v2_build.py` 接线 + bio_stats 合并打印 `GREEN2 hedge=...m grass=...m parks=21 faces=...`
- [ ] 1.5 冒烟 build（日景），`hz_v2_syntax_check`+`hz_review_scan` 全绿
- [ ] 1.6 **面数门**：build 后总面数 - 1,924,385 ≤100k，否则按阶梯降级并记录
- [ ] 1.7 截图专项：宽路中央绿篱、草带、公园各一张探针帧（probe 脚本入 $tmp，本地 rng）

**验收步骤**：
- [ ] `GREEN2` 打印值 + 总面数 + `hz_v2_assert` 仍 `ASSERT_ALL_PASS`（基线项未漂）→ 报告贴 log 摘录
- [ ] 台账由控制器记录；提交 `git -c ... commit -m "task-1: M3 green belts hz_v2_green (hedge/grass/park)"`

**风险**：人行道外缘偏移量与行道树 2.5m 重叠（hz_v2_bio.py:79 树在 +2.5m）——草带内缘从
半宽+2.5m 起（树植于草带内缘，视觉成立）；报告中必须写明实测偏移链。

---

### Task 2: M4-树密度三级补种（树阵 / 错位乔木 / 双侧补种）+ 面数阶梯

**目标**：spec §4-M4 总数 13,210 → **~2.8 万**（目标），面数增量 ≤330k；`trees=` STATS 上调，
`cars=3764 reeds=366` **零漂移**（RNG 契约，Global 2）。

**集成点**：
- `hz_v2_bio.py:69 build_street_trees`（主干单侧 12m、离缘 2.5m、`TREE_SPACING=12.0`）、
  `:135 build_biology`（ECO 单流 rng，顺序 trees→traffic→reeds）、`:84 local rng` 范式。
- `hz_v2_tree.py:72 add_tree(trunks, canopies, x, y, z, sel, rng)`；`_bucket` 4 色桶。
- `iter_road_paths(primary_only=True)` 覆盖 motorway/trunk/primary/secondary；双侧补种
  ③ 需要 **cls 级**遍历（该函数不吐 cls → 直接读 `osm` 道路结构，Task 1 已记录其字段格式，
  报告引用之）。

**三级补种（全部 local rng，不碰 ECO 主流；新循环不得消费 ECO 流的 rng）**：
1. **中分带树阵**：`road_width>=16` 的 run 中央（offset 0），株距 20m、相位 local 哈希；
   树种 local 抽（塔柏优先——中分带语义）。
2. **路侧绿带乔木**（与行道树错位）：草带外缘（半宽+2.5+1.75m），株距 24m，
   相位对既有行道树 +6m 错位；仅已有行道树的 run 上补（树种 local 抽、冠型用矮档）。
3. **次干/支路双侧补种**：cls=secondary 补缺失侧；cls∈(tertiary,residential) 双侧；
   株距 15m；离缘 2.5m。小树档（单簇矮冠）控面数。

**面数分档（Global 1 阶梯的实现）**：新植树按 `tier` 走 `hz_v2_tree` 的
**低配档**——`hz_v2_tree.py` 增 `add_tree_lite(...)`（主干 beam + 1~2 簇 + dome sides 5，
目标 ≤24 面/棵）；中分树阵可中配（≤40 面/棵）。既有 13,210 棵的生成路径**字节级不动**
（新档只给新循环调用）。构建打印
`Green2Trees arr=.. offset=.. double=.. total=.. lite_faces=..`。
预算算式（写进报告）：`inc ≈ n_arr*40 + n_off*24 + n_dbl*24`；若 `inc>330k`
按序降级：lite 档簇数 2→1 → 总数压到 2.4 万 → 次干双侧退单侧，并记录实际落点。

**步骤**：
- [ ] 2.1 报告记录 osm 道路结构字段（Task 1 已读，直接引用；不足再读）与现有 13,210 的类别分布
- [ ] 2.2 `hz_v2_tree.py` 增 `add_tree_lite`（≤40 行新函数，不动 `add_tree` 主路径）
- [ ] 2.3 `hz_v2_bio.py` 追加 `build_extra_trees(osm, dem)`（独立 local 派生流，
      在 `build_biology` 中于 `build_street_trees` 之后调用；**不得消费 ECO rng**）
- [ ] 2.4 汇总打印 + STATS 候选值写入报告；语法+扫描门
- [ ] 2.5 **双门**：(a) 面数 inc ≤330k；(b) assert 日志 `cars=3764 reeds=366` 零漂
      （成败门，缺一不可）
- [ ] 2.6 冒烟截图：中分树阵、双侧补种路各一张探针帧

**验收步骤**：
- [ ] assert `ASSERT_ALL_PASS` + `trees=` 新值（2.4万~2.8万区间先过，阶梯落点写明）
- [ ] 面数/车辆/芦苇三数字贴报告 → 提交 `task-2: M4 tree density tiers (arr/offset/double)`

**风险**：`iter_road_paths` 若吞 cls 则③退化为按 `osm` roads 原始结构重建走线（Task 1 报告
已有字段实录）；补种与建筑重叠靠 `valid_ground` 挡水体、楼区重叠按现有惯例接受，报告注明。

---

### Task 3: M5-车辆升级 `hz_v2_vehicle.py`（车窗 / 尾灯 / 4 车型）

**目标**：spec §4-M5——车窗玻璃（复用 M1 玻璃 shader 独立小格）、尾灯 emission（夜景亮）、
车型 2→4（+公交/货车）。`cars=3764` 不变；面数增量 ≤80k；`vehicles` 进 STATS。

**集成点**：
- `hz_v2_bio.py:48 add_car(verts, faces, p0, p1, z0, kind)`（kind 0=轿车 1=面包/SUV，
  car_a/car_b 两合批对象）；`:91 build_traffic`（每 80m、`use_a = rng.random()<0.6` 每车
  恰好 1 抽——**抽数不许变**）；`:146-147` `Traffic_A/B` 用 `mats["car_a"]/["car_b"]`。
- M1 玻璃：`hz_v2_glass.py` shader 组装（spec §5：**同一组装函数供 facade 与车窗共用**）。
- 材质：`veh_glass`、`veh_lamp`（night emission）进 `make_simple_mats`（night 参数已有）。
- 合批体系：先读 `make_mesh_object` 与车体是否支持逐面材质（`fms`）；
  不支持则玻璃/尾灯拆独立合批对象（`Traffic_Glass`/`Traffic_Lamp`），不重构合批系统。

**设计（几何、≤40 行/函数）**：
- `hz_v2_vehicle.py` 从 `hz_v2_bio.py` **拆出** `add_car` 及新车型（bio 改 import，
  签名不变、行为前向兼容）：kind 2=公交（10m 高顶厢）、kind 3=货车（驾驶舱+货厢）。
- **车型分流不加 ECO 抽取**：`kind` 由「既有 0/1 结果 ⊕ `hash_local(p0)` 的 1 bit」得到
  （原 0→0/2，原 1→1/3，新车型占比 ~10%）；`use_a` 单抽结构与既有分布逐位不变。
- 车窗：每车 windshield + 侧窗 quad（M1 玻璃实例、独立小格 UV）；
  尾灯：车尾 2 个 emission 小 quad（night=on，day=暗红）。
- 面数估算：轿车 +12~16、新车型 +24~40（~10% 占比）→ 3764×18+376×32 ≈ 79k ≤80k；
  超限按 Global 1 阶梯降级。

**步骤**：
- [ ] 3.1 读 `make_mesh_object`/合批材质机制，确定玻璃/尾灯落点——报告写明结论
- [ ] 3.2 写 `hz_v2_vehicle.py`（4 车型 + 玻璃/尾灯辅助 ≤40 行/函数），bio 改调用
- [ ] 3.3 材质键 `veh_glass`（复用 M1 玻璃组装）/ `veh_lamp`（night emission）
- [ ] 3.4 **RNG 验证门**：构建 log `Traffic: 3764`、`Reeds: 366` 零漂 + 面数 inc ≤80k
- [ ] 3.5 夜景冒烟帧：尾灯 emission 可见、车窗无黑块；语法+扫描门

**验收步骤**：
- [ ] assert 全绿 + `vehicles=3764 (kind0=.. kind1=.. kind2=.. kind3=..)` 计数打印 → 报告贴数
- [ ] 提交 `task-3: M5 vehicle upgrade hz_v2_vehicle (glass/tail-lights/4-kinds)`

**风险**：独立合批对象使 Traffic 从 2 个 → 最多 6 个对象（可接受，报告记录）；
车窗透射开销：M1 已全城玻璃无帧时爆炸，预计无感，超 2s 按 Global 1 处理。

---

### Task 4: M6-街景家具 `hz_v2_furniture.py`（信号灯 / 路名牌 / 公交站 / 井盖贴图）

**目标**：spec §4-M6 四件套落地，面数 ≤30k；STATS 新增 `signals bus_stops`（实测打印）。

**集成点**：
- 数据（M0 已入库）：`signals[]` 87 点、`bus_stops[]` 2 点；缺失降级策略（spec §3-M0）：
  bus 缺 → 按主干 300m 推导；signals 只在真实节点摆（不推导，spec §4）。
- 道路：同 Task 1 的 `iter_road_paths`/`road_width`；路名牌挂最近主干/次干**路口**。
- 贴图分块：`hz_v2_mat.py:160 make_road_mat`（现 198 行，余量 102 行——井盖/修补只能
  **+≤60 行**，超了就把辅助函数移进 `hz_v2_furniture.py` 反向 import 不允许，
  改放 `hz_v2_green.py`？**否**——放 `hz_v2_furniture.py: make_road_patches(nt, uv, width)`
  然后 `hz_v2_mat.py` 一行 import + 一行调用）。
- 接线：`hanzhong_v2_build.py` 中 `build_streets` 后调用
  `build_furniture(osm, dem, mats)`；井盖层在 `make_road_mat` 内部（无需接线）。

**四件几何（各 ≤40 行/函数，全部位置哈希 local rng）**：
1. **信号灯**（`signals[]` 87）：杆（8 面柱）+ 三灯箱（盒 6 面）+ 三灯面
   （红黄绿 emission 小 quad，day 常亮红/绿其一，night emission）；路口朝向=最近道路切向。
2. **路名牌**：杆 + 板（蓝底白字用纯色板——Raylib 文字不进 3D，贴图不引）；
   点位=主干/次干相交最近点（从 `osm` roads 两两求最近点，取 top N=40 控面数）。
3. **公交站台**：`bus_stops[]` 2 点 + 按主干 300m 推导补点（`iter_road_paths(primary)`
   重采样 300m 取点、去重 150m）；雨棚（斜面 4 面）+ 柱×2 + 长凳（盒 6 面）。
4. **井盖与路面修补贴图分块**：`make_road_mat` 内加 Voronoi/WhiteNoise 门控色块
   （噪声阈值混入深灰补丁色 + 一个圆形井盖 mask），**纯 shader 零面数**；
   UV=沿路米制已有（`hz_v2_mat.py:161` 注释），井盖沿 U 周期排布。

**步骤**：
- [ ] 4.1 读 `make_road_mat` 全文与 `make_simple_mats` 键位；记录 `signals/bus_stops` 顶点格式
- [ ] 4.2 写 `hz_v2_furniture.py`（`build_furniture` 汇总 + 四件生成函数）
- [ ] 4.3 `make_road_mat` 补丁层（辅助函数放 furniture 模块，mat 侧仅 import+调用，总行数 ≤300）
- [ ] 4.4 材质键 `sig_pole/sig_lamp/sign_board/bus_roof`（进 `make_simple_mats`）
- [ ] 4.5 `hanzhong_v2_build.py` 接线；打印 `Furniture signals=87 bus=.. signs=.. faces=..`
- [ ] 4.6 语法+扫描门 + 冒烟 build：面数 inc ≤30k + assert 全绿（基线项不漂）

**验收步骤**：
- [ ] 探针帧：一个信号灯近景 + 路面井盖/补丁可见性各一张；报告贴 `Furniture` 行与面数
- [ ] 提交 `task-4: M6 street furniture hz_v2_furniture (signals/signs/bus/road-patches)`

**风险**：路名牌两两路口求交 O(n²)——roads=362 条，直接双循环 ≤13 万次比较可接受；
若含曲线段取首尾 20% 采样点求近即可，报告写明算法。

---

### Task 5: M7-屋顶细节 `hz_v2_roofdet.py`（太阳能 / 烟囱水箱 / 女儿墙）

**目标**：spec §4-M7——平顶太阳能板阵列（朝南倾角、行列）、烟囱/水箱、女儿墙压顶环；
坡顶屋脊压顶 + 烟囱。面数 ≤70k；STATS 新增 `roofdet` 计数。

**集成点（先读再接）**：
- `hz_v2_roof.py:138 attach_roof(verts, faces, uvs, pts, z1, height, rng, fms, rtype)`；
  `:36 pick_roof`（kind∉(gable,hip,mansard)→flat）；`fms` 材质索引体系。
- 调用点：`hz_v2_bldg.py:85` 与 `:206`（两处——OSM 楼与程序楼，行号实施时复核）。
- **规格解释（控制器已定，写进报告）**：spec「平顶楼 40%」的**楼 = 有身份的楼**——
  OSM(143)+riverfront(2273)+landmark + infill 中**屋顶短边 ≥12m 者**；
  小顶程序楼（板房烟囱感）不加，防面数爆炸（46676 个 infill 全加必超 70k）。
  概率/占比仍按 spec：太阳能 40%、烟囱水箱 15%、坡顶压顶+烟囱 10%——分母=过尺寸门的楼。
- **RNG**：`attach_roof` 传入的 `rng` 是建筑生成流——**不许多抽**；
  细节决策一律 `random.Random(hash(pts))` 位置哈希 local 派生。

**接口**：
```python
def add_roof_details(verts, faces, uvs, pts, z1, height, fms, rtype):
    """屋顶细节后置钩子。pts=屋顶多边形, z1=屋顶高, rtype=pick_roof 结果。
    仅对过尺寸门(短边>=12m)的楼生效；内部 local rng。返回新增面数(供统计)。"""
```
- 接线：`hz_v2_roof.py` 文件末尾 `attach_roof` return 前**一行**调用
  （import 置于文件头，防环：roofdet 不 import roof）。
- 太阳能：朝南（-Y 或实测汉中朝向，报告注明）倾角 15°，2×3 阵列 = 6 板，
  每板 1 quad + 共用边框（合计 ≤16 面）；行列沿屋顶长边。
- 烟囱/水箱：无底盒复用 `hz_v2_roof.py:116` 的盒辅助（同文件已有，直接调用——
  读其名，实施时按实名）；女儿墙压顶：沿屋顶轮廓内缩 0.3m 一圈（每顶点 4 面）。

**步骤**：
- [ ] 5.1 读 `attach_roof` 全文 + 两个调用点 + 盒辅助函数名；统计「短边≥12m 楼数」与
      过门后 40%/15%/10% 的期望面数（算式进报告）
- [ ] 5.2 写 `hz_v2_roofdet.py`（`add_roof_details` + 太阳能/烟囱/女儿墙子函数 ≤40 行）
- [ ] 5.3 `hz_v2_roof.py` 头 import + `attach_roof` 尾一行钩子（含返回面数累计打印）
- [ ] 5.4 材质键 `solar_panel`（深蓝微金属+粗糙 0.2）、`roof_tank` 进 `make_simple_mats`
- [ ] 5.5 语法+扫描门 + 冒烟 build：inc ≤70k、assert 全绿、`RoofDet n=.. faces=..` 打印

**验收步骤**：
- [ ] 探针帧：一张带太阳能板阵列的屋顶近景（日光下倾角/行列可辨）
- [ ] 提交 `task-5: M7 roof details hz_v2_roofdet (solar/chimney/parapet via hook)`

**风险**：infill 短边≥12m 数量未知——5.1 的**先统计后实现**是硬顺序，统计若显示 40%
会破 70k，则把尺寸门提到 ≥16m（同号语义不改 spec 概率，报告差异）。

---

### Task 6: M8-近景窗几何 `hz_v2_window_geo.py`（82 栋真实窗洞）

**目标**：spec §4-M8——`closeup_buildings.json` 82 栋每窗内凹 0.12m 玻璃 quad + 4 框条 +
窗台（5~8 面/窗），总预算 **3~5 万面**（本计划上限 45k）；格尺寸与 M1 同契约。

**集成点**：
- 清单：`$tmp/closeup_buildings.json` = `{"anchors":[[x,y]×5], "buildings":[{i, pts, h, anchor}]}`。
  `i`=池序号，`pts`=屋顶轮廓（Task 4 定稿：h=实建墙高、双跑 SHA256 全等）。
- 池：`closeup_pool.json` bodies=49,092（143 OSM+46676 infill+2273 riverfront 同池）。
- **匹配方式（防序号漂）**：build 内以**轮廓匹配**——建筑生成处对 footprint 归一化哈希
  （顶点四舍五入 0.1m、起点取序、算 tuple 哈希）与清单预计算哈希集求交；
  **不信任生成顺序 i**（生成顺序与池序的对应仅 Task 4 单次验证过）。
  验收门：匹配命中数 = 82，缺一即 FAIL。
- M1 格契约：3m 层高、格宽高沿用 brick 参数（grep `_window_grid` 四元组，hz_v2_mat/glass 内）；
  玻璃复用 `hz_v2_glass.apply_glass` 同一 shader 组装（spec §5 共用契约）。
- 接线：OSM 楼（`hz_v2_bldg.py` 体生成处）、riverfront、infill 三个体发射点各**一行**
  `window_geo.maybe_add(pts, z0, height, fms)`（实施时 grep 体发射点真实位置）；
  或（若三点太散）在 `attach_roof` 同款后置钩子上做——**钩子优先，散点兜底**，报告写明选择。

**几何与预算（关键决策，控制器已定）**：
- **每栋最多 60 窗**：优先朝街面（法线指向最近道路一侧）+ 两相邻面；
  窗位=层行 × 格列（M1 契约），行/列满 60 截断。
- 每窗：内凹玻璃 quad（1 面）+ 上下左右 4 框条（条=quad，共 4 面）+ 窗台（1 面）= 6 面；
  玻璃/框/台分 3 材质（`fms` 索引或独立合批——与 Task 3 同结论复用）。
- 预算验证：`82×60×6 = 29.7k ≤45k ✓`；若平均窗更大（≥70/栋）按 60 封顶不放宽。
- 建筑高度 h 已知，层行数 = `int(h/3)`；底层是否落地窗：从 1 层起全算（spec 未排除）。

**步骤**：
- [ ] 6.1 读 `closeup_buildings.json` + `_window_grid` 契约 + 三个体发射点位置；
      预计算 82 轮廓哈希，写进 `hz_v2_window_geo.py` 模块常量或启动时读清单
- [ ] 6.2 写 `hz_v2_window_geo.py`（`maybe_add` + 窗行生成 + 框条/窗台辅助 ≤40 行/函数）
- [ ] 6.3 接线（后置钩子或散点一行）+ 材质键 `win_frame/win_sill`（玻璃复用 M1）
- [ ] 6.4 打印 `WindowGeo matched=82/82 windows=.. faces=..`——**matched≠82 即 FAIL**
- [ ] 6.5 语法+扫描门 + 冒烟 build：inc ≤45k、assert 全绿、基线项不漂

**验收步骤**：
- [ ] 探针帧：一栋近景楼的窗洞立面（内凹阴影、框、窗台可辨——日光斜射机位）
- [ ] `matched=82/82` + 面数贴报告 → 提交 `task-6: M8 window geo hz_v2_window_geo (82 closeups)`

**风险**：轮廓哈希若因 infill 生成随机性与池不同源（池是从 blend 还原的）——
Task 4 已证「池↔blend 逐体对应」，且 blend 由 build 产出，build 侧同源；
若匹配失败先查归一化容差（0.1m→0.5m），再查起点序，报告分步记录，禁止静默放宽到无界容差。

---

### Task 7: ★检查点② 终检（全量双 build + 双渲 + 转台 + 七视角 + 近景专项 + assert）

**目标**：spec §4-★终检前半——一次性验收 M3-M8 全部成果，产出终版渲染包与全部证据 log。

**步骤**：
- [ ] 7.1 **全量双 build**：日/夜 build 各一次（标准命令，`Start-Process` 重定向
      `r_p7c_day.log`/`r_p7c_night.log` + err），`BUILD_DONE` 收尾
- [ ] 7.2 **STATS/assert 基线更新**：按 Global 6——把实测值写回 `hz_v2_assert.py`
      （`trees=` 新值、`green_hedge_m parks signals bus_stops vehicles roofdet wingeo_faces`
      新字段、`cars=3764 reeds=366` 等不变项原样），跑 conda+Blender 双侧 assert
      → `ASSERT_ALL_PASS` ×2
- [ ] 7.3 **面数总账**：贴 build 总面数 vs 1,924,385 增量分解（M3..M8 各实际增量 vs 各上限表），
      总数 ≤2.6M 红线；超线则按 Global 1 阶梯降级后重跑 7.1-7.3
- [ ] 7.4 **渲染包**：日主帧 + 夜主帧 Cycles 256spp（GPU OPTIX 路径，
      `hz_v2_render.py` 现网配置）、转台 120 帧（EEVEE，≤2s/帧门；超线按 spec §7.1
      转台关反射）、七视角（`hz_v2_views.py` VIEWS_DONE）
- [ ] 7.5 **近景专项**：`hz_close_street/hz_close_roof` 刷新 + 新增探针帧
      （绿篱/树阵/信号灯/太阳能屋顶/近景窗立面——Task 1-6 各自探针可复用，统一重渲到终版）
- [ ] 7.6 **专项数字采样**（spec §6 要求）：玻璃反射可见（沿用 M1 探针法）、树簇计数、
      绿带覆盖率（`hedge_m / 主干总长`，落 log）、每项一段结论写报告
- [ ] 7.7 门禁终跑：`hz_v2_syntax_check` exit 0 + `hz_review_scan` CLEAN（全部 hz_*）

**验收步骤**：
- [ ] 证据矩阵齐全：2 build log + 2 assert log + 渲染 log×4 + 7.3 面数总账 + 7.6 专项数字
- [ ] 全绿无 FAIL → 提交 `task-7: checkpoint-2 acceptance build+renders (M3-M8 verified)`
      （探针脚本与 log 落 $tmp 提交；渲染 PNG/MP4 被 .gitignore 排除属预期）

**风险**：终检才发现超线 → 阶梯降级重跑代价大，故 Task 1-6 每步的 inc 门就是本任务的
前置防线；7.3 总账若单项越线，责任回到对应任务的修复（审查者按此归因）。

---

### Task 8: 终版交付刷新 + report §9 + README + 归档 + 双仓同步

**目标**：spec §4-★终检后半——交付物、文档、归档、同步一次收口（检查点② 完成）。

**步骤**：
- [ ] 8.1 **交付 5 件刷新**（`C:\Users\HP\Desktop\deliverables\hanzhong\`）：
      `hanzhong_preview.png`（日）/`hanzhong_night.png`（夜）/`hanzhong_turntable.mp4`
      （ffmpeg 120 帧合成，ffprobe 校验）/`hanzhong.blend`+`hanzhong_night.blend`
      （双 build 产物复制）；沿用检查点① 的「Task 7 已渲可跳过」豁免条款
- [ ] 8.2 **`report.md` §9 追加**（终版章节，Edit 工具追加，不改 §1-§8B 历史字节）：
      M3-M8 各验收数字（GREEN2/树密度阶梯落点/vehicles/Furniture/RoofDet/WindowGeo）、
      面数总账终表、断言双 PASS、渲染参数与耗时、专项结论（玻璃/树簇/绿带覆盖率）
- [ ] 8.3 **`deliverables/README.md` 5 尺寸行**按实测刷新（Edit 工具）
- [ ] 8.4 **SDD 归档**：本计划的 briefs/reports/progress 由控制器收口后，
      实现者仅负责把 `scripts/` 增量（Task 1-6 新增/修改的 hz_*.py 与
      `hz_v2_syntax_check` 检查通过）同步进 **建模仓** `C:\Demo\hanzhong-city-3d\scripts\`
      （覆盖式复制）+ `docs/plans/2026-10-09-hanzhong-p7-checkpoint2.md` +
      `data/` 若有更新（closeup/OSM 未变则不动）
- [ ] 8.5 **建模仓提交推送**：`git add -A && git commit -m "checkpoint-2: M3-M8 scripts + plan + data" && git push`
      （cwd=建模仓；这是 docs/脚本的**权威归宿**，对应用户 2026-10-09 指示）
- [ ] 8.6 **桌面镜像（规则 11）**：robocopy 主仓 `docs\` → 桌面 3D 包 `docs\`
      （计划②/台账增量）；src/resources 未动 → **不重拷 exe**；冻结 2D 包零触碰
- [ ] 8.7 **验证清单贴报告**：交付 5 件大小/时间戳、建模仓 push SHA、robocopy 计数、
      主仓 `$tmp` 提交链完整 `git log --oneline`

**验收步骤**：
- [ ] 8.2/8.3 编辑后 grep 自检（新增节存在、旧节未损、中文字节完好——**一律 Edit/Write 工具**，
      禁 Set-Content/Add-Content，2026-10-09 已发生一次 GBK 损坏事故）
- [ ] 建模仓 `git status` clean + push 成功 → 提交 `task-8: checkpoint-2 delivery refresh + repo sync`

**风险**：并行会话可能同时改桌面/主仓——8.6 robocopy 前先 `git status` 记录主仓现场，
只镜像 docs 不动其它；交付目录只覆盖 5 件目标文件，不做整目录镜像。

---

## Self-Review 记录

- 2026-10-09 计划初稿完成（控制器自审）：8 任务与 spec §4 逐行对齐；Global 1-9 覆盖
  检查点① 全部教训（RNG 单流契约、面数上限表、Edit/Write 编码纪律、提交纪律、
  建模仓归宿）。**两处控制器决策已显式标注**（M7「楼」的尺寸门解释、M8 每栋 60 窗封顶），
  均为 spec 预算（3-5 万面 / ≤70k 上限）的实现级解释，不改设计语义——
  实施者发现冲突时按 Global 9 BLOCKED 上报。
- 检查点① 期间 brief 缺陷率（FRACT 枚举/元组契约等 3 处）提示本计划在 Task 3/5/6
  集成点显式给出「先读某函数全文再接线」的硬顺序，减少臆测接口。


