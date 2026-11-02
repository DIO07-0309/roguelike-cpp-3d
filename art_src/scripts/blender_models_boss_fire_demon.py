# A16-Boss: 火魔手建低模 (第二个 Boss)
# 局部空间: 原点 = 骨骼锚点, x=宽, y=深度(前=+Y), z=高
# 骨架 ppu 1.15, 与暗影骑士同级体型; arm/weapon 骨靠中 → 前移避免陷入躯干
# 设计: 熔岩恶魔 — 宽胸巨肩、焦黑岩甲 + 熔岩裂纹、牛角火冠、獠牙巨口、熔火巨刃
import math

from blender_lib import part_box, part_cylinder, part_sphere, join_parts
from blender_models_player import export_part_json

# ── 火魔调色板 (采样自 boss_fire_demon_part_*.png) ──
CHAR = (48, 16, 16)  # 焦黑岩甲
EMBER_DK = (112, 48, 16)  # 暗熔岩
ORANGE = (176, 80, 32)  # 主熔岩橙
ORANGE_LT = (224, 128, 48)  # 亮熔岩
FIRE = (224, 80, 32)  # 火焰红橙
GLOW = (255, 160, 40)  # 熔核/眼 (高饱和火橙)
BONE = (196, 168, 128)  # 獠牙/骨刺
TAN = (160, 132, 100)  # 2D 腰布米色
IRON = (96, 80, 64)  # 武器铁灰

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
    """绕 Y 轴倾斜的长方体 — 正面可见 x-z 斜度 (凶眼/火舌/斜甲)。"""
    obj = part_box(name, (0, 0, 0), size, color, bevel=bevel, taper=taper)
    angle = math.radians(slant_deg)
    cos_a, sin_a = math.cos(angle), math.sin(angle)
    for v in obj.data.vertices:
        x, z = v.co.x, v.co.z
        v.co.x = x * cos_a + z * sin_a + center[0]
        v.co.z = -x * sin_a + z * cos_a + center[2]
        v.co.y += center[1]
    return obj


def _fang(name, center, radius, depth, color, point_up=False):
    """獠牙: 收尖圆柱; 默认尖朝下。"""
    return part_cylinder(
        name,
        center,
        0.08 if not point_up else radius,
        depth,
        color,
        vertices=6,
        radius_top=radius if not point_up else 0.08,
        smooth=False,
    )


