# A16-Boss: Boss 手建低模部件 (试点: 暗影骑士)
# 广告牌局部空间: 原点 = 骨骼锚点, x=宽度(右), y=深度(前=+Y 朝相机), z=高度(上)
# 骨骼世界 z (单位, ×ppu1.15=世界): hips=28 torso=36 head=51 arm=49 weapon=42
# 设计原则 (参考 Boss 建模资料): 剪影优先 · 上重下轻(三角力量) · 宽尖肩 · 小头沉肩 ·
#   尖角/三角威胁语言置于末梢 · 巨大武器 · 前倾压迫姿态 · 暗紫+金+红眼 威胁色
# v2 细节层: 颈甲/胸甲分层+金边+铆钉/肩甲双层/肘膝尖/拳铆/柄环/护手宝石
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
SLIT = (70, 44, 70)  # 面甲缝

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


def _slanted_box(name, center, size, color, slant_deg, bevel=0.0):
    """绕 Y 轴倾斜的长方体 — 用于内倾凶眼/斜甲片 (正面可见 x-z 斜度)。"""
    obj = part_box(name, (0, 0, 0), size, color, bevel=bevel)
    angle = math.radians(slant_deg)
    cos_a, sin_a = math.cos(angle), math.sin(angle)
    for v in obj.data.vertices:
        x, z = v.co.x, v.co.z
        v.co.x = x * cos_a + z * sin_a + center[0]
        v.co.z = -x * sin_a + z * cos_a + center[2]
        v.co.y += center[1]
    return obj


def build_boss_part_head():
    """头: 小头盔沉肩 + 深面甲(中缝) + 红眼 + 金眉(铆钉) + 双后掠尖角 + 冠鳍(宝石) + 颈甲。"""
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
        part_box("visor_slit", (0, 7.7, 1.5), (1.0, 0.9, 8.4), SLIT, bevel=0.2),
        # 红眼 (内倾窄缝 = 凶相; 内侧下压)
        _slanted_box(
            "eye_l", (-2.9, 7.55, 2.2), (4.2, 0.8, 0.75), GLOW, 12.0, bevel=0.18
        ),
        _slanted_box(
            "eye_r", (2.9, 7.55, 2.2), (4.2, 0.8, 0.75), GLOW, -12.0, bevel=0.18
        ),
        # 金眉甲 (面甲上缘, 凶) + 铆钉
        part_box(
            "brow",
            (0, 6.2, 5.6),
            (12, 1.8, 2.4),
            GOLD,
            bevel=0.5,
            rotation_x=math.radians(-8),
        ),
        part_sphere(
            "brow_rivet_l", (-4.2, 6.7, 5.9), 0.55, PURPLE_LT, segments=8, rings=4
        ),
        part_sphere(
            "brow_rivet_c", (0, 6.7, 5.9), 0.55, PURPLE_LT, segments=8, rings=4
        ),
        part_sphere(
            "brow_rivet_r", (4.2, 6.7, 5.9), 0.55, PURPLE_LT, segments=8, rings=4
        ),
        # 双尖角 (加粗后掠, 三角收尖)
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
        # 中央冠鳍 (后掠) + 宝石
        part_box(
            "crest",
            (0, -1.5, 10.5),
            (2.8, 13, 7.5),
            PURPLE_LT,
            bevel=0.7,
            rotation_x=math.radians(-16),
        ),
        part_sphere("crest_gem", (0, -3.4, 12.4), 1.0, GLOW, segments=10, rings=6),
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
        # 颈甲 (gorget)
        part_box("gorget", (0, 0, -6), (13, 11.5, 3.4), PURPLE_DK, bevel=0.7),
        part_box("gorget_trim", (0, 5.6, -6.0), (12, 1.4, 2.6), GOLD, bevel=0.4),
    ]
    return join_parts(parts, "boss_shadow_knight_part_head")


