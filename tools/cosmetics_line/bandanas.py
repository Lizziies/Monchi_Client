import math

import numpy as np

from item import Item
from paint import Art, bezier

TEXEL = 8
OFF = 4.3  # band distance from the head centre, 0.3 off the skin
BAND_Y = 30.2


def wrinkles(art, base=228, amp=14):
    u = art.X / art.w
    return base + amp * np.sin(art.X * 2.1 + 1.3 * np.sin(art.Y * 3.0 + u * 5)) * 0.6 - 18 * (np.abs(art.Y - art.h / 2) / (art.h / 2)) ** 2


def stitch(art, inset=0.18):
    edge = (np.abs(art.Y - inset) < 0.06) | (np.abs(art.Y - (art.h - inset)) < 0.06)
    return (edge & ((np.floor(art.X * 4) % 2) == 0)).astype(float)


def drop(art, c, size, angle):
    """Paisley teardrop: a disc with a curled tail."""
    s, n = art.local(c, angle)
    body = np.hypot(s, n) < size
    tail = (s < 0) & (s > -size * 2.2) & (np.abs(n - 0.35 * size * (s / size) ** 2) < size * (1 + s / (size * 2.2)) * 0.7)
    ring = np.abs(np.hypot(s, n) - size * 0.55) < size * 0.14
    return (body | tail).astype(float), ring.astype(float)


def tail_art(length, width, cut="v"):
    art = Art(width, length, TEXEL)
    u = art.Y / length
    half = width / 2 * (0.78 + 0.22 * u)
    inside = np.abs(art.X - width / 2) < half
    end = length - (0.9 * np.abs(art.X - width / 2) / (width / 2) if cut == "v" else 0.6 * (art.X / width))
    return art, (inside & (art.Y < end)).astype(float)


def knot_art():
    art = Art(2.2, 2.0, TEXEL)
    d = np.hypot((art.X - 1.1) / 1.1, (art.Y - 1.0) / 0.95)
    folds = np.abs(np.sin(np.arctan2(art.Y - 1.0, art.X - 1.1) * 3)) < 0.18
    return art, (d < 1).astype(float), d, folds


def band(item, layers, height, y=BAND_Y):
    """Four cards around the head: front, right, back, left. layers: [(fn(side) -> Art, tint)]."""
    for side, rot in enumerate((0, 90, 180, 270)):
        bone = item.bone(f"band_{side}", (0, y, 0), (0, rot, 0))
        for n, (paint, tint) in enumerate(layers):
            uv = item.place(paint(side).pixels())
            item.card(bone, (-OFF - 0.06, y - height / 2, OFF + n * 0.05), (2 * OFF + 0.12, height, 0.03), uv, tint)


def tails(item, layers, length, spread=13.0, tilt=20.0, sway=8.0, speed=0.7, stream=38.0, y=BAND_Y - 0.2, at=(0.0, -OFF - 0.25), gap=0.35):
    """Two ribbon ends hanging from the knot; each is a spring bone, so they flutter and stream back."""
    uvs = [(item.place(art.pixels()), tint, art.w) for art, tint in layers]
    for side, label in ((1, "r"), (-1, "l")):
        x = at[0] + side * gap
        phys = {"type": "spring", "stiffness": 28.0, "damping": 3.4, "inertia": 1.5,
                "drive": {"sprint": [stream, side * 6, 0], "speed": [stream * 0.4, 0, 0], "air": [18, 0, side * 6], "sneak": [6, 0, 0]}}
        bone = item.bone(f"tail_{label}", (x, y, at[1]), (tilt, side * 24, side * spread),
                         anim={"type": "sway", "axis": "x", "amplitude": sway, "speed": speed, "phase": 0.0 if side > 0 else 1.9}, physics=phys)
        for n, (uv, tint, w) in enumerate(uvs):
            ox = x - w / 2 + side * 0.3
            item.card(bone, (ox, y - length, at[1] - 0.05 - n * 0.05), (w, length, 0.03), uv, tint, mirror=side < 0)


def knot(item, layers):
    bone = item.bone("knot", (0, BAND_Y, -OFF), (0, 0, 0))
    for n, (art, tint) in enumerate(layers):
        uv = item.place(art.pixels())
        item.card(bone, (-art.w / 2, BAND_Y - art.h / 2, -OFF - 0.12 - n * 0.05), (art.w, art.h, 0.03), uv, tint)


