# A16-Player: 玩家完整骨骼动画低模 — 部件建模 (在 Blender 内运行)
# 与 A13 体素 / A14 低模同一管线约定, 但部件 mesh 用"广告牌局部空间":
#   原点 = 骨骼锚点 (部件 pivot), x = 宽度(右), y = 深度(负=朝相机), z = 高度(上)
#   缩放: 引擎 partMeshMatrix 按 kx=ppu*sx / kz=ppu*sy 映射到相机 right/up/normal
# 导出格式与 mesh_asset 加载器一致: {"name","vertices":[[x,y,z,r,g,b]...],"faces":[[i,j,k]...]}
# 构建坐标系 (Blender): X=宽(右), Y=深(前面=+Y 朝相机), Z=高(上); 锚点在原点。
# 导出映射: mesh = (co.x, -co.y, co.z) → 前面(+Y)→mesh.y 负=朝相机。
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
SKIN = (149, 166, 183)  # 皮肤 / 亮甲
ARMOR = (99, 111, 133)  # 主甲 (蓝灰)
ARMOR_LIGHT = (149, 166, 183)  # 亮甲高光
DARK = (63, 38, 49)  # 暗部 / 头发
BOOT = (70, 66, 84)  # 深靴 / 手套
STEEL = (205, 220, 222)  # 钢刃亮缘
GOLD = (175, 129, 88)  # 金饰
GEM = (154, 48, 62)  # 红宝石
CAPE = (226, 228, 234)  # 披风基白 (元素染色)


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
        # Blender 法线 → mesh 局部: (nx, -ny, nz)
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
    """腿: 大腿(锥台) + 小腿 + 靴(前端前伸); 锚点=髋顶, z∈[-20,0], x∈[-11,11]"""
    parts = [
        part_cylinder(
            "thigh", (0, 0, -10), 4.0, 10, ARMOR, vertices=8, radius_top=4.8, bevel=0.35
        ),
        part_cylinder(
            "shin", (0, 0, -18), 3.2, 7, BOOT, vertices=8, radius_top=3.6, bevel=0.35
        ),
        part_box("boot", (0, 1.2, -20), (9, 10, 4), DARK, bevel=0.5),
    ]
    return join_parts(parts, "player_part_leg")


def build_player_part_torso():
    """躯干: 胸甲(上宽锥台) + 腹/腰带 + 胸前饰板; 锚点=中, z∈[-10,10], x∈[-13,15]"""
    parts = [
        part_box("chest", (1, 0, 4), (26, 10, 12), ARMOR, bevel=0.9, taper=0.88),
        part_box("abdomen", (0, 0, -6), (20, 9, 8), ARMOR_LIGHT, bevel=0.7),
        part_box("plate", (1, 5.2, 4), (13, 1.5, 8), SKIN, bevel=0.4),
    ]
    return join_parts(parts, "player_part_torso")


def build_player_part_head():
    """头: 颈(柱) + 头(椭球) + 发(后上盖); 锚点=颈, z∈[-7,13], x∈[-11,11]"""
    parts = [
        part_cylinder("neck", (0, 0, -3.5), 2.8, 5, BOOT, vertices=8),
        part_sphere(
            "skull",
            (0, 0, 3.5),
            8.5,
            SKIN,
            segments=16,
            rings=10,
            scale=(1.0, 0.95, 1.1),
        ),
        part_sphere(
            "hair",
            (0, -2.2, 6.8),
            8.4,
            DARK,
            segments=14,
            rings=8,
            scale=(1.06, 1.05, 0.82),
        ),
    ]
    return join_parts(parts, "player_part_head")


def build_player_part_arm():
    """臂: 上臂(锥台) + 前臂 + 手(球); 锚点=肩顶, z∈[-16,0], x∈[-5,6]"""
    parts = [
        part_cylinder(
            "upper", (0, 0, -4), 2.8, 8, ARMOR, vertices=8, radius_top=3.4, bevel=0.35
        ),
        part_cylinder(
            "forearm",
            (0, 0, -11.5),
            2.2,
            7,
            BOOT,
            vertices=8,
            radius_top=2.8,
            bevel=0.35,
        ),
        part_sphere("hand", (0, 0, -15.5), 2.5, SKIN),
    ]
    return join_parts(parts, "player_part_arm")


def build_player_part_weapon():
    """剑: 握柄(暗) + 护手(金) + 剑刃(锥尖) + 红宝石; 锚点=握柄顶, z∈[-24,0], x∈[-3,3]"""
    parts = [
        part_box("grip", (0, 0, -2), (3, 3, 5), DARK, bevel=0.4),
        part_box("guard", (0, 0, -5), (11, 3, 2.5), GOLD, bevel=0.4),
        part_cylinder(
            "blade",
            (0, 0, -15),
            0.6,
            16,
            STEEL,
            vertices=4,
            radius_top=2.4,
            smooth=False,
        ),
        part_sphere("gem", (0, 0, -4.8), 1.2, GEM),
    ]
    return join_parts(parts, "player_part_weapon")


def build_player_part_cape():
    """披风: 梯形(上窄下宽) + 领; 锚点=顶, z∈[-25,1], 位于背后 y<0, 基白(元素染色)"""
    parts = [
        part_box("cape", (1, -3.5, -12), (28, 3.5, 27), CAPE, bevel=0.9, taper=0.55),
        part_box("collar", (1, -3.5, 0), (13, 4, 3), CAPE, bevel=0.5),
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
