# A14: Blender 预览场景 (相机 + 三点光 + 世界) — 供建模过程实时看效果
# 仅在 Blender 内运行 (bpy)。可被 blender_client.py --file 直接执行
import bpy
import math
from mathutils import Vector

RENDER_W = 960
RENDER_H = 640


def clear_scene():
    if bpy.context.object and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    for block in (
        bpy.data.meshes,
        bpy.data.materials,
        bpy.data.cameras,
        bpy.data.lights,
    ):
        for item in list(block):
            if item.users == 0:
                block.remove(item)


def _aim(obj, target):
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def add_lighting():
    scene = bpy.context.scene
    world = bpy.data.worlds.get("PreviewWorld") or bpy.data.worlds.new("PreviewWorld")
    scene.world = world
    world.use_nodes = True
    nodes = world.node_tree.nodes
    background = nodes.get("Background")
    if background is None:
        background = nodes.new("ShaderNodeBackground")
        output = nodes.get("World Output")
        if output is None:
            output = nodes.new("ShaderNodeOutputWorld")
        world.node_tree.links.new(background.outputs[0], output.inputs[0])
    background.inputs[0].default_value = (0.10, 0.11, 0.15, 1.0)
    background.inputs[1].default_value = 1.0
    # 日光灯 (能量按辐照度, 与距离无关 — 比面光好控)
    specs = (
        ("KeyLight", (90, -110, 120), 3.4),
        ("FillLight", (-110, -60, 60), 1.1),
        ("RimLight", (20, 120, 90), 1.8),
    )
    for name, location, energy in specs:
        light_data = bpy.data.lights.new(name, type="SUN")
        light_data.energy = energy
        light_data.angle = math.radians(12.0)
        light = bpy.data.objects.new(name, light_data)
        light.location = location
        bpy.context.scene.collection.objects.link(light)
        _aim(light, (0, 0, 20))
    _add_ground()


def _add_ground():
    """地面 (承接阴影, 给模型一个站位)"""
    material = bpy.data.materials.new("GroundMat")
    material.use_nodes = True
    tree = material.node_tree
    shader = next(
        (n for n in tree.nodes if n.bl_idname == "ShaderNodeBsdfPrincipled"), None
    )
    if shader is not None:
        shader.inputs["Base Color"].default_value = (0.17, 0.17, 0.20, 1.0)
        shader.inputs["Roughness"].default_value = 0.95
    size = 400.0
    mesh = bpy.data.meshes.new("Ground")
    mesh.from_pydata(
        [(-size, -size, 0), (size, -size, 0), (size, size, 0), (-size, size, 0)],
        [],
        [(0, 1, 2, 3)],
    )
    mesh.update()
    ground = bpy.data.objects.new("Ground", mesh)
    ground.data.materials.append(material)
    bpy.context.scene.collection.objects.link(ground)


def add_camera():
    camera_data = bpy.data.cameras.new("PreviewCam")
    camera_data.lens = 70
    camera = bpy.data.objects.new("PreviewCam", camera_data)
    bpy.context.scene.collection.objects.link(camera)
    bpy.context.scene.camera = camera
    return camera


def setup_render():
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = RENDER_W
    scene.render.resolution_y = RENDER_H
    scene.render.film_transparent = False
    try:
        scene.eevee.taa_render_samples = 64
        scene.eevee.use_raytracing = True
    except Exception:
        pass
    scene.view_settings.view_transform = (
        "Standard"  # 保持像素风饱和度, 不做 filmic 压暗
    )


def frame_camera(camera, target, distance=90.0, azimuth=38.0, elevation=26.0):
    """把相机摆到目标周围的斜俯视角"""
    radians_a = math.radians(azimuth)
    radians_e = math.radians(elevation)
    offset = (
        Vector(
            (
                math.cos(radians_a) * math.cos(radians_e),
                -math.sin(radians_a) * math.cos(radians_e),
                math.sin(radians_e),
            )
        )
        * distance
    )
    camera.location = Vector(target) + offset
    _aim(camera, target)


def setup_scene():
    clear_scene()
    add_lighting()
    camera = add_camera()
    setup_render()
    return camera


def render_to(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    return path
