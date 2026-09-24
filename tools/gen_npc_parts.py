# A6-S2 批次6: NPC 骨骼分件生成器
# 复用 humanoid rig/管道; 产出 npc_{id}_part_*.png + npc_{id}_skeleton.json
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from PIL import Image, ImageDraw

from anim_preview import checkerboard, render_pose
from gen_mon_humanoid_parts import (
    BONES, OUTLINE, PART_ROWS, PART_SLOTS, SIZES, TIERS, WEAPONS, scale_rows,
)

ROOT = Path(__file__).resolve().parents[1]

NPC_FAMILIES = {
    "npc_prisoner": {"s": (52, 48, 44), "m": (92, 86, 76), "l": (134, 126, 112),
                     "h": (182, 172, 154), "r": (88, 70, 52), "R": (128, 102, 74),
                     "p": (170, 150, 120), "b": (36, 32, 28), "g": (96, 88, 70)},
    "npc_hunter": {"s": (58, 42, 28), "m": (104, 74, 48), "l": (152, 112, 72),
                   "h": (206, 160, 108), "r": (78, 56, 36), "R": (118, 86, 54),
                   "p": (196, 152, 100), "b": (34, 26, 18), "g": (96, 88, 70)},
    "npc_survivor": {"s": (48, 46, 42), "m": (88, 84, 76), "l": (128, 122, 110),
                     "h": (174, 166, 150), "r": (72, 68, 60), "R": (108, 102, 90),
                     "p": (160, 152, 136), "b": (30, 28, 26), "g": (96, 88, 70)},
    "npc_collector": {"s": (72, 48, 24), "m": (128, 86, 40), "l": (186, 132, 60),
                      "h": (236, 186, 104), "r": (110, 72, 32), "R": (156, 106, 48),
                      "p": (224, 168, 88), "b": (40, 28, 16), "g": (96, 88, 70)},
    "npc_priest": {"s": (120, 116, 96), "m": (188, 184, 160), "l": (232, 228, 204),
                   "h": (252, 250, 236), "r": (168, 140, 60), "R": (220, 188, 90),
                   "p": (248, 236, 180), "b": (54, 50, 42), "g": (96, 88, 70)},
    "npc_scout": {"s": (40, 58, 36), "m": (70, 104, 54), "l": (110, 152, 80),
                  "h": (156, 198, 118), "r": (54, 78, 44), "R": (86, 120, 64),
                  "p": (140, 184, 104), "b": (28, 36, 24), "g": (96, 88, 70)},
    "npc_pilgrim": {"s": (66, 62, 56), "m": (112, 106, 96), "l": (160, 152, 138),
                    "h": (210, 202, 186), "r": (90, 82, 70), "R": (132, 122, 106),
                    "p": (188, 178, 160), "b": (40, 36, 32), "g": (96, 88, 70)},
    "npc_ghost": {"s": (92, 118, 128), "m": (148, 186, 198), "l": (200, 230, 240),
                  "h": (238, 252, 255), "r": (120, 150, 162), "R": (170, 206, 220),
                  "p": (220, 244, 252), "b": (48, 62, 70), "g": (96, 88, 70)},
    "npc_dreamer": {"s": (52, 36, 72), "m": (92, 64, 126), "l": (140, 104, 180),
                    "h": (194, 158, 230), "r": (76, 52, 104), "R": (118, 84, 156),
                    "p": (176, 136, 220), "b": (28, 20, 40), "g": (96, 88, 70)},
    "npc_watcher": {"s": (36, 56, 72), "m": (58, 98, 124), "l": (96, 148, 178),
                    "h": (150, 204, 232), "r": (70, 120, 90), "R": (100, 166, 128),
                    "p": (180, 230, 210), "b": (28, 38, 48), "g": (96, 88, 70)},
}

NPCS = {
    "npc_20": ("npc_prisoner", "standard", None),
    "npc_30": ("npc_hunter", "standard", "dagger"),
    "npc_40": ("npc_survivor", "standard", None),
    "npc_60": ("npc_collector", "standard", None),
    "npc_70": ("npc_priest", "standard", "staff"),
    "npc_80": ("npc_scout", "runt", "dagger"),
    "npc_90": ("npc_pilgrim", "gaunt", None),
    "npc_110": ("npc_ghost", "gaunt", None),
    "npc_120": ("npc_dreamer", "standard", None),
    "npc_140": ("npc_watcher", "bulk", "cleaver"),
}
assert len(NPCS) == 10, "NPCS must have exactly 10 entries"


