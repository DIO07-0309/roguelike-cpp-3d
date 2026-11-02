# A16-Boss: 魔像手建低模 (第三个 Boss)
# 局部空间: 原点 = 骨骼锚点, x=宽, y=深度(前=+Y), z=高
# 骨架 ppu 1.15; arm/weapon 骨靠中 → 前移避免陷入躯干
# 设计: 远古石魔像 — 无颈巨石肩、厚重石板甲、青铜腰带、琥珀眼、巨型石锤
import math

from blender_lib import part_box, part_cylinder, part_sphere, join_parts
from blender_models_player import export_part_json

# ── 魔像调色板 (采样自 boss_golem_part_*.png) ──
STONE_LT = (160, 160, 176)  # 石面高光
STONE = (112, 112, 128)  # 主石
STONE_DK = (64, 64, 80)  # 暗石
ROCK = (32, 32, 48)  # 石缝/阴影
CRACK = (48, 32, 48)  # 暗紫裂纹
BRONZE = (150, 120, 70)  # 青铜饰带
BRONZE_DK = (96, 80, 64)  # 青铜暗部
GLOW = (255, 176, 64)  # 琥珀眼/符文

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
    """绕 Y 轴倾斜的石板 — 正面可见 x-z 斜度。"""
    obj = part_box(name, (0, 0, 0), size, color, bevel=bevel, taper=taper)
    angle = math.radians(slant_deg)
    cos_a, sin_a = math.cos(angle), math.sin(angle)
    for v in obj.data.vertices:
        x, z = v.co.x, v.co.z
        v.co.x = x * cos_a + z * sin_a + center[0]
        v.co.z = -x * sin_a + z * cos_a + center[2]
        v.co.y += center[1]
    return obj


def _rotate_y(obj, angle_deg):
    """绕 Y 轴整体旋转 (x-z 平面), 用于斜持长柄武器。"""
    angle = math.radians(angle_deg)
    cos_a, sin_a = math.cos(angle), math.sin(angle)
    for v in obj.data.vertices:
        x, z = v.co.x, v.co.z
        v.co.x = x * cos_a + z * sin_a
        v.co.z = -x * sin_a + z * cos_a
    return obj


