import math

import numpy as np

from item import Item
from paint import Art, bezier
from shoes import box_uv

TEXEL = 10


def smooth(x):
    x = min(1.0, max(0.0, x))
    return x * x * (3 - 2 * x)


def pair(item, name, pivot, rotation, anim, physics, build, lag=(0.0, 0.0)):
    """One bone per side at mirrored coordinates; `build(bone, sx)` draws the +x side and mirrors itself by sx."""
    for sx, label in ((1, "l"), (-1, "r")):
        a = dict(anim)
        a["phase"] = round(lag[0] if sx > 0 else lag[1], 3)
        if a.get("axis") in ("y", "z"):
            a["amplitude"] = a["amplitude"] * sx
        p = dict(physics)
        p["drive"] = {k: [v[0], sx * v[1], sx * v[2]] for k, v in physics["drive"].items()}
        bone = item.bone(f"{name}_{label}", (sx * pivot[0], pivot[1], pivot[2]), (rotation[0], sx * rotation[1], sx * rotation[2]), anim=a, physics=p)
        build(bone, sx)


def mirrored(path, sx):
    return [(sx * x, y, z, rx, rz, m) for x, y, z, rx, rz, m in path]


def card_at(item, bone, sx, x0, y0, z, size, uv, tint):
    w, h = size
    item.card(bone, ((x0 if sx > 0 else -(x0 + w)), y0, z), (w, h, 0.04), uv, tint, mirror=sx < 0)


class Blade:
    """An upright ear: a flat tube whose width follows `half(t)` along a centre line from (cx0, base) to (cx1, base + height)."""

    def __init__(self, cx0, cx1, base, height, z, thick, half, bend=0.0, rings=14):
        self.cx0, self.cx1, self.base, self.height, self.z, self.thick, self.half, self.bend, self.rings = cx0, cx1, base, height, z, thick, half, bend, rings

    def cx(self, t):
        return self.cx0 + (self.cx1 - self.cx0) * t + self.bend * math.sin(math.pi * t) * t

    def path(self, tip_from):
        out = []
        for i in range(self.rings + 1):
            t = i / self.rings
            out.append((self.cx(t), self.base + self.height * t, self.z, max(0.08, self.half(t)), self.thick + 0.06, smooth((t - tip_from) / (1 - tip_from))))
        return out


def inner_art(blade, margin, top, width, tufts, depth_dark=70, seed=1):
    """Painted inner ear that sits on the front face of the blade. Returns (art, x0, y0) in model coordinates."""
    ts = np.linspace(margin / blade.height, top, 24)
    x0 = min(blade.cx(t) - blade.half(t) * width for t in ts) - 0.1
    x1 = max(blade.cx(t) + blade.half(t) * width for t in ts) + 0.1
    y0, y1 = blade.base + margin, blade.base + blade.height * top
    art = Art(x1 - x0, y1 - y0, TEXEL)
    t = np.clip((y1 - art.Y - blade.base) / blade.height, 0, 1)
    hf = np.vectorize(blade.half)(t)
    cx = np.vectorize(blade.cx)(t)
    u = (x0 + art.X - cx) / np.maximum(hf * width, 1e-3)
    mask = (np.abs(u) < 1) & (t > margin / blade.height) & (t < top)
    rim = np.clip((1 - np.abs(u)) / 0.35, 0, 1)
    low = np.clip(1 - (t - margin / blade.height) / 0.5, 0, 1)
    art.put(mask.astype(float), 238 - depth_dark * rim ** 0.8 * (0.35 + 0.65 * low) * (1 - 0.4 * t))
    rng = np.random.default_rng(seed)
    lo = blade.half(0.04) * width
    for k in range(tufts):
        f = (k + 0.5) / tufts
        bx = blade.cx(0.04) - lo + f * 2 * lo - x0
        lean = (blade.cx(0.04) - x0 - bx) * 0.5 + rng.uniform(-0.2, 0.2)
        h = rng.uniform(1.0, 1.9) * (1 - 0.35 * abs(2 * f - 1))
        by = y1 - y0 - 0.2
        m, d = art.stroke([(bx, by), (bx + lean * 0.5, by - h * 0.5), (bx + lean, by - h)], 0.26, 0.03)
        art.put(m, 255 - 25 * d)
    for side in (-1, 1):
        for k in range(int(blade.height * 2.2)):
            f = (k + rng.uniform(0, 1)) / (blade.height * 2.2)
            t0 = margin / blade.height + f * (top - margin / blade.height) * 0.92
            pts = [(blade.cx(tt) + side * blade.half(tt) * width * 0.97 - x0, y1 - (blade.base + blade.height * tt)) for tt in (t0, t0 + 0.05)]
            m, d = art.stroke(pts, 0.11, 0.02)
            art.put(m * mask, 255, 0.85)
    return art, x0, y0


