import math

import numpy as np

from item import Item
from paint import Art, bezier

TEXEL = 6
W, H, STRIPS = 10.0, 16.0, 8
TOP, Z = 23.7, -2.55


def cloth(art, base=228, folds=22, dark=0.18):
    """Soft vertical folds that widen towards the hem, a little darker at the bottom and the sides."""
    u, v = art.X / art.w, art.Y / art.h
    fold = np.sin(art.X * 1.25 + 0.9 * np.sin(art.Y * 0.32 + u * 3.0)) * (0.35 + 0.65 * v)
    side = np.clip(np.minimum(u, 1 - u) * art.w / 1.6, 0, 1)
    return (base + folds * fold) * (1 - dark * v * v) * (0.86 + 0.14 * side)


def hem(art, kind, depth=1.2):
    """Alpha mask for the bottom edge."""
    u = art.X / art.w
    bottom = art.h - art.Y
    if kind == "scallop":
        edge = depth * (1 - np.abs(np.sin(u * math.pi * 5)))
    elif kind == "torn":
        edge = depth * (0.5 + 0.5 * np.sin(u * 23.0) * np.sin(u * 7.3 + 1.0)) + depth * 0.6 * (np.sin(u * 41.0) > 0.6)
    elif kind == "points":
        edge = depth * 2 * np.abs((u * 4) % 1 - 0.5)
    elif kind == "leaves":
        edge = depth * (1 - np.abs(np.sin(u * math.pi * 6)) ** 0.5)
    else:
        edge = np.zeros(u.shape)
    return (bottom > edge).astype(float)


def trim(art, width=0.55):
    d = np.minimum.reduce([art.X, art.w - art.X])
    return (d < width).astype(float)


def star(art, c, r):
    dx, dy = np.abs(art.X - c[0]), np.abs(art.Y - c[1])
    return ((dx * dy < r * r * 0.06) & (dx + dy < r)).astype(float)


def cape(item, layers, sway=4.5, speed=0.3):
    """Every layer is its own cloth bone with the same strips, physics and motion, so the layers move as one."""
    # sway grows by up to 1.6x at full speed; the cape must never swing forward past hanging straight down
    tilt = round(sway * 1.6 * 1.15 + 1.5, 1)
    rows = round(H * TEXEL) // STRIPS
    sh = H / STRIPS
    for n, (art, tint) in enumerate(layers):
        px = art.pixels()
        uvs = [item.place(px[i * rows : (i + 1) * rows]) for i in range(STRIPS)]
        bone = item.bone(f"cape{n}" if n else "cape", (0, TOP, Z), (tilt, 0, 0),
                         anim={"type": "sway", "axis": "x", "amplitude": sway, "speed": speed, "phase": 0},
                         physics={"type": "cloth", "stiffness": 36.0, "damping": 2.6, "inertia": 1.1, "wind": 1.1})
        for i, uv in enumerate(uvs):
            item.card(bone, (-W / 2, TOP - (i + 1) * sh, Z - n * 0.06), (W, sh, 0.04), uv, tint)
    return item


def moonlit():
    base, moon, edge = Art(W, H, TEXEL), Art(W, H, TEXEL), Art(W, H, TEXEL)
    keep = hem(base, "scallop", 0.9)
    base.put(keep, cloth(base, 225, 18))
    c = (5.6, 6.2)
    disc = np.hypot(moon.X - c[0], moon.Y - c[1]) < 2.3
    bite = np.hypot(moon.X - c[0] - 1.0, moon.Y - c[1] + 0.5) < 2.0
    moon.put((disc & ~bite).astype(float), 250 - 40 * np.clip((moon.X - c[0] + 2.3) / 4.6, 0, 1))
    for s, r in (((2.4, 3.2), 0.7), ((7.8, 10.2), 0.55), ((3.1, 11.6), 0.45), ((8.2, 3.0), 0.4), ((2.0, 7.6), 0.35), ((6.0, 13.0), 0.5)):
        moon.put(star(moon, s, r), 255)
    edge.put(keep * np.maximum(trim(edge, 0.45), (edge.Y < 0.7).astype(float)), 235)
    edge.put(keep * ((edge.h - edge.Y) < 1.6) * (1 - hem(edge, "scallop", 0.9 + 0.35)), 235)
    it = Item("moonlit_cape", "Moonlit Cape", "cape", ["cape", "night", "stars", "animated"],
              [("Cloth", "#27305e"), ("Moon", "#fff3c4"), ("Trim", "#c9d3ff")], texel=TEXEL)
    return cape(it, [(base, "Cloth"), (moon, "Moon"), (edge, "Trim")])