def build_golem_part_head():
    """头: 无颈巨石颅 + 厚眉岩檐 + 琥珀深眼 + 石块下颚/石须 + 裂纹。"""
    parts = [
        part_box("neck", (0, 0, -6.5), (8.5, 8.5, 5.0), STONE_DK, bevel=0.6),
        part_sphere(
            "skull",
            (0, 0, 3.0),
            7.2,
            STONE,
            segments=18,
            rings=11,
            scale=(1.10, 0.95, 0.96),
        ),
        # 头顶石板 + 后脑岩块
        part_box(
            "crown",
            (0, -0.8, 9.0),
            (12.5, 10.5, 3.2),
            STONE_LT,
            bevel=0.5,
            taper=0.88,
            rotation_x=math.radians(-5),
        ),
        part_box(
            "crown_back",
            (0, -6.0, 6.0),
            (9.5, 4.0, 6.0),
            STONE_DK,
            bevel=0.6,
            taper=0.8,
        ),
        # 厚眉岩檐 (深眼窝)
        part_box(
            "brow",
            (0, 6.6, 4.4),
            (13.5, 4.0, 3.2),
            STONE_DK,
            bevel=0.6,
            taper=0.92,
            rotation_x=math.radians(-8),
        ),
        part_box("eye_socket_l", (-3.2, 7.2, 1.4), (4.2, 1.2, 2.6), ROCK, bevel=0.3),
        part_box("eye_socket_r", (3.2, 7.2, 1.4), (4.2, 1.2, 2.6), ROCK, bevel=0.3),
        part_box("eye_l", (-3.2, 8.0, 1.4), (2.8, 0.8, 0.85), GLOW, bevel=0.15),
        part_box("eye_r", (3.2, 8.0, 1.4), (2.8, 0.8, 0.85), GLOW, bevel=0.15),
        # 鼻梁 + 宽下颚
        part_box(
            "nose", (0, 7.2, -1.2), (4.4, 3.4, 3.2), STONE_DK, bevel=0.4, taper=0.9
        ),
        part_box(
            "jaw", (0, 6.6, -4.6), (11.5, 4.2, 3.0), STONE_DK, bevel=0.5, taper=0.88
        ),
        part_box("chin", (0, 6.8, -6.4), (7.5, 3.2, 2.0), STONE, bevel=0.4),
        # 侧颊巨石
        part_box(
            "cheek_l",
            (-6.6, 4.2, -1.0),
            (4.2, 5.0, 5.0),
            STONE_DK,
            bevel=0.6,
            taper=0.85,
            rotation_z=math.radians(12),
        ),
        part_box(
            "cheek_r",
            (6.6, 4.2, -1.0),
            (4.2, 5.0, 5.0),
            STONE_DK,
            bevel=0.6,
            taper=0.85,
            rotation_z=math.radians(-12),
        ),
        # 石块下颚胡须
        part_box(
            "beard_l", (-3.4, 6.2, -8.2), (3.2, 3.0, 3.6), STONE, bevel=0.4, taper=0.7
        ),
        part_box(
            "beard_c",
            (0.0, 6.6, -8.8),
            (3.8, 3.2, 4.4),
            STONE_LT,
            bevel=0.4,
            taper=0.65,
        ),
        part_box(
            "beard_r", (3.4, 6.2, -8.2), (3.2, 3.0, 3.6), STONE, bevel=0.4, taper=0.7
        ),
        # 青铜眉饰 + 琥珀额符
        part_box("brow_trim", (0, 8.4, 5.4), (10.0, 1.2, 1.2), BRONZE, bevel=0.3),
        part_box("forehead_rune", (0, 7.4, 7.0), (2.2, 1.0, 2.8), BRONZE_DK, bevel=0.2),
        part_sphere("rune_core", (0, 8.2, 7.0), 0.7, GLOW, segments=8, rings=4),
        # 石裂纹
        _slanted_box(
            "crack_face_l", (-5.6, 7.0, 3.2), (0.9, 1.0, 5.0), CRACK, -22.0, bevel=0.1
        ),
        _slanted_box(
            "crack_face_r", (5.4, 7.0, 5.0), (0.8, 1.0, 4.0), CRACK, 18.0, bevel=0.1
        ),
        part_box(
            "crack_jaw",
            (2.2, 8.0, -4.0),
            (0.8, 1.0, 3.0),
            CRACK,
            bevel=0.1,
            rotation_z=math.radians(12),
        ),
    ]
    return join_parts(parts, "boss_golem_part_head")


