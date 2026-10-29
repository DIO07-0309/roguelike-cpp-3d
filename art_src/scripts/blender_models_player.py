# A16-Player: 玩家完整骨骼动画低模 — 部件建模 (在 Blender 内运行)
# 广告牌局部空间: 原点 = 骨骼锚点 (部件 pivot), x = 宽度(右), y = 深度(负=朝相机), z = 高度(上)
# 导出格式与 mesh_asset 加载器一致: {"name","vertices":[[x,y,z,r,g,b]...],"faces":[[i,j,k]...]}
# 构建坐标系 (Blender): X=宽(右), Y=深(前面=+Y 朝相机), Z=高(上); 锚点在原点。
# 导出映射: mesh = (co.x, -co.y, co.z) → 前面(+Y)→mesh.y 负=朝相机。
# 精细版 v4 (借鉴 A14 NPC 人形手法): 颅骨+下颌+颊/鼻/眼白瞳孔/眉/嘴 + 分层头发(顶盖/刘海/后脑/侧发/发缕)
#   + 红发带; 蓝灰胸甲(锥台+V形甲片) + 白腹甲 + 金腰带 + 贴合圆肩甲 + 红肩饰;
#   细长腿+护膝+白靴口+深靴; 外露臂+白袖口; 红柄金护手银刃剑; 白披风+领+金扣。
import json
import math
import os

import bpy

import blender_lib
from blender_lib import (
    COLOR_JITTER,
    SHADE,
    _hash_jitter,
    part_box,
    part_cylinder,
    part_sphere,
    join_parts,
)

# ── 玩家调色板 (采样自 assets/sprites/player_part_*.png) ──
SKIN = (149, 166, 183)  # 皮肤 (浅蓝灰)
SKIN_DARK = (120, 136, 155)  # 皮肤暗部 (颊/耳窝)
HAIR = (212, 224, 230)  # 银白头发
RED = (166, 44, 60)  # 红发饰/肩饰/剑柄
ARMOR = (92, 108, 138)  # 蓝灰主甲
ARMOR_LIGHT = (138, 156, 186)  # 甲高光
WHITE = (228, 232, 240)  # 白布甲/披风基白 (元素染色)
BOOT = (68, 64, 82)  # 深靴/手套
DARK = (58, 36, 46)  # 暗部/瞳孔
STEEL = (208, 222, 226)  # 银刃
STEEL_DARK = (168, 184, 194)  # 刃暗面
GOLD = (182, 136, 92)  # 金饰
GEM = (170, 42, 58)  # 红宝石
EYE_WHITE = (235, 238, 244)  # 眼白
MOUTH = (150, 120, 120)  # 唇


def _shade_part(normal):
    """按 mesh 局部法线主轴取明暗 (广告牌局部: front=mesh.y<0 朝相机)"""
    nx, ny, nz = abs(normal[0]), abs(normal[1]), abs(normal[2])
    if ny >= nx and ny >= nz:
        return SHADE["front"] if normal[1] < 0 else SHADE["back"]
    if nz >= nx:
        return SHADE["top"] if normal[2] > 0 else SHADE["bottom"]
    return SHADE["right"] if normal[0] > 0 else SHADE["left"]


