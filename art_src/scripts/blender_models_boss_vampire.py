# A16-Boss: 吸血鬼手建低模 (第五个 Boss)
# 局部空间: 原点 = 骨骼锚点, x=宽, y=深度(前=+Y), z=高
# 骨架 ppu 1.15; 细剑骨 x=+10 对齐前手, 剑位于身体右侧
# 设计: 吸血鬼贵族 — 苍白面孔、猩红高领大衣、披风、金色饰扣、细剑
import math

from blender_lib import part_box, part_cylinder, part_sphere, join_parts
from blender_models_player import export_part_json

# ── 吸血鬼调色板 (采样自 boss_vampire_part_*.png) ──
CRIMSON = (112, 32, 48)  # 主大衣血红 (压暗)
CRIMSON_LT = (160, 56, 72)  # 衣领/高光
BLOOD = (72, 12, 24)  # 暗红/披风内衬
DARK = (36, 12, 14)  # 深红黑
HAIR = (20, 10, 16)  # 黑发
PALE = (226, 206, 172)  # 苍白皮肤
PALE_DK = (190, 160, 128)  # 皮肤暗部
GOLD = (180, 140, 80)  # 金饰
STEEL = (190, 200, 210)  # 剑刃
GLOW = (255, 80, 90)  # 红眼/血光

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


def _slanted_box(name, center, size, color, slant_deg, bevel=0.0, taper=1.0):
    """绕 Y 轴倾斜的长方体 — 用于翻领/眉眼/衣摆。"""
    obj = part_box(name, (0, 0, 0), size, color, bevel=bevel, taper=taper)
    angle = math.radians(slant_deg)
    cos_a, sin_a = math.cos(angle), math.sin(angle)
    for v in obj.data.vertices:
        x, z = v.co.x, v.co.z
        v.co.x = x * cos_a + z * sin_a + center[0]
        v.co.z = -x * sin_a + z * cos_a + center[2]
        v.co.y += center[1]
    return obj


def build_vampire_part_head():
    """头: 苍白贵族面孔 + 黑发/美人尖 + 红眼 + 尖耳 + 獠牙 + 山羊胡。"""
    parts = [
        part_cylinder("neck", (0, 0, -6), 3.2, 8, PALE_DK, vertices=12),
        part_sphere(
            "skull",
            (0, 0, 3.0),
            6.0,
            PALE,
            segments=18,
            rings=11,
            scale=(1.0, 0.96, 1.12),
        ),
        part_box("jaw", (0, 5.8, -3.0), (7.0, 3.2, 4.0), PALE, bevel=0.6, taper=0.78),
        part_box("chin", (0, 6.0, -5.4), (4.4, 2.4, 2.0), PALE, bevel=0.4, taper=0.7),
        # 黑发 (后脑 + 美人尖 + 侧发)
        part_sphere(
            "hair",
            (0, -1.4, 5.6),
            6.2,
            HAIR,
            segments=16,
            rings=10,
            scale=(1.04, 1.0, 0.72),
        ),
        _slanted_box(
            "widow_peak",
            (0, 6.2, 6.2),
            (3.2, 1.2, 3.2),
            HAIR,
            0.0,
            bevel=0.2,
            taper=0.2,
        ),
        part_box(
            "side_hair_l", (-5.8, 2.0, 2.0), (2.4, 4.0, 7.0), HAIR, bevel=0.3, taper=0.6
        ),
        part_box(
            "side_hair_r", (5.8, 2.0, 2.0), (2.4, 4.0, 7.0), HAIR, bevel=0.3, taper=0.6
        ),
        # 眉/眼/鼻
        _slanted_box(
            "brow_l", (-2.4, 7.4, 4.2), (3.4, 0.9, 0.8), HAIR, -12.0, bevel=0.1
        ),
        _slanted_box("brow_r", (2.4, 7.4, 4.2), (3.4, 0.9, 0.8), HAIR, 12.0, bevel=0.1),
        _slanted_box(
            "eye_l", (-2.4, 8.0, 2.4), (3.2, 0.7, 0.8), GLOW, 10.0, bevel=0.12
        ),
        _slanted_box(
            "eye_r", (2.4, 8.0, 2.4), (3.2, 0.7, 0.8), GLOW, -10.0, bevel=0.12
        ),
        part_box("nose", (0, 7.8, 0.4), (1.4, 1.2, 3.0), PALE_DK, bevel=0.2, taper=0.7),
        # 薄唇 + 獠牙
        part_box("mouth", (0, 7.9, -2.6), (4.0, 0.7, 0.5), DARK, bevel=0.1),
        part_cylinder(
            "fang_l",
            (-1.2, 8.2, -3.1),
            0.3,
            1.5,
            PALE,
            vertices=6,
            radius_top=0.06,
            smooth=False,
        ),
        part_cylinder(
            "fang_r",
            (1.2, 8.2, -3.1),
            0.3,
            1.5,
            PALE,
            vertices=6,
            radius_top=0.06,
            smooth=False,
        ),
        part_box(
            "goatee", (0, 7.0, -5.9), (2.4, 1.6, 3.2), HAIR, bevel=0.2, taper=0.15
        ),
        # 尖耳
        part_box(
            "ear_l",
            (-6.4, 0.8, 2.0),
            (1.8, 2.0, 4.0),
            PALE,
            bevel=0.2,
            taper=0.15,
            rotation_z=math.radians(28),
        ),
        part_box(
            "ear_r",
            (6.4, 0.8, 2.0),
            (1.8, 2.0, 4.0),
            PALE,
            bevel=0.2,
            taper=0.15,
            rotation_z=math.radians(-28),
        ),
        part_sphere("earring", (-7.2, 0.8, 0.4), 0.55, GOLD, segments=8, rings=4),
        # 苍白面颊/颧骨
        part_box(
            "cheek_l", (-3.8, 6.2, 0.4), (2.6, 1.4, 3.2), PALE_DK, bevel=0.3, taper=0.8
        ),
        part_box(
            "cheek_r", (3.8, 6.2, 0.4), (2.6, 1.4, 3.2), PALE_DK, bevel=0.3, taper=0.8
        ),
    ]
    # 头部前探: 下颚/獠牙越过领巾, 形成贵族式俯视
    return _shift(join_parts(parts, "boss_vampire_part_head"), dy=2.0)


