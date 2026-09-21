import argparse
import fnmatch
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SKEL = ROOT / "resources" / "animations" / "player_skeleton.json"
ANIM = ROOT / "resources" / "animations" / "player_anim.json"
OUT = ROOT / "reports" / "anim_preview.png"
POSES = (("bind", 0), ("idle", 1.2), ("walk", 0), ("walk", 0.35),
         ("attack", 0.1), ("attack", 0.2))


def load(path):
    with open(path, "r", encoding="utf-8") as source:
        return json.load(source)


def resolve_part_path(filename, parts_dir=None):
    path = Path(filename)
    if parts_dir is not None:
        return Path(parts_dir) / path.name
    if path.is_absolute():
        return path
    if path.parent == Path("."):
        return ROOT / "assets" / "sprites" / path
    return ROOT / path


def sample_local(bone, clip, time):
    defaults = {"x": bone.get("x", 0), "y": bone.get("y", 0),
                "rot": 0, "sx": 1, "sy": 1}
    if clip is None:
        return defaults
    track = next((track for track in clip["tracks"] if track["bone"] == bone["name"]), None)
    if track is None:
        return defaults
    duration = clip["dur"]
    time = time % duration if clip.get("loop") else max(0, min(time, duration))
    keys = track["keys"]
    first, second = keys[0], keys[0]
    if time >= keys[-1]["t"]:
        first, second = keys[-1], keys[-1]
    elif time > keys[0]["t"]:
        first, second = next((left, right) for left, right in zip(keys, keys[1:])
                             if left["t"] <= time <= right["t"])
    span = second["t"] - first["t"]
    weight = (time - first["t"]) / span if span > 0.000001 else 0
    return {field: first.get(field, default) + weight *
            (second.get(field, default) - first.get(field, default))
            for field, default in defaults.items()}


def compose(parent, local):
    radians = math.radians(parent["rot"])
    cosine, sine = math.cos(radians), math.sin(radians)
    local_x, local_y = local["x"] * parent["sx"], local["y"] * parent["sy"]
    return {"x": parent["x"] + local_x * cosine - local_y * sine,
            "y": parent["y"] + local_x * sine + local_y * cosine,
            "rot": parent["rot"] + local["rot"],
            "sx": parent["sx"] * local["sx"], "sy": parent["sy"] * local["sy"]}


def pose_worlds(skeleton, clip=None, time=0):
    worlds = {}
    identity = {"x": 0, "y": 0, "rot": 0, "sx": 1, "sy": 1}
    for bone in skeleton["bones"]:
        parent = worlds[bone["parent"]] if bone.get("parent") else identity
        worlds[bone["name"]] = compose(parent, sample_local(bone, clip, time))
    return worlds


def bind_worlds(skeleton):
    worlds = pose_worlds(skeleton)
    indices = {bone["name"]: index for index, bone in enumerate(skeleton["bones"])}
    return indices, [(world["x"], world["y"]) for world in worlds.values()]


def piece_transform(piece, part, bone, origin, scale):
    radians = math.radians(bone["rot"])
    cosine, sine = math.cos(radians), math.sin(radians)
    offset_x = part.get("dx", 0) * bone["sx"]
    offset_y = part.get("dy", 0) * bone["sy"]
    bone_x = bone["x"] + offset_x * cosine - offset_y * sine
    bone_y = bone["y"] + offset_x * sine + offset_y * cosine
    anchor_x, anchor_y = origin[0] + bone_x * scale, origin[1] - bone_y * scale
    pivot_x, pivot_y = part.get("pivot", [0, 0])
    scale_x, scale_y = scale * bone["sx"], scale * bone["sy"]
    return (cosine / scale_x, -sine / scale_x,
            pivot_x - (cosine * anchor_x - sine * anchor_y) / scale_x,
            sine / scale_y, cosine / scale_y,
            piece.height - pivot_y - (sine * anchor_x + cosine * anchor_y) / scale_y)


def render_pose(skeleton, pieces, clip=None, time=0, scale=1, markers=False):
    canvas = Image.new("RGBA", (112, 112))
    origin = (56, 88)
    worlds = pose_worlds(skeleton, clip, time)
    for part, piece in zip(skeleton["parts"], pieces):
        bone = worlds[part["bone"]]
        matrix = piece_transform(piece, part, bone, origin, scale)
        layer = piece.transform(canvas.size, Image.Transform.AFFINE, matrix,
                                resample=Image.Resampling.NEAREST)
        canvas.alpha_composite(layer)
    if markers:
        draw = ImageDraw.Draw(canvas)
        for bone in worlds.values():
            center_x, center_y = origin[0] + bone["x"] * scale, origin[1] - bone["y"] * scale
            draw.ellipse((center_x - 1, center_y - 1, center_x + 1, center_y + 1),
                         fill=(255, 90, 90))
    return canvas