def build_boss_part_torso():
    """躯干: 上重下轻(宽胸窄腰) + 巨尖肩甲(双层+金边铆) + 金 V 甲片 + 分层腹甲 + 腰带 + 下摆。"""
    parts = [
        # 上胸 (超宽, 强 taper = 三角力量)
        part_box(
            "chest", (0, 0, 7), (32, 18, 17), PURPLE, bevel=1.6, segments=2, taper=0.66
        ),
        # 亮胸甲 (前凸高光) + 下胸层 + 金边
        part_box(
            "chest_plate", (0, 8.0, 6), (19, 2.4, 13), PURPLE_LT, bevel=1.0, taper=0.82
        ),
        part_box(
            "chest_plate2",
            (0, 7.6, -1.5),
            (15, 2.0, 6),
            PURPLE_LT,
            bevel=0.7,
            taper=0.9,
        ),
        part_box("plate_trim_l", (-8.8, 9.1, 6), (1.4, 1.1, 13.5), GOLD, bevel=0.3),
        part_box("plate_trim_r", (8.8, 9.1, 6), (1.4, 1.1, 13.5), GOLD, bevel=0.3),
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
        # 窄腰 + 深色腹 + 分层腹甲
        part_box("waist", (0, 0, -6), (16, 12, 10), PURPLE_DEEP, bevel=0.9, taper=0.95),
        part_box("ab1", (0, 6.6, -4.5), (12.5, 1.2, 2.4), PURPLE, bevel=0.4),
        part_box("ab2", (0, 6.4, -7.0), (11.5, 1.2, 2.4), PURPLE, bevel=0.4),
        # 金腰带 + 扣
        part_box("belt", (0, 0, -10), (18, 13, 3.4), GOLD, bevel=0.6),
        part_box("buckle", (0, 7.2, -10), (4.4, 1.6, 3.6), PURPLE_LT, bevel=0.5),
        # 巨尖肩甲 (超宽, 三角) + 第二层 + 金边 + 铆
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
        part_box(
            "pauldron2_l",
            (-16.5, 0, 2.5),
            (9.5, 12, 6.5),
            PURPLE_DK,
            bevel=1.0,
            taper=0.6,
            rotation_z=math.radians(24),
        ),
        part_box(
            "pauldron2_r",
            (16.5, 0, 2.5),
            (9.5, 12, 6.5),
            PURPLE_DK,
            bevel=1.0,
            taper=0.6,
            rotation_z=math.radians(-24),
        ),
        part_sphere("p_rivet_l", (-12.8, 6.0, 8.5), 0.65, GOLD, segments=8, rings=4),
        part_sphere("p_rivet_r", (12.8, 6.0, 8.5), 0.65, GOLD, segments=8, rings=4),
        # 金肩尖 (宽三角板)
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
        # 颈甲 + 下摆
        part_box("gorget", (0, 0, 15.5), (14, 12, 3.0), PURPLE_DK, bevel=0.7),
        part_box("hem", (0, 0, -13), (18, 13, 3.2), PURPLE_DK, bevel=0.7),
        # 暗紫破披风 (背后, 增加剪影层次)
        part_box(
            "cape",
            (0, -8.5, -3.0),
            (20.0, 2.0, 22.0),
            PURPLE_DEEP,
            bevel=0.5,
            taper=0.75,
        ),
        part_box(
            "cape_tear_l",
            (-6.0, -8.5, -16.0),
            (3.0, 1.8, 7.0),
            PURPLE_DK,
            bevel=0.2,
            taper=0.35,
        ),
        part_box(
            "cape_tear_c",
            (0.0, -8.5, -17.5),
            (3.2, 1.8, 9.0),
            PURPLE_DEEP,
            bevel=0.2,
            taper=0.3,
        ),
        part_box(
            "cape_tear_r",
            (6.0, -8.5, -16.0),
            (3.0, 1.8, 7.0),
            PURPLE_DK,
            bevel=0.2,
            taper=0.35,
        ),
        part_sphere(
            "cape_clasp_l", (-10.0, -7.0, 13.0), 1.0, GOLD, segments=8, rings=4
        ),
        part_sphere("cape_clasp_r", (10.0, -7.0, 13.0), 1.0, GOLD, segments=8, rings=4),
        # 胸甲金线纹饰
        _slanted_box(
            "filigree_l", (-5.0, 9.7, 4.0), (0.9, 0.8, 8.0), GOLD, -10.0, bevel=0.1
        ),
        _slanted_box(
            "filigree_r", (5.0, 9.7, 4.0), (0.9, 0.8, 8.0), GOLD, 10.0, bevel=0.1
        ),
        part_sphere("chest_rivet_l", (-7.5, 9.7, 8.0), 0.55, GOLD, segments=8, rings=4),
        part_sphere("chest_rivet_r", (7.5, 9.7, 8.0), 0.55, GOLD, segments=8, rings=4),
    ]
    return join_parts(parts, "boss_shadow_knight_part_torso")


