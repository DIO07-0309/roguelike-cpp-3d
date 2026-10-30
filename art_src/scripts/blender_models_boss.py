# A16-Boss: Boss 手建低模部件 (试点: 暗影骑士)
# 广告牌局部空间: 原点 = 骨骼锚点, x=宽度(右), y=深度(前=+Y 朝相机), z=高度(上)
# 骨骼世界 z (单位, ×ppu1.15=世界): hips=28 torso=36 head=51 arm=49 weapon=42
# 设计原则 (参考 Boss 建模资料): 剪影优先 · 上重下轻(三角力量) · 宽尖肩 · 小头沉肩 ·
#   尖角/三角威胁语言置于末梢 · 巨大武器 · 前倾压迫姿态 · 暗紫+金+红眼 威胁色
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
    """头: 小头盔沉肩 + 深面甲 + 红眼 + 金眉 + 双尖角(威胁) + 中央冠鳍。"""
    parts = [
        part_cylinder("neck", (0, 0, -7), 4.4, 8, PURPLE_DEEP, vertices=12),
        # 小头盔主体 (相对躯干显小)
        part_sphere(
            "helm",
            (0, 0, 4),
            7.0,
            PURPLE,
            segments=18,
            rings=11,
            scale=(1.0, 0.95, 1.06),
        ),
        # 深色面甲 (前)
        part_box("visor", (0, 5.6, 1.0), (11, 3.6, 9.5), DARK, bevel=1.0, taper=0.9),
        # 红眼 (发光窄缝)
        part_sphere(
            "eye_l",
            (-2.8, 7.4, 2.2),
            1.5,
            GLOW,
            segments=10,
            rings=6,
            scale=(1.0, 0.5, 0.8),
        ),
        part_sphere(
            "eye_r",
            (2.8, 7.4, 2.2),
            1.5,
            GLOW,
            segments=10,
            rings=6,
            scale=(1.0, 0.5, 0.8),
        ),
        # 金眉甲 (面甲上缘, 凶)
        part_box(
            "brow",
            (0, 6.2, 5.6),
            (12, 1.8, 2.4),
            GOLD,
            bevel=0.5,
            rotation_x=math.radians(-8),
        ),
        # 双尖角 (尖角威胁语言, 向两侧上外, 三角)
        part_box(
            "horn_l",
            (-6.5, -1.0, 9),
            (4.8, 6.0, 12),
            PURPLE_LT,
            bevel=0.7,
            taper=0.45,
            rotation_z=math.radians(22),
            rotation_x=math.radians(-18),
        ),
        part_box(
            "horn_r",
            (6.5, -1.0, 9),
            (4.8, 6.0, 12),
            PURPLE_LT,
            bevel=0.7,
            taper=0.45,
            rotation_z=math.radians(-22),
            rotation_x=math.radians(-18),
        ),
        # 中央冠鳍 (后掠)
        part_box(
            "crest",
            (0, -1.5, 10.5),
            (2.8, 13, 7.5),
            PURPLE_LT,
            bevel=0.7,
            rotation_x=math.radians(-16),
        ),
        # 侧颊甲
        part_sphere(
            "cheek_l",
            (-6.2, 1.8, 0.5),
            3.6,
            PURPLE_DK,
            segments=12,
            rings=8,
            scale=(0.6, 1.0, 1.15),
        ),
        part_sphere(
            "cheek_r",
            (6.2, 1.8, 0.5),
            3.6,
            PURPLE_DK,
            segments=12,
            rings=8,
            scale=(0.6, 1.0, 1.15),
        ),
    ]
    return join_parts(parts, "boss_shadow_knight_part_head")


def build_boss_part_torso():
    """躯干: 上重下轻(宽胸窄腰) + 巨尖肩甲(金尖) + 金 V 甲片 + 腰带 + 下摆。"""
    parts = [
        # 上胸 (超宽, 强 taper = 三角力量)
        part_box(
            "chest", (0, 0, 7), (32, 18, 17), PURPLE, bevel=1.6, segments=2, taper=0.66
        ),
        # 亮胸甲 (前凸高光)
        part_box(
            "chest_plate", (0, 8.0, 6), (19, 2.4, 13), PURPLE_LT, bevel=1.0, taper=0.82
        ),
        # 金 V 甲片
        part_box(
            "vtrim_l",
            (-3.4, 9.0, 7.0),
            (8.0, 1.6, 2.2),
            GOLD,
            bevel=0.4,
            rotation_z=math.radians(32),
        ),
        part_box(
            "vtrim_r",
            (3.4, 9.0, 7.0),
            (8.0, 1.6, 2.2),
            GOLD,
            bevel=0.4,
            rotation_z=math.radians(-32),
        ),
        # 窄腰 + 深色腹
        part_box("waist", (0, 0, -6), (16, 12, 10), PURPLE_DEEP, bevel=0.9, taper=0.95),
        # 金腰带 + 扣
        part_box("belt", (0, 0, -10), (18, 13, 3.4), GOLD, bevel=0.6),
        part_box("buckle", (0, 7.2, -10), (4.4, 1.6, 3.6), PURPLE_LT, bevel=0.5),
        # 巨尖肩甲 (超宽, 三角) — 主剪影
        part_box(
            "pauldron_l",
            (-18.0, 0, 8),
            (12, 16, 13),
            PURPLE,
            bevel=1.3,
            taper=0.5,
            rotation_z=math.radians(24),
        ),
        part_box(
            "pauldron_r",
            (18.0, 0, 8),
            (12, 16, 13),
            PURPLE,
            bevel=1.3,
            taper=0.5,
            rotation_z=math.radians(-24),
        ),
        # 金肩尖 (威胁语言置于末梢, 宽三角板)
        part_box(
            "spike_l",
            (-19.5, 0, 14),
            (5.5, 9.0, 13),
            GOLD,
            bevel=0.5,
            taper=0.4,
            rotation_z=math.radians(30),
        ),
        part_box(
            "spike_r",
            (19.5, 0, 14),
            (5.5, 9.0, 13),
            GOLD,
            bevel=0.5,
            taper=0.4,
            rotation_z=math.radians(-30),
        ),
        # 上下摆
        part_box("hem", (0, 0, -13), (18, 13, 3.2), PURPLE_DK, bevel=0.7),
    ]
    return join_parts(parts, "boss_shadow_knight_part_torso")


