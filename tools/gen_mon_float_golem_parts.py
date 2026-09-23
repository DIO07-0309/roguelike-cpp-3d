# A6-S2 批次3: 浮灵/魔像族骨骼分件生成器 (双造型: golem 石躯 + float 能量体)
# 复用 humanoid rig (BONES/PART_SLOTS/pivot), ppu 固定 0.8 (磁盘全量约定 9e6d6fc).
# 产出 mon_{id}_part_{torso,head,arm,leg,weapon}.png + mon_{id}_skeleton.json
import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw

from anim_preview import checkerboard, render_pose
from gen_mon_humanoid_parts import BONES, PART_SLOTS, SIZES

ROOT = Path(__file__).resolve().parents[1]
OUTLINE = (63, 38, 49, 255)

# 色板通道同 humanoid/soft: s暗 m中 l亮 h高光 r/R深浅 accent p亮 accent b暗 g柄
FAMILIES = {
    "golem_granite": {"s": (74, 66, 58), "m": (120, 108, 94), "l": (164, 152, 136),
                      "h": (206, 196, 180), "r": (96, 86, 74), "R": (140, 128, 112),
                      "p": (180, 170, 154), "b": (48, 42, 36), "g": (120, 96, 64)},
    "guard_moss": {"s": (60, 68, 58), "m": (100, 110, 96), "l": (140, 152, 134),
                   "h": (180, 194, 172), "r": (76, 110, 58), "R": (110, 150, 78),
                   "p": (150, 190, 110), "b": (44, 52, 42), "g": (96, 130, 64)},
    "sentinel_iron": {"s": (54, 60, 74), "m": (90, 100, 120), "l": (134, 146, 170),
                      "h": (186, 196, 216), "r": (70, 80, 104), "R": (110, 124, 152),
                      "p": (150, 166, 196), "b": (38, 42, 54), "g": (88, 98, 124)},
    "orb_electric": {"s": (120, 100, 30), "m": (190, 170, 60), "l": (240, 225, 110),
                     "h": (255, 250, 190), "r": (160, 140, 40), "R": (220, 205, 80),
                     "p": (255, 240, 140), "b": (70, 58, 28), "g": (140, 120, 50)},
    "imp_flame": {"s": (110, 40, 26), "m": (170, 70, 36), "l": (230, 110, 52),
                  "h": (255, 180, 96), "r": (150, 44, 28), "R": (230, 90, 40),
                  "p": (255, 150, 70), "b": (54, 26, 20), "g": (140, 70, 34)},
    "storm_violet": {"s": (44, 50, 96), "m": (76, 88, 156), "l": (120, 138, 210),
                     "h": (200, 210, 255), "r": (60, 70, 140), "R": (104, 116, 190),
                     "p": (160, 170, 240), "b": (30, 34, 64), "g": (88, 96, 168)},
    "void_abyss": {"s": (36, 24, 54), "m": (64, 42, 96), "l": (104, 70, 150),
                   "h": (178, 130, 230), "r": (54, 34, 84), "R": (92, 56, 140),
                   "p": (210, 150, 245), "b": (24, 16, 36), "g": (76, 48, 116)},
}

MONSTERS = {
    "golem": ("golem_granite", "golem"),
    "stone_guardian": ("guard_moss", "golem"),
    "iron_sentinel": ("sentinel_iron", "golem"),
    "lightning_orb": ("orb_electric", "float"),
    "fire_imp": ("imp_flame", "float"),
    "storm_elemental": ("storm_violet", "float"),
    "void_walker": ("void_abyss", "float"),
}

