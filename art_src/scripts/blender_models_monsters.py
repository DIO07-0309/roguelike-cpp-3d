# A17-Monster: 30 种普通怪物手建低模 (参数化批量)
# 局部空间: 原点 = 骨骼锚点, x=宽, y=深度(前=+Y 朝相机), z=高
# 30 种怪物共用同一骨架: ppu 0.8, anchor [24,62], 5 槽位 head/torso/arm/leg/weapon
#   骨骼世界 z: hips=20 torso=26 head=37 arm=35 weapon=30
# 约定与六 Boss 一致: 直接 part_* 局部坐标, 不 flip_y (export_part_json 做 mesh=(x,-y,z))
# 第一版: 4 个形态族模板 + 各自 2D 配色; 二次精修再逐族细化特征
import math
import os

from blender_lib import part_box, part_cylinder, part_sphere, join_parts
from blender_models_player import export_part_json

# ── 30 种怪物配置: family + 配色 (采样自 mon_*_part_*.png) ──
# 每项: (family, dark, main, light, shadow, trim, eye)
DARK = (48, 24, 48)  # 通用暗轮廓
BONE = (200, 190, 150)  # 骨/牙
IRON = (150, 160, 175)  # 铁/银
GOLD = (180, 140, 80)  # 金

MONSTERS = {
    # ── humanoid 类人 (19) ──
    "orc": (
        "humanoid",
        DARK,
        (120, 168, 72),
        (168, 200, 96),
        (72, 104, 40),
        BONE,
        (220, 80, 60),
    ),
    "archer": (
        "humanoid",
        DARK,
        (144, 120, 72),
        (192, 168, 104),
        (96, 80, 48),
        BONE,
        (230, 200, 90),
    ),
    "goblin_hunter": (
        "humanoid",
        DARK,
        (144, 120, 72),
        (192, 168, 104),
        (96, 80, 48),
        BONE,
        (230, 200, 90),
    ),
    "shaman": (
        "humanoid",
        DARK,
        (144, 120, 72),
        (192, 168, 104),
        (96, 80, 48),
        (180, 120, 240),
        (180, 120, 240),
    ),
    "bone_soldier": (
        "humanoid",
        DARK,
        (192, 192, 168),
        (224, 224, 200),
        (140, 140, 116),
        (150, 110, 70),
        (60, 40, 40),
    ),
    "skeleton_archer": (
        "humanoid",
        DARK,
        (192, 192, 168),
        (224, 224, 200),
        (140, 140, 116),
        (150, 110, 70),
        (60, 40, 40),
    ),
    "dark_mage": (
        "humanoid",
        DARK,
        (72, 48, 120),
        (112, 88, 168),
        (48, 32, 80),
        BONE,
        (140, 255, 176),
    ),
    "shadow_stalker": (
        "humanoid",
        DARK,
        (72, 48, 96),
        (112, 80, 144),
        (48, 32, 72),
        BONE,
        (220, 70, 90),
    ),
    "shadow_assassin": (
        "humanoid",
        DARK,
        (48, 48, 72),
        (80, 80, 112),
        (24, 24, 40),
        IRON,
        (220, 70, 90),
    ),
    "night_stalker": (
        "humanoid",
        DARK,
        (120, 96, 48),
        (168, 136, 80),
        (72, 56, 24),
        BONE,
        (230, 200, 90),
    ),
    "blood_priest": (
        "humanoid",
        DARK,
        (144, 24, 48),
        (192, 64, 88),
        (96, 16, 32),
        GOLD,
        (255, 90, 110),
    ),
    "summoner": (
        "humanoid",
        DARK,
        (168, 120, 48),
        (208, 160, 88),
        (120, 80, 32),
        GOLD,
        (180, 120, 240),
    ),
    "necromancer": (
        "humanoid",
        DARK,
        (72, 120, 72),
        (112, 160, 112),
        (48, 80, 48),
        BONE,
        (140, 255, 176),
    ),
    "tank": (
        "humanoid",
        DARK,
        (96, 144, 168),
        (144, 184, 208),
        (64, 96, 120),
        IRON,
        (120, 200, 255),
    ),
    "charger": (
        "humanoid",
        DARK,
        (192, 72, 48),
        (232, 112, 72),
        (120, 40, 24),
        BONE,
        (240, 200, 80),
    ),
    "bomber": (
        "humanoid",
        DARK,
        (192, 96, 48),
        (232, 144, 80),
        (120, 56, 24),
        IRON,
        (255, 220, 90),
    ),
    "elite_orc": (
        "humanoid",
        DARK,
        (120, 72, 72),
        (168, 104, 104),
        (72, 40, 40),
        GOLD,
        (255, 90, 80),
    ),
    "ice_warden": (
        "humanoid",
        DARK,
        (120, 168, 192),
        (176, 216, 232),
        (72, 112, 144),
        IRON,
        (220, 240, 255),
    ),
    "iron_sentinel": (
        "humanoid",
        DARK,
        (72, 96, 120),
        (120, 152, 176),
        (48, 64, 88),
        IRON,
        (140, 220, 255),
    ),
    # ── orb 浮灵/元素 (4) ──
    "fire_imp": (
        "orb",
        DARK,
        (216, 96, 48),
        (240, 168, 96),
        (168, 56, 24),
        GOLD,
        (255, 230, 120),
    ),
    "lightning_orb": (
        "orb",
        DARK,
        (240, 216, 96),
        (240, 240, 168),
        (192, 160, 48),
        GOLD,
        (255, 255, 200),
    ),
    "void_walker": (
        "orb",
        DARK,
        (96, 48, 144),
        (168, 120, 216),
        (64, 32, 96),
        GOLD,
        (210, 160, 255),
    ),
    "storm_elemental": (
        "orb",
        DARK,
        (120, 120, 192),
        (192, 192, 240),
        (72, 72, 144),
        IRON,
        (230, 235, 255),
    ),
    # ── slime 软体/虫 (5) ──
    "slime": (
        "slime",
        DARK,
        (120, 168, 96),
        (168, 208, 128),
        (72, 112, 56),
        BONE,
        (40, 70, 40),
    ),
    "frost_slime": (
        "slime",
        DARK,
        (96, 144, 192),
        (144, 192, 224),
        (48, 96, 144),
        IRON,
        (30, 60, 90),
    ),
    "elite_slime": (
        "slime",
        DARK,
        (120, 96, 144),
        (168, 144, 192),
        (72, 48, 96),
        GOLD,
        (50, 30, 70),
    ),
    "blood_leech": (
        "slime",
        DARK,
        (168, 48, 72),
        (208, 88, 112),
        (120, 24, 48),
        BONE,
        (60, 10, 20),
    ),
    "poison_wyrm": (
        "slime",
        DARK,
        (72, 120, 48),
        (112, 168, 72),
        (48, 72, 24),
        BONE,
        (180, 255, 90),
    ),
    # ── construct 构造/重甲 (2) ──
    "golem": (
        "construct",
        DARK,
        (120, 96, 72),
        (144, 144, 120),
        (72, 56, 40),
        IRON,
        (255, 176, 64),
    ),
    "stone_guardian": (
        "construct",
        DARK,
        (96, 96, 96),
        (120, 144, 120),
        (64, 64, 64),
        IRON,
        (140, 255, 176),
    ),
}

