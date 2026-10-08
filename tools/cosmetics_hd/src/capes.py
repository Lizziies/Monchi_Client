import math
import random

from lib import Canvas, Item, clamp, hexrgb, lerp, paint, rect_painter, smooth


def heart(u, v, cx, cy, r):
    x = (u - cx) / r
    y = (cy - v) / r * 1.05
    return (x * x + y * y - 1) ** 3 - x * x * y ** 3 <= 0


def drape(u, v, wu, hu, base=190.0, amp=24.0):
    fold = math.sin(u * wu * 0.95 + 0.7 * math.sin(v * hu * 0.38 + u * 3.0))
    return base + amp * fold - 22 * v * v


def sides(u, wu):
    return min(u, 1 - u) * wu


def scallop(u, n, amp):
    return 1 - amp * (1 - abs(math.sin(n * math.pi * u)))


def make_art(wu, hu, texel, shape):
    art = Canvas(round(wu * texel), round(hu * texel))
    paint(art, 0, 0, art.w, art.h, shape, ss=3)
    return art


def cape(item_id, name, tints, art, strips, width, height, tint_of, overlay=None, tags=None, texel=6, sway=0.0, top=24.0, z=-2.55):
    it = Item(item_id, name, "cape", ["cape"] + (tags or []), tints, texel=texel, size=256)
    rows = round(height * texel) // strips
    sh = height / strips
    base = make_art(width, height, texel, art)
    layers = [("cape", base, None)]
    if overlay:
        layers.append(("overlay", make_art(width, height, texel, overlay[0]), overlay[1]))
    for lname, image, anim in layers:
        bone = it.bone(lname, pivot=(0, top, z), rot=(5, 0, 0), cloth=True, anim=anim)
        depth = z - (0.06 if lname == "overlay" else 0.0)
        for i in range(strips):
            tint, tint2, mix = tint_of(i, strips) if lname == "cape" else (overlay[2], None, 0.0)
            bone.flat((-width / 2, top - (i + 1) * sh, depth), (width, sh, 0.1), rect_painter(image, 0, i * rows), tint=tint, tint2=tint2, mix=mix)
    return it


def mochi_art(wu, hu, texel):
    n = 5
    ww, hh = wu * texel, hu * texel

    def shape(u, v):
        if v > scallop(u, n, 0.035):
            return None
        g = drape(u, v, wu, hu, 232, 20)
        side = sides(u, ww)
        if side < 1.4:
            g = 120
        elif side < 4.0 and int(v * hh) % 5 < 3 and 2.4 < side < 3.4:
            g = 236
        if v < 0.1:
            g = 215 + 12 * math.sin(u * wu * 2.2)
        edge = (1 - v) * hh - (scallop(u, n, 0.035) - v) * hh
        if v > 0.9 and edge < 1.4:
            g = 120
        if heart(u, v, 0.5, 0.48, 0.2):
            g = 250
            if not heart(u, v, 0.5, 0.48, 0.17):
                g = 215
        return (clamp(g, 0, 255), 1.0)

    return shape


def gradient_art(wu, hu, texel):
    ww, hh = wu * texel, hu * texel

    def shape(u, v):
        if v > 1 - 0.012 * (1 - abs(math.sin(7 * math.pi * u))):
            return None
        g = drape(u, v, wu, hu, 232, 22) + 10 * math.sin((u + v * 0.6) * 9)
        side = sides(u, ww)
        if side < 1.3:
            g = 140
        elif 2.2 < side < 3.0:
            g = 245
        if v > 0.965:
            g = 150
        return (clamp(g, 0, 255), 1.0)

    return shape


def starry_art(wu, hu, texel):
    ww, hh = wu * texel, hu * texel

    def shape(u, v):
        g = 175 + 34 * math.sin(u * wu * 0.8 + 0.8 * math.sin(v * hu * 0.4 + u * 2)) - 30 * v * v
        side = sides(u, ww)
        if side < 1.3:
            g = 215
        elif v < 0.05:
            g = 225
        elif v > 0.97:
            g = 215
        return (clamp(g, 0, 255), 1.0)

    return shape


