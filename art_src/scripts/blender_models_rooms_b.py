# A14: 房间建模 B 组 (赌桌 / 神龛 / 密室) — 未使用 + 用后残迹两态
# Blender Z-up, 1 单位 = 1 世界像素; 正面朝 -Y (导出后 = 引擎 +Z)
import math

from blender_lib import (
    add_book,
    add_candle,
    add_coin_stack,
    join_parts,
    part_box,
    part_cylinder,
    part_prism,
    part_sphere,
    part_torus,
)
from mathutils import Matrix

WOOD = (168, 110, 58)
WOOD_DARK = (118, 74, 40)
WOOD_LIGHT = (204, 148, 86)
IRON = (150, 160, 172)
IRON_DARK = (98, 106, 118)
GOLD = (222, 178, 66)
GOLD_LIGHT = (244, 212, 120)
STONE = (114, 120, 132)
STONE_DARK = (80, 86, 98)
STONE_LIGHT = (148, 154, 166)
STONE_BLUE = (58, 66, 92)
CLOTH_RED = (162, 58, 54)
CLOTH_PURPLE = (96, 58, 118)
WAX = (232, 224, 196)
FLAME = (255, 176, 72)
CARD_WHITE = (236, 232, 220)
CARD_RED = (176, 62, 58)
CARD_BACK = (86, 108, 148)
PARCHMENT = (232, 220, 188)
MOSS = (86, 132, 74)
ASH = (92, 88, 84)


def build_gambler(used=False):
    """赌桌: 圆桌 + 凳 + 纸牌 + 骰子 + 吊灯 + 钱堆; used=掀桌散场"""
    parts = []
    parts.append(
        part_cylinder("table_base", (0, 0, 0.8), 6.4, 1.6, WOOD_DARK, vertices=16)
    )
    parts.append(
        part_cylinder("table_pedestal", (0, 0, 5.0), 2.4, 8.0, WOOD, vertices=14)
    )
    parts.append(
        part_cylinder("table_top", (0, 0, 9.6), 9.6, 1.8, WOOD_LIGHT, vertices=20)
    )
    parts.append(
        part_torus(
            "table_rim",
            (0, 0, 10.4),
            9.4,
            0.5,
            WOOD_DARK,
            major_segments=20,
            minor_segments=6,
        )
    )
    # 凳子 (used: 一张翻倒)
    parts.append(
        part_cylinder("stool_a", (-11.0, 2.0, 5.0), 3.2, 1.2, WOOD_DARK, vertices=12)
    )
    for index in range(3):
        angle = 2.0 * math.pi * index / 3.0
        parts.append(
            part_cylinder(
                "stool_a_leg",
                (-11.0 + math.cos(angle) * 2.2, 2.0 + math.sin(angle) * 2.2, 2.4),
                0.6,
                4.6,
                WOOD_DARK,
                vertices=8,
            )
        )
    if used:
        parts.append(
            part_cylinder(
                "stool_b",
                (10.0, -3.0, 4.2),
                3.2,
                1.2,
                WOOD_DARK,
                vertices=12,
                rotation=Matrix.Rotation(math.radians(86), 3, "X"),
            )
        )
        for index, (x, y, angle) in enumerate(
            (
                (-6.0, -8.0, 25.0),
                (-3.4, -9.4, -40.0),
                (1.2, -8.6, 70.0),
                (4.6, -9.8, -15.0),
                (7.2, -7.6, 110.0),
            )
        ):
            parts.append(
                part_box(
                    "card_floor_%d" % index,
                    (x, y, 0.4),
                    (3.6, 2.6, 0.3),
                    CARD_BACK if index % 2 else CARD_WHITE,
                    bevel=0.08,
                    rotation_z=math.radians(angle),
                )
            )
        parts.append(
            part_box(
                "lantern_dead", (0, -1.0, 13.2), (3.2, 3.2, 3.6), IRON_DARK, bevel=0.4
            )
        )
    else:
        parts.append(
            part_cylinder(
                "stool_b", (10.4, -2.0, 5.0), 3.2, 1.2, WOOD_DARK, vertices=12
            )
        )
        for index in range(3):
            angle = 2.0 * math.pi * index / 3.0
            parts.append(
                part_cylinder(
                    "stool_b_leg",
                    (10.4 + math.cos(angle) * 2.2, -2.0 + math.sin(angle) * 2.2, 2.4),
                    0.6,
                    4.6,
                    WOOD_DARK,
                    vertices=8,
                )
            )
        # 扇形纸牌
        for index in range(5):
            angle = -26.0 + index * 13.0
            parts.append(
                part_box(
                    "card_%d" % index,
                    (index * 1.4 - 2.8, -2.6, 10.7),
                    (3.4, 2.4, 0.3),
                    CARD_WHITE if index % 2 == 0 else CARD_RED,
                    bevel=0.08,
                    rotation_z=math.radians(angle),
                )
            )
        for index, offset in enumerate(((-1.4, 3.2), (1.6, 3.6))):
            parts.append(
                part_box(
                    "die_%d" % index,
                    (offset[0], offset[1], 10.9),
                    (1.6, 1.6, 1.6),
                    CARD_WHITE,
                    bevel=0.25,
                    rotation_z=math.radians(18.0 * index),
                )
            )
        add_coin_stack(parts, (6.2, 2.6, 10.6), 5, GOLD)
        add_coin_stack(parts, (-6.4, 3.0, 10.6), 3, GOLD_LIGHT)
        # 吊灯 (灯笼 + 发光核心)
        parts.append(
            part_box("lantern", (0, -1.0, 15.4), (3.6, 3.6, 4.0), IRON_DARK, bevel=0.4)
        )
        parts.append(
            part_box("lantern_glow", (0, -1.0, 15.4), (2.4, 2.4, 2.6), FLAME, bevel=0.3)
        )
        parts.append(
            part_cylinder("lantern_chain", (0, -1.0, 20.0), 0.3, 5.0, IRON, vertices=6)
        )
    return join_parts(parts, "room_gambler_used" if used else "room_gambler")


