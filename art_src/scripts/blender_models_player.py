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

# ── 玩家骑士调色板 (贴合 2D player_part_*.png) ──
SKIN = (232, 190, 158)  # 皮肤 (暖浅, 可读)
HAIR = (222, 230, 236)  # 银白头发
RED = (176, 52, 64)  # 红发饰/红袖/剑柄
ARMOR = (92, 112, 146)  # 蓝灰主甲
ARMOR_DARK = (66, 84, 116)  # 甲暗色
WHITE = (232, 236, 242)  # 白布甲/披风基白 (元素染色)
TRIM = (150, 164, 208)  # 淡蓝紫饰边
GOLD = (206, 162, 78)  # 金饰/腰带
BUCKLE = (208, 214, 224)  # 带扣 (银)
TEAL = (72, 132, 168)  # 蓝绿靴
SOLE = (44, 42, 54)  # 靴底/暗部

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
    """头: 复用 NPC _head (颅骨/下颌/颊/鼻/眼白瞳孔高光/眉/上下唇/耳/分层头发) + 骑士红发带/红饰。"""
    parts = []
    _npc._head(parts, SKIN, HAIR)
    # 骑士配饰 (世界坐标, 颅骨 z~38.6, 头顶 z~47)
    parts.append(
        part_box("hair_band", (0, -3.6, 42.2), (10.2, 1.2, 1.5), RED, bevel=0.3)
    )
    parts.append(
        part_box("hair_band_back", (0, 3.0, 43.0), (9.0, 1.2, 1.4), RED, bevel=0.3)
    )
    parts.append(part_sphere("hair_ornament", (2.6, 0.4, 46.6), 1.6, RED))
    parts.append(part_sphere("hair_ornament_gem", (2.6, 0.4, 46.6), 0.65, GOLD))
    obj = join_parts(parts, "player_part_head")
    return _to_local(obj, dz=-HEAD_Z, flip_y=True)


def build_player_part_torso():
    """躯干: 复用 NPC _torso (骨盆/胸/胸甲/领口V/纽扣/腰带/带扣/下摆) + 圆肩甲 + 红胸徽。"""
    parts = []
    _npc._torso(parts, ARMOR, ARMOR_DARK, TRIM, GOLD, BUCKLE)
    # 圆肩甲 (罩肩根, 抬到局部 z≥8 = 世界 z 33)
    for sx in (-6.4, 6.4):
        parts.append(
            part_sphere(
                "pauldron",
                (sx, -0.4, 31.4),
                3.4,
                ARMOR,
                segments=14,
                rings=8,
                scale=(1.0, 1.0, 0.92),
            )
        )
        parts.append(
            part_sphere(
                "pauldron_red", (sx, -0.4, 33.4), 1.2, RED, segments=10, rings=6
            )
        )
    # 红胸徽
    parts.append(
        part_box("chest_emblem", (0, -4.7, 27.0), (3.2, 1.2, 3.6), RED, bevel=0.5)
    )
    obj = join_parts(parts, "player_part_torso")
    return _to_local(obj, dz=-TORSO_Z, flip_y=True)


def build_player_part_arm():
    """臂: 复用 NPC _arm (肩球/肩缝/上臂/肘/前臂/护腕/手指手) — 骑士: 蓝肩 + 红袖 + 金护腕。"""
    parts = []
    _npc._arm(parts, 0.0, ARMOR, RED, SKIN, GOLD)
    obj = join_parts(parts, "player_part_arm")
    return _to_local(obj, dz=-ARM_Z, flip_y=True)


def build_player_part_leg():
    """腿: 复用 NPC _leg (smooth 大腿/护膝/靴/靴头/靴底/靴带) — 骑士: 蓝腿 + 蓝绿靴。"""
    parts = []
    _npc._leg(parts, 0.0, ARMOR, TEAL, SOLE)
    obj = join_parts(parts, "player_part_leg")
    return _to_local(obj, dz=-HIPS_Z, flip_y=True)


def build_player_part_weapon():
    """剑: 握柄锚点 z=0, z∈[-23,0], x∈[-3,3]
    金柄尾 + 红柄 + 金护手(十字) + 银剑刃(锥尖, 蓝青剑脊) + 红宝石。"""
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
            (208, 222, 226),
            vertices=8,
            radius_top=2.2,
            smooth=True,
            bevel=0.3,
        ),
        part_box(
            "blade_ridge", (0, 2.2, -15), (1.0, 0.9, 14), (110, 160, 185), bevel=0.2
        ),
        part_sphere("gem", (0, 0, -6.5), 0.9, (170, 42, 58)),
    ]
    return join_parts(parts, "player_part_weapon")


def build_player_part_cape():
    """披风: 锚点 z=0 顶, z∈[-25,1], 位于背后 (Blender y<0 → 导出 mesh.y>0 离相机)
    白主体(后移避穿模) + 淡蓝紫描边 + 内衬 + 领 + 金扣。基白, 元素染色。"""
    parts = [
        part_box(
            "cape_trim", (1, -9.5, -12), (25.5, 0.8, 28), TRIM, bevel=1.0, taper=0.55
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