def ember():
    base, flame, core = Art(W, H, TEXEL), Art(W, H, TEXEL), Art(W, H, TEXEL)
    keep = hem(base, "points", 1.4)
    base.put(keep, cloth(base, 205, 20, 0.1))
    u = flame.X / flame.w
    height = 5.5 + 2.2 * np.sin(u * 19) * np.sin(u * 7 + 0.8) + 1.5 * np.sin(u * 37 + 2)
    rise = flame.h - flame.Y
    flame.put(keep * (rise < height), 240 - 60 * np.clip(rise / np.maximum(height, 0.1), 0, 1))
    core.put(keep * (rise < height * 0.55), 255)
    for x in (2.0, 5.0, 8.1):
        m, _ = core.stroke(bezier([(x, 15.0), (x - 0.6, 12.2), (x + 0.5, 10.0)], 12), 0.32, 0.05)
        core.put(m * keep, 255)
    it = Item("ember_cape", "Ember Cape", "cape", ["cape", "fire", "animated"],
              [("Cloth", "#3a1410"), ("Flame", "#ff5a24"), ("Core", "#ffd45e")], texel=TEXEL)
    return cape(it, [(base, "Cloth"), (flame, "Flame"), (core, "Core")], sway=5.0, speed=0.36)


def web():
    base, threads, dew = Art(W, H, TEXEL), Art(W, H, TEXEL), Art(W, H, TEXEL)
    keep = hem(base, "torn", 1.3)
    base.put(keep, cloth(base, 200, 16, 0.25))
    hub = (0.0, 0.0)
    ends = [(10.0, 0.6), (10.0, 5.0), (9.0, 10.0), (6.0, 14.4), (2.0, 15.5), (0.0, 12.0)]
    lines = np.zeros(threads.X.shape, bool)
    for e in ends:
        lines |= threads.dist([hub, e]) < 0.13
    for f in (0.22, 0.4, 0.58, 0.76, 0.94):
        pts = [(e[0] * f, e[1] * f) for e in ends]
        path = []
        for a, b in zip(pts, pts[1:]):
            mid = ((a[0] + b[0]) / 2 * 0.86, (a[1] + b[1]) / 2 * 0.86)
            path += bezier([a, mid, b], 10)[1:]
        lines |= threads.dist([pts[0]] + path) < 0.11
    threads.put(keep * lines, 240)
    for c in ((4.3, 3.4), (6.6, 7.4), (2.7, 9.1), (7.9, 2.0)):
        m, d = dew.disc(c, 0.3)
        dew.put(m * keep, 255 - 70 * d)
    it = Item("web_cape", "Web Cape", "cape", ["cape", "halloween", "spooky", "animated"],
              [("Cloth", "#2a1d3a"), ("Web", "#ececf4"), ("Dew", "#ff8a2a")], texel=TEXEL)
    return cape(it, [(base, "Cloth"), (threads, "Web"), (dew, "Dew")], sway=5.0, speed=0.28)


