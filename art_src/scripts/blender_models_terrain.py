# A20-Terrain: 群系地形雕饰 3D 件 (Blender MCP)
# 试跑: 遗忘监牢 (forgotten_prison) — 雕饰墙 / 厚地砖 / 真台阶 / 立体门框
# 坐标同 A14/18/19: Z-up, 1 单位=1 世界像素, 脚底 y=0, 正面 -Y
#   export_json 做 (x,y,z)->(x,z,-y) → 引擎正面朝 +Z(相机)
# 尺度: TILE_SIZE=32 (一格 32x32), 墙高 = 32*1.25 = 40, 门 = 32*1.05 ≈ 34
# 配色采样自 resources/biomes.json → tile_palette
import math
import os

from blender_lib import (
    export_json,
    join_parts,
    part_box,
    part_cylinder,
    part_prism,
    part_sphere,
    part_torus,
)
from mathutils import Matrix

REPO = r"C:/Demo/roguelike_cpp"
TILE = 32.0
WALL_H = 40.0

# ── 三群系配色 (biomes.json tile_palette) ──
PALETTES = {
    "forgotten_prison": dict(
        face=(42, 38, 34),
        brick=(50, 46, 42),
        top=(72, 68, 62),
        hi=(90, 85, 78),
        moss=(55, 70, 40),
        floor=(122, 112, 88),
        joint=(95, 88, 75),
        dirt=(95, 85, 55),
        floor_b=(140, 132, 110),
        grid=(50, 45, 38),
        glow=(150, 140, 90),
    ),
    "ash_volcano": dict(
        face=(50, 22, 15),
        brick=(60, 30, 20),
        top=(85, 40, 25),
        hi=(110, 60, 35),
        moss=(70, 45, 20),
        floor=(190, 80, 45),
        joint=(55, 25, 15),
        dirt=(120, 60, 30),
        floor_b=(230, 120, 70),
        grid=(45, 22, 15),
        glow=(240, 140, 50),
    ),
    "void_abyss": dict(
        face=(28, 18, 38),
        brick=(38, 25, 48),
        top=(55, 38, 65),
        hi=(75, 55, 85),
        moss=(50, 25, 55),
        floor=(110, 85, 150),
        joint=(35, 22, 45),
        dirt=(85, 60, 115),
        floor_b=(135, 105, 175),
        grid=(28, 18, 38),
        glow=(176, 128, 240),
    ),
}


def _slanted_box(name, center, size, color, slant_deg, bevel=0.0, taper=1.0):
    obj = part_box(name, (0, 0, 0), size, color, bevel=bevel, taper=taper)
    a = math.radians(slant_deg)
    ca, sa = math.cos(a), math.sin(a)
    for v in obj.data.vertices:
        x, z = v.co.x, v.co.z
        v.co.x = x * ca + z * sa + center[0]
        v.co.z = -x * sa + z * ca + center[2]
        v.co.y += center[1]
    return obj


def _spike(name, center, radius, depth, color, point_up=True):
    return part_cylinder(
        name,
        center,
        0.08 if point_up else radius,
        depth,
        color,
        vertices=6,
        radius_top=radius if point_up else 0.08,
        smooth=False,
    )


