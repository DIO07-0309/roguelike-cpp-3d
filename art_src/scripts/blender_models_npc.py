# A14: NPC 低模建模 (Blender, Z-up, 1 单位 = 1 世界像素, 身高 ~46)
# 正面朝 -Y (导出后 = 引擎 +Z, 正对相机)
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

SKIN = (233, 178, 132)
SKIN_SHADE = (206, 152, 112)
SKIN_WARM = (222, 160, 118)
HAIR_BROWN = (132, 86, 46)
HAIR_DARK = (78, 52, 32)
HAIR_GREY = (150, 146, 140)
CLOTH_WHITE = (233, 236, 244)
CLOTH_WHITE_SHADE = (203, 210, 226)
CLOTH_BLUE = (70, 104, 158)
CLOTH_TAN = (176, 112, 56)
CLOTH_TAN_SHADE = (146, 90, 44)
CLOTH_PURPLE = (126, 64, 162)
CLOTH_PURPLE_SHADE = (98, 46, 128)
CLOTH_DARK = (92, 60, 34)
TROUSER = (74, 62, 54)
BOOT = (58, 48, 44)
SOLE = (38, 32, 30)
METAL = (156, 166, 178)
METAL_DARK = (112, 120, 132)
GOLD = (216, 172, 62)
EYE_WHITE = (240, 242, 246)
EYE_DARK = (46, 34, 30)
MOUTH = (168, 96, 82)
MOUTH_DARK = (128, 68, 58)
BUTTON = (198, 168, 96)
POUCH = (120, 80, 46)


def _boot(parts, x, boot, sole):
    parts.append(part_box("boot", (x, -1.0, 2.4), (7.0, 11.0, 4.6), boot, bevel=1.0))
    parts.append(part_box("toe", (x, -5.4, 2.0), (6.4, 3.6, 3.4), boot, bevel=0.9))
    parts.append(part_box("sole", (x, -1.4, 0.4), (7.4, 12.0, 0.9), sole, bevel=0.25))
    parts.append(part_box("strap", (x, -0.6, 4.4), (7.4, 3.0, 1.0), sole, bevel=0.2))


def _leg(parts, x, trouser, boot, sole):
    parts.append(
        part_cylinder(
            "leg",
            (x, 0.6, 10.2),
            3.0,
            15.4,
            trouser,
            vertices=14,
            radius_top=2.4,
            smooth=True,
        )
    )
    parts.append(
        part_sphere(
            "knee",
            (x, 0.6, 15.8),
            2.6,
            trouser,
            segments=12,
            rings=6,
            scale=(1.0, 1.0, 0.82),
        )
    )
    _boot(parts, x, boot, sole)


def _hand(parts, x, y, z, skin):
    parts.append(part_box("palm", (x, y, z), (3.6, 4.2, 3.4), skin, bevel=0.9))
    parts.append(
        part_box("fingers", (x, y - 0.4, z - 2.2), (3.4, 3.8, 1.6), skin, bevel=0.5)
    )
    parts.append(
        part_box(
            "thumb",
            (x + (1.9 if x > 0 else -1.9), y - 0.8, z + 0.6),
            (1.4, 1.8, 2.2),
            skin,
            bevel=0.4,
        )
    )


def _arm(parts, x, cloth, sleeve, skin, cuff_color, tilt=0.0):
    sign = 1.0 if x > 0 else -1.0
    parts.append(
        part_sphere("shoulder", (x, 0.0, 29.6), 3.6, cloth, segments=14, rings=8)
    )
    parts.append(
        part_box(
            "shoulder_seam", (x, -0.2, 31.4), (4.6, 5.0, 1.2), cuff_color, bevel=0.4
        )
    )
    parts.append(
        part_cylinder(
            "upper_arm",
            (x + tilt * 0.8, 0.0, 25.8),
            2.7,
            9.8,
            sleeve,
            vertices=12,
            radius_top=2.2,
            smooth=True,
        )
    )
    parts.append(
        part_sphere(
            "elbow", (x + tilt * 1.6, -0.2, 21.4), 2.3, sleeve, segments=10, rings=6
        )
    )
    parts.append(
        part_cylinder(
            "forearm",
            (x + tilt * 2.2, -0.5, 19.2),
            2.2,
            8.6,
            sleeve,
            vertices=12,
            radius_top=1.8,
            smooth=True,
        )
    )
    parts.append(
        part_box(
            "cuff", (x + tilt * 2.4, -0.5, 15.2), (4.8, 5.0, 2.6), cuff_color, bevel=0.7
        )
    )
    _hand(parts, x + tilt * 2.7, -1.2, 12.6, skin)


