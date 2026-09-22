# A6-S2 批次2: 软体族 (史莱姆类) 骨骼分件生成器
# 复用 humanoid rig/管道: PART_SLOTS/SIZES/BONES 完全一致, 仅部件像素为软体形状.
# 产出 mon_{id}_part_{torso,head,arm,leg,weapon}.png + mon_{id}_skeleton.json
import argparse
import json
from pathlib import Path

from PIL import Image

from anim_preview import checkerboard, render_pose
from gen_mon_humanoid_parts import (BONES, PART_SLOTS, SIZES,
                                    skeleton_dict)

ROOT = Path(__file__).resolve().parents[1]
OUTLINE = (63, 38, 49, 255)

# 软体色板 (通道约定同 humanoid: s暗 m中 l亮 h高光 r深R红 p浅 b深 g绿)
FAMILIES = {
    "slime_green":  {"s": (54, 84, 46),  "m": (92, 128, 66),  "l": (138, 176, 96),
                     "h": (188, 218, 140), "r": (60, 100, 52), "R": (96, 150, 72),
                     "p": (156, 196, 120), "b": (46, 40, 34),  "g": (110, 96, 66)},
    "bomber_red":   {"s": (88, 40, 36),  "m": (140, 60, 48),  "l": (196, 96, 70),
                     "h": (240, 150, 92), "r": (150, 34, 40), "R": (216, 66, 52),
                     "p": (250, 148, 96), "b": (40, 26, 26),  "g": (110, 60, 44)},
    "elite_purple": {"s": (50, 40, 66),  "m": (84, 62, 110),  "l": (130, 96, 160),
                     "h": (184, 150, 214), "r": (70, 44, 100), "R": (110, 70, 150),
                     "p": (160, 116, 200), "b": (36, 30, 48), "g": (100, 84, 66)},
    "frost_blue":   {"s": (40, 66, 86),  "m": (64, 108, 140), "l": (110, 158, 196),
                     "h": (170, 210, 236), "r": (52, 84, 112), "R": (86, 128, 164),
                     "p": (150, 190, 220), "b": (34, 42, 54), "g": (92, 84, 68)},
    "leech_blood":  {"s": (76, 32, 40),  "m": (120, 44, 54),  "l": (168, 68, 72),
                     "h": (214, 104, 96), "r": (130, 30, 38), "R": (188, 52, 50),
                     "p": (232, 116, 92), "b": (38, 24, 28),  "g": (108, 92, 66)},
}

MONSTERS = {
    "slime": "slime_green",
    "bomber": "bomber_red",
    "elite_slime": "elite_purple",
    "frost_slime": "frost_blue",
    "blood_leech": "leech_blood",
}

# 软体部件像素行 (长度宽松, fit 到 SIZES 画布; 内容居中/靠上)
SOFT_ROWS = {
    "torso": [
        "........oooooooooo..........",
        "......oohhhhhhhhhoo.........",
        ".....ohhhllllllllho.........",
        "....ohlmmmlllllllllo........",
        "...ohlmmmmmllllllllllo......",
        "..ohlmmmmmmlllllllllllo.....",
        "..olmmmmmmmlllllllllllho....",
        ".olmmmmmmmlllllllllllllho...",
        ".ommmlmmmllllllllllllllho...",
        ".ommllmmmllllllllllllllho...",
        ".omllllmmmlllllllllllllho...",
        ".omlllllmmmllllllllllllho...",
        ".ollllllmmmlllllllllllllo...",
        ".ollllllmmmmllllllllllho....",
        "..ollllmmmmmmmllllllllho....",
        "..olllllmmmmmmmlllho........",
        "...olllllmmmmmmllho.........",
        "....olllllmmmmho............",
        ".....ooollmmooo............",
        ".......oooooo..............",
    ],
    "head": [
        "oooooooooooooooooooo",
        "ohhhhhhhhhhhhhhhhhoo",
        "ohlllllllllllllllho.",
        "ohlmmmmmmllllllllho.",
        "ohlmmmmmmlllllllho..",
        "olllllllllllllllho..",
        "ooooooooooooooooo...",
    ],
    "arm": [
        "..ooooo..",
        ".olllho..",
        ".olmmmo..",
        "..olmmo..",
        "...omso..",
        "...ollo..",
        "...ollo..",
        "...ollo..",
        "...ollo..",
        "...ollo..",
        "...ollo..",
        "...ommo..",
        "...osso..",
        "....oo...",
    ],
    "leg": [
        "..oooooooooooooo..",
        ".omllllllllllllmo.",
        ".ommmmmmmmmmmmmmo.",
        ".osmmmmmmmmmmmmso.",
        ".osmmmmmmmmmmmmso.",
        ".osmmmmmmmmmmmmso.",
        "..osmmmmmmmmmmso..",
        "..osmmmmmmmmmso...",
        "...osmmmmmmso.....",
        "....osmmmmso......",
        ".....osmmso.......",
        "......ommo........",
        "......oggo........",
        "......oooo........",
    ],
    "weapon": [
        "..oo..",
        ".oggo.",
        ".orro.",
        ".ogro.",
        ".orro.",
        ".orro.",
        "..oo..",
    ],
}


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
    family = MONSTERS[monster_id]
    pal = palette_for(family)
    width, height = SIZES[part_name]
    rows = fit_rows(SOFT_ROWS[part_name], width, height)
    image = Image.new("RGBA", (width, height))
    image.putdata([pal[p] for row in rows for p in row])
    return image


def soft_skeleton_dict(monster_id):
    # 软体骨架结构与 humanoid 一致 (BONES/PART_SLOTS pivot), ppu 固定 0.8
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
            json.dumps(soft_skeleton_dict(monster_id), indent=2) + "\n", encoding="utf-8")
        print(f"wrote mon_{monster_id} parts+skeleton")
    return {mid: {n: part_image(mid, n) for n in SIZES} for mid in MONSTERS}


def make_sheet(images):
    row_h, part_scale, canvas_w = 132, 3, 640
    sheet = checkerboard((canvas_w, row_h * len(MONSTERS) + 20))
    for idx, m_id in enumerate(MONSTERS):
        y = idx * row_h + 14
        x = 178
        for name in ("torso", "head", "leg", "weapon", "arm"):
            piece = images[m_id][name]
            enlarged = piece.resize((piece.width * part_scale, piece.height * part_scale),
                                    Image.Resampling.NEAREST)
            sheet.alpha_composite(enlarged, (x, y - 6))
            x += enlarged.width + 12
        pieces = [images[m_id][n] for _, n, _ in PART_SLOTS]
        pose = render_pose(soft_skeleton_dict(m_id), pieces, None, 0, 1.6)
        sheet.alpha_composite(pose, (500, y - 16))
    return sheet


def main():
    parser = argparse.ArgumentParser(description="Generate mon_* soft-body skeleton parts.")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "assets" / "sprites")
    parser.add_argument("--skeleton-dir", type=Path, default=ROOT / "resources" / "animations")
    parser.add_argument("--sheet", type=Path, default=ROOT / "reports" / "mon_soft_sheet.png")
    args = parser.parse_args()
    images = generate(args.output_dir, args.skeleton_dir)
    args.sheet.parent.mkdir(parents=True, exist_ok=True)
    make_sheet(images).save(args.sheet)
    print(f"sheet -> {args.sheet}")


if __name__ == "__main__":
    main()
