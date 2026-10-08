import math
import random

from lib import Canvas, Item, clamp, lerp, paint, rect_painter, smooth


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
    return out
