# Hanzhong P7 检查点①（M0 数据 / M1 玻璃 / M2 树形体）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 汉中 3D 照片级深化检查点①——OSM 绿地数据补抓、全城玻璃材质系统（+EEVEE 反射）、树形体升级（簇冠/分叉/4 色桶），交付一版供用户目验。

**Architecture:** 延续 v2 模块惯例——独立 `hz_*` 脚本位于 `C:\Users\HP\AppData\Local\Temp\opencode\`，`hanzhong_v2_build.py` 编排，材质走 `hz_v2_mat.py` 节点工厂。新增共享 `hz_overpass.py`（镜像 fallback）；玻璃升级全部在 `make_facade_mat` 节点层（零面数增量）；树生成从 `hz_v2_bio.py` 拆出为 `hz_v2_tree.py`，canopy 拆 4 合批对象做色变分桶。

**Tech Stack:** 纯 Python 3.11（conda）+ Blender 5.2 bpy + Overpass API。

## Global Constraints

- 设计依据：`docs/superpowers/specs/2026-10-07-hanzhong-p7-detail-design.md`（已批准）
- 函数 ≤40 行；文件 ≤300 行；单次生成 ≤300 行代码；含中文 .py 必须用 Write/Edit 工具创建
- 预算红线（本计划验收项）：EEVEE 帧时 ≤2s；Cycles 256spp ≤5min；面数记录进验收（终版 ≤2.6M）
- 检查点①**不改** `hz_v2_assert.py` 的 STATS 基线（trees=13210/cars=3764/reeds=366 必须保持）
- 每任务收尾固定跑：`hz_v2_syntax_check.py` + `hz_review_scan.py`（期望 CLEAN、无 FAIL）
- **不 git commit 主仓库**；`$tmp`（Temp\opencode）是独立 scratch git 仓，任务提交只发生在 `$tmp` 内（供审查 diff 用），主仓 docs 文件保持 untracked/未提交
- 不关机；Python 一律 `$env:PYTHONIOENCODING='utf-8'; & C:\Users\HP\anaconda3\python.exe <script.py>`（禁止 python -c 内联/多行）
- Blender 长任务：`Start-Process -FilePath "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -ArgumentList '-b','--factory-startup','--python','<script>' -RedirectStandardOutput <log> -RedirectStandardError <err> -NoNewWindow -PassThru | ForEach-Object { $_.WaitForExit() }`
- 所有 `$tmp` = `C:\Users\HP\AppData\Local\Temp\opencode`

---

### Task 1: M0-共享 Overpass 抓取模块 + hz_fetch_tags 接入

**Files:**
- Create: `$tmp\hz_overpass.py`（覆写：基线同名文件是一次性地标脚本，无 importer，旧内容在 `05ae13d` 可恢复）
- Modify: `$tmp\hz_fetch_tags.py`（删本地 fetch_tags/urllib 导入，改用共享模块）

**Interfaces:**
- Consumes: 无（首个任务）
- Produces: `hz_overpass.fetch(query, timeout=180) -> list[dict]`（返回 elements；全镜像失败 SystemExit）。Task 2 的 `hz_fetch_green.py` 依赖此签名。

- [ ] **Step 1: 写共享模块**（Write 工具，含中文注释）

```python
# -*- coding: utf-8 -*-
"""Overpass 共享抓取：三镜像 fallback（P6 hz_fetch_tags 同款逻辑抽公共）。"""
import json
import urllib.parse
import urllib.request

MIRRORS = ("https://overpass.kumi.systems/api/interpreter",
           "https://overpass-api.de/api/interpreter",
           "https://overpass.osm.jp/api/interpreter")
UA = "hanzhong-v2-p7/1.0"


def fetch(query, timeout=180):
    """POST overpass 返回 elements；全镜像失败抛 SystemExit。"""
    payload = urllib.parse.urlencode({"data": query}).encode()
    last = None
    for mirror in MIRRORS:
        try:
            req = urllib.request.Request(
                mirror, data=payload, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                return json.loads(resp.read().decode("utf-8")).get(
                    "elements", [])
        except Exception as exc:  # noqa: BLE001
            last = exc
            print("mirror fail: %s -> %s" % (mirror, exc))
    raise SystemExit("all mirrors failed: %s" % last)
```

- [ ] **Step 2: 改 hz_fetch_tags.py**（Edit 两处）
  1. 删除 `import urllib.parse` 与 `import urllib.request` 两行，在 `import os` 后加 `from hz_overpass import fetch`；
  2. 删除整个 `def fetch_tags(): ...` 函数（L39-56），`main()` 里 `elements = fetch_tags()` → `elements = fetch(QUERY)`。

- [ ] **Step 3: 语法 + review 扫描**

Run: `$env:PYTHONIOENCODING='utf-8'; & C:\Users\HP\anaconda3\python.exe "$tmp\hz_v2_syntax_check.py"; & C:\Users\HP\anaconda3\python.exe "$tmp\hz_review_scan.py"`
Expected: 无 FAIL、REVIEW_SCAN CLEAN

- [ ] **Step 4: 回归验证 hz_fetch_tags 真实抓取**

Run: `& C:\Users\HP\anaconda3\python.exe "$tmp\hz_fetch_tags.py"`
Expected: `P6_DONE bldg=174/174 road=375/376`（与重构前一致；镜像慢时 ≤3min）

---

### Task 2: M0-绿地/中分带/公交站/信号灯补抓合并

**Files:**
- Create: `$tmp\hz_fetch_green.py`

