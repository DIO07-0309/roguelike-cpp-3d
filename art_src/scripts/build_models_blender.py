# A14: Blender 建模主流程 (在 Blender 内运行)
# 用法 (实时交互):
#   python tools/blender_client.py --file art_src/scripts/build_models_blender.py
# 用法 (无头批量):
#   blender -b --factory-startup -P art_src/scripts/build_models_blender.py
# 产出: assets/models/<name>.json + art_src/models/lowpoly_props.blend
#       + art_src/preview/blender_preview*.png (EEVEE 渲染图)
import os
import sys

import bpy

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

# Blender 会缓存已导入模块 — 反复迭代建模时强制重载, 否则改动不生效
import importlib  # noqa: E402

for _name in (
    "blender_lib",
    "blender_models_npc",
    "blender_models_props",
    "blender_models_rooms_a",
    "blender_models_rooms_b",
    "blender_models_challenge",
    "blender_models_items",
    "blender_models_arena",
    "blender_scene",
):
    if _name in sys.modules:
        importlib.reload(sys.modules[_name])

import blender_lib  # noqa: E402
import blender_models_npc as npc_models  # noqa: E402
import blender_models_props as prop_models  # noqa: E402
import blender_models_rooms_a as rooms_a  # noqa: E402
import blender_models_rooms_b as rooms_b  # noqa: E402
import blender_models_challenge as challenge  # noqa: E402
import blender_models_items as item_models  # noqa: E402
import blender_models_arena as arena_models  # noqa: E402
import blender_scene  # noqa: E402

BUILDERS = {
    "npc_1": npc_models.build_npc_1,
    "npc_2": npc_models.build_npc_2,
    "npc_3": npc_models.build_npc_3,
    "npc_blacksmith": npc_models.build_npc_blacksmith,
    "chest": lambda: prop_models.build_chest(False),
    "chest_open": lambda: prop_models.build_chest(True),
    "altar": lambda: prop_models.build_altar(False),
    "altar_used": lambda: prop_models.build_altar(True),
    "spring": lambda: prop_models.build_spring(False),
    "spring_dry": lambda: prop_models.build_spring(True),
    "room_shop": lambda: rooms_a.build_shop(False),
    "room_shop_used": lambda: rooms_a.build_shop(True),
    "room_forge": lambda: rooms_a.build_forge(False),
    "room_forge_used": lambda: rooms_a.build_forge(True),
    "room_library": lambda: rooms_a.build_library(False),
    "room_library_used": lambda: rooms_a.build_library(True),
    "room_gambler": lambda: rooms_b.build_gambler(False),
    "room_gambler_used": lambda: rooms_b.build_gambler(True),
    "room_shrine": lambda: rooms_b.build_shrine(False),
    "room_shrine_used": lambda: rooms_b.build_shrine(True),
    "room_secret": lambda: rooms_b.build_secret(False),
    "room_secret_used": lambda: rooms_b.build_secret(True),
    "room_challenge_pillar": lambda: challenge.build_challenge_pillar(False),
    "room_challenge_pillar_used": lambda: challenge.build_challenge_pillar(True),
    "room_challenge_banner": lambda: challenge.build_challenge_banner(False),
    "room_challenge_banner_used": lambda: challenge.build_challenge_banner(True),
    "room_challenge_gate": lambda: challenge.build_challenge_gate("dormant"),
    "room_challenge_gate_sealed": lambda: challenge.build_challenge_gate("sealed"),
    "room_challenge_gate_active": lambda: challenge.build_challenge_gate("active"),
    "item_potion_red": item_models.build_potion_red,
    "item_potion_blue": item_models.build_potion_blue,
    "item_charm": item_models.build_charm,
    "item_armor": item_models.build_armor,
    "item_robe": item_models.build_robe,
    "item_dagger": item_models.build_dagger,
    "item_sword": item_models.build_sword,
    "item_spear": item_models.build_spear,
    "item_nunchaku": item_models.build_nunchaku,
    "item_crossbow": item_models.build_crossbow,
    "arena_barrel": arena_models.build_arena_barrel,
    "arena_totem": arena_models.build_arena_totem,
    "arena_rock": arena_models.build_arena_rock,
    "arena_spike": arena_models.build_arena_spike,
    "arena_pool": arena_models.build_arena_pool,
    # A15-Guard: 守卫互动 NPC（警戒 + 已交互休息）+ 配套旗帜道具
    "guard_alert": npc_models.build_guard_alert,
    "guard_interacted": npc_models.build_guard_interacted,
    "guard_banner": npc_models.build_guard_banner,
}

GRID_COLUMNS = 6
GRID_SPACING_X = 30.0
GRID_SPACING_Y = 40.0


