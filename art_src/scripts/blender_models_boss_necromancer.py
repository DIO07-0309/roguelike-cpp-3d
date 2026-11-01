# A16-Boss: 死灵法师手建低模 (第四个 Boss)
# 局部空间: 原点 = 骨骼锚点, x=宽, y=深度(前=+Y), z=高
# 骨架 ppu 1.15; 法杖骨 x=+10 对齐前手, 法杖位于身体右侧, 不遮脸
# 设计: 枯瘦死灵法师 — 高尖兜帽、骷髅面、层叠破袍、骨手、亡灵绿法杖
import math

from blender_lib import part_box, part_cylinder, part_sphere, join_parts
from blender_models_player import export_part_json

# ── 死灵法师调色板 (采样自 boss_necromancer_part_*.png) ──
MOSS = (80, 112, 80)  # 主袍苔绿 (压暗)
MOSS_LT = (120, 152, 104)  # 袍高光
MOSS_DK = (40, 64, 48)  # 袍暗部
ROBE_DK = (24, 36, 24)  # 深袍/内衬
DARK = (48, 32, 48)  # 兜帽内阴影
BROWN = (96, 80, 64)  # 皮带/木杖
BROWN_DK = (64, 48, 40)  # 木杖暗部
BONE = (200, 200, 176)  # 骷髅/骨饰
GLOW = (140, 255, 176)  # 亡灵绿
CRYSTAL = (190, 245, 200)  # 魂晶

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
    """绕 Y 轴倾斜的长方体 — 用于斜背带/骨爪/破布。"""
    obj = part_box(name, (0, 0, 0), size, color, bevel=bevel, taper=taper)
    angle = math.radians(slant_deg)
    cos_a, sin_a = math.cos(angle), math.sin(angle)
    for v in obj.data.vertices:
        x, z = v.co.x, v.co.z
        v.co.x = x * cos_a + z * sin_a + center[0]
        v.co.z = -x * sin_a + z * cos_a + center[2]
        v.co.y += center[1]
    return obj


