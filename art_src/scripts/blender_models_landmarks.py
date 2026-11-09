# A18-Landmark: 地牢地标手建低模 (Blender MCP)
# 坐标: Z-up, 1 单位=1 世界像素, 脚底 z≈0, 正面 -Y
#   export_json 做 (x,y,z)->(x,z,-y) → 引擎里正面朝 +Z(相机)
# 配色采样自 resources/landmarks.json 的 tile_color + 描述文案
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

REPO = r"C:/Demo/roguelike_cpp"

# ── 通用调色板 ──
PRISON_STONE = (96, 88, 80)
PRISON_DK = (64, 58, 54)
PRISON_LT = (128, 118, 108)
RUST = (140, 78, 44)
RUST_DK = (96, 52, 32)
BONE = (206, 198, 176)
CHAIN = (120, 124, 132)
LAVA = (216, 88, 32)
LAVA_LT = (248, 152, 56)
LAVA_DK = (140, 44, 20)
OBSIDIAN = (52, 40, 44)
EMBER = (172, 60, 28)
VOID_STONE = (72, 60, 104)
VOID_DK = (44, 34, 72)
VOID_LT = (104, 88, 152)
VOID_GLOW = (176, 128, 240)
VOID_TEAR = (24, 16, 44)


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


# ══════════ 遗忘监牢 ══════════
def build_broken_cell():
    """断裂牢房: 破损铁栏 + 断链 + 墙上抓痕 + 骸骨。"""
    p = [
        part_box("floor", (0, 0, 1), (30, 22, 2), PRISON_DK, bevel=0.5),
        # 断裂矮墙 (不做整面高墙, 否则挡住铁栏主体); 左右两段 + 中央缺口
        part_box("back_wall_l", (-10, 9.5, 8), (10, 3, 14), PRISON_STONE, bevel=0.6),
        part_box("back_wall_r", (10, 9.5, 7), (10, 3, 12), PRISON_STONE, bevel=0.6),
        part_box(
            "wall_crack",
            (-10, 7.8, 12),
            (1.0, 0.6, 8),
            PRISON_DK,
            bevel=0.1,
            rotation_z=math.radians(8),
        ),
    ]
    # 断裂铁栏 (前排)
    for i, x in enumerate([-11, -5.5, 0, 5.5, 11]):
        h = 24 if i in (0, 4) else (10 if i == 2 else 20)
        p.append(part_cylinder("bar_%d" % i, (x, -9, h / 2), 1.1, h, CHAIN, vertices=8))
        if i == 2:
            p.append(_spike("bar_snap_%d" % i, (x, -9, h + 1), 1.3, 3.0, RUST))
    # 横栏 + 断链
    p.append(part_box("rail_top", (0, -9, 22), (26, 2, 1.6), CHAIN, bevel=0.3))
    for i, (x, z) in enumerate([(-8, 14), (-6, 11), (-4, 8)]):
        p.append(
            part_sphere("chain_%d" % i, (x, -7.5, z), 1.4, RUST_DK, segments=8, rings=4)
        )
    # 骸骨
    p.append(
        part_sphere(
            "skull", (-8, 2, 4), 2.6, BONE, segments=12, rings=8, scale=(1.0, 0.9, 0.85)
        )
    )
    p.append(
        part_box(
            "bone_1",
            (-4, 2, 2.5),
            (6, 1.4, 1.4),
            BONE,
            bevel=0.4,
            rotation_z=math.radians(12),
        )
    )
    p.append(
        part_box(
            "bone_2",
            (1, 4, 2.5),
            (6, 1.4, 1.4),
            BONE,
            bevel=0.4,
            rotation_z=math.radians(-20),
        )
    )
    return join_parts(p, "lm_broken_cell")