def _torso(parts, cloth, cloth_dark, trim, belt_color, buckle_color):
    parts.append(
        part_box(
            "pelvis",
            (0, 0.4, 18.4),
            (11.8, 8.4, 6.6),
            cloth_dark,
            bevel=1.1,
            taper=0.92,
        )
    )
    parts.append(
        part_box(
            "torso",
            (0, 0.2, 24.6),
            (13.2, 9.2, 12.4),
            cloth,
            bevel=1.5,
            segments=3,
            taper=0.88,
        )
    )
    parts.append(
        part_box(
            "chest",
            (0, -0.6, 27.4),
            (11.8, 8.2, 6.0),
            cloth,
            bevel=1.4,
            segments=3,
            taper=0.92,
        )
    )
    # 领口 V 形 (两条斜条)
    for sign in (-1, 1):
        parts.append(
            part_box(
                "collar",
                (sign * 2.0, -4.0, 30.4),
                (5.4, 1.6, 1.6),
                trim,
                bevel=0.3,
                rotation_z=sign * math.radians(28.0),
            )
        )
    parts.append(
        part_box("collar_back", (0, 3.4, 31.0), (7.0, 2.2, 1.8), trim, bevel=0.5)
    )
    # 前襟纽扣
    for index in range(3):
        parts.append(
            part_cylinder(
                "button_%d" % index,
                (0, -4.9, 28.6 - index * 3.2),
                0.7,
                0.6,
                BUTTON,
                vertices=8,
                rotation=Matrix.Rotation(math.radians(90), 3, "X"),
            )
        )
    parts.append(
        part_box("belt", (0, 0.2, 19.4), (12.6, 9.6, 2.8), belt_color, bevel=0.6)
    )
    parts.append(
        part_box("buckle", (0, -4.9, 19.4), (3.4, 1.2, 3.0), buckle_color, bevel=0.5)
    )
    parts.append(
        part_box("belt_tip", (2.6, -4.6, 19.4), (3.0, 0.9, 1.6), belt_color, bevel=0.2)
    )
    parts.append(
        part_box("hem", (0, 0.4, 16.4), (13.0, 9.2, 2.4), cloth_dark, bevel=0.8)
    )


def _face(parts, skin, hair):
    # 颊 / 下巴 / 鼻 (楔形) — 让球颅骨有可读的正面
    for sign in (-1, 1):
        parts.append(
            part_sphere(
                "cheek",
                (sign * 2.7, -4.0, 36.7),
                1.7,
                skin,
                segments=10,
                rings=6,
                scale=(1.0, 0.62, 0.85),
            )
        )
    parts.append(
        part_sphere(
            "chin",
            (0, -4.2, 34.5),
            1.9,
            skin,
            segments=10,
            rings=6,
            scale=(1.05, 0.7, 0.75),
        )
    )
    parts.append(
        part_box(
            "nose",
            (0, -5.0, 37.3),
            (1.7, 2.0, 2.2),
            skin,
            bevel=0.45,
            taper=0.55,
            rotation_x=math.radians(-12.0),
        )
    )
    # 眼 (眼白 + 瞳孔 + 高光) / 眉
    for sign in (-1, 1):
        parts.append(
            part_sphere(
                "eye_white",
                (sign * 2.3, -4.4, 38.5),
                1.3,
                EYE_WHITE,
                segments=12,
                rings=7,
                scale=(1.0, 0.5, 1.0),
            )
        )
        parts.append(
            part_sphere(
                "pupil",
                (sign * 2.3, -5.0, 38.4),
                0.66,
                EYE_DARK,
                segments=10,
                rings=5,
                scale=(1.0, 0.55, 1.0),
            )
        )
        parts.append(
            part_sphere(
                "glint",
                (sign * 2.0, -5.4, 38.8),
                0.22,
                (255, 255, 255),
                segments=6,
                rings=3,
            )
        )
        parts.append(
            part_box(
                "brow",
                (sign * 2.4, -4.3, 40.2),
                (3.4, 1.1, 0.9),
                hair,
                bevel=0.25,
                rotation_z=sign * math.radians(-7.0),
            )
        )
    # 嘴 (上下唇)
    parts.append(
        part_box("lip_up", (0, -4.7, 35.9), (2.6, 0.7, 0.5), MOUTH_DARK, bevel=0.15)
    )
    parts.append(
        part_box("lip_low", (0, -4.6, 35.4), (2.2, 0.7, 0.5), MOUTH, bevel=0.15)
    )
    # 耳 (外耳 + 耳窝)
    for sign in (-1, 1):
        parts.append(
            part_sphere(
                "ear",
                (sign * 5.4, 0.4, 37.4),
                1.6,
                skin,
                segments=10,
                rings=6,
                scale=(0.42, 1.0, 1.25),
            )
        )
        parts.append(
            part_sphere(
                "ear_hole",
                (sign * 5.7, 0.4, 37.4),
                0.7,
                SKIN_SHADE,
                segments=8,
                rings=4,
                scale=(0.4, 0.9, 1.0),
            )
        )