# ── 群系专属点缀 (主体石作通用; 火山=熔岩缝/黑曜刺, 深渊=发光符文/紫晶) ──
# 墙/门: 正面高浮雕 (z 在墙面高度范围); 地板/台阶: 只加贴地薄纹 (z≈4, 水平)
def _biome_decor(p, c, biome, kind):
    if biome == "forgotten_prison":
        return  # 监牢: 主体砖/苔/拱券已足够, 不加点缀
    vertical = kind in ("wall", "door")
    if biome == "ash_volcano":
        if vertical:
            # 墙面熔岩裂缝 (发光斜条) + 黑曜岩刺
            for i, (x, z, ang) in enumerate([(-8, 16, 22), (7, 26, -16), (0, 8, 10)]):
                p.append(
                    _slanted_box(
                        "lava_vein_%d" % i,
                        (x, -(TILE / 2) - 1.8, z),
                        (1.6, 0.8, 9),
                        c["glow"],
                        ang,
                        bevel=0.15,
                    )
                )
            for i, (x, z) in enumerate([(-11, 30), (11, 24)]):
                p.append(
                    _spike(
                        "obsidian_%d" % i,
                        (x, -(TILE / 2) - 2.5, z),
                        1.6,
                        6.0,
                        c["face"],
                    )
                )
        else:
            # 地面: 贴地熔岩细缝 (水平短条, 不悬浮)
            for i, (x, y, ang) in enumerate([(-6, -2, 20), (5, 4, -16), (0, -8, 8)]):
                p.append(
                    part_box(
                        "lava_crack_%d" % i,
                        (x, y, 4.6),
                        (5, 0.6, 0.3),
                        c["glow"],
                        bevel=0.08,
                        rotation_z=math.radians(ang),
                    )
                )
    elif biome == "void_abyss":
        if vertical:
            # 墙面发光符文条 + 紫晶
            for i, (x, z, ang) in enumerate([(-7, 20, 18), (6, 12, -22), (2, 30, 8)]):
                p.append(
                    _slanted_box(
                        "rune_%d" % i,
                        (x, -(TILE / 2) - 1.8, z),
                        (2.2, 0.8, 1.2),
                        c["glow"],
                        ang,
                        bevel=0.12,
                    )
                )
            for i, (x, z) in enumerate([(-10, 12), (9, 24)]):
                p.append(
                    part_sphere(
                        "crystal_%d" % i,
                        (x, -(TILE / 2) - 2.0, z),
                        1.3,
                        c["glow"],
                        segments=8,
                        rings=5,
                        scale=(1, 0.5, 1.4),
                    )
                )
        else:
            # 地面: 贴地幽光符点 (扁球)
            for i, (x, y) in enumerate([(-6, -3), (6, 4), (0, 8)]):
                p.append(
                    part_sphere(
                        "rune_glow_%d" % i,
                        (x, y, 4.5),
                        1.1,
                        c["glow"],
                        segments=8,
                        rings=4,
                        scale=(1, 1, 0.25),
                    )
                )


