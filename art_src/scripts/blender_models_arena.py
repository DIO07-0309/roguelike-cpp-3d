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
    return join_parts(parts, "arena_barrel")


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
    return join_parts(parts, "arena_totem")


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
    return join_parts(parts, "arena_rock")


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
    return join_parts(parts, "arena_spike")


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
    return join_parts(parts, "arena_pool")
