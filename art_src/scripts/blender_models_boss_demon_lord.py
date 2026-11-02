# A16-Boss: 魔王手建低模 (第六个 Boss, 最终)
# 局部空间: 原点 = 骨骼锚点, x=宽, y=深度(前=+Y), z=高
# 骨架 ppu 1.20 (六 Boss 最大); 巨剑骨 x=+24 对齐前手并位于右侧
# 设计: 深渊魔王 — 紫罗兰重甲、四角王冠、蝠翼披风、魂核、巨型魔剑
import math

from blender_lib import part_box, part_cylinder, part_sphere, join_parts
from blender_models_player import export_part_json

# ── 魔王调色板 (采样自 boss_demon_lord_part_*.png) ──
VIOLET = (64, 48, 104)  # 主甲紫罗兰 (压暗)
VIOLET_LT = (96, 80, 152)  # 甲高光
VIOLET_PALE = (144, 128, 192)  # 淡紫刃/饰带
INDIGO = (32, 24, 64)  # 深紫甲
DARK = (12, 12, 32)  # 近黑内衬
GOLD = (150, 120, 70)  # 金饰
BONE = (200, 200, 176)  # 骨刺
GLOW = (180, 120, 255)  # 魂核
EYE = (255, 80, 110)  # 魔眼

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
    """绕 Y 轴倾斜的长方体 — 用于角/翼骨/甲片。"""
    obj = part_box(name, (0, 0, 0), size, color, bevel=bevel, taper=taper)
    angle = math.radians(slant_deg)
    cos_a, sin_a = math.cos(angle), math.sin(angle)
    for v in obj.data.vertices:
        x, z = v.co.x, v.co.z
        v.co.x = x * cos_a + z * sin_a + center[0]
        v.co.z = -x * sin_a + z * cos_a + center[2]
        v.co.y += center[1]
    return obj


