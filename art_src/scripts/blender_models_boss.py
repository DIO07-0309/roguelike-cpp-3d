# A16-Boss: Boss 手建低模部件 (试点: 暗影骑士)
# 广告牌局部空间: 原点 = 骨骼锚点, x=宽度(右), y=深度(前=+Y 朝相机), z=高度(上)
# 骨骼世界 z (单位, ×ppu0.8=世界): hips=28 torso=36 head=51 arm=49 weapon=42
# 配色采样自 boss_shadow_knight_part_*.png (暗紫骑士 + 金铜饰)
import math

from blender_lib import part_box, part_cylinder, part_sphere, join_parts
from blender_models_player import export_part_json

# ── 暗影骑士调色板 (采样自 Boss 部件贴图) ──
PURPLE = (80, 64, 112)  # 主甲 暗紫
PURPLE_DK = (48, 48, 80)  # 甲暗
PURPLE_LT = (128, 112, 160)  # 甲高光
PURPLE_DEEP = (32, 16, 48)  # 深紫
DARK = (16, 0, 16)  # 近黑 (面甲/内衬)
GOLD = (150, 120, 70)  # 金铜饰
STEEL = (150, 160, 190)  # 剑刃 (冷银)
GLOW = (220, 70, 60)  # 红眼

HIPS_Z = 28.0
TORSO_Z = 36.0
HEAD_Z = 51.0
ARM_Z = 49.0
WEAPON_Z = 42.0


def _shift(obj, dx=0.0, dy=0.0, dz=0.0):
    for v in obj.data.vertices:
        v.co.x += dx
        v.co.y += dy
        v.co.z += dz
    return obj


def build_boss_part_head():
    """头: 暗紫头盔 + 深色面甲 + 红眼 + 金眉甲 + 盔脊冠饰 (原点=头骨)。"""
    parts = [
        part_cylinder("neck", (0, 0, -6), 3.4, 6, PURPLE_DEEP, vertices=12),
        # 头盔主体 (暗紫, 略高)
        part_sphere(
            "helm",
            (0, 0, 5),
            8.6,
            PURPLE,
            segments=20,
            rings=12,
            scale=(1.0, 0.96, 1.08),
        ),
        # 面甲 (深, 前面 +Y)
        part_box("visor", (0, 6.4, 1.5), (12, 3.6, 10), DARK, bevel=1.1, taper=0.9),
        # 红眼 (发光, 面甲缝内)
        part_sphere(
            "eye_l",
            (-3.2, 8.0, 2.0),
            1.6,
            GLOW,
            segments=10,
            rings=6,
            scale=(1.0, 0.5, 0.8),
        ),
        part_sphere(
            "eye_r",
            (3.2, 8.0, 2.0),
            1.6,
            GLOW,
            segments=10,
            rings=6,
            scale=(1.0, 0.5, 0.8),
        ),
        # 金眉甲 (面甲上缘)
        part_box("brow", (0, 6.8, 6.2), (13, 1.6, 2.2), GOLD, bevel=0.5),
        # 盔脊 + 冠饰 (顶部鳍)
        part_box(
            "crest",
            (0, -0.5, 11.5),
            (2.4, 10, 6.5),
            PURPLE_LT,
            bevel=0.7,
            rotation_x=math.radians(-10),
        ),
        part_box(
            "crest_gold",
            (0, -0.5, 11.5),
            (2.9, 10.4, 1.4),
            GOLD,
            bevel=0.4,
            rotation_x=math.radians(-10),
        ),
        # 两侧颊甲
        part_sphere(
            "cheek_l",
            (-7.0, 2.2, 0.5),
            3.8,
            PURPLE_DK,
            segments=12,
            rings=8,
            scale=(0.6, 1.0, 1.15),
        ),
        part_sphere(
            "cheek_r",
            (7.0, 2.2, 0.5),
            3.8,
            PURPLE_DK,
            segments=12,
            rings=8,
            scale=(0.6, 1.0, 1.15),
        ),
    ]
    return join_parts(parts, "boss_shadow_knight_part_head")