def build_golem_part_torso():
    """躯干: 无颈巨胸 + 双肩巨岩 + 分层石板 + 青铜腰带/铆钉 + 背岩 + 裂纹。"""
    parts = [
        part_box(
            "chest", (0, 0, 7), (36, 20, 20), STONE, bevel=1.4, segments=2, taper=0.68
        ),
        # 胸甲分层石板
        part_box(
            "chest_slab", (0, 9.4, 7.5), (24, 2.4, 13), STONE_LT, bevel=0.8, taper=0.82
        ),
        part_box(
            "chest_slab_low",
            (0, 8.8, -0.5),
            (18, 1.8, 6),
            STONE_LT,
            bevel=0.6,
            taper=0.9,
        ),
        part_box("chest_seam", (0, 10.2, 1.0), (1.0, 1.0, 12), CRACK, bevel=0.1),
        # 青铜胸饰
        part_sphere(
            "chest_medallion",
            (0, 10.8, 8.0),
            2.4,
            BRONZE,
            segments=12,
            rings=8,
            scale=(1.0, 0.5, 1.0),
        ),
        part_sphere(
            "chest_rune",
            (0, 11.4, 8.0),
            1.0,
            GLOW,
            segments=8,
            rings=4,
            scale=(1.0, 0.4, 1.0),
        ),
        # 腰腹 + 青铜腰带
        part_box("waist", (0, 0, -6), (21, 14, 10), STONE_DK, bevel=0.8, taper=0.94),
        part_box("belly", (0, 7.2, -5.0), (14, 1.4, 7.5), STONE_LT, bevel=0.4),
        part_box("belt", (0, 0, -11.0), (23, 14.5, 4.2), BRONZE, bevel=0.6),
        part_box("belt_trim", (0, 7.5, -11.0), (19, 1.0, 2.0), BRONZE_DK, bevel=0.2),
        # 腰带铆钉
        part_sphere("stud_l2", (-8.0, 7.8, -11.0), 0.8, BRONZE_DK, segments=8, rings=4),
        part_sphere("stud_l1", (-4.0, 7.8, -11.0), 0.8, BRONZE_DK, segments=8, rings=4),
        part_sphere("stud_c", (0.0, 7.8, -11.0), 0.9, GLOW, segments=8, rings=4),
        part_sphere("stud_r1", (4.0, 7.8, -11.0), 0.8, BRONZE_DK, segments=8, rings=4),
        part_sphere("stud_r2", (8.0, 7.8, -11.0), 0.8, BRONZE_DK, segments=8, rings=4),
        # 石髋
        part_box("hip", (0, 0, -15.0), (19, 14, 5), STONE_DK, bevel=0.6, taper=0.9),
        # 双肩巨岩 (主剪影)
        part_box(
            "shoulder_l",
            (-20.0, 0, 10.0),
            (15.0, 17.0, 15.0),
            STONE,
            bevel=1.5,
            taper=0.55,
            rotation_z=math.radians(18),
        ),
        part_box(
            "shoulder_r",
            (20.0, 0, 10.0),
            (15.0, 17.0, 15.0),
            STONE,
            bevel=1.5,
            taper=0.55,
            rotation_z=math.radians(-18),
        ),
        part_box(
            "shoulder_top_l",
            (-19.0, 0, 17.0),
            (10.5, 12.0, 4.0),
            STONE_LT,
            bevel=0.6,
            taper=0.8,
            rotation_z=math.radians(18),
        ),
        part_box(
            "shoulder_top_r",
            (19.0, 0, 17.0),
            (10.5, 12.0, 4.0),
            STONE_LT,
            bevel=0.6,
            taper=0.8,
            rotation_z=math.radians(-18),
        ),
        # 肩前青铜饰带 + 铆钉
        part_box(
            "shoulder_band_l", (-20.0, 7.6, 10.5), (11.0, 1.2, 2.0), BRONZE, bevel=0.2
        ),
        part_box(
            "shoulder_band_r", (20.0, 7.6, 10.5), (11.0, 1.2, 2.0), BRONZE, bevel=0.2
        ),
        part_sphere(
            "shoulder_stud_l", (-24.0, 8.0, 10.5), 0.8, BRONZE_DK, segments=8, rings=4
        ),
        part_sphere(
            "shoulder_stud_r", (24.0, 8.0, 10.5), 0.8, BRONZE_DK, segments=8, rings=4
        ),
        # 背岩
        part_box(
            "back_rock_l",
            (-10.0, -8.0, 13.0),
            (7.0, 6.0, 8.0),
            STONE_DK,
            bevel=0.5,
            taper=0.65,
            rotation_z=math.radians(-14),
        ),
        part_box(
            "back_rock_r",
            (10.0, -8.0, 13.0),
            (7.0, 6.0, 8.0),
            STONE_DK,
            bevel=0.5,
            taper=0.65,
            rotation_z=math.radians(14),
        ),
        # 裂纹
        _slanted_box(
            "crack_chest_l", (-9.0, 10.4, 4.0), (0.9, 1.0, 8.0), CRACK, 16.0, bevel=0.1
        ),
        _slanted_box(
            "crack_chest_r", (9.0, 10.4, 6.0), (0.8, 1.0, 7.0), CRACK, -12.0, bevel=0.1
        ),
        part_box(
            "crack_waist",
            (4.0, 7.8, -5.0),
            (0.8, 1.0, 5.0),
            CRACK,
            bevel=0.1,
            rotation_z=math.radians(10),
        ),
        # 肩部琥珀符文 + 碎石缺口
        part_box(
            "shoulder_rune_l", (-20.0, 8.3, 13.0), (2.2, 0.8, 3.2), GLOW, bevel=0.1
        ),
        part_box(
            "shoulder_rune_r", (20.0, 8.3, 13.0), (2.2, 0.8, 3.2), GLOW, bevel=0.1
        ),
        part_sphere(
            "shoulder_chip_l", (-25.0, 5.0, 17.0), 1.2, STONE_LT, segments=8, rings=4
        ),
        part_sphere(
            "shoulder_chip_r", (25.0, 5.0, 17.0), 1.2, STONE_LT, segments=8, rings=4
        ),
        part_box(
            "crack_chest_c",
            (0.0, 10.6, -1.0),
            (0.8, 1.0, 5.0),
            CRACK,
            bevel=0.1,
            rotation_z=math.radians(-6),
        ),
        part_box(
            "back_chip_l",
            (-14.0, -7.0, 18.0),
            (3.0, 3.0, 2.0),
            STONE_LT,
            bevel=0.2,
            rotation_z=math.radians(-18),
        ),
        part_box(
            "back_chip_r",
            (14.0, -7.0, 18.0),
            (3.0, 3.0, 2.0),
            STONE_LT,
            bevel=0.2,
            rotation_z=math.radians(18),
        ),
    ]
    return join_parts(parts, "boss_golem_part_torso")