def build_torture_chamber():
    """刑讯室: 铁架 + 垂链 + 锈刑具 + 暗红印。"""
    p = [
        part_box("floor", (0, 0, 1), (30, 22, 2), PRISON_DK, bevel=0.5),
        # 断裂矮墙 (整面高墙会挡住铁架主体, 与 broken_cell 同理)
        part_box("back_l", (-11, 9.5, 7), (8, 3, 12), PRISON_STONE, bevel=0.6),
        part_box("back_r", (11, 9.5, 6), (8, 3, 10), PRISON_STONE, bevel=0.6),
        part_cylinder("post_l", (-10, 0, 14), 1.6, 27, CHAIN, vertices=8),
        part_cylinder("post_r", (10, 0, 14), 1.6, 27, CHAIN, vertices=8),
        part_box("beam", (0, 0, 27), (24, 3, 2.4), RUST_DK, bevel=0.4),
    ]
    # 垂链
    for i, (x, ln) in enumerate([(-6, 12), (0, 16), (6, 10)]):
        for k in range(4):
            p.append(
                part_sphere(
                    "chain_%d_%d" % (i, k),
                    (x, 0, 26 - k * ln / 4),
                    0.9,
                    CHAIN,
                    segments=8,
                    rings=4,
                )
            )
        p.append(
            part_box("shackle_%d" % i, (x, 0, 26 - ln), (3, 2, 1.4), RUST, bevel=0.3)
        )
    # 刑具台 + 锈刃
    p.append(part_box("table", (0, -3, 5), (16, 8, 3), PRISON_DK, bevel=0.5))
    p.append(part_box("stain", (0, -3, 6.6), (10, 5, 0.4), RUST_DK, bevel=0.2))
    p.append(
        _slanted_box(
            "blade", (6, -3, 8), (5, 1.2, 2), RUST, -20.0, bevel=0.2, taper=0.4
        )
    )
    return join_parts(p, "lm_torture_chamber")


def build_collapsed_tunnel():
    """坍塌隧道: 落石堆 + 断裂拱门 + 支撑木。"""
    p = [
        part_box("floor", (0, 0, 1), (30, 22, 2), PRISON_DK, bevel=0.5),
        part_box("arch_l", (-13, 6, 12), (4, 6, 22), PRISON_STONE, bevel=0.6),
        part_box("arch_r", (13, 6, 12), (4, 6, 22), PRISON_STONE, bevel=0.6),
        part_box("lintel", (0, 6, 24), (30, 6, 4), PRISON_LT, bevel=0.5),
    ]
    # 落石堆
    for i, (x, y, z, r) in enumerate(
        [
            (-8, -2, 5, 5),
            (-2, 1, 6, 6),
            (5, -3, 5, 5.5),
            (0, -5, 10, 4.5),
            (9, 2, 4, 4),
            (-6, 4, 3, 3.5),
        ]
    ):
        p.append(
            part_sphere(
                "rock_%d" % i,
                (x, y, z),
                r,
                PRISON_STONE if i % 2 else PRISON_DK,
                segments=10,
                rings=6,
                scale=(1.0, 0.9, 0.8),
            )
        )
    # 断裂支撑木
    p.append(_slanted_box("beam_1", (-9, -6, 14), (16, 2, 2), RUST_DK, 28.0, bevel=0.3))
    p.append(_slanted_box("beam_2", (8, -6, 12), (14, 2, 2), RUST_DK, -34.0, bevel=0.3))
    return join_parts(p, "lm_collapsed_tunnel")


# ══════════ 灰烬火山 ══════════
def build_lava_rift():
    """熔岩裂缝: 龟裂黑岩 + 发光裂缝 + 熔岩泡 + 硫磺晶。"""
    p = [part_box("ground", (0, 0, 1), (30, 22, 2), OBSIDIAN, bevel=0.5)]
    p.append(part_box("rim_l", (-9, 0, 4), (10, 20, 6), OBSIDIAN, bevel=0.8, taper=0.9))
    p.append(part_box("rim_r", (9, 0, 4), (10, 20, 6), OBSIDIAN, bevel=0.8, taper=0.9))
    p.append(part_box("lava", (0, 0, 1.6), (7, 20, 1.2), LAVA, bevel=0.3))
    p.append(part_box("lava_core", (0, 0, 2.0), (4.5, 18, 0.8), LAVA_LT, bevel=0.2))
    for i, (x, y, r) in enumerate([(-1, -6, 1.6), (1.5, 2, 2.0), (-0.5, 7, 1.4)]):
        p.append(
            part_sphere(
                "bubble_%d" % i,
                (x, y, 2.6),
                r,
                LAVA_LT,
                segments=10,
                rings=6,
                scale=(1, 1, 0.5),
            )
        )
    for i, (x, y, h) in enumerate([(-11, -7, 7), (11, 4, 9), (-12, 6, 6)]):
        p.append(_spike("obsidian_%d" % i, (x, y, 6 + h / 2), 2.2, h, EMBER))
    return join_parts(p, "lm_lava_rift")