**Interfaces:**
- Consumes: `hz_overpass.fetch(query)`；`hanzhong_osm.json` 现有结构（buildings/roads/... 顶层键）
- Produces: `hanzhong_osm.json` 新增顶层键（整键替换、可重复跑）：
  - `greenspaces[]: {"pts": [[x,y]...], "kind": "park"|"grass", "name": str?}`
  - `medians[]: {"pts": [[x,y]...]}`
  - `bus_stops[]: {"xy": [x,y], "name": str?}`
  - `signals[]: {"xy": [x,y]}`
  - 坐标系：`x=(lon-107.02)*93342.6, y=(lat-33.07)*111132.92`（与 hz_fetch_tags.to_xy 相同常量）
- 消费方：M3 绿化带（计划②）读 `greenspaces/medians`；M6 家具（计划②）读 `bus_stops/signals`

- [ ] **Step 1: 写脚本**（Write 工具；文件 ≤300 行，四个 collect 函数各 ≤40 行）

```python
# -*- coding: utf-8 -*-
"""P7-M0 补抓：park/grass 绿地、median 中分带、bus_stop、traffic_signals
→ 整键替换写回 hanzhong_osm.json（可重复跑）。Run: python hz_fetch_green.py
"""
import json
import math
import os

from hz_overpass import fetch

TEMP = r"C:\Users\HP\AppData\Local\Temp\opencode"
OSM_PATH = os.path.join(TEMP, "hanzhong_osm.json")
CENTER_LON, CENTER_LAT = 107.02, 33.07
M_LAT = 111132.92
M_LON = 111412.84 * math.cos(math.radians(CENTER_LAT))
BBOX = "(33.025,106.956,33.115,107.084)"

QUERY = """
[out:json][timeout:180];
(
  way["leisure"="park"]%s;
  way["landuse"~"grass|recreation_ground"]%s;
  way["highway"="median"]%s;
  node["highway"="bus_stop"]%s;
  node["highway"="traffic_signals"]%s;
);
out geom;
""" % (BBOX, BBOX, BBOX, BBOX, BBOX)


def to_xy(lon, lat):
    return [round((lon - CENTER_LON) * M_LON, 2),
            round((lat - CENTER_LAT) * M_LAT, 2)]


def geom_pts(element):
    return [to_xy(p["lon"], p["lat"]) for p in element.get("geometry", [])]


def collect_areas(elements):
    """park/grass 面 → greenspaces（只留 ≥4 点的环）。"""
    out = []
    for e in elements:
        tags = e.get("tags", {})
        if not (tags.get("leisure") == "park"
                or tags.get("landuse") in ("grass", "recreation_ground")):
            continue
        pts = geom_pts(e)
        if len(pts) >= 4:
            item = {"pts": pts, "kind": "park" if tags.get("leisure")
                    else "grass"}
            if tags.get("name"):
                item["name"] = tags["name"]
            out.append(item)
    return out


def collect_medians(elements):
    """highway=median 窄条折线（≥2 点）。"""
    return [{"pts": geom_pts(e)} for e in elements
            if e.get("tags", {}).get("highway") == "median"
            and len(geom_pts(e)) >= 2]


def collect_points(elements, highway_val):
    """node 类：bus_stop / traffic_signals → [{xy, name?}]。"""
    out = []
    for e in elements:
        tags = e.get("tags", {})
        if e.get("node") is None and e.get("type") != "node":
            continue
        if tags.get("highway") != highway_val:
            continue
        lonlat = e.get("lat"), e.get("lon")
        if lonlat[0] is None:
            continue
        item = {"xy": to_xy(lonlat[1], lonlat[0])}
        if tags.get("name"):
            item["name"] = tags["name"]
        out.append(item)
    return out


def main():
    """抓取 → 四键整替 → 原子写回。"""
    elements = fetch(QUERY)
    print("fetched elements=%d" % len(elements))
    with open(OSM_PATH, encoding="utf-8") as fh:
        cached = json.load(fh)
    cached["greenspaces"] = collect_areas(elements)
    cached["medians"] = collect_medians(elements)
    cached["bus_stops"] = collect_points(elements, "bus_stop")
    cached["signals"] = collect_points(elements, "traffic_signals")
    tmp = OSM_PATH + ".tmp"
    with open(tmp, "w", encoding="utf-8") as fh:
        json.dump(cached, fh, ensure_ascii=False)
    os.replace(tmp, OSM_PATH)
    print("GREEN_DONE greens=%d medians=%d bus=%d sig=%d"
          % (len(cached["greenspaces"]), len(cached["medians"]),
             len(cached["bus_stops"]), len(cached["signals"])))


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: 语法 + review**

Run: 同 Task 1 Step 3 命令
Expected: 无 FAIL、CLEAN

- [ ] **Step 3: 执行抓取**

Run: `& C:\Users\HP\anaconda3\python.exe "$tmp\hz_fetch_green.py"`
Expected: `GREEN_DONE greens=N medians=M bus=B sig=S`——**记录四个数**；greens≥30、bus+sig≥100 才算数据到位。若 greens<10 → 触发 spec 风险 2 降级路径（记入 progress，M3 仅做道路推导层），继续后续任务不阻塞。

- [ ] **Step 4: 键存在性验证**

Run: `& C:\Users\HP\anaconda3\python.exe "$tmp\hz_json_keys.py"`（若不存在先 Write 一个 10 行脚本：加载 JSON 打印顶层键与四键长度）
Expected: `greenspaces=.. medians=.. bus_stops=.. signals=..` 全 ≥0 且键存在

---

### Task 3: M1-玻璃材质系统（hz_v2_glass 模块 + make_facade_mat 接入）

**Files:**
- Create: `$tmp\hz_v2_nodes.py`（共享节点工具，从 hz_v2_mat **剪切**而非复制）
- Create: `$tmp\hz_v2_glass.py`（玻璃 shader 组装，spec §3 M1 指定模块）
- Modify: `$tmp\hz_v2_mat.py`（删本地工具函数、`_window_grid` 四元组、主体换单行调用）
- Create: `$tmp\hz_m1_check.py`（基线/验收探针）

**Interfaces:**
- Consumes: 现有 `_window_grid(nt, uv, seed)`（改为返回四元组）；socket 惯例（RGBA Mix: inputs[6]=A, inputs[7]=B, outputs[2]=Result, inputs[0]=Factor）
- Produces:
  - `hz_v2_nodes`: `_new(name)->(mat,nt,bsdf)`、 `_nt_new(nt,type)`、 `_link(nt,out,sock,in,sock)`、 `_vector_math(nt,op,a_out,b)`、 `_math(nt,op,a,b,c)`
  - `hz_v2_mat` **re-export** 上述符号（`from hz_v2_nodes import ...` 置于文件 import 区）→ `hz_v2_mat_env` 的 `from hz_v2_mat import new_principled, _new, _nt_new, _link, _math` **零改动**仍工作
  - `hz_v2_glass`: `apply_glass(nt, brick, mapn, wall_out, bsdf, night, gate, emis)`、常量 `GLASS_A/GLASS_B/FRAME_RGB/INTERIOR_RGB`（M5 车辆复用）、私有 `_glass_tint/_mix_over/_frame_interior`
  - `make_facade_mat(name, tint, night, seed=0, prefix="facade")` 签名**不变**（`hanzhong_v2_build.make_materials` 零改动）

- [ ] **Step 1: 基线探针**（Write `$tmp\hz_m1_check.py`）

```python
# -*- coding: utf-8 -*-
"""M1 玻璃验收探针：raytracing 开关 + facade 节点数。Run: python hz_m1_check.py"""
import os
import sys