def build_demon_lord_part_head():
    """头: 紫罗兰王盔 + 暗面 + 紫瞳 + 獠牙 + 四角王冠 + 金冠环。"""
    parts = [
        part_cylinder("neck", (0, 0, -7), 4.2, 8, INDIGO, vertices=12),
        part_sphere(
            "helm",
            (0, 0, 4.0),
            7.2,
            VIOLET,
            segments=18,
            rings=11,
            scale=(1.05, 0.96, 1.08),
        ),
        # 苍白魔面 + 深眼窝 + 颊甲
        part_box(
            "face", (0, 5.8, 1.2), (11.0, 3.2, 9.0), VIOLET_PALE, bevel=0.8, taper=0.9
        ),
        part_box("socket_l", (-2.8, 7.5, 2.4), (4.8, 0.8, 2.2), INDIGO, bevel=0.2),
        part_box("socket_r", (2.8, 7.5, 2.4), (4.8, 0.8, 2.2), INDIGO, bevel=0.2),
        part_box(
            "cheek_l",
            (-6.2, 3.2, 0.0),
            (3.4, 4.2, 7.0),
            VIOLET_LT,
            bevel=0.5,
            taper=0.7,
            rotation_z=math.radians(10),
        ),
        part_box(
            "cheek_r",
            (6.2, 3.2, 0.0),
            (3.4, 4.2, 7.0),
            VIOLET_LT,
            bevel=0.5,
            taper=0.7,
            rotation_z=math.radians(-10),
        ),
        # 眉/眼
        _slanted_box(
            "brow_l", (-2.8, 7.4, 4.4), (4.2, 1.2, 1.0), INDIGO, -16.0, bevel=0.2
        ),
        _slanted_box(
            "brow_r", (2.8, 7.4, 4.4), (4.2, 1.2, 1.0), INDIGO, 16.0, bevel=0.2
        ),
        _slanted_box("eye_l", (-2.8, 8.0, 2.4), (4.2, 0.8, 0.8), EYE, 12.0, bevel=0.15),
        _slanted_box("eye_r", (2.8, 8.0, 2.4), (4.2, 0.8, 0.8), EYE, -12.0, bevel=0.15),
        # 鼻/口/獠牙
        part_box("nose", (0, 7.8, 0.0), (2.2, 2.2, 3.0), INDIGO, bevel=0.3, taper=0.75),
        part_box("mouth", (0, 7.8, -3.0), (6.5, 0.8, 0.8), DARK, bevel=0.1),
        part_cylinder(
            "fang_l",
            (-2.0, 8.1, -3.5),
            0.45,
            2.2,
            BONE,
            vertices=6,
            radius_top=0.08,
            smooth=False,
        ),
        part_cylinder(
            "fang_r",
            (2.0, 8.1, -3.5),
            0.45,
            2.2,
            BONE,
            vertices=6,
            radius_top=0.08,
            smooth=False,
        ),
        # 王冠金环 + 中央魂晶
        part_box("crown_band", (0, 6.8, 8.4), (13.0, 1.8, 2.0), GOLD, bevel=0.3),
        part_sphere(
            "crown_gem",
            (0, 7.8, 8.4),
            1.3,
            GLOW,
            segments=10,
            rings=6,
            scale=(1.0, 0.5, 1.0),
        ),
        part_sphere("crown_gem_l", (-4.5, 7.8, 8.4), 0.7, GLOW, segments=8, rings=4),
        part_sphere("crown_gem_r", (4.5, 7.8, 8.4), 0.7, GLOW, segments=8, rings=4),
        # 四角 (两长两短) + 中央冠刺
        part_box(
            "horn_l1",
            (-7.0, 0.0, 9.0),
            (3.6, 4.8, 12.0),
            INDIGO,
            bevel=0.4,
            taper=0.45,
            rotation_z=math.radians(28),
            rotation_x=math.radians(-10),
        ),
        part_box(
            "horn_r1",
            (7.0, 0.0, 9.0),
            (3.6, 4.8, 12.0),
            INDIGO,
            bevel=0.4,
            taper=0.45,
            rotation_z=math.radians(-28),
            rotation_x=math.radians(-10),
        ),
        part_box(
            "horn_l2",
            (-9.8, -0.4, 16.0),
            (2.6, 3.4, 7.0),
            VIOLET_PALE,
            bevel=0.3,
            taper=0.15,
            rotation_z=math.radians(36),
            rotation_x=math.radians(-16),
        ),
        part_box(
            "horn_r2",
            (9.8, -0.4, 16.0),
            (2.6, 3.4, 7.0),
            VIOLET_PALE,
            bevel=0.3,
            taper=0.15,
            rotation_z=math.radians(-36),
            rotation_x=math.radians(-16),
        ),
        part_box(
            "horn_f_l",
            (-4.2, 2.8, 12.0),
            (2.0, 2.6, 6.0),
            VIOLET_LT,
            bevel=0.2,
            taper=0.15,
            rotation_z=math.radians(16),
        ),
        part_box(
            "horn_f_r",
            (4.2, 2.8, 12.0),
            (2.0, 2.6, 6.0),
            VIOLET_LT,
            bevel=0.2,
            taper=0.15,
            rotation_z=math.radians(-16),
        ),
        _slanted_box(
            "crown_spike",
            (0, 0, 17.0),
            (3.0, 3.6, 13.0),
            VIOLET_PALE,
            0.0,
            bevel=0.3,
            taper=0.12,
        ),
        # 盔顶甲片
        part_box(
            "helm_top",
            (0, -1.0, 10.0),
            (10.0, 9.0, 2.4),
            VIOLET_LT,
            bevel=0.4,
            taper=0.85,
            rotation_x=math.radians(-5),
        ),
    ]
    # 头部前探: 下颚/獠牙越过胸甲横披
    return _shift(join_parts(parts, "boss_demon_lord_part_head"), dy=1.5)