# golem 造型: 宽石板躯干+方块头(目缝)+柱臂拳+粗腿足座+石锤 (fit 到 SIZES 画布)
GOLEM_ROWS = {
    "torso": [
        "..oooooooooooooooooooooooo..",
        ".ohhhhhhhhhhhhhhhhhhhhhhhho.",
        ".ohllllllllllllllllllllllo..",
        "olllllllllllllllllllllllllo.",
        "olllllslmmmmmmmmmmmmmmllllo.",
        "olllllsmmmmmmmmmmmmmmmllllo.",
        "ollllssmmmmmmmmmmmmmmmllllo.",
        "ollllsmmmmmmsmmmmmmmmlllllo.",
        "ollllsmmmmmmsmmmmmmmmlllllo.",
        "olmmmmmmmmmmsmmmmmmmmlllllo.",
        "..olmmmmmmmmsmmmmmmmmlllo...",
        "..olmmmmmmmmsmmmmmmmllllo....",
        "..olmmmmmmmmmmmmmmmlllllo....",
        "..olllmmmmmmmmmmmmlllllo.....",
        "...osllllllllllllllllso......",
        "...osssssssssssssssssso......",
        "....osssssssssssssssso........",
        ".....oooooooooooooooooo......",
    ],
    "head": [
        "...oooooooooooo.....",
        "..ohhhhhhhhhhhho....",
        "..ohlmmmmmmmmmlho...",
        "..olmmmmmmmmmmmo....",
        "..obbbbbbbbbbbbbo...",
        "..obshhhhhhhhhsbo...",
        "..obbbbbbbbbbbbbo...",
        "..olmmmmmmmmmmmo....",
        "...olmmmmmmmmmlo....",
        "...oollmmmmmmloo....",
        ".....oooooooooo.....",
    ],
    "arm": [
        "..ooooooo...",
        ".ollllllo...",
        ".olmmmmllo..",
        ".olmmmmmlo..",
        ".olmmmmmlo..",
        ".olmmmmmlo..",
        ".olmmmmmlo..",
        ".olmmmmmlo..",
        ".olmmmmmlo..",
        ".osmmmmmso..",
        "olmmmmmmmo..",
        "olmmmmmmmo..",
        "olmmmmmmmo..",
        ".olmmmmmlo..",
        "..ooooooo...",
    ],
    "leg": [
        "....oooooooooooooo....",
        "...ollllllllllllllo...",
        "...olmmmmmmmmmmmmlo...",
        "...olmmmmmmmmmmmmlo...",
        "...olmmmmmmmmmmmmlo...",
        "...olmmmmmmmmmmmmlo...",
        "...osmmmmmmmmmmmmso...",
        "...osmmmmmmmmmmmmso...",
        "...osmmmmmmmmmmmmso...",
        "...osmmmmmmmmmmmmso...",
        "...osmmmmmmmmmmmmso...",
        "...osmmmmmmmmmmmmso...",
        "..oosmmmmmmmmmmmmsoo..",
        ".olllllllllllllllllo..",
        ".olmmmmmmmmmmmmmmmlo..",
        ".osmmmmmmmmmmmmmmmso..",
        ".osmmmmmmmmmmmmmmmso..",
        ".osssssssssssssssssso..",
        "..ossssssssssssssssso..",
        "..oooooooooooooooooo..",
    ],
    "weapon": ("oooooo|ohhhlo|ohhhlo|ohhhlo|ohhhlo|ohhhlo|olllho|.oooo.|"
               "..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|..gg..|"
               "..gg..|..gg..|..gg..|..gg..|.oggo.|.obbo.|..oo..|..oo..").split("|"),
}

# float 造型: 能量球躯干+焰冠头+细穗臂+垂地穗带腿+裂纹法杖 (ppu 0.8)
FLOAT_ROWS = {
    "torso": [
        "..........ommmmmmo..........",
        "........omllllllllmo........",
        ".......omlllllllllllo.......",
        "......omllllllllllllmo......",
        ".....omllllllhhhhllllmo.....",
        "....omllllllhhhhhhllllmo....",
        "....omllllhhhhhhhhllllmo....",
        "...omlllhhhhhhhhhhhhllmo...",
        "...omllhhhhhhhhhhhhhhllmo...",
        "...omllhhhhhhhhhhhhhhllmo...",
        "...omllhhhhhhhhhhhhhhllmo...",
        "...omlllhhhhhhhhhhhhhllmo...",
        "....omllllhhhhhhhhhhllllmo..",
        "....omllllllhhhhhhllllllmo..",
        "....omllllllllhhllllllllmo..",
        ".....omllllllllllllllllmo...",
        "......omllllllllllllllmo....",
        ".......oslllllllllllso......",
        ".........osllllllso.........",
        "...........osssso...........",
    ],
    "head": [
        "........oooooo........",
        ".......olhhhhlo.......",
        "......olhhhhhhlo......",
        ".......olhhhhlo.......",
        "........olhhlo........",
        ".........olmo.........",
        "..........oo..........",
    ],
    "arm": [
        "...ooooo...",
        "..olhhhlo..",
        "..olhhhlo..",
        "...olhho...",
        "...olmlo...",
        "...olmlo...",
        "...ollo....",
        "...omlo....",
        "...olso....",
        "...omso....",
        "...olso....",
        "...omso....",
        "...osso....",
        "....oo.....",
    ],
    "leg": [
        "....oooooooooo....",
        "...ollllllllllo...",
        "...olmmmmmmmmlo...",
        "....olmmmmmmlo.....",
        "....olmmmlllo......",
        ".....olmmlllo......",
        "......olmlllo......",
        ".......ollllo......",
        "........olmmo......",
        "........ollo.......",
        "........omlo.......",
        "........olo........",
        "........omo........",
        ".........oo........",
    ],
    "weapon": (".ohho.|.ohlo.|..oo..|.orro.|.orro.|..oo..|.ohlo.|.ohho.|"
               "..oo..|.orro.|.orro.|..oo..|.ohlo.|.ohho.|..oo..|.orro.|"
               ".orro.|..oo..|.ohlo.|.ohho.|.obbo.|.obbo.|..oo..|......").split("|"),
}

