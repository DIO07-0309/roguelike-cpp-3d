# A6-S2 批次7: Boss 骨骼分件生成器 (5 Boss / 大 rig 1.4x / 独立骨架)
# 产出 boss_<id>_part_*.png + boss_<id>_skeleton.json; 共享 boss_anim.json
import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw

from anim_preview import checkerboard, render_pose
from gen_mon_humanoid_parts import OUTLINE, PART_ROWS, WEAPONS

ROOT = Path(__file__).resolve().parents[1]

SIZES = {"head": (30, 26), "torso": (40, 28), "arm": (16, 22),
         "leg": (32, 28), "weapon": (10, 30)}

BONES = [
    {"name": "root"},
    {"name": "hips", "parent": "root", "y": 28},
    {"name": "torso", "parent": "hips", "y": 8},
    {"name": "head", "parent": "torso", "y": 15},
    {"name": "arm_back", "parent": "torso", "x": -4, "y": 13},
    {"name": "weapon", "parent": "arm_back", "y": -7},
    {"name": "arm_front", "parent": "torso", "x": 6, "y": 13},
    {"name": "leg_front", "parent": "hips", "x": 4},
    {"name": "leg_back", "parent": "hips", "x": -4},
]

PART_SLOTS = [("leg_back", "leg", [8, 28]), ("arm_back", "arm", [6, 22]),
              ("weapon", "weapon", [4, 30]), ("torso", "torso", [18, 14]),
              ("head", "head", [15, 10]), ("arm_front", "arm", [6, 22]),
              ("leg_front", "leg", [8, 28])]

PPU = 0.8
ANCHOR = [34, 87]

PALETTES = {
    "shadow_knight": {"s": (34, 28, 48), "m": (58, 48, 80), "l": (92, 78, 122),
                      "h": (136, 120, 168), "r": (70, 52, 110),
                      "R": (104, 84, 156), "p": (150, 132, 190),
                      "b": (18, 14, 30), "g": (96, 88, 70)},
    "necro_boss": {"s": (36, 52, 42), "m": (62, 86, 66), "l": (96, 128, 98),
                   "h": (150, 182, 140), "r": (96, 72, 50),
                   "R": (136, 104, 70), "p": (214, 220, 190),
                   "b": (24, 30, 26), "g": (96, 88, 70)},
    "vampire_boss": {"s": (52, 18, 28), "m": (96, 30, 44), "l": (148, 48, 66),
                     "h": (206, 84, 104), "r": (124, 40, 52),
                     "R": (186, 66, 80), "p": (240, 214, 160),
                     "b": (34, 14, 22), "g": (110, 80, 64)},
    "fire_demon": {"s": (58, 26, 20), "m": (112, 52, 28), "l": (178, 84, 34),
                   "h": (236, 140, 52), "r": (150, 58, 26),
                   "R": (226, 92, 32), "p": (252, 214, 120),
                   "b": (40, 16, 12), "g": (150, 110, 70)},
    "mirror_void": {"s": (30, 26, 52), "m": (54, 46, 88), "l": (84, 72, 132),
                    "h": (120, 106, 176), "r": (64, 54, 96),
                    "R": (96, 84, 144), "p": (168, 152, 214),
                    "b": (16, 12, 28), "g": (96, 88, 70)},
}

RAPIER = "|".join(["...ohhl..."] * 26 + ["..oohhho..", "....go....", "....go....", "...oggo..."])

MIRROR_BLADE_TAIL = ["...oRRo...", ".oohhhhoo.", "....go....", "....go....", "....go....", "...oggo..."]
MIRROR_BLADE = "|".join(["...ohho..."] * 3 + ["..oRppho.."] * 21 + MIRROR_BLADE_TAIL)

WEAPONS_BOSS = dict(WEAPONS)
WEAPONS_BOSS["rapier"] = RAPIER.split("|")
WEAPONS_BOSS["mirror_blade"] = MIRROR_BLADE.split("|")

BOSS = {
    "shadow_knight": ("shadow_knight", "greatsword"),
    "necromancer": ("necro_boss", "staff"),
    "vampire": ("vampire_boss", "rapier"),
    "fire_demon": ("fire_demon", None),
    "demon_lord": ("mirror_void", "mirror_blade"),
}
assert len(BOSS) == 5, "BOSS must have exactly 5 entries"

DECOR = {
    ("shadow_knight", "head"): [(1, 3, 12, 18, "R")],
    ("shadow_knight", "torso"): [(2, 3, 10, 29, "h"), (4, 6, 2, 7, "l"),
                                 (4, 6, 32, 37, "l")],
    ("shadow_knight", "arm"): [(3, 6, 5, 11, "l")],
    ("necromancer", "head"): [(1, 3, 8, 21, "m")],
    ("necromancer", "torso"): [(1, 2, 12, 27, "R"), (22, 26, 4, 35, "s")],
    ("vampire", "head"): [(7, 10, 10, 19, "p")],
    ("vampire", "torso"): [(0, 2, 8, 31, "R"), (4, 7, 3, 8, "r")],
    ("fire_demon", "head"): [(0, 2, 9, 20, "R"), (1, 1, 13, 16, "p")],
    ("fire_demon", "torso"): [(6, 8, 10, 17, "R"), (13, 15, 19, 30, "R"),
                              (19, 20, 13, 23, "r")],
    ("demon_lord", "head"): [(8, 9, 9, 20, "h")],
    ("demon_lord", "torso"): [(6, 7, 6, 33, "p"), (14, 15, 10, 29, "R")],
}


