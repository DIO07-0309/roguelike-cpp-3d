# A14: 房间建模 A 组 (商店 / 铁匠铺 / 图书馆) — 未使用 + 用后残迹两态
# Blender Z-up, 1 单位 = 1 世界像素; 正面朝 -Y (导出后 = 引擎 +Z)
import math

from blender_lib import (
    add_book,
    add_bottle,
    add_candle,
    add_coin_stack,
    add_crate,
    join_parts,
    part_box,
    part_cylinder,
    part_sphere,
    part_torus,
)
from mathutils import Matrix

WOOD = (186, 124, 62)
WOOD_DARK = (132, 84, 44)
WOOD_LIGHT = (214, 158, 90)
WOOD_OLD = (142, 104, 66)
IRON = (150, 160, 172)
IRON_DARK = (98, 106, 118)
IRON_RUST = (124, 88, 60)
STONE = (112, 118, 130)
STONE_DARK = (78, 84, 96)
STONE_LIGHT = (144, 150, 162)
CLOTH_RED = (168, 62, 58)
CLOTH_CREAM = (228, 214, 182)
CLOTH_BLUE = (72, 104, 152)
GOLD = (222, 178, 66)
PARCHMENT = (232, 220, 188)
PARCHMENT_DARK = (196, 180, 146)
BOOK_COLORS = [
    (146, 62, 62),
    (62, 96, 146),
    (96, 130, 74),
    (150, 116, 62),
    (110, 70, 130),
    (72, 118, 116),
    (176, 92, 60),
    (94, 94, 104),
]
WAX = (232, 224, 196)
FLAME = (255, 176, 72)
FIRE = (255, 126, 42)
FIRE_CORE = (255, 214, 130)
ASH = (86, 82, 78)
COAL = (46, 44, 48)
WATER = (66, 140, 158)
GLASS = (168, 196, 210)


def build_shop(used=False):
    """商店: 柜台 + 雨棚 + 招牌 + 货物; used=打烊 (棚卷起/柜台空/招牌翻下)"""
    parts = []
    parts.append(
        part_box(
            "counter",
            (0, 0, 5.2),
            (26.0, 12.0, 10.4),
            WOOD_OLD if used else WOOD,
            bevel=0.9,
        )
    )
    parts.append(
        part_box("counter_top", (0, 0, 11.2), (28.0, 14.0, 1.8), WOOD_DARK, bevel=0.6)
    )
    for sign_x in (-1, 1):
        for sign_y in (-1, 1):
            parts.append(
                part_cylinder(
                    "post",
                    (sign_x * 12.4, sign_y * 5.4, 13.0),
                    0.9,
                    26.0,
                    WOOD_DARK,
                    vertices=10,
                )
            )
    if used:
        # 雨棚卷起 (横杆 + 布卷)
        parts.append(
            part_cylinder(
                "awning_roll",
                (0, 6.0, 24.6),
                2.0,
                27.0,
                CLOTH_RED,
                vertices=14,
                rotation=Matrix.Rotation(math.radians(90), 3, "Y"),
            )
        )
        parts.append(
            part_cylinder(
                "awning_roll_2",
                (0, 6.4, 24.6),
                1.5,
                26.6,
                CLOTH_CREAM,
                vertices=12,
                rotation=Matrix.Rotation(math.radians(90), 3, "Y"),
            )
        )
        # 空箱倒扣 + 招牌翻下
        parts.append(
            part_box(
                "crate_empty",
                (-9.0, 8.6, 2.4),
                (7.0, 6.0, 4.8),
                WOOD_OLD,
                bevel=0.6,
                rotation_z=math.radians(18.0),
            )
        )
        parts.append(
            part_box(
                "sign",
                (0, -7.6, 18.2),
                (9.0, 0.8, 5.0),
                WOOD_DARK,
                bevel=0.4,
                rotation_x=math.radians(78.0),
            )
        )
    else:
        # 条纹雨棚 (5 条)
        for index in range(5):
            offset = -11.2 + index * 5.6
            parts.append(
                part_box(
                    "awning_%d" % index,
                    (offset, -1.0, 25.4),
                    (5.6, 16.0, 1.2),
                    CLOTH_RED if index % 2 == 0 else CLOTH_CREAM,
                    bevel=0.3,
                    rotation_x=math.radians(-14.0),
                )
            )
        parts.append(
            part_box(
                "awning_bar",
                (0, -1.0, 24.4),
                (28.0, 0.9, 1.0),
                WOOD_DARK,
                bevel=0.2,
                rotation_x=math.radians(-14.0),
            )
        )
        # 招牌 + 吊链
        parts.append(
            part_box("sign", (0, -7.8, 18.6), (9.0, 0.8, 5.0), WOOD_LIGHT, bevel=0.4)
        )
        parts.append(
            part_box(
                "sign_trim", (0, -8.3, 18.6), (7.0, 0.4, 1.2), CLOTH_BLUE, bevel=0.2
            )
        )
        for sign_x in (-1, 1):
            parts.append(
                part_cylinder(
                    "chain", (sign_x * 3.4, -7.8, 22.0), 0.3, 4.0, IRON, vertices=6
                )
            )
        # 货物: 3 瓶 + 2 箱 + 布卷 + 钱堆
        add_bottle(parts, (-8.6, -2.2, 12.8), GLASS, (150, 66, 170))
        add_bottle(parts, (-5.4, -2.4, 12.6), GLASS, (66, 170, 120))
        add_bottle(parts, (-2.2, -2.0, 12.7), GLASS, (200, 92, 74))
        add_crate(parts, (7.2, -1.8, 13.6), (7.0, 6.0, 4.8), WOOD, WOOD_DARK)
        add_crate(parts, (8.0, 2.0, 12.9), (6.0, 5.0, 3.6), WOOD_LIGHT, WOOD_DARK)
        parts.append(
            part_cylinder(
                "cloth_roll",
                (1.6, 2.4, 12.8),
                1.6,
                7.0,
                CLOTH_BLUE,
                vertices=12,
                rotation=Matrix.Rotation(math.radians(90), 3, "Y"),
            )
        )
        add_coin_stack(parts, (3.4, -3.0, 12.2), 4, GOLD)
    return join_parts(parts, "room_shop_used" if used else "room_shop")


