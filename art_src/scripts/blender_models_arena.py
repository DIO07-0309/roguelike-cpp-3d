# A14: 竞技场/地牢场景道具低模 (Blender, Z-up, 1 单位 = 1 世界像素, 正面朝 -Y)
# ArenaObjectType 五型: 爆炸桶/治疗图腾/毒池/岩石/尖刺陷阱
import math

from blender_lib import (
    join_parts,
    part_box,
    part_cylinder,
    part_prism,
    part_sphere,
    part_torus,
)
from mathutils import Matrix

WOOD = (198, 126, 58)
WOOD_DARK = (148, 90, 40)
IRON_DARK = (104, 112, 124)
STONE = (116, 122, 134)
STONE_DARK = (82, 88, 100)
STONE_LIGHT = (146, 152, 164)
HEAL = (74, 214, 122)
HEAL_GLOW = (140, 244, 186)
POISON = (78, 158, 56)
POISON_LIGHT = (132, 214, 88)
RUST = (126, 92, 62)


def _detail_barrel(parts):
    """精修层: 铁箍铆钉 + 木纹竖板 + 警示斜纹 + 底圈 + 引信火星"""
    for z in (5.0, 12.0, 19.0):
        parts.append(part_torus("hoop_%d" % int(z), (0, 0, z), 9.7, 0.7,
                                IRON_DARK, major_segments=14, minor_segments=6))
    for k in range(8):
        ang = math.radians(k * 45)
        x, y = math.cos(ang) * 9.0, math.sin(ang) * 9.0
        parts.append(part_sphere("stud_%d" % k, (x, y, 12.0), 0.8, STONE_LIGHT,
                                 segments=8, rings=4))
    for k, z in enumerate((8.0, 16.0)):
        parts.append(part_box("warn_stripe_%d" % k, (0, -7.2, z),
                              (11.0, 0.5, 1.2), (200, 56, 50), bevel=0.1,
                              rotation_z=math.radians(18 * (1 if k == 0 else -1))))
    parts.append(part_torus("base_ring", (0, 0, 2.5), 9.4, 0.6,
                            WOOD_DARK, major_segments=14, minor_segments=6))
    parts.append(part_sphere("spark", (3.2, 0, 27.6), 0.7, (240, 170, 60),
                             segments=8, rings=4))


def build_arena_barrel():
    """爆炸桶: 木桶身 + 三道铁箍 + 红色警示带 + 顶盖 + 引信"""
    parts = []
    parts.append(part_cylinder("body", (0, 0, 12.0), 9.5, 24.0, WOOD, vertices=12))
    for z in (5.0, 19.0):
        parts.append(
            part_torus(
                "band",
                (0, 0, z),
                9.6,
                0.9,
                IRON_DARK,
                major_segments=14,
                minor_segments=6,
            )
        )
    parts.append(
        part_cylinder(
            "warning_band", (0, 0, 13.5), 9.7, 4.5, (200, 56, 50), vertices=12
        )
    )
    parts.append(part_cylinder("lid", (0, 0, 24.4), 9.0, 1.4, WOOD_DARK, vertices=12))
    parts.append(
        part_cylinder(
            "fuse",
            (2.5, 0, 26.4),
            0.7,
            4.4,
            (214, 192, 124),
            vertices=6,
            rotation=Matrix.Rotation(math.radians(24), 3, "Y"),
        )
    )
    _detail_barrel(parts)
    return join_parts(parts, "arena_barrel")


def _detail_totem(parts):
    """精修层: 腰带符文环 + 面部眉鼻口浮雕 + 底座分层 + 垂饰"""
    for z in (8.0, 12.0, 15.0):
        parts.append(part_torus("rune_band_%d" % int(z), (0, 0, z), 6.0, 0.5,
                                HEAL_GLOW, major_segments=14, minor_segments=6))
    for side in (-1, 1):
        parts.append(part_box("brow_%d" % side, (side * 2.4, -4.8, 24.2),
                              (2.8, 0.6, 0.7), STONE_DARK, bevel=0.1))
    parts.append(part_box("nose", (0, -4.9, 22.6), (1.0, 0.7, 1.8),
                          STONE_DARK, bevel=0.1))
    parts.append(part_box("mouth", (0, -4.8, 21.0), (3.2, 0.6, 0.7),
                          HEAL_GLOW, bevel=0.1))
    parts.append(part_cylinder("base_step", (0, 0, 2.0), 8.0, 2.0,
                               STONE_DARK, vertices=8))
    for k in range(4):
        ang = math.radians(k * 90 + 45)
        parts.append(part_sphere("pendant_%d" % k,
                                 (math.cos(ang) * 6.0, math.sin(ang) * 6.0, 13.0),
                                 0.9, HEAL_GLOW, segments=8, rings=4))


