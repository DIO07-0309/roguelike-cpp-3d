# A14: NPC + 场景道具 低模硬表面模型定义 (真 3D 网格, 非贴图浮雕)
# 运行: python art_src/scripts/build_models.py   (Blender 内亦可: blender -b -P <此脚本>)
# 输出: assets/models/*.json  引擎按顶点色 + flat 法线渲染
# 坐标系: 引擎系 (X 右, Y 上, Z 前), 1 单位 = 1 世界像素 (TILE_SIZE=32), 脚底贴地
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import MeshBuilder  # noqa: E402

REPO = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
)
OUT_DIR = os.path.join(REPO, "assets", "models")

SKIN = (232, 176, 128)
SKIN_LIGHT = (240, 192, 148)
HAIR_BROWN = (138, 90, 46)
HAIR_DARK = (96, 60, 34)
CLOTH_WHITE = (232, 236, 244)
CLOTH_BLUE = (74, 111, 165)
CLOTH_TAN = (176, 112, 56)
CLOTH_PURPLE = (124, 63, 160)
CLOTH_DARK = (90, 58, 34)
TROUSER = (74, 62, 54)
METAL = (154, 164, 176)
GOLD = (216, 169, 60)
WOOD = (200, 129, 60)
WOOD_DARK = (150, 92, 42)
STONE_BLUE = (42, 51, 80)
STONE_EDGE = (58, 70, 108)
CORE_GREEN = (60, 196, 106)
STONE_GREY = (110, 116, 128)
STONE_DARK = (78, 84, 96)
WATER_GREEN = (63, 160, 96)


def _legs(builder, trouser, boot):
    for x in (-4.0, 4.0):
        builder.add_box((x, 4.0, 0.0), (6.5, 8.0, 8.0), trouser)
        builder.add_box((x, 1.6, 1.0), (7.0, 3.2, 9.5), boot)


def _torso(builder, cloth, trim):
    builder.add_box((0.0, 25.0, 0.0), (15.0, 15.0, 9.5), cloth, taper=0.82)
    builder.add_box((0.0, 18.5, 0.0), (15.5, 2.6, 10.0), trim)


def _arms(builder, cloth, sleeve):
    builder.add_box_yaw((-9.4, 26.0, 0.5), (4.6, 14.0, 6.0), sleeve, 6.0)
    builder.add_box_yaw((9.4, 26.0, 0.5), (4.6, 14.0, 6.0), sleeve, -6.0)
    builder.add_box((-9.8, 19.4, 0.6), (4.4, 4.0, 5.4), cloth)
    builder.add_box((9.8, 19.4, 0.6), (4.4, 4.0, 5.4), cloth)


def _eyes(builder, y=39.0, z=4.9):
    builder.add_box((-2.4, y, z), (1.8, 2.0, 0.8), (48, 36, 32))
    builder.add_box((2.4, y, z), (1.8, 2.0, 0.8), (48, 36, 32))


def _head(builder, skin, hair, long_hair=False, eyes=True):
    builder.add_box((0.0, 38.5, 0.0), (11.0, 10.0, 9.5), skin)
    builder.add_box((0.0, 44.2, -0.4), (11.6, 3.4, 10.0), hair)
    if long_hair:
        builder.add_box((0.0, 34.0, -4.4), (11.0, 20.0, 3.0), hair)
    if eyes:
        _eyes(builder)


def build_npc_1():
    """村民: 棕发 + 白蓝衣"""
    b = MeshBuilder()
    _legs(b, TROUSER, (92, 74, 60))
    _torso(b, CLOTH_WHITE, CLOTH_BLUE)
    _arms(b, CLOTH_WHITE, (208, 216, 232))
    _head(b, SKIN, HAIR_BROWN)
    return b


def build_npc_2():
    """行商: 棕发 + 褐色外袍"""
    b = MeshBuilder()
    _legs(b, CLOTH_DARK, (92, 74, 60))
    _torso(b, CLOTH_TAN, CLOTH_DARK)
    _arms(b, CLOTH_TAN, (156, 98, 48))
    _head(b, SKIN, HAIR_BROWN)
    b.add_box((0.0, 26.0, 5.0), (9.0, 10.0, 1.6), (150, 96, 52))  # 围兜
    return b


def build_npc_3():
    """女巫: 长棕发 + 紫裙"""
    b = MeshBuilder()
    _legs(b, CLOTH_PURPLE, (88, 40, 120))
    _torso(b, CLOTH_PURPLE, (88, 40, 120))
    _arms(b, CLOTH_PURPLE, (104, 52, 140))
    _head(b, SKIN_LIGHT, HAIR_DARK, long_hair=True)
    b.add_cone((0.0, 43.0, 0.0), 7.5, 8.0, 8, (70, 40, 96))  # 尖帽
    return b


