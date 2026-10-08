import math

import numpy as np

from item import Item, wing_pair
from paint import Art, along, bezier, facet_shard

TEXEL = 6


def crystal():
    w, h, root = 15.0, 14.6, (0.4, 6.0)
    base, glow = Art(w, h, TEXEL), Art(w, h, TEXEL)
    arm = bezier([(0.2, 6.0), (3.5, 2.6), (8.0, 0.6), (11.6, 0.9)])
    main = [(0.12, 3, 5.2, 1.7), (0.4, 9, 7.6, 2.1), (0.68, 17, 9.6, 2.3), (0.96, 27, 10.0, 2.1)]
    small = [((a[0] + b[0]) / 2, (a[1] + b[1]) / 2 + 2, (a[2] + b[2]) * 0.3, 1.35) for a, b in zip(main, main[1:])]
    top = np.full(base.X.shape, -1)
    shards = []
    for i, (t, out, length, width) in enumerate(main + small):
        p, _ = along(arm, t)
        p = (p[0] - 0.15, p[1] - 0.25)
        lift = 0 if i < len(main) else 18
        mask, u, nn = facet_shard(base, p, math.radians(90 - out), length, width, lit=225 + lift, mid=178 + lift, dark=118 + lift)
        top[mask > 0] = i
        shards.append((mask, u, nn))
    for i, (mask, u, nn) in enumerate(shards):
        mine = (top == i) & (mask > 0)
        rim = mine & (nn < -0.86) & (u > 0.1)
        glow.put(rim.astype(float), 255)
        ridge = mine & (np.abs(np.abs(nn) - 0.34) < 0.05) & (u > 0.12) & (u < 0.96)
        glow.put(ridge.astype(float), 240, 0.55)
        glint = mine & (nn < -0.34) & (nn > -0.86) & (np.abs(u - 0.62) < 0.07)
        glow.put(glint.astype(float), 255, 0.8)
    spar, shine = Art(w, h, TEXEL), Art(w, h, TEXEL)
    m, d = spar.stroke(arm, 0.78, 0.32)
    spar.put(m, 70 + 80 * (1 - d) ** 2)
    hi = bezier([(0.6, 5.3), (3.7, 2.1), (8.0, 0.2), (11.4, 0.5)])
    m, _ = shine.stroke(hi, 0.14, 0.07)
    shine.put(m, 230, 0.9)
    for t in (0.12, 0.68):
        p, _ = along(arm, t)
        m, d = shine.disc(p, 0.5)
        shine.put(m, 180 + 75 * (1 - d))
    it = Item("crystal_wings", "Crystal Wings", "wings", ["wings", "crystal", "animated"], [("Crystal", "#a98bff"), ("Glow", "#f1e9ff")], texel=TEXEL)
    return wing_pair(it, [(base, "Crystal"), (glow, "Glow"), (spar, "Crystal", "arm"), (shine, "Glow", "arm")], root)


ARM = [(0.2, 6.0), (3.5, 2.6), (8.0, 0.6), (11.6, 0.9)]
W, H, ROOT = 15.0, 14.6, (0.4, 6.0)


def flame(art, top, angle, length, width, curl=0.3):
    s, n = art.local(top, angle)
    u = s / length
    n = n - curl * width * np.sin(np.clip(u, 0, 1) * math.pi * 1.4) * np.clip(u, 0, 1) ** 2
    half = np.where(u < 0.34, width * np.clip(u / 0.34, 0, 1) ** 0.55, width * np.clip((1 - u) / 0.66, 0, 1) ** 1.25)
    half = half * (1 - 0.16 * np.clip(np.sin(u * 17), 0, 1) * np.clip(u - 0.4, 0, 1))
    half = np.maximum(half, np.where((u > -0.08) & (u < 0.05), width * 0.5, 0))
    mask = ((u > -0.08) & (u < 1) & (np.abs(n) < half)).astype(float)
    return mask, u, n / np.maximum(half, 1e-6)