def _head(parts, skin, hair, long_hair=False, beard=False, hood=False):
    parts.append(
        part_cylinder("neck", (0, 0.4, 32.4), 1.9, 3.6, SKIN_SHADE, vertices=12)
    )
    parts.append(
        part_sphere(
            "skull",
            (0, 0.0, 38.6),
            5.5,
            skin,
            segments=20,
            rings=12,
            scale=(0.98, 0.92, 1.04),
        )
    )
    parts.append(
        part_box(
            "jaw",
            (0, -0.8, 34.4),
            (7.6, 8.0, 4.2),
            skin,
            bevel=1.3,
            segments=2,
            taper=0.84,
        )
    )
    _face(parts, skin, hair)
    if beard:
        parts.append(
            part_box("mustache", (0, -4.8, 36.4), (5.2, 1.6, 1.1), hair, bevel=0.35)
        )
        parts.append(
            part_box("beard_chin", (0, -4.0, 33.8), (4.2, 2.6, 2.4), hair, bevel=0.6)
        )
    # 头发: 顶盖 + 刘海 + 后脑 + 发缕 + 鬓角
    parts.append(
        part_sphere(
            "hair_cap",
            (0, 0.8, 41.8),
            5.6,
            hair,
            segments=16,
            rings=9,
            scale=(1.02, 0.96, 0.74),
        )
    )
    parts.append(part_box("fringe", (0, -3.3, 41.9), (9.6, 3.0, 2.8), hair, bevel=0.8))
    parts.append(
        part_box("back_hair", (0, 4.4, 38.4), (9.8, 2.8, 8.8), hair, bevel=0.9)
    )
    for sign in (-1, 1):
        parts.append(
            part_box(
                "sideburn", (sign * 4.6, -2.4, 38.4), (1.6, 3.4, 5.2), hair, bevel=0.5
            )
        )
    if long_hair:
        for sign in (-1, 1):
            parts.append(
                part_box(
                    "side_hair",
                    (sign * 5.0, 1.8, 31.6),
                    (3.2, 6.6, 19.0),
                    hair,
                    bevel=0.9,
                )
            )
        parts.append(
            part_box(
                "back_hair_long", (0, 5.4, 31.0), (8.6, 2.6, 20.0), hair, bevel=0.9
            )
        )
    for index in range(6):
        offset = -3.6 + index * 1.45
        parts.append(
            part_box(
                "strand",
                (offset, -1.6 + abs(offset) * 0.25, 44.2 + 0.45 * (index % 2)),
                (1.5, 3.0, 4.6),
                hair,
                bevel=0.4,
                rotation_z=offset * 0.05,
            )
        )
    if hood:
        parts.append(
            part_sphere(
                "hood",
                (0, 1.2, 39.6),
                6.6,
                hair,
                segments=16,
                rings=10,
                scale=(1.06, 1.06, 0.92),
            )
        )
        parts.append(
            part_box("hood_brim", (0, -4.2, 40.6), (11.0, 2.4, 2.2), hair, bevel=0.7)
        )


def _common(
    name,
    skin,
    hair,
    cloth,
    cloth_dark,
    trim,
    trouser,
    boot,
    belt_color,
    buckle_color,
    long_hair=False,
    beard=False,
    hood=False,
    tilt=0.0,
):
    parts = []
    _leg(parts, -4.2, trouser, boot, SOLE)
    _leg(parts, 4.2, trouser, boot, SOLE)
    _torso(parts, cloth, cloth_dark, trim, belt_color, buckle_color)
    _arm(parts, -7.6, cloth, cloth_dark, skin, trim, -tilt)
    _arm(parts, 7.6, cloth, cloth_dark, skin, trim, tilt)
    _head(parts, skin, hair, long_hair, beard, hood)
    return join_parts(parts, name)


