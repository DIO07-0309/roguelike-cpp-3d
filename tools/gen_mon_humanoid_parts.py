import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw

from anim_preview import checkerboard, render_pose
from gen_player_knight_parts import PART_ROWS, SIZES

ROOT = Path(__file__).resolve().parents[1]
OUTLINE = (63, 38, 49, 255)
FAMILIES = {
    "orc_green": {"s": (58, 84, 44), "m": (96, 128, 60), "l": (140, 172, 84),
                  "h": (186, 208, 126), "r": (124, 58, 40), "R": (168, 84, 52),
                  "p": (212, 124, 74), "b": (52, 44, 36), "g": (124, 96, 64)},
    "elite_red": {"s": (48, 36, 42), "m": (86, 52, 58), "l": (134, 72, 76),
                  "h": (196, 92, 86), "r": (154, 32, 40), "R": (214, 58, 52),
                  "p": (248, 140, 96), "b": (30, 22, 28), "g": (96, 54, 46)},
    "goblin_earth": {"s": (70, 62, 38), "m": (112, 98, 58), "l": (156, 138, 86),
                     "h": (196, 180, 124), "r": (110, 82, 44), "R": (150, 116, 64),
                     "p": (196, 158, 96), "b": (48, 40, 32), "g": (120, 102, 66)},
    "goblin_moss": {"s": (70, 62, 38), "m": (112, 98, 58), "l": (156, 138, 86),
                    "h": (196, 180, 124), "r": (64, 96, 44), "R": (100, 148, 64),
                    "p": (156, 196, 102), "b": (48, 40, 32), "g": (120, 102, 66)},
    "goblin_mystic": {"s": (70, 62, 38), "m": (112, 98, 58), "l": (156, 138, 86),
                      "h": (196, 180, 124), "r": (92, 58, 124), "R": (140, 88, 186),
                      "p": (196, 150, 224), "b": (48, 40, 52), "g": (104, 92, 64)},
    "guard_steel": {"s": (44, 58, 76), "m": (74, 96, 124), "l": (116, 144, 176),
                    "h": (178, 204, 228), "r": (52, 64, 84), "R": (84, 104, 130),
                    "p": (120, 148, 178), "b": (36, 42, 54), "g": (92, 84, 68)},
    "bone_white": {"s": (126, 120, 98), "m": (176, 170, 144), "l": (214, 208, 182),
                   "h": (240, 236, 214), "r": (122, 84, 54), "R": (162, 116, 70),
                   "p": (206, 160, 102), "b": (70, 64, 52), "g": (110, 96, 72)},
}
MONSTERS = {
    "orc": ("orc_green", "standard", "cleaver"),
    "elite_orc": ("elite_red", "standard", "cleaver"),
    "archer": ("goblin_earth", "runt", "bow"),
    "shaman": ("goblin_mystic", "runt", "staff"),
    "goblin_hunter": ("goblin_moss", "runt", "dagger"),
    "tank": ("guard_steel", "bulk", "greatsword"),
    "bone_soldier": ("bone_white", "gaunt", "sword"),
    "skeleton_archer": ("bone_white", "gaunt", "bow"),
}
TIERS = {"standard": (1.0, 1.0, 0.5), "runt": (1.0, 1.0, 0.45),
         "bulk": (1.16, 1.05, 0.5), "gaunt": (0.80, 1.06, 0.5)}
BONES = [
    {"name": "root"},
    {"name": "hips", "parent": "root", "y": 20},
    {"name": "torso", "parent": "hips", "y": 6},
    {"name": "head", "parent": "torso", "y": 11},
    {"name": "arm_back", "parent": "torso", "x": -3, "y": 9},
    {"name": "weapon", "parent": "arm_back", "y": -5},
    {"name": "arm_front", "parent": "torso", "x": 4, "y": 9},
    {"name": "leg_front", "parent": "hips", "x": 3},
    {"name": "leg_back", "parent": "hips", "x": -3},
]
PART_SLOTS = [("leg_back", "leg", [6, 20]), ("arm_back", "arm", [4, 16]),
              ("weapon", "weapon", [3, 24]), ("torso", "torso", [13, 10]),
              ("head", "head", [11, 7]), ("arm_front", "arm", [4, 16]),
              ("leg_front", "leg", [6, 20])]