REPO = r"C:/Demo/roguelike_cpp"
PPU = 0.8
SLOTS = ["head", "torso", "arm", "leg", "weapon"]


def _shift(obj, dx=0.0, dy=0.0, dz=0.0):
    for v in obj.data.vertices:
        v.co.x += dx
        v.co.y += dy
        v.co.z += dz
    return obj


def _slanted_box(name, center, size, color, slant_deg, bevel=0.0, taper=1.0):
    """绕 Y 轴倾斜的长方体 (正面可见 x-z 斜度)。"""
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
    """收尖圆柱 (獠牙/爪/刺)。"""
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


def _color(name):
    family, dark, main, light, shadow, trim, eye = MONSTERS[name]
    return dict(dark=dark, main=main, light=light, shadow=shadow, trim=trim, eye=eye)


# ══════════════════════════════════════════════════════════════
# 形态族模板: 每族 build_<slot>(name) -> obj (局部坐标, 原点=骨骼锚点)
# 尺度: 怪物约为 Boss 的 0.7 倍 (部件贴图 22x20/28x20 vs Boss 30x26/40x28)
# ══════════════════════════════════════════════════════════════


def _h_head(name, c):
    """类人: 头骨 + 面 + 眼 + 角/耳/兜帽特征。"""
    parts = [
        part_cylinder("neck", (0, 0, -5), 2.6, 5, c["shadow"], vertices=10),
        part_sphere(
            "skull",
            (0, 0, 2.5),
            5.0,
            c["main"],
            segments=16,
            rings=10,
            scale=(1.0, 0.95, 1.05),
        ),
        part_box(
            "face", (0, 4.0, 0.5), (7.0, 2.2, 6.5), c["light"], bevel=0.6, taper=0.9
        ),
        _slanted_box(
            "eye_l", (-1.8, 5.3, 1.6), (2.2, 0.6, 0.7), c["eye"], 10.0, bevel=0.12
        ),
        _slanted_box(
            "eye_r", (1.8, 5.3, 1.6), (2.2, 0.6, 0.7), c["eye"], -10.0, bevel=0.12
        ),
        part_box("brow", (0, 5.0, 3.6), (7.0, 1.2, 1.2), c["shadow"], bevel=0.3),
        part_box(
            "jaw", (0, 4.2, -3.0), (5.6, 2.4, 2.2), c["shadow"], bevel=0.4, taper=0.85
        ),
        # 双耳/短角
        _slanted_box(
            "horn_l",
            (-4.6, 0, 5.0),
            (1.6, 2.0, 4.0),
            c["trim"],
            -20.0,
            bevel=0.2,
            taper=0.25,
        ),
        _slanted_box(
            "horn_r",
            (4.6, 0, 5.0),
            (1.6, 2.0, 4.0),
            c["trim"],
            20.0,
            bevel=0.2,
            taper=0.25,
        ),
    ]
    return join_parts(parts, "mon_%s_part_head" % name)


