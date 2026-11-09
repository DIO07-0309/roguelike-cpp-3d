# A19-Armor: 8 种护甲地面掉落物专属 3D 模型 (Blender MCP)
# 坐标同 A14 items: Z-up, 1 单位=1 世界像素, 正面朝 -Y, 脚底 z≈0
# 现况: 8 种 item_armor_* icon_key 全挤到 item_armor / item_robe 两个模型,
#       本批给每种护甲独立造型 (配色 + 特征部件), 并扩 kGroundItemModels 映射。
import math
import os

from blender_lib import (
    export_json,
    join_parts,
    part_box,
    part_cylinder,
    part_prism,
    part_sphere,
    part_torus,
)
from mathutils import Matrix

REPO = r"C:/Demo/roguelike_cpp"

# ── 配色 (护甲各系) ──
LEATHER = (128, 84, 46)
LEATHER_DK = (88, 56, 30)
LEATHER_LT = (168, 116, 66)
CHAIN = (140, 148, 160)
CHAIN_DK = (96, 104, 116)
IRON = (152, 162, 174)
IRON_DK = (104, 112, 124)
IRON_LT = (200, 210, 222)
SCALE = (110, 140, 96)
SCALE_DK = (72, 96, 62)
PLATE = (176, 186, 200)
PLATE_DK = (120, 130, 146)
GOLD = (222, 178, 66)
RUNE = (96, 72, 152)
RUNE_DK = (56, 40, 104)
RUNE_GLOW = (176, 128, 240)
ICE = (120, 190, 224)
ICE_DK = (60, 120, 168)
ICE_LT = (200, 236, 248)
SHADOW = (52, 44, 72)
SHADOW_DK = (30, 26, 48)
SHADOW_GLOW = (140, 90, 200)


def _shift(obj, dx=0.0, dy=0.0, dz=0.0):
    for v in obj.data.vertices:
        v.co.x += dx
        v.co.y += dy
        v.co.z += dz
    return obj


def _slanted_box(name, center, size, color, slant_deg, bevel=0.0, taper=1.0):
    obj = part_box(name, (0, 0, 0), size, color, bevel=bevel, taper=taper)
    a = math.radians(slant_deg)
    ca, sa = math.cos(a), math.sin(a)
    for v in obj.data.vertices:
        x, z = v.co.x, v.co.z
        v.co.x = x * ca + z * sa + center[0]
        v.co.z = -x * sa + z * ca + center[2]
        v.co.y += center[1]
    return obj


def _spike(name, center, radius, depth, color, point_up=True):
    return part_cylinder(
        name,
        center,
        0.08 if point_up else radius,
        depth,
        color,
        vertices=6,
        radius_top=radius if point_up else 0.08,
        smooth=False,
    )


def _base_chest(
    main, main_dk, pauldron_color, belt_color, buckle_color, pauldron_r=3.8
):
    """共用立放胸甲骨架 (与 A14 build_armor 同尺度)。"""
    p = [
        part_box("chest", (0, 0, 8.5), (13.0, 7.0, 13.0), main, bevel=1.0, taper=0.78),
        part_box("belt", (0, 0, 1.4), (11.4, 7.6, 2.8), belt_color, bevel=0.4),
        part_box("buckle", (0, -3.9, 1.4), (2.6, 0.8, 2.0), buckle_color, bevel=0.3),
        part_box(
            "ridge", (0, -3.4, 9.0), (2.2, 1.2, 9.0), main_dk, bevel=0.4, taper=0.55
        ),
        part_box("collar", (0, 0, 15.4), (7.0, 6.0, 1.6), main_dk, bevel=0.4),
    ]
    for side in (-1, 1):
        p.append(
            part_sphere(
                "pauldron",
                (side * 7.2, 0, 13.4),
                pauldron_r,
                pauldron_color,
                segments=10,
                rings=6,
                scale=(1.0, 0.85, 0.70),
            )
        )
    return p


