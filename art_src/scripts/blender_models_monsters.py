# A17-Monster: 30 种普通怪物手建低模 (参数化批量)
# 局部空间: 原点 = 骨骼锚点, x=宽, y=深度(前=+Y 朝相机), z=高
# 30 种怪物共用同一骨架: ppu 0.8, anchor [24,62], 5 槽位 head/torso/arm/leg/weapon
#   骨骼世界 z: hips=20 torso=26 head=37 arm=35 weapon=30
# 约定与六 Boss 一致: 直接 part_* 局部坐标, 不 flip_y (export_part_json 做 mesh=(x,-y,z))
# 第一版: 4 个形态族模板 + 各自 2D 配色; 二次精修再逐族细化特征
import math
import os

from blender_lib import (
    part_box,
    part_cylinder,
    part_sphere,
    part_prism,
    part_torus,
    join_parts,
)
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

# ══════════════════════════════════════════════════════════════
# A22: per-monster refinement (逐怪覆盖, 不改共享 FAMILIES 模板)
#   REFINEMENTS[name] = {slot: [detail_spec, ...]}
#   detail_spec = (builder_name, kwargs_dict)  — builder_name 是 blender_lib.part_* 的名字
#   _refine 在每个 slot 的原部件对象上 join 追加细节对象, 返回同一 parts 字典。
#   空 REFINEMENTS 时 _refine 为 no-op; 导出因 bmesh 面序非确定性,
#   与磁盘基线仅语义等价 (顶点数/面数/bbox), 不做字节比对。
# ══════════════════════════════════════════════════════════════
REFINEMENTS = {}


def _refine(name, parts):
    """把 REFINEMENTS[name] 的追加细节 join 进 parts 的对应槽位对象。"""
    spec = REFINEMENTS.get(name)
    if not spec:
        return parts
    import blender_lib

    for slot, details in spec.items():
        base = parts.get(slot)
        if base is None:
            continue
        c = _color(name)
        extras = []
        for detail in details:
            # detail 用 callable(c) -> bpy.Object (当前 4 族配方均为 callable);
            # (builder_name, kwargs) 元组路径保留为 P2 扩展点, 当前无配方使用。
            if callable(detail):
                extras.append(detail(c))
            else:
                builder_name, kwargs = detail
                builder = getattr(blender_lib, builder_name)
                extras.append(builder(**kwargs))
        if extras:
            # A22: 追加细节顶点收敛到基础部件包围盒内 (spec §3.6 包络约束)。
            bx = [v.co.x for v in base.data.vertices]
            by = [v.co.y for v in base.data.vertices]
            bz = [v.co.z for v in base.data.vertices]
            xmin, xmax = min(bx), max(bx)
            ymin, ymax = min(by), max(by)
            zmin, zmax = min(bz), max(bz)
            for ex in extras:
                for v in ex.data.vertices:
                    v.co.x = min(max(v.co.x, xmin), xmax)
                    v.co.y = min(max(v.co.y, ymin), ymax)
                    v.co.z = min(max(v.co.z, zmin), zmax)
            parts[slot] = blender_lib.join_parts([base] + extras, base.name)
    return parts