def back_art(blade, tip_from=None, seed=3, strokes=80):
    """Fur on the back of an ear. With `tip_from` it paints only the darker tip as a fade, for a second card on top."""
    ts = np.linspace(0, 1, 30)
    x0 = min(blade.cx(t) - max(0.08, blade.half(t)) for t in ts) - 0.05
    x1 = max(blade.cx(t) + max(0.08, blade.half(t)) for t in ts) + 0.05
    art = Art(x1 - x0, blade.height, TEXEL)
    t = np.clip((blade.height - art.Y) / blade.height, 0, 1)
    hf = np.maximum(0.08, np.vectorize(blade.half)(t))
    cx = np.vectorize(blade.cx)(t)
    u = (x0 + art.X - cx) / hf
    fringe = 1 + 0.11 * (0.5 + 0.5 * np.sin(t * blade.height * 9.0 + seed)) * np.sin(np.pi * np.clip(t * 1.1, 0, 1))
    mask = ((np.abs(u) < fringe) & (art.Y < blade.height)).astype(float)
    if tip_from is not None:
        art.put(mask, 255, np.clip((t - tip_from) / 0.18, 0, 1) ** 1.2)
        return art, x0
    shade = (0.86 + 0.14 * np.sqrt(np.clip(1 - u * u, 0, 1))) * (0.84 + 0.16 * np.clip(t / 0.3, 0, 1))
    tone = 0.95 + 0.05 * np.sin(art.X * 2.1 + art.Y * 1.3 + seed) * np.sin(art.Y * 0.9 - seed)
    art.put(mask, 252 * shade * tone)
    rng = np.random.default_rng(seed)
    for k in range(strokes):
        ty = rng.uniform(0.0, 0.7)
        f = rng.uniform(-0.85, 0.85)
        h = rng.uniform(0.1, 0.26) * blade.height
        t1 = min(1.0, ty + h / blade.height)
        px = [(blade.cx(tt) + f * max(0.08, blade.half(tt)) - x0, blade.height - blade.height * tt) for tt in np.linspace(ty, t1, 4)]
        m, d = art.stroke(px, 0.085, 0.015)
        art.put(m * mask, 205 if k % 3 else 255, 0.6)
    return art, x0


def ears(item, blade, art, origin, rotation, spring, anim, lag, tip_from=0.55, tip_card=None, seed=3, power=3.4):
    uv = item.place(art.pixels())
    back, bx = back_art(blade, seed=seed)
    back_uv = item.place(back.pixels())
    tip = back_art(blade, tip_card)[0] if tip_card is not None else None
    tip_uv = item.place(tip.pixels()) if tip is not None else None
    rz = blade.thick + 0.06

    def build(bone, sx):
        item.tube(bone, mirrored(blade.path(tip_from), sx), "Fur", "Tip" if tip_card is not None else "Fur", sides=12, power=power)
        card_at(item, bone, sx, origin[0], origin[1], blade.z + rz + 0.05, (art.w, art.h), uv, "Inner")
        card_at(item, bone, sx, bx, blade.base, blade.z - rz - 0.09, (back.w, back.h), back_uv, "Fur")
        if tip_uv:
            card_at(item, bone, sx, bx, blade.base, blade.z - rz - 0.12, (tip.w, tip.h), tip_uv, "Tip")

    pair(item, "ear", (blade.cx0, blade.base + 0.1, blade.z), rotation, anim, spring, build, lag)


