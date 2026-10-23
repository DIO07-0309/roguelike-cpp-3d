# A14: 场景道具低模建模 (Blender, Z-up, 1 单位 = 1 世界像素)
# 正面朝 -Y (导出后 = 引擎 +Z, 正对相机)
# 每个道具分「未使用 / 已使用」两态: 使用后保留残迹 (开箱 / 灰暗祭坛 / 干涸泉水)
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

WOOD = (198, 126, 58)
WOOD_DARK = (148, 90, 40)
WOOD_LIGHT = (218, 152, 84)
WOOD_OLD = (146, 108, 70)
WOOD_OLD_DARK = (104, 76, 50)
IRON = (152, 162, 174)
IRON_DARK = (104, 112, 124)
IRON_RUST = (126, 92, 62)
GOLD = (222, 178, 66)
GOLD_LIGHT = (242, 208, 110)
STONE = (116, 122, 134)
STONE_DARK = (82, 88, 100)
STONE_LIGHT = (146, 152, 164)
STONE_BLUE = (44, 54, 84)
STONE_BLUE_LIGHT = (66, 80, 122)
STONE_USED = (78, 82, 92)
STONE_USED_LIGHT = (98, 102, 112)
CORE = (74, 214, 122)
CORE_GLOW = (140, 244, 186)
CORE_DEAD = (52, 46, 44)
CORE_EMBER = (120, 64, 40)
WATER = (62, 158, 96)
WATER_LIGHT = (110, 206, 148)
MUD = (96, 84, 62)
MUD_DARK = (68, 58, 44)
MOSS = (86, 132, 74)
MOSS_DRY = (128, 116, 70)
LEATHER = (108, 70, 40)
STRAW = (186, 156, 92)


def _rivets(parts, positions, radius=0.55, height=0.7):
    for index, position in enumerate(positions):
        parts.append(
            part_cylinder(
                "rivet_%d" % index,
                position,
                radius,
                height,
                IRON_DARK,
                vertices=8,
                rotation=Matrix.Rotation(math.radians(90), 3, "X"),
            )
        )


def _orbit(center, pivot, angle_x):
    """绕 X 轴指定铰点旋转后的中心 (箱盖开合用)"""
    local = Vector(center) - Vector(pivot)
    return tuple(Vector(pivot) + (Matrix.Rotation(angle_x, 3, "X") @ local))