def ember():
    base, core = Art(W, H, TEXEL), Art(W, H, TEXEL)
    arm = bezier(ARM)
    primaries = [(0.08, 2, 4.6, 0.95), (0.24, 6, 6.0, 1.05), (0.4, 10, 7.4, 1.15), (0.56, 14, 8.6, 1.2), (0.72, 19, 9.6, 1.2), (0.88, 25, 10.2, 1.15), (1.0, 32, 9.4, 1.05)]
    coverts = [(t, out - 4, length * 0.5, width * 1.35) for t, out, length, width in primaries[1:-1:2]]
    top = np.full(base.X.shape, -1)
    shapes = []
    spar, glow = Art(W, H, TEXEL), Art(W, H, TEXEL)
    for i, (t, out, length, width) in enumerate(primaries + coverts):
        p, _ = along(arm, t)
        art = base if i < len(primaries) else spar
        mask, u, nn = flame(art, (p[0], p[1] - 0.3), math.radians(90 - out), length, width, curl=(0.35 if i % 2 else -0.3) if i < len(primaries) else 0.0)
        gray = (150 + 95 * np.clip(u, 0, 1)) * np.where(nn < 0, 1.0, 0.82)
        if i >= len(primaries):
            gray = gray * 0.9 + 18
        art.put(mask, gray)
        layer = top if art is base else None
        if layer is not None:
            layer[mask > 0] = i
        shapes.append((mask, u, nn, art))
    for i, (mask, u, nn, art) in enumerate(shapes):
        mine = ((top == i) if art is base else np.ones(mask.shape, bool)) & (mask > 0)
        out = core if art is base else glow
        heat = mine & (np.abs(nn) < 0.55 * np.clip((u - 0.35) / 0.5, 0, 1))
        out.put(heat.astype(float), 230 + 25 * np.clip(u, 0, 1))
        rim = mine & (nn < -0.8) & (u > 0.25)
        out.put(rim.astype(float), 255, 0.85)
    m, d = spar.stroke(arm, 0.7, 0.3)
    spar.put(m, 120 + 70 * (1 - d) ** 2)
    m, _ = glow.stroke(bezier([(0.6, 5.4), (3.7, 2.1), (8.0, 0.2), (11.4, 0.5)]), 0.12, 0.06)
    glow.put(m, 255, 0.9)
    it = Item("ember_wings", "Ember Wings", "wings", ["wings", "fire", "animated"], [("Flame", "#ff5a24"), ("Core", "#ffd45e")], texel=TEXEL)
    return wing_pair(it, [(base, "Flame"), (core, "Core"), (spar, "Flame", "arm"), (glow, "Core", "arm")], ROOT)


def spirit():
    base, edge = Art(W, H, TEXEL), Art(W, H, TEXEL)
    arm = bezier(ARM)
    wisps = [(0.06, 1.0, 6.4, 1.5), (0.26, 2.2, 7.6, 1.7), (0.46, 3.4, 8.6, 1.8), (0.66, 4.4, 9.4, 1.8), (0.86, 4.8, 9.6, 1.6), (1.0, 3.4, 8.2, 1.3)]
    top = np.full(base.X.shape, -1)
    shapes = []
    for i, (t, dx, length, r) in enumerate(wisps):
        p, _ = along(arm, t)
        path = bezier([p, (p[0] + 0.1, p[1] + length * 0.55), (p[0] + dx * 0.55, p[1] + length * 1.0), (p[0] + dx, p[1] + length * 0.82)], 30)
        mask, d = base.stroke(path, r, 0.1)
        k = np.clip((base.Y - p[1]) / length, 0, 1)
        base.put(mask, 225 + 30 * (1 - d) - 50 * k, 0.8)
        top[mask > 0] = i
        shapes.append((mask, d))
    for i, (mask, d) in enumerate(shapes):
        mine = (top == i) & (mask > 0)
        edge.put((mine & (d > 0.74)).astype(float), 255)
        edge.put((mine & (d < 0.12)).astype(float), 255, 0.55)
    spar, rim = Art(W, H, TEXEL), Art(W, H, TEXEL)
    m, d = spar.stroke(arm, 0.7, 0.3)
    spar.put(m, 240 + 15 * (1 - d))
    m, _ = rim.stroke(arm, 0.24, 0.1)
    rim.put(m, 255)
    it = Item("spirit_wings", "Spirit Wings", "wings", ["wings", "ghost", "animated"], [("Spirit", "#8fe6ff"), ("Edge", "#ffffff")], texel=TEXEL)
    return wing_pair(it, [(base, "Spirit"), (edge, "Edge"), (spar, "Spirit", "arm"), (rim, "Edge", "arm")], ROOT, flap=15.0, speed=0.4, follow=1.45)