def build_golem_part_arm():
    """臂: 巨肩岩球 + 石柱上臂 + 肘岩 + 前臂 + 青铜腕环 + 巨拳。"""
    parts = [
        part_sphere("shoulder", (0, 0, -2), 5.4, STONE, segments=14, rings=8),
        part_cylinder(
            "upper",
            (0, 0, -9),
            5.0,
            13,
            STONE,
            vertices=12,
            radius_top=5.4,
            smooth=True,
            bevel=0.5,
        ),
        part_box(
            "upper_plate", (0, 4.6, -9), (5.0, 1.8, 9.0), STONE_LT, bevel=0.4, taper=0.9
        ),
        part_sphere("elbow", (0, 0, -16), 5.2, STONE_DK, segments=12, rings=8),
        part_cylinder(
            "forearm",
            (0, 0, -21),
            4.6,
            10,
            STONE,
            vertices=12,
            radius_top=5.0,
            smooth=True,
            bevel=0.5,
        ),
        part_cylinder(
            "wrist_band", (0, 0, -25.5), 5.0, 2.0, BRONZE, vertices=12, bevel=0.3
        ),
        part_sphere(
            "fist",
            (0, 0, -29.5),
            5.4,
            STONE,
            segments=14,
            rings=8,
            scale=(1.05, 1.0, 1.0),
        ),
        part_box("knuckle_l", (-2.4, 3.8, -30.5), (3.0, 3.2, 3.4), STONE_LT, bevel=0.4),
        part_box("knuckle_c", (0.0, 4.2, -31.0), (3.4, 3.4, 3.6), STONE_LT, bevel=0.4),
        part_box("knuckle_r", (2.4, 3.8, -30.5), (3.0, 3.2, 3.4), STONE_LT, bevel=0.4),
        part_sphere("fist_rivet", (0, 4.6, -27.8), 0.8, BRONZE_DK, segments=8, rings=4),
        _slanted_box(
            "crack_upper", (-4.2, 3.8, -9.0), (0.8, 1.0, 7.0), CRACK, 12.0, bevel=0.1
        ),
        _slanted_box(
            "crack_fore", (3.8, 3.8, -21.0), (0.8, 1.0, 5.0), CRACK, -14.0, bevel=0.1
        ),
    ]
    return _shift(join_parts(parts, "boss_golem_part_arm"), dy=5.0)


