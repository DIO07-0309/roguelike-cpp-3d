# A14: Blender 建模基元库 (仅在 Blender 内可用 — bpy 不在系统 Python)
# 约定: Blender Z-up, 1 单位 = 1 世界像素 (TILE_SIZE=32); 导出时旋转为引擎 Y-up
# 导出格式 (与引擎 mesh_asset 加载器一致):
#   {"name", "vertices": [[x,y,z,r,g,b]...], "normals": [[x,y,z]...], "faces": [[i,j,k]...]}
# 顶点按面拆分 (每面 3 顶点) + 逐面法线 (Blender 环法线: 平滑件平滑/硬表面件平)
import json
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

COLOR_JITTER = 7  # 逐面色扰动幅度 (石/木的斑驳感, 0 = 纯基色)
SMOOTH_ROUND = True  # 圆柱/球体默认平滑着色
WRITE_NORMALS = False  # 引擎无实时光照 (明暗已烘进顶点色) → 法线可不写, 省体积

# A14: 明暗烘焙表 (与 A13 体素同一套约定) — 引擎无实时光照, 方向明暗必须写进顶点色
# 引擎系: +X 右, +Y 上, +Z 朝相机 (正面)
SHADE = {
    "front": 1.00,  # 朝相机 = 原色
    "back": 0.45,
    "left": 0.62,
    "right": 0.62,
    "top": 1.55,
    "bottom": 0.30,
}


def _shade_for(normal):
    """按面法线主轴取明暗系数 (引擎系坐标)"""
    ax, ay, az = abs(normal[0]), abs(normal[1]), abs(normal[2])
    if ay >= ax and ay >= az:
        return SHADE["top"] if normal[1] > 0 else SHADE["bottom"]
    if az >= ax:
        return SHADE["front"] if normal[2] > 0 else SHADE["back"]
    return SHADE["right"] if normal[0] > 0 else SHADE["left"]


def _hash_jitter(index, amount):
    """确定性扰动 (同一面每次导出结果一致)"""
    value = (index * 2654435761) % 1000
    return (value / 1000.0 - 0.5) * 2.0 * amount


def _finish(bm, name, color, bevel=0.0, segments=2, smooth=False):
    """倒角 → 生成物体 → 赋材质 (逐面基色)"""
    if bevel > 0.0:
        bmesh.ops.bevel(
            bm,
            geom=list(bm.edges) + list(bm.verts),
            offset=bevel,
            offset_type="OFFSET",
            segments=segments,
            profile=0.5,
            affect="EDGES",
            clamp_overlap=True,
        )
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    material = bpy.data.materials.new(name + "_mat")
    material.use_nodes = True
    # Blender 5.x 节点名本地化 (中文) — 按 bl_idname 找, 不能靠名字
    tree = material.node_tree
    shader = next(
        (n for n in tree.nodes if n.bl_idname == "ShaderNodeBsdfPrincipled"), None
    )
    output = next(
        (n for n in tree.nodes if n.bl_idname == "ShaderNodeOutputMaterial"), None
    )
    if shader is None:
        shader = tree.nodes.new("ShaderNodeBsdfPrincipled")
    if output is None:
        output = tree.nodes.new("ShaderNodeOutputMaterial")
    for link in list(tree.links):
        if link.to_node == output:
            tree.links.remove(link)
    tree.links.new(shader.outputs[0], output.inputs[0])
    shader.inputs["Base Color"].default_value = (
        color[0] / 255.0,
        color[1] / 255.0,
        color[2] / 255.0,
        1.0,
    )
    shader.inputs["Roughness"].default_value = 0.72
    if "Specular IOR Level" in shader.inputs:
        shader.inputs["Specular IOR Level"].default_value = 0.35
    material.diffuse_color = (color[0] / 255.0, color[1] / 255.0, color[2] / 255.0, 1.0)
    obj.data.materials.append(material)
    if smooth:
        for polygon in obj.data.polygons:
            polygon.use_smooth = True
    return obj


def part_box(
    name,
    center,
    size,
    color,
    bevel=0.0,
    segments=2,
    taper=1.0,
    rotation_z=0.0,
    rotation_x=0.0,
):
    """长方体/锥台 (taper<1 顶面收窄)"""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    if taper != 1.0:
        for vert in bm.verts:
            if vert.co.z > 0:
                vert.co.x *= taper
                vert.co.y *= taper
    if rotation_z != 0.0:
        bmesh.ops.rotate(
            bm,
            cent=(0, 0, 0),
            verts=bm.verts,
            matrix=Matrix.Rotation(rotation_z, 3, "Z"),
        )
    if rotation_x != 0.0:
        bmesh.ops.rotate(
            bm,
            cent=(0, 0, 0),
            verts=bm.verts,
            matrix=Matrix.Rotation(rotation_x, 3, "X"),
        )
    bmesh.ops.translate(bm, vec=Vector(center), verts=bm.verts)
    return _finish(bm, name, color, bevel, segments)