def blade(art, top, angle, length, width):
    s, n = art.local(top, angle)
    u = s / length
    half = np.where(u < 0.78, width, width * np.clip((1 - u) / 0.22, 0, 1))
    lean = np.where(u < 0.78, 0, (u - 0.78) / 0.22 * width)
    mask = ((u > -0.04) & (u < 1) & (n < half - lean * 0) & (n > -half + lean * 2)).astype(float)
    nn = n / width
    return mask, u, nn


def clockwork():
    steel, brass, glow = Art(W, H, TEXEL), Art(W, H, TEXEL), Art(W, H, TEXEL)
    arm = bezier(ARM)
    blades = [(0.1, 3, 5.0, 0.8), (0.28, 7, 6.4, 0.85), (0.46, 11, 7.8, 0.9), (0.64, 16, 9.0, 0.9), (0.82, 22, 9.8, 0.88), (1.0, 29, 9.4, 0.82)]
    for t, out, length, width in blades:
        p, _ = along(arm, t)
        mask, u, nn = blade(steel, (p[0], p[1] - 0.2), math.radians(90 - out), length, width)
        gray = np.where(nn < -0.62, 245, np.where(nn > 0.62, 105, 175)) - 25 * (1 - np.clip(u / 0.25, 0, 1))
        steel.put(mask, gray)
        glow.put(mask * 0, 0)
        slot = mask * (np.abs(nn) < 0.14) * (u > 0.22) * (u < 0.8)
        glow.put(slot, 255)
    m, d = brass.stroke(arm, 0.8, 0.42)
    s_along = np.hypot(brass.X - ARM[0][0], brass.Y - ARM[0][1])
    band = (np.abs(np.sin(s_along * 2.2)) > 0.93).astype(float)
    brass.put(m, (150 + 95 * (1 - d) ** 1.5) * (1 - 0.35 * band))
    for t in (0.1, 0.46, 0.82, 1.0):
        p, _ = along(arm, t)
        m, d = brass.disc(p, 0.38)
        brass.put(m, 255 - 90 * d)
    gear = Art(3.6, 3.6, TEXEL)
    r = np.hypot(gear.X - 1.8, gear.Y - 1.8)
    a = np.arctan2(gear.Y - 1.8, gear.X - 1.8)
    teeth = 1.48 + 0.3 * (np.cos(a * 10) > 0.2)
    gm = ((r < teeth) & (r > 0.42)).astype(float)
    spokes = (np.abs(np.sin(a * 3)) < 0.32) | (r < 0.82) | (r > 1.12)
    gear.put(gm * spokes, 200 + 45 * (r < 0.82) - 50 * (r > 1.48))
    it = Item("clockwork_wings", "Clockwork Wings", "wings", ["wings", "mechanical", "animated"],
              [("Steel", "#9aa3b5"), ("Brass", "#d9a64e"), ("Glow", "#5fe0ff")], texel=TEXEL)
    wing_pair(it, [(steel, "Steel"), (glow, "Glow"), (brass, "Brass", "arm")], ROOT, flap=10.0, speed=0.55, follow=1.2)
    uv = it.place(gear.pixels())
    for side, label in ((1, "r"), (-1, "l")):
        px, py, _ = it.wing_pivot
        b = it.bone(f"gear_{label}", (side * px, py, -2.95), anim={"type": "spin", "axis": "z", "amplitude": 0, "speed": side * 0.08, "phase": 0})
        it.card(b, (side * px - 1.8, py - 1.8, -2.97), (3.6, 3.6, 0.04), uv, "Brass", mirror=side < 0)
    return it