def build_palette(palette_key):
    palette = {".": (0, 0, 0, 0), "o": OUTLINE}
    for char, rgb in PALETTES[palette_key].items():
        palette[char] = (*rgb, 255)
    return palette


def upscale(rows, width, height):
    src_w, src_h = len(rows[0]), len(rows)
    fx, fy = width / src_w, height / src_h
    out = [["." for _ in range(width)] for _ in range(height)]
    for oy in range(height):
        row = rows[min(src_h - 1, int(round(oy / fy)))]
        for ox in range(width):
            out[oy][ox] = row[min(src_w - 1, int(round(ox / fx)))]
    return out


def fill_rect(rows, y0, y1, x0, x1, char):
    for y in range(max(0, y0), min(len(rows) - 1, y1) + 1):
        for x in range(max(0, x0), min(len(rows[0]) - 1, x1) + 1):
            rows[y][x] = char


def part_rows(boss_id, part):
    weapon = BOSS[boss_id][1]
    if part == "weapon":
        if weapon is None:
            width, height = SIZES["weapon"]
            return ["." * width for _ in range(height)]
        rows = WEAPONS_BOSS[weapon]
    else:
        rows = PART_ROWS[part]
    width, height = SIZES[part]
    rows = upscale(list(rows), width, height)
    for y0, y1, x0, x1, char in DECOR.get((boss_id, part), []):
        fill_rect(rows, y0, y1, x0, x1, char)
    return rows


def part_image(boss_id, part):
    palette = build_palette(BOSS[boss_id][0])
    rows = part_rows(boss_id, part)
    width, height = SIZES[part]
    if len(rows) != height or any(len(r) != width for r in rows):
        raise ValueError(f"Invalid pixel grid: {boss_id} {part}")
    image = Image.new("RGBA", (width, height))
    image.putdata([palette[pixel] for row in rows for pixel in row])
    if not {pixel[3] for pixel in image.getdata()} <= {0, 255}:
        raise ValueError(f"Non-binary alpha: {boss_id} {part}")
    return image


def skeleton_dict(boss_id):
    parts = [{"bone": bone,
              "file": f"assets/sprites/boss_{boss_id}_part_{name}.png",
              "pivot": list(pivot)} for bone, name, pivot in PART_SLOTS]
    return {"pixels_per_unit": PPU, "anchor": list(ANCHOR),
            "bones": BONES, "parts": parts}


def generate(output_dir, skeleton_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    skeleton_dir.mkdir(parents=True, exist_ok=True)
    for boss_id in BOSS:
        images = {name: part_image(boss_id, name) for name in SIZES}
        for name, image in images.items():
            image.save(output_dir / f"boss_{boss_id}_part_{name}.png")
        path = skeleton_dir / f"boss_{boss_id}_skeleton.json"
        path.write_text(json.dumps(skeleton_dict(boss_id), indent=2) + "\n",
                        encoding="utf-8")
        print(f"wrote boss_{boss_id} parts+skeleton")
    return {bid: {name: part_image(bid, name) for name in SIZES}
            for bid in BOSS}


def make_sheet(images):
    row_h, part_scale, canvas_w = 150, 3, 760
    sheet = checkerboard((canvas_w, row_h * len(BOSS) + 20))
    draw = ImageDraw.Draw(sheet)
    for index, boss_id in enumerate(BOSS):
        palette_key, weapon = BOSS[boss_id]
        y = index * row_h + 18
        draw.text((6, y - 12), boss_id, fill=(205, 220, 222))
        label = "empty" if weapon is None else weapon
        draw.text((6, y + 2), f"{palette_key} ppu={PPU} {label}",
                  fill=(149, 166, 183))
        x = 150
        for name in ("head", "torso", "arm", "leg", "weapon"):
            piece = images[boss_id][name]
            enlarged = piece.resize((piece.width * part_scale,
                                     piece.height * part_scale),
                                    Image.Resampling.NEAREST)
            sheet.alpha_composite(enlarged, (x, y - 8))
            x += enlarged.width + 12
        pieces = [images[boss_id][name] for _, name, _ in PART_SLOTS]
        pose = render_pose(skeleton_dict(boss_id), pieces, None, 0, 1.2)
        sheet.alpha_composite(pose, (600, y - 26))
    return sheet


def main():
    parser = argparse.ArgumentParser(description="Generate boss_* skeleton parts.")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "assets" / "sprites")
    parser.add_argument("--skeleton-dir", type=Path,
                        default=ROOT / "resources" / "animations")
    parser.add_argument("--sheet", type=Path, default=ROOT / "reports" / "boss_sheet.png")
    options = parser.parse_args()
    images = generate(options.output_dir, options.skeleton_dir)
    options.sheet.parent.mkdir(parents=True, exist_ok=True)
    make_sheet(images).save(options.sheet)
    print(f"sheet -> {options.sheet}")


if __name__ == "__main__":
    main()