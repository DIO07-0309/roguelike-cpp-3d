# A14: 地面掉落物低模 (Blender, Z-up, 1 单位 = 1 世界像素, 正面朝 -Y)
# 由 icon key 映射: 药水×2 / 护符 / 重甲 / 长袍 / 五种武器 (平躺贴地, 确定性 yaw 散摆)
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
LEATHER = (108, 70, 40)
IRON = (152, 162, 174)
IRON_DARK = (104, 112, 124)
GOLD = (222, 178, 66)
STEEL = (188, 198, 212)
STEEL_DARK = (140, 150, 166)
STEEL_LIGHT = (226, 234, 244)
GLASS = (228, 236, 242)
CORK = (146, 108, 70)

_PITCH = Matrix.Rotation(math.radians(90), 3, "X")  # 柱轴 Z→Y (平躺件)


def build_potion_red():
    return _potion((214, 58, 66), "item_potion_red")


def build_potion_blue():
    return _potion((66, 116, 226), "item_potion_blue")


def _potion(liquid, name):
    """药水: 液体瓶身(下半) + 玻璃瓶身(上半) + 肩/颈/塞 + 正面标签"""
    parts = []
    parts.append(
        part_cylinder("liquid_body", (0, 0, 3.0), 4.0, 6.0, liquid, vertices=10)
    )
    parts.append(part_cylinder("glass_top", (0, 0, 7.5), 4.0, 3.0, GLASS, vertices=10))
    parts.append(
        part_cylinder(
            "shoulder", (0, 0, 9.75), 3.9, 1.5, GLASS, vertices=10, radius_top=1.7
        )
    )
    parts.append(part_cylinder("neck", (0, 0, 11.4), 1.7, 1.8, GLASS, vertices=8))
    parts.append(part_cylinder("cork", (0, 0, 13.0), 1.9, 1.4, CORK, vertices=8))
    parts.append(
        part_box("label", (0, -3.85, 3.2), (4.8, 0.5, 3.6), (238, 234, 220), bevel=0.2)
    )
    return join_parts(parts, name)


def build_charm():
    """护符: 三节链环 + 金托圆盘 + 紫宝石 (平躺)"""
    parts = []
    for x in (-4.6, 0.0, 4.6):
        parts.append(
            part_torus(
                "link",
                (x, 6.8, 0.9),
                2.0,
                0.55,
                GOLD,
                major_segments=10,
                minor_segments=6,
            )
        )
    parts.append(part_cylinder("pendant", (0, -1.5, 1.1), 5.2, 2.2, GOLD, vertices=12))
    parts.append(
        part_sphere(
            "gem",
            (0, -1.5, 3.0),
            2.8,
            (168, 76, 222),
            segments=8,
            rings=6,
            scale=(1.0, 1.0, 0.62),
        )
    )
    parts.append(
        part_sphere(
            "gem_hi", (-1.0, -2.5, 3.9), 1.0, (226, 176, 255), segments=6, rings=4
        )
    )
    return join_parts(parts, "item_charm")


def build_armor():
    """重甲: 上窄胸甲主体 + 腰带金扣 + 左右肩甲 + 胸脊 + 领口 (立放)"""
    parts = []
    parts.append(
        part_box("chest", (0, 0, 8.5), (13.0, 7.0, 13.0), IRON, bevel=1.0, taper=0.78)
    )
    parts.append(part_box("belt", (0, 0, 1.4), (11.4, 7.6, 2.8), LEATHER, bevel=0.4))
    parts.append(part_box("buckle", (0, -3.9, 1.4), (2.6, 0.8, 2.0), GOLD, bevel=0.3))
    for side in (-1, 1):
        parts.append(
            part_sphere(
                "pauldron",
                (side * 7.2, 0, 13.4),
                3.8,
                STEEL,
                segments=10,
                rings=6,
                scale=(1.0, 0.85, 0.70),
            )
        )
    parts.append(
        part_box(
            "ridge", (0, -3.4, 9.0), (2.2, 1.2, 9.0), STEEL_LIGHT, bevel=0.4, taper=0.55
        )
    )
    parts.append(
        part_box("collar", (0, 0, 15.4), (7.0, 6.0, 1.6), IRON_DARK, bevel=0.4)
    )
    return join_parts(parts, "item_armor")


def build_robe():
    """布袍: 下摆宽上窄袍身 + 束带 + 前襟饰条 + 兜帽 (冰霜/暗影甲)"""
    parts = []
    parts.append(
        part_box(
            "gown", (0, 0, 7.6), (12.5, 8.0, 15.2), (58, 72, 132), bevel=0.9, taper=0.50
        )
    )
    parts.append(
        part_box("sash", (0, 0, 5.6), (13.0, 8.4, 2.2), (140, 160, 210), bevel=0.4)
    )
    parts.append(
        part_box(
            "front_trim", (0, -3.3, 8.4), (2.4, 1.0, 12.0), (196, 208, 226), bevel=0.3
        )
    )
    parts.append(
        part_sphere(
            "hood",
            (0, 0, 15.8),
            4.4,
            (44, 54, 104),
            segments=10,
            rings=6,
            scale=(1.0, 0.85, 0.75),
        )
    )
    return join_parts(parts, "item_robe")