def classic_like(id, name, tags, tints, pattern, tail_len=5.2, tail_w=1.5, **kw):
    """Cloth band with a pattern layer, a knot at the back and two ends."""
    h = 1.7

    def cloth(side):
        art = Art(2 * OFF + 0.12, h, TEXEL)
        art.put(np.ones(art.X.shape), wrinkles(art))
        art.put(stitch(art), 150)
        return art

    def pat(side):
        art = Art(2 * OFF + 0.12, h, TEXEL)
        pattern(art, side)
        return art

    it = Item(id, name, "face", tags, tints, texel=TEXEL, size=256)
    band(it, [(cloth, tints[0][0]), (pat, tints[1][0])], h)
    k, mask, d, folds = knot_art()
    k.put(mask, 236 - 70 * d ** 2 - 40 * folds)
    knot(it, [(k, tints[0][0])])
    t, tmask = tail_art(tail_len, tail_w)
    t.put(tmask, wrinkles(t, 225, 10) - 30 * (t.Y / tail_len))
    tp, tpmask = tail_art(tail_len, tail_w)
    pattern(tp, 5)
    tp.a *= tpmask
    tails(it, [(t, tints[0][0]), (tp, tints[1][0])], tail_len, **kw)
    return it


def paisley_pattern(art, side):
    rng = np.random.default_rng(side + 3)
    x = 0.6
    while x < art.w:
        y = art.h * (0.5 + 0.18 * rng.uniform(-1, 1)) if art.h < 3 else rng.uniform(0.6, art.h - 0.6)
        body, ring = drop(art, (x, y), 0.36, rng.uniform(0, math.tau))
        art.put(body, 245)
        art.put(ring * body, 120)
        art.put(art.disc((x + 0.75, y + rng.uniform(-0.4, 0.4)), 0.11)[0], 250)
        x += rng.uniform(1.2, 1.6)
    if art.h > 3:
        for k in range(int(art.h / 1.4)):
            body, ring = drop(art, (art.w / 2 + rng.uniform(-0.2, 0.2), 0.7 + 1.4 * k), 0.34, rng.uniform(0, math.tau))
            art.put(body, 245)


def flame_pattern(art, side):
    u = art.X
    height = art.h * (0.45 + 0.25 * np.sin(u * 4.1 + side) * np.sin(u * 1.7 + 2))
    rise = art.h - art.Y
    if art.h > 3:
        height = art.h * (0.35 + 0.06 * np.sin(u * 9))
    art.put((rise < height).astype(float), 245 - 50 * np.clip(rise / np.maximum(height, 0.05), 0, 1))


def star_pattern(art, side):
    rng = np.random.default_rng(side + 11)
    for _ in range(int(art.w * art.h / 2.2) + 1):
        c = (rng.uniform(0.3, art.w - 0.3), rng.uniform(0.3, art.h - 0.3))
        r = rng.uniform(0.18, 0.34)
        dx, dy = np.abs(art.X - c[0]), np.abs(art.Y - c[1])
        art.put(((dx * dy < r * r * 0.06) & (dx + dy < r)).astype(float), 255)
    if side == 0:
        c = (art.w / 2, art.h / 2)
        disc = np.hypot(art.X - c[0], art.Y - c[1]) < 0.62
        bite = np.hypot(art.X - c[0] - 0.28, art.Y - c[1] + 0.12) < 0.52
        art.put((disc & ~bite).astype(float), 255)
    art.put(((art.Y < 0.12) | (art.Y > art.h - 0.12)).astype(float) * (art.h < 3), 235)


def classic():
    return classic_like("classic_bandana", "Classic Bandana", ["face", "bandana", "paisley", "animated"],
                        [("Cloth", "#c8242b"), ("Pattern", "#ffffff")], paisley_pattern)


def ember():
    return classic_like("ember_bandana", "Ember Bandana", ["face", "bandana", "fire", "animated"],
                        [("Cloth", "#3a1410"), ("Flame", "#ff6a2a")], flame_pattern, sway=9.0, speed=0.8)


def moonlit():
    return classic_like("moonlit_bandana", "Moonlit Bandana", ["face", "bandana", "night", "animated"],
                        [("Cloth", "#27305e"), ("Stars", "#e9edff")], star_pattern, sway=7.0, speed=0.55)


def ninja():
    h = 1.9

    def cloth(side):
        art = Art(2 * OFF + 0.12, h, TEXEL)
        art.put(np.ones(art.X.shape), wrinkles(art, 205, 10))
        return art

    def trim(side):
        art = Art(2 * OFF + 0.12, h, TEXEL)
        art.put(stitch(art, 0.2), 255)
        art.put(((art.Y < 0.08) | (art.Y > h - 0.08)).astype(float), 220)
        return art

    it = Item("ninja_band", "Ninja Band", "face", ["face", "bandana", "headband", "animated"],
              [("Cloth", "#1f2433"), ("Trim", "#c9cfdb")], texel=TEXEL, size=256)
    band(it, [(cloth, "Cloth"), (trim, "Trim")], h)
    k, mask, d, folds = knot_art()
    k.put(mask, 215 - 70 * d ** 2 - 40 * folds)
    knot(it, [(k, "Cloth")])
    length = 9.5
    t, tmask = tail_art(length, 1.2, cut="slant")
    t.put(tmask, wrinkles(t, 210, 10) - 25 * (t.Y / length))
    tt, ttmask = tail_art(length, 1.2, cut="slant")
    tt.put(ttmask * ((np.abs(tt.X - 0.12) < 0.06) | (np.abs(tt.X - 1.08) < 0.06)).astype(float), 235)
    tails(it, [(t, "Cloth"), (tt, "Trim")], length, spread=9.0, tilt=24.0, sway=9.0, speed=0.6, stream=40.0)
    return it