def leaf_shape(art, top, angle, length, width):
    s, n = art.local(top, angle)
    u = s / length
    half = width * np.clip(np.sin(np.clip(u, 0, 1) * math.pi), 0, 1) ** 0.8 * (1 - 0.25 * np.clip(u - 0.6, 0, 1))
    mask = ((u > 0) & (u < 1) & (np.abs(n) < half)).astype(float)
    return mask, u, n / np.maximum(half, 1e-6)


def leaf():
    leaves, vine, bloom = Art(W, H, TEXEL), Art(W, H, TEXEL), Art(W, H, TEXEL)
    arm = bezier(ARM)
    spec = [(0.06, 4, 4.2, 1.2), (0.2, 9, 5.6, 1.4), (0.34, 14, 7.0, 1.6), (0.5, 18, 8.2, 1.7), (0.66, 23, 9.0, 1.75), (0.82, 29, 9.4, 1.7), (0.97, 37, 8.6, 1.55),
            (0.14, -10, 3.4, 1.0), (0.42, 2, 4.2, 1.15), (0.74, 12, 4.6, 1.2)]
    for t, out, length, width in spec:
        p, _ = along(arm, t)
        mask, u, nn = leaf_shape(leaves, p, math.radians(90 - out), length, width)
        vein = (np.abs(nn) < 0.07) | ((np.abs(np.sin((u * 7 - np.abs(nn) * 1.6) * math.pi)) < 0.12) & (u > 0.12) & (u < 0.9))
        gray = np.where(nn < 0, 225, 170) - 55 * vein - 25 * (np.abs(nn) > 0.85)
        leaves.put(mask, gray)
    m, d = vine.stroke(arm, 0.55, 0.22)
    vine.put(m, 140 + 90 * (1 - d) ** 2)
    tendril = bezier([(11.4, 0.9), (13.2, 0.4), (13.8, 1.8), (12.8, 2.0)], 16)
    m, d = vine.stroke(tendril, 0.2, 0.08)
    vine.put(m, 180)
    for t, r in ((0.27, 0.95), (0.58, 1.1), (0.9, 0.85)):
        c, _ = along(arm, t)
        a = np.arctan2(bloom.Y - c[1], bloom.X - c[0])
        dd = np.hypot(bloom.X - c[0], bloom.Y - c[1])
        petal = dd < r * (0.6 + 0.4 * np.abs(np.cos(a * 2.5)))
        bloom.put(petal.astype(float), 235 - 60 * np.clip(dd / r, 0, 1) ** 2)
        bloom.put((dd < r * 0.28).astype(float), 255)
    it = Item("leaf_wings", "Leaf Wings", "wings", ["wings", "nature", "animated"],
              [("Leaf", "#6fcf5b"), ("Vine", "#7a5c3a"), ("Bloom", "#ffb6d5")], texel=TEXEL)
    return wing_pair(it, [(leaves, "Leaf"), (vine, "Vine", "arm"), (bloom, "Bloom", "arm")], ROOT, flap=12.0, speed=0.45, follow=1.4)


