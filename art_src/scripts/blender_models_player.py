# A16-Player: 玩家完整骨骼动画低模 — 部件建模 (在 Blender 内运行)
# 广告牌局部空间: 原点 = 骨骼锚点 (部件 pivot), x = 宽度(右), y = 深度(负=朝相机), z = 高度(上)
# 导出格式与 mesh_asset 加载器一致: {"name","vertices":[[x,y,z,r,g,b]...],"faces":[[i,j,k]...]}
# 构建坐标系 (Blender): X=宽(右), Y=深(前面=+Y 朝相机), Z=高(上); 锚点在原点。
# 导出映射: mesh = (co.x, -co.y, co.z) → 前面(+Y)→mesh.y 负=朝相机。
#
# v5: 直接复用 A14 NPC 的精修人形手法 (blender_models_npc 的 _head/_face/_torso/_arm/_leg),
#     换玩家骑士配色, 构建后整体平移到各骨骼锚点局部空间 (骨骼世界 z: hips=18 torso=25 head=36 arm=34)。
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
import blender_models_npc as _npc

# ── 玩家骑士调色板 (直接采样自 2D player_part_*.png 像素) ──
SKIN = (206, 198, 190)  # 皮肤 (冷浅, 贴合2D冷色调)
HAIR = (176, 190, 198)  # 银发
SILVER = (192, 208, 208)  # 银亮 (发/刃高光)
RED = (192, 64, 64)  # 红饰/红袖/剑柄/红顶
RED_DARK = (144, 48, 48)  # 红暗
ARMOR = (96, 96, 128)  # 蓝紫灰主甲 (2D 主色)
ARMOR_LT = (144, 160, 176)  # 甲高光/浅饰
ARMOR_DK = (64, 64, 80)  # 甲暗
DARK = (48, 32, 48)  # 最深/描边/深靴
WHITE = (224, 224, 224)  # 白布甲/披风基白 (元素染色)
GOLD = (160, 128, 80)  # 金/棕腰带/护腕
LIP = (224, 112, 96)  # 唇
SOLE = (38, 26, 38)  # 靴底

# 骨骼世界 z (与 resources/animations/player_skeleton.json 一致) → 部件局部 z = 世界 z - 锚点 z
HIPS_Z = 18.0
TORSO_Z = 25.0
HEAD_Z = 36.0
ARM_Z = 34.0
WEAPON_Z = 29.0
CAPE_Z = 34.0


def _to_local(obj, dz=0.0, dx=0.0, dy=0.0, flip_y=False):
    """把已构建的 mesh 平移到部件局部原点 (世界锚点 → 局部)。
    flip_y: NPC 几何正面朝 -Y, 玩家正面朝 +Y → 镜像 Y (并反转面绕序保持法线朝外)。"""
    import bmesh

    mesh = obj.data
    if flip_y:
        bm = bmesh.new()
        bm.from_mesh(mesh)
        for vert in bm.verts:
            vert.co.y = -vert.co.y
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
        bm.to_mesh(mesh)
        bm.free()
    for vert in mesh.vertices:
        vert.co.x += dx
        vert.co.y += dy
        vert.co.z += dz
    return obj


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


# ── 部件构建 (复用 NPC 精修几何, 再平移到骨骼局部) ──


def build_player_part_head():
    """头: 复用 NPC _head (颅骨/下颌/颊/鼻/眼白瞳孔高光/眉/上下唇/耳/分层头发)
    + 骑士红顶(2D 头顶大片红)/金饰/红发带。"""
    parts = []
    _npc._head(parts, SKIN, HAIR)
    # 红顶盖 (2D: 头顶红): 覆盖头顶的红色帽/发
    parts.append(
        part_sphere(
            "red_cap",
            (0, 0.2, 45.4),
            4.2,
            RED,
            segments=16,
            rings=9,
            scale=(1.02, 0.98, 0.66),
        )
    )
    parts.append(
        part_box("red_cap_fr", (0, -2.8, 45.0), (9.6, 1.8, 2.6), RED, bevel=0.6)
    )
    parts.append(
        part_box("red_cap_bk", (0, 2.8, 44.6), (8.6, 1.6, 3.0), RED, bevel=0.6)
    )
    # 金 stud
    parts.append(
        part_sphere("cap_stud_l", (-1.8, -1.4, 46.6), 0.7, GOLD, segments=8, rings=4)
    )
    parts.append(
        part_sphere("cap_stud_r", (1.8, -1.4, 46.6), 0.7, GOLD, segments=8, rings=4)
    )
    # 红发带 (前额)
    parts.append(
        part_box("hair_band", (0, -3.6, 42.0), (10.2, 1.2, 1.4), RED, bevel=0.3)
    )
    obj = join_parts(parts, "player_part_head")
    return _to_local(obj, dz=-HEAD_Z, flip_y=True)


