# A14: 挑战房装饰 (立柱火盆 / 挂旗 / 传送门框三态) — 高细节
# Blender Z-up, 1 单位 = 1 世界像素; 正面朝 -Y (导出后 = 引擎 +Z)
import math

from blender_lib import (
    join_parts,
    part_box,
    part_cylinder,
    part_prism,
    part_sphere,
    part_torus,
)
from mathutils import Matrix, Vector

STONE = (104, 110, 126)
STONE_DARK = (74, 80, 96)
STONE_LIGHT = (138, 144, 160)
STONE_COLD = (70, 76, 92)
IRON = (146, 156, 168)
IRON_DARK = (94, 102, 114)
GOLD = (216, 172, 62)
BONE = (226, 220, 200)
BONE_DARK = (150, 142, 126)
CLOTH_RED = (150, 52, 50)
CLOTH_DARK = (74, 34, 40)
COAL = (44, 42, 46)
FIRE = (255, 126, 42)
FIRE_CORE = (255, 216, 140)
RUNE_DORMANT = (52, 52, 60)
RUNE_SEALED = (222, 78, 58)
RUNE_ACTIVE = (98, 242, 220)
GLOW_SEALED = (168, 44, 40)
GLOW_ACTIVE = (74, 210, 200)
MOSS = (86, 132, 74)


def _octagon_parts(parts, center, radius, height, color, segments=8):
    parts.append(part_cylinder("oct", center, radius, height, color, vertices=segments))


def build_challenge_pillar(used=False):
    """挑战房立柱: 八棱基座 + 凹槽柱身 + 柱头 + 火盆 (used=火盆熄灭)"""
    parts = []
    # 基座三层
    _octagon_parts(parts, (0, 0, 0.7), 5.0, 1.4, STONE_DARK)
    _octagon_parts(parts, (0, 0, 1.9), 4.2, 1.0, STONE)
    _octagon_parts(parts, (0, 0, 2.9), 3.4, 1.0, STONE_LIGHT)
    # 柱身 + 8 条凹槽
    parts.append(part_cylinder("shaft", (0, 0, 12.6), 2.9, 18.4, STONE, vertices=8))
    for index in range(8):
        angle = 2.0 * math.pi * index / 8.0
        parts.append(
            part_box(
                "flute_%d" % index,
                (math.cos(angle) * 2.86, math.sin(angle) * 2.86, 12.6),
                (0.62, 0.62, 17.4),
                STONE_LIGHT,
                bevel=0.15,
                rotation_z=angle,
            )
        )
    # 刻纹环带
    for height in (5.4, 19.4):
        parts.append(
            part_torus(
                "band",
                (0, 0, height),
                2.98,
                0.34,
                STONE_DARK,
                major_segments=12,
                minor_segments=6,
            )
        )
    # 柱身符文 (四面菱形)
    for index in range(4):
        angle = math.pi * 0.5 * index + math.pi * 0.25
        parts.append(
            part_prism(
                "rune_%d" % index,
                (math.cos(angle) * 2.9, math.sin(angle) * 2.9, 16.0),
                [(0, 1.1), (0.9, 0), (0, -1.1), (-0.9, 0)],
                0.5,
                RUNE_DORMANT if used else RUNE_ACTIVE,
                bevel=0.1,
                rotation_z=angle,
            )
        )
    # 柱头
    parts.append(
        part_cylinder("capital", (0, 0, 22.4), 3.8, 1.6, STONE_LIGHT, vertices=8)
    )
    parts.append(part_box("abacus", (0, 0, 23.6), (8.2, 8.2, 1.0), STONE, bevel=0.3))
    # 火盆
    parts.append(
        part_cylinder(
            "bowl", (0, 0, 25.6), 3.2, 2.6, IRON_DARK, vertices=12, radius_top=3.6
        )
    )
    parts.append(
        part_torus(
            "bowl_rim",
            (0, 0, 26.9),
            3.5,
            0.35,
            IRON,
            major_segments=12,
            minor_segments=6,
        )
    )
    if used:
        parts.append(
            part_cylinder("ash", (0, 0, 26.6), 3.0, 0.6, (92, 88, 84), vertices=12)
        )
        for index, offset in enumerate(((-1.2, 0.4), (1.0, -0.6))):
            parts.append(
                part_sphere(
                    "coal_%d" % index,
                    (offset[0], offset[1], 26.9),
                    0.9,
                    COAL,
                    segments=8,
                    rings=4,
                )
            )
    else:
        parts.append(part_cylinder("coals", (0, 0, 26.7), 3.0, 0.9, COAL, vertices=12))
        for index, offset in enumerate(((-1.3, 0.2), (0.2, 1.1), (1.3, -0.4))):
            parts.append(
                part_cylinder(
                    "flame_%d" % index,
                    (offset[0], offset[1], 28.6),
                    0.9,
                    3.0,
                    FIRE,
                    vertices=8,
                    radius_top=0.12,
                    smooth=False,
                )
            )
        parts.append(
            part_sphere("fire_core", (0, 0, 27.8), 1.4, FIRE_CORE, segments=10, rings=6)
        )
    # 悬挂铁链
    for index in range(4):
        angle = math.pi * 0.5 * index + math.pi * 0.25
        x = math.cos(angle) * 3.4
        y = math.sin(angle) * 3.4
        parts.append(
            part_cylinder(
                "chain_%d" % index, (x, y, 18.4), 0.22, 7.0, IRON_DARK, vertices=6
            )
        )
        for link in range(3):
            parts.append(
                part_torus(
                    "chain_link",
                    (x, y, 15.2 + link * 2.2),
                    0.5,
                    0.14,
                    IRON,
                    major_segments=8,
                    minor_segments=4,
                )
            )
    # 苔藓
    for index, offset in enumerate((-2.4, 2.2)):
        parts.append(
            part_box(
                "moss_%d" % index, (offset, -2.6, 3.2), (2.6, 1.0, 0.7), MOSS, bevel=0.3
            )
        )
    return join_parts(
        parts, "room_challenge_pillar_used" if used else "room_challenge_pillar"
    )