def build_necromancer_part_head():
    """头: 高尖兜帽 + 深帽兜阴影 + 骷髅面 + 亡灵绿眼 + 骨冠/帽檐。"""
    parts = [
        part_cylinder("neck", (0, 0, -6), 3.8, 8, MOSS_DK, vertices=12),
        part_sphere(
            "hood",
            (0, -0.5, 3.5),
            7.2,
            MOSS,
            segments=18,
            rings=11,
            scale=(1.08, 1.0, 1.12),
        ),
        part_box(
            "hood_back",
            (0, -6.0, 4.0),
            (10.5, 4.0, 10.0),
            MOSS_DK,
            bevel=0.6,
            taper=0.8,
        ),
        # 帽兜阴影 + 骷髅面
        part_box(
            "hood_face", (0, 5.6, 1.8), (10.0, 3.0, 9.0), DARK, bevel=0.8, taper=0.9
        ),
        part_box("skull", (0, 7.0, 2.0), (7.0, 1.5, 7.5), BONE, bevel=0.5, taper=0.9),
        part_box("eye_socket_l", (-2.4, 7.9, 3.2), (2.6, 1.0, 2.0), DARK, bevel=0.3),
        part_box("eye_socket_r", (2.4, 7.9, 3.2), (2.6, 1.0, 2.0), DARK, bevel=0.3),
        part_box("eye_l", (-2.4, 8.2, 3.2), (1.3, 0.6, 0.8), GLOW, bevel=0.15),
        part_box("eye_r", (2.4, 8.2, 3.2), (1.3, 0.6, 0.8), GLOW, bevel=0.15),
        _slanted_box(
            "brow_bone_l", (-2.4, 8.3, 4.4), (3.2, 1.0, 0.8), BONE, -14.0, bevel=0.1
        ),
        _slanted_box(
            "brow_bone_r", (2.4, 8.3, 4.4), (3.2, 1.0, 0.8), BONE, 14.0, bevel=0.1
        ),
        part_box("nose_hole", (0, 8.1, 1.0), (1.2, 0.8, 1.8), DARK, bevel=0.2),
        part_box(
            "skull_jaw", (0, 7.4, -1.4), (5.6, 1.4, 3.0), BONE, bevel=0.4, taper=0.85
        ),
        part_box("teeth", (0, 8.0, -1.8), (4.0, 0.6, 0.9), DARK, bevel=0.1),
        # 帽檐/兜帽侧片
        part_box(
            "brim_top",
            (0, 6.8, 6.3),
            (13.0, 2.2, 2.2),
            MOSS_DK,
            bevel=0.4,
            taper=0.9,
            rotation_x=math.radians(-6),
        ),
        part_box(
            "brim_l", (-5.6, 5.8, 3.0), (2.6, 3.0, 8.0), MOSS_DK, bevel=0.4, taper=0.8
        ),
        part_box(
            "brim_r", (5.6, 5.8, 3.0), (2.6, 3.0, 8.0), MOSS_DK, bevel=0.4, taper=0.8
        ),
        # 高尖帽 + 两侧垂布
        part_box(
            "hood_peak",
            (0, -1.5, 11.5),
            (4.8, 6.5, 9.0),
            MOSS_LT,
            bevel=0.4,
            taper=0.15,
            rotation_x=math.radians(-8),
        ),
        part_box(
            "hood_tail_l",
            (-7.2, -1.0, 1.0),
            (2.8, 4.0, 9.0),
            MOSS_DK,
            bevel=0.4,
            taper=0.35,
            rotation_z=math.radians(10),
        ),
        part_box(
            "hood_tail_r",
            (7.2, -1.0, 1.0),
            (2.8, 4.0, 9.0),
            MOSS_DK,
            bevel=0.4,
            taper=0.35,
            rotation_z=math.radians(-10),
        ),
        # 骨冠 + 魂晶
        part_box("bone_circlet", (0, 7.8, 6.2), (8.5, 1.2, 1.2), BONE, bevel=0.2),
        part_sphere("circlet_gem", (0, 8.4, 6.2), 0.75, GLOW, segments=8, rings=4),
        part_box(
            "circlet_spike_l",
            (-4.8, 7.0, 7.4),
            (1.0, 1.0, 3.0),
            BONE,
            bevel=0.15,
            taper=0.2,
            rotation_z=math.radians(-20),
        ),
        part_box(
            "circlet_spike_r",
            (4.8, 7.0, 7.4),
            (1.0, 1.0, 3.0),
            BONE,
            bevel=0.15,
            taper=0.2,
            rotation_z=math.radians(20),
        ),
        # 帽布破口
        _slanted_box(
            "hood_tear_l",
            (-5.0, 4.0, 7.5),
            (1.0, 1.0, 3.5),
            ROBE_DK,
            -18.0,
            bevel=0.1,
            taper=0.3,
        ),
        _slanted_box(
            "hood_tear_r",
            (4.6, 3.8, 6.0),
            (0.9, 1.0, 3.0),
            ROBE_DK,
            15.0,
            bevel=0.1,
            taper=0.3,
        ),
    ]
    return join_parts(parts, "boss_necromancer_part_head")