def build_boss_part_arm():
    """臂: 粗肩球 + 粗上臂 + 肘 + 前臂 + 金护腕 + 大拳 (原点=肩骨)。"""
    parts = [
        part_sphere("shoulder", (0, 0, -2), 4.0, PURPLE, segments=14, rings=8),
        part_cylinder(
            "upper",
            (0, 0, -9),
            3.6,
            13,
            PURPLE,
            vertices=12,
            radius_top=4.0,
            smooth=True,
            bevel=0.4,
        ),
        part_sphere("elbow", (0, 0, -16), 3.6, PURPLE_DK, segments=12, rings=8),
        part_cylinder(
            "forearm",
            (0, 0, -21),
            3.2,
            10,
            PURPLE_DK,
            vertices=12,
            radius_top=3.6,
            smooth=True,
            bevel=0.4,
        ),
        part_cylinder("cuff", (0, 0, -25.5), 3.7, 2.6, GOLD, vertices=14, bevel=0.4),
        part_sphere(
            "fist",
            (0, 0, -29),
            3.4,
            PURPLE,
            segments=12,
            rings=8,
            scale=(1.05, 1.05, 1.0),
        ),
    ]
    # 前移: 骨骼 arm 骨靠中(x=-4/6), 陷进躯干; 前移显于胸前 (举剑姿态)
    return _shift(join_parts(parts, "boss_shadow_knight_part_arm"), dy=5.0)


def build_boss_part_leg():
    """腿: 粗大腿锥台 + 膝甲 + 小腿 + 大靴(金边) (原点=髋骨)。"""
    parts = [
        part_cylinder(
            "thigh",
            (0, 0, -9),
            6.0,
            15,
            PURPLE,
            vertices=12,
            radius_top=5.2,
            smooth=True,
            bevel=0.5,
        ),
        part_sphere(
            "knee",
            (0, 0.8, -17),
            4.8,
            PURPLE_LT,
            segments=12,
            rings=8,
            scale=(1.0, 1.0, 0.85),
        ),
        part_cylinder(
            "shin",
            (0, 0, -23),
            4.8,
            10,
            PURPLE_DK,
            vertices=12,
            radius_top=4.0,
            smooth=True,
            bevel=0.5,
        ),
        part_box("boot", (0, 2.2, -27), (13, 16, 6), PURPLE_DEEP, bevel=0.8),
        part_box("boot_toe", (0, 8.0, -28), (9, 4.5, 3.6), DARK, bevel=0.5),
        part_box("boot_trim", (0, 2.2, -23.8), (13, 16, 1.6), GOLD, bevel=0.3),
    ]
    return join_parts(parts, "boss_shadow_knight_part_leg")


def build_boss_part_weapon():
    """巨剑: 金柄尾 + 深柄 + 金十字护手 + 宽暗刃(冷银脊) (原点=握柄顶)。"""
    parts = [
        part_cylinder("grip", (0, 0, -5), 2.6, 10, PURPLE_DEEP, vertices=10, bevel=0.3),
        part_sphere("pommel", (0, 0, 0), 2.8, GOLD),
        part_box("guard", (0, 0, -11), (24, 4.0, 3.6), GOLD, bevel=0.7),
        part_sphere("guard_mid", (0, 0, -11), 3.0, PURPLE_LT),
        part_cylinder(
            "blade",
            (0, 0, -27),
            0.8,
            30,
            PURPLE_DK,
            vertices=6,
            radius_top=4.6,
            smooth=False,
            bevel=0.4,
        ),
        part_box("ridge", (0, 4.2, -27), (2.0, 1.6, 28), STEEL, bevel=0.3),
    ]
    return _shift(join_parts(parts, "boss_shadow_knight_part_weapon"), dy=11.0)


BOSS_PART_BUILDERS = {
    "head": build_boss_part_head,
    "torso": build_boss_part_torso,
    "arm": build_boss_part_arm,
    "leg": build_boss_part_leg,
    "weapon": build_boss_part_weapon,
}