import bpy

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import hz_v2_data as D          # noqa: E402

bpy.ops.wm.open_mainfile(
    filepath=os.path.join(D.TEMP_DIR, "hanzhong_v2.blend"))
scene = bpy.context.scene
mat = bpy.data.materials.get("V2_Facade0")
nodes = len(mat.node_tree.nodes) if mat else -1
print("M1_CHECK raytracing=%s facade0_nodes=%d"
      % (scene.eevee.use_raytracing, nodes))
```

- [ ] **Step 2: 跑基线（旧 blend，未升级）**

Run: `& C:\Users\HP\anaconda3\python.exe "$tmp\hz_m1_check.py"`
Expected: `M1_CHECK raytracing=False facade0_nodes=N0` —— **记下 N0**（约 20~25）

- [ ] **Step 3: 抽共享节点工具 → hz_v2_nodes.py**

1. 先 grep 依赖面：`Select-String -Path "$tmp\hz_*.py" -Pattern "from hz_v2_mat import"`——列出全部从 mat 导入工具的文件（已知 `hz_v2_mat_env`），记入报告；
2. Write `$tmp\hz_v2_nodes.py`：把 hz_v2_mat.py 的 `_new/_nt_new/_link/_vector_math/_math` 五个函数**原样剪切**过去（文件头 `# -*- coding: utf-8 -*-` + docstring `"""共享节点工具（mat/glass/mat_env 同用，从 hz_v2_mat 抽出）。"""` + `import bpy` 视需要），各函数体一字不改；
3. hz_v2_mat.py 删除这五个函数定义，import 区加：

```python
from hz_v2_nodes import _link, _math, _new, _nt_new, _vector_math  # noqa: F401
```

（re-export：mat_env 等既有 `from hz_v2_mat import ...` 调用方零改动。）

- [ ] **Step 4: 新建 hz_v2_glass.py**（Write 工具；≤120 行）

文件头 `# -*- coding: utf-8 -*-`、docstring `"""P7-M1 玻璃 shader：窗格→玻璃+框带+假室内+朝向色差（make_facade_mat / M5 车辆共用）。"""`、`from hz_v2_nodes import _link, _math, _nt_new, _vector_math`。

常量（放 import 之后、函数之前）+ 三个私有函数（代码见下）+ `apply_glass`（见 Step 5b，函数名**不带**前导下划线）：

```python
GLASS_A = (0.14, 0.19, 0.22)       # 朝阳面玻璃
GLASS_B = (0.10, 0.14, 0.19)       # 背阴面玻璃
FRAME_RGB = (0.52, 0.54, 0.56)     # 窗框金属灰
INTERIOR_RGB = (0.04, 0.05, 0.07)  # 假室内暗色
```

