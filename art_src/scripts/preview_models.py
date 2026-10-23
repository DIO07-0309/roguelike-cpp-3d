# A14: 低模模型软件光栅预览 (不依赖 GPU/Blender, 纯几何检视用)
# 运行: python art_src/scripts/preview_models.py
# 输出: art_src/preview/models_preview.png  七个模型并排 (体素风无贴图, 顶点色 + 方向光)
import json
import math
import os

from PIL import Image, ImageDraw

REPO = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
)
MODEL_DIR = os.path.join(REPO, "assets", "models")
OUT_PATH = os.path.join(REPO, "art_src", "preview", "models_preview.png")
NAMES = ["npc_1", "npc_2", "npc_3", "npc_blacksmith", "chest", "altar", "spring"]

CELL_W, CELL_H = 220, 260
LIGHT = (0.45, 0.80, 0.55)
AMBIENT = 0.45


def _normalize(v):
    length = math.sqrt(sum(c * c for c in v))
    return tuple(c / length for c in v) if length > 1e-6 else (0.0, 0.0, 1.0)


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def load_model(name):
    with open(os.path.join(MODEL_DIR, name + ".json"), encoding="utf-8") as handle:
        data = json.load(handle)
    return data["vertices"], data["faces"]


def draw_model(image, name, origin_x, origin_y):
    vertices, faces = load_model(name)
    if not vertices:
        return
    xs = [v[0] for v in vertices]
    ys = [v[1] for v in vertices]
    zs = [v[2] for v in vertices]
    center = (
        (min(xs) + max(xs)) * 0.5,
        (min(ys) + max(ys)) * 0.5,
        (min(zs) + max(zs)) * 0.5,
    )
    height = max(max(ys) - min(ys), 1.0)
    width = max(max(xs) - min(xs), 1.0)
    depth = max(max(zs) - min(zs), 1.0)
    distance = math.sqrt(width * width + height * height + depth * depth) * 1.35
    eye = (
        center[0] + distance * 0.62,
        center[1] + distance * 0.45,
        center[2] + distance * 0.85,
    )
    forward = _normalize((center[0] - eye[0], center[1] - eye[1], center[2] - eye[2]))
    right = _normalize(_cross(forward, (0.0, 1.0, 0.0)))
    up = _cross(right, forward)
    focal = CELL_H * 1.5

    pixels = image.load()
    depth = [[1e9] * CELL_W for _ in range(CELL_H)]
    for face in faces:
        pts = [vertices[i] for i in face]
        world = [(p[0] - center[0], p[1] - center[1], p[2] - center[2]) for p in pts]
        normal = _normalize(
            _cross(
                (
                    world[1][0] - world[0][0],
                    world[1][1] - world[0][1],
                    world[1][2] - world[0][2],
                ),
                (
                    world[2][0] - world[0][0],
                    world[2][1] - world[0][1],
                    world[2][2] - world[0][2],
                ),
            )
        )
        lambert = max(0.0, _dot(normal, _normalize(LIGHT)))
        shade = AMBIENT + (1.0 - AMBIENT) * lambert
        # A14: 顶点色已烘焙方向明暗 (与 A13 体素同一套约定) — 预览直接用, 不再叠光
        color = tuple(min(255, int(pts[0][3 + i])) for i in range(3))
        screen = []
        for p in world:
            d = (
                p[0] - (eye[0] - center[0]),
                p[1] - (eye[1] - center[1]),
                p[2] - (eye[2] - center[2]),
            )
            z = _dot(d, forward)
            if z <= 1.0:
                screen = []
                break
            sx = origin_x + CELL_W * 0.5 + _dot(d, right) * focal / z
            sy = origin_y + CELL_H * 0.78 - _dot(d, up) * focal / z
            screen.append((sx, sy, z))
        if len(screen) != 3:
            continue
        _rasterize(pixels, depth, screen, color, origin_x)


def _rasterize(pixels, depth, tri, color, origin_x):
    """格子内局部坐标 (x 相对 origin_x), z 缓冲裁剪在本格内"""
    (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = tri
    area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
    if abs(area) < 1e-6:
        return
    min_x = max(0, int(min(x0, x1, x2)) - origin_x)
    max_x = min(CELL_W - 1, int(max(x0, x1, x2)) - origin_x)
    min_y = max(0, int(min(y0, y1, y2)))
    max_y = min(CELL_H - 1, int(max(y0, y1, y2)))
    for y in range(min_y, max_y + 1):
        for x in range(min_x, max_x + 1):
            px, py = x + 0.5 + origin_x, y + 0.5
            w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area
            w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area
            w2 = 1.0 - w0 - w1
            if w0 < 0 or w1 < 0 or w2 < 0:
                continue
            z = w0 * z0 + w1 * z1 + w2 * z2
            if z >= depth[y][x]:
                continue
            depth[y][x] = z
            pixels[x + origin_x, y] = color


def main():
    sheet = Image.new("RGB", (CELL_W * len(NAMES), CELL_H + 24), (32, 32, 38))
    draw = ImageDraw.Draw(sheet)
    for index, name in enumerate(NAMES):
        draw_model(sheet, name, index * CELL_W, 0)
        draw.text((index * CELL_W + 8, CELL_H + 6), name, fill=(220, 220, 225))
    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    sheet.save(OUT_PATH)
    print("wrote %s" % OUT_PATH)


if __name__ == "__main__":
    main()