def build_arena_totem():
    """治疗图腾: 八棱石座 + 柱身 + 治愈符文腰带 + 石首(发光眼) + 顶冠双角"""
    parts = []
    parts.append(
        part_cylinder("base", (0, 0, 3.0), 8.5, 6.0, STONE, vertices=8, smooth=False)
    )
    parts.append(
        part_cylinder(
            "body", (0, 0, 12.0), 5.5, 12.0, STONE_LIGHT, vertices=8, smooth=False
        )
    )
    parts.append(
        part_cylinder("ring", (0, 0, 15.0), 6.2, 3.0, HEAL, vertices=8, smooth=False)
    )
    parts.append(
        part_box("head", (0, 0, 22.5), (10.0, 9.0, 9.0), STONE, bevel=0.8, taper=0.92)
    )
    for side in (-1, 1):
        parts.append(
            part_box(
                "eye", (side * 2.4, -4.7, 23.6), (2.6, 1.0, 2.2), HEAL_GLOW, bevel=0.3
            )
        )
    parts.append(
        part_cylinder(
            "crown", (0, 0, 28.0), 4.0, 3.5, STONE_DARK, vertices=6, smooth=False
        )
    )
    for side in (-1, 1):
        parts.append(
            part_cylinder(
                "horn",
                (side * 3.4, 0, 30.8),
                1.3,
                5.4,
                STONE_LIGHT,
                vertices=6,
                radius_top=0.2,
                rotation=Matrix.Rotation(math.radians(side * 30), 3, "Y"),
            )
        )
    _detail_totem(parts)
    return join_parts(parts, "arena_totem")


def _detail_rock(parts):
    """精修层: 碎裂棱面环 + 苔痕 + 小碎石簇 + 裂缝线"""
    for i, (x, y, z, r) in enumerate([
        (-4, -2, 8, 2.4), (3, 2, 9, 2.6), (0, 0, 11, 2.8), (4, -2, 6, 2.2)
    ]):
        parts.append(part_torus("facet_ring_%d" % i, (x, y, z), r, 0.6,
                                STONE_LIGHT, major_segments=14, minor_segments=6))
    for i, (x, y, z) in enumerate([(-5, -4, 11), (4, 3, 12), (0, 5, 9)]):
        parts.append(part_sphere("moss_%d" % i, (x, y, z), 1.8,
                                 (58, 92, 48), segments=8, rings=4,
                                 scale=(1.2, 1.0, 0.5)))
    for i, (x, y) in enumerate([(-6, 5), (7, 4), (-5, -5), (8, -3), (2, -6)]):
        parts.append(part_sphere("pebble_%d" % i, (x, y, 1.0), 1.4,
                                 STONE_DARK, segments=8, rings=4,
                                 scale=(1.0, 1.0, 0.7)))
    for i, (x, y, ang) in enumerate([(-4, 2, 25), (4, -3, -20)]):
        parts.append(part_box("crack_%d" % i, (x, y, 10.5), (0.5, 6.0, 0.4),
                              STONE_DARK, bevel=0.1, rotation_z=math.radians(ang)))


def build_arena_rock():
    """岩石: 主石块(压扁球) + 侧碎石 + 顶部棱面石片"""
    parts = []
    parts.append(
        part_sphere(
            "main",
            (0, 0, 6.4),
            9.0,
            STONE,
            segments=10,
            rings=7,
            scale=(1.0, 0.92, 0.72),
        )
    )
    parts.append(
        part_sphere(
            "chunk",
            (7.2, 4.0, 3.0),
            4.2,
            STONE_DARK,
            segments=8,
            rings=5,
            scale=(1.0, 0.9, 0.72),
        )
    )
    parts.append(
        part_prism(
            "facet",
            (0, -1.5, 12.6),
            [(-4.5, -2.2), (4.0, -3.0), (5.0, 2.4), (-3.5, 3.2)],
            2.0,
            STONE_LIGHT,
        )
    )
    _detail_rock(parts)
    return join_parts(parts, "arena_rock")