def build_forge_ruins():
    """废弃铁匠铺: 半埋铁砧 + 未熄炉火 + 铁锤 + 熔岩。"""
    p = [
        part_box("ground", (0, 0, 1), (30, 22, 2), OBSIDIAN, bevel=0.5),
        part_box("lava_pool", (0, -4, 1.8), (22, 10, 1.6), LAVA_DK, bevel=0.4),
        part_box("forge", (7, 5, 7), (12, 8, 12), OBSIDIAN, bevel=0.6),
        part_box("forge_mouth", (7, 0.8, 6), (6, 1.2, 5), LAVA, bevel=0.3),
        part_box("ember", (7, 0.5, 6), (4, 0.8, 3), LAVA_LT, bevel=0.2),
        part_box("anvil_base", (-6, -2, 3), (8, 6, 4), CHAIN, bevel=0.5),
        part_box(
            "anvil_top",
            (-6, -2, 6),
            (11, 5, 2.4),
            (150, 160, 172),
            bevel=0.4,
            taper=0.85,
        ),
        part_cylinder(
            "anvil_horn",
            (-11, -2, 6),
            1.6,
            5.0,
            (150, 160, 172),
            vertices=6,
            radius_top=0.2,
            smooth=False,
        ),
        part_cylinder("hammer_handle", (3, -2, 7), 0.8, 10, RUST_DK, vertices=8),
        part_box("hammer_head", (5, -2, 12), (5, 3, 3), CHAIN, bevel=0.4),
    ]
    return join_parts(p, "lm_forge_ruins")


def build_fire_pillar():
    """火柱: 岩座 + 多层火焰 + 上升火星。"""
    p = [
        part_cylinder("base", (0, 0, 3), 8, 6, OBSIDIAN, vertices=12),
        part_cylinder("base_lava", (0, 0, 6.2), 6.5, 0.8, LAVA, vertices=12),
    ]
    for i, (z, r, tilt) in enumerate(
        [(10, 5.0, 8), (16, 4.2, -10), (22, 3.2, 12), (28, 2.2, -8)]
    ):
        p.append(
            _spike(
                "flame_%d" % i,
                (tilt * 0.1, 0, z),
                r,
                9,
                LAVA if i % 2 == 0 else LAVA_LT,
            )
        )
    p.append(_spike("flame_top", (0, 0, 33), 1.6, 7, LAVA_LT))
    for i, (x, y, z) in enumerate([(-6, -3, 18), (7, 2, 24), (-4, 4, 30), (5, -5, 12)]):
        p.append(
            part_sphere("spark_%d" % i, (x, y, z), 0.9, LAVA_LT, segments=8, rings=4)
        )
    return join_parts(p, "lm_fire_pillar")


# ══════════ 虚空深渊 ══════════
def build_floating_altar():
    """浮空祭坛: 悬浮石板 + 幽光符文 + 下方浮石 + 空间扭曲环。"""
    p = [
        # 贴地虚空基座 (脚底 y=0; 同时作为空间扭曲的视觉锚)
        part_cylinder("ground_rift", (0, 0, 0.6), 9.5, 1.2, VOID_DK, vertices=16),
        part_box("slab", (0, 0, 14), (18, 12, 3), VOID_STONE, bevel=0.8, taper=0.92),
        part_box("slab_top", (0, 0, 16), (15, 9, 1.4), VOID_LT, bevel=0.4),
        part_sphere(
            "gem", (0, 0, 17.6), 1.5, VOID_GLOW, segments=14, rings=8, scale=(1, 1, 0.7)
        ),
    ]
    for i, (x, y) in enumerate([(-7, -4), (7, -4), (-7, 4), (7, 4)]):
        p.append(
            part_cylinder("pillar_%d" % i, (x, y, 18), 1.2, 5, VOID_DK, vertices=6)
        )
        p.append(
            part_sphere("orb_%d" % i, (x, y, 21), 1.0, VOID_GLOW, segments=8, rings=4)
        )
    for i, (x, y, z, r) in enumerate(
        [(-4, 2, 8, 2.2), (5, -1, 6, 2.6), (0, 3, 3, 1.8)]
    ):
        p.append(
            part_sphere("float_%d" % i, (x, y, z), r, VOID_DK, segments=8, rings=5)
        )
    return join_parts(p, "lm_floating_altar")