def build_npc_1():
    """村民: 棕发 + 白蓝衣 + 腰包"""
    obj = _common(
        "npc_1",
        SKIN,
        HAIR_BROWN,
        CLOTH_WHITE,
        CLOTH_WHITE_SHADE,
        CLOTH_BLUE,
        TROUSER,
        BOOT,
        (126, 84, 44),
        METAL,
    )
    extra = [
        part_box("pouch", (5.4, -3.4, 17.2), (4.0, 3.0, 4.4), POUCH, bevel=0.7),
        part_box(
            "pouch_flap", (5.4, -4.6, 19.0), (4.2, 1.0, 1.6), (92, 60, 34), bevel=0.3
        ),
    ]
    return join_parts([obj] + extra, "npc_1")


def build_npc_2():
    """行商: 兜帽 + 褐袍 + 围兜 + 挎包 + 腰侧卷轴"""
    obj = _common(
        "npc_2",
        SKIN,
        CLOTH_TAN_SHADE,
        CLOTH_TAN,
        CLOTH_TAN_SHADE,
        CLOTH_DARK,
        (92, 60, 34),
        BOOT,
        (84, 54, 32),
        GOLD,
        hood=True,
    )
    extra = [
        part_box("bib", (0, -4.6, 24.0), (7.4, 1.6, 12.4), (150, 96, 52), bevel=0.8),
        part_box("satchel", (7.4, 3.4, 19.4), (5.8, 4.6, 6.2), POUCH, bevel=0.9),
        part_box(
            "satchel_flap", (7.4, 1.0, 22.2), (6.0, 1.6, 2.8), (84, 54, 30), bevel=0.4
        ),
        part_torus(
            "strap",
            (3.4, 1.0, 26.0),
            5.6,
            0.8,
            (96, 62, 36),
            major_segments=16,
            minor_segments=6,
            rotation=Matrix.Rotation(math.radians(28), 3, "Y"),
        ),
        part_cylinder(
            "scroll",
            (-6.2, -3.6, 18.6),
            1.1,
            5.0,
            (226, 214, 186),
            vertices=10,
            rotation=Matrix.Rotation(math.radians(18), 3, "X"),
        ),
        part_box(
            "scroll_band",
            (-6.2, -3.8, 18.6),
            (2.6, 1.2, 1.0),
            (150, 110, 70),
            bevel=0.2,
        ),
    ]
    return join_parts([obj] + extra, "npc_2")


def build_npc_3():
    """女巫: 长发 + 紫袍 + 披风 + 弯尖帽 + 护符"""
    obj = _common(
        "npc_3",
        (240, 192, 150),
        HAIR_DARK,
        CLOTH_PURPLE,
        CLOTH_PURPLE_SHADE,
        (88, 40, 120),
        (88, 40, 120),
        (62, 46, 66),
        (200, 168, 90),
        GOLD,
        long_hair=True,
    )
    extra = [
        # 披风 (背侧两层)
        part_box(
            "cloak", (0, 4.6, 26.0), (13.6, 2.0, 20.0), CLOTH_PURPLE_SHADE, bevel=1.0
        ),
        part_box(
            "cloak_hem", (0, 4.8, 16.6), (14.2, 2.2, 2.4), (74, 34, 96), bevel=0.6
        ),
        # 弯尖帽: 帽檐 + 三段锥 (逐段偏折)
        part_cylinder(
            "hat_brim",
            (0, 0.2, 44.6),
            8.8,
            1.3,
            (74, 42, 100),
            vertices=20,
            radius_top=8.2,
        ),
        part_cylinder(
            "hat_cone_a",
            (0, 0.4, 48.4),
            5.2,
            7.0,
            (74, 42, 100),
            vertices=18,
            radius_top=3.4,
        ),
        part_cylinder(
            "hat_cone_b",
            (0.6, 0.9, 53.0),
            3.3,
            5.0,
            (74, 42, 100),
            vertices=16,
            radius_top=1.9,
            rotation=Matrix.Rotation(math.radians(9), 3, "Y"),
        ),
        part_cylinder(
            "hat_cone_c",
            (2.0, 1.6, 56.2),
            1.8,
            3.6,
            (74, 42, 100),
            vertices=14,
            radius_top=0.5,
            rotation=Matrix.Rotation(math.radians(22), 3, "Y"),
        ),
        part_box(
            "hat_band", (0, 0.2, 45.8), (10.4, 10.4, 1.4), (52, 28, 74), bevel=0.3
        ),
        part_box("hat_buckle", (0, -4.6, 45.8), (3.2, 1.2, 2.6), GOLD, bevel=0.4),
        # 护符 + 腰带垂饰
        part_torus(
            "amulet_chain",
            (0, -4.8, 29.4),
            2.6,
            0.35,
            GOLD,
            major_segments=14,
            minor_segments=6,
            rotation=Matrix.Rotation(math.radians(90), 3, "X"),
        ),
        part_sphere(
            "amulet", (0, -5.4, 27.2), 1.1, (96, 226, 160), segments=10, rings=6
        ),
        part_box("sash", (0, -4.8, 21.0), (13.2, 1.4, 2.6), (200, 168, 90), bevel=0.4),
    ]
    return join_parts([obj] + extra, "npc_3")