def build_demon_lord_part_torso():
    """躯干: 宽胸重甲 + 淡紫横披 + 魂核 + 巨肩尖甲 + 蝠翼 + 金腰 + 甲裙。"""
    parts = [
        part_box(
            "chest", (0, 0, 7), (36, 20, 20), VIOLET, bevel=1.4, segments=2, taper=0.65
        ),
        # 2D 淡紫横披
        part_box(
            "mantle",
            (0, 10.2, 7.0),
            (28.0, 2.4, 4.0),
            VIOLET_PALE,
            bevel=0.4,
            taper=0.9,
        ),
        part_box("mantle_trim", (0, 10.8, 9.2), (24.0, 1.0, 1.2), GOLD, bevel=0.2),
        # 胸口魂核
        part_sphere(
            "core",
            (0, 11.0, 5.0),
            3.2,
            GLOW,
            segments=14,
            rings=8,
            scale=(1.0, 0.5, 1.0),
        ),
        part_box(
            "core_rim_l",
            (-4.4, 10.4, 5.0),
            (2.2, 1.6, 7.0),
            INDIGO,
            bevel=0.3,
            taper=0.6,
            rotation_z=math.radians(-16),
        ),
        part_box(
            "core_rim_r",
            (4.4, 10.4, 5.0),
            (2.2, 1.6, 7.0),
            INDIGO,
            bevel=0.3,
            taper=0.6,
            rotation_z=math.radians(16),
        ),
        # 高立领
        part_box(
            "collar_l",
            (-9.0, -1.0, 16.0),
            (5.0, 5.0, 8.0),
            INDIGO,
            bevel=0.4,
            taper=0.55,
            rotation_z=math.radians(8),
        ),
        part_box(
            "collar_r",
            (9.0, -1.0, 16.0),
            (5.0, 5.0, 8.0),
            INDIGO,
            bevel=0.4,
            taper=0.55,
            rotation_z=math.radians(-8),
        ),
        part_box("collar_trim_l", (-9.0, 2.0, 16.0), (4.0, 1.0, 1.2), GOLD, bevel=0.2),
        part_box("collar_trim_r", (9.0, 2.0, 16.0), (4.0, 1.0, 1.2), GOLD, bevel=0.2),
        # 巨肩尖甲
        part_box(
            "pauldron_l",
            (-20.0, 0, 10.0),
            (14.0, 16.0, 14.0),
            VIOLET,
            bevel=1.2,
            taper=0.45,
            rotation_z=math.radians(22),
        ),
        part_box(
            "pauldron_r",
            (20.0, 0, 10.0),
            (14.0, 16.0, 14.0),
            VIOLET,
            bevel=1.2,
            taper=0.45,
            rotation_z=math.radians(-22),
        ),
        part_box(
            "pauldron2_l",
            (-18.0, 0, 3.5),
            (10.0, 12.0, 7.0),
            INDIGO,
            bevel=0.8,
            taper=0.6,
            rotation_z=math.radians(22),
        ),
        part_box(
            "pauldron2_r",
            (18.0, 0, 3.5),
            (10.0, 12.0, 7.0),
            INDIGO,
            bevel=0.8,
            taper=0.6,
            rotation_z=math.radians(-22),
        ),
        part_box(
            "spike_l",
            (-23.0, 0, 17.0),
            (4.0, 5.0, 12.0),
            VIOLET_PALE,
            bevel=0.3,
            taper=0.15,
            rotation_z=math.radians(34),
        ),
        part_box(
            "spike_r",
            (23.0, 0, 17.0),
            (4.0, 5.0, 12.0),
            VIOLET_PALE,
            bevel=0.3,
            taper=0.15,
            rotation_z=math.radians(-34),
        ),
        # 蝠翼: 三层尖翼膜 + 骨肋 + 翼爪
        _slanted_box(
            "wing_l1",
            (-22.0, -9.0, 15.0),
            (18.0, 2.0, 10.0),
            INDIGO,
            22.0,
            bevel=0.3,
            taper=0.12,
        ),
        _slanted_box(
            "wing_l2",
            (-24.0, -9.0, 6.0),
            (16.0, 2.0, 11.0),
            INDIGO,
            12.0,
            bevel=0.3,
            taper=0.12,
        ),
        _slanted_box(
            "wing_l3",
            (-25.0, -9.0, -3.0),
            (13.0, 2.0, 10.0),
            INDIGO,
            4.0,
            bevel=0.3,
            taper=0.12,
        ),
        _slanted_box(
            "wing_r1",
            (22.0, -9.0, 15.0),
            (18.0, 2.0, 10.0),
            INDIGO,
            -22.0,
            bevel=0.3,
            taper=0.12,
        ),
        _slanted_box(
            "wing_r2",
            (24.0, -9.0, 6.0),
            (16.0, 2.0, 11.0),
            INDIGO,
            -12.0,
            bevel=0.3,
            taper=0.12,
        ),
        _slanted_box(
            "wing_r3",
            (25.0, -9.0, -3.0),
            (13.0, 2.0, 10.0),
            INDIGO,
            -4.0,
            bevel=0.3,
            taper=0.12,
        ),
        _slanted_box(
            "wing_rib_l1",
            (-21.0, -10.0, 16.0),
            (18.0, 1.2, 1.2),
            VIOLET_LT,
            22.0,
            bevel=0.1,
        ),
        _slanted_box(
            "wing_rib_l2",
            (-23.0, -10.0, 7.0),
            (16.0, 1.2, 1.2),
            VIOLET_LT,
            12.0,
            bevel=0.1,
        ),
        _slanted_box(
            "wing_rib_r1",
            (21.0, -10.0, 16.0),
            (18.0, 1.2, 1.2),
            VIOLET_LT,
            -22.0,
            bevel=0.1,
        ),
        _slanted_box(
            "wing_rib_r2",
            (23.0, -10.0, 7.0),
            (16.0, 1.2, 1.2),
            VIOLET_LT,
            -12.0,
            bevel=0.1,
        ),
        _slanted_box(
            "wing_claw_l",
            (-29.0, -9.0, 21.0),
            (2.4, 2.8, 5.0),
            BONE,
            30.0,
            bevel=0.2,
            taper=0.12,
        ),
        _slanted_box(
            "wing_claw_r",
            (29.0, -9.0, 21.0),
            (2.4, 2.8, 5.0),
            BONE,
            -30.0,
            bevel=0.2,
            taper=0.12,
        ),
        # 腰 + 金腰 + 甲裙
        part_box(
            "waist", (0, 0, -7.0), (18.0, 13.0, 9.0), INDIGO, bevel=0.6, taper=0.95
        ),
        part_box("belt", (0, 0, -10.5), (21.0, 13.5, 4.0), GOLD, bevel=0.5),
        part_box("belt_gem", (0, 7.4, -10.5), (3.0, 1.0, 3.2), GLOW, bevel=0.2),
        part_box(
            "tasset_l",
            (-7.5, 7.0, -15.0),
            (6.0, 2.0, 10.0),
            VIOLET,
            bevel=0.4,
            taper=0.8,
        ),
        part_box(
            "tasset_c",
            (0, 7.2, -16.0),
            (6.0, 2.0, 11.0),
            VIOLET_LT,
            bevel=0.4,
            taper=0.8,
        ),
        part_box(
            "tasset_r",
            (7.5, 7.0, -15.0),
            (6.0, 2.0, 10.0),
            VIOLET,
            bevel=0.4,
            taper=0.8,
        ),
        part_box(
            "tasset_side_l",
            (-11.0, 2.0, -15.0),
            (5.0, 10.0, 9.0),
            INDIGO,
            bevel=0.4,
            taper=0.8,
        ),
        part_box(
            "tasset_side_r",
            (11.0, 2.0, -15.0),
            (5.0, 10.0, 9.0),
            INDIGO,
            bevel=0.4,
            taper=0.8,
        ),
        # 金色前襟
        part_box("front_trim", (0, 10.8, 2.0), (1.4, 0.8, 10.0), GOLD, bevel=0.15),
        # 翼膜 (连接三层尖翼) + 胸甲金线 + 肩铆
        part_box(
            "wing_membrane_l",
            (-23.0, -9.8, 4.0),
            (18.0, 1.2, 24.0),
            INDIGO,
            bevel=0.3,
            taper=0.8,
            rotation_z=math.radians(8),
        ),
        part_box(
            "wing_membrane_r",
            (23.0, -9.8, 4.0),
            (18.0, 1.2, 24.0),
            INDIGO,
            bevel=0.3,
            taper=0.8,
            rotation_z=math.radians(-8),
        ),
        _slanted_box(
            "chest_gold_l", (-7.0, 10.8, 4.0), (0.8, 0.8, 8.0), GOLD, -12.0, bevel=0.1
        ),
        _slanted_box(
            "chest_gold_r", (7.0, 10.8, 4.0), (0.8, 0.8, 8.0), GOLD, 12.0, bevel=0.1
        ),
        part_sphere(
            "shoulder_rivet_l", (-22.0, 8.0, 10.0), 0.75, GOLD, segments=8, rings=4
        ),
        part_sphere(
            "shoulder_rivet_r", (22.0, 8.0, 10.0), 0.75, GOLD, segments=8, rings=4
        ),
        part_box("belt_gem_l", (-6.0, 7.4, -10.5), (1.6, 0.8, 2.0), GLOW, bevel=0.1),
        part_box("belt_gem_r", (6.0, 7.4, -10.5), (1.6, 0.8, 2.0), GLOW, bevel=0.1),
    ]
    return join_parts(parts, "boss_demon_lord_part_torso")