def _detail_spike(parts):
    """精修层: 钉身倒刺环 + 血槽 + 基板格栅 + 铆钉帽 + 锈斑"""
    for i, (x, y, h) in enumerate(
        ((0, 0, 11.0), (-7, -7, 8.0), (7, -7, 8.0), (-7, 7, 8.0), (7, 7, 8.0))
    ):
        for k, z in enumerate((h * 0.35 + 2.0, h * 0.65 + 2.0)):
            parts.append(part_torus("barb_%d_%d" % (i, k), (x, y, z), 1.5, 0.45,
                                    IRON_DARK, major_segments=12, minor_segments=6))
        parts.append(part_box("groove_%d" % i, (x, y - 1.8, h * 0.5 + 2.0),
                              (0.5, 0.5, h * 0.7), STONE_DARK, bevel=0.1))
    for i in (-8, 0, 8):
        parts.append(part_box("grate_x_%d" % i, (i, 0, 2.1),
                              (0.6, 24.0, 0.5), IRON_DARK, bevel=0.1))
        parts.append(part_box("grate_y_%d" % i, (0, i, 2.1),
                              (24.0, 0.6, 0.5), IRON_DARK, bevel=0.1))
    for corner in ((-11, -11), (11, -11), (-11, 11), (11, 11)):
        parts.append(part_sphere("rivet_cap", (corner[0], corner[1], 3.2), 1.8,
                                 STONE_LIGHT, segments=8, rings=4))
    for i, (x, y) in enumerate([(-5, -9), (6, 8), (-9, 3)]):
        parts.append(part_sphere("rust_%d" % i, (x, y, 2.05), 1.2, RUST,
                                 segments=8, rings=4, scale=(1.0, 1.0, 0.3)))


def build_arena_spike():
    """尖刺陷阱: 铁基板 + 五根圆锥钉 (中1周4)"""
    parts = []
    parts.append(
        part_box("plate", (0, 0, 1.0), (26.0, 26.0, 2.0), (72, 76, 88), bevel=1.0)
    )
    for index, (x, y, h) in enumerate(
        ((0, 0, 11.0), (-7, -7, 8.0), (7, -7, 8.0), (-7, 7, 8.0), (7, 7, 8.0))
    ):
        parts.append(
            part_cylinder(
                "spike_%d" % index,
                (x, y, h * 0.5 + 2.0),
                3.0,
                h,
                RUST,
                vertices=6,
                radius_top=0.1,
                smooth=False,
            )
        )
    for corner in ((-11, -11), (11, -11), (-11, 11), (11, 11)):
        parts.append(
            part_cylinder(
                "rivet",
                (corner[0], corner[1], 2.3),
                1.4,
                1.4,
                STONE_DARK,
                vertices=6,
                smooth=False,
            )
        )
    _detail_spike(parts)
    return join_parts(parts, "arena_spike")


def _detail_pool(parts):
    """精修层: 涟漪环 + 气泡簇 + 边缘结晶 + 漂浮碎屑"""
    for i, (major, z) in enumerate([(3.5, 2.6), (7.0, 2.5), (10.5, 2.3)]):
        parts.append(part_torus("ripple_%d" % i, (0, 0, z), major, 0.4,
                                POISON_LIGHT, major_segments=16, minor_segments=6))
    for i, (x, y, r) in enumerate([
        (-7, 4, 1.4), (6, -5, 1.2), (2, 8, 1.0), (-9, -6, 0.9), (9, 6, 1.1),
        (0, -2, 0.8), (-4, 9, 0.7), (8, 1, 0.9)
    ]):
        parts.append(part_sphere("bubble_%d" % i, (x, y, 3.0 + r * 0.4), r,
                                 (150, 220, 104), segments=8, rings=4,
                                 scale=(1.0, 1.0, 0.7)))
    for i in range(8):
        ang = math.radians(i * 45)
        parts.append(part_prism("crystal_%d" % i,
                                (math.cos(ang) * 11.5, math.sin(ang) * 11.5, 2.0),
                                [(-1.0, -1.0), (1.0, -1.0), (0, 1.5)],
                                2.2, POISON_LIGHT, bevel=0.1,
                                rotation_z=ang))
    for i, (x, y) in enumerate([(-3, -8), (5, 7), (-8, 2)]):
        parts.append(part_box("debris_%d" % i, (x, y, 2.6), (2.0, 1.2, 0.5),
                              POISON, bevel=0.1, rotation_z=math.radians(i * 40)))


def build_arena_pool():
    """毒池: 不规则液面厚片 + 略高内层 + 两颗气泡 (贴地)"""
    parts = []
    outer = []
    for index in range(12):
        angle = math.radians(index * 30)
        radius = 13.0 + 2.4 * math.sin(index * 2.1) + 1.2 * math.cos(index * 3.3)
        outer.append((math.cos(angle) * radius, math.sin(angle) * radius))
    parts.append(part_prism("outer", (0, 0, 0.9), outer, 1.8, POISON, bevel=0.5))
    inner = [(x * 0.72, y * 0.72) for x, y in outer]
    parts.append(part_prism("inner", (0, 0, 2.0), inner, 1.2, POISON_LIGHT, bevel=0.4))
    parts.append(
        part_sphere(
            "bubble", (-4.5, 3.0, 3.4), 2.0, (168, 232, 118), segments=8, rings=5
        )
    )
    parts.append(
        part_sphere(
            "bubble", (5.0, -2.5, 3.0), 1.4, (150, 220, 104), segments=8, rings=5
        )
    )
    _detail_pool(parts)
    return join_parts(parts, "arena_pool")
