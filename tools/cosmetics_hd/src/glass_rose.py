import math
import random

from lib import Item, clamp, hexrgb, lerp

SILVER, SILVER_HI = hexrgb("#c9d3e0"), hexrgb("#f4f8ff")
BRASS, BRASS_HI, BRASS_LO = hexrgb("#c79a45"), hexrgb("#f2d488"), hexrgb("#8a6428")
LEAF, LEAF_HI, STEM = hexrgb("#3c8f45"), hexrgb("#6cc46f"), hexrgb("#2f6a33")


def painter(face_fn, seed=1):
    rng = random.Random(seed)
    grain = [[rng.random() for _ in range(32)] for _ in range(32)]

    def run(canvas, faces):
        for name, (x, y, w, h) in faces.items():
            for py in range(h):
                for px in range(w):
                    c = face_fn(name, (px + 0.5) / w, (py + 0.5) / h, px, py, w, h)
                    if c is None:
                        continue
                    r, g, b, a = c if len(c) == 4 else (*c, 255)
                    k = 0.96 + 0.08 * grain[py % 32][px % 32] if a == 255 else 1.0
                    canvas.set(x + px, y + py, (clamp(r * k, 0, 255), clamp(g * k, 0, 255), clamp(b * k, 0, 255), a))

    return run


def metal(lo, mid, hi):
    def face(name, u, v, px, py, w, h):
        if name == "top":
            return hi
        if name == "bottom":
            return lo
        t = 0.5 + 0.5 * math.sin((u + v) * 7.0)
        return tuple(lerp(a, b, t * 0.6) for a, b in zip(mid, hi))

    return face


def glass(name, u, v, px, py, w, h):
    if name in ("top", "bottom"):
        return None
    edge = min(u, 1 - u, v, 1 - v)
    if name == "back":
        s = u + v * 0.5
        if (abs(s - 0.26) < 0.03 or abs(s - 0.36) < 0.012) and 0.04 < v < 0.5:
            return (255, 255, 255, 200)
    if edge < 0.04:
        return (235, 248, 255, 150)
    return (215, 240, 255, 80 + int(30 * (1 - v)))


def water(name, u, v, px, py, w, h):
    if name == "top":
        ring = 0.5 + 0.5 * math.sin(u * 23 + v * 7)
        return (150, 215, 250, 150 + int(40 * ring))
    if name in ("front", "back", "left", "right"):
        for bu, bv in ((0.22, 0.7), (0.3, 0.42), (0.71, 0.6), (0.78, 0.25), (0.55, 0.8)):
            if ((u - bu) * w) ** 2 + ((v - bv) * h) ** 2 < 0.8:
                return (240, 252, 255, 220)
        if v < 0.1:
            return (170, 225, 255, 175)
    return (70, 155, 220, 135)


def bloom(name, u, v, px, py, w, h):
    if name == "top":
        x, y = u - 0.5, v - 0.5
        r = (x * x + y * y) ** 0.5 * 2
        a = math.atan2(y, x)
        s = (2.6 * r + a / (2 * math.pi)) % 1.0
        g = 255 - 70 * r
        if s < 0.18:
            g *= 0.62
        return (g, g, g)
    if name == "bottom":
        return (120, 120, 120)
    lobes = 0.5 + 0.5 * math.cos(u * math.pi * 4)
    g = 235 - 80 * v
    if v < 0.18 + 0.12 * lobes:
        g = 255
    if abs(((u * 4) % 1) - 0.5) > 0.44:
        g *= 0.7
    return (g, g, g)


ROSE = [
    "....DMMMMD....",
    "..DMMHHHHMMD..",
    ".DMHHMMMMHHMD.",
    ".MHMDDDDDDMHM.",
    "DMHDXMMMMXDHMD",
    "DHMDMHHHMDXMHD",
    "DHMDMHXDHMXMHD",
    "DMHDXMDDMXDHMD",
    ".DMHDXXXXDHMD.",
    ".DMMHDDDDHMMD.",
    "..DMMHHHHMMD..",
    "...DDMMMMDD...",
    ".....DDDD.....",
]
SHADES = {"H": 255, "M": 205, "D": 150, "X": 100}


