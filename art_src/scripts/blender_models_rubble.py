# P5: 群系破碎地面装饰件 (Blender MCP)
# 坐标同 A20: Z-up, 正面 -Y, export_json (x,y,z)->(x,z,-y) → 引擎 y=高, z=深
# 贴地 y_min≈0, 高 ≤6, 占地 x/z ±16 (1 tile=32)。C++ 渲染时整体抬到石板顶(y≈5)。
import math
import os

from blender_lib import (
    export_json,
    join_parts,
    part_box,
    part_sphere,
    part_prism,
    part_cylinder,
)
from mathutils import Matrix

REPO = r"C:/Demo/roguelike_cpp"

# 三群系配色(取自 blender_models_terrain.py 的 PALETTES)
PALETTES = {
    "prison": dict(
        face=(42, 38, 34),
        brick=(50, 46, 42),
        top=(72, 68, 62),
        hi=(90, 85, 78),
        grid=(50, 45, 38),
        glow=(150, 140, 90),
    ),
    "volcano": dict(
        face=(50, 22, 15),
        brick=(60, 30, 20),
        top=(85, 40, 25),
        hi=(110, 60, 35),
        grid=(45, 22, 15),
        glow=(240, 140, 50),
    ),
    "void": dict(
        face=(28, 18, 38),
        brick=(38, 25, 48),
        top=(55, 38, 65),
        hi=(75, 55, 85),
        grid=(28, 18, 38),
        glow=(176, 128, 240),
    ),
}


def _spike_shard(name, center, radius, depth, color):
    """朝上的碎石棱 (6 边低段锥)。"""
    from blender_lib import part_cylinder

    return part_cylinder(
        name, center, 0.08, depth, color, vertices=6, radius_top=radius, smooth=False
    )


def _build_rubble(biome):
    c = PALETTES[biome]
    p = []
    # 碎裂石板: 几块错缝扁 box (错开高低/角度, 制造破碎拼接地表)
    slabs = [
        (-8, -7, 0.9, 11, 9, 1.6, 8),
        (7, -6, 1.2, 10, 11, 2.0, -6),
        (-6, 8, 0.7, 12, 8, 1.4, 12),
        (8, 7, 1.1, 9, 10, 1.8, -10),
        (0, 0, 1.5, 8, 8, 2.4, 22),
    ]
    for i, (x, y, z, sx, sy, sz, ang) in enumerate(slabs):
        p.append(
            part_box(
                "slab_%d" % i,
                (x, y, z),
                (sx, sy, sz),
                c["brick"] if i % 2 else c["face"],
                bevel=0.25,
                rotation_z=math.radians(ang),
            )
        )
        # 石板顶面高光棱
        p.append(
            part_box(
                "slab_hi_%d" % i,
                (x, y - sy * 0.32, z + sz * 0.5),
                (sx * 0.8, 0.5, 0.4),
                c["hi"],
                bevel=0.1,
                rotation_z=math.radians(ang),
            )
        )
    # 龟裂发光线: 几条细长发光条, 嵌在石板缝里
    for i, (x, y, ang, ln) in enumerate(
        [(-3, -2, 28, 13), (4, 3, -20, 11), (0, -9, 8, 10), (-9, 4, 42, 9)]
    ):
        p.append(
            part_box(
                "crack_glow_%d" % i,
                (x, y, 2.1),
                (ln, 0.5, 0.35),
                c["glow"],
                bevel=0.08,
                rotation_z=math.radians(ang),
            )
        )
    # 崩落碎石: 小棱锥 + 小扁球簇
    shards = [(-12, -12), (12, -11), (-13, 11), (11, 12), (-2, 13), (3, -13)]
    for i, (x, y) in enumerate(shards):
        p.append(
            _spike_shard(
                "shard_%d" % i, (x, y, 1.4), 1.3, 2.8, c["face"] if i % 2 else c["top"]
            )
        )
    for i, (x, y) in enumerate([(-5, 5), (6, -4), (2, 9), (-8, -3)]):
        p.append(
            part_sphere(
                "pebble_%d" % i,
                (x, y, 0.8),
                0.9,
                c["grid"],
                segments=8,
                rings=4,
                scale=(1, 1, 0.6),
            )
        )
    return join_parts(p, "terrain_rubble_" + biome)


BUILDERS = {
    "terrain_rubble_prison": lambda: _build_rubble("prison"),
    "terrain_rubble_volcano": lambda: _build_rubble("volcano"),
    "terrain_rubble_void": lambda: _build_rubble("void"),
}


def export_one(name):
    obj = BUILDERS[name]()
    path = os.path.join(REPO, "assets", "models", name + ".json")
    verts, faces = export_json(obj, path, name)
    return obj, verts, faces


def export_all():
    for name in BUILDERS:
        _, v, f = export_one(name)
        print("EXPORT %-26s verts=%d tris=%d" % (name, v, f))


def preview_one(name):
    import bpy
    import blender_scene

    camera = blender_scene.setup_scene()
    obj = BUILDERS[name]()
    bpy.context.view_layer.update()
    pts = [v.co for v in obj.data.vertices]
    low, high = min(p.z for p in pts), max(p.z for p in pts)
    blender_scene.frame_camera(
        camera, (0, 0, (low + high) * 0.5), distance=90.0, azimuth=128.0, elevation=30.0
    )
    out = os.path.join(REPO, "art_src", "preview", "terrain_preview_%s.png" % name)
    blender_scene.render_to(out)
    print("PREVIEW %s height=%.1f" % (name, high - low))
