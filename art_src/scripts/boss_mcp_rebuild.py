"""Blender MCP helper: rebuild one hand-built boss and render review previews.

Run inside Blender via MCP:
    import boss_mcp_rebuild as rebuild
    rebuild.rebuild("demon_lord")

The helper keeps the per-boss skeleton anchors/PPU in one place so the MCP
session and the headless preview scripts cannot drift apart.
"""

import importlib
import os
import sys

import bpy

SCRIPT_DIR = r"C:/Demo/roguelike_cpp/art_src/scripts"
REPO = r"C:/Demo/roguelike_cpp"
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

CONFIGS = {
    "shadow_knight": {
        "module": "blender_models_boss",
        "builders": "BOSS_PART_BUILDERS",
        "ppu": 1.15,
        "anchors": [
            ("leg_back", "leg", -4, 28),
            ("arm_back", "arm", -4, 49),
            ("weapon", "weapon", -4, 42),
            ("torso", "torso", 0, 36),
            ("head", "head", 0, 51),
            ("arm_front", "arm", 6, 49),
            ("leg_front", "leg", 4, 28),
        ],
        "prefix": "blender_preview_boss_sk",
        "distance": 300.0,
    },
    "fire_demon": {
        "module": "blender_models_boss_fire_demon",
        "builders": "FIRE_DEMON_PART_BUILDERS",
        "ppu": 1.15,
        "anchors": [
            ("leg_back", "leg", -4, 28),
            ("arm_back", "arm", -4, 49),
            ("weapon", "weapon", -4, 42),
            ("torso", "torso", 0, 36),
            ("head", "head", 0, 51),
            ("arm_front", "arm", 6, 49),
            ("leg_front", "leg", 4, 28),
        ],
        "prefix": "blender_preview_boss_fd",
        "distance": 320.0,
    },
    "golem": {
        "module": "blender_models_boss_golem",
        "builders": "GOLEM_PART_BUILDERS",
        "ppu": 1.15,
        "anchors": [
            ("leg_back", "leg", -4, 28),
            ("arm_back", "arm", -14, 49),
            ("weapon", "weapon", -4, 42),
            ("torso", "torso", 0, 36),
            ("head", "head", 0, 51),
            ("arm_front", "arm", 14, 49),
            ("leg_front", "leg", 4, 28),
        ],
        "prefix": "blender_preview_boss_gl",
        "distance": 300.0,
    },
    "necromancer": {
        "module": "blender_models_boss_necromancer",
        "builders": "NECROMANCER_PART_BUILDERS",
        "ppu": 1.15,
        "anchors": [
            ("leg_back", "leg", -4, 28),
            ("arm_back", "arm", -4, 49),
            ("weapon", "weapon", 6, 42),
            ("torso", "torso", 0, 36),
            ("head", "head", 0, 51),
            ("arm_front", "arm", 6, 49),
            ("leg_front", "leg", 4, 28),
        ],
        "prefix": "blender_preview_boss_nc",
        "distance": 300.0,
    },
    "vampire": {
        "module": "blender_models_boss_vampire",
        "builders": "VAMPIRE_PART_BUILDERS",
        "ppu": 1.15,
        "anchors": [
            ("leg_back", "leg", -4, 28),
            ("arm_back", "arm", -4, 49),
            ("weapon", "weapon", 6, 42),
            ("torso", "torso", 0, 36),
            ("head", "head", 0, 51),
            ("arm_front", "arm", 6, 49),
            ("leg_front", "leg", 4, 28),
        ],
        "prefix": "blender_preview_boss_vp",
        "distance": 300.0,
    },
    "demon_lord": {
        "module": "blender_models_boss_demon_lord",
        "builders": "DEMON_LORD_PART_BUILDERS",
        "ppu": 1.2,
        "anchors": [
            ("leg_back", "leg", -6, 28),
            ("arm_back", "arm", -12, 49),
            ("weapon", "weapon", 12, 42),
            ("torso", "torso", 0, 36),
            ("head", "head", 0, 51),
            ("arm_front", "arm", 12, 49),
            ("leg_front", "leg", 6, 28),
        ],
        "prefix": "blender_preview_boss_dl",
        "distance": 345.0,
    },
}


def _clear_scene():
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


def _bounds(obj):
    points = [obj.matrix_world @ vertex.co for vertex in obj.data.vertices]
    return min(p.z for p in points), max(p.z for p in points)


def rebuild(name, previews=True):
    """Rebuild and export all five parts for one boss, then render previews."""
    config = CONFIGS[name]
    _clear_scene()
    module = importlib.import_module(config["module"])
    module = importlib.reload(module)
    builders = getattr(module, config["builders"])
    for slot, factory in builders.items():
        obj = factory()
        asset_name = "boss_%s_part_%s" % (name, slot)
        verts, tris = module.export_part_json(
            obj,
            os.path.join(REPO, "assets", "models", "%s.json" % asset_name),
            asset_name,
        )
        print("EXPORT %s %s vertices=%d triangles=%d" % (name, slot, verts, tris))
    if not previews:
        return {"name": name, "parts": len(builders)}
    import blender_scene

    blender_scene = importlib.reload(blender_scene)
    camera = blender_scene.setup_scene()
    parts = []
    for bone_name, slot, bone_x, bone_z in config["anchors"]:
        part = builders[slot]()
        part.name = bone_name
        part.scale = (config["ppu"], config["ppu"], config["ppu"])
        part.location = (bone_x * config["ppu"], 0, bone_z * config["ppu"])
        parts.append(part)
    bpy.context.view_layer.update()
    low = min(_bounds(obj)[0] for obj in parts)
    high = max(_bounds(obj)[1] for obj in parts)
    center_z = (low + high) * 0.5
    distance = config["distance"]
    prefix = config["prefix"]
    blender_scene.frame_camera(
        camera, (0, 0, center_z), distance=distance, azimuth=-90.0, elevation=6.0
    )
    blender_scene.render_to(
        os.path.join(REPO, "art_src", "preview", prefix + "_full.png")
    )
    blender_scene.frame_camera(
        camera,
        (0, 0, center_z),
        distance=distance + 10.0,
        azimuth=-52.0,
        elevation=12.0,
    )
    blender_scene.render_to(
        os.path.join(REPO, "art_src", "preview", prefix + "_3q.png")
    )
    blender_scene.frame_camera(
        camera,
        (0, 0, high - 18),
        distance=distance * 0.42,
        azimuth=-90.0,
        elevation=4.0,
    )
    blender_scene.render_to(
        os.path.join(REPO, "art_src", "preview", prefix + "_head.png")
    )
    print(
        "REBUILD_OK %s height=%.2f low=%.2f high=%.2f" % (name, high - low, low, high)
    )
    return {"name": name, "height": high - low, "low": low, "high": high}