def rose_face(name, u, v, px, py, w, h):
    if name not in ("front", "back"):
        return None
    c = ROSE[min(len(ROSE) - 1, py * len(ROSE) // h)][min(13, px * 14 // w)]
    if c == ".":
        return None
    g = SHADES[c]
    return (g, g, g)


def petal(name, u, v, px, py, w, h):
    g = 230 - 70 * v
    if v < 0.15:
        g = 255
    return (g, g, g)


def green(name, u, v, px, py, w, h):
    if name == "top":
        return LEAF_HI
    return LEAF if abs(v - 0.5) > 0.12 or name not in ("top", "front", "back") else STEM


def leaf(name, u, v, px, py, w, h):
    if name not in ("front", "back"):
        return None
    half = 0.5 * math.sin(math.pi * u) ** 0.8
    if abs(v - 0.5) > half:
        return None
    if abs(v - 0.5) < 0.08 and 0.1 < u < 0.85:
        return STEM
    return LEAF_HI if v < 0.5 else LEAF


def stem(name, u, v, px, py, w, h):
    return LEAF_HI if name in ("left", "top") else STEM


def spark(name, u, v, px, py, w, h):
    return (255, 246, 210)


def build():
    it = Item("glass_rose", "Glass Rose", "cape", ["cape", "rose", "glass", "animated"], [("Rose", "#e3264c")], texel=4, size=256)
    x0, x1, y0, y1, z0, z1 = -5.0, 5.0, 7.6, 23.6, -4.0, -2.4
    zc = (z0 + z1) / 2
    bar = 0.3
    case = it.bone("case", pivot=(0, 24.0, -2.3), rot=(3, 0, 0),
                   physics={"stiffness": 46, "damping": 5.5, "inertia": 1.3, "drive": {"sprint": [14, 0, 0], "air": [9, 0, 0], "sneak": [6, 0, 0], "hop": 0.12}})

    frame = painter(metal(hexrgb("#8b95a5"), SILVER, SILVER_HI), 2)
    for x in (x0, x1 - bar):
        for z in (z0, z1 - bar):
            case.box((x, y0, z), (bar, y1 - y0, bar), frame, key="post")
    for y in (y0, y1 - bar):
        for z in (z0, z1 - bar):
            case.box((x0, y, z), (x1 - x0, bar, bar), frame, key="rail_x")
        for x in (x0, x1 - bar):
            case.box((x, y, z0), (bar, bar, z1 - z0), frame, key="rail_z")

    brass = painter(metal(BRASS_LO, BRASS, BRASS_HI), 3)
    case.box((x0 - 0.25, y1, z0 - 0.2), (x1 - x0 + 0.5, 0.5, z1 - z0 + 0.4), brass)
    case.box((-0.8, y1 + 0.5, zc - 0.25), (1.6, 0.4, 0.5), brass, key="knob")
    case.box((x0 - 0.25, y0 - 0.55, z0 - 0.2), (x1 - x0 + 0.5, 0.55, z1 - z0 + 0.4), brass)

    case.box((-0.2, 8.2, zc - 0.2), (0.4, 8.8, 0.4), painter(stem, 4))
    case.box((0.2, 12.2, zc - 0.1), (2.6, 1.2, 0.2), painter(leaf, 5), key="leaf")
    case.box((-2.8, 14.2, zc - 0.1), (2.6, 1.2, 0.2), painter(leaf, 5), key="leaf")

    rose = painter(bloom, 7)
    petals = painter(petal, 8)
    case.box((-1.4, 18.0, zc - 0.3), (2.8, 2.7, 0.6), rose, tint="Rose")
    case.box((-2.2, 17.3, zc - 0.3), (0.8, 2.2, 0.6), petals, key="petal_x", tint="Rose")
    case.box((1.4, 17.3, zc - 0.3), (0.8, 2.2, 0.6), petals, key="petal_x", tint="Rose")
    face = painter(rose_face, 12)
    case.box((-1.75, 17.0, zc - 0.48), (3.5, 3.3, 0.18), face, key="rose_face", tint="Rose")
    case.box((-1.75, 17.0, zc + 0.3), (3.5, 3.3, 0.18), face, key="rose_face", tint="Rose")
    case.box((-0.85, 20.7, zc - 0.25), (1.7, 0.6, 0.5), rose, key="bud", tint="Rose")
    case.box((-1.0, 16.8, zc - 0.3), (2.0, 0.5, 0.6), painter(green, 6))
    case.box((1.5, 11.25, zc - 0.25), (1.1, 0.12, 0.5), petals, key="fallen", tint="Rose")
    case.box((-3.1, 8.0, zc - 0.2), (0.9, 0.12, 0.45), petals, key="fallen", tint="Rose")

    glow = it.bone("glow", pivot=(0, 18.0, zc), anim={"type": "sparkle", "speed": 0.4, "amplitude": 1, "phase": 0})
    for x, y, dz in ((-3.2, 20.6, 0.2), (2.9, 21.4, -0.3), (-2.6, 15.8, -0.25), (3.3, 16.4, 0.15), (0.6, 22.2, 0.0)):
        glow.box((x, y, zc + dz - 0.13), (0.26, 0.26, 0.26), painter(spark, 9), key="spark")

    inner = (x1 - x0 - 2 * bar, z1 - z0 - 2 * bar)
    case.box((x0 + bar + 0.05, y0 + bar, z0 + bar + 0.05), (inner[0] - 0.1, 3.4, inner[1] - 0.1), painter(water, 10))
    case.box((x0 + bar - 0.05, y0 + bar - 0.05, z0 + bar - 0.05), (inner[0] + 0.1, y1 - y0 - 2 * bar + 0.1, inner[1] + 0.1), painter(glass, 11))
    return it