# ══════════ 8 种护甲 ══════════
def build_armor_leather():
    """皮甲: 棕革主体 + 交叉缝线 + 皮肩。"""
    p = _base_chest(LEATHER, LEATHER_DK, LEATHER_DK, LEATHER_LT, GOLD, 3.4)
    p.append(
        _slanted_box(
            "stitch_l", (-3, -3.6, 9), (9, 0.6, 0.8), LEATHER_LT, -32, bevel=0.15
        )
    )
    p.append(
        _slanted_box(
            "stitch_r", (3, -3.6, 9), (9, 0.6, 0.8), LEATHER_LT, 32, bevel=0.15
        )
    )
    p.append(part_box("strap", (0, -3.7, 5), (11, 0.6, 1.2), LEATHER_DK, bevel=0.2))
    return join_parts(p, "item_armor_leather")


def build_armor_chain():
    """锁子甲: 灰环主体 + 密集铆环 + 软肩。"""
    p = _base_chest(CHAIN, CHAIN_DK, CHAIN_DK, CHAIN_DK, IRON, 3.2)
    for row in range(3):
        for col in range(4):
            x = -4.5 + col * 3.0
            z = 5 + row * 3.0
            p.append(
                part_sphere(
                    "ring_%d_%d" % (row, col),
                    (x, -3.7, z),
                    0.55,
                    IRON_LT,
                    segments=8,
                    rings=4,
                    scale=(1, 0.4, 1),
                )
            )
    return join_parts(p, "item_armor_chain")


def build_armor_iron():
    """铁铠: 银灰重甲 + 双排铆钉 + 亮肩。"""
    p = _base_chest(IRON, IRON_DK, IRON_LT, IRON_DK, GOLD, 4.0)
    for side in (-1, 1):
        for k in range(3):
            p.append(
                part_sphere(
                    "rivet_%d_%d" % (side, k),
                    (side * 5.2, -3.7, 5 + k * 3.2),
                    0.5,
                    IRON_LT,
                    segments=8,
                    rings=4,
                )
            )
    p.append(
        part_box(
            "chest_plate", (0, -3.8, 9), (9, 0.8, 7), IRON_LT, bevel=0.3, taper=0.85
        )
    )
    return join_parts(p, "item_armor_iron")


def build_armor_scale():
    """鳞甲: 绿鳞主体 + 层叠鳞片 + 皮肩。"""
    p = _base_chest(SCALE, SCALE_DK, SCALE_DK, LEATHER, GOLD, 3.4)
    for row in range(4):
        for col in range(3):
            x = -3.2 + col * 3.2
            z = 4.5 + row * 2.6
            p.append(
                part_sphere(
                    "scale_%d_%d" % (row, col),
                    (x, -3.8, z),
                    1.1,
                    SCALE_DK if (row + col) % 2 else SCALE,
                    segments=8,
                    rings=4,
                    scale=(1.1, 0.5, 0.8),
                )
            )
    return join_parts(p, "item_armor_scale")


def build_armor_plate():
    """板甲: 重胸 + 巨型尖肩 + 金饰 + 护心镜。"""
    p = _base_chest(PLATE, PLATE_DK, PLATE, GOLD, GOLD, 5.0)
    for side in (-1, 1):
        p.append(
            _spike("pauldron_spike_%d" % side, (side * 8.6, 0, 16.5), 1.6, 5.0, GOLD)
        )
    p.append(
        part_sphere(
            "heart", (0, -4.2, 10), 2.0, GOLD, segments=12, rings=6, scale=(1, 0.4, 1)
        )
    )
    p.append(part_box("trim", (0, -3.9, 3), (9, 0.6, 1.0), GOLD, bevel=0.2))
    return join_parts(p, "item_armor_plate")


def build_armor_rune():
    """符文甲: 紫甲 + 发光符文 + 符文肩。"""
    p = _base_chest(RUNE, RUNE_DK, RUNE_DK, RUNE_DK, RUNE_GLOW, 3.8)
    for i, (x, z, s) in enumerate([(-3, 7, 18), (0, 11, 0), (3, 7, -18)]):
        p.append(
            _slanted_box(
                "rune_%d" % i, (x, -3.9, z), (4, 0.7, 1.2), RUNE_GLOW, s, bevel=0.15
            )
        )
    p.append(
        part_sphere(
            "core",
            (0, -4.3, 9),
            1.8,
            RUNE_GLOW,
            segments=10,
            rings=6,
            scale=(1, 0.4, 1),
        )
    )
    return join_parts(p, "item_armor_rune")