def build_necromancer_part_torso():
    """躯干: 层叠破袍 + 高立领 + 宽肩垂布 + 斜背带 + 骷髅腰扣 + 破布下摆。"""
    parts = [
        part_box(
            "chest", (0, 0, 7), (30, 18, 18), MOSS, bevel=1.2, segments=2, taper=0.65
        ),
        # 高立领 (左右两片 + 后领)
        _slanted_box(
            "collar_l",
            (-5.6, 5.2, 14.0),
            (5.8, 4.0, 9.0),
            MOSS_LT,
            -18.0,
            bevel=0.4,
            taper=0.65,
        ),
        _slanted_box(
            "collar_r",
            (5.6, 5.2, 14.0),
            (5.8, 4.0, 9.0),
            MOSS_LT,
            18.0,
            bevel=0.4,
            taper=0.65,
        ),
        part_box(
            "collar_back",
            (0, -4.0, 15.0),
            (12.0, 5.0, 5.0),
            MOSS_DK,
            bevel=0.4,
            taper=0.8,
        ),
        # 宽肩垂布
        part_box(
            "shoulder_l",
            (-16.5, 0, 9.0),
            (12.0, 15.0, 9.0),
            MOSS_DK,
            bevel=0.8,
            taper=0.5,
            rotation_z=math.radians(12),
        ),
        part_box(
            "shoulder_r",
            (16.5, 0, 9.0),
            (12.0, 15.0, 9.0),
            MOSS_DK,
            bevel=0.8,
            taper=0.5,
            rotation_z=math.radians(-12),
        ),
        part_box(
            "shoulder_trim_l", (-16.5, 7.6, 9.0), (10.0, 1.2, 2.0), MOSS_LT, bevel=0.2
        ),
        part_box(
            "shoulder_trim_r", (16.5, 7.6, 9.0), (10.0, 1.2, 2.0), MOSS_LT, bevel=0.2
        ),
        # 前襟层叠
        part_box(
            "robe_front",
            (0, 9.0, 4.0),
            (18.0, 2.2, 14.0),
            MOSS_LT,
            bevel=0.6,
            taper=0.82,
        ),
        part_box(
            "robe_inner",
            (0, 8.8, -5.0),
            (12.0, 1.8, 8.0),
            MOSS_DK,
            bevel=0.5,
            taper=0.9,
        ),
        # 斜背带 + 骷髅扣
        _slanted_box("strap", (0, 10.3, 7.0), (24.0, 1.6, 3.4), BROWN, 28.0, bevel=0.2),
        part_box("strap_buckle", (0, 11.0, 7.0), (4.2, 1.2, 3.8), BROWN_DK, bevel=0.2),
        part_sphere(
            "strap_skull",
            (0, 11.7, 7.0),
            1.5,
            BONE,
            segments=12,
            rings=8,
            scale=(1.0, 0.5, 1.0),
        ),
        part_box("strap_eye_l", (-0.55, 12.1, 7.2), (0.45, 0.4, 0.55), DARK, bevel=0.1),
        part_box("strap_eye_r", (0.55, 12.1, 7.2), (0.45, 0.4, 0.55), DARK, bevel=0.1),
        # 腰 + 骷髅腰扣
        part_box(
            "waist", (0, 0, -6.0), (18.0, 13.0, 9.0), ROBE_DK, bevel=0.6, taper=0.95
        ),
        part_box("belt", (0, 0, -9.5), (20.0, 13.5, 3.4), BROWN_DK, bevel=0.4),
        part_sphere(
            "belt_skull",
            (0, 7.6, -9.5),
            1.8,
            BONE,
            segments=12,
            rings=8,
            scale=(1.0, 0.5, 1.0),
        ),
        part_box("belt_eye_l", (-0.65, 8.1, -9.3), (0.5, 0.4, 0.6), DARK, bevel=0.1),
        part_box("belt_eye_r", (0.65, 8.1, -9.3), (0.5, 0.4, 0.6), DARK, bevel=0.1),
        # 破布下摆 (长短不齐)
        part_box(
            "hem", (0, 0, -14.0), (20.0, 13.0, 4.0), ROBE_DK, bevel=0.5, taper=0.95
        ),
        part_box(
            "hem_l2", (-8.0, 0, -16.5), (3.2, 11.0, 6.0), MOSS_DK, bevel=0.3, taper=0.7
        ),
        part_box(
            "hem_l1", (-4.0, 0, -17.5), (3.0, 11.0, 8.0), ROBE_DK, bevel=0.3, taper=0.65
        ),
        part_box(
            "hem_c", (0, 0, -18.5), (3.4, 11.5, 10.0), MOSS_DK, bevel=0.3, taper=0.6
        ),
        part_box(
            "hem_r1", (4.0, 0, -17.0), (3.0, 11.0, 7.0), ROBE_DK, bevel=0.3, taper=0.65
        ),
        part_box(
            "hem_r2", (8.0, 0, -16.0), (3.2, 11.0, 5.0), MOSS_DK, bevel=0.3, taper=0.7
        ),
        # 背后破披风
        part_box(
            "cape_back",
            (0, -8.0, 2.0),
            (21.0, 2.0, 20.0),
            MOSS_DK,
            bevel=0.4,
            taper=0.78,
        ),
        # 袍面亡灵绿符文
        _slanted_box(
            "rune_l", (-5.0, 10.2, 3.0), (0.9, 0.8, 6.0), GLOW, -12.0, bevel=0.1
        ),
        _slanted_box(
            "rune_r", (5.0, 10.2, 1.0), (0.8, 0.8, 5.0), GLOW, 10.0, bevel=0.1
        ),
    ]
    return join_parts(parts, "boss_necromancer_part_torso")