def build_palette(family):
    palette = {".": (0, 0, 0, 0), "o": OUTLINE}
    for char, rgb in NPC_FAMILIES[family].items():
        palette[char] = (*rgb, 255)
    return palette


def part_grid(npc_id, part_name):
    family, tier, weapon = NPCS[npc_id]
    if part_name == "weapon":
        if weapon is None:
            w, h = SIZES["weapon"]
            return ["." * w for _ in range(h)]
        rows = list(WEAPONS[weapon])
    else:
        rows = list(PART_ROWS[part_name])
    factor_x, factor_y, _ = TIERS[tier]
    return scale_rows(rows, factor_x, factor_y)


def part_image(npc_id, part_name):
    family = NPCS[npc_id][0]
    palette = build_palette(family)
    rows = part_grid(npc_id, part_name)
    width, height = SIZES[part_name]
    if len(rows) != height or any(len(r) != width for r in rows):
        raise ValueError(f"Invalid pixel grid: {npc_id} {part_name}")
    image = Image.new("RGBA", (width, height))
    image.putdata([palette[pixel] for row in rows for pixel in row])
    if not {p[3] for p in image.getdata()} <= {0, 255}:
        raise ValueError(f"Non-binary alpha: {npc_id} {part_name}")
    return image


def skeleton_dict(npc_id):
    _, tier, _ = NPCS[npc_id]
    ppu = TIERS[tier][2]
    parts = [{"bone": bone,
              "file": f"assets/sprites/{npc_id}_part_{name}.png",
              "pivot": list(pivot)} for bone, name, pivot in PART_SLOTS]
    return {"pixels_per_unit": ppu, "anchor": [24, 62],
            "bones": BONES, "parts": parts}


def generate(output_dir, skeleton_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    skeleton_dir.mkdir(parents=True, exist_ok=True)
    for npc_id in NPCS:
        images = {name: part_image(npc_id, name) for name in SIZES}
        for name, image in images.items():
            image.save(output_dir / f"{npc_id}_part_{name}.png")
        path = skeleton_dir / f"{npc_id}_skeleton.json"
        path.write_text(json.dumps(skeleton_dict(npc_id), indent=2) + "\n",
                        encoding="utf-8")
        print(f"wrote {npc_id} parts+skeleton")
    return {nid: {name: part_image(nid, name) for name in SIZES}
            for nid in NPCS}


def make_sheet(images):
    row_h, part_scale, canvas_w = 132, 3, 640
    sheet = checkerboard((canvas_w, row_h * len(NPCS) + 20))
    draw = ImageDraw.Draw(sheet)
    for index, npc_id in enumerate(NPCS):
        family, tier, weapon = NPCS[npc_id]
        y = index * row_h + 14
        draw.text((6, y - 10), npc_id, fill=(205, 220, 222))
        label = "empty" if weapon is None else weapon
        draw.text((6, y + 2), f"{tier} ppu={TIERS[tier][2]} {label}",
                  fill=(149, 166, 183))
        x = 178
        for name in ("head", "torso", "arm", "leg", "weapon"):
            piece = images[npc_id][name]
            enlarged = piece.resize((piece.width * part_scale,
                                     piece.height * part_scale),
                                    Image.Resampling.NEAREST)
            sheet.alpha_composite(enlarged, (x, y - 6))
            x += enlarged.width + 12
        pieces = [images[npc_id][name] for _, name, _ in PART_SLOTS]
        pose = render_pose(skeleton_dict(npc_id), pieces, None, 0, 1.6)
        sheet.alpha_composite(pose, (500, y - 16))
    return sheet


def main():
    parser = argparse.ArgumentParser(description="Generate npc_* skeleton parts.")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "assets" / "sprites")
    parser.add_argument("--skeleton-dir", type=Path,
                        default=ROOT / "resources" / "animations")
    parser.add_argument("--sheet", type=Path, default=ROOT / "reports" / "npc_sheet.png")
    args = parser.parse_args()
    images = generate(args.output_dir, args.skeleton_dir)
    args.sheet.parent.mkdir(parents=True, exist_ok=True)
    make_sheet(images).save(args.sheet)
    print(f"sheet -> {args.sheet}")


if __name__ == "__main__":
    main()