SHAPES = {"golem": GOLEM_ROWS, "float": FLOAT_ROWS}


def fit_rows(rows, width, height):
    out = []
    for y in range(height):
        row = rows[y] if y < len(rows) else ""
        if len(row) < width:
            row = row + "." * (width - len(row))
        elif len(row) > width:
            row = row[:width]
        out.append(row)
    return out


def palette_for(family):
    pal = {".": (0, 0, 0, 0), "o": OUTLINE}
    for ch, rgb in FAMILIES[family].items():
        pal[ch] = (*rgb, 255)
    return pal


def part_image(monster_id, part_name):
    family, shape = MONSTERS[monster_id]
    pal = palette_for(family)
    width, height = SIZES[part_name]
    rows = fit_rows(SHAPES[shape][part_name], width, height)
    image = Image.new("RGBA", (width, height))
    image.putdata([pal[ch] for row in rows for ch in row])
    return image


def skeleton_dict(monster_id):
    parts = [{"bone": bone, "file": f"assets/sprites/mon_{monster_id}_part_{name}.png",
              "pivot": list(pivot)} for bone, name, pivot in PART_SLOTS]
    return {"pixels_per_unit": 0.8, "anchor": [24, 62], "bones": BONES, "parts": parts}


def generate(output_dir, skeleton_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    skeleton_dir.mkdir(parents=True, exist_ok=True)
    for monster_id in MONSTERS:
        for name in SIZES:
            part_image(monster_id, name).save(
                output_dir / f"mon_{monster_id}_part_{name}.png")
        (skeleton_dir / f"mon_{monster_id}_skeleton.json").write_text(
            json.dumps(skeleton_dict(monster_id), indent=2) + "\n", encoding="utf-8")
        print(f"wrote mon_{monster_id} parts+skeleton")
    return {mid: {n: part_image(mid, n) for n in SIZES} for mid in MONSTERS}


def make_sheet(images):
    row_h, part_scale, canvas_w = 132, 3, 640
    sheet = checkerboard((canvas_w, row_h * len(MONSTERS) + 20))
    draw = ImageDraw.Draw(sheet)
    for idx, m_id in enumerate(MONSTERS):
        family, shape = MONSTERS[m_id]
        y = idx * row_h + 14
        draw.text((6, y - 8), m_id, fill=(205, 220, 222))
        draw.text((6, y + 4), family, fill=(149, 166, 183))
        x = 178
        for name in ("torso", "head", "leg", "weapon", "arm"):
            piece = images[m_id][name]
            enlarged = piece.resize((piece.width * part_scale, piece.height * part_scale),
                                    Image.Resampling.NEAREST)
            sheet.alpha_composite(enlarged, (x, y - 6))
            x += enlarged.width + 12
        pieces = [images[m_id][n] for _, n, _ in PART_SLOTS]
        pose = render_pose(skeleton_dict(m_id), pieces, None, 0, 1.6)
        sheet.alpha_composite(pose, (500, y - 16))
    return sheet


def main():
    parser = argparse.ArgumentParser(description="Generate mon_* float/golem skeleton parts.")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "assets" / "sprites")
    parser.add_argument("--skeleton-dir", type=Path, default=ROOT / "resources" / "animations")
    parser.add_argument("--sheet", type=Path, default=ROOT / "reports" / "mon_float_golem_sheet.png")
    args = parser.parse_args()
    images = generate(args.output_dir, args.skeleton_dir)
    args.sheet.parent.mkdir(parents=True, exist_ok=True)
    make_sheet(images).save(args.sheet)
    print(f"sheet -> {args.sheet}")


if __name__ == "__main__":
    main()