def build_fire_demon_part_head():
    """头: 焦黑头骨 + 熔岩面甲 + 斜亮眼 + 獠牙巨口 + 三段牛角 + 火焰冠。"""
    parts = [
        part_cylinder("neck", (0, 0, -8), 4.8, 9, CHAR, vertices=12),
        part_sphere(
            "skull",
            (0, 0, 3.5),
            6.5,
            ORANGE,
            segments=18,
            rings=11,
            scale=(1.04, 0.96, 1.0),
        ),
        part_box(
            "face", (0, 5.2, 1.0), (12.0, 3.2, 9.0), EMBER_DK, bevel=0.8, taper=0.88
        ),
        _slanted_box(
            "brow_l", (-3.0, 7.5, 4.2), (5.2, 1.4, 1.2), CHAR, 18.0, bevel=0.2
        ),
        _slanted_box(
            "brow_r", (3.0, 7.5, 4.2), (5.2, 1.4, 1.2), CHAR, -18.0, bevel=0.2
        ),
        _slanted_box(
            "eye_l", (-2.9, 8.0, 1.8), (4.4, 0.8, 0.72), GLOW, 12.0, bevel=0.16
        ),
        _slanted_box(
            "eye_r", (2.9, 8.0, 1.8), (4.4, 0.8, 0.72), GLOW, -12.0, bevel=0.16
        ),
        part_box(
            "snout", (0, 7.0, -2.0), (8.0, 3.4, 4.6), ORANGE, bevel=0.7, taper=0.82
        ),
        part_box("jaw", (0, 6.4, -5.2), (9.6, 4.4, 2.4), CHAR, bevel=0.5, taper=0.92),
        _fang("tooth_u0", (-3.1, 8.4, -5.4), 0.62, 2.2, BONE),
        _fang("tooth_u1", (-1.1, 8.5, -5.4), 0.56, 2.0, BONE),
        _fang("tooth_u2", (1.1, 8.5, -5.4), 0.56, 2.0, BONE),
        _fang("tooth_u3", (3.1, 8.4, -5.4), 0.62, 2.2, BONE),
        _fang("tooth_d0", (-2.1, 8.3, -6.3), 0.48, 1.5, BONE, point_up=True),
        _fang("tooth_d1", (0.0, 8.4, -6.3), 0.46, 1.4, BONE, point_up=True),
        _fang("tooth_d2", (2.1, 8.3, -6.3), 0.48, 1.5, BONE, point_up=True),
        part_box(
            "horn_l1",
            (-6.4, 0.5, 7.0),
            (4.2, 5.2, 11.0),
            EMBER_DK,
            bevel=0.5,
            taper=0.62,
            rotation_z=math.radians(24),
            rotation_x=math.radians(-10),
        ),
        part_box(
            "horn_r1",
            (6.4, 0.5, 7.0),
            (4.2, 5.2, 11.0),
            EMBER_DK,
            bevel=0.5,
            taper=0.62,
            rotation_z=math.radians(-24),
            rotation_x=math.radians(-10),
        ),
        part_box(
            "horn_l2",
            (-9.2, 0.0, 13.6),
            (3.2, 3.8, 7.5),
            ORANGE,
            bevel=0.4,
            taper=0.55,
            rotation_z=math.radians(34),
            rotation_x=math.radians(-14),
        ),
        part_box(
            "horn_r2",
            (9.2, 0.0, 13.6),
            (3.2, 3.8, 7.5),
            ORANGE,
            bevel=0.4,
            taper=0.55,
            rotation_z=math.radians(-34),
            rotation_x=math.radians(-14),
        ),
        part_box(
            "horn_l3",
            (-10.4, -0.4, 16.8),
            (2.8, 3.2, 6.0),
            ORANGE_LT,
            bevel=0.3,
            taper=0.28,
            rotation_z=math.radians(38),
            rotation_x=math.radians(-16),
        ),
        part_box(
            "horn_r3",
            (10.4, -0.4, 16.8),
            (2.8, 3.2, 6.0),
            ORANGE_LT,
            bevel=0.3,
            taper=0.28,
            rotation_z=math.radians(-38),
            rotation_x=math.radians(-16),
        ),
        # 三层火冠: 宽底收尖的整块火焰, 避免碎尖
        _slanted_box(
            "flame_c",
            (0, -0.8, 16.0),
            (6.0, 4.6, 14.0),
            ORANGE_LT,
            0.0,
            bevel=0.4,
            taper=0.22,
        ),
        _slanted_box(
            "flame_c_core",
            (0, 1.4, 15.2),
            (2.4, 1.0, 10.5),
            GLOW,
            0.0,
            bevel=0.2,
            taper=0.15,
        ),
        _slanted_box(
            "flame_l",
            (-5.6, -0.4, 13.6),
            (4.6, 3.8, 10.5),
            FIRE,
            18.0,
            bevel=0.3,
            taper=0.18,
        ),
        _slanted_box(
            "flame_r",
            (5.6, -0.4, 13.6),
            (4.6, 3.8, 10.5),
            FIRE,
            -18.0,
            bevel=0.3,
            taper=0.18,
        ),
        _slanted_box(
            "cheek_l",
            (-7.6, 3.0, -2.5),
            (2.8, 3.4, 6.0),
            BONE,
            -35.0,
            bevel=0.2,
            taper=0.32,
        ),
        _slanted_box(
            "cheek_r",
            (7.6, 3.0, -2.5),
            (2.8, 3.4, 6.0),
            BONE,
            35.0,
            bevel=0.2,
            taper=0.32,
        ),
        # 下颚长獠牙 (前伸, 不被胸甲遮挡)
        _fang("tusk_l", (-4.3, 8.8, -5.6), 0.72, 4.6, BONE, point_up=True),
        _fang("tusk_r", (4.3, 8.8, -5.6), 0.72, 4.6, BONE, point_up=True),
        part_box("gorget", (0, 0, -7.0), (13.5, 12.0, 3.2), EMBER_DK, bevel=0.6),
        part_box("gorget_core", (0, 6.2, -7.0), (7.0, 1.0, 1.6), GLOW, bevel=0.2),
    ]
    # 头部前探: 让下颚/獠牙越过胸甲前缘, 形成恶魔式压迫姿态
    return _shift(join_parts(parts, "boss_fire_demon_part_head"), dy=3.0)