def stars_art(wu, hu, texel, seed=5):
    rng = random.Random(seed)
    ww, hh = round(wu * texel), round(hu * texel)
    pts = [(rng.random(), 0.06 + rng.random() * 0.88, rng.random()) for _ in range(34)]

    def shape(u, v):
        best = 0.0
        for (sx, sy, big) in pts:
            dx = abs(u - sx) * ww
            dy = abs(v - sy) * hh
            r = 2.6 if big > 0.7 else 1.5
            cross = max(0.0, 1 - (dx / r + dy / (r * 0.28)) if dx > dy else 1 - (dy / r + dx / (r * 0.28)))
            core = max(0.0, 1 - (dx * dx + dy * dy) ** 0.5 / (r * 0.55))
            best = max(best, cross, core)
        if best <= 0.05:
            return None
        return (255, clamp(best * 1.3))

    return shape


def flame_art(wu, hu, texel):
    ww, hh = wu * texel, hu * texel
    rng = random.Random(11)
    tongues = [(rng.random(), 0.05 + rng.random() * 0.06) for _ in range(9)]

    def edge(u):
        d = 0.0
        for c, h in tongues:
            d = max(d, h * max(0.0, 1 - abs(u - c) * 9))
        return 1 - 0.13 + d

    def shape(u, v):
        e = edge(u)
        if v > e + 0.02 * math.sin(u * 40):
            return None
        g = 170 + 60 * smooth(0.5, 1.0, v) + 20 * math.sin(u * wu * 1.3 + v * 7) * (0.4 + v)
        side = sides(u, ww)
        if side < 1.2:
            g *= 0.75
        if v > e - 0.04:
            g = 255
        return (clamp(g, 0, 255), 1.0)

    return shape


def royal_art(wu, hu, texel):
    ww, hh = wu * texel, hu * texel
    rng = random.Random(3)
    spots = [(rng.random(), rng.random() * 0.16) for _ in range(14)] + [(rng.random(), 0.93 + rng.random() * 0.06) for _ in range(9)]

    def shape(u, v):
        if v > 1 - 0.012 * (1 - abs(math.sin(9 * math.pi * u))):
            return None
        fur = v < 0.17 or v > 0.9
        if fur:
            g = 240 - 14 * math.sin(u * 40 + v * 30)
            for (sx, sy) in spots:
                dx, dy = (u - sx) * ww, (v - sy) * hh
                if abs(dx) + abs(dy) * 0.8 < 2.6 and (v < 0.17) == (sy < 0.5):
                    g = 40
            return (g, 1.0)
        g = drape(u, v, wu, hu, 215, 24)
        side = sides(u, ww)
        if side < 1.4:
            g = 110
        return (clamp(g, 0, 255), 1.0)

    return shape


def royal_gold_art(wu, hu, texel):
    ww, hh = wu * texel, hu * texel

    def shape(u, v):
        if 0.17 < v < 0.9:
            side = sides(u, ww)
            if 2.0 < side < 4.2:
                return (255 - 30 * (abs(side - 3.1)), 1.0)
            if abs(u - 0.5) * ww < 1.2 and 0.45 < v < 0.52:
                return (250, 1.0)
            cx, cy = 0.5, 0.58
            d = (((u - cx) * ww) ** 2 + ((v - cy) * hh * 0.9) ** 2) ** 0.5
            if 9.0 < d < 11.0:
                return (255 - 20 * abs(d - 10), 1.0)
        return None

    return shape


def mini_art(wu, hu, texel):
    ww, hh = wu * texel, hu * texel

    def shape(u, v):
        if v > 1 - 0.28 * abs(2 * u - 1) ** 1.0 * 0.5 - 0.0:
            if v > 1 - 0.14 * abs(2 * u - 1) - 0.0 and False:
                return None
        edge = 1 - 0.2 * (1 - abs(2 * u - 1) ** 1.2)
        if v > edge + 0.0:
            return None
        g = drape(u, v, wu, hu, 230, 18)
        side = sides(u, ww)
        if side < 1.4:
            g = 125
        if v < 0.18:
            g = 232
        return (clamp(g, 0, 255), 1.0)

    return shape