def build_all():
    """逐个建模 → 导出 JSON → 计入面数"""
    results = {}
    for name, factory in BUILDERS.items():
        blender_scene.clear_scene()
        obj = factory()
        obj.name = name
        vertices, faces = blender_lib.export_json(
            obj, os.path.join(REPO, "assets", "models", name + ".json"), name
        )
        results[name] = (vertices, faces, blender_lib.tri_count(obj))
        print("built %-16s verts=%-6d tris=%-6d" % (name, vertices, faces))
    return results


def save_blend(path=None):
    path = path or os.path.join(REPO, "art_src", "models", "lowpoly_props.blend")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=path)
    print("saved blend:", path)
    return path


def _arrange_scene():
    """按网格摆好全部模型 + 相机/灯光, 返回相机"""
    camera = blender_scene.setup_scene()
    objects = []
    for index, (name, factory) in enumerate(BUILDERS.items()):
        obj = factory()
        obj.name = name
        column = index % GRID_COLUMNS
        row = index // GRID_COLUMNS
        obj.location = (
            column * GRID_SPACING_X - GRID_SPACING_X * (GRID_COLUMNS - 1) / 2,
            row * GRID_SPACING_Y,
            0,
        )
        objects.append(obj)
    bpy.context.view_layer.update()
    return camera, objects


def _layout(objects, names, columns, spacing_x, spacing_y):
    """把指定模型按网格摆位, 其余沉到地下 (z=-1000, 俯视相机永远拍不到)"""
    for obj in objects:
        obj.location = (0, 0, -1000)
    for index, name in enumerate(names):
        obj = next(item for item in objects if item.name == name)
        column = index % columns
        row = index // columns
        obj.location = (
            column * spacing_x - spacing_x * (columns - 1) / 2,
            row * spacing_y,
            0,
        )
    bpy.context.view_layer.update()


def main():
    results = build_all()
    camera, objects = _arrange_scene()
    preview_dir = os.path.join(REPO, "art_src", "preview")
    os.makedirs(preview_dir, exist_ok=True)
    # 全家福 (44 件: 4 NPC + 6 道具 + 12 房间 + 7 挑战房 + 10 掉落物 + 5 竞技场)
    total_count = len(BUILDERS)
    total_rows = (total_count + GRID_COLUMNS - 1) // GRID_COLUMNS
    blender_scene.frame_camera(
        camera,
        (0, GRID_SPACING_Y * (total_rows - 1) * 0.5, 20),
        distance=max(330.0, GRID_SPACING_Y * total_rows * 1.7),
        azimuth=18.0,
        elevation=26.0,
    )
    blender_scene.render_to(os.path.join(preview_dir, "blender_preview.png"))
    # 房间专图 (12 件: 六类 × 两态, 2 排)
    room_names = [name for name in BUILDERS if name.startswith("room_")]
    _layout(objects, room_names, 6, 34.0, 40.0)
    blender_scene.frame_camera(
        camera, (0, 20, 14), distance=250.0, azimuth=20.0, elevation=24.0
    )
    blender_scene.render_to(os.path.join(preview_dir, "blender_preview_rooms.png"))
    # 游戏视角 (HD-2D 相机: 45° 俯视)
    blender_scene.frame_camera(
        camera, (0, 20, 12), distance=300.0, azimuth=26.0, elevation=36.0
    )
    blender_scene.render_to(os.path.join(preview_dir, "blender_preview_rooms_game.png"))
    # 道具两态对照
    prop_names = ["chest", "chest_open", "altar", "altar_used", "spring", "spring_dry"]
    _layout(objects, prop_names, 6, 30.0, 0.0)
    blender_scene.frame_camera(
        camera, (0, 0, 14), distance=190.0, azimuth=20.0, elevation=22.0
    )
    blender_scene.render_to(os.path.join(preview_dir, "blender_preview_states.png"))
    # 单角色特写 (判断实际可读性)
    _layout(objects, ["npc_1"], 1, 0.0, 0.0)
    blender_scene.frame_camera(
        camera, (0, 0, 24), distance=95.0, azimuth=26.0, elevation=28.0
    )
    blender_scene.render_to(os.path.join(preview_dir, "blender_preview_closeup.png"))
    # 掉落物 + 竞技场道具 (15 件)
    gear_names = [name for name in BUILDERS if name.startswith(("item_", "arena_"))]
    _layout(objects, gear_names, 5, 34.0, 36.0)
    blender_scene.frame_camera(
        camera, (0, 36, 14), distance=300.0, azimuth=24.0, elevation=30.0
    )
    blender_scene.render_to(os.path.join(preview_dir, "blender_preview_items.png"))
    save_blend()
    total = sum(value[2] for value in results.values())
    print("TOTAL TRIS:", total)


if __name__ == "__main__":
    main()