def _h_torso(name, c):
    """类人: 胸甲 + 腰 + 肩甲 + 胸饰。"""
    parts = [
        part_box(
            "chest",
            (0, 0, 4),
            (18, 11, 13),
            c["main"],
            bevel=1.0,
            segments=2,
            taper=0.7,
        ),
        part_box(
            "chest_plate", (0, 5.4, 4), (12, 1.6, 9), c["light"], bevel=0.6, taper=0.82
        ),
        part_box("waist", (0, 0, -5), (11, 8, 7), c["shadow"], bevel=0.6, taper=0.92),
        part_box("belt", (0, 0, -7.5), (13, 8.5, 2.2), c["trim"], bevel=0.4),
        part_box("buckle", (0, 4.6, -7.5), (2.6, 1.0, 2.4), c["light"], bevel=0.3),
        part_box(
            "pauldron_l",
            (-10.0, 0, 6),
            (6.5, 9, 6.5),
            c["main"],
            bevel=0.8,
            taper=0.55,
            rotation_z=math.radians(18),
        ),
        part_box(
            "pauldron_r",
            (10.0, 0, 6),
            (6.5, 9, 6.5),
            c["main"],
            bevel=0.8,
            taper=0.55,
            rotation_z=math.radians(-18),
        ),
        part_sphere(
            "gem",
            (0, 6.0, 5.5),
            1.6,
            c["eye"],
            segments=10,
            rings=6,
            scale=(1.0, 0.5, 1.0),
        ),
        part_box("hem", (0, 0, -9.5), (12, 8.5, 2.4), c["shadow"], bevel=0.5),
    ]
    return join_parts(parts, "mon_%s_part_torso" % name)