def build_chest(used=False):
    """宝箱: 木板箱身 + 弧形箱盖 + 铁箍铆钉 + 金锁; used=开盖空箱"""
    wood = WOOD_OLD if used else WOOD
    wood_dark = WOOD_OLD_DARK if used else WOOD_DARK
    parts = []
    parts.append(
        part_box("body", (0, 0, 6.4), (22.0, 15.0, 12.8), wood, bevel=0.9, segments=2)
    )
    for offset in (-6.0, -2.0, 2.0, 6.0):
        parts.append(
            part_box(
                "plank_seam",
                (offset, -7.4, 6.4),
                (0.7, 0.6, 11.0),
                wood_dark,
                bevel=0.2,
            )
        )
    if used:
        # 空箱内部 (深色) + 残留稻草 + 几枚金币
        parts.append(
            part_box(
                "inner", (0, 0.4, 11.0), (19.6, 12.6, 3.0), (48, 36, 26), bevel=0.3
            )
        )
        for index, offset in enumerate((-6.0, 0.0, 5.0)):
            parts.append(
                part_box(
                    "straw_%d" % index,
                    (offset, 0.6, 11.6),
                    (4.4, 6.0, 1.2),
                    STRAW,
                    bevel=0.3,
                )
            )
        for index, offset in enumerate((-3.4, 0.6, 3.8)):
            parts.append(
                part_cylinder(
                    "coin_%d" % index, (offset, -1.6, 12.0), 1.5, 0.4, GOLD, vertices=10
                )
            )
    # 箱盖 (used: 绕后铰点翻起 ~105°)
    hinge = (0.0, 7.4, 13.0)
    angle = math.radians(-105.0) if used else 0.0
    lid_specs = (
        ("lid_low", (0, 0, 13.6), (21.4, 14.6, 3.4), wood_dark, 0.8),
        ("lid_mid", (0, 0, 15.6), (20.0, 13.6, 2.4), wood, 0.8),
        (
            "lid_top",
            (0, 0, 17.0),
            (17.4, 11.4, 1.8),
            WOOD_LIGHT if not used else WOOD_OLD,
            0.7,
        ),
    )
    for name, center, size, color, bevel in lid_specs:
        parts.append(
            part_box(
                name,
                _orbit(center, hinge, angle),
                size,
                color,
                bevel=bevel,
                rotation_x=angle,
            )
        )
    for offset in (-7.4, 7.4):
        parts.append(
            part_box(
                "band_front", (offset, -7.6, 7.0), (2.4, 1.0, 13.6), IRON, bevel=0.4
            )
        )
        parts.append(
            part_box(
                "band_back", (offset, 7.6, 7.0), (2.4, 1.0, 13.6), IRON_DARK, bevel=0.4
            )
        )
        parts.append(
            part_box(
                "band_lid",
                _orbit((offset, 0, 16.4), hinge, angle),
                (2.4, 14.4, 4.4),
                IRON_RUST if used else IRON,
                bevel=0.5,
                rotation_x=angle,
            )
        )
        _rivets(parts, [(offset, -8.3, 4.2), (offset, -8.3, 10.0)])
    # 锁 (used: 挂在箱前下垂)
    parts.append(
        part_box("lock_plate", (0, -8.2, 9.2), (5.6, 1.2, 6.0), IRON_DARK, bevel=0.5)
    )
    if used:
        parts.append(
            part_box(
                "lock",
                _orbit((0, -8.8, 7.0), (0, -8.2, 10.6), math.radians(150.0)),
                (3.4, 1.2, 3.4),
                GOLD,
                bevel=0.6,
                rotation_x=math.radians(150.0),
            )
        )
    else:
        parts.append(part_box("lock", (0, -8.8, 8.0), (3.6, 1.2, 3.6), GOLD, bevel=0.6))
        parts.append(
            part_cylinder(
                "keyhole",
                (0, -9.5, 8.0),
                0.7,
                0.6,
                (40, 32, 24),
                vertices=8,
                rotation=Matrix.Rotation(math.radians(90), 3, "X"),
            )
        )
        parts.append(
            part_sphere(
                "lock_gem", (0, -9.2, 9.6), 0.7, GOLD_LIGHT, segments=8, rings=4
            )
        )
    for sign_x in (-1, 1):
        for sign_y in (-1, 1):
            parts.append(
                part_box(
                    "foot",
                    (sign_x * 8.6, sign_y * 5.4, 0.9),
                    (3.4, 3.4, 1.8),
                    IRON_DARK,
                    bevel=0.5,
                )
            )
    return join_parts(parts, "chest_open" if used else "chest")