def build_challenge_banner(used=False):
    """挑战房挂旗: 横杆(顶端) + 三层垂布 + 金边 + 符文 + 流苏; used=落地揉皱
    模型原点在旗帜底端 (z=0), 引擎侧抬高挂在墙上"""
    parts = []
    if used:
        parts.append(
            part_box(
                "cloth_a",
                (0, 0, 0.35),
                (11.0, 8.0, 0.7),
                CLOTH_DARK,
                bevel=0.2,
                rotation_z=math.radians(16.0),
            )
        )
        parts.append(
            part_box(
                "cloth_b",
                (1.6, 1.2, 0.95),
                (8.0, 5.0, 0.6),
                CLOTH_RED,
                bevel=0.2,
                rotation_z=math.radians(-22.0),
            )
        )
        parts.append(
            part_cylinder(
                "rod",
                (-2.0, -4.6, 0.5),
                0.34,
                12.0,
                IRON_DARK,
                vertices=8,
                rotation=Matrix.Rotation(math.radians(90), 3, "Y"),
            )
        )
        return join_parts(parts, "room_challenge_banner_used")
    rod_z = 9.2
    parts.append(
        part_cylinder(
            "rod",
            (0, 0, rod_z),
            0.34,
            13.0,
            IRON_DARK,
            vertices=10,
            rotation=Matrix.Rotation(math.radians(90), 3, "Y"),
        )
    )
    for sign in (-1, 1):
        parts.append(
            part_sphere(
                "finial", (sign * 6.6, 0, rod_z), 0.7, GOLD, segments=10, rings=6
            )
        )
    layers = (
        (0, 7.2, (11.0, 0.5, 4.0), CLOTH_RED),
        (-1.8, 4.4, (8.6, 0.5, 3.4), CLOTH_DARK),
        (-2.6, 2.0, (6.2, 0.5, 3.0), CLOTH_RED),
    )
    for index, (offset_x, offset_z, size, color) in enumerate(layers):
        parts.append(
            part_box(
                "cloth_%d" % index, (offset_x, 0, offset_z), size, color, bevel=0.25
            )
        )
    for sign in (-1, 1):
        parts.append(
            part_box(
                "swallowtail",
                (sign * 2.0, 0, 0.9),
                (2.2, 0.5, 1.8),
                CLOTH_RED,
                bevel=0.2,
                rotation_z=math.radians(sign * 24.0),
            )
        )
    parts.append(
        part_box("trim_top", (0, -0.1, 9.0), (11.2, 0.7, 0.6), GOLD, bevel=0.15)
    )
    for sign in (-1, 1):
        parts.append(
            part_box(
                "trim_side", (sign * 5.2, -0.1, 6.6), (0.5, 0.7, 4.6), GOLD, bevel=0.12
            )
        )
    parts.append(
        part_prism(
            "rune",
            (0, -0.5, 5.4),
            [(0, 1.6), (1.3, 0), (0, -1.6), (-1.3, 0)],
            0.5,
            GOLD,
            bevel=0.12,
        )
    )
    parts.append(
        part_torus(
            "rune_ring",
            (0, -0.5, 5.4),
            2.2,
            0.16,
            GOLD,
            major_segments=14,
            minor_segments=5,
            rotation=Matrix.Rotation(math.radians(90), 3, "X"),
        )
    )
    for index, offset in enumerate((-3.4, 0.0, 3.4)):
        parts.append(
            part_cylinder(
                "tassel_%d" % index, (offset, 0, 1.2), 0.3, 2.2, GOLD, vertices=6
            )
        )
    return join_parts(parts, "room_challenge_banner")