def _h_arm(name, c):
    """类人: 肩球 + 上臂 + 肘 + 前臂 + 腕环 + 手。"""
    parts = [
        part_sphere("shoulder", (0, 0, -1.5), 2.8, c["main"], segments=12, rings=8),
        part_cylinder(
            "upper",
            (0, 0, -6),
            2.4,
            9,
            c["main"],
            vertices=10,
            radius_top=2.6,
            smooth=True,
            bevel=0.3,
        ),
        part_sphere("elbow", (0, 0, -10.5), 2.2, c["shadow"], segments=10, rings=6),
        part_cylinder(
            "forearm",
            (0, 0, -14),
            2.0,
            8,
            c["light"],
            vertices=10,
            radius_top=2.2,
            smooth=True,
            bevel=0.3,
        ),
        part_cylinder(
            "cuff", (0, 0, -17.5), 2.3, 1.6, c["trim"], vertices=10, bevel=0.2
        ),
        part_sphere("hand", (0, 0, -19.5), 2.1, c["main"], segments=10, rings=6),
        _fang("claw_a", (-1.0, 0.8, -21.2), 0.3, 2.4, c["trim"]),
        _fang("claw_b", (1.0, 0.8, -21.2), 0.3, 2.4, c["trim"]),
    ]
    # arm 骨靠中 (x=-3/4) → 前移显于胸前
    return _shift(join_parts(parts, "mon_%s_part_arm" % name), dy=4.0)


def _h_leg(name, c):
    """类人: 大腿 + 膝 + 小腿 + 靴。"""
    parts = [
        part_cylinder(
            "thigh",
            (0, 0, -6),
            3.4,
            10,
            c["main"],
            vertices=10,
            radius_top=3.0,
            smooth=True,
            bevel=0.4,
        ),
        part_sphere(
            "knee",
            (0, 0.6, -11),
            2.6,
            c["light"],
            segments=10,
            rings=6,
            scale=(1.0, 1.0, 0.85),
        ),
        part_cylinder(
            "shin",
            (0, 0, -15),
            2.8,
            8,
            c["shadow"],
            vertices=10,
            radius_top=2.4,
            smooth=True,
            bevel=0.4,
        ),
        part_box("boot", (0, 1.6, -18.5), (6.5, 9, 4), c["shadow"], bevel=0.5),
        part_box("boot_toe", (0, 5.2, -19), (4.0, 3.0, 2.2), c["dark"], bevel=0.3),
        part_box("boot_trim", (0, 1.6, -16.5), (6.5, 9, 1.0), c["trim"], bevel=0.2),
    ]
    return join_parts(parts, "mon_%s_part_leg" % name)


def _h_weapon(name, c):
    """类人: 通用长柄武器 (剑/杖由配色区分)。"""
    parts = [
        part_cylinder("shaft", (0, 0, -8), 1.0, 22, c["shadow"], vertices=8, bevel=0.2),
        part_sphere("pommel", (0, 0, 3.0), 1.6, c["trim"]),
        part_box("guard", (0, 0, -6), (8, 2.0, 1.8), c["trim"], bevel=0.3),
        part_cylinder(
            "blade",
            (0, 0, -14),
            0.2,
            12,
            c["light"],
            vertices=4,
            radius_top=2.2,
            smooth=False,
            bevel=0.1,
        ),
        part_box("fuller", (0, 1.8, -14), (0.5, 0.4, 10), c["main"], bevel=0.05),
        part_sphere("gem", (0, 1.2, -6), 0.8, c["eye"], segments=8, rings=4),
        _fang("tip", (0, 0, -20.5), 0.6, 2.0, c["light"]),
    ]
    return _shift(join_parts(parts, "mon_%s_part_weapon" % name), dy=6.0)


HUMANOID = {
    "head": _h_head,
    "torso": _h_torso,
    "arm": _h_arm,
    "leg": _h_leg,
    "weapon": _h_weapon,
}