```python
def _glass_tint(nt, normal_out):
    """朝向玻璃色：墙面法线 N 象限在 GLASS_A/B 间选色。"""
    sep = _nt_new(nt, "ShaderNodeSeparateXYZ")
    nt.links.new(normal_out, sep.inputs["Vector"])
    warm = _math(nt, "ADD", (sep, "X"), (sep, "Y"))
    gate = _math(nt, "GREATER_THAN", (warm, "Value"), 0.0)
    mix = _nt_new(nt, "ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.inputs[6].default_value = (*GLASS_A, 1.0)
    mix.inputs[7].default_value = (*GLASS_B, 1.0)
    nt.links.new(gate.outputs["Value"], mix.inputs[0])
    return mix.outputs[2]


def _mix_over(nt, base_out, rgb, factor_out):
    """factor 混色盖层：base ↔ 常量 rgb。"""
    mix = _nt_new(nt, "ShaderNodeMix")
    mix.data_type = "RGBA"
    nt.links.new(base_out, mix.inputs[6])
    mix.inputs[7].default_value = (*rgb, 1.0)
    nt.links.new(factor_out, mix.inputs[0])
    return mix.outputs[2]


def _frame_interior(nt, brick, mapn):
    """窗框带(Fac 0.55~0.90) + 格中心径向假室内 → (色, 框带mask)。"""
    gt = _math(nt, "GREATER_THAN", (brick, "Fac"), 0.55)
    lt = _math(nt, "LESS_THAN", (brick, "Fac"), 0.90)
    band = _math(nt, "MULTIPLY", (gt, "Value"), (lt, "Value"))
    framed = _mix_over(nt, brick.outputs["Color"], FRAME_RGB,
                       band.outputs["Value"])
    cell = _vector_math(nt, "FRACT", a_out=mapn.outputs["Vector"])
    sep = _nt_new(nt, "ShaderNodeSeparateXYZ")
    nt.links.new(cell.outputs["Vector"], sep.inputs["Vector"])
    dx = _math(nt, "SUBTRACT", (sep, "X"), 0.5)
    dy = _math(nt, "SUBTRACT", (sep, "Y"), 0.5)
    d2x = _math(nt, "MULTIPLY", (dx, "Value"), (dx, "Value"))
    d2y = _math(nt, "MULTIPLY", (dy, "Value"), (dy, "Value"))
    sum2 = _math(nt, "ADD", (d2x, "Value"), (d2y, "Value"))
    dist = _math(nt, "SQRT", (sum2, "Value"))
    room = _math(nt, "LESS_THAN", (dist, "Value"), 0.30)
    interior = _mix_over(nt, framed, INTERIOR_RGB, room.outputs["Value"])
    return interior, band.outputs["Value"]
```

- [ ] **Step 5: make_facade_mat 接入**（Edit hz_v2_mat.py）

**Step 5a**：改 `_window_grid` 返回值为四元组——尾行 `return brick, gate, col_mix` → `return brick, gate, col_mix, mapn`；grep `_window_grid(` 确认唯一调用方是 `make_facade_mat`（若 `hz_v2_mat_test.py` 也调用则同步解包）。

**Step 5b**：`apply_glass` 函数体已随 Step 4 写入 hz_v2_glass.py（下述代码即其内容，函数名 `apply_glass`——实现时放进 glass 文件，**不要**再往 hz_v2_mat 里加）：

```python
def apply_glass(nt, brick, mapn, wall_out, bsdf, night, gate, emis):
    """P7 玻璃链：墙↔玻璃↔框↔假室内 + 粗糙度 + 夜灯排除框带。"""
    nt.links.new(wall_out, brick.inputs["Mortar"])
    geo = _nt_new(nt, "ShaderNodeNewGeometry")
    glass = _glass_tint(nt, geo.outputs["Normal"])
    nt.links.new(glass, brick.inputs["Color1"])
    nt.links.new(glass, brick.inputs["Color2"])
    color_out, band = _frame_interior(nt, brick, mapn)
    nt.links.new(color_out, bsdf.inputs["Base Color"])
    rough = _math(nt, "MULTIPLY_ADD", (brick, "Fac"), 0.70, 0.06)
    _link(nt, rough, "Value", bsdf, "Roughness")
    bump = _nt_new(nt, "ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.35
    bump.inputs["Distance"].default_value = 0.08
    nt.links.new(brick.outputs["Fac"], bump.inputs["Height"])
    _link(nt, bump, "Normal", bsdf, "Normal")
    bsdf.inputs["IOR"].default_value = 1.45
    if night:
        _link(nt, emis, "Result", bsdf, "Emission Color")
        inv = _math(nt, "SUBTRACT", 1.0, (brick, "Fac"))
        no_band = _math(nt, "SUBTRACT", 1.0, (band, "Value"))
        lit = _math(nt, "MULTIPLY", (gate, "Value"), (inv, "Value"))
        lit = _math(nt, "MULTIPLY", (lit, "Value"), (no_band, "Value"))
        lit = _math(nt, "MULTIPLY", (lit, "Value"), 6.0)
        _link(nt, lit, "Value", bsdf, "Emission Strength")
```

**Step 5c**：hz_v2_mat.py import 区加 `from hz_v2_glass import apply_glass`；`make_facade_mat` 内原 `brick, gate, emis = _window_grid(nt, uv, seed)` 到 `return mat` 前的整段（砖色/rough/bump/夜灯 L160-178）删掉，改为：

```python
    brick, gate, emis, mapn = _window_grid(nt, uv, seed)
    apply_glass(nt, brick, mapn, wall_out, bsdf, night, gate, emis)
    return mat
```

（`wall_out` 即此前已构建的墙底色 socket，位置不变；函数体由原 ~20 行降到 4 行，hz_v2_mat.py 终态 ≤230 行。）

- [ ] **Step 6: 语法 + review + 探针**

Run: syntax + review（同 Task1 Step3）；再 `& C:\Users\HP\anaconda3\python.exe "$tmp\hz_m1_check.py"`
Expected: CLEAN；`facade0_nodes` 比 N0 **至少 +8**（raytracing 仍 False 属正常，开关在 Task 4）；`facade0_nodes` 记为 N1

---

### Task 4: M1-EEVEE 反射开关 + 近景楼选型清单

**Files:**
- Modify: `$tmp\hanzhong_v2_build.py`（main() 加 1 行开关）
- Create: `$tmp\hz_pick_closeups.py`

**Interfaces:**
- Consumes: Task 2 产出的 `hanzhong_osm.json`；`hz_v2_data.resolve_pois(osm)`、`hz_v2_data.find_tower_xy(osm, dem)`（既有签名）
- Produces: `scene.eevee.use_raytracing=True` 写入 blend（持久）；`$tmp\closeup_buildings.json` = `{"anchors": [[x,y]...], "buildings": [{"i": int, "pts": [[x,y]...], "h": float, "anchor": str}]}`——**M8（计划②）唯一消费此清单**