def build_vampire_part_torso():
    """躯干: 猩红高领大衣 + 苍白领巾 + 金扣 + 披风/燕尾 + 窄腰。"""
    parts = [
        part_box(
            "chest", (0, 0, 6), (26, 16, 18), CRIMSON, bevel=1.2, segments=2, taper=0.55
        ),
        # 高领 (外红内暗)
        part_box(
            "collar_l",
            (-7.6, -2.0, 15.0),
            (5.5, 4.0, 12.0),
            CRIMSON_LT,
            bevel=0.4,
            taper=0.5,
            rotation_z=math.radians(8),
        ),
        part_box(
            "collar_r",
            (7.6, -2.0, 15.0),
            (5.5, 4.0, 12.0),
            CRIMSON_LT,
            bevel=0.4,
            taper=0.5,
            rotation_z=math.radians(-8),
        ),
        part_box(
            "collar_in_l",
            (-7.0, 0.4, 14.0),
            (4.0, 1.4, 9.0),
            BLOOD,
            bevel=0.2,
            taper=0.6,
            rotation_z=math.radians(8),
        ),
        part_box(
            "collar_in_r",
            (7.0, 0.4, 14.0),
            (4.0, 1.4, 9.0),
            BLOOD,
            bevel=0.2,
            taper=0.6,
            rotation_z=math.radians(-8),
        ),
        # 肩披层
        part_box(
            "shoulder_l",
            (-14.0, 0, 10.0),
            (10.5, 14.0, 7.5),
            CRIMSON,
            bevel=0.8,
            taper=0.45,
            rotation_z=math.radians(12),
        ),
        part_box(
            "shoulder_r",
            (14.0, 0, 10.0),
            (10.5, 14.0, 7.5),
            CRIMSON,
            bevel=0.8,
            taper=0.45,
            rotation_z=math.radians(-12),
        ),
        part_box(
            "shoulder_under_l",
            (-14.0, 0, 7.0),
            (9.0, 12.0, 4.5),
            BLOOD,
            bevel=0.6,
            taper=0.5,
            rotation_z=math.radians(12),
        ),
        part_box(
            "shoulder_under_r",
            (14.0, 0, 7.0),
            (9.0, 12.0, 4.5),
            BLOOD,
            bevel=0.6,
            taper=0.5,
            rotation_z=math.radians(-12),
        ),
        # 翻领 + 领巾
        _slanted_box(
            "lapel_l",
            (-4.2, 8.6, 8.0),
            (7.0, 2.0, 12.0),
            CRIMSON_LT,
            -18.0,
            bevel=0.3,
            taper=0.82,
        ),
        _slanted_box(
            "lapel_r",
            (4.2, 8.6, 8.0),
            (7.0, 2.0, 12.0),
            CRIMSON_LT,
            18.0,
            bevel=0.3,
            taper=0.82,
        ),
        part_box("cravat", (0, 9.5, 7.0), (4.2, 2.0, 6.0), PALE, bevel=0.4, taper=0.75),
        part_sphere("cravat_pin", (0, 10.6, 9.2), 0.8, GOLD, segments=8, rings=4),
        part_box("cravat_gem", (0, 10.6, 8.2), (1.2, 0.6, 1.4), GLOW, bevel=0.15),
        # 金扣
        part_sphere("button_a", (3.4, 9.6, 6.0), 0.65, GOLD, segments=8, rings=4),
        part_sphere("button_b", (3.4, 9.6, 2.5), 0.65, GOLD, segments=8, rings=4),
        part_sphere("button_c", (3.4, 9.6, -1.0), 0.65, GOLD, segments=8, rings=4),
        # 窄腰 + 腰带
        part_box("waist", (0, 0, -7.0), (15.0, 12.0, 9.0), DARK, bevel=0.6, taper=0.95),
        part_box("belt", (0, 0, -8.5), (17.0, 12.5, 3.4), HAIR, bevel=0.4),
        part_box("buckle", (0, 7.0, -8.5), (3.6, 1.2, 3.0), GOLD, bevel=0.3),
        # 燕尾 (前/侧/后)
        part_box(
            "tail_l",
            (-5.4, 7.0, -14.0),
            (5.8, 2.2, 15.0),
            CRIMSON_LT,
            bevel=0.4,
            taper=0.85,
        ),
        part_box(
            "tail_r",
            (5.4, 7.0, -14.0),
            (5.8, 2.2, 15.0),
            CRIMSON_LT,
            bevel=0.4,
            taper=0.85,
        ),
        part_box(
            "tail_side_l",
            (-9.8, -3.0, -10.0),
            (5.0, 3.0, 20.0),
            CRIMSON,
            bevel=0.4,
            taper=0.7,
        ),
        part_box(
            "tail_side_r",
            (9.8, -3.0, -10.0),
            (5.0, 3.0, 20.0),
            CRIMSON,
            bevel=0.4,
            taper=0.7,
        ),
        # 背后披风
        part_box(
            "cape", (0, -8.0, -3.0), (22.0, 2.2, 26.0), BLOOD, bevel=0.5, taper=0.8
        ),
        part_box(
            "cape_trim", (0, -9.0, -16.0), (16.0, 1.0, 2.0), DARK, bevel=0.2, taper=0.85
        ),
        # 金色前襟饰条
        part_box("front_trim", (0, 9.8, 1.0), (1.2, 0.8, 12.0), GOLD, bevel=0.15),
        # 血色胸针
        part_sphere(
            "brooch",
            (-5.0, 9.6, 5.0),
            1.2,
            GLOW,
            segments=10,
            rings=6,
            scale=(1.0, 0.5, 1.0),
        ),
    ]
    return join_parts(parts, "boss_vampire_part_torso")