def build_boss_part_torso():
    """躯干: 暗紫胸甲(锥台) + 尖肩甲 + 金 V 甲片 + 腰带 + 下摆 (原点=躯干骨)。"""
    parts = [
        # 胸甲主体
        part_box(
            "chest", (0, 0, 4), (26, 15, 22), PURPLE, bevel=1.5, segments=2, taper=0.82
        ),
        # 胸甲亮板 (高光, 前凸)
        part_box(
            "chest_plate", (0, 6.6, 4), (16, 2.2, 15), PURPLE_LT, bevel=0.9, taper=0.85
        ),
        # 金 V 甲片 (两条斜条)
        part_box(
            "vtrim_l",
            (-3.0, 7.6, 6.0),
            (7.0, 1.4, 2.0),
            GOLD,
            bevel=0.4,
            rotation_z=math.radians(30),
        ),
        part_box(
            "vtrim_r",
            (3.0, 7.6, 6.0),
            (7.0, 1.4, 2.0),
            GOLD,
            bevel=0.4,
            rotation_z=math.radians(-30),
        ),
        # 腹部深色 + 金腰带
        part_box(
            "abdomen", (0, 0, -6), (18, 12, 8), PURPLE_DEEP, bevel=0.8, taper=0.95
        ),
        part_box("belt", (0, 0, -8.5), (21, 13, 3.0), GOLD, bevel=0.6),
        part_box("buckle", (0, 7.0, -8.5), (4.0, 1.6, 3.4), PURPLE_LT, bevel=0.5),
        # 尖肩甲 (两侧, 尖角朝外上)
        part_box(
            "pauldron_l",
            (-13.5, 0, 9),
            (8, 11, 8),
            PURPLE,
            bevel=1.2,
            taper=0.6,
            rotation_z=math.radians(18),
        ),
        part_box(
            "pauldron_r",
            (13.5, 0, 9),
            (8, 11, 8),
            PURPLE,
            bevel=1.2,
            taper=0.6,
            rotation_z=math.radians(-18),
        ),
        part_box(
            "pauldron_spike_l",
            (-15.5, 0, 14),
            (3.0, 3.0, 5.0),
            GOLD,
            bevel=0.4,
            rotation_z=math.radians(22),
        ),
        part_box(
            "pauldron_spike_r",
            (15.5, 0, 14),
            (3.0, 3.0, 5.0),
            GOLD,
            bevel=0.4,
            rotation_z=math.radians(-22),
        ),
        # 下摆
        part_box("hem", (0, 0, -12), (22, 13, 3.0), PURPLE_DK, bevel=0.7),
    ]
    return join_parts(parts, "boss_shadow_knight_part_torso")


def build_boss_part_arm():
    """臂: 肩球 + 上臂 + 肘 + 前臂 + 金护腕 + 铁手 (原点=肩骨)。"""
    parts = [
        part_sphere("shoulder", (0, 0, -2), 4.0, PURPLE, segments=14, rings=8),
        part_cylinder(
            "upper",
            (0, 0, -8),
            3.4,
            11,
            PURPLE,
            vertices=12,
            radius_top=3.8,
            smooth=True,
            bevel=0.4,
        ),
        part_sphere("elbow", (0, 0, -13.5), 3.4, PURPLE_DK, segments=12, rings=8),
        part_cylinder(
            "forearm",
            (0, 0, -17.5),
            3.0,
            9,
            PURPLE_DK,
            vertices=12,
            radius_top=3.4,
            smooth=True,
            bevel=0.4,
        ),
        part_cylinder("cuff", (0, 0, -21.0), 3.4, 2.2, GOLD, vertices=14, bevel=0.4),
        part_sphere(
            "hand",
            (0, 0, -23.5),
            3.0,
            PURPLE,
            segments=12,
            rings=8,
            scale=(1.0, 1.0, 1.1),
        ),
    ]
    # 前移: 骨骼 arm 骨靠中(x=-4/6), 陷进躯干; 前移显于胸前 (举剑姿态)
    return _shift(join_parts(parts, "boss_shadow_knight_part_arm"), dy=7.0)


def build_boss_part_leg():
    """腿: 大腿锥台 + 膝甲 + 小腿 + 靴 (原点=髋骨)。"""
    parts = [
        part_cylinder(
            "thigh",
            (0, 0, -9),
            4.6,
            13,
            PURPLE,
            vertices=12,
            radius_top=4.0,
            smooth=True,
            bevel=0.5,
        ),
        part_sphere(
            "knee",
            (0, 0.6, -16),
            3.6,
            PURPLE_LT,
            segments=12,
            rings=8,
            scale=(1.0, 1.0, 0.85),
        ),
        part_cylinder(
            "shin",
            (0, 0, -22),
            3.6,
            9,
            PURPLE_DK,
            vertices=12,
            radius_top=3.0,
            smooth=True,
            bevel=0.5,
        ),
        part_box("boot", (0, 1.8, -26.5), (9, 12, 5), PURPLE_DEEP, bevel=0.7),
        part_box("boot_toe", (0, 6.4, -27.5), (6, 3.5, 3.0), DARK, bevel=0.5),
        part_box("boot_trim", (0, 1.8, -23.8), (9, 12, 1.4), GOLD, bevel=0.3),
    ]
    return join_parts(parts, "boss_shadow_knight_part_leg")


def build_boss_part_weapon():
    """巨剑: 金柄尾 + 深柄 + 金十字护手 + 暗刃(冷银脊) (原点=握柄顶)。"""
    parts = [
        part_cylinder("grip", (0, 0, -4), 2.0, 8, PURPLE_DEEP, vertices=10, bevel=0.3),
        part_sphere("pommel", (0, 0, 0), 2.2, GOLD),
        part_box("guard", (0, 0, -8.5), (16, 3.0, 2.8), GOLD, bevel=0.6),
        part_sphere("guard_mid", (0, 0, -8.5), 2.4, PURPLE_LT),
        part_cylinder(
            "blade",
            (0, 0, -22),
            0.6,
            24,
            PURPLE_DK,
            vertices=6,
            radius_top=3.2,
            smooth=False,
            bevel=0.4,
        ),
        part_box("ridge", (0, 3.0, -22), (1.4, 1.2, 21), STEEL, bevel=0.3),
    ]
    return _shift(join_parts(parts, "boss_shadow_knight_part_weapon"), dy=10.0)


BOSS_PART_BUILDERS = {
    "head": build_boss_part_head,
    "torso": build_boss_part_torso,
    "arm": build_boss_part_arm,
    "leg": build_boss_part_leg,
    "weapon": build_boss_part_weapon,
}