- [ ] **Step 1: build 开关**（Edit `hanzhong_v2_build.py` main()）

`scene.render.engine = "BLENDER_EEVEE"`（L94）之后插入：

```python
    scene.eevee.use_raytracing = True
```

- [ ] **Step 2: 写近景选型脚本**（Write `$tmp\hz_pick_closeups.py`；规则：5 锚点（古汉台/天汉楼/汉中站/拜将坛/电视塔）150m 内 OSM 楼，每锚点上限 24 栋，按距离升序，去重）

```python
# -*- coding: utf-8 -*-
"""P7-M1 近景楼选型：4 地标 + 电视塔 150m 内 OSM 楼（每锚点 24 上限）。
产出 closeup_buildings.json 供 M8 窗几何。Run: python hz_pick_closeups.py"""
import json
import math
import os

import hz_v2_data as D

TEMP = r"C:\Users\HP\AppData\Local\Temp\opencode"
RADIUS_M = 150.0
PER_ANCHOR = 24


def anchor_list(osm, dem):
    """5 个锚点：4 POI + 电视塔（找不到的跳过）。"""
    pois = D.resolve_pois(osm)
    out = [(k, xy) for k, xy in pois.items()]
    tower = D.find_tower_xy(osm, dem)
    if tower != (0.0, 0.0):
        out.append(("电视塔", tower))
    return out


def centroid(pts):
    return (sum(p[0] for p in pts) / len(pts),
            sum(p[1] for p in pts) / len(pts))


def pick(osm, anchors):
    """每锚点半径内按距离取前 PER_ANCHOR 栋，i 去重。"""
    chosen = {}
    for name, xy in anchors:
        ranked = []
        for i, b in enumerate(osm["buildings"]):
            c = centroid(b["pts"])
            dist = math.hypot(c[0] - xy[0], c[1] - xy[1])
            if dist < RADIUS_M:
                ranked.append((dist, i, b))
        ranked.sort(key=lambda t: t[0])
        for _d, i, b in ranked[:PER_ANCHOR]:
            chosen.setdefault(i, {"i": i, "pts": b["pts"],
                                  "h": b.get("h", 12.0), "anchor": name})
    return list(chosen.values())


def main():
    """选型 → JSON 落盘。"""
    osm, dem = D.load_caches()
    anchors = anchor_list(osm, dem)
    buildings = pick(osm, anchors)
    payload = {"anchors": [xy for _k, xy in anchors],
               "buildings": buildings}
    path = os.path.join(TEMP, "closeup_buildings.json")
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(payload, fh, ensure_ascii=False)
    print("CLOSEUP n=%d anchors=%d -> %s"
          % (len(buildings), len(anchors), path))


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: 语法 + review**

Run: 同 Task1 Step3
Expected: CLEAN

- [ ] **Step 4: 跑选型**

Run: `& C:\Users\HP\anaconda3\python.exe "$tmp\hz_pick_closeups.py"`
Expected: `CLOSEUP n=60..120 anchors=5`；n<40 时把 RADIUS_M 提到 200 重跑并在 progress 记录（锚点周边 OSM 密度差异）

---

### Task 5: M1-验收构建与渲染

**Files:**
- 无新文件；跑既有 `hanzhong_v2_build.py`、`hz_v2_render.py`、`hz_p3_closeups.py`、`hz_v2_assert.py`

**Interfaces:**
- Consumes: Task 3/4 全部改动
- Produces: 日/夜 blend + 日主帧 + 街景/楼群近景 PNG；验收记录

- [ ] **Step 1: 双 build**

Run（日）: `Start-Process -FilePath "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -ArgumentList '-b','--factory-startup','--python',"$tmp\hanzhong_v2_build.py" -RedirectStandardOutput "$tmp\p7_build_day.log" -RedirectStandardError "$tmp\p7_build_day.err" -NoNewWindow -PassThru | ForEach-Object { $_.WaitForExit() }`
Run（夜）: 同上加 `,'--','--night'`，日志 `p7_build_night.*`
Expected: `BUILD_DONE night=False ...` / `BUILD_DONE night=True ...`；err 无 Traceback；`Textures packed: 8` 仍在日志

- [ ] **Step 2: 探针复验**

Run: `& C:\Users\HP\anaconda3\python.exe "$tmp\hz_m1_check.py"`
Expected: `M1_CHECK raytracing=True facade0_nodes=N1`（N1 ≥ N0+8）

- [ ] **Step 3: 日主帧 + 近景渲染**

Run: `& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup --python "$tmp\hz_v2_render.py" -- --still --spp 256`（日志 r_p7_day.log）
Run: `& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup --python "$tmp\hz_p3_closeups.py"`（街景+楼群近景）
Expected: `hz_v2_day.png`、`hz_close_street.png`、`hz_close_roof.png` 时间戳刷新；单帧 Cycles ≤5min

- [ ] **Step 4: 断言全跑**

Run: `& C:\Users\HP\anaconda3\python.exe "$tmp\hz_v2_assert.py"`
Expected: `ASSERT_ALL_PASS`（法线×6、天空角、STATS 基线不动——trees=13210 等）

- [ ] **Step 5: 目验点自查**（read 工具直接看 PNG）

- `hz_close_street.png`：窗格出现**金属灰框带 + 窗心暗室点 + 玻璃面与天空同色系反射感**；墙面仍为灰泥/砖贴图×tint；夜窗未提前亮（日景）
- `hz_v2_day.png`：远中景楼群窗格反射方向随楼朝向有冷暖差；无破面/无全黑楼
- 若玻璃过亮/过暗：调 `GLASS_A/B`（暗 10%~20%）后重跑 Step 1-3；若框带过宽：把 band 阈值 `0.55/0.90` 收窄到 `0.65/0.92`