def build_demon_lord_part_arm():
    """臂: 重甲肩球 + 紫甲上臂 + 肘刺 + 前臂 + 金腕 + 巨爪护手。"""
    parts = [
        part_sphere("shoulder", (0, 0, -2), 5.0, VIOLET, segments=14, rings=8),
        part_cylinder(
            "upper",
            (0, 0, -9),
            4.6,
            13,
            VIOLET,
            vertices=12,
            radius_top=5.0,
            smooth=True,
            bevel=0.4,
        ),
        part_box(
            "upper_plate",
            (0, 4.6, -9),
            (5.0, 1.6, 8.0),
            VIOLET_LT,
            bevel=0.3,
            taper=0.85,
        ),
        part_sphere("elbow", (0, 0, -16), 4.4, INDIGO, segments=12, rings=8),
        part_box(
            "elbow_spike",
            (0, -4.0, -15.5),
            (2.4, 2.4, 6.0),
            VIOLET_PALE,
            bevel=0.3,
            taper=0.15,
            rotation_x=math.radians(90),
        ),
        part_cylinder(
            "forearm",
            (0, 0, -21),
            4.0,
            10,
            INDIGO,
            vertices=12,
            radius_top=4.4,
            smooth=True,
            bevel=0.4,
        ),
        part_cylinder("wrist", (0, 0, -25.5), 4.6, 2.2, GOLD, vertices=12, bevel=0.3),
        part_sphere(
            "fist",
            (0, 0, -29.0),
            4.8,
            VIOLET,
            segments=12,
            rings=8,
            scale=(1.05, 1.0, 1.05),
        ),
        part_cylinder(
            "claw_a",
            (-2.6, 1.2, -32.5),
            0.8,
            5.0,
            BONE,
            vertices=6,
            radius_top=0.1,
            smooth=False,
        ),
        part_cylinder(
            "claw_b",
            (0.0, 1.6, -33.2),
            0.9,
            5.8,
            BONE,
            vertices=6,
            radius_top=0.1,
            smooth=False,
        ),
        part_cylinder(
            "claw_c",
            (2.6, 1.2, -32.5),
            0.8,
            5.0,
            BONE,
            vertices=6,
            radius_top=0.1,
            smooth=False,
        ),
        part_sphere("gauntlet_gem", (0, 4.2, -28.5), 1.0, GLOW, segments=8, rings=4),
        _slanted_box(
            "crack_upper",
            (-4.2, 4.0, -9.0),
            (0.8, 1.0, 6.0),
            VIOLET_LT,
            12.0,
            bevel=0.1,
        ),
    ]
    return _shift(join_parts(parts, "boss_demon_lord_part_arm"), dy=6.0)