def checkerboard(size, step=8):
    image = Image.new("RGBA", size, (30, 30, 42, 255))
    draw = ImageDraw.Draw(image)
    for top in range(0, size[1], step):
        for left in range(0, size[0], step):
            if (left // step + top // step) % 2:
                draw.rectangle((left, top, left + step - 1, top + step - 1),
                               fill=(39, 39, 52, 255))
    return image


def draw_reference(sheet, skeleton, pieces):
    draw = ImageDraw.Draw(sheet)
    draw.text((16, 12), "T5 / RED-CREST KNIGHT / nearest-neighbor / unchanged rig", fill="#cddcde")
    with Image.open(ROOT / "assets" / "sprites" / "player_fire.png") as source:
        reference = source.convert("RGBA").resize((128, 128), Image.Resampling.NEAREST)
    sheet.alpha_composite(reference, (20, 42))
    draw.text((16, 180), "Original 16x16 / 8x", fill="#cddcde")
    unique = {}
    for part, piece in zip(skeleton["parts"], pieces):
        unique.setdefault(Path(part["file"]).stem, (part, piece))
    for index, (name, (part, piece)) in enumerate(unique.items()):
        left = 240 + index * 210
        enlarged = piece.resize((piece.width * 3, piece.height * 3), Image.Resampling.NEAREST)
        sheet.alpha_composite(enlarged, (left, 42))
        draw.text((left, 172), name.removeprefix("player_part_"), fill="#cddcde")
        draw.text((left, 186), f"{piece.width}x{piece.height} pivot {part['pivot']}", fill="#95a6b7")


def make_sheet(skeleton, animations, pieces, markers=False):
    sheet = checkerboard((1344, 544))
    draw_reference(sheet, skeleton, pieces)
    draw = ImageDraw.Draw(sheet)
    for index, (name, time) in enumerate(POSES):
        left = index * 224
        clip = animations.get(name)
        pose = render_pose(skeleton, pieces, clip, time, markers=markers)
        sheet.alpha_composite(pose.resize((224, 224), Image.Resampling.NEAREST), (left, 220))
        draw.text((left + 12, 210), f"{name} t={time:.2f}s / 2x", fill="#cddcde")
        draw.line((left + 10, 397, left + 214, 397), fill="#464254")
        small = render_pose(skeleton, pieces, clip, time, skeleton["pixels_per_unit"])
        sheet.alpha_composite(small, (left + 112, 405))
        draw.text((left + 12, 454), "PPU 0.5 / 1x ->", fill="#95a6b7")
    draw.text((16, 524), "Actual JSON keys, parent transforms and 7-part draw order; no gameplay overlay or lighting.",
              fill="#95a6b7")
    return sheet


def guarded_output(path: Path) -> Path:
    protected_names = {"player_fire.png", "player_ice.png", "player_poison.png"}
    protected_globs = ("player_part_*.png", "mon_*_part_*.png")
    resolved = path.resolve()
    if resolved in {SKEL.resolve(), ANIM.resolve()}:
        raise SystemExit(f"refusing to overwrite rig data: {resolved}")
    name = resolved.name
    if name in protected_names or any(fnmatch.fnmatch(name, pattern) for pattern in protected_globs):
        raise SystemExit(f"refusing to overwrite sprite asset: {resolved}")
    return resolved


def main(argv=None):
    parser = argparse.ArgumentParser(description="Preview player parts with existing animation keys.")
    parser.add_argument("--parts-dir", type=Path)
    parser.add_argument("--output", type=Path, default=OUT)
    parser.add_argument("--markers", action="store_true")
    options = parser.parse_args(argv)
    output_path = guarded_output(options.output)
    skeleton = load(SKEL)
    animations = load(ANIM)["animations"]
    pieces = []
    for part in skeleton["parts"]:
        with Image.open(resolve_part_path(part["file"], options.parts_dir)) as source:
            pieces.append(source.convert("RGBA"))
    sheet = make_sheet(skeleton, animations, pieces, options.markers)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path)
    print(f"preview -> {output_path}")


if __name__ == "__main__":
    main()