def build_golem_part_leg():
    """腿: 粗石柱大腿 + 膝岩 + 小腿 + 青铜踝环 + 巨岩足/石趾。"""
    parts = [
        part_cylinder(
            "thigh",
            (0, 0, -9),
            6.5,
            15,
            STONE,
            vertices=12,
            radius_top=5.8,
            smooth=True,
            bevel=0.6,
        ),
        part_box(
            "thigh_plate",
            (0, 5.6, -9),
            (9.5, 1.8, 10.0),
            STONE_LT,
            bevel=0.5,
            taper=0.88,
        ),
        part_sphere(
            "knee",
            (0, 1.0, -17),
            5.4,
            STONE_LT,
            segments=12,
            rings=8,
            scale=(1.0, 1.0, 0.88),
        ),
        part_cylinder(
            "shin",
            (0, 0, -23),
            5.2,
            10,
            STONE_DK,
            vertices=12,
            radius_top=4.6,
            smooth=True,
            bevel=0.6,
        ),
        part_cylinder(
            "ankle_band", (0, 0, -25.0), 5.4, 1.8, BRONZE, vertices=12, bevel=0.3
        ),
        part_box(
            "foot", (0, 3.0, -27.5), (15.5, 18.0, 6.0), STONE, bevel=0.8, taper=0.92
        ),
        part_box(
            "foot_top",
            (0, 3.0, -24.5),
            (14.0, 16.0, 2.0),
            STONE_LT,
            bevel=0.4,
            taper=0.92,
        ),
        # 三块石趾
        part_box(
            "toe_l",
            (-5.2, 10.5, -28.0),
            (3.6, 5.0, 3.4),
            STONE_LT,
            bevel=0.4,
            taper=0.7,
        ),
        part_box(
            "toe_c", (0.0, 11.2, -28.0), (4.0, 5.6, 3.8), STONE_LT, bevel=0.4, taper=0.7
        ),
        part_box(
            "toe_r", (5.2, 10.5, -28.0), (3.6, 5.0, 3.4), STONE_LT, bevel=0.4, taper=0.7
        ),
        _slanted_box(
            "crack_thigh", (-5.0, 4.6, -9.0), (0.8, 1.0, 8.0), CRACK, 14.0, bevel=0.1
        ),
        part_box(
            "crack_shin",
            (3.8, 4.4, -22.0),
            (0.8, 1.0, 5.0),
            CRACK,
            bevel=0.1,
            rotation_z=math.radians(-10),
        ),
    ]
    return join_parts(parts, "boss_golem_part_leg")


