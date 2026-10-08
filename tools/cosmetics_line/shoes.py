import math

import numpy as np

from item import Item
from paint import Art

TEXEL = 8
IN = 0.08
GROUND = 0.03


def faces(size, texel):
    """Pixel rectangles of a Minecraft box layout: (name, x, y, w, h) with w and h in art units."""
    w, h, d = size
    k = texel
    pw, ph, pd = round(w * k), round(h * k), round(d * k)
    layout = {
        "top": (pd, 0, w, d), "bottom": (pd + pw, 0, w, d),
        "front": (pd, pd, w, h), "back": (2 * pd + pw, pd, w, h),
        "right": (pd + pw, pd, d, h), "left": (0, pd, d, h),
    }
    return (2 * pd + 2 * pw, pd + ph), layout


def box_uv(item, size, paint):
    """Paints the six faces of a box through `paint(face, art)` and places the result. The left face is the right face
    flipped, so a shoe painted once looks right on both feet."""
    (tw, th), layout = faces(size, item.texel)
    rect = np.zeros((th, tw, 4), np.uint8)
    cache = {}
    for name, (x, y, w, h) in layout.items():
        src = "right" if name == "left" else name
        if src not in cache:
            art = Art(w, h, item.texel)
            paint(src, art)
            cache[src] = art.pixels()
        px = cache[src][:, ::-1] if name == "left" else cache[src]
        rect[y : y + px.shape[0], x : x + px.shape[1]] = px[: th - y, : tw - x]
    return item.place(rect)


def stitched(art, rows, seam=170):
    for y in rows:
        art.put((np.abs(art.Y - y) < 0.045) & (np.mod(art.X, 0.5) < 0.3), seam, 0.85)


def plain(base=240, bottom_dark=0.12, seam_rows=()):
    def paint(face, art):
        v = art.Y / max(art.h, 1e-6)
        shade = 1 - bottom_dark * v ** 2 if face in ("front", "back", "right") else 1.0
        art.put(np.ones(art.X.shape), base * shade)
        stitched(art, seam_rows)
        edge = np.minimum.reduce([art.X, art.w - art.X, art.Y, art.h - art.Y])
        art.put((edge < 0.07).astype(float), base * 0.78, 0.7)
    return paint


def sole(face, art):
    art.put(np.ones(art.X.shape), 248)
    if face == "bottom":
        grooves = (np.mod(art.Y, 0.62) < 0.09) | (np.abs(art.X - art.w / 2) < 0.07)
        art.put(grooves.astype(float), 150, 0.9)
        art.put(((np.abs(art.Y - art.h * 0.62) < 0.1)).astype(float), 130, 0.9)
        return
    band = (art.Y / art.h) > 0.62
    art.put(band.astype(float), 205)
    art.put((np.abs(art.Y / art.h - 0.62) < 0.05).astype(float), 170, 0.8)
    if face in ("front", "right", "left"):
        dots = (np.hypot(np.mod(art.X, 0.9) - 0.45, art.Y - art.h * 0.3) < 0.1)
        art.put(dots.astype(float), 210, 0.9)


def upper(face, art, perf=False):
    plain(238, 0.16, (art.h * 0.5,) if face in ("front", "right") else ())(face, art)
    if face == "right" and perf:
        for r in (0.34, 0.52):
            art.put((np.hypot(np.mod(art.X + r, 0.55) - 0.27, np.abs(art.Y - art.h * r)) < 0.08).astype(float), 180, 0.8)


def accent_plate(face, art):
    if face not in ("right", "front", "top", "back"):
        return
    art.put(np.ones(art.X.shape), 255)
    u, v = art.X / art.w, art.Y / art.h
    art.put((v > 0.82).astype(float), 215)


def stripes(face, art):
    if face != "right":
        return
    u, v = art.X / art.w, art.Y / art.h
    for lo, hi in ((0.0, 0.2), (0.3, 0.42)):
        art.put(art.inside([(art.w * (x + 0.0), art.h * y) for x, y in ((0.2 + lo, 0), (0.2 + hi, 0), (0.78 + hi, 1), (0.78 + lo, 1))]), 250)


def laces(face, art):
    if face in ("left", "right", "bottom", "back"):
        return
    if face == "front":
        v = art.Y / art.h
        bars = np.abs(np.mod(art.Y, 0.55) - 0.275) < 0.11
        art.put(bars.astype(float), 252)
        art.put((np.abs(art.X - art.w / 2) < 0.12).astype(float), 200, 0.9)
    else:
        v = art.Y / art.h
        bars = np.abs(np.mod(v * 3.0, 1.0) - 0.5) < 0.2
        art.put(bars.astype(float), 252)


