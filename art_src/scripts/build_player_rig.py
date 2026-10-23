import bpy
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from voxel_lib import load_grid, build_part_mesh

REPO = r"C:\Demo\roguelike_cpp"
SPR = os.path.join(REPO, "assets", "sprites")
SCALE = 0.025

BONES = {
    "root": (None, 0, 0),
    "hips": ("root", 0, 18),
    "torso": ("hips", 0, 7),
    "cape": ("torso", 0, 0),
    "head": ("torso", 0, 11),
    "arm_back": ("torso", -3, 9),
    "weapon": ("arm_back", 0, -5),
    "arm_front": ("torso", 4, 9),
    "leg_front": ("hips", 3, 0),
    "leg_back": ("hips", -3, 0),
}

P = {
    "leg": dict(base=8, growth=0.8, cap=18, ink=7),
    "cape": dict(base=4, growth=0.35, cap=10, ink=5, hem1=(2, 3), hem2=(6, 2)),
    "arm": dict(base=6, growth=0.7, cap=11, ink=7),
    "weapon": dict(base=4, growth=0.3, cap=8, ink=4),
    "torso": dict(base=9, growth=0.75, cap=20, ink=10),
    "head": dict(base=9, growth=0.9, cap=22, ink=10),
}

PARTS = [
    ("part_leg_back", "player_part_leg", "leg_back", (11, 20), 0, 0, "leg", 0.0),
    ("part_cape", "player_part_cape", "cape", (13, 25), -4, 9, "cape", 0.24),
    ("part_arm_back", "player_part_arm", "arm_back", (5, 16), 0, 0, "arm", -0.06),
    ("part_weapon", "player_part_weapon", "weapon", (3, 24), 0, 0, "weapon", -0.04),
    ("part_torso", "player_part_torso", "torso", (13, 10), 0, 0, "torso", 0.0),
    ("part_head", "player_part_head", "head", (11, 7), 0, 0, "head", -0.03),
    ("part_arm_front", "player_part_arm", "arm_front", (5, 16), 0, 0, "arm", -0.06),
    ("part_leg_front", "player_part_leg", "leg_front", (11, 20), 0, 0, "leg", 0.0),
]


def make_bones():
    obs = {}
    for name, (parent, dx, dy) in BONES.items():
        e = bpy.data.objects.new(name, None)
        bpy.context.scene.collection.objects.link(e)
        e.empty_display_size = 0.03
        if parent:
            e.parent = obs[parent]
        e.location = (dx * SCALE, 0, dy * SCALE)
        obs[name] = e
    return obs


def build_parts(bones):
    total = 0
    for name, sprite, bone, pivot, dx, dy, key, y_off in PARTS:
        grid, palette, ink = load_grid(os.path.join(SPR, sprite + ".png"))
        me = build_part_mesh(name, grid, palette, ink, pivot, SCALE, P[key], y_off)
        ob = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(ob)
        ob.parent = bones[bone]
        ob.location = (dx * SCALE, 0, dy * SCALE)
        total += len(me.polygons)
        print(
            "PART %-16s faces=%4d ink=%d/%d"
            % (name, len(me.polygons), len(ink), len(palette))
        )
    return total


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bones = make_bones()
    total = build_parts(bones)
    print("TOTAL_FACES=%d" % total)
    bpy.ops.wm.save_as_mainfile(
        filepath=os.path.join(REPO, "art_src", "player_rig.blend")
    )
    print("SAVED")


main()