# ══════════════════════════════════════════════════════════════
# 雕饰墙 (每格 32x32, 高 40) — 砖缝凹凸 + 壁柱 + 拱券 + 苔痕/裂纹浮雕
# ══════════════════════════════════════════════════════════════
def build_wall(c, variant=0, biome="forgotten_prison"):
    """带雕饰的墙块: 主体 + 层叠砖块(正面浮雕) + 壁柱 + 拱券 + 苔/裂。"""
    p = [
        # 主体墙块 (略收, 让砖块浮雕浮出)
        part_box(
            "core",
            (0, 0, WALL_H / 2),
            (TILE - 1, TILE - 1, WALL_H),
            c["face"],
            bevel=0.6,
        ),
        # 顶面压檐
        part_box(
            "cap", (0, 0, WALL_H + 1), (TILE + 1, TILE + 1, 3), c["top"], bevel=0.5
        ),
        part_box(
            "cap_trim",
            (0, -(TILE / 2) - 0.4, WALL_H + 1),
            (TILE + 1, 1.0, 1.4),
            c["hi"],
            bevel=0.2,
        ),
    ]
    # 正面砖砌浮雕 (错缝 3 层 x 2 列)
    for row in range(3):
        z = 6 + row * 11
        for col in range(2):
            x = -8 + col * 16 + (8 if row % 2 else 0)
            if x > TILE / 2 - 4:
                continue
            p.append(
                part_box(
                    "brick_%d_%d" % (row, col),
                    (x, -(TILE / 2) - 0.5, z),
                    (14, 2.0, 8),
                    c["brick"],
                    bevel=0.5,
                    taper=0.95,
                )
            )
            # 砖面高光棱
            p.append(
                part_box(
                    "brick_hi_%d_%d" % (row, col),
                    (x, -(TILE / 2) - 1.6, z + 3),
                    (12, 0.6, 0.8),
                    c["hi"],
                    bevel=0.1,
                )
            )
    # ── 结构变体 (A20 二次精修: 三群系各 3 种墙面, 按 tile 坐标确定性选择) ──
    if variant == 0:
        # v0 拱券墙: 双壁柱(柱础/柱头) + 半圆拱券楔石 + 拱心石
        for side in (-1, 1):
            p.append(part_box("pilaster_%d" % side,
                              (side * (TILE / 2 - 3), -(TILE / 2) - 1.0, WALL_H / 2),
                              (5, 3, WALL_H - 4), c["top"], bevel=0.5, taper=0.92))
            p.append(part_box("pilaster_base_%d" % side,
                              (side * (TILE / 2 - 3), -(TILE / 2) - 1.4, 3),
                              (6, 3.5, 5), c["hi"], bevel=0.4))
            p.append(part_box("pilaster_cap_%d" % side,
                              (side * (TILE / 2 - 3), -(TILE / 2) - 1.4, WALL_H - 3),
                              (6, 3.5, 4), c["hi"], bevel=0.4))
        for i, ang in enumerate([-60, -30, 0, 30, 60]):
            a = math.radians(ang)
            p.append(part_box("arch_%d" % i,
                              (math.sin(a) * 8.0, -(TILE / 2) - 1.4,
                               WALL_H - 9 + math.cos(a) * 8.0),
                              (3.2, 2.0, 3.2), c["hi"], bevel=0.3, rotation_z=-a))
        p.append(part_box("arch_key", (0, -(TILE / 2) - 2.0, WALL_H - 1),
                          (3.6, 2.6, 4.0), c["top"], bevel=0.3))
    elif variant == 1:
        # v1 纯砖墙: 无壁柱/拱券, 满版错缝砖 + 两道横石带 + 竖向裂纹
        for row in range(4):
            z = 4 + row * 8
            for col in range(2):
                x = -8 + col * 16 + (8 if row % 2 else 0)
                if x > TILE / 2 - 4:
                    continue
                p.append(part_box("brick2_%d_%d" % (row, col),
                                  (x, -(TILE / 2) - 1.2, z), (14, 2.2, 6.6),
                                  c["brick"] if (row + col) % 2 else c["top"],
                                  bevel=0.5, taper=0.95))
        for k, z in enumerate((14, 30)):
            p.append(part_box("band_%d" % k, (0, -(TILE / 2) - 1.5, z),
                              (TILE - 2, 2.4, 2.2), c["hi"], bevel=0.3))
        p.append(_slanted_box("crack", (3, -(TILE / 2) - 1.8, 20),
                              (1.2, 0.8, 20), c["grid"], 10, bevel=0.1))
    else:
        # v2 浮雕墙: 中央单壁柱 + 两侧菱形盾徽 + 苔痕, 无拱券
        p.append(part_box("center_pilaster", (0, -(TILE / 2) - 1.2, WALL_H / 2),
                          (5, 3, WALL_H - 4), c["top"], bevel=0.5, taper=0.94))
        for side in (-1, 1):
            p.append(part_box("shield_%d" % side,
                              (side * 8, -(TILE / 2) - 1.6, 20),
                              (6, 1.4, 6), c["hi"], bevel=0.3,
                              rotation_z=math.radians(45)))
            p.append(part_sphere("boss_%d" % side,
                                 (side * 8, -(TILE / 2) - 2.4, 20), 1.4,
                                 c["top"], segments=8, rings=4, scale=(1, 0.5, 1)))
        for i, (x, z) in enumerate([(-10, 8), (10, 30), (7, 12)]):
            p.append(part_sphere("moss_v2_%d" % i, (x, -(TILE / 2) - 1.6, z),
                                 2.0, c["moss"], segments=8, rings=5,
                                 scale=(1.3, 0.4, 0.8)))
    _biome_decor(p, c, biome, "wall")
    return join_parts(p, "terrain_wall_" + biome)