def cuff(face, art):
    art.put(np.ones(art.X.shape), 250 - 40 * (art.Y / max(art.h, 1e-6)) ** 2)
    art.put((np.abs(np.mod(art.X, 0.7) - 0.35) < 0.05).astype(float), 210, 0.7)


def parts(s):
    """name -> (xa, xb, y0, y1, z0, z1, tint, painter); x runs outward from the inner edge of the +x foot."""
    cx = (IN + 4.15) / 2
    so, top, vamp, toe, f = s["sole"], s["top"], s["vamp"], s["toe"], s.get("front", 1.6)
    up = s.get("upper", upper)
    out = {
        "sole": (IN, 4.38, 0, so, -2.4, f + 1.45, "Sole", s.get("soleart", sole)),
        "upper": (IN + 0.02, 4.22, so, top, -2.25, f, "Main", lambda fc, a: up(fc, a, True)),
        "vamp": (IN + 0.04, 4.17, so, vamp, f, f + 0.85, "Main", upper),
        "toe": (IN + 0.06, 4.12, so, toe, f + 0.85, f + 1.4, "Main", lambda fc, a: upper(fc, a, True)),
        "collar": (IN, 4.26, top - 0.1, top + 0.5, -2.32, f + 0.08, "Accent", cuff),
        "tab": (cx - 0.7, cx + 0.7, top - 1.6, top - 0.2, -2.4, -2.25, "Accent", accent_plate),
        "laces": (IN + 0.9, 3.3, vamp, vamp + 0.1, f, f + 0.85, "Accent", laces),
        "tongue": (IN + 0.9, 3.3, vamp - 0.05, top - 0.1, f, f + 0.12, "Accent", laces),
        "stripe": (4.22, 4.31, so + 0.35, top - 0.7, -1.8, f - 0.2, "Accent", stripes),
    }
    if not s.get("stripe", True):
        del out["stripe"]
    return out


def wear(item, layout):
    """Places every part of a shoe layout on both feet; each foot is one bone at the leg's own pivot."""
    layout = {n: (xa, xb, max(y0, GROUND), y1, z0, z1, t, pt) for n, (xa, xb, y0, y1, z0, z1, t, pt) in layout.items()}
    uv = {name: box_uv(item, (xb - xa, y1 - y0, z1 - z0), painter) for name, (xa, xb, y0, y1, z0, z1, tint, painter) in layout.items()}
    for sx, label in ((1, "l"), (-1, "r")):
        bone = item.bone(f"shoe_{label}", (sx * 2.0, 3.4, 0.0))
        for name, (xa, xb, y0, y1, z0, z1, tint, _) in layout.items():
            item.box(bone, (xa if sx > 0 else -xb, y0, z0), (xb - xa, y1 - y0, z1 - z0), uv[name], tint)
    return item


def build_shoes(item, spec):
    return wear(item, parts(spec))


def sneakers():
    it = Item("court_sneakers", "Court Sneakers", "feet", ["feet", "shoes", "sneakers"],
              [("Main", "#f1f3f8"), ("Sole", "#ffffff"), ("Accent", "#ff5f93")], texel=TEXEL, size=256)
    return build_shoes(it, {"sole": 0.95, "top": 3.3, "vamp": 2.6, "toe": 1.95})


def boots():
    it = Item("trail_boots", "Trail Boots", "feet", ["feet", "shoes", "boots"],
              [("Main", "#8a5f3c"), ("Sole", "#33302f"), ("Accent", "#e6d3ae")], texel=TEXEL, size=256)
    return build_shoes(it, {"sole": 1.25, "top": 5.4, "vamp": 3.2, "toe": 2.3, "front": 2.2, "stripe": False})


def high_sneakers():
    it = Item("high_sneakers", "High Sneakers", "feet", ["feet", "shoes", "sneakers"],
              [("Main", "#f1f2f6"), ("Sole", "#2a2a30"), ("Accent", "#e5304a")], texel=TEXEL, size=256)
    return build_shoes(it, {"sole": 1.1, "top": 5.0, "vamp": 2.9, "toe": 2.1, "front": 2.2})


def build():
    return [sneakers(), high_sneakers(), boots()]