def rose_cloth_art(wu, hu, texel):
    ww, hh = wu * texel, hu * texel
    top, bottom = hexrgb("#5c1530"), hexrgb("#1e0812")
    gold, gold_hi = hexrgb("#c08a3a"), hexrgb("#f3cf7a")
    leaf, leaf_hi, stem = hexrgb("#2f7a3c"), hexrgb("#5cb862"), hexrgb("#2b5e2e")

    def vine_x(v, side):
        return 7.5 + 2.2 * math.sin(v * hh * 0.11 + side * 1.7)

    def stem_x(v):
        return 0.5 * ww - 3.0 * math.sin((v - 0.5) * 5.0) - 2.0 * (v - 0.5)

    def shape(u, v):
        if v > scallop(u, 6, 0.03):
            return None
        px, py = u * ww, v * hh
        side = sides(u, ww)
        if side < 1.0 or (1 - v) * hh < 1.2:
            return (*gold, 1.0)
        if side < 2.4:
            return (*[lerp(a, b, 0.5 + 0.5 * math.sin(py * 0.35)) for a, b in zip(gold, gold_hi)], 1.0)
        if v < 0.07:
            if v > 0.055 or v < 0.012:
                return (*[lerp(a, b, 0.5 + 0.5 * math.sin(px * 0.35)) for a, b in zip(gold, gold_hi)], 1.0)
            return (*hexrgb("#2a0a15"), 1.0)
        fold = 0.5 + 0.5 * math.sin(u * wu * 0.95 + 0.7 * math.sin(v * hu * 0.38 + u * 3.0))
        base = [lerp(a, b, smooth(0.05, 1.0, v)) for a, b in zip(top, bottom)]
        k = 0.78 + 0.32 * clamp(fold, 0, 1) + 0.05 * math.sin(px * 0.9 + py * 0.15)
        col = tuple(c * k for c in base)
        if v > 0.3:
            for sd in (0, 1):
                vx = vine_x(v, sd)
                dx = (px if sd == 0 else ww - px) - vx
                if abs(dx) < 0.7 and v > 0.34 + 0.08 * sd:
                    return (*stem, 1.0)
                if int(py) % 13 == 4 and 0.7 <= dx < 2.6 and abs(py % 13 - 4.5) < 1.5:
                    return (*leaf, 1.0)
                if int(py) % 13 == 10 and -2.6 < dx <= -0.7:
                    return (*leaf_hi, 1.0)
                if int(py) % 9 == 7 and abs(dx - 1.1) < 0.5:
                    return (*hexrgb("#8a2a3a"), 1.0)
        bx, by = 0.5 * ww + 21, 0.6 * hh + 7
        jx, jy = stem_x(0.76), 0.76 * hh
        t = (px - jx) / (bx - jx)
        if 0 < t < 1:
            off = py - (jy + (by - jy) * t + 3.0 * math.sin(t * math.pi))
            if abs(off) < 1.25:
                return (*(leaf_hi if off < 0 else stem), 1.0)
        for gx, gy in ((0.27, 0.22), (0.74, 0.18), (0.22, 0.45), (0.8, 0.4), (0.33, 0.12), (0.66, 0.5)):
            dx, dy = abs(px - gx * ww), abs(py - gy * hh)
            if (dx < 0.6 and dy < 2.1) or (dy < 0.6 and dx < 2.1):
                return (*(gold_hi if dx + dy < 0.9 else gold), 1.0)
        if 0.52 < v < 0.93:
            sx = stem_x(v)
            if abs(px - sx) < 1.1:
                return (*(leaf_hi if px < sx else stem), 1.0)
            for ly, dir in ((0.64, -1), (0.78, 1)):
                lx, lyp = sx + dir * 6.5, ly * hh - 2.5
                a, b = (px - lx) * 0.85 + (py - lyp) * 0.5 * dir, -(px - lx) * 0.5 * dir + (py - lyp) * 0.85
                d = (a / 7.0) ** 2 + (b / 3.3) ** 2
                if d < 1.0:
                    if abs(b) < 0.45 and abs(a) < 5.5:
                        return (*stem, 1.0)
                    return (*(leaf_hi if b < 0 else leaf), 1.0)
            if abs(px - sx - 1.6) < 0.6 and int(py) % 11 == 3:
                return (*hexrgb("#8a2a3a"), 1.0)
        return (*col, 1.0)

    return shape