def neon():
    fill, line, inner = Art(W, H, TEXEL), Art(W, H, TEXEL), Art(W, H, TEXEL)
    lead = bezier([(0.3, 6.0), (3.6, 2.2), (9.0, 0.4), (14.6, 1.6)], 30)
    tips = [(14.6, 1.6), (14.2, 5.2), (12.8, 8.4), (10.6, 10.9), (7.8, 12.4), (5.0, 12.4), (2.9, 10.8), (1.1, 8.0)]
    trail = []
    for a, b in zip(tips, tips[1:]):
        mid = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
        cx, cy = mid[0] - (mid[0] - 3) * 0.12, mid[1] - (mid[1] - 3) * 0.12
        trail += bezier([a, (cx, cy), b], 10)[1:]
    outline = lead + trail
    fill.put(fill.inside(outline), 70, 0.62)
    d = fill.dist(outline, closed=True)
    line.put((d < 0.2).astype(float), 255)
    line.put(((d >= 0.2) & (d < 0.42)).astype(float), 255, 0.4)
    for tip in tips[1:-1]:
        path = bezier([(1.2, 6.4), ((1.2 + tip[0]) / 2, (6.4 + tip[1]) / 2 - 1.2), tip], 20)
        dd = inner.dist(path)
        inner.put((dd < 0.1).astype(float), 255)
        inner.put(((dd >= 0.1) & (dd < 0.24)).astype(float), 255, 0.38)
    it = Item("neon_wings", "Neon Wings", "wings", ["wings", "neon", "animated"],
              [("Fill", "#1d1238"), ("Line", "#ff47d7"), ("Inner", "#45e9ff")], texel=TEXEL)
    return wing_pair(it, [(line, "Line"), (inner, "Inner"), (fill, "Fill"), (inner, "Inner"), (line, "Line")], ROOT, flap=12.0, speed=0.5, gap=0.06)


def abyss():
    blades, glow, frame = Art(W, H, TEXEL), Art(W, H, TEXEL), Art(W, H, TEXEL)
    arm = bezier([(0.2, 5.6), (3.8, 1.8), (8.4, 0.5), (12.6, 1.6)])
    spec = [(0.1, 2, 5.6, 1.15), (0.27, 6, 7.4, 1.3), (0.44, 10, 8.8, 1.4), (0.61, 15, 9.8, 1.45), (0.78, 21, 10.4, 1.4), (0.95, 28, 9.6, 1.25)]
    top = np.full(blades.X.shape, -1)
    shapes = []
    for i, (t, out, length, width) in enumerate(spec):
        p, _ = along(arm, t)
        mask, u, nn = facet_shard(blades, (p[0], p[1] - 0.2), math.radians(90 - out), length, width, lit=215, mid=165, dark=95, base_dark=0.45)
        top[mask > 0] = i
        shapes.append((mask, u, nn))
    for i, (mask, u, nn) in enumerate(shapes):
        mine = (top == i) & (mask > 0)
        glow.put((mine & (np.abs(nn) < 0.07) & (u > 0.18) & (u < 0.9)).astype(float), 255)
        glow.put((mine & (nn < -0.88) & (u > 0.35)).astype(float), 255, 0.9)
        glow.put((mine & (u > 0.86)).astype(float), 255, 0.7)
    gems = Art(W, H, TEXEL)
    m, d = frame.stroke(arm, 0.95, 0.38)
    frame.put(m, 110 + 120 * (1 - d) ** 2)
    for t, length in ((0.18, 1.6), (0.5, 2.0), (0.82, 1.7)):
        p, ang = along(arm, t)
        spike = bezier([p, (p[0] + 0.4 * math.cos(ang - 1.2) * length, p[1] + math.sin(ang - 1.2) * length)], 6)
        m, d = frame.stroke(spike, 0.42, 0.06)
        frame.put(m, 150 + 90 * (1 - d))
    tip = bezier([(12.4, 1.5), (13.8, 1.0), (14.6, 0.2)], 8)
    m, d = frame.stroke(tip, 0.36, 0.05)
    frame.put(m, 160 + 80 * (1 - d))
    for t in (0.1, 0.44, 0.78):
        p, _ = along(arm, t)
        m, d = gems.disc(p, 0.42)
        gems.put(m, 255 - 60 * d)
    it = Item("abyss_crystal_wings", "Abyss Crystal Wings", "wings", ["wings", "crystal", "dark", "animated"],
              [("Main", "#8f6bff"), ("Accent", "#e9e1ff"), ("Frame", "#3a2758")], texel=TEXEL)
    return wing_pair(it, [(blades, "Main"), (glow, "Accent"), (frame, "Frame", "arm"), (gems, "Accent", "arm")], (0.4, 5.6), flap=12.0, speed=0.45)