def build_fire_demon_part_torso():
    """躯干: 宽胸窄腰 + 焦黑岩甲 + 熔核 + 熔岩裂纹 + 巨肩火山刺 + 米色腰布。"""
    parts = [
        part_box(
            "chest", (0, 0, 7), (32, 18, 17), ORANGE, bevel=1.6, segments=2, taper=0.62
        ),
        part_box(
            "chest_plate", (0, 8.0, 7), (20, 2.6, 12), ORANGE_LT, bevel=0.9, taper=0.76
        ),
        # 熔核 + 周围岩片
        part_sphere(
            "core",
            (4.0, 9.8, 6.0),
            3.4,
            GLOW,
            segments=14,
            rings=8,
            scale=(1.0, 0.45, 1.0),
        ),
        part_box(
            "core_rim_l",
            (-0.6, 9.3, 6.0),
            (2.4, 1.6, 7.0),
            ORANGE,
            bevel=0.3,
            taper=0.6,
            rotation_z=math.radians(-18),
        ),
        part_box(
            "core_rim_r",
            (8.6, 9.3, 6.0),
            (2.4, 1.6, 7.0),
            ORANGE,
            bevel=0.3,
            taper=0.6,
            rotation_z=math.radians(18),
        ),
        part_box(
            "core_rim_t",
            (4.0, 9.3, 10.4),
            (8.0, 1.6, 2.0),
            ORANGE,
            bevel=0.3,
            taper=0.7,
        ),
        part_box(
            "core_rim_b", (4.0, 9.3, 1.6), (7.0, 1.6, 2.0), ORANGE, bevel=0.3, taper=0.7
        ),
        # 熔岩裂纹
        part_box(
            "crack_l",
            (-8.6, 9.1, 3.0),
            (1.0, 1.0, 9.0),
            FIRE,
            bevel=0.15,
            rotation_z=math.radians(12),
        ),
        part_box(
            "crack_r",
            (8.6, 9.1, 2.0),
            (0.9, 1.0, 7.0),
            FIRE,
            bevel=0.15,
            rotation_z=math.radians(-9),
        ),
        # 窄腰 + 腹甲 + 腰带
        part_box("waist", (0, 0, -6), (16, 12, 10), EMBER_DK, bevel=0.9, taper=0.9),
        part_box(
            "ab_l",
            (-3.0, 6.5, -5.0),
            (5.4, 1.2, 2.2),
            ORANGE,
            bevel=0.3,
            rotation_z=math.radians(8),
        ),
        part_box(
            "ab_r",
            (3.0, 6.5, -5.0),
            (5.4, 1.2, 2.2),
            ORANGE,
            bevel=0.3,
            rotation_z=math.radians(-8),
        ),
        part_box("belt", (0, 0, -10.5), (19, 13.5, 3.4), CHAR, bevel=0.6),
        part_box("buckle", (0, 7.4, -10.5), (5.0, 1.4, 3.6), GLOW, bevel=0.4),
        # 2D 米色腰布 (前后两片)
        part_box(
            "loin_front", (0, 7.4, -14.5), (11.0, 1.8, 7.5), TAN, bevel=0.5, taper=0.88
        ),
        part_box(
            "loin_back", (0, -7.0, -14.5), (10.0, 1.6, 6.5), TAN, bevel=0.5, taper=0.9
        ),
        # 巨肩: 两层火山岩甲 + 外张火刺
        part_box(
            "pauldron_l",
            (-18.0, 0, 8.5),
            (12.5, 16.0, 12.5),
            CHAR,
            bevel=1.2,
            taper=0.5,
            rotation_z=math.radians(22),
        ),
        part_box(
            "pauldron_r",
            (18.0, 0, 8.5),
            (12.5, 16.0, 12.5),
            CHAR,
            bevel=1.2,
            taper=0.5,
            rotation_z=math.radians(-22),
        ),
        part_box(
            "pauldron2_l",
            (-16.5, 0, 2.8),
            (10.0, 12.5, 6.5),
            EMBER_DK,
            bevel=0.9,
            taper=0.6,
            rotation_z=math.radians(22),
        ),
        part_box(
            "pauldron2_r",
            (16.5, 0, 2.8),
            (10.0, 12.5, 6.5),
            EMBER_DK,
            bevel=0.9,
            taper=0.6,
            rotation_z=math.radians(-22),
        ),
        part_box(
            "spike_l1",
            (-20.5, 0, 15.0),
            (4.4, 7.0, 11.0),
            ORANGE,
            bevel=0.4,
            taper=0.25,
            rotation_z=math.radians(30),
        ),
        part_box(
            "spike_r1",
            (20.5, 0, 15.0),
            (4.4, 7.0, 11.0),
            ORANGE,
            bevel=0.4,
            taper=0.25,
            rotation_z=math.radians(-30),
        ),
        part_box(
            "spike_l2",
            (-23.0, 0, 8.0),
            (3.0, 4.5, 7.0),
            FIRE,
            bevel=0.3,
            taper=0.2,
            rotation_z=math.radians(58),
        ),
        part_box(
            "spike_r2",
            (23.0, 0, 8.0),
            (3.0, 4.5, 7.0),
            FIRE,
            bevel=0.3,
            taper=0.2,
            rotation_z=math.radians(-58),
        ),
        # 背部火山刺 (侧后剪影)
        part_box(
            "back_spike_l",
            (-8.0, -7.5, 14.0),
            (3.4, 3.4, 9.0),
            EMBER_DK,
            bevel=0.3,
            taper=0.2,
            rotation_z=math.radians(-18),
            rotation_x=math.radians(22),
        ),
        part_box(
            "back_spike_r",
            (8.0, -7.5, 14.0),
            (3.4, 3.4, 9.0),
            EMBER_DK,
            bevel=0.3,
            taper=0.2,
            rotation_z=math.radians(18),
            rotation_x=math.radians(22),
        ),
        # 肩部余烬碎晶 + 胸甲熔岩脉
        _slanted_box(
            "shoulder_ember_l1",
            (-21.0, 3.0, 19.0),
            (2.4, 2.4, 5.0),
            GLOW,
            -28.0,
            bevel=0.15,
            taper=0.15,
        ),
        _slanted_box(
            "shoulder_ember_l2",
            (-17.0, 2.0, 22.0),
            (1.8, 2.0, 4.0),
            FIRE,
            -14.0,
            bevel=0.15,
            taper=0.15,
        ),
        _slanted_box(
            "shoulder_ember_r1",
            (21.0, 3.0, 19.0),
            (2.4, 2.4, 5.0),
            GLOW,
            28.0,
            bevel=0.15,
            taper=0.15,
        ),
        _slanted_box(
            "shoulder_ember_r2",
            (17.0, 2.0, 22.0),
            (1.8, 2.0, 4.0),
            FIRE,
            14.0,
            bevel=0.15,
            taper=0.15,
        ),
        _slanted_box(
            "chest_vein_l", (-6.0, 9.4, 5.0), (0.9, 0.8, 7.0), FIRE, -12.0, bevel=0.1
        ),
        _slanted_box(
            "chest_vein_r", (6.0, 9.4, 2.0), (0.8, 0.8, 5.5), ORANGE_LT, 10.0, bevel=0.1
        ),
        part_sphere("belt_ember_l", (-6.0, 7.6, -10.5), 0.7, GLOW, segments=8, rings=4),
        part_sphere("belt_ember_r", (6.0, 7.6, -10.5), 0.7, GLOW, segments=8, rings=4),
    ]
    return join_parts(parts, "boss_fire_demon_part_torso")