# ── orb 浮灵/元素: 球形核心 + 光环 + 悬浮碎晶; 无实体腿 ──
def _o_head(name, c):
    parts = [
        part_sphere("core", (0, 0, 1), 4.2, c["main"], segments=16, rings=10),
        part_sphere(
            "core_glow",
            (0, 3.0, 1),
            2.4,
            c["light"],
            segments=12,
            rings=8,
            scale=(1.0, 0.5, 1.0),
        ),
        _slanted_box(
            "eye_l", (-1.6, 4.0, 1.6), (1.8, 0.5, 0.6), c["eye"], 8.0, bevel=0.1
        ),
        _slanted_box(
            "eye_r", (1.6, 4.0, 1.6), (1.8, 0.5, 0.6), c["eye"], -8.0, bevel=0.1
        ),
        _slanted_box(
            "shard_l",
            (-4.6, 0, 4.0),
            (1.6, 1.6, 4.0),
            c["light"],
            -24.0,
            bevel=0.15,
            taper=0.15,
        ),
        _slanted_box(
            "shard_r",
            (4.6, 0, 4.0),
            (1.6, 1.6, 4.0),
            c["light"],
            24.0,
            bevel=0.15,
            taper=0.15,
        ),
    ]
    return join_parts(parts, "mon_%s_part_head" % name)


def _o_torso(name, c):
    parts = [
        part_sphere("orb", (0, 0, 3), 7.0, c["main"], segments=18, rings=12),
        part_sphere(
            "orb_inner",
            (0, 3.5, 3),
            4.4,
            c["light"],
            segments=14,
            rings=10,
            scale=(1.0, 0.55, 1.0),
        ),
        part_box("band", (0, 0, 3), (13.5, 13.5, 1.6), c["trim"], bevel=0.5),
        part_sphere(
            "core",
            (0, 7.0, 3),
            2.0,
            c["eye"],
            segments=10,
            rings=6,
            scale=(1.0, 0.5, 1.0),
        ),
        _slanted_box(
            "spike_t",
            (0, 0, 10.0),
            (2.0, 2.0, 5.0),
            c["light"],
            0.0,
            bevel=0.2,
            taper=0.15,
        ),
        _slanted_box(
            "spike_l",
            (-7.0, 0, 3.0),
            (3.5, 2.0, 2.0),
            c["light"],
            -18.0,
            bevel=0.2,
            taper=0.2,
        ),
        _slanted_box(
            "spike_r",
            (7.0, 0, 3.0),
            (3.5, 2.0, 2.0),
            c["light"],
            18.0,
            bevel=0.2,
            taper=0.2,
        ),
    ]
    return join_parts(parts, "mon_%s_part_torso" % name)


def _o_arm(name, c):
    parts = [
        part_sphere("shoulder", (0, 0, -1), 2.2, c["main"], segments=12, rings=8),
        part_cylinder(
            "beam",
            (0, 0, -7),
            1.4,
            10,
            c["light"],
            vertices=8,
            radius_top=1.0,
            smooth=True,
            bevel=0.2,
        ),
        _fang("tip", (0, 0, -13.5), 1.2, 4.0, c["eye"]),
        part_sphere("joint", (0, 0, -11), 1.4, c["trim"], segments=8, rings=4),
    ]
    return _shift(join_parts(parts, "mon_%s_part_arm" % name), dy=4.0)


def _o_leg(name, c):
    parts = [
        part_sphere(
            "drop",
            (0, 0, -6),
            3.0,
            c["main"],
            segments=12,
            rings=8,
            scale=(1.0, 1.0, 1.3),
        ),
        _fang("tail", (0, 0, -11), 1.8, 6.0, c["light"]),
        part_sphere("spark", (0, 2.0, -6), 1.0, c["eye"], segments=8, rings=4),
    ]
    return join_parts(parts, "mon_%s_part_leg" % name)


def _o_weapon(name, c):
    parts = [
        part_cylinder("rod", (0, 0, -6), 0.8, 20, c["shadow"], vertices=8, bevel=0.15),
        part_sphere("top", (0, 0, 5.0), 2.4, c["eye"], segments=12, rings=8),
        _slanted_box(
            "wing_l",
            (-2.8, 0, 4.0),
            (1.4, 1.4, 5.0),
            c["light"],
            -22.0,
            bevel=0.15,
            taper=0.15,
        ),
        _slanted_box(
            "wing_r",
            (2.8, 0, 4.0),
            (1.4, 1.4, 5.0),
            c["light"],
            22.0,
            bevel=0.15,
            taper=0.15,
        ),
        part_sphere("pommel", (0, 0, -16), 1.4, c["trim"]),
    ]
    return _shift(join_parts(parts, "mon_%s_part_weapon" % name), dy=6.0)