def build_necromancer_part_arm():
    """臂: 宽大垂袖 + 破布袖口 + 枯骨前臂/手 + 亡灵绿腕环 + 骨指。"""
    parts = [
        part_sphere("shoulder", (0, 0, -2), 4.2, MOSS, segments=14, rings=8),
        part_cylinder(
            "sleeve",
            (0, 0, -8),
            5.4,
            14,
            MOSS_LT,
            vertices=12,
            radius_top=4.0,
            smooth=True,
            bevel=0.4,
        ),
        part_cylinder("cuff", (0, 0, -15.0), 5.5, 1.5, MOSS_DK, vertices=12),
        part_box(
            "tear_l", (-3.4, 0, -18.0), (1.8, 3.0, 5.0), MOSS_DK, bevel=0.2, taper=0.4
        ),
        part_box(
            "tear_c", (0.0, 0, -19.0), (2.0, 3.2, 6.0), ROBE_DK, bevel=0.2, taper=0.4
        ),
        part_box(
            "tear_r", (3.4, 0, -18.0), (1.8, 3.0, 5.0), MOSS_DK, bevel=0.2, taper=0.4
        ),
        part_sphere("elbow", (0, 0, -17.0), 1.8, BONE, segments=10, rings=6),
        part_cylinder(
            "forearm",
            (0, 0, -21.0),
            1.4,
            8,
            BONE,
            vertices=10,
            radius_top=1.6,
            smooth=True,
            bevel=0.2,
        ),
        part_cylinder("wrist_ring", (0, 0, -24.0), 1.8, 1.0, GLOW, vertices=10),
        part_sphere(
            "hand",
            (0, 0, -26.0),
            2.0,
            BONE,
            segments=10,
            rings=6,
            scale=(1.0, 1.0, 1.1),
        ),
        # 三根骨指
        part_cylinder(
            "finger_a",
            (-1.2, 1.0, -28.5),
            0.45,
            4.0,
            BONE,
            vertices=6,
            radius_top=0.15,
            smooth=False,
        ),
        part_cylinder(
            "finger_b",
            (0.0, 1.3, -29.2),
            0.5,
            4.8,
            BONE,
            vertices=6,
            radius_top=0.15,
            smooth=False,
        ),
        part_cylinder(
            "finger_c",
            (1.2, 1.0, -28.5),
            0.45,
            4.0,
            BONE,
            vertices=6,
            radius_top=0.15,
            smooth=False,
        ),
        part_cylinder(
            "thumb",
            (-1.8, 0.5, -26.5),
            0.4,
            3.0,
            BONE,
            vertices=6,
            radius_top=0.15,
            smooth=False,
        ),
        part_box("sleeve_rune", (0, 4.8, -9.0), (0.8, 0.8, 5.0), GLOW, bevel=0.1),
    ]
    return _shift(join_parts(parts, "boss_necromancer_part_arm"), dy=7.0)


def build_necromancer_part_leg():
    """腿: 袍腿 + 破布踝口 + 骨趾长靴。"""
    parts = [
        part_cylinder(
            "thigh",
            (0, 0, -9),
            5.5,
            15,
            MOSS_DK,
            vertices=12,
            radius_top=5.0,
            smooth=True,
            bevel=0.5,
        ),
        part_box(
            "thigh_fold", (0, 4.8, -9), (7.5, 1.6, 11.0), MOSS, bevel=0.4, taper=0.85
        ),
        part_cylinder(
            "shin",
            (0, 0, -22),
            4.6,
            10,
            ROBE_DK,
            vertices=12,
            radius_top=4.2,
            smooth=True,
            bevel=0.4,
        ),
        part_box(
            "ankle_tear_l",
            (-3.0, 0, -25.5),
            (1.6, 3.0, 4.0),
            MOSS_DK,
            bevel=0.2,
            taper=0.4,
        ),
        part_box(
            "ankle_tear_c",
            (0.0, 0, -26.0),
            (1.8, 3.2, 5.0),
            ROBE_DK,
            bevel=0.2,
            taper=0.4,
        ),
        part_box(
            "ankle_tear_r",
            (3.0, 0, -25.5),
            (1.6, 3.0, 4.0),
            MOSS_DK,
            bevel=0.2,
            taper=0.4,
        ),
        part_box(
            "boot", (0, 2.4, -27.5), (9.0, 12.0, 5.0), ROBE_DK, bevel=0.5, taper=0.9
        ),
        part_box(
            "boot_top", (0, 2.4, -25.0), (8.0, 10.0, 1.4), MOSS_DK, bevel=0.2, taper=0.9
        ),
        part_cylinder(
            "toe_a",
            (-2.4, 7.5, -28.5),
            0.8,
            4.0,
            BONE,
            vertices=6,
            radius_top=0.15,
            smooth=False,
        ),
        part_cylinder(
            "toe_b",
            (0.0, 8.0, -28.5),
            0.9,
            4.8,
            BONE,
            vertices=6,
            radius_top=0.15,
            smooth=False,
        ),
        part_cylinder(
            "toe_c",
            (2.4, 7.5, -28.5),
            0.8,
            4.0,
            BONE,
            vertices=6,
            radius_top=0.15,
            smooth=False,
        ),
        part_box("leg_rune", (0, 4.8, -20.0), (0.8, 0.8, 4.0), GLOW, bevel=0.1),
    ]
    return join_parts(parts, "boss_necromancer_part_leg")


