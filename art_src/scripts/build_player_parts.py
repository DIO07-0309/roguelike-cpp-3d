# A16-Player: 玩家低模部件构建导出 + 预览 (在 Blender 内运行)
# 用法: "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup -P art_src/scripts/build_player_parts.py
# 产出: assets/models/player_part_{leg,torso,head,arm,weapon,cape}.json
#       + art_src/preview/blender_preview_player.png (部件排) + ..._figure*.png (装配)
import importlib
import os
import sys

import bpy

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

for _name in ("blender_lib", "blender_models_player", "blender_scene"):
    if _name in sys.modules:
        importlib.reload(sys.modules[_name])

import blender_lib  # noqa: E402
import blender_models_player as player  # noqa: E402
import blender_scene  # noqa: E402


def build_all_parts():
    """在同一场景构建 6 个部件并导出 JSON (导出只读 obj.data, 不需清场); 返回 {slot: obj}"""
    objs = {}
    for slot, factory in player.PLAYER_PART_BUILDERS.items():
        obj = factory()
        obj.name = "player_part_" + slot
        path = os.path.join(REPO, "assets", "models", "player_part_%s.json" % slot)
        vertices, faces = player.export_part_json(obj, path, "player_part_" + slot)
        objs[slot] = obj
        print("built player_part_%-6s verts=%-5d tris=%-5d" % (slot, vertices, faces))
    return objs


def _bbox_min_z(obj):
    return min(v.co.z for v in obj.data.vertices)


def _bbox_max_z(obj):
    return max(v.co.z for v in obj.data.vertices)


def render_parts_row(slots):
    """6 部件排成一排, 底部对齐地面, 正面视角"""
    camera = blender_scene.setup_scene()
    objs = {}
    for index, slot in enumerate(slots):
        obj = player.PLAYER_PART_BUILDERS[slot]()
        obj.location = ((index - (len(slots) - 1) / 2) * 34.0, 0, -_bbox_min_z(obj))
        objs[slot] = obj
    bpy.context.view_layer.update()
    blender_scene.frame_camera(
        camera, (0, 0, 12), distance=300.0, azimuth=-90.0, elevation=8.0
    )
    blender_scene.render_to(
        os.path.join(REPO, "art_src", "preview", "blender_preview_player.png")
    )
    return objs


# 装配锚点 (skeleton 像素空间 y 上, 脚底≈0): (name, slot, bone_x, bone_y, depth_y)
FIGURE_ANCHORS = [
    ("leg_back", "leg", -3, 18, -2),
    ("cape", "cape", -4, 34, 0),
    ("arm_back", "arm", -3, 34, -2),
    ("weapon", "weapon", -3, 29, 2),
    ("torso", "torso", 0, 25, 0),
    ("head", "head", 0, 36, 0),
    ("arm_front", "arm", 4, 34, 2),
    ("leg_front", "leg", 3, 18, 2),
]


def render_figure():
    """8 部件按绑定锚点装配成站姿, 正面 + 3/4 视角"""
    camera = blender_scene.setup_scene()
    parts = []
    for name, slot, bx, by, dy in FIGURE_ANCHORS:
        obj = player.PLAYER_PART_BUILDERS[slot]()
        obj.name = name
        obj.location = (bx, dy, by)
        parts.append(obj)
    bpy.context.view_layer.update()
    top = max(_bbox_max_z(o) + o.location.z for o in parts)
    low = min(_bbox_min_z(o) + o.location.z for o in parts)
    center_z = (top + low) * 0.5
    blender_scene.frame_camera(
        camera, (0, 0, center_z), distance=130.0, azimuth=-90.0, elevation=4.0
    )
    blender_scene.render_to(
        os.path.join(REPO, "art_src", "preview", "blender_preview_player_figure.png")
    )
    blender_scene.frame_camera(
        camera, (0, 0, center_z), distance=135.0, azimuth=-62.0, elevation=10.0
    )
    blender_scene.render_to(
        os.path.join(REPO, "art_src", "preview", "blender_preview_player_figure_3q.png")
    )
    print("figure z span %.1f..%.1f" % (low, top))


def main():
    slots = list(player.PLAYER_PART_BUILDERS.keys())
    build_all_parts()
    render_parts_row(slots)
    render_figure()
    print("A16 player parts done")


if __name__ == "__main__":
    main()
