# A13-M2: 批量生成 怪/boss/玩家 部件体素化参考面数 (voxel_lib.py 规则 = 真理源)
# 运行: blender -b --factory-startup -P art_src/scripts/voxel_batch_counts.py
# 输出: art_src/preview/voxel_faces_ref.json  {"<parts_key>/<slot>": quad 数}
# 自检: player/* 六件必须与 voxelizer_test.cpp 硬编码基线一致, 否则脚本或库已漂移
import json
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from voxel_lib import build_part_mesh, load_grid  # noqa: E402

REPO = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
)

PLAYER_BASELINE = {
    "leg": 1054,
    "arm": 386,
    "weapon": 424,
    "torso": 2288,
    "head": 1602,
    "cape": 2230,
}


def build_params(slot_cfg):
    p = {
        "base": slot_cfg["base"],
        "growth": slot_cfg["growth"],
        "cap": slot_cfg["cap"],
        "ink": slot_cfg["ink"],
    }
    hem = slot_cfg.get("hem") or []
    if len(hem) >= 1:
        p["hem1"] = hem[0]
    if len(hem) >= 2:
        p["hem2"] = hem[1]
    return p


# A13-M2: 与引擎 VoxelConfig::find_for 同语义 — overrides[key][slot] 字段合并进默认槽
def params_for(cfg, key, slot):
    merged = dict(cfg["parts"][slot])
    over = (cfg.get("overrides") or {}).get(key, {}).get(slot)
    if over:
        merged.update(over)
    return build_params(merged)


def count_one(sprites_dir, fname, slot_cfg, alpha_min):
    grid, palette, ink = load_grid(os.path.join(sprites_dir, fname), alpha_min)
    me = build_part_mesh(
        fname, grid, palette, ink, (0.0, 0.0), 0.025, build_params(slot_cfg), 0.0
    )
    quads = len(me.polygons)
    bpy.data.meshes.remove(me)
    return quads


def main():
    with open(
        os.path.join(REPO, "resources", "voxel_parts.json"), encoding="utf-8"
    ) as f:
        cfg = json.load(f)
    alpha_min = cfg.get("alpha_min", 0.5)
    slots = cfg["parts"]
    sprites_dir = os.path.join(REPO, "assets", "sprites")

    out = {}
    for fname in sorted(os.listdir(sprites_dir)):
        if "_part_" not in fname or not fname.endswith(".png"):
            continue
        key, slot = fname.split("_part_", 1)
        slot = slot[:-4]
        if slot not in slots:
            print("SKIP unknown slot:", fname)
            continue
        out[key + "/" + slot] = count_one(
            sprites_dir, fname, params_for(cfg, key, slot), alpha_min
        )

    path = os.path.join(REPO, "art_src", "preview", "voxel_faces_ref.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1, sort_keys=True)

    players = {k.split("/", 1)[1]: v for k, v in out.items() if k.startswith("player/")}
    mismatches = {
        k: (players.get(k), want)
        for k, want in PLAYER_BASELINE.items()
        if players.get(k) != want
    }
    print("BATCH_COUNTS_WRITTEN parts=%d -> %s" % (len(out), path))
    print("PLAYER_XCHECK", json.dumps(players, sort_keys=True))
    if mismatches:
        print("PLAYER_XCHECK_FAILED", json.dumps(mismatches))
        sys.exit(1)
    print("PLAYER_XCHECK_OK")


main()