- [ ] **Step 6: 收尾扫描**

Run: syntax + review（同 Task1 Step3）
Expected: CLEAN；**M1 完成，M2 前把目验结论记进 progress（可边做 M2 边等你确认）**

---

### Task 6: M2-树形体升级（hz_v2_tree.py 拆分 + 4 色桶）

**Files:**
- Create: `$tmp\hz_v2_tree.py`
- Modify: `$tmp\hz_v2_bio.py`（删 4 个树函数 + CANOPY_RADII，改 import 与桶化调用）
- Modify: `$tmp\hz_v2_mat_env.py`（spec 加 leaf0..leaf3）

**Interfaces:**
- Consumes: `hz_v2_mesh.add_beam(v, f, p0, p1, r)`、`add_dome(v, f, x, y, radius, z, height, sides=, rings=)`（既有）；`hz_v2_data.WATER_LEVEL`
- Produces:
  - `hz_v2_tree.add_tree(trunks, canopies, x, y, z, sel, rng)`——`trunks=(verts, faces)`，`canopies=[(verts,faces)]×4`；签名变化点：`build_street_trees` 同步改
  - `hz_v2_tree.CANOPY_BUCKETS = 4`、`CANOPY_RADII = (3.4, 4.6, 5.8)`
  - 材质键 `leaf0..leaf3`（`make_simple_mats` 产出）；合批对象 `StreetTrees_Canopy0..3`
- **rng 契约（不可破）**：所有新几何随机数只准用传入的 `local` rng（位置派生）；主 rng 消耗序列保持 `sel = rng.random()` 单次调用 → **trees/cars/reeds 计数必须不变**

- [ ] **Step 1: 写 hz_v2_tree.py**（Write 工具；≤150 行）

```python
# -*- coding: utf-8 -*-
"""P7-M2 树形体：阔叶=干+分叉+3~4 球簇、塔柏=3 层叠锥、垂柳=簇+垂枝。
所有几何随机数用调用方传入的 local rng（主 rng 序列不受影响）。"""
import math

from hz_v2_data import WATER_LEVEL
from hz_v2_mesh import add_beam, add_dome

CANOPY_BUCKETS = 4
CANOPY_RADII = (3.4, 4.6, 5.8)


def _bucket(canopies, rng):
    """选一个色变桶（4 选 1，local rng 驱动）。"""
    return canopies[rng.randrange(CANOPY_BUCKETS)]


def add_broadleaf(trunks, canopies, x, y, z, rng):
    """阔叶：主干 + 2~3 分叉斜枝 + 3~4 球簇（主簇 7×3、侧簇 6×2）。"""
    radius = rng.choice(CANOPY_RADII)
    top = z + 2.6
    add_beam(trunks[0], trunks[1], (x, y, z - 0.3), (x, y, top), 0.18)
    cv, cf = _bucket(canopies, rng)
    add_dome(cv, cf, x, y, radius, z + 1.8, radius * 1.25,
             sides=7, rings=3)
    for _k in range(rng.randint(2, 3)):
        a = rng.uniform(0.0, 2.0 * math.pi)
        reach = radius * rng.uniform(0.5, 0.8)
        bx, by = x + math.cos(a) * reach, y + math.sin(a) * reach
        bz = top + rng.uniform(0.4, 1.4)
        add_beam(trunks[0], trunks[1], (x, y, top - 0.6), (bx, by, bz),
                 0.09)
        add_dome(cv, cf, bx, by, radius * rng.uniform(0.4, 0.55),
                 bz - 0.3, radius * 0.7, sides=6, rings=2)


def add_cypress(trunks, canopies, x, y, z, rng):
    """塔柏：细干 + 3 层递减窄冠（层叠锥，竖向节奏）。"""
    base_r = rng.uniform(1.1, 1.5)
    height = rng.uniform(5.0, 7.0)
    add_beam(trunks[0], trunks[1], (x, y, z - 0.3), (x, y, z + 1.6), 0.14)
    cv, cf = _bucket(canopies, rng)
    add_dome(cv, cf, x, y, base_r, z + 1.4, height * 0.55,
             sides=6, rings=2)
    add_dome(cv, cf, x, y, base_r * 0.75, z + 1.4 + height * 0.45,
             height * 0.4, sides=6, rings=2)
    add_dome(cv, cf, x, y, base_r * 0.45, z + 1.4 + height * 0.75,
             height * 0.3, sides=6, rings=2)


def add_willow(trunks, canopies, x, y, z, rng):
    """垂柳（河滩 ≤ 水位+8m）：扁阔主簇 + 2 侧簇 + 4 垂枝。"""
    radius = rng.uniform(4.0, 5.5)
    add_beam(trunks[0], trunks[1], (x, y, z - 0.3), (x, y, z + 3.0), 0.20)
    cv, cf = _bucket(canopies, rng)
    add_dome(cv, cf, x, y, radius, z + 2.4, radius * 0.7,
             sides=8, rings=3)
    for _k in range(2):
        a = rng.uniform(0.0, 2.0 * math.pi)
        ox, oy = math.cos(a) * radius * 0.7, math.sin(a) * radius * 0.7
        add_dome(cv, cf, x + ox, y + oy, radius * 0.5, z + 2.8,
                 radius * 0.5, sides=6, rings=2)
    for _k in range(4):
        a = rng.uniform(0.0, 2.0 * math.pi)
        reach = radius * rng.uniform(0.5, 0.85)
        bx, by = x + math.cos(a) * reach, y + math.sin(a) * reach
        add_beam(trunks[0], trunks[1], (bx, by, z + 2.6),
                 (bx + rng.uniform(-0.3, 0.3), by + rng.uniform(-0.3, 0.3),
                  z + 0.6), 0.06)


def add_tree(trunks, canopies, x, y, z, sel, rng):
    """树型分派：河滩垂柳 > 30% 塔柏 > 阔叶（sel=主 rng 单次抽值）。"""
    if z < WATER_LEVEL + 8.0:
        add_willow(trunks, canopies, x, y, z, rng)
    elif sel < 0.30:
        add_cypress(trunks, canopies, x, y, z, rng)
    else:
        add_broadleaf(trunks, canopies, x, y, z, rng)
```