def build_forge(used=False):
    """铁匠铺: 石炉 + 烟囱 + 铁砧 + 风箱 + 淬火桶 + 工具架; used=炉火熄灭"""
    parts = []
    parts.append(
        part_box("furnace", (0, 2.0, 8.0), (16.0, 13.0, 16.0), STONE, bevel=0.9)
    )
    parts.append(
        part_box(
            "furnace_top", (0, 2.0, 16.6), (17.0, 14.0, 1.6), STONE_LIGHT, bevel=0.5
        )
    )
    parts.append(
        part_box("chimney", (0, 4.6, 22.0), (6.4, 5.4, 10.0), STONE_DARK, bevel=0.6)
    )
    # 炉口 + 火/灰
    parts.append(
        part_box("mouth", (0, -4.4, 7.4), (8.4, 2.2, 6.6), (34, 30, 30), bevel=0.4)
    )
    if used:
        parts.append(part_box("ash", (0, -4.6, 5.6), (7.4, 1.6, 2.4), ASH, bevel=0.5))
        for index, offset in enumerate((-2.4, 0.0, 2.2)):
            parts.append(
                part_sphere(
                    "coal_%d" % index,
                    (offset, -4.8, 6.4),
                    1.2,
                    COAL,
                    segments=8,
                    rings=4,
                )
            )
    else:
        parts.append(part_box("fire", (0, -4.6, 6.6), (7.0, 1.6, 4.4), FIRE, bevel=0.6))
        parts.append(
            part_box("fire_core", (0, -5.2, 5.8), (4.4, 1.0, 2.4), FIRE_CORE, bevel=0.4)
        )
        for index, offset in enumerate((-2.6, 0.0, 2.4)):
            parts.append(
                part_cylinder(
                    "flame_%d" % index,
                    (offset, -4.6, 9.4),
                    0.9,
                    2.6,
                    FLAME,
                    vertices=8,
                    radius_top=0.15,
                    smooth=False,
                )
            )
    # 铁砧
    parts.append(
        part_box(
            "anvil_foot", (-13.0, 1.0, 0.8), (11.0, 7.4, 1.6), STONE_DARK, bevel=0.3
        )
    )
    parts.append(
        part_box("anvil_base", (-13.0, 1.0, 3.4), (9.0, 6.4, 5.0), IRON_DARK, bevel=0.8)
    )
    parts.append(
        part_box("anvil_body", (-13.0, 1.0, 7.2), (5.6, 5.0, 2.8), IRON, bevel=0.6)
    )
    parts.append(
        part_cylinder(
            "anvil_horn",
            (-8.6, 1.0, 7.2),
            1.3,
            4.0,
            IRON,
            vertices=12,
            rotation=Matrix.Rotation(math.radians(90), 3, "Y"),
        )
    )
    # 淬火桶
    parts.append(
        part_cylinder("quench", (13.0, 4.0, 4.4), 3.6, 8.8, WOOD_OLD, vertices=14)
    )
    parts.append(
        part_cylinder(
            "quench_water",
            (13.0, 4.0, 8.0),
            3.1,
            0.9,
            (58, 66, 74) if used else WATER,
            vertices=14,
        )
    )
    for height in (2.6, 6.4):
        parts.append(
            part_torus(
                "quench_hoop",
                (13.0, 4.0, height),
                3.7,
                0.35,
                IRON_RUST,
                major_segments=14,
                minor_segments=6,
            )
        )
    # 风箱
    if used:
        parts.append(
            part_box(
                "bellows",
                (-13.0, -6.6, 3.2),
                (9.0, 5.0, 2.2),
                WOOD_OLD,
                bevel=0.6,
                rotation_x=math.radians(16.0),
            )
        )
    else:
        parts.append(
            part_box("bellows", (-13.0, -6.6, 4.2), (9.0, 5.0, 3.4), WOOD, bevel=0.6)
        )
        parts.append(
            part_box(
                "bellows_top", (-13.0, -6.6, 6.2), (8.0, 4.0, 1.2), WOOD_DARK, bevel=0.4
            )
        )
        parts.append(
            part_cylinder(
                "bellows_handle",
                (-8.6, -6.6, 6.0),
                0.6,
                5.0,
                WOOD_DARK,
                vertices=8,
                rotation=Matrix.Rotation(math.radians(90), 3, "Y"),
            )
        )
    # 工具架 (挂墙) + 工具
    parts.append(
        part_box("rack", (0, 9.4, 15.0), (14.0, 0.8, 1.2), WOOD_DARK, bevel=0.2)
    )
    tool_z = 0.7 if used else 11.0
    tool_y = 9.0 if used else 9.4
    parts.append(
        part_box(
            "tool_hammer",
            (-4.0, tool_y, tool_z),
            (1.6, 1.6, 5.0),
            WOOD_DARK,
            bevel=0.3,
            rotation_x=math.radians(90.0 if used else 0.0),
        )
    )
    parts.append(
        part_box(
            "tool_hammer_head",
            (-4.0, tool_y, tool_z + (2.4 if not used else 0.0)),
            (3.4, 1.8, 1.6),
            IRON,
            bevel=0.3,
        )
    )
    parts.append(
        part_box(
            "tool_tongs",
            (0.0, tool_y, tool_z),
            (1.2, 1.2, 5.4),
            IRON_DARK,
            bevel=0.2,
            rotation_x=math.radians(90.0 if used else 0.0),
        )
    )
    parts.append(
        part_box(
            "tool_poker",
            (3.6, tool_y, tool_z),
            (0.8, 0.8, 6.0),
            IRON_RUST,
            bevel=0.2,
            rotation_x=math.radians(90.0 if used else 0.0),
        )
    )
    # 煤堆
    for index, (offset, depth) in enumerate(((-6.0, 8.0), (-4.6, 9.2), (-7.2, 9.6))):
        parts.append(
            part_sphere(
                "coal_pile_%d" % index,
                (offset, depth, 1.2),
                1.6,
                COAL,
                segments=8,
                rings=5,
                scale=(1.0, 1.0, 0.7),
            )
        )
    return join_parts(parts, "room_forge_used" if used else "room_forge")