def crystal():
    base, shards, glow = Art(W, H, TEXEL), Art(W, H, TEXEL), Art(W, H, TEXEL)
    keep = (base.Y < H - 2.6).astype(float)
    base.put(keep, cloth(base, 222, 18))
    for i, x in enumerate(np.linspace(0.9, 9.1, 7)):
        length = 2.6 - 0.9 * abs(i - 3) / 3 + 0.3 * (i % 2)
        top = H - 2.9
        s, n = (shards.Y - top) / length, (shards.X - x) / 0.62
        half = np.clip(1 - s, 0, 1) * np.where(s < 0.2, 0.6 + 2 * s, 1)
        m = ((s > 0) & (s < 1) & (np.abs(n) < half)).astype(float)
        shards.put(m, np.where(n < 0, 235, 150) + 20 * s)
        glow.put(m * (n < -0.75 * half), 255)
    facets = (np.abs(np.sin((base.X + base.Y * 0.6) * 1.1)) < 0.05) | (np.abs(np.sin((base.X - base.Y * 0.6) * 1.1)) < 0.05)
    glow.put(keep * facets, 255, 0.35)
    c = (5.0, 6.0)
    dx, dy = shards.X - c[0], shards.Y - c[1]
    gem = (np.abs(dx) / 2.2 + np.abs(dy + 0.6 * (dy < 0)) / 3.4) < 1
    shards.put(gem.astype(float), np.where(dx < 0, np.where(dy < -0.9, 250, 225), np.where(dy < -0.9, 190, 140)))
    glow.put((gem & ((np.abs(dx) < 0.07) | (np.abs(dy + 0.9) < 0.07))).astype(float), 255, 0.8)
    edge = (np.abs(dx) / 2.2 + np.abs(dy + 0.6 * (dy < 0)) / 3.4)
    glow.put(((edge > 0.9) & (edge < 1.0)).astype(float), 255)
    glow.put(trim(glow, 0.35) * keep, 245)
    band = (np.abs(glow.Y - (H - 2.75)) < 0.3).astype(float)
    glow.put(band, 250)
    it = Item("crystal_cape", "Crystal Cape", "cape", ["cape", "crystal", "animated"],
              [("Cloth", "#3b2c6e"), ("Crystal", "#a98bff"), ("Glow", "#f1e9ff")], texel=TEXEL)
    return cape(it, [(base, "Cloth"), (shards, "Crystal"), (glow, "Glow")])


def cherub():
    base, gold = Art(W, H, TEXEL), Art(W, H, TEXEL)
    keep = hem(base, "scallop", 0.8)
    base.put(keep, cloth(base, 250, 14, 0.12))
    gold.put(keep * trim(gold, 0.4), 255)
    gold.put(keep * (np.abs(gold.Y - 0.5) < 0.3), 255)
    gold.put(keep * ((gold.h - gold.Y) < 1.3) * (1 - hem(gold, "scallop", 0.8 + 0.4)), 255)
    for side in (-1, 1):
        for k, (length, ang) in enumerate(((3.8, 28), (3.5, 8), (3.0, -12), (2.5, -32), (1.9, -52))):
            a = math.radians(ang)
            root = (5.0 + side * 0.5, 6.3 + 0.25 * k)
            tip = (root[0] + side * length * math.cos(a), root[1] - length * math.sin(a))
            m, d = gold.stroke(bezier([root, ((root[0] + tip[0]) / 2, (root[1] + tip[1]) / 2 - 0.3), tip], 10), 0.55, 0.3)
            gold.put(m, 245 - 35 * d)
    ring = np.abs(np.hypot((gold.X - 5.0) / 1.5, (gold.Y - 3.7) / 0.5) - 1) < 0.22
    gold.put(ring.astype(float), 255)
    m, d = gold.disc((5.0, 6.7), 0.7)
    gold.put(m, 255 - 30 * d)
    it = Item("cherub_cape", "Cherub Cape", "cape", ["cape", "white", "gold", "animated"],
              [("Cloth", "#ffffff"), ("Gold", "#ffd77a")], texel=TEXEL)
    return cape(it, [(base, "Cloth"), (gold, "Gold")], sway=4.0, speed=0.32)