def build_golem_part_weapon():
    """石锤: 粗石柄 + 巨型方形锤头 + 青铜束带/符文 + 柄尾石。"""
    parts = [
        part_cylinder("haft", (0, 0, -1), 2.8, 58, ROCK, vertices=10, bevel=0.3),
        part_sphere("pommel", (0, 0, 28.5), 3.4, STONE_DK, segments=12, rings=8),
        part_cylinder("haft_band_a", (0, 0, -20.0), 3.1, 1.4, BRONZE, vertices=12),
        part_cylinder("haft_band_b", (0, 0, -9.0), 3.1, 1.4, BRONZE_DK, vertices=12),
        part_cylinder("haft_band_c", (0, 0, 8.0), 3.1, 1.4, BRONZE, vertices=12),
        # 巨型锤头
        part_box(
            "hammer_core",
            (0, 0, 22.0),
            (20.0, 13.0, 13.0),
            STONE,
            bevel=1.0,
            taper=0.88,
        ),
        part_box(
            "hammer_top",
            (0, 0, 28.8),
            (21.0, 13.5, 2.8),
            STONE_LT,
            bevel=0.4,
            taper=0.9,
        ),
        part_box(
            "hammer_bottom",
            (0, 0, 15.2),
            (21.0, 13.5, 2.8),
            STONE_DK,
            bevel=0.4,
            taper=0.9,
        ),
        part_box(
            "hammer_side_l",
            (-11.0, 0, 22.0),
            (2.6, 11.0, 10.0),
            STONE_DK,
            bevel=0.4,
            taper=0.8,
        ),
        part_box(
            "hammer_side_r",
            (11.0, 0, 22.0),
            (2.6, 11.0, 10.0),
            STONE_DK,
            bevel=0.4,
            taper=0.8,
        ),
        # 青铜束带 + 中央符文
        part_box("hammer_band", (0, 7.2, 22.0), (6.0, 1.2, 13.5), BRONZE, bevel=0.3),
        part_sphere(
            "hammer_medallion",
            (0, 7.8, 22.0),
            2.6,
            BRONZE,
            segments=12,
            rings=8,
            scale=(1.0, 0.4, 1.0),
        ),
        part_sphere(
            "hammer_rune",
            (0, 8.4, 22.0),
            1.1,
            GLOW,
            segments=8,
            rings=4,
            scale=(1.0, 0.4, 1.0),
        ),
        # 锤头裂纹/缺角
        _slanted_box(
            "hammer_crack_l", (-8.0, 7.0, 23.0), (0.9, 1.0, 6.0), CRACK, 18.0, bevel=0.1
        ),
        _slanted_box(
            "hammer_crack_r", (8.0, 7.0, 20.0), (0.8, 1.0, 5.0), CRACK, -14.0, bevel=0.1
        ),
        part_box(
            "hammer_chip",
            (-9.5, 4.0, 27.0),
            (3.0, 3.0, 2.0),
            STONE_DK,
            bevel=0.2,
            rotation_z=math.radians(20),
        ),
        # 锤面符文 + 铜角 + 顶部碎石
        part_box("hammer_rune_l", (-5.0, 7.6, 23.0), (1.2, 0.8, 4.0), GLOW, bevel=0.1),
        part_box("hammer_rune_r", (5.0, 7.6, 21.0), (1.2, 0.8, 4.0), GLOW, bevel=0.1),
        part_box(
            "hammer_corner_l", (-8.0, 6.8, 28.0), (3.0, 1.4, 2.4), BRONZE, bevel=0.2
        ),
        part_box(
            "hammer_corner_r", (8.0, 6.8, 16.0), (3.0, 1.4, 2.4), BRONZE, bevel=0.2
        ),
        part_box(
            "hammer_top_spike_l",
            (-5.0, 0, 31.5),
            (2.0, 2.4, 4.0),
            STONE_DK,
            bevel=0.2,
            taper=0.15,
            rotation_z=math.radians(-14),
        ),
        part_box(
            "hammer_top_spike_r",
            (5.0, 0, 31.5),
            (2.0, 2.4, 4.0),
            STONE_DK,
            bevel=0.2,
            taper=0.15,
            rotation_z=math.radians(14),
        ),
        # 柄尾尖石
        part_box(
            "haft_tip", (0, 0, -31.0), (4.2, 4.2, 4.0), STONE_DK, bevel=0.3, taper=0.4
        ),
    ]
    # 扛肩 78°: 锤头偏到肩侧, 露出头部与胸口
    return _shift(_rotate_y(join_parts(parts, "boss_golem_part_weapon"), 78.0), dy=11.0)


GOLEM_PART_BUILDERS = {
    "head": build_golem_part_head,
    "torso": build_golem_part_torso,
    "arm": build_golem_part_arm,
    "leg": build_golem_part_leg,
    "weapon": build_golem_part_weapon,
}