def build_armor_ice():
    """冰霜甲: 冰蓝晶甲 + 肩部冰刺 + 霜纹。"""
    p = _base_chest(ICE, ICE_DK, ICE_LT, ICE_DK, ICE_LT, 3.8)
    for side in (-1, 1):
        p.append(_spike("ice_spike_%d" % side, (side * 7.6, 0, 17), 1.4, 6.0, ICE_LT))
    p.append(
        _slanted_box("frost_l", (-3, -3.9, 8), (6, 0.6, 0.9), ICE_LT, -25, bevel=0.15)
    )
    p.append(
        _slanted_box("frost_r", (3, -3.9, 8), (6, 0.6, 0.9), ICE_LT, 25, bevel=0.15)
    )
    return join_parts(p, "item_armor_ice")


def build_armor_robe():
    """暗影袍: 黑紫兜帽袍 + 幽光符 + 垂布 (替代 item_robe 的暗影版)。"""
    p = [
        part_box("gown", (0, 0, 7.6), (12.5, 8.0, 15.2), SHADOW, bevel=0.9, taper=0.50),
        part_box("sash", (0, 0, 5.6), (13.0, 8.4, 2.2), SHADOW_DK, bevel=0.4),
        part_box(
            "front_trim", (0, -3.3, 8.4), (2.4, 1.0, 12.0), SHADOW_GLOW, bevel=0.3
        ),
        part_sphere(
            "hood",
            (0, 0, 15.8),
            4.4,
            SHADOW_DK,
            segments=10,
            rings=6,
            scale=(1.0, 0.85, 0.75),
        ),
        part_sphere(
            "gem",
            (0, -4.2, 11),
            1.4,
            SHADOW_GLOW,
            segments=10,
            rings=6,
            scale=(1, 0.4, 1),
        ),
    ]
    for side in (-1, 1):
        p.append(
            _slanted_box(
                "drape_%d" % side,
                (side * 5, -3.5, 4),
                (2, 0.7, 8),
                SHADOW_DK,
                side * 10,
                bevel=0.2,
                taper=0.5,
            )
        )
    return join_parts(p, "item_armor_robe")


ARMOR_BUILDERS = {
    "leather": build_armor_leather,
    "chain": build_armor_chain,
    "iron": build_armor_iron,
    "scale": build_armor_scale,
    "plate": build_armor_plate,
    "rune": build_armor_rune,
    "ice": build_armor_ice,
    "robe": build_armor_robe,
}


def export_armor(key):
    obj = ARMOR_BUILDERS[key]()
    name = "item_armor_" + key
    path = os.path.join(REPO, "assets", "models", name + ".json")
    verts, faces = export_json(obj, path, name)
    return obj, verts, faces


def export_all():
    for key in ARMOR_BUILDERS:
        _, v, f = export_armor(key)
        print("EXPORT item_armor_%-10s verts=%d tris=%d" % (key, v, f))
    print("EXPORT_ALL_OK armors=%d" % len(ARMOR_BUILDERS))


def preview_one(key):
    """MCP 内渲染单个护甲预览 (3/4 俯视)。"""
    import bpy
    import blender_scene

    camera = blender_scene.setup_scene()
    obj = ARMOR_BUILDERS[key]()
    bpy.context.view_layer.update()
    pts = [v.co for v in obj.data.vertices]
    low, high = min(p.z for p in pts), max(p.z for p in pts)
    cz = (low + high) * 0.5
    blender_scene.frame_camera(
        camera, (0, 0, cz), distance=62.0, azimuth=128.0, elevation=16.0
    )
    out = os.path.join(REPO, "art_src", "preview", "armor_preview_%s.png" % key)
    blender_scene.render_to(out)
    print("PREVIEW %s height=%.1f" % (key, high - low))
    return out