def build_npc_blacksmith():
    """铁匠: 光头 + 深棕围裙 + 铁砧"""
    b = MeshBuilder()
    _legs(b, TROUSER, (60, 50, 44))
    _torso(b, CLOTH_DARK, (60, 40, 30))
    _arms(b, CLOTH_DARK, (78, 50, 32))
    b.add_box((0.0, 38.5, 0.0), (11.0, 10.0, 9.5), SKIN)
    b.add_box((0.0, 36.6, 4.9), (7.0, 2.0, 1.4), (60, 44, 36))  # 胡子
    b.add_box((0.0, 25.0, 5.2), (12.0, 16.0, 1.4), (120, 78, 44))  # 围裙
    b.add_box((16.0, 7.0, 0.0), (12.0, 5.0, 7.0), METAL)  # 铁砧台
    b.add_box((16.0, 11.0, 0.0), (7.0, 3.0, 5.0), METAL)
    return b


def build_chest():
    """宝箱: 木箱 + 金属箍 + 金锁"""
    b = MeshBuilder()
    b.add_box((0.0, 6.0, 0.0), (22.0, 12.0, 15.0), WOOD)
    b.add_box((0.0, 14.0, 0.0), (21.0, 4.0, 14.0), WOOD_DARK)  # 盖沿
    b.add_box((0.0, 16.4, 0.0), (17.0, 2.8, 11.0), WOOD)  # 拱盖
    b.add_box((0.0, 18.0, 0.0), (11.0, 2.2, 8.0), WOOD)
    for x in (-7.0, 7.0):
        b.add_box((x, 9.0, 0.0), (2.6, 16.0, 15.6), METAL)  # 金属箍
    b.add_box((0.0, 8.0, 8.0), (5.0, 6.0, 2.0), GOLD)  # 锁
    b.add_box((0.0, 10.2, 8.0), (2.4, 2.4, 1.4), (240, 200, 90))
    return b


def build_altar():
    """祭坛: 深蓝石台 + 发光绿核"""
    b = MeshBuilder()
    b.add_box((0.0, 2.0, 0.0), (20.0, 4.0, 16.0), STONE_BLUE)
    b.add_box((0.0, 5.0, 0.0), (16.0, 2.0, 13.0), STONE_EDGE)
    b.add_box((0.0, 12.0, 0.0), (11.0, 12.0, 9.0), STONE_BLUE, taper=0.85)
    b.add_box((0.0, 19.0, 0.0), (17.0, 3.0, 13.0), STONE_EDGE)
    b.add_box((0.0, 21.4, 0.0), (12.0, 2.0, 9.0), STONE_BLUE)
    b.add_box((0.0, 12.0, 5.0), (6.0, 6.0, 3.0), CORE_GREEN)  # 核心
    return b


def build_spring():
    """泉水: 八边石环 (切向排布) + 绿水面"""
    import math

    b = MeshBuilder()
    for i in range(8):
        angle = 22.5 + i * 45.0
        radians = math.radians(angle)
        x = math.cos(radians) * 12.0
        z = math.sin(radians) * 12.0
        b.add_box_yaw((x, 3.0, z), (9.4, 6.0, 5.0), STONE_GREY, angle + 90.0)
        b.add_box_yaw((x, 0.8, z), (9.8, 1.6, 5.4), STONE_DARK, angle + 90.0)
    b.add_cylinder((0.0, 1.0, 0.0), 8.0, 2.0, 8, STONE_DARK)  # 井底
    b.add_cylinder((0.0, 4.8, 0.0), 8.4, 0.8, 8, WATER_GREEN)  # 水面
    return b


BUILDERS = {
    "npc_1": build_npc_1,
    "npc_2": build_npc_2,
    "npc_3": build_npc_3,
    "npc_blacksmith": build_npc_blacksmith,
    "chest": build_chest,
    "altar": build_altar,
    "spring": build_spring,
}


def main():
    for name, factory in BUILDERS.items():
        builder = factory()
        path = builder.save(OUT_DIR, name)
        print(
            "wrote %s  verts=%d tris=%d"
            % (path, len(builder.vertices), builder.triangle_count())
        )
    _save_blend_preview()


def _save_blend_preview():
    """可选: 有 Blender 时把网格写进 .blend 供后续手工微调"""
    try:
        import bpy
    except ImportError:
        print("blender not available, skip .blend preview")
        return
    for name, factory in BUILDERS.items():
        builder = factory()
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(
            [(v[0], v[1], v[2]) for v in builder.vertices], [], builder.faces
        )
        mesh.update()
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.collection.objects.link(obj)
    blend_path = os.path.join(REPO, "art_src", "models", "lowpoly_props.blend")
    os.makedirs(os.path.dirname(blend_path), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=blend_path)
    print("wrote %s" % blend_path)


if __name__ == "__main__":
    main()
