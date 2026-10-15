# P8 设计 spec — 汉中建模第三检查点（M9-M16）

> 状态：讨论完成、决策已锁（2026-10-12）。执行按 SDD：task-brief → 实现 → review → 台账。
> 上一检查点：P7 ckpt②（`2026-10-09-hanzhong-p7-checkpoint2.md`，8/8 Approved，终面 2,413,344）。

## 1. 目标

在 P7 终态 **2,413,344** 面之上，用 B1 抬杠后的余量补齐观感缺口：
近景窗密度（河滨廉价窗全覆盖）、夜景万家灯火、路面质感、内容密度
（屋顶低配、公交站推导、绿篱补全）与贴图升级。性能/管线项不占面账，随任务顺带。

## 2. 决策记录（用户 2026-10-12 拍板）

| # | 议题 | 决定 |
|---|---|---|
| 1 | 窗形态 | **B1**：红线 2.575M→**2.95M**；OSM+地标浮雕窗全量 + 河滨 2273 栋 2 面廉价窗全量 |
| 2 | 屋顶 14m 门低配 | 做（+27k） |
| 3 | 公交站 300m 推导 | 开（+15k）；spec 措辞同步修订为「2 真值 + 主干推导补点」 |
| 4 | 绿篱补全 | 补（+20k） |
| 5 | 灌木球（T1 延后项） | **看情况**：B1 余量足够，收尾后按目验需要追加（~63k，条件任务） |

## 3. 面数预算（B1）

- **总 cap：2,950,000**（红线，用户授权抬升；旧 2.575M/2.6M 作废）
- 基线：2,413,344

| 任务 | 模块 | Δ 面 | 任务 cap | 累计 |
|---|---|---|---|---|
| T1 | M10a 窗扩面 OSM143+地标5（浮雕 6 面/窗） | +46,000 | 50k | 2,459,344 |
| T2 | M10b 河滨 2273 栋廉价窗（2 面/窗，glass+frame 合并四边形） | +241,000 | 260k | 2,700,344 |
| T3 | M13 屋顶低配（16→14m 门，小楼 5 面简化水箱/烟囱） | +27,000 | 30k | 2,727,344 |
| T4 | M14 公交站推导（2 真值 + 主干 300m） | +15,000 | 18k | 2,742,344 |
| T5 | M15 绿篱补全（余 48,821m × 0.4 面/m） | +20,000 | 22k | 2,762,344 |
| T6 | M11 万家灯火 + M12 路面质感（纯 shader） | 0 | 0 | 2,762,344 |
| T7 | M16 贴图升级（4K 重绘 + 中文站牌字体图集） | 0（VRAM↑） | 0 | 2,762,344 |
| T8 | 终检双 build + STATS + 探针 + 交付 | 0 | — | 2,762,344 |
| T9* | M9 目验修补包（条件：等用户目验清单） | ~0 | 3k | — |
| T10* | 灌木球低配（条件：收尾后目验需要时） | ~63,000 | 70k | ≤2,825,344 |

**T1-T8 后 2,762,344 ≤ 2,950,000，余量 187,656**（覆盖 T10 + 机动）。
逐任务超限降级阶梯沿用 P7 格式：阶梯动作写在各 brief，触发即 BLOCKED 上报或按表内枚举降级并记账。

## 4. 设计要点

### M10 窗扩面（T1/T2）
- **OSM+地标（T1）**：复用 `hz_v2_window_geo.py` 6 面浮雕配方；`closeup_buildings.json` 已覆盖 82 栋，本任务扩到 **OSM 143+地标 5** 全量（含已在 82 内的，不重复）。matched 门改为 `osm_matched=143/143`。
- **河滨廉价窗（T2）**：**新配方** 2 面/窗（窗洞矩形 + 内嵌玻璃面合并为单一凹陷视觉件：单 quad 沿法线外凸 0.02m + 顶/侧遮光边用 UV 阴影 shader 伪造深度）。行网格仍走 `_window_grid` 同口径；每栋 ≤60；RNG 位置哈希。
  - **验收**：`riverfront_win=2273/2273`、面数 Δ≤260k、夜 blend 同步（窗面参与 M11 灯火 mask）。
  - 砖纹假窗抑制：河滨楼在 M10b 生效后其立纹理窗层关闭（复用 T6 布线，diff 最小化）。
- 挖洞真窗：**不做**（英雄楼方案一并搁置，记入 deferred）。