def export_part_json(obj, path, name):
    """导出部件为广告牌局部空间 JSON (mesh=(x,-y,z), 逐面明暗烘进顶点色)"""
    mesh = obj.data
    material_colors = []
    for slot in obj.material_slots:
        rgba = slot.material.diffuse_color if slot.material else (1, 1, 1, 1)
        material_colors.append(
            (int(rgba[0] * 255), int(rgba[1] * 255), int(rgba[2] * 255))
        )
    vertices, faces = [], []
    for polygon in mesh.polygons:
        base = len(vertices)
        color = (
            material_colors[polygon.material_index]
            if polygon.material_index < len(material_colors)
            else (200, 200, 200)
        )
        jitter = _hash_jitter(polygon.index, COLOR_JITTER)
        normal = polygon.normal
        shade = _shade_part((normal.x, -normal.y, normal.z))
        shaded = tuple(
            max(0, min(255, int((channel + jitter) * shade))) for channel in color
        )
        for loop_index in polygon.loop_indices:
            co = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            vertices.append(
                [
                    round(co.x, 3),
                    round(-co.y, 3),
                    round(co.z, 3),
                    shaded[0],
                    shaded[1],
                    shaded[2],
                ]
            )
        for i in range(1, polygon.loop_total - 1):
            faces.append([base, base + i, base + i + 1])
    data = {"name": name, "vertices": vertices, "faces": faces}
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    return len(vertices), len(faces)


# ── 部件构建 (锚点在原点; z 上, 前面=+Y 朝相机) ──


def build_player_part_leg():
    """腿: 髋锚点 z=0, 向下 z∈[-23,0], x∈[-5,5]
    细长大腿锥台 + 护膝 + 小腿 + 白靴口 + 深靴(前端前伸)。"""
    parts = [
        part_cylinder(
            "thigh", (0, 0, -7), 3.4, 13, ARMOR, vertices=10, radius_top=3.0, bevel=0.5
        ),
        part_cylinder(
            "knee", (0, 0.4, -13.5), 2.9, 2.2, ARMOR_LIGHT, vertices=10, bevel=0.4
        ),
        part_cylinder(
            "shin", (0, 0, -17.5), 2.6, 6, BOOT, vertices=10, radius_top=2.2, bevel=0.5
        ),
        part_cylinder(
            "boot_cuff", (0, 0, -19.5), 3.0, 1.6, WHITE, vertices=10, bevel=0.3
        ),
        part_box("boot", (0, 1.6, -21.5), (8, 10, 4), BOOT, bevel=0.6),
        part_box("boot_toe", (0, 5.6, -22.5), (5, 3, 2.4), DARK, bevel=0.5),
    ]
    return join_parts(parts, "player_part_leg")


def build_player_part_torso():
    """躯干: 锚点 z=0 中间, z∈[-9,12], x∈[-12,12]
    蓝灰胸甲(锥台) + V形亮甲片 + 白腹甲 + 金腰带 + 贴合圆肩甲 + 红肩饰。"""
    parts = [
        # 胸甲主体 (蓝灰, 上宽下窄, 圆润)
        part_box("chest", (0, 0, 4), (21, 10, 13), ARMOR, bevel=1.3, taper=0.78),
        # 胸前 V 形亮甲片 (分层, 微凸)
        part_box(
            "chest_plate",
            (0, 4.6, 4.5),
            (14, 1.4, 9),
            ARMOR_LIGHT,
            bevel=0.6,
            taper=0.82,
        ),
        # 腹部白布甲 (收窄)
        part_box("abdomen", (0, 0, -6), (16, 8, 6), WHITE, bevel=0.7, taper=0.95),
        # 金腰带
        part_box("belt", (0, 0, -2.5), (18, 2.2, 2.0), GOLD, bevel=0.5),
        # 贴合圆肩甲 (蓝灰, 半嵌入胸甲上缘)
        part_sphere(
            "r_shoulder",
            (-9.5, -2, 8),
            2.8,
            ARMOR,
            segments=12,
            rings=8,
            scale=(1.0, 1.0, 0.9),
        ),
        part_sphere(
            "l_shoulder",
            (9.5, -2, 8),
            2.8,
            ARMOR,
            segments=12,
            rings=8,
            scale=(1.0, 1.0, 0.9),
        ),
        # 红肩饰 (肩甲上缘)
        part_sphere("r_shoulder_red", (-9.5, -2, 10.4), 1.1, RED),
        part_sphere("l_shoulder_red", (9.5, -2, 10.4), 1.1, RED),
    ]
    return join_parts(parts, "player_part_torso")


