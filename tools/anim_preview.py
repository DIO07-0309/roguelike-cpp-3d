# A5-T1: 按 bind pose 合成骨骼预览图 (不启动游戏查件位/锚点)
import json
import os
import sys
from PIL import Image, ImageDraw

SKEL = os.path.join("resources", "animations", "player_skeleton.json")
OUT = os.path.join("reports", "anim_preview.png")
CANVAS_W, CANVAS_H = 128, 128

def load(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

def bind_worlds(skel):
    idx = {b["name"]: i for i, b in enumerate(skel["bones"])}
    worlds = []
    for b in skel["bones"]:
        lx, ly = b.get("x", 0), b.get("y", 0)
        if b.get("parent") is not None:
            px, py = worlds[idx[b["parent"]]][0], worlds[idx[b["parent"]]][1]
            lx, ly = lx + px, ly + py
        worlds.append((lx, ly))
    return idx, worlds

def main():
    skel = load(SKEL)
    idx, worlds = bind_worlds(skel)
    img = Image.new("RGBA", (CANVAS_W * 2, CANVAS_H * 2), (24, 24, 36, 255))
    draw = ImageDraw.Draw(img)
    for part in skel["parts"]:
        path = os.path.join("assets", "sprites", part["file"])
        if not os.path.exists(path):
            print("MISSING", path)
            continue
        piece = Image.open(path).convert("RGBA")
        bx, by = worlds[idx[part["bone"]]]
        bx += part.get("dx", 0)
        by += part.get("dy", 0)
        px, py = part.get("pivot", [0, 0])
        scaled = piece.resize((piece.size[0] * 2, piece.size[1] * 2), Image.NEAREST)
        # 骨空间 y-up → 画布 y-down (2x): pivot 点 (底原点 py 高处) 对齐骨点
        ox = CANVAS_W + (bx - px) * 2
        oy = CANVAS_H - by * 2 - scaled.size[1] + py * 2
        img.alpha_composite(scaled, (int(ox), int(oy)))
        bone_x = CANVAS_W + bx * 2
        bone_y = CANVAS_H - by * 2
        draw.ellipse([bone_x - 2, bone_y - 2, bone_x + 2, bone_y + 2], fill=(255, 60, 60, 255))
    os.makedirs("reports", exist_ok=True)
    img.save(OUT)
    print("preview ->", OUT)

if __name__ == "__main__":
    main()