# A22 orc 精修 (humanoid 代表): 结构分层 + 特征件, 全部落在 A17 包络内。
# 用 callable detail 注入配色 c; part_* 基元坐标均为部件局部系 (原点=骨骼 pivot)。
# A22 slime 精修 (slime 代表): 软体核心 + 气泡 + 骨色/金色分层, 落在 A17 包络内。
# 低面数部件 (weapon 120/arm 188) 用高分段球体高效补足面数并做视觉层次。
# A22 golem 精修 (construct 代表): 石块分层 + 金色符文核心 + 铁箍, 落在 A17 包络内。
# A22 fire_imp 精修 (orb 代表): 火焰层叠 + 金色核心 + 亮黄符文, 落在 A17 包络内。
REFINEMENTS["fire_imp"] = {
    "head": [
        # 亮黄眼 + 火焰眉 + 金冠
        lambda c: part_sphere(
            "imp_eye_l", (-2.2, 4.0, 2.0), 1.0, c["eye"], segments=16, rings=10
        ),
        lambda c: part_sphere(
            "imp_eye_r", (2.2, 4.0, 2.0), 1.0, c["eye"], segments=16, rings=10
        ),
        lambda c: part_prism(
            "imp_flame_brow",
            (0.0, 3.5, 5.0),
            [(-3, 0), (3, 0), (0, 5)],
            1.5,
            GOLD,
            bevel=0.2,
        ),
        lambda c: part_box(
            "imp_brow",
            (0.0, 4.2, 3.8),
            (7.0, 1.2, 1.2),
            c["shadow"],
            bevel=0.2,
            segments=2,
        ),
    ],
    "torso": [
        # 前置金色火核 + 亮黄环 + 火焰尖层 + 亮黄腰带
        lambda c: part_sphere(
            "imp_core", (0.0, 8.4, 3.0), 3.2, c["eye"], segments=20, rings=12
        ),
        lambda c: part_sphere(
            "imp_core_ring", (0.0, 8.0, 3.0), 4.2, GOLD, segments=16, rings=10
        ),
        lambda c: part_prism(
            "imp_flame1",
            (-4.0, 7.6, 9.0),
            [(-2, 0), (2, 0), (0, 6)],
            1.5,
            c["light"],
            bevel=0.2,
        ),
        lambda c: part_prism(
            "imp_flame2",
            (4.0, 7.6, 8.0),
            [(-2, 0), (2, 0), (0, 5)],
            1.5,
            c["main"],
            bevel=0.2,
        ),
        lambda c: part_box(
            "imp_belt",
            (0.0, 8.6, -2.0),
            (14.0, 2.0, 2.4),
            GOLD,
            bevel=0.25,
            segments=2,
        ),
    ],
    "arm": [
        # 金色火环 + 高分段火焰臂核 (456 -> 1200+)
        lambda c: part_sphere(
            "imp_arm_core", (0.0, 5.2, -6.0), 1.8, c["eye"], segments=24, rings=14
        ),
        lambda c: part_cylinder(
            "imp_arm_ring", (0.0, 5.2, -11.0), 2.2, 4.0, GOLD, vertices=14, bevel=0.25
        ),
        lambda c: part_sphere(
            "imp_arm_bub", (0.0, 5.2, -2.0), 1.2, c["light"], segments=20, rings=12
        ),
    ],
    "leg": [
        # 高分段火焰腿核 + 金环 (236 -> 1200+)
        lambda c: part_sphere(
            "imp_leg_core", (0.0, 2.8, -8.0), 2.2, c["main"], segments=24, rings=14
        ),
        lambda c: part_cylinder(
            "imp_leg_ring", (0.0, 2.8, -12.0), 2.6, 3.0, GOLD, vertices=16, bevel=0.25
        ),
        lambda c: part_sphere(
            "imp_leg_bub", (0.0, 2.8, -3.0), 1.4, c["eye"], segments=20, rings=12
        ),
    ],
    "weapon": [
        # 火焰刃层 + 金柄 + 亮黄护手
        lambda c: part_prism(
            "imp_blade",
            (0.0, 7.2, 4.0),
            [(-3, 0), (3, 0), (0, 12)],
            1.6,
            c["eye"],
            bevel=0.25,
        ),
        lambda c: part_box(
            "imp_guard",
            (0.0, 7.0, -1.0),
            (7.0, 2.0, 2.0),
            GOLD,
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_cylinder(
            "imp_grip",
            (0.0, 6.8, -10.0),
            1.5,
            12.0,
            c["shadow"],
            vertices=12,
            bevel=0.25,
        ),
    ],
}

REFINEMENTS["golem"] = {
    "head": [
        # 金色符文眼 + 铁质眉脊 + 石质额甲分层
        lambda c: part_box(
            "golem_eyes",
            (0.0, 4.6, 2.0),
            (7.0, 1.0, 1.4),
            c["eye"],
            bevel=0.2,
            segments=2,
        ),
        lambda c: part_box(
            "golem_brow",
            (0.0, 4.8, 4.0),
            (9.0, 1.6, 1.8),
            IRON,
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_box(
            "golem_crest",
            (0.0, 3.0, 6.4),
            (4.0, 2.0, 1.8),
            c["light"],
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_box(
            "golem_jaw", (0.0, 4.6, -3.0), (7.0, 1.4, 1.6), IRON, bevel=0.2, segments=2
        ),
        lambda c: part_sphere(
            "golem_eye_l", (-2.0, 5.0, 2.0), 0.9, c["eye"], segments=16, rings=10
        ),
        lambda c: part_sphere(
            "golem_eye_r", (2.0, 5.0, 2.0), 0.9, c["eye"], segments=16, rings=10
        ),
    ],
    "torso": [
        # 前置铁质胸甲板 + 金色核心 + 双肩石块 + 腹部铁箍
        lambda c: part_box(
            "golem_chest",
            (0.0, 8.3, 2.0),
            (22.0, 2.0, 13.0),
            IRON,
            bevel=0.3,
            segments=2,
        ),
        lambda c: part_sphere(
            "golem_core", (0.0, 9.6, 2.0), 3.0, c["eye"], segments=20, rings=12
        ),
        lambda c: part_box(
            "golem_shoulder_l",
            (-13.0, 5.0, 7.0),
            (7.0, 4.0, 4.0),
            c["light"],
            bevel=0.3,
            segments=2,
        ),
        lambda c: part_box(
            "golem_shoulder_r",
            (13.0, 5.0, 7.0),
            (7.0, 4.0, 4.0),
            c["light"],
            bevel=0.3,
            segments=2,
        ),
        lambda c: part_box(
            "golem_belt",
            (0.0, 8.8, -7.0),
            (24.0, 2.2, 3.0),
            GOLD,
            bevel=0.25,
            segments=2,
        ),
    ],
    "arm": [
        # 铁箍 + 金环 + 石质护手
        lambda c: part_cylinder(
            "golem_band", (0.0, 6.4, -8.0), 3.2, 6.0, IRON, vertices=14, bevel=0.25
        ),
        lambda c: part_cylinder(
            "golem_ring", (0.0, 6.4, -14.0), 3.0, 3.0, GOLD, vertices=12, bevel=0.25
        ),
        lambda c: part_box(
            "golem_fist",
            (0.0, 6.4, -19.0),
            (5.5, 4.0, 4.0),
            c["light"],
            bevel=0.3,
            segments=2,
        ),
    ],
    "leg": [
        # 铁质护膝 + 金胫甲 + 石块脚
        lambda c: part_box(
            "golem_knee",
            (0.0, 7.0, -9.0),
            (7.0, 3.0, 5.0),
            IRON,
            bevel=0.3,
            segments=2,
        ),
        lambda c: part_box(
            "golem_shin",
            (0.0, 7.2, -15.0),
            (5.0, 2.0, 8.0),
            GOLD,
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_box(
            "golem_foot",
            (0.0, 6.0, -20.0),
            (7.0, 4.0, 2.0),
            c["light"],
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_cylinder(
            "golem_ankle", (0.0, 7.0, -18.0), 3.2, 3.0, IRON, vertices=14, bevel=0.25
        ),
    ],
    "weapon": [
        # 巨岩锤头 + 铁箍 + 符文 + 握柄
        lambda c: part_box(
            "golem_hammer",
            (0.0, 8.4, 3.0),
            (11.0, 5.0, 8.0),
            c["light"],
            bevel=0.35,
            segments=2,
        ),
        lambda c: part_box(
            "golem_hammer_band",
            (0.0, 10.0, 3.0),
            (11.5, 1.2, 8.5),
            IRON,
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_box(
            "golem_rune",
            (0.0, 10.8, 3.0),
            (4.0, 1.0, 4.0),
            c["eye"],
            bevel=0.2,
            segments=2,
        ),
        lambda c: part_cylinder(
            "golem_grip", (0.0, 6.8, -12.0), 2.0, 12.0, GOLD, vertices=12, bevel=0.25
        ),
    ],
}

REFINEMENTS["slime"] = {
    # head: 骨色眼窝环 + 顶部晶核 + 两侧气泡
    "head": [
        lambda c: part_sphere(
            "slime_core", (0.0, 5.2, 2.0), 1.8, GOLD, segments=20, rings=12
        ),
        lambda c: part_sphere(
            "slime_bub_l", (-2.2, 5.0, 0.0), 0.9, BONE, segments=16, rings=10
        ),
        lambda c: part_sphere(
            "slime_bub_r", (2.2, 5.0, 0.0), 0.9, BONE, segments=16, rings=10
        ),
        lambda c: part_box(
            "slime_brow",
            (0.0, 6.0, 3.6),
            (5.0, 0.8, 0.9),
            c["dark"],
            bevel=0.2,
            segments=2,
        ),
    ],
    # torso: 前置骨色核心环 + 金色晶核 + 表面气泡群
    "torso": [
        lambda c: part_sphere(
            "slime_heart", (0.0, 7.6, 2.0), 3.2, GOLD, segments=16, rings=9
        ),
        lambda c: part_sphere(
            "slime_heart_ring", (0.0, 7.2, 2.0), 4.2, BONE, segments=14, rings=8
        ),
        lambda c: part_sphere(
            "slime_bub1", (-5.5, 6.8, 8.0), 1.4, BONE, segments=16, rings=10
        ),
        lambda c: part_sphere(
            "slime_bub3", (0.0, 6.8, -6.0), 1.6, GOLD, segments=16, rings=10
        ),
    ],
    # arm: 高分段骨色核心 (188 -> 1200+)
    "arm": [
        lambda c: part_sphere(
            "slime_arm_core", (0.0, 5.5, -4.0), 1.9, GOLD, segments=24, rings=14
        ),
        lambda c: part_sphere(
            "slime_arm_bub", (0.0, 5.5, -1.5), 1.3, BONE, segments=20, rings=12
        ),
    ],
    # leg: 高分段核心 + 骨色足环
    "leg": [
        lambda c: part_sphere(
            "slime_leg_core", (0.0, 4.4, -1.4), 2.6, GOLD, segments=24, rings=14
        ),
        lambda c: part_cylinder(
            "slime_leg_ring", (0.0, 4.4, -2.2), 3.4, 1.2, BONE, vertices=20, bevel=0.2
        ),
    ],
    # weapon: 高分段骨色刺核 + 金色环 (120 -> 1200+)
    "weapon": [
        lambda c: part_sphere(
            "slime_weapon_core", (0.0, 6.0, -0.5), 1.2, BONE, segments=28, rings=16
        ),
        lambda c: part_sphere(
            "slime_weapon_tip", (0.0, 6.0, 1.8), 0.9, GOLD, segments=24, rings=14
        ),
        lambda c: part_cylinder(
            "slime_weapon_ring",
            (0.0, 6.0, -2.5),
            1.3,
            1.0,
            GOLD,
            vertices=20,
            bevel=0.2,
        ),
    ],
}

REFINEMENTS["orc"] = {
    # head: 骨色眉脊 + 深色眼罩 + 骨色獠牙, 全部前置 (y 更负 = 朝相机)
    "head": [
        lambda c: part_box(
            "orc_brow", (0.0, 5.0, 3.6), (9.0, 1.2, 1.4), BONE, bevel=0.25, segments=2
        ),
        lambda c: part_box(
            "orc_eyemask",
            (0.0, 5.1, 0.5),
            (8.0, 1.0, 2.6),
            c["dark"],
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_box(
            "orc_tusk_l",
            (-3.2, 5.0, -5.2),
            (1.2, 1.2, 2.4),
            BONE,
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_box(
            "orc_tusk_r",
            (3.2, 5.0, -5.2),
            (1.2, 1.2, 2.4),
            BONE,
            bevel=0.25,
            segments=2,
        ),
    ],
    # torso: 银灰胸甲 + 金色高光 + 骨色肩扣 + 深色腹带, 前置
    "torso": [
        lambda c: part_box(
            "orc_chest",
            (0.0, 7.2, 2.5),
            (17.0, 1.6, 10.0),
            IRON,
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_box(
            "orc_chest_hl",
            (0.0, 8.1, 4.0),
            (12.0, 0.9, 1.4),
            GOLD,
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_box(
            "orc_shoulder_l",
            (-11.0, 6.5, 7.0),
            (5.0, 3.0, 3.5),
            BONE,
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_box(
            "orc_shoulder_r",
            (11.0, 6.5, 7.0),
            (5.0, 3.0, 3.5),
            BONE,
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_box(
            "orc_belt",
            (0.0, 7.6, -6.5),
            (19.0, 2.0, 2.8),
            c["dark"],
            bevel=0.25,
            segments=2,
        ),
    ],
    # arm: 银灰护腕 + 金扣
    "arm": [
        lambda c: part_cylinder(
            "orc_bracer", (0.0, 6.5, -12.0), 3.2, 8.0, IRON, vertices=12, bevel=0.25
        ),
        lambda c: part_box(
            "orc_wrist",
            (0.0, 6.8, -17.5),
            (5.5, 2.4, 2.0),
            GOLD,
            bevel=0.25,
            segments=2,
        ),
    ],
    # leg: 银灰护膝 + 金胫甲条 + 骨色脚踝扣
    "leg": [
        lambda c: part_box(
            "orc_knee",
            (0.0, 6.5, -10.0),
            (6.5, 2.8, 4.5),
            IRON,
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_box(
            "orc_shin",
            (0.0, 6.8, -15.0),
            (4.5, 1.6, 7.0),
            GOLD,
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_box(
            "orc_ankle",
            (0.0, 6.5, -18.5),
            (5.5, 2.4, 1.8),
            BONE,
            bevel=0.25,
            segments=2,
        ),
    ],
    # weapon: 银灰斧刃 + 金握柄缠带 + 骨色柄尾
    "weapon": [
        lambda c: part_box(
            "orc_axe_edge",
            (0.0, 7.0, 2.0),
            (7.5, 1.4, 9.0),
            IRON,
            bevel=0.25,
            segments=2,
        ),
        lambda c: part_cylinder(
            "orc_grip", (0.0, 6.8, -14.0), 1.5, 9.0, GOLD, vertices=10, bevel=0.25
        ),
        lambda c: part_box(
            "orc_pommel",
            (0.0, 6.8, -19.0),
            (2.8, 2.8, 2.4),
            BONE,
            bevel=0.25,
            segments=2,
        ),
    ],
}


# A22b: 同族配方复用 — 4 族样板配方直接套用到同族其他怪物。
# 同族共享 builder (几何基线完全相同), 仅 _color(name) 配色不同,
# 因此复用样板配方即可得到结构一致、配色各异的精修结果。
_REFINE_FAMILY = {
    "humanoid": "orc",
    "orb": "fire_imp",
    "slime": "slime",
    "construct": "golem",
}
# ══════════════════════════════════════════════════════════════
# A22b-一怪一形: 强身份怪的独立签名细节 (包络内特征件)。
# _sig(mon, head, weapon?, torso?) 注册签名槽; merge 助手逐槽 list() 深拷贝
# pilot list 后整槽替换签名槽 — 绝不共享可变 list(否则污染 pilot 全族)。
# 细节坐标在 base bbox 内、正面 +Y(head 3~5 / torso 4~6.5 / weapon 5~8),
# 不同基元/坐标/分段数制造哈希与视觉差异。
# ══════════════════════════════════════════════════════════════
SIGNATURE_DETAILS = {}


def _sig(mon, head, weapon=None, torso=None):
    spec = {"head": head}
    if weapon is not None:
        spec["weapon"] = weapon
    if torso is not None:
        spec["torso"] = torso
    SIGNATURE_DETAILS[mon] = spec


# ── 第一批 1/2: humanoid 强身份 ──
_sig(
    "night_stalker",  # 兽首尖耳+尖牙+兽目; 爪刃
    [
        lambda c: part_prism(
            "beast_ear",
            (-3.0, 4.4, 5.6),
            [(-2, 0), (2, 0), (0, 4)],
            1.1,
            c["shadow"],
            bevel=0.2,
        ),
        lambda c: part_prism(
            "beast_fang",
            (0.0, 4.6, 1.0),
            [(-1, 0), (1, 0), (0, -3)],
            0.9,
            (230, 226, 200),
            bevel=0.1,
        ),
        lambda c: part_sphere(
            "beast_eye", (0.0, 4.8, 3.2), 1.3, c["eye"], segments=9, rings=5
        ),
    ],
    weapon=[
        lambda c: part_prism(
            "claw", (0.0, 6.0, 2.0), [(-3, 0), (3, 0), (0, 7)], 1.2, IRON, bevel=0.2
        ),
        lambda c: part_sphere(
            "claw_socket", (0.0, 5.0, -2.0), 1.1, c["shadow"], segments=9, rings=5
        ),
    ],
)
_sig(
    "skeleton_archer",  # 骷髅黑眼窝+额裂; 骨弓
    [
        lambda c: part_box(
            "skull_brow", (0.0, 4.6, 4.0), (8.0, 1.3, 1.5), (200, 198, 180), bevel=0.3
        ),
        lambda c: part_box(
            "eye_socket_l", (-2.3, 4.9, 2.0), (2.4, 0.8, 2.4), (30, 28, 26), bevel=0.2
        ),
        lambda c: part_box(
            "eye_socket_r", (2.3, 4.9, 2.0), (2.4, 0.8, 2.4), (30, 28, 26), bevel=0.2
        ),
    ],
    weapon=[
        lambda c: part_torus(
            "bone_bow",
            (0.0, 6.0, -4.0),
            5.0,
            0.7,
            (214, 210, 190),
            major_segments=11,
            minor_segments=5,
        ),
        lambda c: part_box(
            "bone_string", (0.0, 6.4, -4.0), (0.5, 0.5, 11.0), c["trim"], bevel=0.1
        ),
    ],
)
_sig(
    "bone_soldier",  # 破胸甲+红眼窝; 锈剑
    [
        lambda c: part_box(
            "broken_helm", (0.0, 4.4, 4.6), (9.0, 1.4, 2.0), IRON, bevel=0.3
        ),
        lambda c: part_sphere(
            "red_eye_l", (-2.4, 4.8, 1.8), 1.1, (220, 50, 40), segments=9, rings=5
        ),
        lambda c: part_sphere(
            "red_eye_r", (2.4, 4.8, 1.8), 1.1, (220, 50, 40), segments=9, rings=5
        ),
    ],
    weapon=[
        lambda c: part_prism(
            "rusty_sword",
            (0.0, 6.0, 0.0),
            [(-1.5, 0), (1.5, 0), (0, 10)],
            1.0,
            IRON,
            bevel=0.2,
        ),
        lambda c: part_box(
            "sword_guard", (0.0, 5.6, -3.0), (6.0, 1.0, 0.8), GOLD, bevel=0.2
        ),
    ],
)

# ── 第一批 2/2: humanoid 暗影 + orb 雷暴 ──
_sig(
    "shadow_assassin",  # 头巾斗篷+幽蓝双刃
    [
        lambda c: part_prism(
            "hood_point",
            (0.0, 4.4, 6.2),
            [(-3, 0), (3, 0), (0, 4)],
            1.3,
            c["shadow"],
            bevel=0.2,
        ),
        lambda c: part_box(
            "face_wrap", (0.0, 4.9, 2.4), (7.0, 0.9, 2.6), c["main"], bevel=0.2
        ),
        lambda c: part_sphere(
            "assassin_eye", (0.0, 5.0, 3.4), 1.0, (90, 190, 240), segments=9, rings=5
        ),
    ],
    weapon=[
        lambda c: part_prism(
            "blue_blade_l",
            (-2.0, 6.0, 2.0),
            [(-1, 0), (1, 0), (0, 9)],
            0.8,
            (90, 190, 240),
            bevel=0.1,
        ),
        lambda c: part_prism(
            "blue_blade_r",
            (2.0, 6.0, 2.0),
            [(-1, 0), (1, 0), (0, 9)],
            0.8,
            (90, 190, 240),
            bevel=0.1,
        ),
    ],
)
_sig(
    "shadow_stalker",  # 幽灵锯齿披风+红眼 (torso 签名)
    [
        lambda c: part_sphere(
            "ghost_eye", (0.0, 4.6, 3.0), 1.4, (230, 60, 70), segments=9, rings=5
        ),
        lambda c: part_prism(
            "ghost_crest",
            (0.0, 4.2, 6.0),
            [(-4, 0), (4, 0), (0, 5)],
            1.0,
            c["shadow"],
            bevel=0.2,
        ),
    ],
    torso=[
        lambda c: part_prism(
            "cape_tooth_1",
            (-6.0, 6.0, -7.0),
            [(-3, 0), (3, 0), (0, -5)],
            1.0,
            c["shadow"],
            bevel=0.2,
        ),
        lambda c: part_prism(
            "cape_tooth_2",
            (6.0, 6.0, -7.0),
            [(-3, 0), (3, 0), (0, -5)],
            1.0,
            c["shadow"],
            bevel=0.2,
        ),
    ],
)
_sig(
    "storm_elemental",  # 雷云体+落雷 (torso 签名)
    [
        lambda c: part_sphere(
            "storm_eye", (0.0, 3.6, 2.0), 1.5, c["eye"], segments=9, rings=5
        ),
        lambda c: part_prism(
            "cloud_peak",
            (0.0, 3.2, 5.0),
            [(-4, 0), (4, 0), (0, 4)],
            1.2,
            c["main"],
            bevel=0.2,
        ),
    ],
    torso=[
        lambda c: part_prism(
            "bolt_main",
            (0.0, 6.4, 2.0),
            [(-2, 4), (2, 4), (0, -4)],
            1.1,
            c["eye"],
            bevel=0.15,
        ),
        lambda c: part_prism(
            "cloud_hem",
            (0.0, 5.8, -7.0),
            [(-7, 0), (7, 0), (0, -4)],
            1.0,
            c["shadow"],
            bevel=0.2,
        ),
    ],
)

# ── 第二批: orb / slime / construct + humanoid 冰卫铁卫 ──
_sig(
    "lightning_orb",  # 交叉电弧+四向尖刺 (球头)
    [
        lambda c: part_prism(
            "arc_x",
            (0.0, 3.4, 1.0),
            [(-4, 0), (4, 0), (0, 1)],
            0.7,
            c["eye"],
            bevel=0.1,
            rotation_z=0.5,
        ),
        lambda c: part_prism(
            "arc_z",
            (0.0, 3.6, 1.0),
            [(-4, 0), (4, 0), (0, 1)],
            0.7,
            c["eye"],
            bevel=0.1,
            rotation_z=-0.5,
        ),
        lambda c: part_sphere(
            "orb_core", (0.0, 3.0, 1.0), 1.6, c["eye"], segments=11, rings=6
        ),
    ],
)
_sig(
    "void_walker",  # 白瞳+斜向紫裂隙 (torso 签名)
    [
        lambda c: part_sphere(
            "void_pupil", (0.0, 3.6, 1.5), 1.3, (240, 240, 250), segments=11, rings=6
        ),
        lambda c: part_prism(
            "face_rift",
            (0.0, 3.2, 3.4),
            [(-2, 0), (2, 0), (0, 3)],
            0.8,
            c["eye"],
            bevel=0.1,
        ),
    ],
    torso=[
        lambda c: part_prism(
            "body_rift",
            (0.0, 6.2, 3.0),
            [(-3, 5), (3, 5), (0, -5)],
            0.9,
            c["eye"],
            bevel=0.15,
        ),
        lambda c: part_sphere(
            "void_core", (0.0, 6.6, -2.0), 1.8, c["shadow"], segments=11, rings=6
        ),
    ],
)
_sig(
    "blood_leech",  # 前端吸盘+环节 (torso 签名)
    [
        lambda c: part_torus(
            "sucker",
            (0.0, 5.0, 2.0),
            2.0,
            0.6,
            c["eye"],
            major_segments=13,
            minor_segments=5,
        ),
        lambda c: part_sphere(
            "leech_eye", (0.0, 4.6, -1.0), 1.0, (240, 60, 50), segments=9, rings=5
        ),
    ],
    torso=[
        lambda c: part_torus(
            "segment_1",
            (0.0, 6.0, 6.0),
            4.0,
            0.7,
            c["main"],
            major_segments=13,
            minor_segments=5,
        ),
        lambda c: part_torus(
            "segment_2",
            (0.0, 6.0, 0.0),
            5.0,
            0.7,
            c["shadow"],
            major_segments=13,
            minor_segments=5,
        ),
    ],
)
_sig(
    "poison_wyrm",  # 蛇头+毒牙+背毒泡 (weapon 签名)
    [
        lambda c: part_prism(
            "snout",
            (0.0, 5.0, 2.4),
            [(-2, 0), (2, 0), (0, 4)],
            1.0,
            c["main"],
            bevel=0.2,
        ),
        lambda c: part_sphere(
            "snake_eye", (0.0, 5.4, 1.4), 1.1, c["eye"], segments=9, rings=5
        ),
    ],
    weapon=[
        lambda c: part_prism(
            "fang",
            (0.0, 5.5, 0.0),
            [(-1, 0), (1, 0), (0, -4)],
            0.7,
            c["eye"],
            bevel=0.1,
        ),
        lambda c: part_sphere(
            "venom_bubble", (0.0, 5.0, -2.0), 1.0, c["eye"], segments=9, rings=5
        ),
    ],
)
_sig(
    "stone_guardian",  # 符文眼+胸符 (torso 签名, vs golem 魔核)
    [
        lambda c: part_box(
            "rune_eye_l", (-2.4, 4.2, 2.0), (2.0, 0.9, 2.0), c["eye"], bevel=0.2
        ),
        lambda c: part_box(
            "rune_eye_r", (2.4, 4.2, 2.0), (2.0, 0.9, 2.0), c["eye"], bevel=0.2
        ),
    ],
    torso=[
        lambda c: part_prism(
            "chest_rune",
            (0.0, 7.0, 2.0),
            [(-3, 0), (3, 0), (0, 6)],
            0.9,
            c["eye"],
            bevel=0.2,
        ),
        lambda c: part_box(
            "stone_seam", (0.0, 6.6, -4.0), (10.0, 0.8, 0.7), c["shadow"], bevel=0.1
        ),
    ],
)
_sig(
    "ice_warden",  # 三峰冰晶冠+冰瞳 (torso 签名)
    [
        lambda c: part_prism(
            "ice_crown_c",
            (0.0, 4.0, 6.0),
            [(-2, 0), (2, 0), (0, 5)],
            1.0,
            c["eye"],
            bevel=0.15,
        ),
        lambda c: part_prism(
            "ice_crown_l",
            (-3.2, 4.0, 4.6),
            [(-2, 0), (2, 0), (0, 4)],
            0.9,
            c["eye"],
            bevel=0.15,
        ),
        lambda c: part_sphere(
            "ice_pupil", (0.0, 4.6, 2.0), 1.2, c["eye"], segments=10, rings=6
        ),
    ],
    torso=[
        lambda c: part_sphere(
            "ice_core", (0.0, 6.2, 3.0), 2.0, c["eye"], segments=12, rings=7
        ),
        lambda c: part_prism(
            "ice_hem",
            (0.0, 6.0, -7.0),
            [(-6, 0), (6, 0), (0, -4)],
            0.9,
            c["eye"],
            bevel=0.15,
        ),
    ],
)
_sig(
    "iron_sentinel",  # 一字蓝目镜+胸前中缝 (torso 签名)
    [
        lambda c: part_box(
            "visor", (0.0, 4.6, 2.4), (9.0, 1.0, 2.0), c["eye"], bevel=0.2
        ),
        lambda c: part_box(
            "helm_crest", (0.0, 4.0, 6.0), (1.4, 0.9, 3.4), c["shadow"], bevel=0.1
        ),
    ],
    torso=[
        lambda c: part_box(
            "chest_seam", (0.0, 6.4, 3.0), (0.8, 0.8, 14.0), c["eye"], bevel=0.1
        ),
        lambda c: part_box(
            "rank_plate", (0.0, 6.6, -3.0), (6.0, 0.8, 3.0), IRON, bevel=0.2
        ),
    ],
)

# ── 第二批 1/2: humanoid 弱线索/撞色对 (7 种) ──
_sig("archer",  # 长耳+木弓+皮甲 (撞色对: 与goblin_hunter配色同, 靠造型分)
     [lambda c: part_prism("elf_ear", (-3.6, 4.2, 4.4), [(-1, 0), (1, 0), (0, 6)], 0.8, c["main"], bevel=0.1),
      lambda c: part_sphere("archer_eye", (0.0, 4.6, 2.6), 1.1, c["eye"], segments=9, rings=5)],
     weapon=[lambda c: part_torus("wood_bow", (0.0, 5.6, -3.0), 5.0, 0.6, c["main"], major_segments=11, minor_segments=5),
             lambda c: part_box("bow_string", (0.0, 6.0, -3.0), (0.5, 0.5, 11.0), (210, 200, 170), bevel=0.1)])
_sig("goblin_hunter",  # 长耳+黄目+背后猎网 (撞色对)
     [lambda c: part_prism("hunter_ear", (-3.6, 4.0, 5.0), [(-1, 0), (1, 0), (0, 7)], 0.8, c["light"], bevel=0.1),
      lambda c: part_prism("hunter_ear_r", (3.6, 4.0, 5.0), [(-1, 0), (1, 0), (0, 7)], 0.8, c["light"], bevel=0.1)],
     weapon=[lambda c: part_box("hunt_net", (0.0, 6.0, 0.0), (7.0, 0.7, 8.0), c["trim"], bevel=0.2),
             lambda c: part_sphere("net_knot", (0.0, 6.4, 0.0), 1.0, c["eye"], segments=9, rings=5)])
_sig("shaman",  # 图腾杖+羽毛头饰+面具 (torso 签名)
     [lambda c: part_prism("feather", (0.0, 4.2, 6.0), [(-1, 0), (1, 0), (0, 5)], 0.7, c["eye"], bevel=0.1),
      lambda c: part_box("mask", (0.0, 4.8, 2.0), (7.0, 1.0, 4.0), c["trim"], bevel=0.2)],
     torso=[lambda c: part_cylinder("totem_pole", (0.0, 6.2, 4.0), 1.0, 12.0, c["main"], vertices=7),
            lambda c: part_sphere("totem_head", (0.0, 6.4, 9.0), 1.6, c["eye"], segments=9, rings=5)])
_sig("dark_mage",  # 兜帽紫瞳+悬浮法球
     [lambda c: part_prism("hood", (0.0, 4.0, 5.0), [(-4, 0), (4, 0), (0, 5)], 1.2, c["shadow"], bevel=0.2),
      lambda c: part_sphere("purple_pupil", (0.0, 4.7, 2.4), 1.2, c["eye"], segments=10, rings=6)],
     weapon=[lambda c: part_sphere("orb", (0.0, 6.4, 2.0), 2.0, c["eye"], segments=12, rings=7),
             lambda c: part_torus("orb_ring", (0.0, 6.0, 2.0), 2.6, 0.3, GOLD, major_segments=11, minor_segments=5)])
_sig("blood_priest",  # 红袍面纱+金十字 (torso 签名)
     [lambda c: part_box("veil", (0.0, 4.8, 3.0), (8.0, 0.9, 5.0), c["main"], bevel=0.2),
      lambda c: part_sphere("priest_eye", (0.0, 5.0, 2.0), 1.0, c["eye"], segments=9, rings=5)],
     torso=[lambda c: part_box("gold_cross_v", (0.0, 6.4, 2.0), (1.2, 0.8, 6.0), GOLD, bevel=0.2),
            lambda c: part_box("gold_cross_h", (0.0, 6.4, 2.0), (5.0, 0.8, 1.2), GOLD, bevel=0.2)])
_sig("summoner",  # 符文冠+召唤法杖
     [lambda c: part_prism("rune_crown", (0.0, 4.0, 6.0), [(-3, 0), (3, 0), (0, 5)], 1.0, c["eye"], bevel=0.15),
      lambda c: part_sphere("summoner_eye", (0.0, 4.7, 2.4), 1.1, c["eye"], segments=9, rings=5)],
     weapon=[lambda c: part_cylinder("staff", (0.0, 6.0, 0.0), 0.8, 14.0, c["main"], vertices=7),
             lambda c: part_sphere("staff_orb", (0.0, 6.2, 7.0), 1.6, c["eye"], segments=10, rings=6)])
_sig("necromancer",  # 绿袍兜帽+骷髅脸+绿瞳
     [lambda c: part_prism("necro_hood", (0.0, 4.0, 5.4), [(-4, 0), (4, 0), (0, 5)], 1.1, c["shadow"], bevel=0.2),
      lambda c: part_box("skull_face", (0.0, 4.8, 2.0), (6.0, 0.9, 4.0), (200, 196, 170), bevel=0.2),
      lambda c: part_sphere("necro_eye", (0.0, 5.0, 2.2), 1.0, c["eye"], segments=9, rings=5)],
     weapon=[lambda c: part_cylinder("bone_staff", (0.0, 6.0, 0.0), 0.7, 15.0, (210, 204, 180), vertices=7),
             lambda c: part_sphere("skull_top", (0.0, 6.2, 7.5), 1.8, (200, 196, 170), segments=10, rings=6)])

# ── 第二批 2/2: humanoid 战士系 + slime 精英系 (6 种) ──
_sig("tank",  # 厚肩甲+铁面罩+塔盾 (torso 签名)
     [lambda c: part_box("iron_mask", (0.0, 4.6, 2.4), (9.0, 1.0, 5.0), IRON, bevel=0.3),
      lambda c: part_box("mask_slit", (0.0, 5.0, 2.4), (7.0, 0.6, 0.8), c["eye"], bevel=0.1)],
     torso=[lambda c: part_box("tower_shield", (-9.0, 6.2, 2.0), (2.0, 1.2, 16.0), IRON, bevel=0.4),
            lambda c: part_sphere("shield_boss", (-9.0, 6.6, 2.0), 2.0, GOLD, segments=9, rings=5)])
_sig("charger",  # 铁刺+冲锋斧
     [lambda c: part_prism("helm_spike", (0.0, 4.0, 6.4), [(-2, 0), (2, 0), (0, 4)], 0.9, IRON, bevel=0.1),
      lambda c: part_sphere("charge_eye", (0.0, 4.8, 2.6), 1.1, c["eye"], segments=9, rings=5)],
     weapon=[lambda c: part_prism("war_axe", (0.0, 6.0, 0.0), [(-3, 3), (3, 3), (2, -3), (-2, -3)], 1.2, IRON, bevel=0.2),
             lambda c: part_box("axe_grip", (0.0, 5.6, -5.0), (1.0, 0.8, 6.0), c["main"], bevel=0.1)])
_sig("bomber",  # 圆腹+引信+炸弹背囊 (torso 签名)
     [lambda c: part_sphere("bomb_head", (0.0, 4.0, 2.0), 3.0, c["main"], segments=12, rings=7),
      lambda c: part_cylinder("fuse", (0.0, 4.6, 6.0), 0.5, 3.0, (220, 200, 120), vertices=6)],
     torso=[lambda c: part_sphere("bomb_belly", (0.0, 6.4, 0.0), 5.0, c["main"], segments=14, rings=8),
            lambda c: part_cylinder("bomb_fuse", (0.0, 6.6, 8.0), 0.6, 4.0, (220, 200, 120), vertices=6)])
_sig("elite_orc",  # 铁灰重甲+肩刺+獠牙
     [lambda c: part_prism("elite_tusk_l", (-3.4, 4.8, 1.0), [(-1, 0), (1, 0), (0, -4)], 0.8, (230, 224, 200), bevel=0.1),
      lambda c: part_prism("elite_tusk_r", (3.4, 4.8, 1.0), [(-1, 0), (1, 0), (0, -4)], 0.8, (230, 224, 200), bevel=0.1),
      lambda c: part_box("elite_crest", (0.0, 4.0, 6.4), (2.0, 1.0, 2.4), IRON, bevel=0.2)],
     weapon=[lambda c: part_prism("elite_blade", (0.0, 6.0, 0.0), [(-2, 0), (2, 0), (0, 11)], 1.1, IRON, bevel=0.2),
             lambda c: part_sphere("elite_rune", (0.0, 6.2, 4.0), 1.2, c["eye"], segments=9, rings=5)])
_sig("frost_slime",  # 冰核+顶部三根冰晶尖 (slime head 窄: x±3 z[-0.4,4.2])
     [lambda c: part_prism("ice_tip_1", (0.0, 5.4, 3.2), [(-1, 0), (1, 0), (0, 3)], 0.6, (180, 230, 255), bevel=0.1),
      lambda c: part_prism("ice_tip_2", (-1.8, 5.0, 2.4), [(-1, 0), (1, 0), (0, 2)], 0.5, (180, 230, 255), bevel=0.1),
      lambda c: part_sphere("ice_core", (0.0, 5.6, 1.0), 1.2, (180, 230, 255), segments=10, rings=6)])
_sig("elite_slime",  # 金冠三尖+冠珠红
     [lambda c: part_prism("crown_1", (0.0, 5.4, 3.0), [(-1, 0), (1, 0), (0, 3)], 0.6, GOLD, bevel=0.1),
      lambda c: part_prism("crown_2", (-1.8, 5.0, 2.4), [(-1, 0), (1, 0), (0, 2)], 0.5, GOLD, bevel=0.1),
      lambda c: part_sphere("crown_jewel", (0.0, 5.8, 1.4), 1.0, (230, 60, 50), segments=9, rings=5)])


def _signature_refinements(mon_id):
    """逐槽深拷贝 pilot list, 签名槽整槽替换; 绝不共享可变 list。"""
    pilot = _REFINE_FAMILY[MONSTERS[mon_id][0]]
    merged = {slot: list(details) for slot, details in REFINEMENTS[pilot].items()}
    for slot, details in SIGNATURE_DETAILS[mon_id].items():
        merged[slot] = list(details)
    return merged


for _mon in SIGNATURE_DETAILS:
    REFINEMENTS[_mon] = _signature_refinements(_mon)

for _mon, _meta in MONSTERS.items():
    if _mon in REFINEMENTS:
        continue
    _pilot = _REFINE_FAMILY.get(_meta[0])
    if _pilot:
        REFINEMENTS[_mon] = dict(
            REFINEMENTS[_pilot]
        )  # 浅拷贝, 避免日后按名改动联动全族

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
    out = {slot: builders[slot](name, c) for slot in SLOTS}
    return _refine(name, out)


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
    family = MONSTERS[name][0]
    builders = FAMILIES[family]
    c = _color(name)
    objs = []
    # 每个骨骼锚点新建对象 (leg/arm 有 front/back 两条骨, 复用同一对象会让
    # 第二次 location 覆盖第一次 → 预览里两条腿塌成一条)
    for bone_name, slot, bx, bz in ANCHORS:
        obj = _refine(name, {slot: builders[slot](name, c)}).get(slot)
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
