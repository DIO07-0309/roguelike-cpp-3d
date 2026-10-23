import bpy

GRID = [
    "....AAAAAAAAA...",
    "...AAAAAAAAAAA..",
    "...AABCCCCCCAA..",
    "...AADBCCCCBAA..",
    "...AADCAAAACAA..",
    "..AAADBCCACBAA..",
    ".AAAADBBCACBAAA.",
    "AAAEEDBBCACDFAAA",
    "AAEEEEFFFDFFFCAA",
    "AAEEEEEEFACCBCAA",
    "AAEFFFEEFACCGGAA",
    "AAAFFFFFFABBDDAA",
    ".AAFFFFFAGGGCCAA",
    ".AAFFFFFDDDDCCAA",
    ".AAFFFFFAADDAAAA",
    ".AAFFFFFAAGGAAAA",
]
PALETTE = {
    "A": "#3F2631",
    "B": "#8B9BB4",
    "C": "#C0CBDC",
    "D": "#52607C",
    "E": "#75E3FF",
    "F": "#0099DB",
    "G": "#763B36",
}
SCALE = 0.1
FRONT = -0.25
X0 = -0.8


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hex_lin(h):
    return tuple(srgb_to_linear(int(h[i : i + 2], 16) / 255.0) for i in (1, 3, 5))


PARAMS = {
    "head": (2.7, 0.95, 7.4),
    "torso": (2.7, 0.95, 6.8),
    "arm": (2.4, 1.08, 5.4),
    "cape": (3.0, 0.30, 7.0),
    "leg": (2.7, 1.08, 4.7),
}


def region_of(c, x, y):
    if c == "A":
        return "ink"
    if c in "EF":
        return "cape"
    if (x >= 12 and 7 <= y <= 12) or (x >= 11 and 9 <= y <= 12):
        return "arm"
    if y <= 6:
        return "head"
    if y <= 12:
        return "torso"
    return "leg"


def depth_of(c, x, y):
    r = region_of(c, x, y)
    if r == "ink":
        return 1.8 if y >= 13 else 2.6
    xl = x
    while xl > 0 and region_of(GRID[y][xl - 1], xl - 1, y) in (r, "ink"):
        xl -= 1
    xr = x
    while xr < 15 and region_of(GRID[y][xr + 1], xr + 1, y) in (r, "ink"):
        xr += 1
    yt = y
    while yt > 0 and region_of(GRID[yt - 1][x], x, yt - 1) in (r, "ink"):
        yt -= 1
    yb = y
    while yb < 15 and region_of(GRID[yb + 1][x], x, yb + 1) in (r, "ink"):
        yb += 1
    d = min(x - xl, xr - x, y - yt, yb - y)
    base, growth, cap = PARAMS[r]
    dep = base + growth * d
    if r == "cape":
        if y >= 14:
            dep += 2
        elif y >= 11:
            dep += 1
    return min(dep, cap)


def front_shift(c, x, y):
    r = region_of(c, x, y)
    if r == "head":
        return -0.05
    if r == "cape":
        return -0.07
    if r == "arm":
        return -0.10
    if r == "ink":
        if (y == 4 and 7 <= x <= 10) or (y == 5 and x == 9):
            return 0.06
        return -0.05 if y <= 6 else 0.0
    return 0.0


cols = {}
for y in range(16):
    for x in range(16):
        c = GRID[y][x]
        if c != ".":
            fr = FRONT + front_shift(c, x, y)
            cols[(x, y)] = (c, fr, fr + depth_of(c, x, y) * SCALE)

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

verts, faces, loop_cols = [], [], []


def emit(quad, key, c):
    base = len(verts)
    verts.extend(quad)
    faces.append((base, base + 1, base + 2, base + 3))
    lin = hex_lin(PALETTE[c])
    s = INK_SHADE[key] if c == "A" else SHADE[key]
    loop_cols.append(tuple(min(1.0, v * s) for v in lin) + (1.0,))


def same(a, b):
    return a is not None and b is not None and a[1] == b[1] and a[2] == b[2]


for (x, y), (c, y0, y1) in cols.items():
    xa, xb = x * SCALE + X0, (x + 1) * SCALE + X0
    z0, z1 = (15 - y) * SCALE, (16 - y) * SCALE
    emit([(xa, y0, z0), (xb, y0, z0), (xb, y0, z1), (xa, y0, z1)], "front", c)
    emit([(xa, y1, z0), (xa, y1, z1), (xb, y1, z1), (xb, y1, z0)], "back", c)
    if not same(cols.get((x - 1, y)), (c, y0, y1)):
        emit([(xa, y0, z0), (xa, y0, z1), (xa, y1, z1), (xa, y1, z0)], "left", c)
    if not same(cols.get((x + 1, y)), (c, y0, y1)):
        emit([(xb, y0, z0), (xb, y1, z0), (xb, y1, z1), (xb, y0, z1)], "right", c)
    if not same(cols.get((x, y - 1)), (c, y0, y1)):
        emit([(xa, y0, z1), (xb, y0, z1), (xb, y1, z1), (xa, y1, z1)], "top", c)
    if not same(cols.get((x, y + 1)), (c, y0, y1)):
        emit([(xa, y0, z0), (xa, y1, z0), (xb, y1, z0), (xb, y0, z0)], "bottom", c)

me = bpy.data.meshes.new("PlayerIce_Voxel_mesh")
me.from_pydata(verts, [], faces)
me.update()
ca = me.color_attributes.new(name="Col", type="FLOAT_COLOR", domain="CORNER")
for i, poly in enumerate(me.polygons):
    for li in poly.loop_indices:
        ca.data[li].color = loop_cols[i]

m = bpy.data.materials.get("vox_vc")
if m is None:
    m = bpy.data.materials.new("vox_vc")
    m.use_nodes = True
    nodes = m.node_tree.nodes
    bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
    attr = nodes.new("ShaderNodeAttribute")
    attr.attribute_name = "Col"
    m.node_tree.links.new(attr.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.65
me.materials.append(m)

ob = bpy.data.objects.get("PlayerIce_Voxel")
if ob is None:
    ob = bpy.data.objects.new("PlayerIce_Voxel", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = (-0.9, 0, 0)
else:
    old = ob.data
    ob.data = me
    bpy.data.meshes.remove(old)

bpy.ops.object.select_all(action="DESELECT")
print("BUILD_DONE faces=%d verts=%d" % (len(faces), len(verts)))
bpy.ops.wm.save_mainfile()
print("SAVED")