- [ ] **Step 2: 改 hz_v2_bio.py**（Edits）
  1. 删 L15 `CANOPY_RADII` 行；删 `add_broadleaf/add_cypress/add_willow/add_tree` 四个函数整段（L48-87）；
  2. import 区加 `from hz_v2_tree import CANOPY_BUCKETS, add_tree`；删不再用的 `add_dome` import（`hz_v2_mesh` 行保留 `add_beam/add_oriented_box/make_mesh_object`——grep 确认 add_dome 仅树用后才删）；
  3. `build_street_trees`：
     - `canopy = ([], [])` → `canopies = [([], []) for _ in range(CANOPY_BUCKETS)]`
     - 调用处 `add_tree(trunk[0], trunk[1], canopy[0], canopy[1], px, py, z, sel, local)` → `add_tree(trunk, canopies, px, py, z, sel, local)`
     - `return trunk, canopy, tree_n` → `return trunk, canopies, tree_n`
  4. `build_biology` 尾部对象创建改为四桶：

```python
    for i, (cv, cf) in enumerate(canopies):
        make_mesh_object("StreetTrees_Canopy%d" % i, cv, cf,
                         mats["leaf%d" % i])
    print("Canopy buckets verts=%s" % [len(v) for v, _f in canopies])
```

（原 `make_mesh_object("StreetTrees_Canopy", ...)` 单行删除；`mats["leaf"]` 引用随之消失——grep `mats\["leaf"\]` 全 $tmp 确认无残留。）

- [ ] **Step 3: 加 leaf0..leaf3 材质**（Edit `hz_v2_mat_env.py` spec dict，`"leaf"` 行前后插入 4 行）

```python
        "leaf0": ((0.15, 0.38, 0.12), 0.9, 0.0, None, 0.0),
        "leaf1": ((0.19, 0.43, 0.15), 0.9, 0.0, None, 0.0),
        "leaf2": ((0.12, 0.34, 0.10), 0.9, 0.0, None, 0.0),
        "leaf3": ((0.22, 0.44, 0.18), 0.9, 0.0, None, 0.0),
```

（`"leaf"` 保留不动，防其他消费方；grep 确认后本任务不删。）

- [ ] **Step 4: 语法 + review**

Run: 同 Task1 Step3
Expected: 无 FAIL、CLEAN；grep `add_broadleaf|CANOPY_RADII` 于 hz_v2_bio.py 无定义残留（import 引用除外）

---

### Task 7: M2-验收构建与渲染

**Files:**
- 无新文件；跑既有 build/render/closeups/assert

**Interfaces:**
- Consumes: Task 6 全部改动
- Produces: 新日/夜 blend（树簇形态）+ 渲染图 + 计数记录

- [ ] **Step 1: 双 build**

Run: Task 5 Step 1 同款命令，日志改 `p7b2_build_day.log` / `p7b2_build_night.log`
Expected: `BUILD_DONE` ×2；日志同时含 `StreetTrees: 13210`（**计数不变**）与 `Canopy buckets verts=[v0,v1,v2,v3]`——四值全 >0 且差值 <50%（分桶均匀）

- [ ] **Step 2: 断言**

Run: `& C:\Users\HP\anaconda3\python.exe "$tmp\hz_v2_assert.py"`
Expected: `ASSERT_ALL_PASS`（trees=13210/cars=3764/reeds=366 基线未动）

- [ ] **Step 3: 日主帧 + 近景**

Run: Task 5 Step 3 同款两条（hz_v2_render --still 256spp + hz_p3_closeups）
Expected: PNG 时间戳刷新；单帧 ≤5min

- [ ] **Step 4: 目验点自查**（read 工具看 PNG）

- `hz_close_street.png` / `hz_close_roof.png`：树冠呈**多球簇轮廓**（边缘起伏非单圆包）、可见分叉枝、邻树冠色有深浅差；塔柏为层叠锥
- 面数预算记录：从 `p7b2_build_day.log` 汇总四桶 verts 之和与树干 verts，估算树部分面数（目标：全场景面数较 P6 增幅 ≤0.5M）；超 0.5M → 侧簇 `sides=6,rings=2` 降为 `sides=5,rings=2` 重跑 Step 1
- **M2 完成** → 进 Task 8 交付（若你对玻璃/树有反馈，先返 Task 3/6 调参）

---

### Task 8: 检查点① 交付刷新

**Files:**
- Modify: `C:\Users\HP\Desktop\deliverables\hanzhong\report.md`
- Modify: `C:\Users\HP\Desktop\deliverables\README.md`
- 交付 5 件复制；sdd 归档与 ledger

**Interfaces:**
- Consumes: Task 5/7 全部产物
- Produces: 用户目验包；通过后才启动计划②（M3-M8）

- [ ] **Step 1: 全量渲染套件**（顺序执行）

1. 日主帧: `hz_v2_render.py -- --still --spp 256`（Task 7 已渲可跳过——除非 7 后又改过代码）
2. 夜主帧: `hz_v2_render.py -- --night --spp 256` → `hz_v2_night.png`
3. 转台: `hz_v2_render.py -- --turn` → `hz_v2_turntable.mp4`；**记录 EEVEE 平均帧时 ≤2s**（红线，log 内 Time 行）
4. 七视角: `hz_v2_views.py` → 末行 `VIEWS_DONE`
5. assert: `hz_v2_assert.py` → `ASSERT_ALL_PASS`