ORB = {
    "head": _o_head,
    "torso": _o_torso,
    "arm": _o_arm,
    "leg": _o_leg,
    "weapon": _o_weapon,
}


# ── slime 软体/虫: 球核 + 下垂裙边 + 退化肢 ──
def _s_head(name, c):
    """软体头: 贴在主球上部的眼点 + 顶部高光 (非独立球, 避免雪人)。"""
    parts = [
        part_sphere(
            "eye_l",
            (-2.0, 5.6, 0.5),
            0.95,
            c["eye"],
            segments=10,
            rings=6,
            scale=(1.0, 0.55, 1.0),
        ),
        part_sphere(
            "eye_r",
            (2.0, 5.6, 0.5),
            0.95,
            c["eye"],
            segments=10,
            rings=6,
            scale=(1.0, 0.55, 1.0),
        ),
        part_sphere("glint_l", (-2.3, 6.3, 0.8), 0.28, c["light"], segments=8, rings=4),
        part_sphere("glint_r", (1.7, 6.3, 0.8), 0.28, c["light"], segments=8, rings=4),
        part_sphere(
            "crown",
            (0, 0.5, 3.0),
            3.0,
            c["light"],
            segments=12,
            rings=8,
            scale=(1.0, 1.0, 0.4),
        ),
    ]
    return join_parts(parts, "mon_%s_part_head" % name)


def _s_torso(name, c):
    """软体身: 纵向拉长主凝胶球 (覆盖 head~leg 全部锚点) + 内核 + 底裙。"""
    parts = [
        part_sphere(
            "gel",
            (0, 0, 4),
            8.0,
            c["main"],
            segments=20,
            rings=14,
            scale=(1.0, 0.92, 1.55),
        ),
        part_sphere(
            "core",
            (0, 5.0, 2.0),
            3.4,
            c["light"],
            segments=12,
            rings=8,
            scale=(1.0, 0.5, 1.0),
        ),
        part_sphere(
            "nucleus",
            (0, 6.0, 2.0),
            1.7,
            c["eye"],
            segments=10,
            rings=6,
            scale=(1.0, 0.5, 1.0),
        ),
        part_box(
            "skirt", (0, 0, -6.0), (13, 10, 4), c["shadow"], bevel=1.2, taper=0.62
        ),
        _slanted_box(
            "drip_l",
            (-5.5, 0, -8.0),
            (2.2, 2.2, 5.0),
            c["main"],
            10.0,
            bevel=0.3,
            taper=0.3,
        ),
        _slanted_box(
            "drip_r",
            (5.5, 0, -8.0),
            (2.2, 2.2, 5.0),
            c["main"],
            -10.0,
            bevel=0.3,
            taper=0.3,
        ),
    ]
    return join_parts(parts, "mon_%s_part_torso" % name)


def _s_arm(name, c):
    """软体臂: 贴球的侧凝胶滴 (短, 不悬浮)。"""
    parts = [
        part_sphere("blob", (0, 0, -2), 2.6, c["main"], segments=12, rings=8),
        _fang("tip", (0, 0, -6.5), 1.1, 3.5, c["main"]),
    ]
    return _shift(join_parts(parts, "mon_%s_part_arm" % name), dy=4.5)


def _s_leg(name, c):
    """软体腿: 贴主球底部的扁平涟漪盘 (非悬浮蛋)。"""
    parts = [
        part_sphere(
            "base",
            (0, 0, -1),
            4.2,
            c["main"],
            segments=14,
            rings=8,
            scale=(1.1, 1.1, 0.4),
        ),
    ]
    # 底盘涟漪环 (用扁球叠出层次)
    parts = [
        part_sphere(
            "base",
            (0, 0, -1),
            4.4,
            c["main"],
            segments=14,
            rings=8,
            scale=(1.1, 1.1, 0.42),
        ),
        part_sphere(
            "ripple",
            (0, 0, -2.0),
            3.0,
            c["shadow"],
            segments=12,
            rings=6,
            scale=(1.2, 1.2, 0.25),
        ),
    ]
    return join_parts(parts, "mon_%s_part_leg" % name)