# ══════════════════════════════════════════════════════════════
# 带厚度地砖 (每格 32x32, 厚 ~3) — 分块石板 + 接缝 + 磨损
# ══════════════════════════════════════════════════════════════
def build_floor(c, variant=0, biome="forgotten_prison"):
    p = [
        part_box("base", (0, 0, 1.5), (TILE, TILE, 3), c["joint"], bevel=0.4),
    ]
    # 2x2 石板 (留缝)
    for row in range(2):
        for col in range(2):
            x = -8 + col * 16
            y = -8 + row * 16
            shade = c["floor"] if (row + col) % 2 == 0 else c["floor_b"]
            p.append(
                part_box(
                    "slab_%d_%d" % (row, col),
                    (x, y, 3.4),
                    (14.5, 14.5, 1.4),
                    shade,
                    bevel=0.5,
                    taper=0.97,
                )
            )
    # 四角磨损/凿点
    for i, (x, y) in enumerate([(-13, -13), (13, -13), (-13, 13), (13, 13)]):
        p.append(
            part_sphere(
                "chip_%d" % i,
                (x, y, 4.0),
                1.2,
                c["dirt"],
                segments=8,
                rings=4,
                scale=(1, 1, 0.4),
            )
        )
    if variant == 1:
        p.append(
            _slanted_box("crack", (2, 0, 4.4), (0.8, 12, 0.5), c["grid"], 20, bevel=0.1)
        )
        p.append(
            part_sphere(
                "stain",
                (-4, 4, 4.2),
                3.0,
                c["dirt"],
                segments=10,
                rings=5,
                scale=(1.4, 1.0, 0.2),
            )
        )
    elif variant == 2:
        for i, (x, y, r) in enumerate([(-6, 2, 1.4), (7, -5, 1.0), (2, 9, 1.2)]):
            p.append(
                part_sphere(
                    "grit_%d" % i,
                    (x, y, 4.3),
                    r,
                    c["dirt"],
                    segments=6,
                    rings=4,
                    scale=(1, 1, 0.3),
                )
            )
    _biome_decor(p, c, biome, "floor")
    return join_parts(p, "terrain_floor_" + biome)


# ══════════════════════════════════════════════════════════════
# 真台阶 (楼梯 tile) — 递降台阶 + 两侧栏 + 金色方向提示
# ══════════════════════════════════════════════════════════════
def build_stairs(c, biome="forgotten_prison"):
    p = [part_box("well", (0, 0, 1), (TILE, TILE, 2), c["grid"], bevel=0.4)]
    for s in range(4):
        z = 2 + s * 4
        depth = TILE - 4 - s * 5
        p.append(
            part_box(
                "step_%d" % s,
                (0, -2 + s * 3, z),
                (depth, 7, 3),
                c["floor_b"] if s % 2 else c["floor"],
                bevel=0.5,
                taper=0.95,
            )
        )
        p.append(
            part_box(
                "nosing_%d" % s,
                (0, -2 + s * 3, z + 1.6),
                (depth, 1.0, 0.8),
                c["hi"],
                bevel=0.2,
            )
        )
    # 两侧栏柱
    for side in (-1, 1):
        p.append(
            part_box(
                "rail_%d" % side,
                (side * (TILE / 2 - 2), 0, 8),
                (3, TILE - 2, 14),
                c["top"],
                bevel=0.4,
            )
        )
    _biome_decor(p, c, biome, "stairs")
    return join_parts(p, "terrain_stairs_" + biome)