def spring(stiffness, damping, inertia, drive):
    return {"type": "spring", "stiffness": stiffness, "damping": damping, "inertia": inertia, "drive": drive}


def cat_ears():
    it = Item("cat_ears", "Cat Ears", "head", ["head", "ears", "cat", "animated"],
              [("Fur", "#e8c9a0"), ("Inner", "#ff9fb8")], texel=TEXEL, size=128)
    blade = Blade(2.4, 2.75, 31.9, 4.4, -0.6, 0.5, lambda t: 1.85 * (1 - t) ** 0.9 if t < 0.9 else 1.85 * 0.1 ** 0.9 * (1 - (t - 0.9) / 0.1) + 0.12)
    art, x0, y0 = inner_art(blade, 0.35, 0.82, 0.6, 5)
    phys = spring(44.0, 4.2, 1.1, {"air": [-12, 0, -8], "sprint": [-10, 0, -5], "speed": [-3, 0, 0], "sneak": [1, 0, 3]})
    ears(it, blade, art, (x0, y0), (-6, 0, -4), phys, {"type": "twitch", "axis": "z", "amplitude": 9, "speed": 0.32}, (0.0, 2.2), seed=5)
    return it


def fox_ears():
    it = Item("fox_ears", "Fox Ears", "head", ["head", "ears", "fox", "animated"],
              [("Fur", "#e8742a"), ("Inner", "#fff1dc"), ("Tip", "#2a1a14")], texel=TEXEL, size=128)
    blade = Blade(2.3, 3.1, 31.9, 5.6, -0.5, 0.5, lambda t: 1.7 * (1 - t) ** 0.85 if t < 0.92 else 0.12 + 0.2 * (1 - t) / 0.08)
    art, x0, y0 = inner_art(blade, 0.3, 0.8, 0.66, 6, depth_dark=45, seed=4)
    phys = spring(44.0, 4.2, 1.1, {"air": [-12, 0, -8], "sprint": [-12, 0, -5], "speed": [-3, 0, 0], "sneak": [1, 0, 3]})
    ears(it, blade, art, (x0, y0), (-6, 0, -4), phys, {"type": "twitch", "axis": "z", "amplitude": 8, "speed": 0.26}, (0.0, 1.7), tip_from=0.62, tip_card=0.6, seed=9)
    return it


def hare_ears():
    it = Item("hare_ears", "Hare Ears", "head", ["head", "ears", "hare", "animated"],
              [("Fur", "#f2ece4"), ("Inner", "#f7a9bf")], texel=TEXEL, size=256)
    blade = Blade(1.9, 3.3, 31.9, 10.5, -0.5, 0.55, lambda t: 1.3 * math.sin(math.pi * (0.06 + 0.88 * t)) ** 0.62 * (0.75 + 0.25 * (1 - t)), bend=0.35, rings=16)
    art, x0, y0 = inner_art(blade, 0.4, 0.88, 0.58, 4, depth_dark=62, seed=7)
    phys = spring(30.0, 3.4, 1.3, {"air": [-16, 0, -10], "sprint": [-22, 0, -8], "speed": [-5, 0, 0], "sneak": [6, 0, 6]})
    ears(it, blade, art, (x0, y0), (-3, 0, -6), phys, {"type": "twitch", "axis": "z", "amplitude": 7, "speed": 0.22}, (0.0, 2.6), tip_from=0.7, seed=11)
    return it


def flap(face, art, base=250, seed=0):
    """Fur for a hanging ear: darker towards the lower end, vertical strokes, lighter rim."""
    v = art.Y / max(art.h, 1e-6)
    side = face in ("front", "back", "left", "right")
    cell = np.floor(art.X / 0.125) * 12.9898 + np.floor(art.Y / 0.125) * 78.233 + seed
    grain = np.sin(cell) * 43758.5453
    grain -= np.floor(grain)
    art.put(np.ones(art.X.shape), base * (1 - 0.3 * v ** 1.4 * side) * (1 - 0.07 * grain))
    rng = np.random.default_rng(seed + 1)
    for k in range(int(art.w * art.h * 5)):
        x0, y0 = rng.uniform(0, art.w), rng.uniform(0, art.h)
        m, _ = art.stroke([(x0, y0), (x0 + rng.uniform(-0.05, 0.05), y0 + rng.uniform(0.25, 0.7))], 0.07, 0.015)
        art.put(m, 205 if k % 3 else 255, 0.5)
    edge = np.minimum.reduce([art.X, art.w - art.X, art.Y, art.h - art.Y])
    art.put((edge < 0.07).astype(float), 255, 0.35)


