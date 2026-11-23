# P2: 批量逐件资产预览 — 复用 preview_models 的纯软件光栅器, 生成分类合照 sheet.
# 运行: python art_src/scripts/preview_all_assets.py
# 输出: art_src/preview/sheet_<group>.png (每资产一个带名字的 cell)
# 覆盖: 怪物150 + Boss30 + 玩家6 + NPC/守卫7 + 道具16 + arena5 + 房间19 + 交互6 = 239 件.
import glob
import os

from PIL import Image, ImageDraw

import preview_models as pm

REPO = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
)
MODEL_DIR = os.path.join(REPO, "assets", "models")
OUT_DIR = os.path.join(REPO, "art_src", "preview")

# group -> (文件名前缀过滤, 列数, cell宽, cell高)
GROUPS = {
    "sheet_monster_parts": ("mon_", 15, 150, 175),
    "sheet_boss_parts": ("boss_", 6, 170, 195),
    "sheet_player_parts": ("player_part_", 6, 190, 215),
    "sheet_npc_guard": (None, 7, 200, 230),  # 特殊: npc_ + guard_
    "sheet_items": ("item_", 8, 165, 190),
    "sheet_arena": ("arena_", 5, 200, 225),
    "sheet_rooms": ("room_", 7, 200, 225),
    "sheet_props": (None, 6, 190, 215),  # 特殊: chest/altar/spring
}


def group_names(group):
    """返回该组.models 名 (按字典序)."""
    if group == "sheet_npc_guard":
        pats = ("npc_", "guard_")
    elif group == "sheet_props":
        pats = ("chest", "altar", "spring")
    else:
        pats = (GROUPS[group][0],)
    names = set()
    for pat in pats:
        names |= {
            os.path.basename(f)[:-5]
            for f in glob.glob(os.path.join(MODEL_DIR, pat + "*.json"))
        }
    return sorted(names)


def label_lines(name):
    """标签拆两行, 让 *_part_<slot> 的 5 个部件可区分 (避免截断后重名)."""
    if "_part_" in name:
        family, slot = name.split("_part_", 1)
        return family[-16:] + "\n." + slot
    if len(name) <= 22:
        return name
    return name[:12] + "\n" + name[-12:]


def render_cell(sheet, name, ox, oy, cw, ch):
    """把模型渲染进独立小图再贴到 (ox,oy): draw_model 的 y 裁剪仅支持单行."""
    pm.CELL_W, pm.CELL_H = cw, ch
    cell = Image.new("RGB", (cw, ch + 32), (30, 30, 36))
    pm.draw_model(cell, name, 0, 0)
    ImageDraw.Draw(cell).multiline_text(
        (6, ch + 2), label_lines(name), fill=(225, 225, 230), spacing=1
    )
    sheet.paste(cell, (ox, oy))


def build_sheet(group, names, cols, cw, ch):
    """生成一张分类合照 sheet."""
    stride = ch + 32  # cell 高 = 模型区 ch + 两行标签 32
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new("RGB", (cw * cols, stride * rows + 8), (30, 30, 36))
    for i, name in enumerate(names):
        render_cell(sheet, name, (i % cols) * cw, (i // cols) * stride, cw, ch)
    path = os.path.join(OUT_DIR, group + ".png")
    sheet.save(path)
    print("%-22s %3d cells  %dx%d" % (group, len(names), sheet.width, sheet.height))
    return path


def main():
    import json

    os.makedirs(OUT_DIR, exist_ok=True)
    total = 0
    manifest = {}
    for group, (_, cols, cw, ch) in GROUPS.items():
        names = group_names(group)
        if not names:
            print("WARN empty group", group)
            continue
        build_sheet(group, names, cols, cw, ch)
        manifest[group] = names
        total += len(names)
    with open(os.path.join(OUT_DIR, "sheet_manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2)
    print("TOTAL", total, "assets previewed")


if __name__ == "__main__":
    main()
