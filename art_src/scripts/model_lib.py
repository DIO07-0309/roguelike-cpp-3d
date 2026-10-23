# A14: 低模硬表面建模基元库 (纯 Python, Blender 内/外均可跑)
# 约定: 引擎坐标系 (X 右, Y 上, Z 前), 1 单位 = 1 世界像素 (TILE_SIZE=32)
# 输出 JSON: {"name","vertices":[[x,y,z,r,g,b]...],"faces":[[i,j,k]...]}
# 面着色 (flat): 每个面独立顶点, 颜色写在顶点上, 法线由引擎按绕序算
import json
import math
import os


class MeshBuilder:
    """累积三角面 (每面独立顶点), 输出引擎可直读的 JSON 网格"""

    def __init__(self):
        self.vertices = []
        self.faces = []

    def _push(self, point, color):
        self.vertices.append(
            [
                round(point[0], 3),
                round(point[1], 3),
                round(point[2], 3),
                int(color[0]),
                int(color[1]),
                int(color[2]),
            ]
        )
        return len(self.vertices) - 1

    def add_triangle(self, p0, p1, p2, color):
        a = self._push(p0, color)
        b = self._push(p1, color)
        c = self._push(p2, color)
        self.faces.append([a, b, c])

    def add_quad(self, p0, p1, p2, p3, color):
        """CCW 绕序 (从外侧看) — 引擎据此算外向法线"""
        a = self._push(p0, color)
        b = self._push(p1, color)
        c = self._push(p2, color)
        d = self._push(p3, color)
        self.faces.append([a, b, c])
        self.faces.append([a, c, d])

    def add_box(self, center, size, color, taper=1.0):
        """长方体; taper<1 时顶面横向收缩 (锥台)"""
        hx, hy, hz = size[0] * 0.5, size[1] * 0.5, size[2] * 0.5
        cx, cy, cz = center
        tx, tz = hx * taper, hz * taper
        bottom = [
            (cx - hx, cy - hy, cz - hz),
            (cx + hx, cy - hy, cz - hz),
            (cx + hx, cy - hy, cz + hz),
            (cx - hx, cy - hy, cz + hz),
        ]
        top = [
            (cx - tx, cy + hy, cz - tz),
            (cx + tx, cy + hy, cz - tz),
            (cx + tx, cy + hy, cz + tz),
            (cx - tx, cy + hy, cz + tz),
        ]
        self.add_quad(bottom[3], bottom[2], bottom[1], bottom[0], color)  # 底
        self.add_quad(top[0], top[1], top[2], top[3], color)  # 顶
        self.add_quad(bottom[0], bottom[1], top[1], top[0], color)  # 前 (-z)
        self.add_quad(bottom[2], bottom[3], top[3], top[2], color)  # 后 (+z)
        self.add_quad(bottom[1], bottom[2], top[2], top[1], color)  # 右 (+x)
        self.add_quad(bottom[3], bottom[0], top[0], top[3], color)  # 左 (-x)

    def add_box_yaw(self, center, size, color, yaw_deg, taper=1.0):
        """带 Y 轴偏转的长方体 (石环分段/手臂姿态)"""
        radians = math.radians(yaw_deg)
        cosine, sine = math.cos(radians), math.sin(radians)
        hx, hy, hz = size[0] * 0.5, size[1] * 0.5, size[2] * 0.5
        cx, cy, cz = center
        tx, tz = hx * taper, hz * taper
        raw_bottom = [(-hx, -hz), (hx, -hz), (hx, hz), (-hx, hz)]
        raw_top = [(-tx, -tz), (tx, -tz), (tx, tz), (-tx, tz)]
        bottom = [
            (cx + x * cosine - z * sine, cy - hy, cz + x * sine + z * cosine)
            for x, z in raw_bottom
        ]
        top = [
            (cx + x * cosine - z * sine, cy + hy, cz + x * sine + z * cosine)
            for x, z in raw_top
        ]
        self.add_quad(bottom[3], bottom[2], bottom[1], bottom[0], color)
        self.add_quad(top[0], top[1], top[2], top[3], color)
        self.add_quad(bottom[0], bottom[1], top[1], top[0], color)
        self.add_quad(bottom[2], bottom[3], top[3], top[2], color)
        self.add_quad(bottom[1], bottom[2], top[2], top[1], color)
        self.add_quad(bottom[3], bottom[0], top[0], top[3], color)

    def add_cylinder(self, center, radius, height, segments, color, cap=True):
        cx, cy, cz = center
        hy = height * 0.5
        ring = []
        for i in range(segments):
            angle = 2.0 * math.pi * i / segments
            ring.append((cx + math.cos(angle) * radius, cz + math.sin(angle) * radius))
        for i in range(segments):
            x0, z0 = ring[i]
            x1, z1 = ring[(i + 1) % segments]
            self.add_quad(
                (x0, cy - hy, z0),
                (x1, cy - hy, z1),
                (x1, cy + hy, z1),
                (x0, cy + hy, z0),
                color,
            )
        if cap:
            for i in range(1, segments - 1):
                self.add_triangle(
                    (cx, cy + hy, cz),
                    (ring[i][0], cy + hy, ring[i][1]),
                    (ring[i + 1][0], cy + hy, ring[i + 1][1]),
                    color,
                )
                self.add_triangle(
                    (cx, cy - hy, cz),
                    (ring[i + 1][0], cy - hy, ring[i + 1][1]),
                    (ring[i][0], cy - hy, ring[i][1]),
                    color,
                )

    def add_cone(self, center, radius, height, segments, color):
        cx, cy, cz = center
        apex = (cx, cy + height, cz)
        ring = []
        for i in range(segments):
            angle = 2.0 * math.pi * i / segments
            ring.append((cx + math.cos(angle) * radius, cz + math.sin(angle) * radius))
        for i in range(segments):
            x0, z0 = ring[i]
            x1, z1 = ring[(i + 1) % segments]
            self.add_triangle((x0, cy, z0), (x1, cy, z1), apex, color)
        for i in range(1, segments - 1):
            self.add_triangle(
                (cx, cy, cz),
                (ring[i + 1][0], cy, ring[i + 1][1]),
                (ring[i][0], cy, ring[i][1]),
                color,
            )

    def add_sphere(self, center, radius, segments, rings, color):
        """低模球 (头/关节); 顶部收成点"""
        cx, cy, cz = center
        for r in range(rings - 1):
            phi0 = math.pi * r / (rings - 1)
            phi1 = math.pi * (r + 1) / (rings - 1)
            for s in range(segments):
                a0 = 2.0 * math.pi * s / segments
                a1 = 2.0 * math.pi * (s + 1) / segments
                p00 = (
                    cx + radius * math.sin(phi0) * math.cos(a0),
                    cy + radius * math.cos(phi0),
                    cz + radius * math.sin(phi0) * math.sin(a0),
                )
                p10 = (
                    cx + radius * math.sin(phi0) * math.cos(a1),
                    cy + radius * math.cos(phi0),
                    cz + radius * math.sin(phi0) * math.sin(a1),
                )
                p01 = (
                    cx + radius * math.sin(phi1) * math.cos(a0),
                    cy + radius * math.cos(phi1),
                    cz + radius * math.sin(phi1) * math.sin(a0),
                )
                p11 = (
                    cx + radius * math.sin(phi1) * math.cos(a1),
                    cy + radius * math.cos(phi1),
                    cz + radius * math.sin(phi1) * math.sin(a1),
                )
                self.add_quad(p00, p10, p11, p01, color)

    def triangle_count(self):
        return len(self.faces)

    def to_dict(self, name):
        return {"name": name, "vertices": self.vertices, "faces": self.faces}

    def save(self, directory, name):
        os.makedirs(directory, exist_ok=True)
        path = os.path.join(directory, name + ".json")
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(
                self.to_dict(name), handle, ensure_ascii=False, separators=(",", ":")
            )
        return path