def _skull(parts, center):
    """门楣骷髅 (颅 + 眼窝 + 牙)"""
    cx, cy, cz = center
    parts.append(
        part_sphere(
            "skull",
            (cx, cy, cz),
            1.5,
            BONE,
            segments=12,
            rings=8,
            scale=(1.0, 0.92, 1.0),
        )
    )
    parts.append(
        part_box("jaw", (cx, cy - 0.2, cz - 1.3), (2.0, 1.4, 1.0), BONE, bevel=0.3)
    )
    for sign in (-1, 1):
        parts.append(
            part_sphere(
                "socket",
                (cx + sign * 0.62, cy - 1.1, cz + 0.2),
                0.55,
                BONE_DARK,
                segments=8,
                rings=4,
                scale=(1.0, 0.5, 1.0),
            )
        )
    parts.append(
        part_box(
            "teeth", (cx, cy - 1.2, cz - 1.3), (1.4, 0.4, 0.5), BONE_DARK, bevel=0.1
        )
    )


def build_challenge_gate(state="dormant"):
    """传送门框: 石基 + 双柱 + 门楣 + 拱石 + 符文 + 骷髅 + 铁链
    state: dormant(沉睡) / sealed(战斗封印·红) / active(激活·青)"""
    stone = (
        STONE_COLD
        if state == "dormant"
        else (58, 54, 62)
        if state == "sealed"
        else (66, 74, 96)
    )
    stone_light = (
        STONE_DARK
        if state == "dormant"
        else (78, 72, 82)
        if state == "sealed"
        else (92, 104, 132)
    )
    rune = (
        RUNE_DORMANT
        if state == "dormant"
        else RUNE_SEALED
        if state == "sealed"
        else RUNE_ACTIVE
    )
    glow = (
        None
        if state == "dormant"
        else GLOW_SEALED
        if state == "sealed"
        else GLOW_ACTIVE
    )
    parts = []
    # 台阶
    parts.append(
        part_box("step_low", (0, 0, 0.8), (40.0, 18.0, 1.6), stone_light, bevel=0.4)
    )
    parts.append(
        part_box("step_high", (0, 0, 2.2), (36.0, 15.0, 1.4), stone, bevel=0.35)
    )
    # 双柱
    for sign in (-1, 1):
        x = sign * 15.0
        parts.append(part_box("post", (x, 0, 14.4), (5.2, 5.2, 24.0), stone, bevel=0.7))
        parts.append(
            part_box("post_base", (x, 0, 3.8), (7.0, 7.0, 2.0), stone_light, bevel=0.5)
        )
        parts.append(
            part_box("post_cap", (x, 0, 27.0), (7.2, 7.2, 2.2), stone_light, bevel=0.5)
        )
        # 柱身三道刻槽
        for groove in (-6.0, 0.0, 6.0):
            parts.append(
                part_box(
                    "groove",
                    (x, -2.7, 14.4 + groove),
                    (0.9, 0.5, 4.0),
                    stone_light,
                    bevel=0.1,
                )
            )
        # 侧符文
        for rune_z in (10.0, 19.0):
            parts.append(
                part_prism(
                    "side_rune",
                    (x, -2.9, rune_z),
                    [(0, 1.2), (1.0, 0), (0, -1.2), (-1.0, 0)],
                    0.4,
                    rune,
                    bevel=0.08,
                )
            )
    # 门楣 + 拱石
    parts.append(part_box("lintel", (0, 0, 30.0), (36.0, 6.0, 4.6), stone, bevel=0.8))
    for sign in (-1, 1):
        parts.append(
            part_box(
                "arch_stone",
                (sign * 9.0, 0, 27.4),
                (7.0, 5.6, 2.4),
                stone_light,
                bevel=0.4,
                rotation_z=math.radians(sign * 12.0),
            )
        )
    parts.append(
        part_box("lintel_top", (0, 0, 32.6), (38.0, 7.0, 1.6), stone_light, bevel=0.4)
    )
    # 拱心石 + 符文
    parts.append(
        part_prism(
            "keystone",
            (0, -1.0, 30.0),
            [(0, 3.2), (2.4, 0), (0, -3.2), (-2.4, 0)],
            2.2,
            stone_light,
            bevel=0.3,
        )
    )
    parts.append(
        part_prism(
            "keystone_rune",
            (0, -2.4, 30.0),
            [(0, 1.8), (1.3, 0), (0, -1.8), (-1.3, 0)],
            0.6,
            rune,
            bevel=0.1,
        )
    )
    # 骷髅 (三颗)
    for index, offset in enumerate((-11.0, 0.0, 11.0)):
        _skull(parts, (offset, -2.6, 34.4))
    # 铁链 (sealed 时横锁门洞)
    for sign in (-1, 1):
        parts.append(
            part_cylinder(
                "chain", (sign * 12.6, -2.0, 16.0), 0.3, 12.0, IRON_DARK, vertices=6
            )
        )
        for link in range(4):
            parts.append(
                part_torus(
                    "chain_link",
                    (sign * 12.6, -2.0, 11.0 + link * 3.0),
                    0.7,
                    0.18,
                    IRON,
                    major_segments=8,
                    minor_segments=4,
                )
            )
    if state == "sealed":
        for index, height in enumerate((12.0, 20.0)):
            parts.append(
                part_box(
                    "lock_bar_%d" % index,
                    (0, -2.2, height),
                    (28.0, 0.9, 1.0),
                    IRON_DARK,
                    bevel=0.15,
                )
            )
        parts.append(
            part_box("lock", (0, -2.8, 16.0), (4.4, 1.6, 4.4), IRON, bevel=0.4)
        )
        parts.append(
            part_sphere("lock_rune", (0, -3.8, 16.0), 1.2, glow, segments=10, rings=6)
        )
    if state == "active":
        # 门洞内发光面 + 四角光点
        parts.append(
            part_box("portal_glow", (0, 1.6, 16.0), (24.0, 0.6, 22.0), glow, bevel=0.2)
        )
        for sign_x in (-1, 1):
            for sign_z in (-1, 1):
                parts.append(
                    part_sphere(
                        "spark",
                        (sign_x * 9.6, -1.4, 16.0 + sign_z * 8.0),
                        0.9,
                        rune,
                        segments=8,
                        rings=5,
                    )
                )
    # 苔藓 / 地面碎石
    for index, (x, z) in enumerate(((-17.0, 1.4), (16.0, 1.2), (6.0, 1.0))):
        parts.append(
            part_box(
                "rubble_%d" % index,
                (x, -6.0, z),
                (2.4, 1.6, 1.0),
                stone_light,
                bevel=0.3,
            )
        )
    return join_parts(
        parts,
        "room_challenge_gate_" + state if state != "dormant" else "room_challenge_gate",
    )