def battle():
    base, patch, stitch = Art(W, H, TEXEL), Art(W, H, TEXEL), Art(W, H, TEXEL)
    keep = hem(base, "torn", 2.2)
    rng = np.random.default_rng(7)
    grain = rng.normal(0, 6, base.X.shape)
    base.put(keep, cloth(base, 200, 26, 0.3) + grain)
    for (x, y, w, h) in ((1.2, 4.0, 2.6, 2.2), (6.0, 9.0, 2.4, 2.8)):
        box = ((patch.X > x) & (patch.X < x + w) & (patch.Y > y) & (patch.Y < y + h)).astype(float)
        patch.put(box * keep, 205 + rng.normal(0, 5, patch.X.shape))
        ring = box * ((np.minimum.reduce([patch.X - x, x + w - patch.X, patch.Y - y, y + h - patch.Y]) < 0.25)) * ((np.floor((patch.X + patch.Y) * 3) % 2) == 0)
        stitch.put(ring, 240)
    stitch.put(keep * (np.abs(stitch.Y - 0.6) < 0.18) * ((np.floor(stitch.X * 3) % 2) == 0), 240)
    it = Item("battle_cape", "Battle Cape", "cape", ["cape", "torn", "animated"],
              [("Cloth", "#7a2b2b"), ("Patch", "#a68b5b"), ("Stitch", "#f0e3c2")], texel=TEXEL)
    return cape(it, [(base, "Cloth"), (patch, "Patch"), (stitch, "Stitch")], sway=5.5, speed=0.34)


def neon():
    base, line, inner = Art(W, H, TEXEL), Art(W, H, TEXEL), Art(W, H, TEXEL)
    keep = hem(base, "points", 1.1)
    base.put(keep, cloth(base, 120, 14, 0.2))
    outline = [(0, 0), (W, 0)] + [(W - k * W / 8, H - (1.1 if k % 2 == 0 else 0)) for k in range(9)]
    d = line.dist([(W, 0), (W, H - 1.1)] + outline[2:] + [(0, 0)])
    line.put(((d < 0.22) | (line.Y < 0.4)).astype(float) * keep, 255)
    for path in ([(2.0, 0.4), (2.0, 5.0), (3.6, 6.6), (3.6, 12.0)], [(8.0, 0.4), (8.0, 4.0), (6.4, 5.6), (6.4, 13.0)], [(5.0, 0.4), (5.0, 9.0)]):
        dd = inner.dist(path)
        inner.put((dd < 0.12).astype(float) * keep, 255)
        m, _ = inner.disc(path[-1], 0.38)
        inner.put(m * keep, 255)
    it = Item("neon_cape", "Neon Cape", "cape", ["cape", "neon", "animated"],
              [("Cloth", "#1d1238"), ("Line", "#ff47d7"), ("Inner", "#45e9ff")], texel=TEXEL)
    return cape(it, [(base, "Cloth"), (inner, "Inner"), (line, "Line")], sway=4.5, speed=0.34)


def leaf():
    base, leaves, vine = Art(W, H, TEXEL), Art(W, H, TEXEL), Art(W, H, TEXEL)
    keep = (base.Y < H - 1.6).astype(float)
    base.put(keep, cloth(base, 150, 14, 0.15))
    rows = np.arange(0.4, H - 1.4, 1.75)
    for r, y in enumerate(rows):
        shift = 0.0 if r % 2 else 0.85
        for x in np.arange(-0.85 + shift, W + 0.9, 1.7):
            length = 2.5 if y < rows[-1] - 0.1 else 3.2
            s_ = leaves.Y - y
            n = leaves.X - x
            half = 0.88 * np.clip(np.sin(np.clip(s_ / length, 0, 1) * math.pi), 0, 1) ** 0.7
            m = ((s_ > 0) & (s_ < length) & (np.abs(n) < half)).astype(float)
            vein = (np.abs(n) < 0.06) & (s_ > 0.2)
            shade = np.where(n < 0, 232, 182) - 18 * (r % 3 == 1) - 40 * vein
            leaves.put(m, shade * (0.9 + 0.1 * np.clip(s_ / length, 0, 1)))
    vine.put(trim(vine, 0.3) * keep, 190)
    vine.put((vine.Y < 0.45).astype(float), 190)
    it = Item("leaf_cape", "Leaf Cape", "cape", ["cape", "nature", "animated"],
              [("Cloth", "#2f5a2d"), ("Leaf", "#6fcf5b"), ("Vine", "#7a5c3a")], texel=TEXEL)
    return cape(it, [(base, "Cloth"), (leaves, "Leaf"), (vine, "Vine")], sway=4.5, speed=0.3)


def build():
    return [moonlit(), ember(), web(), crystal(), cherub(), battle(), neon(), leaf()]