def build_npc_blacksmith():
    """铁匠: 光头 + 胡子 + 皮围裙(带背带) + 铁砧(带角) + 锤 + 臂环"""
    obj = _common(
        "npc_blacksmith",
        SKIN_WARM,
        HAIR_GREY,
        CLOTH_DARK,
        (78, 50, 32),
        (60, 40, 30),
        TROUSER,
        (52, 42, 38),
        (92, 58, 32),
        METAL,
        beard=True,
    )
    extra = [
        part_box("apron", (0, -5.2, 23.0), (11.0, 1.4, 18.4), (122, 78, 44), bevel=0.9),
        part_box(
            "apron_pocket", (0, -6.0, 19.4), (5.2, 1.0, 3.8), (100, 62, 34), bevel=0.4
        ),
        part_box(
            "apron_strap_l",
            (-3.4, -3.4, 30.4),
            (2.0, 1.2, 8.0),
            (100, 62, 34),
            bevel=0.3,
            rotation_z=math.radians(12),
        ),
        part_box(
            "apron_strap_r",
            (3.4, -3.4, 30.4),
            (2.0, 1.2, 8.0),
            (100, 62, 34),
            bevel=0.3,
            rotation_z=math.radians(-12),
        ),
        part_box(
            "armband", (7.6, 0.0, 27.4), (5.4, 5.4, 2.2), (150, 96, 52), bevel=0.5
        ),
        part_box(
            "anvil_base", (16.0, 1.0, 3.0), (11.0, 7.0, 6.0), METAL_DARK, bevel=0.9
        ),
        part_box("anvil_body", (16.0, 1.0, 7.4), (6.4, 5.0, 3.0), METAL, bevel=0.7),
        part_cylinder(
            "anvil_horn",
            (20.6, 1.0, 7.4),
            1.5,
            4.4,
            METAL,
            vertices=12,
            rotation=Matrix.Rotation(math.radians(90), 3, "Y"),
        ),
        part_box(
            "anvil_foot", (16.0, 1.0, 0.6), (12.0, 8.0, 1.2), METAL_DARK, bevel=0.3
        ),
        part_cylinder(
            "hammer_handle",
            (10.6, -3.0, 16.0),
            0.8,
            11.0,
            (132, 88, 48),
            vertices=10,
            rotation=Matrix.Rotation(math.radians(28), 3, "X"),
        ),
        part_box(
            "hammer_grip", (10.9, -3.6, 13.2), (2.0, 2.0, 3.0), (92, 60, 34), bevel=0.4
        ),
        part_box(
            "hammer_head", (10.0, -4.6, 21.2), (4.8, 3.2, 3.4), METAL_DARK, bevel=0.7
        ),
    ]
    return join_parts([obj] + extra, "npc_blacksmith")


# A15-Guard: 守卫互动 NPC（警戒 + 已交互休息）+ 配套场景道具
# 使用现有调色板: CLOTH_BLUE / CLOTH_BLUE_DARK; METAL / METAL_DARK; GOLD
CLOTH_BLUE_DARK = (46, 76, 120)
IRON = METAL  # 铁色盔甲（复用 METAL）
IRON_DARK = METAL_DARK
GOLD_LIGHT = (242, 208, 110)