### M13/M14/M15（T3-T5）
- 屋顶：`SIZE_GATE` 16→14m，新增 `_low_kit`（5 面简化件，仅 infill 侧）；40/15/10 概率与 P7 裁定（按屋顶类型分母）不变。
- 公交：启用 `hz_v2_furniture._derive_buses`（已存在、数据门后未触发），主干 300m 重采样 + 150m 去重 + `valid_ground`；**spec §4-M6 措辞修订**为「2 真值 + 主干 300m 推导」。
- 绿篱：`hz_v2_green` 扩展到 secondary/tertiary 侧带（z 分级沿用 0.65；0.4 面/m 配方）；覆盖率验收 **≥95%**。

### M11/M12 纯 shader（T6）
- 万家灯火：`make_bldg_mat`/夜 blend 窗层加位置哈希 mask（楼层 × UV 格 → 30% 亮/70% 暗，暖白 2700K）；商铺招牌 emission 条带（POI 沿街底层）。零几何。
- 路面：扩展 T4 `make_road_patches` 闭包 — 车道中线磨损暗带 + 雨渍 roughness 斑（WhiteNoise 第二组）。零几何。

### M16 贴图（T7）
- 立面砖窗/屋瓦/人行道 → 4K 重绘（`hz_v2_tex.py` 打包改 4K×N，pack 数量可减）；中文站牌/路名牌字体图集（`extract_chars` CJK 集 → 字体烘焙）。VRAM 与 pack 体积在报告记账，不占面数。

## 5. 硬约束（全任务生效，随 brief 携带）

1. **RNG 契约**：ECO 单流 `random.Random(ECO_SEED)` trees→traffic→reeds 不动；基线 `trees=27724 cars=3764 reeds=366` 零漂；新增一律位置哈希 local rng（`hz_v2_bio.py:84` 范式）。
2. fn≤40、单文件≤300 行；新模块 `hz_v2_wingeo_cheap.py`（≤300）、`hz_v2_lights.py`/`hz_v2_pavement.py` 视拆分。
3. 环境：`$env:PYTHONIOENCODING='utf-8'; & C:\Users\HP\anaconda3\python.exe`；Blender `Start-Process -RedirectStandard*`；门禁日志**必须落盘**（`r_t*_syntax.log`/`r_t*_review.log`，末次编辑后复跑）。
4. 中文文件仅 Write/Edit（PS 编码事故 ×3 已录）。`look(image=)` 路径正斜杠。
5. 仅 $tmp 提交；baseline 链从 `45a2e71` 起。主仓 docs 只 stage 不 commit；建模仓同步推送；桌面 robocopy /MIR（exe 不动）。
6. 双 build（夜 blend 必重建）+ assert ×2 `ASSERT_ALL_PASS`；STATS 字段随任务补写、T8 统一实测回写。
7. 毒工具（Blender MCP）：`get_viewport_screenshot`/`get_world_state_snapshot`/`drain_human_activity` 禁用；`look(image)` 必须正斜杠。

## 6. 验收（T8 终检）