def _s_weapon(name, c):
    """软体武器: 贴身小骨刺 (前手位置, 不做长柄)。"""
    parts = [
        _fang("sting", (0, 0, -1.5), 1.3, 4.0, c["trim"]),
        part_sphere("bulb", (0, 0, 1.0), 1.5, c["eye"], segments=10, rings=6),
    ]
    return _shift(join_parts(parts, "mon_%s_part_weapon" % name), dy=6.0)


SLIME = {
    "head": _s_head,
    "torso": _s_torso,
    "arm": _s_arm,
    "leg": _s_leg,
    "weapon": _s_weapon,
}


# ── construct 构造/重甲: 方块石甲 + 厚板 + 发光核心 ──
def _c_head(name, c):
    parts = [
        part_box("helm", (0, 0, 2.5), (10, 8, 8), c["main"], bevel=0.8, taper=0.9),
        part_box("visor", (0, 4.2, 1.5), (8.5, 1.0, 2.0), c["shadow"], bevel=0.2),
        part_box("eye_l", (-2.0, 4.6, 1.6), (2.2, 0.5, 0.8), c["eye"], bevel=0.1),
        part_box("eye_r", (2.0, 4.6, 1.6), (2.2, 0.5, 0.8), c["eye"], bevel=0.1),
        part_box("helm_top", (0, 0, 6.8), (11, 9, 1.6), c["light"], bevel=0.3),
        part_box("jaw", (0, 3.0, -3.0), (7.0, 3.0, 2.0), c["shadow"], bevel=0.3),
    ]
    return join_parts(parts, "mon_%s_part_head" % name)


def _c_torso(name, c):
    parts = [
        part_box(
            "chest",
            (0, 0, 4),
            (20, 13, 14),
            c["main"],
            bevel=1.0,
            segments=2,
            taper=0.85,
        ),
        part_box("plate", (0, 6.6, 4), (14, 1.8, 10), c["light"], bevel=0.5),
        part_sphere(
            "core",
            (0, 7.4, 4),
            2.2,
            c["eye"],
            segments=10,
            rings=6,
            scale=(1.0, 0.5, 1.0),
        ),
        part_box(
            "pauldron_l",
            (-12.0, 0, 6),
            (8.0, 11, 8.0),
            c["main"],
            bevel=0.8,
            taper=0.7,
            rotation_z=math.radians(14),
        ),
        part_box(
            "pauldron_r",
            (12.0, 0, 6),
            (8.0, 11, 8.0),
            c["main"],
            bevel=0.8,
            taper=0.7,
            rotation_z=math.radians(-14),
        ),
        part_box("belt", (0, 0, -6.5), (16, 11, 2.4), c["trim"], bevel=0.4),
        part_box("hem", (0, 0, -9), (14, 10, 3), c["shadow"], bevel=0.5),
    ]
    return join_parts(parts, "mon_%s_part_torso" % name)


def _c_arm(name, c):
    parts = [
        part_box("shoulder", (0, 0, -1), (5.5, 5.5, 5), c["main"], bevel=0.6),
        part_box("upper", (0, 0, -6), (4.5, 4.5, 8), c["main"], bevel=0.5),
        part_box("elbow", (0, 0, -10), (5.0, 5.0, 3), c["shadow"], bevel=0.4),
        part_box("forearm", (0, 0, -14), (4.5, 4.5, 7), c["light"], bevel=0.5),
        part_box("fist", (0, 0, -18.5), (6.0, 5.5, 5), c["main"], bevel=0.6),
    ]
    return _shift(join_parts(parts, "mon_%s_part_arm" % name), dy=4.0)


def _c_leg(name, c):
    parts = [
        part_box("thigh", (0, 0, -6), (6.0, 6.0, 10), c["main"], bevel=0.6),
        part_box("knee", (0, 0.5, -11), (6.5, 6.5, 3), c["shadow"], bevel=0.4),
        part_box("shin", (0, 0, -15), (5.5, 5.5, 8), c["light"], bevel=0.5),
        part_box("foot", (0, 2.0, -19), (7.5, 10, 4), c["shadow"], bevel=0.6),
        part_box("toe", (0, 6.0, -19.5), (5.0, 3.0, 2.5), c["dark"], bevel=0.3),
    ]
    return join_parts(parts, "mon_%s_part_leg" % name)