def _statue(parts, center, color, dark):
    """神像: 袍身 + 头 + 兜帽 + 手"""
    cx, cy, cz = center
    parts.append(
        part_cylinder(
            "statue_body",
            (cx, cy, cz + 7.0),
            2.6,
            12.0,
            color,
            vertices=12,
            radius_top=1.8,
        )
    )
    parts.append(
        part_cylinder("statue_base", (cx, cy, cz + 1.0), 3.4, 2.0, dark, vertices=12)
    )
    parts.append(
        part_sphere(
            "statue_head", (cx, cy - 0.2, cz + 14.4), 2.2, color, segments=12, rings=7
        )
    )
    parts.append(
        part_sphere(
            "statue_hood",
            (cx, cy + 0.4, cz + 15.0),
            2.5,
            dark,
            segments=12,
            rings=7,
            scale=(1.0, 1.0, 0.8),
        )
    )
    for sign in (-1, 1):
        parts.append(
            part_cylinder(
                "statue_arm",
                (cx + sign * 2.2, cy - 1.0, cz + 9.4),
                0.8,
                5.0,
                color,
                vertices=8,
                rotation=Matrix.Rotation(math.radians(sign * 18), 3, "Y"),
            )
        )


def build_shrine(used=False):
    """神龛: 石阶 + 壁龛 + 神像 + 三烛 + 供碗; used=香火熄灭"""
    parts = []
    parts.append(part_box("base", (0, 0, 1.6), (18.0, 14.0, 3.2), STONE, bevel=0.8))
    parts.append(
        part_box("base_step", (0, 0, 3.6), (15.0, 11.6, 1.8), STONE_LIGHT, bevel=0.5)
    )
    parts.append(
        part_box("body", (0, 2.4, 12.0), (13.0, 8.0, 15.0), STONE_BLUE, bevel=0.8)
    )
    parts.append(
        part_box("niche", (0, -1.8, 11.6), (8.4, 1.6, 10.6), (36, 40, 56), bevel=0.4)
    )
    _statue(
        parts,
        (0.0, -2.6, 5.0),
        (140, 128, 108) if used else (176, 162, 132),
        (72, 70, 76) if used else (108, 96, 74),
    )
    # 屋顶 (三角棱 + 檐)
    parts.append(
        part_prism(
            "roof",
            (0, 2.4, 21.4),
            [(-9.4, -1.4), (9.4, -1.4), (0, 5.0)],
            11.0,
            STONE_DARK,
            bevel=0.4,
        )
    )
    parts.append(
        part_box("eave", (0, 2.4, 19.6), (19.6, 12.0, 1.2), STONE_LIGHT, bevel=0.3)
    )
    # 三烛 + 供碗
    for index, offset in enumerate((-4.4, 0.0, 4.4)):
        height = 1.0 if used else 3.6
        add_candle(
            parts,
            (offset, -5.2, 4.6 + height * 0.5),
            height,
            WAX,
            None if used else FLAME,
        )
    parts.append(
        part_cylinder(
            "bowl", (0, -5.6, 5.4), 2.4, 1.4, GOLD, vertices=14, radius_top=2.8
        )
    )
    if used:
        parts.append(
            part_cylinder("bowl_ash", (0, -5.6, 6.0), 2.2, 0.5, ASH, vertices=12)
        )
    else:
        add_coin_stack(parts, (0.0, -5.6, 6.0), 3, GOLD_LIGHT)
    # 苔藓
    for index, offset in enumerate((-6.6, 5.8)):
        parts.append(
            part_box(
                "moss_%d" % index, (offset, -6.2, 3.2), (3.2, 1.2, 0.8), MOSS, bevel=0.3
            )
        )
    return join_parts(parts, "room_shrine_used" if used else "room_shrine")