def build_boss_part_arm():
    """臂: 粗肩球 + 上臂 + 肘(尖) + 前臂(环) + 金护腕 + 大拳(铆钉)。"""
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
        part_box(
            "elbow_spike",
            (0, -3.6, -15.5),
            (2.2, 2.2, 5),
            GOLD,
            bevel=0.4,
            rotation_x=math.radians(90),
        ),
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
        part_cylinder("fore_ring_a", (0, 0, -19.4), 3.7, 1.0, PURPLE, vertices=12),
        part_cylinder("fore_ring_b", (0, 0, -23.0), 3.5, 1.0, PURPLE, vertices=12),
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
        part_sphere("knuckle_a", (-1.3, -3.2, -29.5), 0.6, GOLD, segments=8, rings=4),
        part_sphere("knuckle_b", (1.3, -3.2, -29.5), 0.6, GOLD, segments=8, rings=4),
    ]
    # 前移: 骨骼 arm 骨靠中(x=-4/6), 陷进躯干; 前移显于胸前 (举剑姿态)
    return _shift(join_parts(parts, "boss_shadow_knight_part_arm"), dy=5.0)


def build_boss_part_leg():
    """腿: 粗大腿(甲片) + 膝甲(尖) + 小腿 + 大靴(金边铆) (原点=髋骨)。"""
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
        part_box(
            "thigh_plate",
            (0, 5.4, -9),
            (9.5, 2.0, 12),
            PURPLE_LT,
            bevel=0.7,
            taper=0.88,
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
        part_box(
            "knee_spike",
            (0, -4.0, -16.5),
            (2.4, 2.4, 6),
            GOLD,
            bevel=0.4,
            rotation_x=math.radians(90),
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
        part_sphere("boot_rivet", (0, 9.2, -27), 0.7, GOLD, segments=8, rings=4),
    ]
    return join_parts(parts, "boss_shadow_knight_part_leg")


def build_boss_part_weapon():
    """巨剑: 金柄尾 + 深柄(双金环) + 金十字护手(宝石) + 宽暗刃(冷银脊+血槽) (原点=握柄顶)。"""
    parts = [
        part_cylinder("grip", (0, 0, -5), 2.6, 10, PURPLE_DEEP, vertices=10, bevel=0.3),
        part_sphere("pommel", (0, 0, 0), 2.8, GOLD),
        part_cylinder("grip_band_a", (0, 0, -2.6), 2.9, 0.8, GOLD, vertices=12),
        part_cylinder("grip_band_b", (0, 0, -7.4), 2.9, 0.8, GOLD, vertices=12),
        part_box("guard", (0, 0, -11), (24, 4.0, 3.6), GOLD, bevel=0.7),
        part_sphere("guard_mid", (0, 0, -11), 3.0, PURPLE_LT),
        part_sphere("guard_gem", (0, -2.8, -11), 1.0, GLOW, segments=10, rings=6),
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
        part_box("fuller", (0, 4.7, -27), (0.8, 1.0, 26), PURPLE, bevel=0.2),
        # 刃身血槽魂光符文
        part_box("blade_rune_a", (0, 5.1, -18.0), (1.2, 0.8, 2.6), GLOW, bevel=0.1),
        part_box("blade_rune_b", (0, 5.1, -25.0), (1.2, 0.8, 2.6), GLOW, bevel=0.1),
        part_box("blade_rune_c", (0, 5.1, -32.0), (1.2, 0.8, 2.6), GLOW, bevel=0.1),
    ]
    return _shift(join_parts(parts, "boss_shadow_knight_part_weapon"), dy=11.0)


BOSS_PART_BUILDERS = {
    "head": build_boss_part_head,
    "torso": build_boss_part_torso,
    "arm": build_boss_part_arm,
    "leg": build_boss_part_leg,
    "weapon": build_boss_part_weapon,
}