def build_vampire_part_arm():
    """臂: 猩红长袖 + 金袖口 + 苍白骨手/利爪。"""
    parts = [
        part_sphere("shoulder", (0, 0, -2), 3.6, CRIMSON, segments=14, rings=8),
        part_cylinder(
            "upper",
            (0, 0, -9),
            3.2,
            13,
            CRIMSON,
            vertices=12,
            radius_top=3.6,
            smooth=True,
            bevel=0.3,
        ),
        part_sphere("elbow", (0, 0, -16), 3.0, BLOOD, segments=12, rings=8),
        part_cylinder(
            "forearm",
            (0, 0, -21),
            2.8,
            10,
            CRIMSON_LT,
            vertices=12,
            radius_top=3.2,
            smooth=True,
            bevel=0.3,
        ),
        part_cylinder("cuff", (0, 0, -25.5), 3.2, 2.0, GOLD, vertices=12, bevel=0.3),
        part_sphere(
            "hand",
            (0, 0, -28.5),
            2.6,
            PALE,
            segments=12,
            rings=8,
            scale=(1.0, 1.0, 1.1),
        ),
        part_cylinder(
            "claw_a",
            (-1.5, 1.0, -31.0),
            0.38,
            4.0,
            PALE,
            vertices=6,
            radius_top=0.05,
            smooth=False,
        ),
        part_cylinder(
            "claw_b",
            (0.0, 1.3, -31.8),
            0.42,
            4.8,
            PALE,
            vertices=6,
            radius_top=0.05,
            smooth=False,
        ),
        part_cylinder(
            "claw_c",
            (1.5, 1.0, -31.0),
            0.38,
            4.0,
            PALE,
            vertices=6,
            radius_top=0.05,
            smooth=False,
        ),
        part_sphere("cufflink", (0, 3.0, -25.5), 0.6, GLOW, segments=8, rings=4),
        part_box("sleeve_trim", (0, 4.0, -9.0), (0.8, 0.8, 6.0), GOLD, bevel=0.1),
    ]
    return _shift(join_parts(parts, "boss_vampire_part_arm"), dy=7.0)