CLEAVER = ("oooooo|ohlllm|ohlllm|ohlllm|ohllmm|ohllmm|ohllmm|ohllmm|ohllmm|"
           "orrmmm|orrrmm|.oooo.|.ogo..|.ogo..|.ogo..|.ogo..|.ogo..|.ogo..|"
           ".ogo..|.ogo..|.ogo..|.obbo.|.obbo.|..oo..")
BOW = ("..ooml|.ohhol|.ohhol|.ohhol|.ohhol|.ohhol|.ohhol|.ohrll|.ohrbl|"
       ".ohrbl|.ohrll|.ohhol|.ohhol|.ohhol|.ohhol|.ohhol|.ohhol|.ohhol|"
        ".ohhol|.ohhol|.ohhol|.ohhol|.ohhml|..ooml")
STAFF = ("..oo..|.opho.|oppppo|ophppo|.oppo.|..oo..|..gg..|..gg..|..gg..|"
         "..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|"
         "..gg..|..gg..|..gg..|..gg..|..go..|..oo..")
DAGGER = ("......|......|......|......|......|......|......|......|......|"
          "......|..oo..|.ohho.|.ohho.|.ohho.|.ohho.|.ohho.|.ohho.|.ohho.|"
          "oooooo|.obbo.|.obbo.|.obbo.|.obbo.|..oo..")
GREATSWORD = ("..oooo|.ohhlo|.ohhlo|.ohhlo|.ohhlo|.ohhlo|.ohhlo|.ohhlo|"
              ".ohhlo|.ohhlo|.ohhlo|.ohhlo|.ohhlo|orrrro|oooooo|.oggo.|"
              ".oggo.|.oggo.|.oggo.|.oggo.|.obbo.|.obbo.|.obbo.|..oo..")
WEAPONS = {"sword": list(PART_ROWS["weapon"]), "cleaver": CLEAVER.split("|"),
           "bow": BOW.split("|"), "staff": STAFF.split("|"),
           "dagger": DAGGER.split("|"), "greatsword": GREATSWORD.split("|")}


def build_palette(family):
    palette = {".": (0, 0, 0, 0), "o": OUTLINE}
    for char, rgb in FAMILIES[family].items():
        palette[char] = (*rgb, 255)
    return palette


def _bbox(rows):
    def filled(row):
        return any(c != "." for c in row)
    top = next(i for i, r in enumerate(rows) if filled(r))
    bottom = next(i for i in range(len(rows) - 1, -1, -1) if filled(rows[i]))
    left = min(min(i for i, c in enumerate(r) if c != ".")
               for r in rows[top:bottom + 1] if filled(r))
    right = max(max(i for i, c in enumerate(r) if c != ".")
                for r in rows[top:bottom + 1] if filled(r))
    return top, bottom, left, right