def build_player_part_head():
    """头: 颈锚点 z=0, z∈[-3,12], x∈[-7,7]
    Blender +Y=前面(朝相机), -Y=背面。面部(+Y) + 分层头发(顶盖/刘海前/后脑后/侧发) + 红发带。"""
    parts = [
        # 颈
        part_cylinder("neck", (0, 0.2, -1.5), 2.0, 3.5, SKIN, vertices=10),
        # 颅骨
        part_sphere(
            "skull",
            (0, 0, 5.5),
            6.0,
            SKIN,
            segments=18,
            rings=11,
            scale=(0.98, 0.92, 1.04),
        ),
        # 下颌 (让球颅骨有可读正面, 微前凸 +Y)
        part_box("jaw", (0, 0.8, 2.6), (7.0, 7.4, 4.0), SKIN, bevel=1.2, taper=0.84),
        # 颊 + 下巴 (前面 +Y)
        part_sphere(
            "cheek_l",
            (-2.4, 3.4, 3.6),
            1.5,
            SKIN,
            segments=10,
            rings=6,
            scale=(1.0, 0.62, 0.85),
        ),
        part_sphere(
            "cheek_r",
            (2.4, 3.4, 3.6),
            1.5,
            SKIN,
            segments=10,
            rings=6,
            scale=(1.0, 0.62, 0.85),
        ),
        part_sphere(
            "chin",
            (0, 3.8, 2.2),
            1.6,
            SKIN,
            segments=10,
            rings=6,
            scale=(1.05, 0.7, 0.75),
        ),
        # 鼻 (楔形, 前面 +Y)
        part_box(
            "nose",
            (0, 4.4, 4.0),
            (1.4, 1.7, 1.9),
            SKIN,
            bevel=0.4,
            taper=0.55,
            rotation_x=math.radians(10),
        ),
        # 眼 (眼白 + 瞳孔, 加大) + 眉 (前面 +Y)
        part_sphere(
            "eye_white_l",
            (-2.3, 4.5, 5.2),
            1.5,
            EYE_WHITE,
            segments=12,
            rings=6,
            scale=(0.95, 0.45, 1.2),
        ),
        part_sphere(
            "eye_white_r",
            (2.3, 4.5, 5.2),
            1.5,
            EYE_WHITE,
            segments=12,
            rings=6,
            scale=(0.95, 0.45, 1.2),
        ),
        part_sphere("pupil_l", (-2.3, 5.1, 5.1), 0.75, DARK, segments=8, rings=4),
        part_sphere("pupil_r", (2.3, 5.1, 5.1), 0.75, DARK, segments=8, rings=4),
        part_box(
            "brow_l",
            (-2.3, 4.6, 6.9),
            (3.4, 1.1, 0.9),
            (150, 162, 178),
            bevel=0.25,
            rotation_z=math.radians(8),
        ),
        part_box(
            "brow_r",
            (2.3, 4.6, 6.9),
            (3.4, 1.1, 0.9),
            (150, 162, 178),
            bevel=0.25,
            rotation_z=math.radians(-8),
        ),
        # 嘴 (暗色, 前移避让下巴, 干净横条)
        part_box("mouth", (0, 4.9, 3.0), (3.0, 0.6, 0.6), (108, 82, 88), bevel=0.2),
        # 头发: 顶盖 + 刘海(前 +Y) + 后脑(后 -Y) + 侧发
        part_sphere(
            "hair_cap",
            (0, -0.4, 7.6),
            5.6,
            HAIR,
            segments=16,
            rings=9,
            scale=(1.02, 0.96, 0.74),
        ),
        part_box("fringe", (0, 2.9, 8.0), (8.6, 2.6, 2.4), HAIR, bevel=0.8),
        part_box("back_hair", (0, -3.6, 5.2), (8.8, 2.6, 7.6), HAIR, bevel=0.9),
        part_box("sideburn_l", (-4.2, 1.8, 4.6), (1.5, 3.0, 4.6), HAIR, bevel=0.5),
        part_box("sideburn_r", (4.2, 1.8, 4.6), (1.5, 3.0, 4.6), HAIR, bevel=0.5),
        # 红发带 (前额 +Y, 前移避开刘海)
        part_box("hair_band", (0, 3.6, 7.3), (8.6, 1.0, 1.3), RED, bevel=0.3),
    ]
    return join_parts(parts, "player_part_head")


