import bpy

SHADE = {
    "front": 1.00,
    "back": 0.72,
    "left": 0.80,
    "right": 0.80,
    "top": 1.15,
    "bottom": 0.55,
}
INK_SHADE = {
    "front": 1.00,
    "back": 0.80,
    "left": 1.08,
    "right": 1.08,
    "top": 1.25,
    "bottom": 0.60,
}
INK_MAX_LUM = 0.25


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def _is_ink(rgb8):
    r, g, b = (v / 255.0 for v in rgb8)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b < INK_MAX_LUM


def load_grid(path, alpha_min=0.5):
    img = bpy.data.images.load(path)
    w, h = img.size
    px = list(img.pixels)
    grid = []
    seen = {}
    for r in range(h):
        row = []
        base = (h - 1 - r) * w * 4
        for x in range(w):
            i = base + x * 4
            if px[i + 3] < alpha_min:
                row.append(".")
                continue
            key = (round(px[i] * 255), round(px[i + 1] * 255), round(px[i + 2] * 255))
            if key not in seen:
                seen[key] = "p%d" % len(seen)
            row.append(seen[key])
        grid.append(row)
    bpy.data.images.remove(img)
    palette = {v: k for k, v in seen.items()}
    ink = {cid for cid, rgb in palette.items() if _is_ink(rgb)}
    return grid, palette, ink


def _lin(palette, c, key, ink):
    h = palette[c]
    lin = tuple(srgb_to_linear(v / 255.0) for v in h)
    s = (INK_SHADE if c in ink else SHADE)[key]
    return tuple(min(1.0, v * s) for v in lin) + (1.0,)


def _edge_dist(grid, r, x):
    h, w = len(grid), len(grid[0])
    xl = x
    while xl > 0 and grid[r][xl - 1] != ".":
        xl -= 1
    xr = x
    while xr < w - 1 and grid[r][xr + 1] != ".":
        xr += 1
    rt = r
    while rt > 0 and grid[rt - 1][x] != ".":
        rt -= 1
    rb = r
    while rb < h - 1 and grid[rb + 1][x] != ".":
        rb += 1
    return min(x - xl, xr - x, r - rt, rb - r)


def _depths(grid, ink, params):
    h = len(grid)
    out = {}
    for r, row in enumerate(grid):
        for x, c in enumerate(row):
            if c == ".":
                continue
            if c in ink:
                out[(r, x)] = float(params["ink"])
                continue
            d = _edge_dist(grid, r, x)
            dep = params["base"] + params["growth"] * d
            from_bottom = h - 1 - r
            hem1 = params.get("hem1")
            hem2 = params.get("hem2")
            if hem1 and from_bottom <= hem1[0]:
                dep += hem1[1]
            elif hem2 and from_bottom <= hem2[0]:
                dep += hem2[1]
            out[(r, x)] = float(min(dep, params["cap"]))
    return out


def _emit(verts, faces, cols, quad, key, c, palette, ink):
    base = len(verts)
    verts.extend(quad)
    faces.append((base, base + 1, base + 2, base + 3))
    cols.append(_lin(palette, c, key, ink))


def _cell_faces(meta, grid_h, r, x, scale, px, pz, verts, faces, cols, palette, ink):
    c, y0, y1 = meta[(r, x)]
    xa = (x - px) * scale
    xb = xa + scale
    zb = (grid_h - r) * scale - pz * scale
    za = zb - scale
    _emit(
        verts,
        faces,
        cols,
        [(xa, y0, za), (xb, y0, za), (xb, y0, zb), (xa, y0, zb)],
        "front",
        c,
        palette,
        ink,
    )
    _emit(
        verts,
        faces,
        cols,
        [(xa, y1, za), (xa, y1, zb), (xb, y1, zb), (xb, y1, za)],
        "back",
        c,
        palette,
        ink,
    )
    if meta.get((r, x - 1)) != meta[(r, x)]:
        _emit(
            verts,
            faces,
            cols,
            [(xa, y0, za), (xa, y0, zb), (xa, y1, zb), (xa, y1, za)],
            "left",
            c,
            palette,
            ink,
        )
    if meta.get((r, x + 1)) != meta[(r, x)]:
        _emit(
            verts,
            faces,
            cols,
            [(xb, y0, za), (xb, y1, za), (xb, y1, zb), (xb, y0, zb)],
            "right",
            c,
            palette,
            ink,
        )
    if meta.get((r - 1, x)) != meta[(r, x)]:
        _emit(
            verts,
            faces,
            cols,
            [(xa, y0, zb), (xb, y0, zb), (xb, y1, zb), (xa, y1, zb)],
            "top",
            c,
            palette,
            ink,
        )
    if meta.get((r + 1, x)) != meta[(r, x)]:
        _emit(
            verts,
            faces,
            cols,
            [(xa, y0, za), (xa, y1, za), (xb, y1, za), (xb, y0, za)],
            "bottom",
            c,
            palette,
            ink,
        )


def get_part_material():
    m = bpy.data.materials.get("vox_part_vc")
    if m is None:
        m = bpy.data.materials.new("vox_part_vc")
        m.use_nodes = True
        bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
        attr = m.node_tree.nodes.new("ShaderNodeAttribute")
        attr.attribute_name = "Col"
        m.node_tree.links.new(attr.outputs["Color"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.65
    return m


def build_part_mesh(name, grid, palette, ink, pivot, scale, params, y_off=0.0):
    dep = _depths(grid, ink, params)
    meta = {}
    for (r, x), d in dep.items():
        meta[(r, x)] = (grid[r][x], -d * scale / 2 + y_off, d * scale / 2 + y_off)
    verts, faces, cols = [], [], []
    px, pz = pivot
    for r, row in enumerate(grid):
        for x, c in enumerate(row):
            if c == ".":
                continue
            _cell_faces(
                meta, len(grid), r, x, scale, px, pz, verts, faces, cols, palette, ink
            )
    me = bpy.data.meshes.new(name + "_mesh")
    me.from_pydata(verts, [], faces)
    me.update()
    ca = me.color_attributes.new(name="Col", type="FLOAT_COLOR", domain="CORNER")
    for i, poly in enumerate(me.polygons):
        for li in poly.loop_indices:
            ca.data[li].color = cols[i]
    me.materials.append(get_part_material())
    return me