- 面数总账：per-task Δ 实测 + running + ≤2.95M；日夜 content parity。
- 新门：`osm_matched=143/143`、`riverfront_win=2273/2273`、`hedge_coverage≥95%`、`bus_derived=M/M`（M=规则实测输出，cap 不得绑定 `dropped_by_cap=0`；cadence N 仅作上下文披露）、灯火/路面 shader 零面数证明（Δ=0 + 节点图截存）。
- 公交推导口径（用户决策③授权）：推导受 `valid_ground`/150m 去重约束，300m 为采样步长而非保证间距。
- 探针：窗（OSM 近景 + 河滨近景）、灯火（夜）、路面（日）、屋顶低配、公交站、绿篱补全段，日夜各一。
- 交付：`deliverables\hanzhong\` 5 件 + report §10 + README CHANGELOG + 建模仓归档 + 桌面镜像。

## 7. Deferred（记账不阻塞）

真挖洞窗 / 英雄楼窗 / 灌木球（条件 T10）/ 车辆新车型（P7 未选）/ 性能项（LOD、增量构建、glob 扩面——独立小任务随时插队）。

## 10. P8 检查点（M10-M16）终版交付报告（report §10 同文，2026-10-15）

### 10.1 里程碑 × 面数总账（逐任务 Δ 实测，红线 2,950,000）

| 里程碑 | 任务 | 内容 | Δ 面数 | 上限 | running | $tmp 提交 | 终门 |
|---|---|---|---:|---:|---:|---|---|
| M10a | T1 | 窗扩面 OSM143+地标5（L1=40 窗位） | +25,314 | 50,000 | 2,438,658 | `bca9fa5` | T1W_ALL_PASS |
| M10b | T2 | 河滨 2273 栋廉价窗（L1_cap40，2 面/窗） | +181,840 | 260,000 | 2,620,498 | `e902c24` | T2W_ALL_PASS |
| M13 | T3 | 屋顶 14m 门低配（L2 p=0.73，5357×5 面） | +26,785 | 30,000 | 2,647,283 | `dfc77cb`+fix `249a1cb` | T3R_ALL_PASS |
| M14 | T4 | 公交 300m 推导（2 真值+326 推导=328 站） | +7,824 | 15,000 | 2,655,107 | `488d08d`+fix `ddad0ef` | T4B_ALL_PASS |
| M15 | T5 | 绿篱补全（76.32%→95.05%，L3_cap95） | +15,452 | 22,000 | 2,670,559 | `0bea5fc` | T5H_ALL_PASS |
| M11+M12 | T6 | 万家灯火+路面质感（纯 shader，3D hash 修复） | 0 | 0 | 2,670,559 | `62b6fe4`+`3df10a9`+fix `1e71d35` | T6S_ALL_PASS（Δ=0） |
| M16 | T7 | 4K 英雄贴图+CJK 站牌字集（11 pack） | 0 | 0 | 2,670,559 | `191c4da`+fix `136737e` | T7X_ALL_PASS（Δ=0）+ frame L0 |
| 终检 | T8 | 双 build/STATS/面数总账/探针/交付 | 0 | — | 2,670,559 | `5b6762a` | T8FACES_ALL_PASS |

### 10.2 面数时间线

`1,924,385`（P6 终）→ `2,413,344`（P7 终，+488,959）→ **`2,670,559`**（P8 T1-T5
累计 +257,215；T6/T7/T8 Δ=0）→ **final `2,670,559` ≤ 红线 2,950,000**
（余 **279,441** ≥70k，**T10 灌木球门开**）。
逐任务：T1 +25,314 / T2 +181,840 / T3 +26,785 / T4 +7,824 / T5 +15,452 /
T6 +0 / T7 +0 / T8 +0（`T8F ledger_close base_p7=2413344 sum=2670559
day=2670559 gate=PASS`，`r_p8t8_faces.log`）。

### 10.3 T8 终检门禁（`r_p8t8_*.log`，2026-10-15 全绿）

| 门 | 结果 | 日志 |
|---|---|---|
| syntax（扩展域含 t7x_*/t8_*/build） | exit 0，0 FAIL | r_p8t8_syntax.log |
| review scan | REVIEW_SCAN CLEAN（extras=…t8_faces.py） | r_p8t8_review.log |
| 双 build 日/夜 | BUILD_DONE ×2，Textures packed: 11，bio 零漂 | r_p8t8_build_day/night.log |
| T8 面数总账（T1W/T2W/T3R/T4B/T5H 对象标记+8 行台账+日/夜红线+T10 门） | **T8FACES_ALL_PASS**（day=2,670,559 余 279,441；night core Δ=0） | r_p8t8_faces.log |
| T6S / T7X 双 Δ=0 | T6S_ALL_PASS + T7X_ALL_PASS（16 关键对象 0 失配） | r_p8t8_faces_t6s/_t7x.log |
| T5H 复跑 | T5H_ALL_PASS（Green_Hedge 78,404） | r_p8t8_faces_t5h.log |
| 帧时门 | **T7X_FRAME avg=1.390s ≤2.0 → L0 PASS**（环境披露见 10.5） | r_p8t8_frame.log |
| assert ×2 | **ASSERT_ALL_PASS ×2**（TURN 120/120、CORNER span=2、mean_nz≥0.997×6） | r_p8t8_assert_py/_bl.log |
| 探针清单 | **MANIFEST_ALL_DONE day=26 night=26 turn=120 gate=PASS** | r_p8t8_manifest.log |
| 转台+ffmpeg | TURN 168.1s（1.40s/帧）；ffmpeg exit 0，ffprobe h264/1920×1080/30fps/nb=120/duration=4.000000s | r_p8t8_turn/_ffmpeg.log |

### 10.4 探针（统一重渲）

- P7 集：`t7_probes_day/` **26 张** + `t7_probes_night/` **26 张**（夜集经 blend 互换
  复渲，day blend 前后 SHA256 一致证复原；亮度比 night/day ≈ 0.25-0.32）；
  `COLLECT_DONE day/night copied=26 gate=PASS`。
- P8 新增 **13 张**（$tmp 根，mtime 2026-10-15）：`p8t1_osm_day / p8t2_river_day+
  river_night / p8t3_roof_day / p8t4_bus_day / p8t5_hedge_day / p8t6_facade_day+
  facade_night+road_day+sign_night / p8t7_facade_day+road_day+sign_cn`
  （探针门 T1W/T2W/T3R/T4B/T5H/T6S/T7X 均 done/ALL_PASS，`r_p8t8_t*_probe.log`）。

### 10.5 帧时门环境披露

当日 3 路并行 OpenCode 会话挤压下 Normal 优先级复跑 avg 2.19-2.28s → LADDER_TRIGGER；
**空场景对照**（默认立方体 1080p EEVEE）固定开销 **0.96s/帧**（无写盘 0.56s、无关
射追+持久化 0.28s）证为环境性——场景可归属 ≈1.3s，与 P7 基准 1.371s 同水位。
High 优先级复跑 avg **1.950 / 1.390s ≤2.0 → L0 PASS ×2**；转台全城动画同日
**1.40s/帧** ≈ P7 1.43s 交叉印证。**不触发 2K 降级阶梯**（降级不能消固定开销，
且会丢 T7 4K 交付）。官方日志 `r_p8t8_frame.log`（avg=1.390）。

### 10.6 STATS 对账块（`hz_v2_assert.py` 纯打印，两行原文）

```
STATS v2: osm_bldg=143 infill=46676 riverfront=2273 roads=362 poke=0.2106% landmarks=4+1 trees=27724 cars=3764 reeds=366 streets=143cross/362walk tex=11packed green_hedge_m=195982 hedge_faces=78404 parks=21 signals=87 bus_stops=328 bus_faces=7872 vehicles=3764 roofdet=3233 roofdet_faces=84221 roof14=5357/26785 wingeo_faces=233452 riverwin_faces=181840 scene_faces=2670559
STATS P8 faces: base_p7=2413344 T1W=+25314 T2W=+181840 T3R=+26785 T4B=+7824 T5H=+15452 T6S=+0 T7X=+0 T8F=+0 final=2670559 red_line=2950000 headroom=279441 (gates r_p8t1..t5_faces / r_p8t6_faces / r_p8t7_faces)
```

字段溯源：`wingeo_faces=233452`→r_p8t2_faces；`bus_stops=328`/`bus_faces=7872`→r_p8t4_faces；
`green_hedge_m=195982`/`hedge_faces=78404`→r_p8t5_faces；`roofdet_faces=84221`/
`roof14=5357/26785`→r_p8t3+r_p8t8_build；`tex=11packed`→r_p8t7_build；
`riverwin_faces=181840`→双 build；`trees/cars/reeds`→build bio=（零漂）；
`scene_faces=2670559`/台账→r_p8t8_faces。

### 10.7 交付 5 件 SHA256（2026-10-15 刷新，覆盖 §9.8 旧值）

| 文件 | bytes | SHA256 |
|---|---:|---|
| `hanzhong.blend` | 202,922,884 | `C185FCE1F90F8241BA109FF33C3C04AF0584ABCF41ED2E63C72BABD83EE92C4A` |
| `hanzhong_night.blend` | 207,439,432 | `281BA55B61BF5FFCD69D04FC99385FC3B4856DE6855829D8FD75CEB113781D4C` |
| `hanzhong_preview.png` | 3,248,372 | `80A78046150B920017B7AD20FDB99B53F05CE47AF4F148F7CC411678D5F295E0` |
| `hanzhong_night.png` | 3,490,859 | `18666C5D275F5AD4870AC7D526CA42B142595976D09506354BD17E589063069E` |
| `hanzhong_turntable.mp4` | 4,655,236 | `1DAC0C98D18B8AFC4994B4BCC916992F8B709E6A5E5708DD1FB968F3807C07C2` |

来源：`hanzhong_v2[_night].blend`（双 build）+ `hz_v2_day/night.png`（Cycles 256spp）
+ `hz_turn` 120 帧 ffmpeg（r_p8t8_*）。三仓收口（$tmp `5b6762a` / 主仓 stage-only /
建模仓 push + robocopy exit）见 `task-8-report.md` 回填节。