def _c_weapon(name, c):
    parts = [
        part_cylinder("haft", (0, 0, -6), 1.2, 22, c["shadow"], vertices=8, bevel=0.2),
        part_box("head", (0, 0, 3), (10, 5, 7), c["main"], bevel=0.6),
        part_box("head_face_l", (-5.4, 0, 3), (1.4, 5.5, 6), c["light"], bevel=0.2),
        part_box("head_face_r", (5.4, 0, 3), (1.4, 5.5, 6), c["light"], bevel=0.2),
        part_sphere("gem", (0, 3.0, 3), 1.4, c["eye"], segments=8, rings=4),
    ]
    return _shift(join_parts(parts, "mon_%s_part_weapon" % name), dy=6.0)


CONSTRUCT = {
    "head": _c_head,
    "torso": _c_torso,
    "arm": _c_arm,
    "leg": _c_leg,
    "weapon": _c_weapon,
}

FAMILIES = {"humanoid": HUMANOID, "orb": ORB, "slime": SLIME, "construct": CONSTRUCT}


# ══════════════════════════════════════════════════════════════
# 装配预览 + 批量导出 (经 Blender MCP 调用)
# ══════════════════════════════════════════════════════════════
# 30 种怪物共用骨架: hips=20 torso=26 head=37 arm=35 weapon=30
ANCHORS = [
    ("leg_back", "leg", -3, 20),
    ("arm_back", "arm", -3, 35),
    ("weapon", "weapon", -3, 30),
    ("torso", "torso", 0, 26),
    ("head", "head", 0, 37),
    ("arm_front", "arm", 4, 35),
    ("leg_front", "leg", 3, 20),
]


def build_parts(name):
    """按形态族生成某怪物的 5 个部件对象 (局部坐标, 原点=骨骼锚点)。"""
    family = MONSTERS[name][0]
    builders = FAMILIES[family]
    c = _color(name)
    return {slot: builders[slot](name, c) for slot in SLOTS}


def export_monster(name):
    """导出某怪物全部 5 件到 assets/models/mon_<name>_part_<slot>.json。"""
    result = {}
    for slot, obj in build_parts(name).items():
        asset = "mon_%s_part_%s" % (name, slot)
        path = os.path.join(REPO, "assets", "models", "%s.json" % asset)
        verts, tris = export_part_json(obj, path, asset)
        result[slot] = (verts, tris)
    return result


def export_all():
    total = {}
    for name in MONSTERS:
        total[name] = export_monster(name)
        print("EXPORT mon_%-20s %s" % (name, {k: v[1] for k, v in total[name].items()}))
    print("EXPORT_ALL_OK monsters=%d files=%d" % (len(total), len(total) * 5))
    return total


def preview_one(name):
    """在 Blender 里装配某怪物并渲染一张正面/3q 预览。"""
    import bpy
    import blender_scene

    camera = blender_scene.setup_scene()  # 先清场并布置灯光/相机
    parts = build_parts(name)  # 再建模 (避免被 clear_scene 删掉)
    objs = []
    for bone_name, slot, bx, bz in ANCHORS:
        obj = parts[slot]
        obj.name = bone_name
        obj.scale = (PPU, PPU, PPU)
        obj.location = (bx * PPU, 0, bz * PPU)
        objs.append(obj)
    bpy.context.view_layer.update()
    pts = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
    low = min(p.z for p in pts)
    high = max(p.z for p in pts)
    cz = (low + high) * 0.5
    blender_scene.frame_camera(
        camera, (0, 0, cz), distance=105.0, azimuth=-90.0, elevation=6.0
    )
    out = os.path.join(REPO, "art_src", "preview", "monster_preview_%s.png" % name)
    blender_scene.render_to(out)
    print("PREVIEW %s height=%.1f -> %s" % (name, high - low, out))
    return out