# ══════════════════════════════════════════════════════════════
# 立体门框 (门 tile) — 石框 + 门楣雕饰 + 门板 + 铆钉
# ══════════════════════════════════════════════════════════════
def build_door(c, biome="forgotten_prison"):
    h = 34.0
    p = [
        part_box("jamb_l", (-12, 0, h / 2), (6, 8, h), c["top"], bevel=0.6),
        part_box("jamb_r", (12, 0, h / 2), (6, 8, h), c["top"], bevel=0.6),
        part_box("lintel", (0, 0, h - 2), (TILE, 9, 5), c["hi"], bevel=0.6),
        part_box("threshold", (0, 0, 1), (TILE, 9, 2), c["grid"], bevel=0.3),
        # 门板 (深木/铁)
        part_box("door", (0, -1, h / 2 - 2), (18, 3, h - 6), c["brick"], bevel=0.6),
    ]
    # 门板分板 + 铆钉
    for row in range(3):
        p.append(
            part_box(
                "panel_%d" % row,
                (0, -2.7, 7 + row * 9),
                (14, 0.8, 6),
                c["face"],
                bevel=0.3,
            )
        )
        for side in (-1, 1):
            p.append(
                part_sphere(
                    "stud_%d_%d" % (row, side),
                    (side * 6, -3.3, 7 + row * 9),
                    0.6,
                    c["hi"],
                    segments=8,
                    rings=4,
                )
            )
    # 门楣符/拱
    p.append(
        part_sphere(
            "keystone",
            (0, -4.6, h - 2),
            1.8,
            c["glow"],
            segments=10,
            rings=6,
            scale=(1, 0.5, 1),
        )
    )
    _biome_decor(p, c, biome, "door")
    return join_parts(p, "terrain_door_" + biome)


# 三群系: 墙 3 结构变体 (v0 拱券 / v1 纯砖 / v2 浮雕) + 地板/楼梯/门 各 1 = 18 件。
# 墙保留无后缀别名 (= v0), 供 builder 确定性 hash 选变体时作默认回退名。
PRISON = PALETTES["forgotten_prison"]
VOLCANO = PALETTES["ash_volcano"]
VOID = PALETTES["void_abyss"]
TERRAIN_BUILDERS = {}
for _short, _pal, _biome in (
    ("prison", PRISON, "forgotten_prison"),
    ("volcano", VOLCANO, "ash_volcano"),
    ("void", VOID, "void_abyss"),
):
    TERRAIN_BUILDERS["terrain_floor_%s" % _short] = (
        lambda c=_pal, b=_biome: build_floor(c, 0, b))
    TERRAIN_BUILDERS["terrain_stairs_%s" % _short] = (
        lambda c=_pal, b=_biome: build_stairs(c, b))
    TERRAIN_BUILDERS["terrain_door_%s" % _short] = (
        lambda c=_pal, b=_biome: build_door(c, b))
    for _v in (0, 1, 2):
        TERRAIN_BUILDERS["terrain_wall_%s_v%d" % (_short, _v)] = (
            lambda c=_pal, v=_v, b=_biome: build_wall(c, v, b))
    # 无后缀别名 = v0 (确定性 hash 的默认/回退)
    TERRAIN_BUILDERS["terrain_wall_%s" % _short] = (
        lambda c=_pal, b=_biome: build_wall(c, 0, b))


def export_one(name):
    obj = TERRAIN_BUILDERS[name]()
    path = os.path.join(REPO, "assets", "models", name + ".json")
    verts, faces = export_json(obj, path, name)
    return obj, verts, faces


def export_all():
    for name in TERRAIN_BUILDERS:
        _, v, f = export_one(name)
        print("EXPORT %-24s verts=%d tris=%d" % (name, v, f))
    print("EXPORT_ALL_OK terrains=%d" % len(TERRAIN_BUILDERS))


def preview_one(name):
    import bpy
    import blender_scene

    camera = blender_scene.setup_scene()
    obj = TERRAIN_BUILDERS[name]()
    bpy.context.view_layer.update()
    pts = [v.co for v in obj.data.vertices]
    low, high = min(p.z for p in pts), max(p.z for p in pts)
    cz = (low + high) * 0.5
    blender_scene.frame_camera(
        camera, (0, 0, cz), distance=110.0, azimuth=128.0, elevation=24.0
    )
    out = os.path.join(REPO, "art_src", "preview", "terrain_preview_%s.png" % name)
    blender_scene.render_to(out)
    print("PREVIEW %s height=%.1f" % (name, high - low))
    return out