def bow():
    h, y = 1.25, 31.0

    def satin(side):
        art = Art(2 * OFF + 0.12, h, TEXEL)
        k = np.abs(art.Y - h * 0.38) / (h / 2)
        art.put(np.ones(art.X.shape), 225 - 45 * np.clip(k, 0, 1) ** 2 + 10 * np.sin(art.X * 1.7))
        return art

    def dots(side):
        art = Art(2 * OFF + 0.12, h, TEXEL)
        for k, x in enumerate(np.arange(0.5, art.w, 0.95)):
            m, _ = art.disc((x, h * (0.36 if k % 2 else 0.68)), 0.17)
            art.put(m, 255)
        art.put((np.abs(art.Y - h * 0.3) < 0.05).astype(float), 255, 0.6)
        return art

    it = Item("bow_headband", "Bow Headband", "face", ["face", "headband", "bow", "cute", "animated"],
              [("Ribbon", "#ff6fae"), ("Shine", "#ffffff"), ("Charm", "#ffd77a")], texel=TEXEL, size=256)
    band(it, [(satin, "Ribbon"), (dots, "Shine")], h, y=y)

    w, bh = 6.2, 3.6
    cx, cy = w / 2, bh / 2
    ribbon, shine, charm = Art(w, bh, TEXEL), Art(w, bh, TEXEL), Art(w, bh, TEXEL)
    for side in (-1, 1):
        s_ = (ribbon.X - cx) * side
        k = np.clip(s_ / (w / 2 - 0.15), 0, 1)
        half = 0.42 + 1.32 * np.sin(k * math.pi * 0.92) ** 0.55
        dy = ribbon.Y - cy + 0.25 * k
        loop = (s_ > 0.35) & (np.abs(dy) < half) & (k < 1)
        inner = np.abs(dy) < half * 0.42
        shade = 240 - 34 * k - 26 * (dy > 0) - 40 * inner * (k > 0.25) * (k < 0.8)
        ribbon.put(loop.astype(float), shade)
        crease = loop & (np.abs(dy - 0.32 * half * np.sin(k * 4)) < 0.07) & (k > 0.18) & (k < 0.9)
        ribbon.put(crease.astype(float), 180)
        shine.put((loop & (np.abs(dy + half * 0.68) < 0.1) & (k > 0.22) & (k < 0.85)).astype(float), 255, 0.9)
        rim = loop & (np.abs(np.abs(dy) - half) < 0.09)
        ribbon.put(rim.astype(float), 170)
    m, d = ribbon.disc((cx, cy), 0.62)
    ribbon.put(m, 245 - 70 * d ** 2)
    hx, hy = (charm.X - cx) / 0.42, (cy - charm.Y) / 0.42 + 0.25
    heart = (hx * hx + hy * hy - 1) ** 3 - hx * hx * hy ** 3 <= 0
    charm.put(heart.astype(float), 255 - 50 * np.clip(np.hypot(charm.X - cx + 0.12, charm.Y - cy + 0.12) / 0.5, 0, 1))
    uvs = [(it.place(a.pixels()), t) for a, t in ((ribbon, "Ribbon"), (shine, "Shine"), (charm, "Charm"))]
    px, py, pz = 1.7, 32.4, -OFF - 0.3
    bone = it.bone("bow", (px, py, pz), (-8, 0, -12),
                   anim={"type": "sway", "axis": "z", "amplitude": 7.0, "speed": 0.85, "phase": 0},
                   physics={"type": "spring", "stiffness": 30.0, "damping": 2.8, "inertia": 1.9, "drive": {"air": [10, 0, -8], "sprint": [16, 0, 0], "speed": [6, 0, 0]}})
    for n, (uv, tint) in enumerate(uvs):
        it.card(bone, (px - cx, py - cy, pz - n * 0.05), (w, bh, 0.03), uv, tint)

    length = 4.6
    t, tmask = tail_art(length, 1.15)
    t.put(tmask, 238 - 40 * (t.Y / length) - 25 * (np.abs(t.X - 0.575) < 0.05))
    ts, tsmask = tail_art(length, 1.15)
    ts.put(tsmask * (np.abs(ts.X - 0.3) < 0.07) * (ts.Y > 0.4) * (ts.Y < length - 0.8), 255, 0.85)
    tails(it, [(t, "Ribbon"), (ts, "Shine")], length, spread=16.0, tilt=14.0, sway=11.0, speed=0.95, stream=42.0, y=py - 0.4, at=(px, pz - 0.05), gap=0.3)
    return it


def build():
    return [classic(), ninja(), bow(), ember(), moonlit()]