def build_vampire_part_leg():
    """腿: 黑裤 + 高筒靴 + 金扣 + 尖头靴。"""
    parts = [
        part_cylinder(
            "thigh",
            (0, 0, -9),
            4.2,
            14,
            DARK,
            vertices=12,
            radius_top=3.8,
            smooth=True,
            bevel=0.4,
        ),
        part_sphere(
            "knee", (0, 0, -16), 3.6, HAIR, segments=12, rings=8, scale=(1.0, 1.0, 0.9)
        ),
        part_cylinder(
            "shin",
            (0, 0, -22),
            3.6,
            10,
            HAIR,
            vertices=12,
            radius_top=3.2,
            smooth=True,
            bevel=0.4,
        ),
        part_box("boot", (0, 2.4, -27.0), (8.0, 12.0, 6.0), HAIR, bevel=0.5, taper=0.9),
        part_box(
            "boot_cuff",
            (0, 2.4, -24.0),
            (8.5, 13.0, 1.8),
            CRIMSON,
            bevel=0.3,
            taper=0.92,
        ),
        part_box(
            "boot_toe", (0, 8.2, -28.0), (4.6, 4.0, 2.8), DARK, bevel=0.3, taper=0.6
        ),
        part_box("boot_buckle", (0, 7.0, -25.2), (2.0, 0.8, 1.6), GOLD, bevel=0.2),
        part_box("leg_stripe", (3.2, 4.0, -21.0), (0.8, 0.8, 7.0), CRIMSON, bevel=0.1),
    ]
    return join_parts(parts, "boss_vampire_part_leg")


def build_vampire_part_weapon():
    """猩红细剑: 黑柄 + 金护手/护指 + 血色宝石 + 冷银细刃。"""
    parts = [
        part_cylinder("grip", (0, 0, -4.0), 1.1, 8.0, HAIR, vertices=10, bevel=0.2),
        part_sphere("pommel", (0, 0, 0.5), 1.6, GOLD, segments=10, rings=6),
        part_sphere("pommel_gem", (0, 1.2, 0.5), 0.7, GLOW, segments=8, rings=4),
        part_box("guard", (0, 0, -8.8), (9.0, 2.4, 1.6), GOLD, bevel=0.3),
        part_box(
            "guard_tip_l",
            (-5.0, 0, -8.8),
            (2.0, 2.0, 2.6),
            GOLD,
            bevel=0.2,
            taper=0.2,
            rotation_z=math.radians(-28),
        ),
        part_box(
            "guard_tip_r",
            (5.0, 0, -8.8),
            (2.0, 2.0, 2.6),
            GOLD,
            bevel=0.2,
            taper=0.2,
            rotation_z=math.radians(28),
        ),
        part_box(
            "knuckle_bow",
            (3.4, 0, -5.0),
            (0.9, 1.2, 7.0),
            GOLD,
            bevel=0.2,
            rotation_z=math.radians(-8),
        ),
        part_sphere("guard_gem", (0, 1.6, -8.8), 0.9, GLOW, segments=8, rings=4),
        part_cylinder(
            "blade",
            (0, 0, -23.0),
            0.75,
            28.0,
            STEEL,
            vertices=4,
            radius_top=0.06,
            smooth=False,
            bevel=0.1,
        ),
        part_box("fuller", (0, 0.7, -22.0), (0.35, 0.25, 22.0), CRIMSON, bevel=0.05),
        part_box(
            "blade_tip", (0, 0, -37.2), (0.8, 0.8, 2.4), STEEL, bevel=0.1, taper=0.05
        ),
    ]
    return _shift(join_parts(parts, "boss_vampire_part_weapon"), dy=11.0)


VAMPIRE_PART_BUILDERS = {
    "head": build_vampire_part_head,
    "torso": build_vampire_part_torso,
    "arm": build_vampire_part_arm,
    "leg": build_vampire_part_leg,
    "weapon": build_vampire_part_weapon,
}