def rose_bloom_art(wu, hu, texel):
    ww, hh = wu * texel, hu * texel
    petals = [
        (-9.0, -7.0, 10.0, 8.0, -0.5, 0.6),
        (9.0, -7.0, 10.0, 8.0, 0.5, 0.6),
        (0.0, -11.0, 9.0, 6.5, 0.0, 0.52),
        (0.0, -5.0, 7.0, 8.0, 0.0, 0.82),
        (-11.0, 2.0, 7.5, 10.0, 0.45, 0.8),
        (11.0, 2.0, 7.5, 10.0, -0.45, 0.8),
        (0.0, 6.0, 13.5, 8.5, 0.0, 1.0),
        (0.0, 11.5, 9.5, 5.5, 0.0, 0.88),
    ]

    def rose(px, py, cx, cy, scale):
        g = None
        for dx, dy, rx, ry, rot, base in petals:
            x, y = (px - cx) / scale - dx, (py - cy) / scale - dy
            a = x * math.cos(rot) + y * math.sin(rot)
            b = -x * math.sin(rot) + y * math.cos(rot)
            d = (a / rx) ** 2 + (b / ry) ** 2
            if d > 1:
                continue
            t = (b + ry) / (2 * ry)
            k = base * (1.08 - 0.4 * t)
            if d > 0.72:
                k *= 1.22 if b < 0 else 0.62
            g = 255 * k
        if g is not None:
            x, y = (px - cx) / scale, (py - cy) / scale + 5.5
            r = (x * x + y * y) ** 0.5
            ang = math.atan2(y, x)
            if abs(r - 3.6) < 0.7 and -2.6 < ang < 1.2:
                g *= 0.55
            if abs(r - 1.6) < 0.6 and ang > -0.5:
                g *= 0.6
        return g

    fallen = [(0.24, 0.84, 0.6), (0.76, 0.9, -0.5), (0.63, 0.955, 0.2)]

    def shape(u, v):
        px, py = u * ww, v * hh
        g = rose(px, py, 0.5 * ww, 0.35 * hh, 1.25)
        if g is None:
            g = rose(px, py, 0.5 * ww + 21, 0.6 * hh, 0.45)
        if g is None:
            for pu, pv, tilt in fallen:
                dx, dy = px - pu * ww, py - pv * hh
                a, b = dx * math.cos(tilt) + dy * math.sin(tilt), -dx * math.sin(tilt) + dy * math.cos(tilt)
                if (a / 3.0) ** 2 + (b / 1.8) ** 2 < 1:
                    g = 220 if b < 0 else 160
        if g is None:
            return None
        return (clamp(g, 0, 255), 1.0)

    return shape

def build():
    out = []
    w, h, t = 12.0, 19.2, 5

    out.append(
        cape(
            "mochi_cape", "Monchi Cape", [("Main", "#ff7eb6"), ("Trim", "#ffffff")], mochi_art(w, h, t), 16, w, h,
            lambda i, n: ("Trim" if i in (0, n - 1) else "Main", None, 0.0), tags=["pink", "heart"],
        )
    )
    out.append(
        cape(
            "gradient_cape", "Gradient Cape", [("Top", "#7ec8ff"), ("Bottom", "#c77dff")], gradient_art(w, h, t), 16, w, h,
            lambda i, n: ("Top", "Bottom", i / (n - 1)), tags=["gradient"],
        )
    )
    out.append(
        cape(
            "starry_cape", "Starry Cape", [("Top", "#141a4f"), ("Bottom", "#5a2f8f"), ("Stars", "#ffe9a8")], starry_art(w, h, t), 16, w, h,
            lambda i, n: ("Top", "Bottom", i / (n - 1)),
            overlay=(stars_art(w, h, t), {"type": "sparkle", "speed": 0.35, "amplitude": 1, "phase": 0}, "Stars"), tags=["night", "animated"],
        )
    )
    out.append(
        cape(
            "flame_cape", "Flame Cape", [("Base", "#ff4a1c"), ("Tips", "#ffd04a")], flame_art(w, h, t), 16, w, h,
            lambda i, n: ("Base", "Tips", (i / (n - 1)) ** 1.4), tags=["fire", "animated"],
        )
    )
    out.append(
        cape(
            "royal_cape", "Royal Cape", [("Cloth", "#8f1d2c"), ("Fur", "#f6f1e8"), ("Gold", "#ffcf5a")], royal_art(w, h, t), 16, w, h,
            lambda i, n: ("Fur" if i in (0, 1, 2, n - 1) else "Cloth", None, 0.0), overlay=(royal_gold_art(w, h, t), None, "Gold"), tags=["royal"],
        )
    )
    out.append(
        cape(
            "mini_cape", "Mini Cape", [("Main", "#7ee0c8"), ("Trim", "#ffffff")], mini_art(8.0, 8.0, t), 6, 8.0, 8.0,
            lambda i, n: ("Trim" if i == 0 else "Main", None, 0.0), tags=["short"],
        )
    )
    out.append(
        cape(
            "rose_cape", "Rose Cape", [("Rose", "#e3264c")], rose_cloth_art(12.0, 20.0, 8), 16, 12.0, 20.0,
            lambda i, n: (None, None, 0.0), overlay=(rose_bloom_art(12.0, 20.0, 8), None, "Rose"), tags=["rose", "flower"], texel=8,
        )
    )
    return out