def build_necromancer_part_weapon():
    """亡灵法杖: 扭曲木杖 + 骨爪托魂晶 + 骷髅饰 + 绿符文 + 柄尾骨刺。"""
    parts = [
        part_cylinder("shaft", (0, 0, -1), 1.4, 62, BROWN, vertices=10, bevel=0.2),
        part_cylinder("shaft_knot_a", (0, 0, -16.0), 1.9, 1.6, BROWN_DK, vertices=10),
        part_cylinder("shaft_knot_b", (0, 0, -2.0), 1.8, 1.4, BROWN_DK, vertices=10),
        part_cylinder("shaft_knot_c", (0, 0, 12.0), 1.9, 1.6, BROWN_DK, vertices=10),
        part_cylinder("grip", (0, 0, -22.0), 1.8, 7.0, BROWN_DK, vertices=10),
        # 顶部骨爪 + 棱面魂晶
        part_cylinder(
            "crystal",
            (0, 2.8, 31.5),
            2.4,
            8.0,
            CRYSTAL,
            vertices=6,
            radius_top=0.3,
            smooth=False,
            bevel=0.2,
        ),
        part_cylinder(
            "crystal_core",
            (0, 3.6, 31.5),
            1.1,
            5.5,
            GLOW,
            vertices=6,
            radius_top=0.15,
            smooth=False,
        ),
        _slanted_box(
            "crystal_shard_l",
            (-2.6, 2.8, 28.8),
            (1.5, 1.5, 4.5),
            CRYSTAL,
            -20.0,
            bevel=0.15,
            taper=0.1,
        ),
        _slanted_box(
            "crystal_shard_r",
            (2.6, 2.8, 28.8),
            (1.5, 1.5, 4.5),
            CRYSTAL,
            20.0,
            bevel=0.15,
            taper=0.1,
        ),
        part_box(
            "claw_l",
            (-2.8, 0, 28.0),
            (1.4, 1.8, 8.0),
            BONE,
            bevel=0.2,
            taper=0.2,
            rotation_z=math.radians(-24),
        ),
        part_box(
            "claw_r",
            (2.8, 0, 28.0),
            (1.4, 1.8, 8.0),
            BONE,
            bevel=0.2,
            taper=0.2,
            rotation_z=math.radians(24),
        ),
        part_box("claw_c", (0, 2.8, 28.5), (1.4, 1.8, 7.0), BONE, bevel=0.2, taper=0.2),
        # 骷髅饰 (魂晶下方)
        part_sphere(
            "skull_ornament",
            (0, 2.0, 24.5),
            1.7,
            BONE,
            segments=12,
            rings=8,
            scale=(1.0, 0.7, 1.0),
        ),
        part_box("skull_eye_l", (-0.6, 3.2, 24.8), (0.45, 0.4, 0.55), DARK, bevel=0.1),
        part_box("skull_eye_r", (0.6, 3.2, 24.8), (0.45, 0.4, 0.55), DARK, bevel=0.1),
        # 杖身绿符文
        part_box("rune_a", (0, 1.3, 8.0), (0.8, 0.8, 4.0), GLOW, bevel=0.1),
        part_box("rune_b", (0, 1.3, -6.0), (0.8, 0.8, 3.0), GLOW, bevel=0.1),
        part_box("rune_c", (0, 1.3, -14.0), (0.8, 0.8, 2.5), GLOW, bevel=0.1),
        # 悬挂骨珠
        part_sphere("charm_a", (-2.2, 0, 13.0), 0.7, BONE, segments=8, rings=4),
        part_sphere("charm_b", (-2.2, 0, 10.8), 0.55, BONE, segments=8, rings=4),
        # 柄尾骨刺
        part_cylinder(
            "haft_spike",
            (0, 0, -33.0),
            1.6,
            4.0,
            BONE,
            vertices=6,
            radius_top=0.15,
            smooth=False,
        ),
    ]
    return _shift(join_parts(parts, "boss_necromancer_part_weapon"), dy=10.0)


NECROMANCER_PART_BUILDERS = {
    "head": build_necromancer_part_head,
    "torso": build_necromancer_part_torso,
    "arm": build_necromancer_part_arm,
    "leg": build_necromancer_part_leg,
    "weapon": build_necromancer_part_weapon,
}