def dog_ears():
    it = Item("dog_ears", "Dog Ears", "head", ["head", "ears", "dog", "animated"],
              [("Fur", "#c08a55"), ("Tip", "#6d4528")], texel=TEXEL, size=256)
    # x measured outward from the head side (x = 4); z centred at -0.4, steps get narrower towards the lower end
    parts = (
        ("root", (-0.7, 0.8, 31.9, 32.6, -1.9, 1.1), "Fur"),
        ("upper", (0.1, 0.8, 27.9, 32.0, -1.9, 1.1), "Fur"),
        ("middle", (0.1, 0.8, 26.1, 27.9, -1.65, 0.85), "Fur"),
        ("lower", (0.1, 0.8, 25.0, 26.1, -1.3, 0.5), "Tip"),
    )
    uv = {}
    for name, (xa, xb, y0, y1, z0, z1), _ in parts:
        uv[name] = box_uv(it, (xb - xa, y1 - y0, z1 - z0), lambda f, a, n=name: flap(f, a, 250, len(n)))
    phys = spring(30.0, 3.2, 1.5, {"air": [10, 0, 24], "sprint": [8, 0, 14], "speed": [3, 0, 6], "sneak": [-2, 0, -4]})

    def build(bone, sx):
        for name, (xa, xb, y0, y1, z0, z1), tint in parts:
            lo, hi = 4.0 + xa, 4.0 + xb
            it.box(bone, (lo if sx > 0 else -hi, y0, z0), (xb - xa, y1 - y0, z1 - z0), uv[name], tint)

    pair(it, "ear", (4.1, 32.0, -0.4), (0, 0, 0), {"type": "sway", "axis": "z", "amplitude": 3.5, "speed": 0.55}, phys, build, (0.0, 1.9))
    return it


def tail(item, curve, radius, mix, anim, physics, wave, rings=40, sides=12, fluff=0.0, tufts=10):
    """One tube on one bone. The follow-through comes from the tube's own travelling wave, so the tail bends along its
    length instead of being cut into parts that could drift apart."""
    pts = bezier(curve, rings)
    n = len(pts)
    path = []
    for i, (x, y, z) in enumerate(pts):
        t = i / (n - 1)
        r = radius(t) * (1 + fluff * math.sin(t * tufts * 2 * math.pi) * min(1, t * 6))
        if t > 0.9:
            r *= max(0.28, math.sqrt(max(0.0, 1 - ((t - 0.9) / 0.1) ** 2)))
        path.append((x, y, z, r, r, mix(t)))
    bone = item.bone("tail", curve[0], anim=anim, physics=physics)
    item.tube(bone, path, "Fur", "Tip", sides=sides, wave=wave)


def bands(spans, soft=0.012):
    def mix(t):
        return max((smooth((t - a) / soft) * smooth((b - t) / soft) for a, b in spans), default=0.0)
    return mix


def cat_tail():
    it = Item("cat_tail", "Cat Tail", "waist", ["waist", "tail", "cat", "animated"], [("Fur", "#e8c9a0"), ("Tip", "#6b4c3b")], texel=1, size=32)
    curve = [(0, 13.4, -2.3), (0, 12.0, -5.8), (0, 13.6, -9.8), (0, 18.0, -10.6), (0, 21.0, -8.6)]
    phys = spring(38.0, 4.4, 1.0, {"air": [10, 0, 0], "sprint": [8, 0, 0], "speed": [2, 0, 0], "sneak": [-6, 0, 0]})
    anim = {"type": "sway", "axis": "y", "amplitude": 7, "speed": 0.42, "phase": 0}
    tail(it, curve, lambda t: 0.85 + 0.4 * math.sin(math.pi * min(1, t * 1.15)), bands(((0.2, 0.28), (0.37, 0.45), (0.54, 0.62), (0.71, 0.79), (0.87, 1.01))), anim, phys, {"axis": "x", "amplitude": 1.6, "speed": 0.42, "freq": 1.4}, fluff=0.03, tufts=12)
    return it