def build_player_part_arm():
    """臂: 肩锚点 z=0, z∈[-19,0], x∈[-4,4]
    肩球 + 细上臂 + 肘 + 前臂 + 白袖口 + 手套。"""
    parts = [
        part_sphere("shoulder", (0, 0, -1.2), 2.6, ARMOR, segments=12, rings=8),
        part_cylinder(
            "upper", (0, 0, -5), 2.2, 8, ARMOR, vertices=10, radius_top=2.4, bevel=0.4
        ),
        part_cylinder(
            "elbow", (0, 0, -9.5), 2.2, 1.8, ARMOR_LIGHT, vertices=10, bevel=0.3
        ),
        part_cylinder(
            "forearm", (0, 0, -13), 1.9, 6, BOOT, vertices=10, radius_top=2.2, bevel=0.4
        ),
        part_cylinder("cuff", (0, 0, -15.5), 2.2, 1.3, WHITE, vertices=10, bevel=0.3),
        part_sphere("hand", (0, 0, -17.5), 2.0, DARK),
    ]
    return join_parts(parts, "player_part_arm")


def build_player_part_weapon():
    """剑: 握柄锚点 z=0, z∈[-23,0], x∈[-3,3]
    红柄 + 金护手(十字) + 银剑刃(锥尖, 剑脊) + 红宝石。"""
    parts = [
        part_cylinder("grip", (0, 0, -3), 1.5, 6, RED, vertices=8, bevel=0.3),
        part_sphere("pommel", (0, 0, 0), 1.3, GOLD),
        part_box("guard", (0, 0, -6.5), (11, 2.2, 2.0), GOLD, bevel=0.5),
        part_sphere("guard_mid", (0, 0, -6.5), 1.3, GOLD),
        part_cylinder(
            "blade",
            (0, 0, -15),
            0.5,
            16,
            STEEL,
            vertices=6,
            radius_top=2.2,
            smooth=False,
            bevel=0.3,
        ),
        part_box("blade_ridge", (0, 2.2, -15), (1.0, 0.9, 14), STEEL_DARK, bevel=0.2),
        part_sphere("gem", (0, 0, -6.5), 0.9, GEM),
    ]
    return join_parts(parts, "player_part_weapon")


def build_player_part_cape():
    """披风: 锚点 z=0 顶, z∈[-25,1], 位于背后 (Blender y<0 → 导出 mesh.y>0 离相机)
    梯形主体(上窄下宽) + 领 + 内衬 + 金扣。基白, 元素染色。"""
    parts = [
        part_box("cape", (1, -3.2, -12), (28, 4.2, 27), WHITE, bevel=1.2, taper=0.52),
        part_box(
            "cape_inner",
            (1, -1.4, -10),
            (22, 1.1, 22),
            (192, 197, 208),
            bevel=0.8,
            taper=0.62,
        ),
        part_box("collar", (1, -3.2, 0), (13, 4.2, 3.2), WHITE, bevel=0.6),
        part_sphere("clasp", (1, -3.2, 0), 1.1, GOLD),
    ]
    return join_parts(parts, "player_part_cape")


# slot → 构建函数 (与 voxel_slot_from_path 返回的 slot 一致)
PLAYER_PART_BUILDERS = {
    "leg": build_player_part_leg,
    "torso": build_player_part_torso,
    "head": build_player_part_head,
    "arm": build_player_part_arm,
    "weapon": build_player_part_weapon,
    "cape": build_player_part_cape,
}