def scale_rows(rows, factor_x, factor_y):
    height, width = len(rows), len(rows[0])
    if factor_x == 1.0 and factor_y == 1.0:
        return list(rows)
    top, bottom, left, right = _bbox(rows)
    bbox_h, bbox_w = bottom - top + 1, right - left + 1
    new_h = min(height, max(1, round(bbox_h * factor_y)))
    new_w = min(width, max(1, round(bbox_w * factor_x)))
    y0 = max(0, min(bottom - new_h + 1, height - new_h))
    x0 = max(0, min(left + (bbox_w - new_w) // 2, width - new_w))
    out = [["." for _ in range(width)] for _ in range(height)]
    for yy in range(new_h):
        for xx in range(new_w):
            sy = top + min(bbox_h - 1, round(yy * (bbox_h - 1) / max(1, new_h - 1)))
            sx = left + min(bbox_w - 1, round(xx * (bbox_w - 1) / max(1, new_w - 1)))
            out[y0 + yy][x0 + xx] = rows[sy][sx]
    return ["".join(r) for r in out]


def part_grid(monster_id, part_name):
    family, tier, weapon = MONSTERS[monster_id]
    rows = WEAPONS[weapon] if part_name == "weapon" else PART_ROWS[part_name]
    factor_x, factor_y, _ = TIERS[tier]
    return scale_rows(list(rows), factor_x, factor_y)


def part_image(monster_id, part_name):
    family = MONSTERS[monster_id][0]
    palette = build_palette(family)
    rows = part_grid(monster_id, part_name)
    width, height = SIZES[part_name]
    if len(rows) != height or any(len(r) != width for r in rows):
        raise ValueError(f"Invalid pixel grid: {monster_id} {part_name}")
    image = Image.new("RGBA", (width, height))
    image.putdata([palette[pixel] for row in rows for pixel in row])
    if not {p[3] for p in image.getdata()} <= {0, 255}:
        raise ValueError(f"Non-binary alpha: {monster_id} {part_name}")
    return image


def skeleton_dict(monster_id):
    _, tier, _ = MONSTERS[monster_id]
    ppu = TIERS[tier][2]
    parts = [{"bone": bone,
              "file": f"assets/sprites/mon_{monster_id}_part_{name}.png",
              "pivot": list(pivot)} for bone, name, pivot in PART_SLOTS]
    return {"pixels_per_unit": ppu, "anchor": [24, 62],
            "bones": BONES, "parts": parts}


def generate(output_dir, skeleton_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    skeleton_dir.mkdir(parents=True, exist_ok=True)
    for monster_id in MONSTERS:
        images = {name: part_image(monster_id, name) for name in SIZES}
        for name, image in images.items():
            destination = output_dir / f"mon_{monster_id}_part_{name}.png"
            image.save(destination)
        path = skeleton_dir / f"mon_{monster_id}_skeleton.json"
        path.write_text(json.dumps(skeleton_dict(monster_id), indent=2) + "\n",
                        encoding="utf-8")
        print(f"wrote mon_{monster_id} parts+skeleton")
    return {mid: {name: part_image(mid, name) for name in SIZES}
            for mid in MONSTERS}


def make_sheet(images):
    row_h, part_scale, canvas_w = 132, 3, 640
    sheet = checkerboard((canvas_w, row_h * len(MONSTERS) + 20))
    draw = ImageDraw.Draw(sheet)
    for index, monster_id in enumerate(MONSTERS):
        family, tier, weapon = MONSTERS[monster_id]
        y = index * row_h + 14
        draw.text((6, y - 10), monster_id, fill=(205, 220, 222))
        draw.text((6, y + 2), f"{tier} ppu={TIERS[tier][2]} {weapon}",
                  fill=(149, 166, 183))
        x = 178
        for name in ("head", "torso", "arm", "leg", "weapon"):
            piece = images[monster_id][name]
            enlarged = piece.resize((piece.width * part_scale,
                                     piece.height * part_scale),
                                    Image.Resampling.NEAREST)
            sheet.alpha_composite(enlarged, (x, y - 6))
            x += enlarged.width + 12
        pieces = [images[monster_id][name] for _, name, _ in PART_SLOTS]
        pose = render_pose(skeleton_dict(monster_id), pieces, None, 0, 1.6)
        sheet.alpha_composite(pose, (500, y - 16))
    return sheet


def main():
    parser = argparse.ArgumentParser(
        description="Generate mon_* humanoid skeleton parts from knight template.")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "assets" / "sprites")
    parser.add_argument("--skeleton-dir", type=Path,
                        default=ROOT / "resources" / "animations")
    parser.add_argument("--sheet", type=Path,
                        default=ROOT / "reports" / "mon_humanoid_sheet.png")
    options = parser.parse_args()
    images = generate(options.output_dir, options.skeleton_dir)
    options.sheet.parent.mkdir(parents=True, exist_ok=True)
    make_sheet(images).save(options.sheet)
    print(f"sheet -> {options.sheet}")


if __name__ == "__main__":
    main()