def fox_tail():
    it = Item("fox_tail", "Fox Tail", "waist", ["waist", "tail", "fox", "animated"], [("Fur", "#e8742a"), ("Tip", "#fff4e6")], texel=1, size=32)
    curve = [(0, 13.6, -2.4), (0, 12.8, -6.0), (0, 10.6, -11.0), (0, 7.4, -15.2)]
    phys = spring(34.0, 4.0, 1.1, {"air": [10, 0, 0], "sprint": [16, 0, 0], "speed": [4, 0, 0], "sneak": [-5, 0, 0]})
    anim = {"type": "sway", "axis": "y", "amplitude": 6, "speed": 0.4, "phase": 0}
    tail(it, curve, lambda t: 0.85 + 1.7 * math.sin(math.pi * t) ** 0.8, lambda t: smooth((t - 0.72) / 0.1), anim, phys, {"axis": "x", "amplitude": 1.5, "speed": 0.4, "freq": 1.2}, sides=14, fluff=0.025, tufts=7)
    return it


def dog_tail():
    it = Item("dog_tail", "Dog Tail", "waist", ["waist", "tail", "dog", "animated"], [("Fur", "#c08a55"), ("Tip", "#f3e4cd")], texel=1, size=32)
    curve = [(0, 13.4, -2.3), (0, 14.6, -5.0), (0, 17.4, -6.6), (0, 21.2, -6.0)]
    phys = spring(60.0, 5.5, 0.9, {"air": [8, 0, 0], "sprint": [10, 0, 0], "speed": [2, 0, 0], "sneak": [-8, 0, 0]})
    anim = {"type": "wag", "axis": "y", "amplitude": 15, "speed": 1.7, "phase": 0}
    tail(it, curve, lambda t: 0.95 - 0.35 * t + 0.5 * math.sin(math.pi * t), lambda t: smooth((t - 0.8) / 0.12), anim, phys, {"axis": "x", "amplitude": 1.3, "speed": 1.7, "freq": 1.1}, fluff=0.02, tufts=6)
    return it


def ball(item, bone, c, r, tint, tint2=None, rings=9, sides=14):
    path = []
    for i in range(rings):
        a = math.pi * (i + 0.5) / rings
        path.append((c[0], c[1], c[2] - r * math.cos(a), r * math.sin(a), r * math.sin(a), 0.5 - 0.5 * math.cos(a)))
    item.tube(bone, path, tint, tint2, sides=sides)


def hare_tail():
    it = Item("hare_tail", "Hare Tail", "waist", ["waist", "tail", "hare", "animated"], [("Fur", "#f4efe8"), ("Tip", "#cdbfb2")], texel=1, size=32)
    phys = spring(46.0, 4.6, 1.2, {"air": [10, 0, 0], "sprint": [8, 0, 0], "speed": [3, 0, 0], "sneak": [-4, 0, 0]})
    bone = it.bone("puff", (0, 13.8, -2.4), anim={"type": "wag", "axis": "y", "amplitude": 5, "speed": 0.9, "phase": 0}, physics=phys)
    ball(it, bone, (0, 14.2, -4.6), 2.3, "Fur", "Tip")
    for dx, dy, dz, r in ((1.3, 0.8, 0.0, 1.5), (-1.4, 0.3, -0.5, 1.45), (0.2, 1.5, -1.0, 1.35), (-0.4, -1.1, -0.7, 1.3), (1.1, -0.6, -1.3, 1.2)):
        ball(it, bone, (dx, 14.2 + dy, -4.6 + dz), r, "Fur", "Tip", rings=7, sides=10)
    return it


def build():
    return [cat_ears(), fox_ears(), hare_ears(), dog_ears(), cat_tail(), fox_tail(), dog_tail(), hare_tail()]