def build_demon_lord_part_leg():
    """腿: 紫甲重腿 + 膝甲 + 胫甲 + 金边巨靴 + 骨爪趾。"""
    parts = [
        part_cylinder(
            "thigh",
            (0, 0, -9),
            5.8,
            15,
            VIOLET,
            vertices=12,
            radius_top=5.2,
            smooth=True,
            bevel=0.5,
        ),
        part_box(
            "thigh_plate",
            (0, 5.4, -9),
            (9.0, 2.0, 11.0),
            VIOLET_LT,
            bevel=0.4,
            taper=0.85,
        ),
        part_sphere(
            "knee",
            (0, 1.0, -17),
            4.8,
            INDIGO,
            segments=12,
            rings=8,
            scale=(1.0, 1.0, 0.88),
        ),
        part_box(
            "knee_spike",
            (0, -4.2, -16.5),
            (2.2, 2.2, 5.5),
            VIOLET_PALE,
            bevel=0.3,
            taper=0.15,
            rotation_x=math.radians(90),
        ),
        part_cylinder(
            "shin",
            (0, 0, -23),
            4.6,
            10,
            INDIGO,
            vertices=12,
            radius_top=4.0,
            smooth=True,
            bevel=0.5,
        ),
        part_box(
            "greave", (0, 5.0, -23), (7.0, 1.6, 9.0), VIOLET, bevel=0.4, taper=0.85
        ),
        part_cylinder(
            "ankle_band", (0, 0, -25.0), 4.8, 1.6, GOLD, vertices=12, bevel=0.2
        ),
        part_box(
            "boot", (0, 2.8, -27.5), (13.0, 16.0, 6.0), DARK, bevel=0.7, taper=0.9
        ),
        part_box(
            "boot_trim", (0, 2.8, -24.5), (13.0, 16.0, 1.4), GOLD, bevel=0.2, taper=0.9
        ),
        part_cylinder(
            "toe_a",
            (-3.8, 9.5, -28.5),
            0.9,
            4.5,
            BONE,
            vertices=6,
            radius_top=0.1,
            smooth=False,
        ),
        part_cylinder(
            "toe_b",
            (0.0, 10.0, -28.5),
            1.0,
            5.0,
            BONE,
            vertices=6,
            radius_top=0.1,
            smooth=False,
        ),
        part_cylinder(
            "toe_c",
            (3.8, 9.5, -28.5),
            0.9,
            4.5,
            BONE,
            vertices=6,
            radius_top=0.1,
            smooth=False,
        ),
    ]
    return join_parts(parts, "boss_demon_lord_part_leg")