def _bookshelf(parts, center, books, used):
    """书架: 两侧板 + 背板 + 三层隔板 + 书"""
    cx, cy, cz = center
    width, depth, height = 18.0, 6.0, 30.0
    for sign in (-1, 1):
        parts.append(
            part_box(
                "shelf_side",
                (cx + sign * width * 0.5, cy, cz + height * 0.5),
                (1.6, depth, height),
                WOOD_DARK,
                bevel=0.4,
            )
        )
    parts.append(
        part_box(
            "shelf_back",
            (cx, cy + depth * 0.5, cz + height * 0.5),
            (width, 1.0, height),
            WOOD_OLD,
            bevel=0.3,
        )
    )
    for level in range(3):
        shelf_z = cz + 1.0 + level * 9.6
        parts.append(
            part_box(
                "shelf_board", (cx, cy, shelf_z), (width, depth, 1.2), WOOD, bevel=0.3
            )
        )
        # 书: 按层数递减 (used 时每层少 2 本)
        count = books - level - (2 if used else 0)
        for index in range(max(0, count)):
            book_x = cx - width * 0.42 + index * (width * 0.84 / max(1, books))
            color = BOOK_COLORS[(index + level * 3) % len(BOOK_COLORS)]
            add_book(
                parts,
                (book_x, cy - 1.0, shelf_z + 3.4),
                (2.2, 4.0, 6.0),
                color,
                PARCHMENT_DARK,
                rotation_z=0.0,
            )