def build_altar(used=False):
    """祭坛: 阶梯底座 + 凹槽立柱 + 顶板 + 发光绿核; used=灰暗熄灭"""
    stone = STONE_USED if used else STONE_BLUE
    stone_light = STONE_USED_LIGHT if used else STONE_BLUE_LIGHT
    parts = []
    parts.append(part_box("base_low", (0, 0, 1.6), (20.0, 17.0, 3.2), stone, bevel=0.8))
    parts.append(
        part_box("base_mid", (0, 0, 3.6), (16.0, 13.4, 2.0), stone_light, bevel=0.6)
    )
    parts.append(
        part_box("base_step", (0, 0, 5.2), (13.0, 11.0, 1.6), stone, bevel=0.5)
    )
    parts.append(part_cylinder("column", (0, 0, 12.4), 5.0, 12.8, stone, vertices=12))
    for index in range(8):
        angle = 2.0 * math.pi * index / 8.0
        parts.append(
            part_box(
                "flute_%d" % index,
                (math.cos(angle) * 4.9, math.sin(angle) * 4.9, 12.4),
                (1.1, 1.1, 11.4),
                stone_light,
                bevel=0.3,
                rotation_z=angle,
            )
        )
    parts.append(
        part_cylinder("column_base", (0, 0, 6.6), 5.8, 1.8, stone_light, vertices=12)
    )
    parts.append(
        part_box("capital", (0, 0, 19.4), (14.0, 12.0, 2.0), stone_light, bevel=0.6)
    )
    parts.append(
        part_box("top_slab", (0, 0, 21.4), (18.0, 15.0, 2.4), stone, bevel=0.8)
    )
    parts.append(
        part_box("top_crest", (0, 0, 23.2), (10.0, 7.0, 1.6), stone_light, bevel=0.6)
    )
    if used:
        # 熄灭的核心: 焦黑石壳 + 一点余烬
        parts.append(
            part_sphere("core", (0, -4.2, 12.0), 2.6, CORE_DEAD, segments=14, rings=8)
        )
        parts.append(
            part_sphere(
                "core_ember", (0, -4.9, 11.4), 0.8, CORE_EMBER, segments=8, rings=4
            )
        )
        parts.append(
            part_torus(
                "core_ring",
                (0, -4.4, 12.0),
                3.2,
                0.6,
                IRON_RUST,
                major_segments=14,
                minor_segments=6,
                rotation=Matrix.Rotation(math.radians(90), 3, "X"),
            )
        )
        for index, offset in enumerate((-3.2, 0.0, 3.2)):
            parts.append(
                part_box(
                    "rune_%d" % index,
                    (offset, -4.6, 7.4),
                    (0.9, 0.5, 3.2),
                    (62, 58, 54),
                    bevel=0.2,
                )
            )
        # 裂纹 (细深色条)
        for index, (offset, height, tilt) in enumerate(
            ((-2.0, 9.0, 0.4), (1.6, 15.0, -0.5), (3.4, 5.6, 0.2))
        ):
            parts.append(
                part_box(
                    "crack_%d" % index,
                    (offset, -4.8, height),
                    (0.5, 0.4, 4.6),
                    (44, 40, 38),
                    bevel=0.1,
                    rotation_z=tilt,
                )
            )
        for index, offset in enumerate((-7.0, -2.0, 5.4)):
            parts.append(
                part_box(
                    "moss_%d" % index,
                    (offset, -6.0, 4.4),
                    (3.4, 1.2, 0.8),
                    MOSS_DRY,
                    bevel=0.3,
                )
            )
    else:
        parts.append(
            part_sphere("core", (0, -4.2, 12.0), 2.6, CORE, segments=14, rings=8)
        )
        parts.append(
            part_sphere(
                "core_glow", (0, -4.8, 12.0), 1.4, CORE_GLOW, segments=10, rings=6
            )
        )
        parts.append(
            part_torus(
                "core_ring",
                (0, -4.4, 12.0),
                3.2,
                0.6,
                IRON_DARK,
                major_segments=14,
                minor_segments=6,
                rotation=Matrix.Rotation(math.radians(90), 3, "X"),
            )
        )
        for index, offset in enumerate((-3.2, 0.0, 3.2)):
            parts.append(
                part_box(
                    "rune_%d" % index,
                    (offset, -4.6, 7.4),
                    (0.9, 0.5, 3.2),
                    CORE_GLOW,
                    bevel=0.2,
                )
            )
        for index, offset in enumerate((-7.0, -2.0, 5.4)):
            parts.append(
                part_box(
                    "moss_%d" % index,
                    (offset, -6.0, 4.4),
                    (3.4, 1.2, 0.8),
                    MOSS,
                    bevel=0.3,
                )
            )
    return join_parts(parts, "altar_used" if used else "altar")


