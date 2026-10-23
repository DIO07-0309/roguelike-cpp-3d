import bpy
import os
from math import sin, pi, radians
from mathutils import Vector

OUT = r"C:\Users\HP\AppData\Local\Temp\opencode"
scn = bpy.context.scene


def get(name):
    return bpy.data.objects[name]


def setup_scene():
    try:
        scn.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        scn.render.engine = "BLENDER_EEVEE"
    scn.view_settings.view_transform = "Standard"
    scn.render.resolution_x = 320
    scn.render.resolution_y = 320
    world = scn.world or bpy.data.worlds.new("W")
    scn.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs[0].default_value = (0.82, 0.86, 0.92, 1.0)
        bg.inputs[1].default_value = 1.0
    sun_data = bpy.data.lights.new("rsun", type="SUN")
    sun_data.energy = 2.5
    sun = bpy.data.objects.new("rsun", sun_data)
    scn.collection.objects.link(sun)
    sun.rotation_euler = (radians(50), 0, radians(-30))
    fill_data = bpy.data.lights.new("rfill", type="SUN")
    fill_data.energy = 0.9
    fill = bpy.data.objects.new("rfill", fill_data)
    scn.collection.objects.link(fill)
    fill.rotation_euler = (radians(70), 0, radians(140))
    cam_data = bpy.data.cameras.new("rcam")
    cam_data.lens = 50
    cam = bpy.data.objects.new("rcam", cam_data)
    scn.collection.objects.link(cam)
    scn.camera = cam
    target = Vector((0.0, 0.0, 0.78))
    loc = Vector((0.9, -2.1, 2.75))
    cam.location = loc
    cam.rotation_euler = (target - loc).to_track_quat("-Z", "Y").to_euler()


def rot_key(name, frame, rx=0.0, ry=0.0, rz=0.0):
    e = get(name)
    e.rotation_mode = "XYZ"
    e.rotation_euler = (radians(rx), radians(ry), radians(rz))
    e.keyframe_insert("rotation_euler", frame=frame)


def loc_key(name, frame, loc):
    e = get(name)
    e.location = loc
    e.keyframe_insert("location", frame=frame)


def walk():
    n = 8
    for i in range(n + 1):
        f = 1 + i
        t = 2 * pi * i / n
        s = sin(t)
        rot_key("leg_front", f, rx=-22 * s)
        rot_key("leg_back", f, rx=22 * s)
        rot_key("arm_front", f, rx=13 * s)
        rot_key("arm_back", f, rx=-13 * s)
        rot_key("cape", f, rx=7 + 4 * s)
        bob = 0.022 * (0.5 - 0.5 * sin(2 * t - pi / 2))
        loc_key("hips", f, (0, 0, 0.45 + bob))
    return n


def attack():
    keys = {
        "arm_back": [20, 45, -60, -35, -10, 0],
        "weapon": [0, 15, -35, -15, 0, 0],
        "torso": [0, 2, -8, -5, 0, 0],
        "arm_front": [0, -8, 14, 8, 0, 0],
    }
    for name, vals in keys.items():
        for i, v in enumerate(vals):
            rot_key(name, 1 + i, rx=v)
    return len(next(iter(keys.values())))


def render_range(prefix, count):
    for i in range(1, count + 1):
        scn.frame_set(i)
        scn.render.filepath = os.path.join(OUT, "%s_%02d.png" % (prefix, i))
        bpy.ops.render.render(write_still=True)
        print("rendered %s %d" % (prefix, i))


def main():
    setup_scene()
    nw = walk()
    render_range("rig_walk", nw)
    na = attack()
    render_range("rig_atk", na)
    print("ANIM_DONE")


main()