def build_library(used=False):
    """图书馆: 书架 + 书桌 + 摊开的书 + 蜡烛 + 凳子; used=书散落/烛灭"""
    parts = []
    _bookshelf(parts, (-11.0, 6.0, 0.0), 6, used)
    _bookshelf(parts, (9.0, 6.0, 0.0), 5, used)
    # 书桌
    parts.append(
        part_box("desk_top", (0.0, -4.0, 9.4), (20.0, 11.0, 1.6), WOOD, bevel=0.6)
    )
    for sign_x in (-1, 1):
        for sign_y in (-1, 1):
            parts.append(
                part_box(
                    "desk_leg",
                    (sign_x * 8.4, -4.0 + sign_y * 4.2, 4.6),
                    (1.8, 1.8, 9.0),
                    WOOD_DARK,
                    bevel=0.3,
                )
            )
    # 摊开的书 (两页) / used: 面朝下
    if used:
        add_book(
            parts,
            (-3.0, -4.0, 10.6),
            (7.0, 9.0, 1.0),
            BOOK_COLORS[0],
            PARCHMENT,
            rotation_z=math.radians(24.0),
        )
    else:
        for sign in (-1, 1):
            parts.append(
                part_box(
                    "open_book_page",
                    (sign * 2.0, -4.0, 10.5),
                    (4.2, 8.0, 0.6),
                    PARCHMENT,
                    bevel=0.15,
                    rotation_z=math.radians(sign * 4.0),
                    rotation_x=math.radians(sign * -8.0),
                )
            )
        parts.append(
            part_box(
                "open_book_spine",
                (0.0, -4.0, 10.4),
                (0.8, 8.2, 0.9),
                BOOK_COLORS[0],
                bevel=0.2,
            )
        )
    # 蜡烛 / 墨水瓶
    add_candle(parts, (6.0, -6.0, 11.0), 3.4, WAX, None if used else FLAME)
    parts.append(
        part_cylinder("inkwell", (3.6, -6.4, 10.6), 1.1, 2.0, (48, 52, 62), vertices=10)
    )
    # 凳子
    parts.append(
        part_cylinder("stool", (0.0, -11.0, 5.0), 3.4, 1.2, WOOD_DARK, vertices=12)
    )
    for index in range(3):
        angle = 2.0 * math.pi * index / 3.0
        parts.append(
            part_cylinder(
                "stool_leg",
                (math.cos(angle) * 2.4, -11.0 + math.sin(angle) * 2.4, 2.4),
                0.6,
                4.6,
                WOOD_DARK,
                vertices=8,
            )
        )
    # used: 地上散落的书
    if used:
        scatter = (
            (-8.0, -9.0, 20.0),
            (-5.6, -10.4, -35.0),
            (2.4, -9.4, 55.0),
            (5.2, -10.6, 10.0),
            (8.0, -8.4, -60.0),
            (-2.0, -8.6, 130.0),
        )
        for index, (x, y, angle) in enumerate(scatter):
            color = BOOK_COLORS[(index + 2) % len(BOOK_COLORS)]
            add_book(
                parts,
                (x, y, 0.9),
                (5.6, 4.0, 1.0),
                color,
                PARCHMENT_DARK,
                rotation_z=math.radians(angle),
            )
    return join_parts(parts, "room_library_used" if used else "room_library")