def _small_chest(parts, center, opened):
    """小箱 (密室用): 箱体 + 盖 + 铁箍 + 锁"""
    cx, cy, cz = center
    parts.append(
        part_box("chest_body", (cx, cy, cz + 3.4), (13.0, 9.0, 6.8), WOOD, bevel=0.7)
    )
    hinge = (cx, cy + 4.4, cz + 6.8)
    angle = math.radians(-102.0) if opened else 0.0
    for name, dz, size, color in (
        ("chest_lid", 3.4, (12.6, 8.6, 2.0), WOOD_DARK),
        ("chest_lid_top", 4.8, (10.4, 7.0, 1.4), WOOD_LIGHT),
    ):
        local = (cx, cy, cz + 6.8 + dz)
        from mathutils import Vector

        offset = Vector(local) - Vector(hinge)
        rotated = Matrix.Rotation(angle, 3, "X") @ offset
        center_pos = tuple(Vector(hinge) + rotated)
        parts.append(
            part_box(name, center_pos, size, color, bevel=0.5, rotation_x=angle)
        )
    for sign in (-1, 1):
        parts.append(
            part_box(
                "chest_band",
                (cx + sign * 4.4, cy - 4.7, cz + 3.4),
                (1.6, 0.8, 7.2),
                IRON,
                bevel=0.25,
            )
        )
    if opened:
        parts.append(
            part_box(
                "chest_inner",
                (cx, cy, cz + 5.4),
                (11.0, 7.2, 1.8),
                (46, 34, 24),
                bevel=0.3,
            )
        )
    else:
        parts.append(
            part_box(
                "chest_lock", (cx, cy - 5.0, cz + 4.4), (3.0, 1.0, 3.0), GOLD, bevel=0.4
            )
        )
    return parts


def build_secret(used=False):
    """密室: 石壁龛 + 小箱 + 侧烛 + 挂旗 + 金币; used=洗劫一空"""
    parts = []
    # 壁龛 (左右墙 + 背墙 + 门楣)
    for sign in (-1, 1):
        parts.append(
            part_box(
                "alcove_side",
                (sign * 9.4, 2.0, 9.0),
                (2.0, 14.0, 18.0),
                STONE,
                bevel=0.6,
            )
        )
    parts.append(
        part_box("alcove_back", (0, 8.6, 9.0), (21.0, 2.0, 18.0), STONE_DARK, bevel=0.6)
    )
    parts.append(
        part_box(
            "alcove_lintel", (0, 2.0, 18.6), (21.0, 14.0, 2.4), STONE_LIGHT, bevel=0.5
        )
    )
    parts.append(
        part_box(
            "alcove_floor", (0, 2.0, 0.6), (19.0, 13.0, 1.2), STONE_DARK, bevel=0.3
        )
    )
    _small_chest(parts, (0.0, 2.6, 1.2), used)
    # 侧烛 (used: 只剩烛泪)
    for sign in (-1, 1):
        height = 1.0 if used else 3.2
        add_candle(
            parts,
            (sign * 7.4, 0.0, 1.2 + height * 0.5),
            height,
            WAX,
            None if used else FLAME,
        )
    # 挂旗 (used: 落地揉皱)
    if used:
        parts.append(
            part_box(
                "banner",
                (3.0, -2.0, 0.4),
                (7.0, 5.0, 0.5),
                CLOTH_PURPLE,
                bevel=0.3,
                rotation_z=math.radians(24.0),
            )
        )
    else:
        parts.append(
            part_box(
                "banner", (0, 7.4, 12.0), (12.0, 0.5, 9.0), CLOTH_PURPLE, bevel=0.3
            )
        )
        parts.append(
            part_box("banner_trim", (0, 7.1, 7.8), (12.0, 0.6, 1.4), GOLD, bevel=0.2)
        )
    # 金币 (used: 散落地面)
    if used:
        for index, (x, y) in enumerate(
            ((-5.6, -3.0), (-3.4, -4.2), (5.0, -3.6), (7.0, -1.8))
        ):
            add_coin_stack(parts, (x, y, 0.6), 2, GOLD)
        parts.append(
            part_box(
                "torn_cloth",
                (-7.0, -4.4, 0.4),
                (4.0, 3.0, 0.4),
                CLOTH_RED,
                bevel=0.1,
                rotation_z=math.radians(38.0),
            )
        )
    else:
        add_coin_stack(parts, (-6.0, -1.0, 1.6), 4, GOLD)
        add_coin_stack(parts, (6.2, -0.4, 1.6), 3, GOLD_LIGHT)
    return join_parts(parts, "room_secret_used" if used else "room_secret")