- [ ] **Step 2: 复制 5 件**

```powershell
$dst = "C:\Users\HP\Desktop\deliverables\hanzhong"
Copy-Item "$tmp\hanzhong_v2.blend" "$dst\hanzhong.blend" -Force
Copy-Item "$tmp\hanzhong_v2_night.blend" "$dst\hanzhong_night.blend" -Force
Copy-Item "$tmp\hz_v2_day.png" "$dst\hanzhong_preview.png" -Force
Copy-Item "$tmp\hz_v2_night.png" "$dst\hanzhong_night.png" -Force
Copy-Item "$tmp\hz_v2_turntable.mp4" "$dst\hanzhong_turntable.mp4" -Force
Get-ChildItem -LiteralPath $dst -File | Select-Object Name, Length
```

Expected: 6 文件（5 件 + report.md）+ v1_backup\4；无 `.blend1` 残留（有则删）

- [ ] **Step 3: report.md 增补 §8B**

在 §8（P1-P6 表）之后追加：

```markdown
## §8B P7 检查点①（M0-M2，2026-10-07）

| 项 | 内容 | 验收 |
|---|---|---|
| M0 数据 | OSM 补抓 greenspaces/medians/bus_stops/signals → hanzhong_osm.json | GREEN_DONE 四键计数 |
| M1 玻璃 | 全城窗格=玻璃+金属框带+假室内+朝向色差；EEVEE raytracing 反射天空；夜灯排除框带 | M1_CHECK nodes N0→N1、近景目验 |
| M2 树形体 | 阔叶干+分叉+3~4 球簇、塔柏 3 层锥、垂柳簇+垂枝；4 色变桶合批 | StreetTrees=13210 不变、桶均匀、近景目验 |
| 近景选型 | 5 锚点 150m、每锚 24 上限 → closeup_buildings.json | CLOSEUP n=60..120 |

STATS 基线不动（osm_bldg=143 infill=46676 riverfront=2273 roads=362
trees=13210 cars=3764 reeds=366）。渲染：Cycles 256spp 日/夜、转台 120 帧。
```

（数值列按实跑结果改写——GREEN_DONE/CLOSEUP/帧时填实际数。）

- [ ] **Step 4: deliverables README 更新**

Edit `C:\Users\HP\Desktop\deliverables\README.md`：hanzhong 5 件表的文件大小按 Step 2 实测字节刷新；汇总行保持 hanzhong=10 计数不变（件数没变）。

- [ ] **Step 5: sdd 归档**（`$arc` = 本计划的 SDD workspace）

```powershell
$arc = "C:\Demo\roguelike_cpp\.superpowers\sdd\2026-10-07-hanzhong-p7-checkpoint1"
New-Item -ItemType Directory -Path "$arc\scripts" -Force | Out-Null
@("hz_overpass.py","hz_fetch_green.py","hz_pick_closeups.py","hz_m1_check.py",
  "hz_v2_tree.py","hz_v2_mat.py","hz_v2_glass.py","hz_v2_nodes.py",
  "hz_fetch_tags.py","hanzhong_v2_build.py",
  "hz_v2_bio.py","hz_v2_mat_env.py") | ForEach-Object {
    Copy-Item "$tmp\$_" "$arc\scripts\$_" -Force }
```

- [ ] **Step 6: 交付记录写入 SDD ledger**

在 `$arc\progress.md`（SDD ledger，首行是计划身份、任务完成行由控制器维护）**末尾追加**「P7 检查点①交付记录」一节（Write/Edit 工具、中文）：M0 四键计数、M1 N0→N1 与目验结论、M2 桶 verts 与面数增幅、EEVEE 帧时、assert 结果、5 件字节、遗留风险（raytracing 帧时是否超线、绿数据是否降级）。

- [ ] **Step 7: 规则 11 镜像（docs 变了）**

```powershell
robocopy "C:\Demo\roguelike_cpp\docs" "C:\Users\HP\Desktop\Roguelike-CPP-3D版\docs" /E
```

Expected: 退出码 0-7 均可（8+ 才算失败）；src/根文件本轮未动 → 不需其他镜像、不需重拷 exe。

- [ ] **Step 8: 用户目验门**

向用户交付三张重点图路径（`hanzhong_preview.png`、`hanzhong_night.png`、转台）+ 近景两张（`$tmp\hz_close_street.png`/`hz_close_roof.png`），请求确认：
1) 玻璃框带/反射方向对不对；2) 树簇与色差可接受否。
**通过 → 启动计划②（M3-M8）；不通过 → 返 Task 3/6 调参重渲。** 不自动关机。

---

## Self-Review 记录

1. **Spec 覆盖**：M0(Task1-2)、M1(Task3-5 含近景选型)、M2(Task6-7)、检查点①交付(Task8)——spec §3 全覆盖；§4/§5/§6 属计划②。
2. **占位符**：无 TBD/TODO；Task 2 Step 4 的 hz_json_keys.py 明确了"不存在则 Write 10 行"的完整动作。
3. **类型一致性**：`_window_grid` 四元组改签名已在 Task 3 Step 5a 显式同步；`add_tree(trunks, canopies, ...)` 在 Task 6 Step 2 调用处同步；`leaf0..3` 键 mat_env 与 bio 消费一致；`hz_overpass.fetch(query, timeout=180)` 两调用方一致；`closeup_buildings.json` 结构 Task 4 定义、计划② M8 消费。
4. **rng 不变量**：Task 6 明确 local-only + 主 rng 单次 `sel` → STATS 基线不动是硬验收（Task 7 Step 2）。