def build_guard_alert():
    # 警戒状态: 站立持盾, 深蓝制服 + 铁色盔甲 + 头盔 (露脸)
    obj = _common(
        "guard_alert",
        SKIN,
        HAIR_DARK,
        CLOTH_BLUE,  # 制服主色 (深蓝)
        CLOTH_BLUE_DARK,  # 制服暗色
        CLOTH_WHITE,  # 领口/装饰
        TROUSER,
        BOOT,
        (46, 76, 120),  # 腰带 (深蓝)
        GOLD,  # 带扣 (金)
    )
    extra = [
        # 头盔壳: 罩住头顶, 下缘 >39 露出眼睛 (z~38.4)
        part_sphere(
            "guard_helmet",
            (0, 0.8, 43.2),
            6.0,
            IRON_DARK,
            segments=16,
            rings=10,
            scale=(1.08, 1.05, 0.68),
        ),
        # 眉甲 (前额横条)
        part_box("guard_brim", (0, -4.6, 39.8), (10.4, 2.0, 1.6), IRON_DARK, bevel=0.4),
        # 盔顶金色冠饰
        part_box("guard_crest", (0, 0.6, 47.4), (1.6, 4.6, 2.4), GOLD, bevel=0.3),
        # 肩甲 (左右, 罩臂根)
        part_box(
            "guard_pauldron_l",
            (-7.0, -0.2, 30.6),
            (4.6, 6.4, 3.0),
            IRON_DARK,
            bevel=0.7,
        ),
        part_box(
            "guard_pauldron_r", (7.0, -0.2, 30.6), (4.6, 6.4, 3.0), IRON_DARK, bevel=0.7
        ),
        # 胸甲 (金边护胸)
        part_box(
            "guard_cuirass", (0, -3.2, 26.4), (11.0, 2.2, 9.4), IRON_DARK, bevel=0.8
        ),
        part_box(
            "guard_cuirass_trim", (0, -4.4, 30.6), (7.4, 0.8, 1.4), GOLD, bevel=0.3
        ),
        # 盾牌 (左手持, 身前)
        part_box(
            "guard_shield", (-10.2, -5.0, 23.0), (2.0, 7.0, 13.0), IRON_DARK, bevel=0.7
        ),
        part_box(
            "guard_shield_boss", (-10.2, -8.4, 23.0), (1.6, 1.8, 4.6), GOLD, bevel=0.5
        ),
    ]
    return join_parts([obj] + extra, "guard_alert")


def build_guard_interacted():
    # 已交互休息: 放松站姿手持书卷, 服色暗色系, 头盔略松 (下移)
    obj = _common(
        "guard_interacted",
        SKIN,
        HAIR_DARK,
        CLOTH_BLUE_DARK,  # 制服暗色 (已交互)
        CLOTH_BLUE,  # 制服主色
        CLOTH_WHITE,
        TROUSER,
        BOOT,
        (46, 76, 120),
        GOLD,
    )
    extra = [
        # 头盔壳 (下移, 更休闲)
        part_sphere(
            "guard_helmet_relax",
            (0, 0.8, 41.6),
            6.0,
            IRON_DARK,
            segments=16,
            rings=10,
            scale=(1.08, 1.05, 0.62),
        ),
        part_box(
            "guard_brim_relax", (0, -4.6, 39.0), (10.4, 2.0, 1.4), IRON_DARK, bevel=0.4
        ),
        # 肩甲
        part_box(
            "guard_pauldron_l",
            (-7.0, -0.2, 30.6),
            (4.6, 6.4, 3.0),
            IRON_DARK,
            bevel=0.7,
        ),
        part_box(
            "guard_pauldron_r", (7.0, -0.2, 30.6), (4.6, 6.4, 3.0), IRON_DARK, bevel=0.7
        ),
        # 手持书卷 (右手, 身前)
        part_box(
            "guard_scroll", (8.2, -5.4, 20.0), (2.6, 4.4, 5.6), GOLD_LIGHT, bevel=0.5
        ),
        part_box(
            "guard_scroll_band", (8.2, -5.6, 22.2), (2.8, 4.6, 0.9), GOLD, bevel=0.3
        ),
    ]
    return join_parts([obj] + extra, "guard_interacted")


def build_guard_banner():
    # 守卫旗帜: 深蓝布面 + 金色边框, 挂在高杆上 (场景道具)
    # pole center z=20, depth 40 → 底部贴地 z=0, 顶 z=40
    pole = part_cylinder("guard_pole", (0, 0, 20.0), 0.9, 40.0, IRON_DARK, vertices=12)
    flag = part_box(
        "guard_flag", (0, 0.2, 33.0), (14.0, 0.5, 8.0), CLOTH_BLUE, bevel=0.3
    )
    trim = part_box(
        "guard_flag_trim", (0, 0.2, 33.0), (14.6, 0.2, 8.6), GOLD, bevel=0.1
    )
    finial = part_sphere("guard_finial", (0, 0, 41.0), 1.4, GOLD, segments=12, rings=8)
    return join_parts([pole, flag, trim, finial], "guard_banner")