def plume(art, top, angle, length, width, gray=228, shade=0.8, rim=150):
    """Plump feather with a round tip."""
    s, n = art.local(top, angle)
    r = width
    core = np.clip(s, 0, length - r)
    d = np.hypot(s - core, n)
    grow = np.clip((s + 0.3) / (length * 0.35), 0, 1) ** 0.5
    rad = r * np.where(s < length - r, 0.55 + 0.45 * grow, 1.0)
    mask = ((s > -0.3) & (d < rad)).astype(float)
    k = np.clip(d / np.maximum(rad, 1e-6), 0, 1)
    g = gray * np.where(n < 0, 1.0, shade) * (0.86 + 0.14 * np.clip(s / length, 0, 1))
    g = np.where(k > 0.82, rim + (g - rim) * 0.35, g)
    g = np.where((np.abs(n) < 0.06) & (s > 0.4) & (s < length - r * 0.6), g - 14, g)
    art.put(mask, g)
    return mask


def cherub():
    w, h, root = 10.0, 9.4, (0.4, 4.4)
    primaries, coverts, gold = Art(w, h, 8), Art(w, h, 8), Art(w, h, 8)
    arm = bezier([(0.2, 4.4), (2.4, 1.7), (5.6, 0.6), (8.4, 1.3)])
    long = ((1.0, 42, 4.4), (0.86, 31, 5.6), (0.7, 21, 6.3), (0.53, 13, 6.2), (0.36, 7, 5.5), (0.19, 2, 4.6), (0.04, -3, 3.6))
    for t, out, length in long:
        p, _ = along(arm, t)
        plume(primaries, (p[0], p[1] - 0.2), math.radians(90 - out), length, 1.32, gray=252, shade=0.9, rim=190)
    for (t0, o0, l0), (t1, o1, l1) in zip(long, long[1:]):
        p, _ = along(arm, (t0 + t1) / 2)
        plume(primaries, (p[0], p[1] - 0.1), math.radians(90 - (o0 + o1) / 2), (l0 + l1) / 2 * 0.72, 1.3, gray=255, shade=0.92, rim=200)
    for t, out, length in ((0.94, 30, 3.3), (0.76, 20, 3.6), (0.58, 13, 3.7), (0.4, 8, 3.4), (0.22, 3, 3.0), (0.06, 0, 2.5)):
        p, _ = along(arm, t)
        plume(coverts, (p[0], p[1] + 0.2), math.radians(90 - out), length, 1.3, gray=255, shade=0.93, rim=205)
    m, d = coverts.stroke(arm, 1.15, 0.75)
    coverts.put(m, 250 - 40 * d ** 2)
    for t in (0.18, 0.42, 0.66, 0.9):
        p, _ = along(arm, t)
        m, dd = coverts.disc((p[0], p[1] + 0.35), 0.8)
        coverts.put(m, 246 - 30 * dd ** 2)
    m, _ = gold.stroke(bezier([(0.5, 3.4), (2.5, 0.9), (5.6, -0.2), (8.6, 0.5)]), 0.18, 0.1)
    gold.put(m, 255)
    it = Item("baby_cherub_wings", "Baby Cherub Wings", "wings", ["wings", "white", "feathers", "cute", "animated"],
              [("Feather", "#ffffff"), ("Gold", "#ffd77a")], texel=8)
    return wing_pair(it, [(primaries, "Feather"), (coverts, "Feather", "arm"), (gold, "Gold", "arm")], root,
                     pivot=(2.0, 22.4, -2.6), sweep=18.0, lift=6.0, flap=16.0, speed=1.05, follow=1.35)