def build_fire_demon_part_arm():
    """臂: 粗肩球 + 熔岩上臂 + 肘刺 + 前臂 + 金腕环 + 三爪巨手。"""
    parts = [
        part_sphere("shoulder", (0, 0, -2), 4.4, CHAR, segments=14, rings=8),
        part_cylinder(
            "upper",
            (0, 0, -9),
            4.0,
            13,
            EMBER_DK,
            vertices=12,
            radius_top=4.5,
            smooth=True,
            bevel=0.4,
        ),
        part_box("upper_crack", (0, 4.2, -9), (1.0, 1.0, 8.0), FIRE, bevel=0.15),
        part_sphere("elbow", (0, 0, -16), 4.1, CHAR, segments=12, rings=8),
        part_box(
            "elbow_spike",
            (0, -3.8, -15.5),
            (2.4, 2.4, 5.5),
            ORANGE,
            bevel=0.3,
            taper=0.2,
            rotation_x=math.radians(90),
        ),
        part_cylinder(
            "forearm",
            (0, 0, -21),
            3.6,
            10,
            CHAR,
            vertices=12,
            radius_top=4.0,
            smooth=True,
            bevel=0.4,
        ),
        part_cylinder("fore_ring_a", (0, 0, -19.0), 4.1, 1.2, ORANGE, vertices=12),
        part_cylinder("fore_ring_b", (0, 0, -23.2), 3.9, 1.2, FIRE, vertices=12),
        part_cylinder(
            "wrist", (0, 0, -25.5), 3.8, 2.4, EMBER_DK, vertices=12, bevel=0.3
        ),
        part_sphere(
            "palm",
            (0, 0, -29.0),
            3.8,
            CHAR,
            segments=12,
            rings=8,
            scale=(1.05, 1.0, 1.05),
        ),
        # 三根前伸利爪
        _fang("claw_a", (-2.3, 1.2, -32.5), 0.75, 5.0, BONE),
        _fang("claw_b", (0.0, 1.6, -33.2), 0.82, 5.8, BONE),
        _fang("claw_c", (2.3, 1.2, -32.5), 0.75, 5.0, BONE),
        part_sphere("knuckle", (0, 3.0, -28.8), 0.75, GLOW, segments=8, rings=4),
    ]
    # 前移: 骨骼 arm 骨靠中, 陷进躯干; 前移显于胸前
    return _shift(join_parts(parts, "boss_fire_demon_part_arm"), dy=5.0)