def build_dagger():
    """匕首 (平躺): 短刃 + 刃尖 + 宽护手 + 缠柄 + 柄头"""
    parts = []
    parts.append(part_box("blade", (0, -2.5, 0.6), (3.0, 9.0, 1.2), STEEL, bevel=0.25))
    parts.append(
        part_prism(
            "tip",
            (0, -7.0, 0.6),
            [(-1.5, 0.5), (1.5, 0.5), (0, -3.4)],
            1.2,
            STEEL_LIGHT,
        )
    )
    parts.append(
        part_box("guard", (0, 2.6, 0.8), (7.5, 1.5, 1.5), IRON_DARK, bevel=0.4)
    )
    parts.append(
        part_cylinder(
            "grip", (0, 5.0, 1.0), 1.0, 3.4, WOOD_DARK, vertices=8, rotation=_PITCH
        )
    )
    parts.append(
        part_sphere("pommel", (0, 7.2, 1.2), 1.2, IRON_DARK, segments=8, rings=6)
    )
    return join_parts(parts, "item_dagger")


def build_sword():
    """长剑 (平躺): 双刃直身 + 血槽 + 金护手 + 皮柄 + 金柄头"""
    parts = []
    parts.append(part_box("blade", (0, -4.0, 0.7), (3.4, 16.0, 1.4), STEEL, bevel=0.3))
    parts.append(
        part_prism(
            "tip",
            (0, -12.0, 0.7),
            [(-1.7, 0.6), (1.7, 0.6), (0, -4.2)],
            1.4,
            STEEL_LIGHT,
        )
    )
    parts.append(
        part_box("fuller", (0, -4.0, 1.2), (0.9, 14.0, 0.5), STEEL_DARK, bevel=0.15)
    )
    parts.append(part_box("guard", (0, 4.6, 0.9), (9.5, 1.8, 1.8), GOLD, bevel=0.5))
    parts.append(
        part_cylinder(
            "grip", (0, 7.6, 1.15), 1.15, 5.0, LEATHER, vertices=8, rotation=_PITCH
        )
    )
    parts.append(part_sphere("pommel", (0, 10.6, 1.5), 1.5, GOLD, segments=8, rings=6))
    return join_parts(parts, "item_sword")


def build_spear():
    """长枪 (平躺): 木杆 + 套筒 + 菱形枪头 + 护环 + 尾鐏"""
    parts = []
    parts.append(
        part_cylinder(
            "shaft", (0, 1.0, 1.1), 1.1, 24.0, WOOD_DARK, vertices=8, rotation=_PITCH
        )
    )
    parts.append(
        part_cylinder(
            "socket", (0, -11.8, 1.6), 1.5, 2.6, IRON_DARK, vertices=8, rotation=_PITCH
        )
    )
    parts.append(
        part_prism(
            "blade",
            (0, -16.2, 1.6),
            [(0, -3.8), (2.2, 0.2), (0, 3.6), (-2.2, 0.2)],
            1.1,
            STEEL_LIGHT,
        )
    )
    parts.append(part_box("collar", (0, -13.2, 1.6), (5.0, 1.2, 1.4), IRON, bevel=0.3))
    parts.append(
        part_cylinder(
            "butt", (0, 13.4, 1.5), 1.4, 2.2, IRON_DARK, vertices=8, rotation=_PITCH
        )
    )
    return join_parts(parts, "item_spear")


def build_nunchaku():
    """双截棍 (平躺): 两根八字短棍 + 三节链环"""
    parts = []
    for side, ang in ((-1, 20), (1, -20)):
        rot = Matrix.Rotation(math.radians(ang), 3, "Z") @ _PITCH
        parts.append(
            part_cylinder(
                "stick", (side * 5.0, 0, 1.3), 1.3, 11.0, WOOD, vertices=8, rotation=rot
            )
        )
    for index, y in enumerate((-1.9, 0.0, 1.9)):
        if index % 2 == 0:
            parts.append(
                part_torus(
                    "chain",
                    (0, y, 1.7),
                    1.3,
                    0.4,
                    IRON_DARK,
                    major_segments=8,
                    minor_segments=5,
                    rotation=_PITCH,
                )
            )
        else:
            parts.append(
                part_torus(
                    "chain",
                    (0, y, 0.9),
                    1.3,
                    0.4,
                    IRON_DARK,
                    major_segments=8,
                    minor_segments=5,
                )
            )
    return join_parts(parts, "item_nunchaku")


def build_crossbow():
    """弩 (平躺): 枪托 + 握把 + 左右弓臂 + 弩弦 + 上弦弩箭 + 扳机"""
    parts = []
    parts.append(
        part_box(
            "stock", (0, 1.5, 1.6), (4.0, 17.0, 3.0), WOOD_DARK, bevel=0.6, taper=0.85
        )
    )
    parts.append(part_box("grip", (0, 6.5, 1.7), (3.0, 3.2, 3.4), WOOD, bevel=0.5))
    for side in (-1, 1):
        parts.append(
            part_box(
                "limb",
                (side * 7.5, -5.5, 1.9),
                (12.0, 2.6, 1.6),
                IRON_DARK,
                bevel=0.4,
                rotation_z=math.radians(side * -22),
            )
        )
    for side in (-1, 1):
        parts.append(
            part_box(
                "string",
                (side * 6.2, 0.5, 2.4),
                (0.5, 11.0, 0.5),
                (60, 58, 54),
                rotation_z=math.radians(side * -28),
            )
        )
    parts.append(
        part_cylinder(
            "bolt",
            (0, -3.0, 3.4),
            0.6,
            10.0,
            (170, 140, 100),
            vertices=6,
            rotation=_PITCH,
        )
    )
    parts.append(
        part_prism(
            "bolt_tip",
            (0, -8.6, 3.4),
            [(-0.9, 0.4), (0.9, 0.4), (0, -2.2)],
            1.0,
            STEEL_LIGHT,
        )
    )
    parts.append(part_box("trigger", (0, 3.8, 0.7), (1.6, 2.2, 1.4), IRON, bevel=0.3))
    return join_parts(parts, "item_crossbow")