def scallops(points, sag):
    out = [points[0]]
    for a, b in zip(points, points[1:]):
        mid = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
        out += bezier([a, (mid[0] + sag[0], mid[1] + sag[1]), b], 10)[1:]
    return out


def baby_dragon():
    w, h, root = 11.0, 9.8, (0.4, 4.8)
    membrane, rim, bones = Art(w, h, 8), Art(w, h, 8), Art(w, h, 8)
    shoulder, elbow, wrist = (0.3, 4.8), (3.4, 2.1), (6.2, 0.9)
    tips = [(10.6, 1.8), (10.0, 5.7), (7.6, 8.6), (4.2, 9.2), (1.2, 7.8)]
    edge = []
    prev = wrist
    for tip in tips:
        mid = ((prev[0] + tip[0]) / 2, (prev[1] + tip[1]) / 2)
        pull = (wrist[0] - mid[0], wrist[1] - mid[1])
        sag = 0.0 if prev is wrist else 0.24
        edge += bezier([prev, (mid[0] + pull[0] * sag, mid[1] + pull[1] * sag), tip], 12)[1:]
        prev = tip
    outline = [shoulder, elbow, wrist] + edge
    inside = membrane.inside(outline)
    vein = np.zeros(inside.shape)
    for tip in tips[:3]:
        vein = np.maximum(vein, (membrane.dist([wrist, tip]) < 0.2).astype(float))
    d = membrane.dist(outline, closed=True)
    membrane.put(inside, 205 - 40 * np.clip(d / 2.5, 0, 1) - 60 * vein + 10 * np.sin(membrane.X * 1.6))
    rim.put(inside * ((d < 0.22) | ((vein > 0) & (membrane.dist([wrist, tips[0]]) < 0.08))).astype(float), 255)
    for tip, r1 in ((tips[0], 0.18), (tips[1], 0.17), (tips[2], 0.16)):
        m, dd = membrane.stroke([wrist, tip], 0.34, r1)
        membrane.put(m, 120 + 70 * (1 - dd))
    m, dd = bones.stroke([shoulder, elbow, wrist], 0.72, 0.5)
    bones.put(m, 140 + 100 * (1 - dd) ** 2)
    for c, r in ((elbow, 0.62), (wrist, 0.58)):
        m, dd = bones.disc(c, r)
        bones.put(m, 230 - 70 * dd)
    claw = bezier([(wrist[0] + 0.1, wrist[1] - 0.2), (wrist[0] + 0.5, wrist[1] - 1.3), (wrist[0] - 0.4, wrist[1] - 1.7)], 10)
    m, dd = bones.stroke(claw, 0.32, 0.06)
    bones.put(m, 250 - 60 * dd)
    it = Item("baby_dragon_wings", "Baby Dragon Wings", "wings", ["wings", "black", "dragon", "cute", "animated"],
              [("Membrane", "#3b3546"), ("Bone", "#5d5570"), ("Accent", "#a07cff")], texel=8)
    return wing_pair(it, [(membrane, "Membrane"), (rim, "Accent"), (bones, "Bone", "arm")], root,
                     pivot=(2.0, 22.2, -2.6), sweep=20.0, lift=8.0, flap=17.0, speed=0.9, follow=1.3)


