import json
import math
import pathlib
import random
import struct
import zlib


def clamp(x, lo=0.0, hi=1.0):
    return lo if x < lo else hi if x > hi else x


def smooth(a, b, x):
    t = clamp((x - a) / (b - a)) if b != a else 0.0
    return t * t * (3 - 2 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def hexrgb(s):
    s = s.lstrip("#")
    return tuple(int(s[i : i + 2], 16) for i in (0, 2, 4))


class Canvas:
    def __init__(self, w, h):
        self.w = w
        self.h = h
        self.data = bytearray(w * h * 4)

    def get(self, x, y):
        i = (y * self.w + x) * 4
        return tuple(self.data[i : i + 4])

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            i = (y * self.w + x) * 4
            self.data[i : i + 4] = bytes(int(clamp(v, 0, 255)) for v in c)

    def over(self, x, y, c):
        if not (0 <= x < self.w and 0 <= y < self.h) or c[3] <= 0:
            return
        i = (y * self.w + x) * 4
        da = self.data[i + 3] / 255.0
        sa = c[3] / 255.0
        oa = sa + da * (1 - sa)
        if oa <= 0:
            return
        for k in range(3):
            self.data[i + k] = int(clamp((c[k] * sa + self.data[i + k] * da * (1 - sa)) / oa, 0, 255))
        self.data[i + 3] = int(oa * 255)

    def save(self, path):
        raw = b"".join(b"\x00" + bytes(self.data[y * self.w * 4 : (y + 1) * self.w * 4]) for y in range(self.h))

        def chunk(kind, body):
            data = kind + body
            return struct.pack(">I", len(body)) + data + struct.pack(">I", zlib.crc32(data) & 0xFFFFFFFF)

        header = struct.pack(">IIBBBBB", self.w, self.h, 8, 6, 0, 0, 0)
        pathlib.Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def paint(canvas, x, y, w, h, shape, ss=3):
    """shape(u, v) -> (gray, alpha) or (r, g, b, alpha) with alpha 0..1, or None. u, v run 0..1 across the region."""
    for py in range(h):
        for px in range(w):
            acc = 0.0
            sums = [0.0, 0.0, 0.0]
            for sy in range(ss):
                for sx in range(ss):
                    r = shape((px + (sx + 0.5) / ss) / w, (py + (sy + 0.5) / ss) / h)
                    if not r or r[-1] <= 0:
                        continue
                    a = r[-1]
                    acc += a
                    if len(r) == 2:
                        sums[0] += r[0] * a
                        sums[1] += r[0] * a
                        sums[2] += r[0] * a
                    else:
                        sums[0] += r[0] * a
                        sums[1] += r[1] * a
                        sums[2] += r[2] * a
            if acc > 0:
                canvas.over(x + px, y + py, (sums[0] / acc, sums[1] / acc, sums[2] / acc, 255 * acc / (ss * ss)))


def rect_painter(art, sx, sy):
    def run(canvas, x, y, w, h):
        for py in range(h):
            for px in range(w):
                canvas.set(x + px, y + py, art.get(sx + px, sy + py))

    return run


def inside(poly, x, y):
    n = len(poly)
    hit = False
    j = n - 1
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi + 1e-12) + xi:
            hit = not hit
        j = i
    return hit