def build_player_part_torso():
    """躯干: 复用 NPC _torso (骨盆/胸/胸甲/领口V/纽扣/腰带/带扣/下摆)
    + 中央白胸甲(系绳, 2D) + 左肩红块/右肩蓝甲 + 腰带浅色铆钉。"""
    parts = []
    _npc._torso(parts, ARMOR, ARMOR_DK, WHITE, GOLD, ARMOR_LT)
    # 中央白胸甲前板 (2D: 中央白)
    parts.append(
        part_box("tabard", (0, -4.8, 25.4), (8.8, 1.4, 12.0), WHITE, bevel=0.9)
    )
    # 系绳交叉 (深色细条)
    for sign in (-1, 1):
        parts.append(
            part_box(
                "lace_v",
                (sign * 2.4, -5.6, 25.4),
                (0.9, 0.8, 9.2),
                ARMOR_DK,
                bevel=0.2,
                rotation_z=sign * 0.45,
            )
        )
    for level in (28.6, 25.4, 22.2):
        parts.append(
            part_box("lace_h", (0, -5.6, level), (7.6, 0.8, 0.9), ARMOR_DK, bevel=0.2)
        )
    # 圆肩甲 (2D: 左肩红块, 右肩蓝甲)
    for sx, col in ((6.4, RED), (-6.4, ARMOR)):
        parts.append(
            part_sphere(
                "pauldron",
                (sx, -0.4, 31.2),
                3.4,
                col,
                segments=14,
                rings=8,
                scale=(1.0, 1.0, 0.94),
            )
        )
    # 腰带浅色铆钉
    for sx in (-4.2, -1.4, 1.4, 4.2):
        parts.append(
            part_box(
                "belt_stud", (sx, -5.2, 19.4), (0.9, 0.9, 1.4), ARMOR_LT, bevel=0.2
            )
        )
    obj = join_parts(parts, "player_part_torso")
    return _to_local(obj, dz=-TORSO_Z, flip_y=True)


def build_player_part_arm():
    """臂: 复用 NPC _arm (肩球/肩缝/上臂/肘/前臂/护腕/手指手)
    — 骑士: 蓝肩蓝臂 + 红中段带(2D) + 金护腕。"""
    parts = []
    _npc._arm(parts, 0.0, ARMOR, ARMOR, SKIN, GOLD)
    # 红中段带 (2D: 红在中段)
    parts.append(
        part_cylinder(
            "sleeve_red", (0, 0, 23.2), 2.75, 3.4, RED, vertices=12, bevel=0.3
        )
    )
    obj = join_parts(parts, "player_part_arm")
    return _to_local(obj, dz=-ARM_Z, flip_y=True)


def build_player_part_leg():
    """腿: 复用 NPC _leg (smooth 大腿/护膝/靴/靴头/靴底/靴带)
    — 骑士: 蓝腿 + 深靴(2D 非蓝绿) + 白靴口 + 金饰。"""
    parts = []
    _npc._leg(parts, 0.0, ARMOR, DARK, SOLE)
    # 白靴口 + 金饰 (2D)
    parts.append(
        part_cylinder("cuff", (0, -0.4, 5.2), 3.2, 1.5, WHITE, vertices=14, bevel=0.4)
    )
    parts.append(
        part_sphere("boot_stud", (0, -1.9, 5.2), 0.85, GOLD, segments=8, rings=4)
    )
    obj = join_parts(parts, "player_part_leg")
    return _to_local(obj, dz=-HIPS_Z, flip_y=True)


def build_player_part_weapon():
    """剑: 握柄锚点 z=0, z∈[-23,0], x∈[-3,3]
    金柄尾 + 红柄(带金环, 2D) + 金护手(十字) + 银刃(锥尖, 蓝灰剑脊) + 银护手中。"""
    parts = [
        part_cylinder("grip", (0, 0, -3), 1.5, 6, RED, vertices=8, bevel=0.3),
        part_sphere("pommel", (0, 0, 0), 1.3, GOLD),
        part_cylinder("grip_band_a", (0, 0, -1.9), 1.62, 0.6, GOLD, vertices=10),
        part_cylinder("grip_band_b", (0, 0, -4.1), 1.62, 0.6, GOLD, vertices=10),
        part_box("guard", (0, 0, -6.5), (11, 2.2, 2.0), GOLD, bevel=0.5),
        part_sphere("guard_mid", (0, 0, -6.5), 1.3, SILVER),
        part_cylinder(
            "blade",
            (0, 0, -15),
            0.5,
            16,
            SILVER,
            vertices=8,
            radius_top=2.2,
            smooth=True,
            bevel=0.3,
        ),
        part_box(
            "blade_ridge", (0, 2.2, -15), (1.0, 0.9, 14), (120, 140, 160), bevel=0.2
        ),
    ]
    return join_parts(parts, "player_part_weapon")


def build_player_part_cape():
    """披风: 锚点 z=0 顶, z∈[-25,1], 位于背后 (Blender y<0 → 导出 mesh.y>0 离相机)
    白主体(后移避穿模) + 淡蓝灰细边 + 锯齿下摆(2D 花边) + 领 + 金扣。基白, 元素染色。"""
    parts = [
        part_box(
            "cape_trim",
            (1, -9.5, -12),
            (25.0, 0.8, 28),
            ARMOR_LT,
            bevel=1.0,
            taper=0.55,
        ),
        part_box("cape", (1, -7.3, -12), (24, 4.0, 27), WHITE, bevel=1.2, taper=0.55),
        part_box(
            "cape_inner",
            (1, -5.6, -10),
            (18, 0.8, 20),
            (192, 197, 208),
            bevel=0.8,
            taper=0.62,
        ),
        part_box("collar", (1, -6.5, 0), (13, 3.0, 3.2), WHITE, bevel=0.6),
        part_sphere("clasp", (1, -6.5, 0), 1.1, GOLD),
        # 披领 (顶部加厚前伸贴住上背, 遮住连接缝; 移动/摆动时也不露空隙)
        part_box(
            "cape_mantle", (1, -5.2, -2.0), (21, 6.6, 5.4), WHITE, bevel=1.0, taper=0.9
        ),
        part_sphere("mantle_clasp", (1, -1.9, 0.2), 1.2, GOLD),
    ]
    # 锯齿下摆 (下垂小片形成花边)
    for sx in (-8.0, -3.5, 1.0, 5.5, 9.5):
        parts.append(
            part_box(
                "scallop",
                (sx, -7.2, -26.2),
                (4.8, 3.8, 3.2),
                WHITE,
                bevel=0.9,
                taper=0.72,
            )
        )
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