def build_spring(used=False):
    """泉水: 不规则石环 + 池底 + 水面涟漪 + 木桶; used=干涸开裂"""
    parts = []
    stones = 9
    for index in range(stones):
        angle = 2.0 * math.pi * index / stones + 0.2
        radius = 12.0 + (1.1 if index % 3 == 0 else 0.0)
        height = 5.4 + (1.2 if index % 4 == 1 else 0.0)
        if used and index % 5 == 0:
            height += 1.4  # 干涸后石块更歪斜
        parts.append(
            part_box(
                "stone_%d" % index,
                (math.cos(angle) * radius, math.sin(angle) * radius, height * 0.5),
                (10.4, 4.6, height),
                STONE_USED
                if used and index % 2
                else (STONE if index % 2 else STONE_DARK),
                bevel=0.8,
                segments=2,
                rotation_z=angle + math.radians(22.0 * ((index % 3) - 1)),
            )
        )
    parts.append(
        part_cylinder(
            "pool_floor",
            (0, 0, 1.2),
            9.6,
            2.4,
            MUD_DARK if used else STONE_DARK,
            vertices=16,
        )
    )
    if used:
        # 干涸: 泥底 + 裂缝 + 枯草 + 翻倒的木桶
        parts.append(part_cylinder("mud", (0, 0, 3.0), 9.0, 1.4, MUD, vertices=18))
        for index in range(5):
            angle = 2.0 * math.pi * index / 5.0 + 0.4
            parts.append(
                part_box(
                    "crack_%d" % index,
                    (math.cos(angle) * 4.6, math.sin(angle) * 4.6, 3.9),
                    (5.4, 0.5, 0.5),
                    MUD_DARK,
                    bevel=0.1,
                    rotation_z=angle,
                )
            )
        for index, offset in enumerate((-5.0, 1.0, 6.2)):
            parts.append(
                part_box(
                    "dead_grass_%d" % index,
                    (offset, -5.0, 4.2),
                    (1.0, 1.0, 2.6),
                    MOSS_DRY,
                    bevel=0.2,
                    rotation_z=0.4 * index,
                )
            )
        parts.append(
            part_cylinder(
                "bucket",
                (-15.0, 4.0, 4.3),
                3.4,
                8.0,
                LEATHER,
                vertices=14,
                radius_top=3.9,
                rotation=Matrix.Rotation(math.radians(82), 3, "Y"),
            )
        )
        parts.append(
            part_torus(
                "bucket_hoop",
                (-15.0, 4.0, 4.3),
                3.8,
                0.4,
                IRON_RUST,
                major_segments=14,
                minor_segments=6,
                rotation=Matrix.Rotation(math.radians(82), 3, "Y"),
            )
        )
    else:
        parts.append(part_cylinder("water", (0, 0, 4.6), 9.0, 1.0, WATER, vertices=18))
        parts.append(
            part_torus(
                "ripple_1",
                (0, 0, 5.2),
                5.0,
                0.35,
                WATER_LIGHT,
                major_segments=18,
                minor_segments=6,
            )
        )
        parts.append(
            part_torus(
                "ripple_2",
                (0, 0, 5.3),
                2.6,
                0.28,
                WATER_LIGHT,
                major_segments=14,
                minor_segments=6,
            )
        )
        parts.append(
            part_cylinder(
                "bucket",
                (-15.0, 4.0, 4.0),
                3.4,
                8.0,
                LEATHER,
                vertices=14,
                radius_top=3.9,
            )
        )
        parts.append(
            part_torus(
                "bucket_hoop",
                (-15.0, 4.0, 6.4),
                3.8,
                0.4,
                IRON_DARK,
                major_segments=14,
                minor_segments=6,
            )
        )
        parts.append(
            part_torus(
                "bucket_handle",
                (-15.0, 4.0, 8.0),
                3.6,
                0.35,
                IRON,
                major_segments=14,
                minor_segments=6,
                rotation=Matrix.Rotation(math.radians(90), 3, "X"),
            )
        )
    for index, offset in enumerate((-8.0, 3.0, 9.0)):
        parts.append(
            part_box(
                "moss_%d" % index,
                (offset, 9.5, 1.0),
                (3.6, 1.6, 0.9),
                MOSS_DRY if used else MOSS,
                bevel=0.3,
            )
        )
    return join_parts(parts, "spring_dry" if used else "spring")