def build_fire_demon_part_leg():
    """腿: 粗大腿 + 熔岩甲片 + 膝刺 + 小腿 + 三趾巨足。"""
    parts = [
        part_cylinder(
            "thigh",
            (0, 0, -9),
            6.0,
            15,
            EMBER_DK,
            vertices=12,
            radius_top=5.2,
            smooth=True,
            bevel=0.5,
        ),
        part_box(
            "thigh_plate", (0, 5.4, -9), (10.0, 2.2, 12), CHAR, bevel=0.7, taper=0.86
        ),
        part_box("thigh_crack", (0, 6.6, -9), (1.0, 0.8, 8.5), FIRE, bevel=0.15),
        part_sphere(
            "knee",
            (0, 1.0, -17),
            4.8,
            CHAR,
            segments=12,
            rings=8,
            scale=(1.0, 1.0, 0.85),
        ),
        part_box(
            "knee_spike",
            (0, -4.2, -16.5),
            (2.5, 2.5, 6.0),
            ORANGE,
            bevel=0.3,
            taper=0.2,
            rotation_x=math.radians(90),
        ),
        part_cylinder(
            "shin",
            (0, 0, -23),
            4.8,
            10,
            CHAR,
            vertices=12,
            radius_top=4.0,
            smooth=True,
            bevel=0.5,
        ),
        part_cylinder("shin_band", (0, 0, -21.0), 4.9, 1.2, FIRE, vertices=12),
        part_box("foot", (0, 2.6, -27.5), (13.5, 16, 6), CHAR, bevel=0.8, taper=0.9),
        part_box("foot_trim", (0, 2.6, -24.5), (13.5, 16, 1.4), EMBER_DK, bevel=0.3),
        # 三趾利爪
        _fang("toe_l", (-4.4, 10.0, -28.0), 1.0, 5.0, BONE),
        _fang("toe_c", (0.0, 10.6, -28.0), 1.1, 5.8, BONE),
        _fang("toe_r", (4.4, 10.0, -28.0), 1.0, 5.0, BONE),
    ]
    return join_parts(parts, "boss_fire_demon_part_leg")


