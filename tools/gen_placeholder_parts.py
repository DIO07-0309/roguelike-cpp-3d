# A5-T1: 生成玩家骨骼占位分件 (纯色+深边, 真素材 T5 替换)
from PIL import Image
import os

OUT = os.path.join("assets", "sprites")
PARTS = {
    "player_part_head.png":   (20, 20, (60, 200, 130)),
    "player_part_torso.png":  (24, 28, (46, 160, 90)),
    "player_part_arm.png":    (8, 22, (40, 130, 75)),
    "player_part_leg.png":    (10, 28, (28, 100, 60)),
    "player_part_weapon.png": (6, 40, (190, 190, 210)),
}

def darker(c, k):
    return tuple(int(v * k) for v in c)

def main():
    for name, (w, h, c) in PARTS.items():
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        px = img.load()
        for x in range(w):
            for y in range(h):
                edge = x < 1 or y < 1 or x >= w - 1 or y >= h - 1
                px[x, y] = (*darker(c, 0.6), 255) if edge else (*c, 255)
        img.save(os.path.join(OUT, name))
        print("wrote", name, img.size)

if __name__ == "__main__":
    main()
