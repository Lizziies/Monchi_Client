import colorsys
import math

from lib import Canvas, Item, clamp, curve, feather, inside, lerp, paint, poly_dist, rect_painter, region_painter, seg_dist, smooth


def stroke(a, b, ra, rb, x, y):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    t = clamp(((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy + 1e-12))
    d = math.hypot(x - (ax + dx * t), y - (ay + dy * t))
    r = lerp(ra, rb, t)
    return d, r


def bat_art(wu, hu, arm, fingers, bulge, bone_gray=95, membrane=212, spikes=False):
    """Wing image in art units with the shoulder on the left. arm = [S, E, W], fingers = tips from the leading edge down."""
    s, e, w = arm
    edge = [w]
    prev = w
    for tip in fingers:
        edge += curve([prev, tip], 6, bulge)[1:]
        prev = tip
    body = (0.4, fingers[-1][1] + 0.4)
    edge += curve([prev, body], 6, bulge * 0.8)[1:]
    poly = edge + [s, e]
    strokes = [(s, e, 0.62, 0.5), (e, w, 0.5, 0.36)] + [(w, tip, 0.34, 0.1) for tip in fingers]

    def shape(u, v):
        x, y = u * wu, v * hu
        best = None
        for a, b, ra, rb in strokes:
            d, r = stroke(a, b, ra, rb, x, y)
            if d < r:
                shade = bone_gray + 70 * (d / r) ** 2 - 20 * (1 - d / r)
                best = max(best or 0, 1)
                return (clamp(shade, 0, 255), 1.0)
        if not inside(poly, x, y):
            return None
        d = poly_dist(poly, x, y)
        g = membrane - 22 * smooth(0, 3.5, d) + 8 * math.sin(x * 1.8 + y * 0.7)
        for tip in fingers:
            vd = seg_dist(w, tip, x, y)
            if vd < 0.16:
                g -= 30
        if d < 0.22:
            g = 120
        return (clamp(g, 0, 255), 0.93)

    def glow(u, v):
        x, y = u * wu, v * hu
        if not inside(poly, x, y) and poly_dist(poly, x, y) > 0.1:
            return None
        d = poly_dist(poly, x, y)
        if d < 0.28:
            return (255, 1.0 - d / 0.28)
        for tip in fingers:
            vd = seg_dist(w, tip, x, y)
            if vd < 0.2:
                return (255, 0.8 * (1 - vd / 0.2) * clamp((math.hypot(x - w[0], y - w[1])) / 6))
        return None

    return shape, glow, w[0]


def build_membrane(item_id, name, tags, tints, wu, hu, arm, fingers, bulge, sc=0.85, shoulder=(2.4, 21.0, -3.2), texel=6, glow_tint="Accent", spikes=False, seed=1):
    it = Item(item_id, name, "wings", ["wings", "animated"] + tags, tints, texel=texel, size=256)
    shape, glow, wrist = bat_art(wu, hu, arm, fingers, bulge)
    full = Canvas(round(wu * sc * texel), round(hu * sc * texel))
    paint(full, 0, 0, full.w, full.h, shape, ss=2)
    over = Canvas(full.w, full.h)
    paint(over, 0, 0, over.w, over.h, glow, ss=2)
    sx = arm[0][0]
    split = round(wrist * sc * texel)
    parts = [("inner", 0, split, 14.0, 0.0, 70.0, 6.5), ("outer", split, full.w, 22.0, 0.55, 38.0, 4.2)]
    for side, label in ((1, "r"), (-1, "l")):
        for pname, px0, px1, amp, phase, stiff, damp in parts:
            width = (px1 - px0) / texel
            height = full.h / texel
            sy = arm[0][1] * sc
            bone = it.bone(
                f"{pname}_{label}", pivot=(side * shoulder[0], shoulder[1], shoulder[2]), rot=(0, side * 12, side * 8),
                anim={"type": "flap", "axis": "y", "amplitude": side * amp, "speed": 0.5, "phase": phase},
                physics={"stiffness": stiff, "damping": damp, "inertia": 1.0 if pname == "inner" else 1.6,
                         "drive": {"air": [0, 0, side * 16], "sprint": [0, side * 18, 0], "speed": [0, side * 5, 0]}},
            )
            ox = side * shoulder[0] + (px0 / texel if side > 0 else -px0 / texel - width)
            y1 = shoulder[1] + sy
            bone.flat((ox, y1 - height, shoulder[2]), (width, height, 0.1), rect_painter(full, px0, 0), tint=tints[0][0], mirror=side < 0)
            bone.flat((ox, y1 - height, shoulder[2] - 0.05), (width, height, 0.1), rect_painter(over, px0, 0), tint=glow_tint, mirror=side < 0)
    return it


def demon():
    return build_membrane(
        "demon_wings", "Demon Wings", ["dark", "bat", "glow"], [("Main", "#3a1f4d"), ("Accent", "#ff4a6a")], 22.0, 16.0,
        arm=[(0.4, 13.2), (5.4, 4.6), (12.6, 2.0)],
        fingers=[(22.0, 2.6), (21.0, 8.8), (16.0, 14.0), (9.4, 15.4)], bulge=-0.16,
    )


def dragon():
    return build_membrane(
        "dragon_wings", "Dragon Wings", ["green", "dragon", "scales"], [("Main", "#2f8f55"), ("Accent", "#ffd27e")], 24.0, 17.0,
        arm=[(0.4, 14.0), (6.0, 3.2), (13.8, 1.6)],
        fingers=[(23.5, 1.2), (23.0, 7.0), (19.0, 12.5), (13.0, 16.0), (6.5, 16.4)], bulge=-0.24,
    )


def butterfly_art(wu, hu, kind):
    if kind == "fore":
        outline = [(0.3, 11.0), (0.8, 5.0), (4.0, 1.2), (10.0, 0.2), (15.0, 2.0), (16.6, 6.0), (14.0, 10.0), (8.0, 12.4), (2.0, 12.2)]
        base = (0.3, 11.0)
        spots = [(11.0, 4.2, 1.15), (13.6, 7.6, 0.9), (9.6, 9.6, 0.7)]
    else:
        outline = [(0.3, 0.8), (5.0, 0.3), (9.5, 2.5), (11.5, 6.0), (9.0, 9.6), (4.5, 10.3), (1.2, 7.0)]
        base = (0.3, 0.8)
        spots = [(8.2, 4.6, 0.95), (6.0, 7.4, 0.7)]
    outline = curve(outline + [outline[0]], 5, 0.0)

    def shape(u, v):
        x, y = u * wu, v * hu
        if not inside(outline, x, y):
            return None
        d = poly_dist(outline, x, y)
        ang = math.atan2(y - base[1], x - base[0])
        dist = math.hypot(x - base[0], y - base[1])
        g = 240 - 36 * smooth(0, 6, dist / 2.2) - 14 * smooth(0, 2.5, d)
        if abs(math.sin(ang * 6.0 + 0.3)) > 0.975 and dist > 1.2:
            g -= 70
        if d < 0.3:
            g = 85
        return (clamp(g, 0, 255), 1.0)

    def trim(u, v):
        x, y = u * wu, v * hu
        if not inside(outline, x, y):
            return None
        d = poly_dist(outline, x, y)
        if 0.35 < d < 0.95:
            return (255, 0.95)
        for cx, cy, r in spots:
            sd = math.hypot(x - cx, y - cy)
            if sd < r:
                return (255, 1.0 if sd < r - 0.2 else 0.7)
        return None

    return shape, trim


def butterfly():
    it = Item("butterfly_wings", "Butterfly Wings", "wings", ["wings", "animated", "butterfly"], [("Main", "#ff9fd0"), ("Accent", "#ffffff")], texel=6, size=256)
    parts = [("fore", 17.0, 13.0, 21.5, 11.0, 24.0), ("hind", 12.0, 10.5, 20.0, 0.8, -26.0)]
    for side, label in ((1, "r"), (-1, "l")):
        for kind, wu, hu, py, base_y, roll in parts:
            fill, trim = butterfly_art(wu, hu, kind)
            if ("img", kind) not in it.atlas.cache:
                a = Canvas(round(wu * 6), round(hu * 6))
                paint(a, 0, 0, a.w, a.h, fill, ss=2)
                b = Canvas(a.w, a.h)
                paint(b, 0, 0, b.w, b.h, trim, ss=2)
                it.atlas.cache[("img", kind)] = (a, b)
            a, b = it.atlas.cache[("img", kind)]
            fore = kind == "fore"
            bone = it.bone(
                f"{kind}_{label}", pivot=(side * 2.2, py, -3.1), rot=(0, side * 16, side * roll),
                anim={"type": "flap", "axis": "y", "amplitude": side * (30 if fore else 24), "speed": 1.05, "phase": 0.0 if fore else 0.35},
                physics={"stiffness": 90 if fore else 70, "damping": 6, "inertia": 1.2, "drive": {"air": [0, 0, side * 10], "sprint": [0, side * 22, 0]}},
            )
            ox = side * 2.2 if side > 0 else -2.2 - wu
            y1 = py + base_y
            bone.flat((ox, y1 - hu, -3.1), (wu, hu, 0.1), rect_painter(a, 0, 0), tint="Main", mirror=side < 0)
            bone.flat((ox, y1 - hu, -3.15), (wu, hu, 0.1), rect_painter(b, 0, 0), tint="Accent", mirror=side < 0)
    return it


def petal(length_tex, width_tex, seed):
    def shape(u, v):
        c = v - 0.5
        hw = 0.5 * math.sin(math.pi * clamp(u ** 0.72)) ** 0.85
        if u > 0.9 and abs(c) < (u - 0.9) * 2.4:
            return None
        d = abs(c)
        if d > hw or hw <= 0.01:
            return None
        edge = (hw - d) * width_tex
        g = 232 - 38 * (d / hw) ** 1.6 - 14 * (1 - u)
        g += 8 * math.sin((c / max(hw, 0.05)) * 7 + seed)
        if abs(c) < 0.018:
            g = 250
        if edge < 1.2:
            g = lerp(150, g, clamp(edge / 1.2))
        return (clamp(g, 0, 255), 1.0 if edge > 0.5 else 0.6)

    return shape


def petal_glow(length_tex, width_tex):
    def shape(u, v):
        c = v - 0.5
        hw = 0.5 * math.sin(math.pi * clamp(u ** 0.72)) ** 0.85
        if hw <= 0.01 or abs(c) > hw:
            return None
        edge = (hw - abs(c)) * width_tex
        if u > 0.9 and abs(c) < (u - 0.9) * 2.4:
            return None
        a = smooth(0.55, 1.0, u) * 0.85
        if edge < 1.6:
            a = max(a, 0.55 * (1 - edge / 1.6))
        return (255, a) if a > 0.04 else None

    return shape


def sakura():
    it = Item("sakura_wings", "Sakura Wings", "wings", ["wings", "pink", "petals", "animated"], [("Main", "#ff8fbf"), ("Accent", "#ffe6f0")], texel=5, size=256)
    rows = [("big", 7, (14.0, 9.5), 6.2, (72.0, -6.0), 0.0, -3.0), ("small", 6, (9.0, 5.5), 4.6, (60.0, 8.0), 0.6, -3.25)]
    for side, label in ((1, "r"), (-1, "l")):
        for name, count, (l0, l1), width, (a0, a1), dy, z in rows:
            for i in range(count):
                t = i / (count - 1)
                length = lerp(l0, l1, t)
                angle = lerp(a0, a1, t)
                pivot = (side * 2.4, 21.0 + dy, z - 0.012 * i)
                bone = it.bone(
                    f"{name}_{label}{i}", pivot=pivot, rot=(0, side * (14 + 12 * t), side * angle),
                    anim={"type": "flap", "axis": "y", "amplitude": side * (13 - 5 * t), "speed": 0.62, "phase": 0.2 * i},
                    physics={"stiffness": 60 - 22 * t, "damping": 5.5, "inertia": 1.1 + 0.8 * t,
                             "drive": {"air": [0, 0, side * 14], "sprint": [0, side * 15, 0], "speed": [0, side * 4, 0]}},
                )
                lt, wt = round(length * it.texel), round(width * it.texel)
                ox = pivot[0] if side > 0 else pivot[0] - length
                bone.flat((ox, pivot[1] - width / 2, pivot[2]), (length, width, 0.1), region_painter(petal(lt, wt, i * 1.7)), key=(name, i, "p"), tint="Main", mirror=side < 0)
                bone.flat((ox, pivot[1] - width / 2, pivot[2] - 0.03), (length, width, 0.1), region_painter(petal_glow(lt, wt)), key=(name, i, "g"), tint="Accent", mirror=side < 0)
    return it


PIXEL = [
    "......XXXXX.....",
    "....XXXXXXXXX...",
    "...XXXXXXXXXXXX.",
    "..XXXXXXXXXXXXXX",
    "..XXXXXXXXXXXXX.",
    ".XXXXXXXXXXXXX..",
    ".XXXXXXXXXXXX...",
    "XXXXXXXXXX......",
    "XXXXXXXX........",
    ".XXXXX..........",
    "..XX............",
]


def pixel():
    it = Item("pixel_wings", "Pixel Wings", "wings", ["wings", "pixel", "rainbow", "animated"], [("Main", "#ffffff")], texel=1, size=64)
    h, w = len(PIXEL), len(PIXEL[0])
    art = Canvas(w, h)
    for y in range(h):
        hue = y / (h - 1) * 0.82
        for x in range(w):
            if PIXEL[y][x] != "X":
                continue
            edge = any(not (0 <= x + dx < w and 0 <= y + dy < h) or PIXEL[y + dy][x + dx] != "X" for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            r, g, b = colorsys.hsv_to_rgb(hue, 0.55 if not edge else 0.8, 1.0 if not edge else 0.78)
            art.set(x, y, (r * 255, g * 255, b * 255, 255))
    for side, label in ((1, "r"), (-1, "l")):
        bone = it.bone(
            f"wing_{label}", pivot=(side * 2.2, 22.0, -3.0), rot=(0, side * 22, side * 4),
            anim={"type": "flap", "axis": "y", "amplitude": side * 15, "speed": 0.9, "phase": 0.0},
            physics={"stiffness": 80, "damping": 6, "drive": {"air": [0, 0, side * 14], "sprint": [0, side * 18, 0]}},
        )
        ox = side * 2.2 if side > 0 else -2.2 - w
        bone.flat((ox, 22.0 - 8.0, -3.0), (float(w), float(h), 0.5), rect_painter(art, 0, 0), key="pix", tint="Main", mirror=side < 0)
    return it


def build():
    return [demon(), dragon(), butterfly(), sakura(), pixel()]