def part_cylinder(
    name,
    center,
    radius,
    depth,
    color,
    vertices=16,
    bevel=0.0,
    radius_top=None,
    rotation=None,
    smooth=SMOOTH_ROUND,
):
    """柱/台 (radius_top 可做锥台)"""
    bm = bmesh.new()
    bmesh.ops.create_cone(
        bm,
        cap_ends=True,
        cap_tris=False,
        segments=vertices,
        radius1=radius,
        radius2=radius if radius_top is None else radius_top,
        depth=depth,
    )
    if rotation is not None:
        bmesh.ops.rotate(bm, cent=(0, 0, 0), verts=bm.verts, matrix=rotation)
    bmesh.ops.translate(bm, vec=Vector(center), verts=bm.verts)
    return _finish(bm, name, color, bevel, 2, smooth)


def part_sphere(
    name, center, radius, color, segments=16, rings=8, scale=None, smooth=True
):
    """球/椭球"""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings, radius=radius)
    if scale is not None:
        bmesh.ops.scale(bm, vec=Vector(scale), verts=bm.verts)
    bmesh.ops.translate(bm, vec=Vector(center), verts=bm.verts)
    return _finish(bm, name, color, 0.0, 2, smooth)


def part_torus(
    name,
    center,
    major_radius,
    minor_radius,
    color,
    major_segments=16,
    minor_segments=8,
    rotation=None,
    smooth=True,
):
    """圆环 (把手/箍/水波)"""
    bm = bmesh.new()
    for i in range(major_segments):
        angle = 2.0 * math.pi * i / major_segments
        ring_center = Vector(
            (math.cos(angle) * major_radius, math.sin(angle) * major_radius, 0.0)
        )
        for j in range(minor_segments):
            tube = 2.0 * math.pi * j / minor_segments
            point = ring_center + Vector(
                (math.cos(tube) * minor_radius, math.sin(tube) * minor_radius, 0.0)
            )
            bm.verts.new((point.x, point.y, point.z))
    bm.verts.ensure_lookup_table()
    for i in range(major_segments):
        for j in range(minor_segments):
            a = i * minor_segments + j
            b = i * minor_segments + (j + 1) % minor_segments
            c = ((i + 1) % major_segments) * minor_segments + (j + 1) % minor_segments
            d = ((i + 1) % major_segments) * minor_segments + j
            bm.faces.new((bm.verts[a], bm.verts[b], bm.verts[c], bm.verts[d]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if rotation is not None:
        bmesh.ops.rotate(bm, cent=(0, 0, 0), verts=bm.verts, matrix=rotation)
    bmesh.ops.translate(bm, vec=Vector(center), verts=bm.verts)
    return _finish(bm, name, color, 0.0, 2, smooth)


def part_prism(
    name, center, profile, depth, color, bevel=0.0, rotation_z=0.0, smooth=False
):
    """按 2D 轮廓拉伸 (任意截面: 拱盖/水滴/星形)"""
    bm = bmesh.new()
    half = depth * 0.5
    bottom = [bm.verts.new((p[0], p[1], -half)) for p in profile]
    top = [bm.verts.new((p[0], p[1], half)) for p in profile]
    count = len(profile)
    for i in range(count):
        j = (i + 1) % count
        bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
    bm.faces.new(list(reversed(bottom)))
    bm.faces.new(top)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if rotation_z != 0.0:
        bmesh.ops.rotate(
            bm,
            cent=(0, 0, 0),
            verts=bm.verts,
            matrix=Matrix.Rotation(rotation_z, 3, "Z"),
        )
    bmesh.ops.translate(bm, vec=Vector(center), verts=bm.verts)
    return _finish(bm, name, color, bevel, 2, smooth)


def join_parts(parts, name):
    """把若干部件合并为单物体 (材质随面保留)"""
    bpy.ops.object.select_all(action="DESELECT")
    for obj in parts:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    result = bpy.context.view_layer.objects.active
    result.name = name
    bpy.ops.object.select_all(action="DESELECT")
    return result


def export_json(obj, path, name):
    """导出引擎格式: 面拆顶点 + 逐面法线 (环法线) + 逐面顶点色 (含扰动)"""
    mesh = obj.data
    material_colors = []
    for slot in obj.material_slots:
        rgba = slot.material.diffuse_color if slot.material else (1, 1, 1, 1)
        material_colors.append(
            (int(rgba[0] * 255), int(rgba[1] * 255), int(rgba[2] * 255))
        )
    vertices, normals, faces = [], [], []
    for polygon in mesh.polygons:
        base = len(vertices)
        color = (
            material_colors[polygon.material_index]
            if polygon.material_index < len(material_colors)
            else (200, 200, 200)
        )
        jitter = _hash_jitter(polygon.index, COLOR_JITTER)
        # 面法线 (引擎系) → 明暗系数烘进顶点色 (引擎无实时光照)
        corners = [
            mesh.vertices[mesh.loops[index].vertex_index].co
            for index in polygon.loop_indices
        ]
        face_normal = (corners[1] - corners[0]).cross(corners[2] - corners[0])
        if face_normal.length <= 1e-6:
            continue  # 零面积面 (倒角偶发) — 不导出, 免得引擎判退化
        face_normal.normalize()
        shade = _shade_for((face_normal.x, face_normal.z, -face_normal.y))
        shaded = tuple(
            max(0, min(255, int((channel + jitter) * shade))) for channel in color
        )
        for loop_index in polygon.loop_indices:
            vertex_index = mesh.loops[loop_index].vertex_index
            co = mesh.vertices[vertex_index].co
            normal = mesh.loops[loop_index].normal
            # Blender (x, y, z) → 引擎 (x, z, -y): 绕 X 转 -90°, 手性保持
            vertices.append(
                [
                    round(co.x, 3),
                    round(co.z, 3),
                    round(-co.y, 3),
                    shaded[0],
                    shaded[1],
                    shaded[2],
                ]
            )
            normals.append(
                [round(normal.x, 3), round(normal.z, 3), round(-normal.y, 3)]
            )
        for i in range(1, polygon.loop_total - 1):
            faces.append([base, base + i, base + i + 1])
    data = {"name": name, "vertices": vertices, "faces": faces}
    if WRITE_NORMALS:
        data["normals"] = normals
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    return len(vertices), len(faces)


def tri_count(obj):
    return sum(len(polygon.vertices) - 2 for polygon in obj.data.polygons)


# ── 复合件 (房间道具常用) ──────────────────────────────────
def add_crate(parts, center, size, color, plank_color):
    """木板箱: 箱体 + 四边板缝"""
    parts.append(part_box("crate", center, size, color, bevel=0.7))
    half_w, half_d = size[0] * 0.5, size[1] * 0.5
    for offset in (-half_w * 0.5, half_w * 0.5):
        parts.append(
            part_box(
                "crate_seam",
                (center[0] + offset, center[1] - half_d - 0.2, center[2]),
                (0.6, 0.5, size[2] * 0.86),
                plank_color,
                bevel=0.15,
            )
        )


def add_book(parts, center, size, cover, pages, rotation_z=0.0, rotation_x=0.0):
    """书: 封面 + 书页芯"""
    parts.append(
        part_box(
            "book_cover",
            center,
            size,
            cover,
            bevel=0.2,
            rotation_z=rotation_z,
            rotation_x=rotation_x,
        )
    )
    parts.append(
        part_box(
            "book_pages",
            (center[0], center[1] - size[1] * 0.06, center[2]),
            (size[0] * 0.9, size[1] * 0.86, size[2] * 0.72),
            pages,
            bevel=0.1,
            rotation_z=rotation_z,
            rotation_x=rotation_x,
        )
    )


def add_candle(parts, center, height, wax, flame=None):
    """蜡烛 (+ 火焰: 传入颜色则点亮)"""
    parts.append(part_cylinder("candle", center, 0.9, height, wax, vertices=10))
    if flame is not None:
        parts.append(
            part_cylinder(
                "flame",
                (center[0], center[1], center[2] + height * 0.5 + 0.8),
                0.55,
                1.8,
                flame,
                vertices=8,
                radius_top=0.1,
                smooth=False,
            )
        )


def add_bottle(parts, center, color, liquid, height=3.2):
    """药瓶: 瓶身 + 瓶颈 + 瓶塞 + 液面"""
    parts.append(part_cylinder("bottle", center, 1.0, height, color, vertices=10))
    parts.append(
        part_cylinder(
            "bottle_neck",
            (center[0], center[1], center[2] + height * 0.5 + 0.5),
            0.45,
            1.2,
            color,
            vertices=8,
        )
    )
    parts.append(
        part_cylinder(
            "bottle_cork",
            (center[0], center[1], center[2] + height * 0.5 + 1.2),
            0.5,
            0.6,
            (146, 108, 70),
            vertices=8,
        )
    )
    parts.append(
        part_cylinder(
            "bottle_liquid",
            (center[0], center[1], center[2] - height * 0.12),
            0.92,
            height * 0.62,
            liquid,
            vertices=10,
        )
    )


def add_coin_stack(parts, center, count, coin_color):
    """一摞金币"""
    for index in range(count):
        parts.append(
            part_cylinder(
                "coin",
                (center[0], center[1], center[2] + index * 0.45),
                1.3,
                0.4,
                coin_color,
                vertices=10,
            )
        )