def web():
    w, h, root = 15.0, 13.4, (0.4, 5.2)
    threads, dew, fill = Art(w, h, 8), Art(w, h, 8), Art(w, h, 8)
    hub = (0.7, 5.2)
    tips = [(13.6, 0.6), (14.4, 4.4), (13.0, 8.0), (10.4, 10.8), (7.0, 12.6), (3.6, 12.6), (1.2, 10.4)]
    outline = [hub] + scallops(tips, (0.0, 0.0))
    ring_masks = np.zeros(threads.X.shape, bool)
    rings = (0.26, 0.44, 0.62, 0.8, 1.0)
    for f in rings:
        pts = [(hub[0] + (t[0] - hub[0]) * f, hub[1] + (t[1] - hub[1]) * f) for t in tips]
        line = []
        for a, b in zip(pts, pts[1:]):
            mid = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
            sag = 0.16 + 0.06 * f
            line += bezier([a, (mid[0] + (hub[0] - mid[0]) * sag, mid[1] + (hub[1] - mid[1]) * sag), b], 12)[1:]
        if f == 1.0:
            outline = [hub, pts[0]] + line
        ring_masks |= threads.dist([pts[0]] + line) < (0.15 if f < 1 else 0.2)
    spokes = np.zeros(threads.X.shape, bool)
    for t in tips:
        spokes |= threads.dist([hub, t]) < 0.16
    spokes |= threads.dist([hub, tips[0]]) < 0.24
    fill.put(fill.inside(outline), 60, 0.42)
    threads.put((ring_masks | spokes).astype(float), 238)
    m, dd = threads.disc(hub, 0.55)
    threads.put(m, 250 - 50 * dd)
    for t, f in ((1, 0.62), (3, 0.8), (4, 0.44), (2, 1.0), (5, 0.62)):
        c = (hub[0] + (tips[t][0] - hub[0]) * f, hub[1] + (tips[t][1] - hub[1]) * f + 0.15)
        m, dd = dew.disc(c, 0.3)
        dew.put(m, 255 - 70 * dd)
    it = Item("web_wings", "Web Wings", "wings", ["wings", "halloween", "spooky", "animated"],
              [("Web", "#ececf4"), ("Dew", "#ff8a2a"), ("Shadow", "#251a33"), ("Spider", "#1e1a24")], texel=8)
    wing_pair(it, [(threads, "Web"), (dew, "Dew"), (fill, "Shadow"), (threads, "Web"), (dew, "Dew")], root, flap=11.0, speed=0.45, gap=0.06)
    spider = Art(3.0, 5.4, 8)
    cx, cy = 1.5, 4.2
    m, _ = spider.stroke([(cx, 0.0), (cx, cy - 0.5)], 0.06, 0.06)
    spider.put(m, 230)
    for side in (-1, 1):
        for k, (ax, ay) in enumerate(((1.25, -0.9), (1.4, -0.25), (1.35, 0.35), (1.1, 0.95))):
            knee = (cx + side * ax * 0.6, cy + ay - 0.35)
            foot = (cx + side * ax, cy + ay + 0.25)
            m, _ = spider.stroke([(cx, cy), knee, foot], 0.13, 0.07)
            spider.put(m, 70)
    m, dd = spider.disc((cx, cy + 0.15), 0.62)
    spider.put(m, 95 + 60 * (1 - dd))
    m, dd = spider.disc((cx, cy - 0.55), 0.36)
    spider.put(m, 80 + 50 * (1 - dd))
    uv = it.place(spider.pixels())
    for side, label in ((1, "r"), (-1, "l")):
        px, py, pz = it.wing_pivot
        sweep = it.bones[0]["rotation"][1]
        b = it.bone(f"spider_{label}", (side * px, py, pz), (0, side * sweep, side * 4.0),
                    anim={"type": "bob", "axis": "y", "amplitude": 0.45, "speed": 0.35, "phase": 0.0 if side > 0 else 2.1})
        x0 = px + 4.6 - 0.4 - 1.5
        it.card(b, (x0 if side > 0 else -x0 - 3.0, py + 5.2 - 9.1 - 5.4, pz - 0.35), (3.0, 5.4, 0.04), uv, "Spider", mirror=side < 0)
    return it


def build():
    return [crystal(), ember(), spirit(), clockwork(), leaf(), neon(), abyss(), cherub(), baby_dragon(), web()]