def build_fire_demon_part_weapon():
    """熔火长柄战斧: 焦黑长柄 + 高位单刃巨斧 + 火心宝石 + 柄尾尖。"""
    parts = [
        part_cylinder("haft", (0, 0, -1), 2.1, 58, CHAR, vertices=10, bevel=0.3),
        part_sphere("pommel", (0, 0, 28.5), 2.7, ORANGE),
        part_cylinder("band_a", (0, 0, -20.0), 2.4, 1.2, ORANGE, vertices=12),
        part_cylinder("band_b", (0, 0, -10.0), 2.4, 1.2, FIRE, vertices=12),
        part_cylinder("band_c", (0, 0, 8.0), 2.4, 1.2, ORANGE, vertices=12),
        part_cylinder("band_d", (0, 0, 20.0), 2.4, 1.2, FIRE, vertices=12),
        # 高位单刃巨斧 (定剪影, 不遮胸口)
        part_box(
            "axe_core", (0, 0, 26.0), (7.0, 4.4, 9.0), EMBER_DK, bevel=0.6, taper=0.85
        ),
        part_box(
            "axe_l",
            (-12.5, 0, 26.0),
            (16.0, 3.8, 13.0),
            ORANGE,
            bevel=0.5,
            taper=0.32,
            rotation_z=math.radians(-3),
        ),
        part_box(
            "axe_r",
            (12.5, 0, 26.0),
            (16.0, 3.8, 13.0),
            ORANGE,
            bevel=0.5,
            taper=0.32,
            rotation_z=math.radians(3),
        ),
        part_box(
            "edge_l", (-21.0, 0, 26.0), (2.6, 4.2, 10.5), GLOW, bevel=0.2, taper=0.28
        ),
        part_box(
            "edge_r", (21.0, 0, 26.0), (2.6, 4.2, 10.5), GLOW, bevel=0.2, taper=0.28
        ),
        part_box("axe_rune_l", (-12.5, 2.2, 26.0), (1.2, 0.8, 9.0), CHAR, bevel=0.15),
        part_box("axe_rune_r", (12.5, 2.2, 26.0), (1.2, 0.8, 9.0), CHAR, bevel=0.15),
        part_sphere(
            "axe_gem",
            (0, 3.2, 26.0),
            1.8,
            GLOW,
            segments=12,
            rings=8,
            scale=(1.0, 0.5, 1.0),
        ),
        _slanted_box(
            "axe_spike_l",
            (-9.5, 0, 34.0),
            (2.4, 2.8, 7.0),
            FIRE,
            35.0,
            bevel=0.2,
            taper=0.15,
        ),
        _slanted_box(
            "axe_spike_r",
            (9.5, 0, 34.0),
            (2.4, 2.8, 7.0),
            FIRE,
            -35.0,
            bevel=0.2,
            taper=0.15,
        ),
        _slanted_box(
            "axe_top",
            (0, 0, 35.0),
            (2.6, 3.0, 8.0),
            ORANGE_LT,
            0.0,
            bevel=0.2,
            taper=0.15,
        ),
        # 斧刃锯齿 + 柄身火符
        _slanted_box(
            "axe_tooth_l1",
            (-19.5, 0, 30.0),
            (2.0, 2.4, 4.0),
            FIRE,
            -52.0,
            bevel=0.15,
            taper=0.12,
        ),
        _slanted_box(
            "axe_tooth_l2",
            (-19.5, 0, 22.0),
            (2.0, 2.4, 4.0),
            FIRE,
            -38.0,
            bevel=0.15,
            taper=0.12,
        ),
        _slanted_box(
            "axe_tooth_r1",
            (19.5, 0, 30.0),
            (2.0, 2.4, 4.0),
            FIRE,
            52.0,
            bevel=0.15,
            taper=0.12,
        ),
        _slanted_box(
            "axe_tooth_r2",
            (19.5, 0, 22.0),
            (2.0, 2.4, 4.0),
            FIRE,
            38.0,
            bevel=0.15,
            taper=0.12,
        ),
        part_box("haft_rune_a", (0, 1.8, -8.0), (1.0, 0.8, 3.0), GLOW, bevel=0.1),
        part_box("haft_rune_b", (0, 1.8, -18.0), (1.0, 0.8, 3.0), GLOW, bevel=0.1),
        _fang("haft_spike", (0, 0, -31.0), 1.6, 5.0, ORANGE),
    ]
    return _shift(join_parts(parts, "boss_fire_demon_part_weapon"), dy=11.0)


FIRE_DEMON_PART_BUILDERS = {
    "head": build_fire_demon_part_head,
    "torso": build_fire_demon_part_torso,
    "arm": build_fire_demon_part_arm,
    "leg": build_fire_demon_part_leg,
    "weapon": build_fire_demon_part_weapon,
}