def seg_dist(a, b, x, y):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    t = clamp(((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy + 1e-12))
    return math.hypot(x - (ax + dx * t), y - (ay + dy * t))


def poly_dist(poly, x, y):
    return min(seg_dist(poly[i], poly[(i + 1) % len(poly)], x, y) for i in range(len(poly)))


def curve(points, steps=8, bulge=0.0):
    """Smooth-ish polyline through points using quadratic arcs bulging by `bulge` of the chord length."""
    out = []
    for i in range(len(points) - 1):
        (x0, y0), (x1, y1) = points[i], points[i + 1]
        mx, my = (x0 + x1) / 2, (y0 + y1) / 2
        nx, ny = -(y1 - y0), x1 - x0
        cx, cy = mx + nx * bulge, my + ny * bulge
        for k in range(steps):
            t = k / steps
            out.append(((1 - t) ** 2 * x0 + 2 * (1 - t) * t * cx + t * t * x1, (1 - t) ** 2 * y0 + 2 * (1 - t) * t * cy + t * t * y1))
    out.append(points[-1])
    return out
class Atlas:
    def __init__(self, size):
        self.size = size
        self.x = 1
        self.y = 1
        self.row = 0
        self.cache = {}

    def alloc(self, w, h):
        if self.x + w + 1 > self.size:
            self.x = 1
            self.y += self.row + 1
            self.row = 0
        if self.y + h + 1 > self.size:
            raise RuntimeError("texture atlas is full")
        at = (self.x, self.y)
        self.x += w + 1
        self.row = max(self.row, h)
        return at


class Bone:
    def __init__(self, item, name, pivot, rot, anim, cloth, physics=None):
        self.item = item
        self.name = name
        self.pivot = list(pivot)
        self.rot = list(rot)
        self.anim = anim
        self.cloth = cloth
        self.physics = physics
        self.cubes = []

    def flat(self, origin, size, painter, key=None, tint=None, tint2=None, mix=0.0, mirror=False, edge=(0, 0, 0, 0)):
        k = self.item.texel
        w, h = round(size[0] * k), round(size[1] * k)
        item = self.item
        if key is not None and key in item.atlas.cache:
            x, y = item.atlas.cache[key]
        else:
            x, y = item.atlas.alloc(w, h + 1)
            painter(item.canvas, x, y, w, h)
            item.canvas.set(x, y + h, edge if len(edge) == 4 else (*edge, 255))
            if key is not None:
                item.atlas.cache[key] = (x, y)
        cube = {"origin": [round(v, 3) for v in origin], "size": [round(v, 3) for v in size], "uv": [x, y], "flat": True}
        if mirror:
            cube["mirror"] = True
        if tint:
            cube["tint"] = tint
        if tint2:
            cube["tint2"] = tint2
            cube["mix"] = round(mix, 3)
        self.cubes.append(cube)
        return cube

    def box(self, origin, size, painter, key=None, tint=None, tint2=None, mix=0.0):
        k = self.item.texel
        w, h, d = (round(v * k) for v in size)
        item = self.item
        if key is not None and key in item.atlas.cache:
            x, y = item.atlas.cache[key]
        else:
            x, y = item.atlas.alloc(2 * d + 2 * w, d + h)
            faces = {
                "top": (x + d, y, w, d),
                "bottom": (x + d + w, y, w, d),
                "left": (x, y + d, d, h),
                "front": (x + d, y + d, w, h),
                "right": (x + d + w, y + d, d, h),
                "back": (x + 2 * d + w, y + d, w, h),
            }
            painter(item.canvas, faces)
            if key is not None:
                item.atlas.cache[key] = (x, y)
        cube = {"origin": [round(v, 3) for v in origin], "size": [round(v, 3) for v in size], "uv": [x, y]}
        if tint:
            cube["tint"] = tint
        if tint2:
            cube["tint2"] = tint2
            cube["mix"] = round(mix, 3)
        self.cubes.append(cube)
        return cube


class Item:
    def __init__(self, id, name, slot, tags, tints, texel=1, size=128):
        self.id = id
        self.name = name
        self.slot = slot
        self.tags = tags
        self.tints = tints
        self.texel = texel
        self.canvas = Canvas(size, size)
        self.atlas = Atlas(size)
        self.bones = []

    def bone(self, name, pivot=(0, 0, 0), rot=(0, 0, 0), anim=None, cloth=False, physics=None):
        b = Bone(self, name, pivot, rot, anim, cloth, physics)
        self.bones.append(b)
        return b

    def cubes(self):
        return sum(len(b.cubes) for b in self.bones)

    def write(self, root):
        if self.cubes() > 80:
            raise RuntimeError(f"{self.id}: {self.cubes()} cubes, the limit is 80")
        folder = pathlib.Path(root) / self.id
        folder.mkdir(parents=True, exist_ok=True)
        bones = []
        for b in self.bones:
            j = {"name": b.name, "pivot": b.pivot, "rotation": b.rot, "cubes": b.cubes}
            if b.anim:
                j["anim"] = b.anim
            if b.cloth:
                j["physics"] = dict(b.physics or {}, type="cloth")
            elif b.physics:
                j["physics"] = dict(b.physics, type="spring")
            bones.append(j)
        doc = {
            "id": self.id,
            "name": self.name,
            "slot": self.slot,
            "tags": self.tags,
            "texture": "tex.png",
            "texel": self.texel,
            "tint": [{"name": n, "default": c} for n, c in self.tints],
            "bones": bones,
        }
        (folder / "item.json").write_text(json.dumps(doc, indent=1) + "\n")
        self.canvas.save(folder / "tex.png")
        return folder


def feather(length_tex, width_tex, tip=0.0, curve=0.0, vein=True, grain=1.0, seed=1, base=205, rim=130, hi=255):
    """Feather lying along u (base at 0, tip at 1)."""
    rng = random.Random(seed)
    phase = rng.random() * 6.28

    def shape(u, v):
        c = (v - 0.5) + curve * (u - 0.3) * (u - 0.3) * 2
        w0 = clamp(u / 0.16) ** 0.55
        w1 = 1.0 if u < 0.28 else max(0.0, 1.0 - ((u - 0.28) / 0.72) ** (1.7 - tip))
        hw = 0.5 * w0 * w1 ** 0.62
        d = abs(c)
        if d > hw or hw <= 0:
            return None
        edge = (hw - d) * width_tex
        g = lerp(base, hi, 1 - d / hw) * (0.82 + 0.18 * smooth(0, 0.5, u))
        barb = math.sin((u * length_tex * 0.9 + c * width_tex * 0.7) * 1.7 + phase)
        g += barb * 7 * grain
        if vein and d * width_tex < 0.9:
            g = hi
        if edge < 1.0:
            g = lerp(rim, g, clamp(edge))
        return (clamp(g, 0, 255), clamp(edge * 1.5 + 0.2) if edge < 0.7 else 1.0)

    return shape


def solid(gray, edge=None):
    def shape(u, v):
        return (gray, 1.0)

    return shape


def region_painter(shape, ss=3):
    def run(canvas, x, y, w, h):
        paint(canvas, x, y, w, h, shape, ss)

    return run