def build_demon_lord_part_weapon():
    """深渊魔剑: 金护手 + 紫晶柄尾 + 淡紫巨刃 + 暗脊/魂光符文。"""
    parts = [
        part_cylinder("grip", (0, 0, -4.0), 1.6, 8.0, DARK, vertices=10, bevel=0.2),
        part_sphere("pommel", (0, 0, 0.5), 2.0, GOLD, segments=10, rings=6),
        part_sphere("pommel_gem", (0, 1.4, 0.5), 0.9, GLOW, segments=8, rings=4),
        part_box("guard", (0, 0, -8.8), (22.0, 3.0, 2.4), GOLD, bevel=0.4),
        part_box(
            "guard_tip_l",
            (-11.5, 0, -8.8),
            (4.0, 3.0, 4.0),
            VIOLET_PALE,
            bevel=0.2,
            taper=0.15,
            rotation_z=math.radians(-28),
        ),
        part_box(
            "guard_tip_r",
            (11.5, 0, -8.8),
            (4.0, 3.0, 4.0),
            VIOLET_PALE,
            bevel=0.2,
            taper=0.15,
            rotation_z=math.radians(28),
        ),
        part_sphere(
            "guard_gem",
            (0, 2.2, -8.8),
            1.4,
            GLOW,
            segments=10,
            rings=6,
            scale=(1.0, 0.5, 1.0),
        ),
        part_cylinder(
            "blade",
            (0, 0, -25.0),
            0.25,
            32.0,
            VIOLET_PALE,
            vertices=4,
            radius_top=4.8,
            smooth=False,
            bevel=0.1,
        ),
        part_box(
            "fuller", (0, 1.8, -23.0), (1.0, 0.4, 26.0), DARK, bevel=0.1, taper=0.4
        ),
        part_box("rune_a", (0, 2.1, -16.0), (1.2, 0.4, 3.0), GLOW, bevel=0.1),
        part_box("rune_b", (0, 2.1, -25.0), (1.2, 0.4, 3.0), GLOW, bevel=0.1),
        part_box("rune_c", (0, 2.1, -34.0), (1.2, 0.4, 3.0), GLOW, bevel=0.1),
        part_box(
            "blade_tip",
            (0, 0, -42.5),
            (1.2, 1.2, 3.0),
            VIOLET_PALE,
            bevel=0.1,
            taper=0.05,
        ),
    ]
    return _shift(join_parts(parts, "boss_demon_lord_part_weapon"), dy=10.0)


DEMON_LORD_PART_BUILDERS = {
    "head": build_demon_lord_part_head,
    "torso": build_demon_lord_part_torso,
    "arm": build_demon_lord_part_arm,
    "leg": build_demon_lord_part_leg,
    "weapon": build_demon_lord_part_weapon,
}