def build_void_crack():
    """虚空裂缝: 空间裂口 + 紫光渗出 + 扭曲碎石。"""
    p = [part_box("ground", (0, 0, 1), (30, 22, 2), VOID_DK, bevel=0.5)]
    p.append(
        _slanted_box(
            "tear", (0, 5, 14), (7, 1.6, 24), VOID_TEAR, 6.0, bevel=0.5, taper=0.6
        )
    )
    p.append(
        _slanted_box(
            "tear_glow",
            (0, 4.2, 14),
            (3, 1.0, 22),
            VOID_GLOW,
            6.0,
            bevel=0.3,
            taper=0.55,
        )
    )
    for i, (x, z, sg) in enumerate([(-8, 8, 1), (7, 11, -1), (-6, 18, 1), (8, 20, -1)]):
        p.append(
            _slanted_box(
                "shard_%d" % i,
                (x, 2, z),
                (3, 3, 7),
                VOID_STONE,
                sg * 25.0,
                bevel=0.4,
                taper=0.2,
            )
        )
    for i, (x, y, z) in enumerate([(-2, 1, 10), (2, 0, 16), (0, 2, 22)]):
        p.append(
            part_sphere("wisp_%d" % i, (x, y, z), 1.1, VOID_GLOW, segments=8, rings=4)
        )
    return join_parts(p, "lm_void_crack")


def build_ancient_gate():
    """远古石门: 双柱 + 门楣 + 发光符文 + 虚空门洞。"""
    p = [
        part_box("threshold", (0, 0, 1), (26, 10, 2), VOID_DK, bevel=0.5),
        part_box("pillar_l", (-9, 3, 14), (5, 6, 26), VOID_STONE, bevel=0.6),
        part_box("pillar_r", (9, 3, 14), (5, 6, 26), VOID_STONE, bevel=0.6),
        part_box("lintel", (0, 3, 28), (26, 6, 5), VOID_LT, bevel=0.6),
        part_box("doorway", (0, 3.5, 14), (13, 1.6, 24), VOID_TEAR, bevel=0.4),
        part_box("door_glow", (0, 2.6, 14), (10, 0.8, 20), VOID_GLOW, bevel=0.3),
    ]
    for i, (x, z) in enumerate([(-9, 8), (-9, 14), (-9, 20), (9, 8), (9, 14), (9, 20)]):
        p.append(
            part_box("rune_%d" % i, (x, 0.0, z), (2.4, 0.8, 2.4), VOID_GLOW, bevel=0.3)
        )
    p.append(
        part_sphere(
            "keystone",
            (0, 0.0, 28),
            2.4,
            VOID_GLOW,
            segments=12,
            rings=6,
            scale=(1, 0.5, 1),
        )
    )
    return join_parts(p, "lm_ancient_gate")


LANDMARK_BUILDERS = {
    "broken_cell": build_broken_cell,
    "torture_chamber": build_torture_chamber,
    "collapsed_tunnel": build_collapsed_tunnel,
    "lava_rift": build_lava_rift,
    "forge_ruins": build_forge_ruins,
    "fire_pillar": build_fire_pillar,
    "floating_altar": build_floating_altar,
    "void_crack": build_void_crack,
    "ancient_gate": build_ancient_gate,
}


def export_landmark(key):
    obj = LANDMARK_BUILDERS[key]()
    name = "lm_" + key
    path = os.path.join(REPO, "assets", "models", name + ".json")
    verts, faces = export_json(obj, path, name)
    return obj, verts, faces


def export_all():
    for key in LANDMARK_BUILDERS:
        _, v, f = export_landmark(key)
        print("EXPORT lm_%-20s verts=%d tris=%d" % (key, v, f))
    print("EXPORT_ALL_OK landmarks=%d" % len(LANDMARK_BUILDERS))


def preview_one(key):
    """MCP 内渲染单个地标预览。"""
    import bpy
    import blender_scene

    camera = blender_scene.setup_scene()
    obj = LANDMARK_BUILDERS[key]()
    bpy.context.view_layer.update()
    pts = [v.co for v in obj.data.vertices]
    low, high = min(p.z for p in pts), max(p.z for p in pts)
    cz = (low + high) * 0.5
    blender_scene.frame_camera(
        camera, (0, 0, cz), distance=95.0, azimuth=-52.0, elevation=22.0
    )
    out = os.path.join(REPO, "art_src", "preview", "landmark_preview_%s.png" % key)
    blender_scene.render_to(out)
    print("PREVIEW %s height=%.1f -> %s" % (key, high - low, out))
    return out
