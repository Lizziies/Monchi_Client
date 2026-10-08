import numpy as np

from item import Item
from shoes import IN, TEXEL, wear

W, H = 4.2, 3.2
Z0, Z1 = -2.3, 2.8


def noise(art, cell=0.125, seed=0):
    cx, cy = np.floor(art.X / cell), np.floor(art.Y / cell)
    h = np.sin(cx * 12.9898 + cy * 78.233 + seed) * 43758.5453
    return h - np.floor(h)


def fur(base=244, edge=0.12, speckle=0.07, seed=0):
    def paint(face, art):
        v = art.Y / max(art.h, 1e-6)
        shade = 1 - edge * v ** 2 if face in ("front", "back", "left", "right") else 1.0
        art.put(np.ones(art.X.shape), base * shade * (1 - speckle * noise(art, seed=seed)))
    return paint


def rect(art, x0, y0, x1, y1):
    return ((art.X > x0) & (art.X < x1) & (art.Y > y0) & (art.Y < y1)).astype(float)


def face(eyes, features=None, base=244, seed=0):
    """Front of an animal head: the fur plus dark eyes with a highlight; `features(art)` paints the rest."""
    def paint(f, art):
        fur(base, seed=seed)(f, art)
        if f != "front":
            return
        for ex, ey, ew, eh in eyes:
            art.put(rect(art, ex, ey, ex + ew, ey + eh), 34)
            art.put(rect(art, ex + ew * 0.55, ey + eh * 0.1, ex + ew * 0.9, ey + eh * 0.4), 255)
        if features:
            features(art)
    return paint


def whiskers(art):
    for side in (0.55, W - 0.55):
        for dy, tilt in ((2.15, -0.12), (2.45, 0.12)):
            d = 0.9 if side < W / 2 else -0.9
            x = np.linspace(side, side + d, 8)
            for xi in x:
                art.put(rect(art, xi - 0.04, dy + tilt * (xi - side) - 0.035, xi + 0.04, dy + tilt * (xi - side) + 0.035), 120, 0.9)


def smile(art):
    mid = W / 2
    art.put(rect(art, mid - 0.05, 2.2, mid + 0.05, 2.55), 40)
    art.put(rect(art, mid - 0.55, 2.5, mid - 0.05, 2.6), 40)
    art.put(rect(art, mid + 0.05, 2.5, mid + 0.55, 2.6), 40)


def cheeks(art):
    for x in (0.85, W - 0.85):
        d = np.hypot(art.X - x, art.Y - 2.25)
        art.put((d < 0.5).astype(float), 255, 0.9)


def axolotl_mouth(art):
    mid = W / 2
    art.put(rect(art, mid - 0.9, 2.3, mid + 0.9, 2.4), 60)
    art.put(rect(art, mid - 1.05, 2.2, mid - 0.9, 2.3), 60)
    art.put(rect(art, mid + 0.9, 2.2, mid + 1.05, 2.3), 60)
    for x in (0.7, 0.95, W - 0.95, W - 0.7):
        art.put(((np.hypot(art.X - x, art.Y - 1.95) < 0.09)).astype(float), 150, 0.8)


def solid(base=240, seed=0):
    return fur(base, edge=0.05, speckle=0.05, seed=seed)


def head(main, light, accent, eyes, features, seed):
    """x runs outward from the foot's inner edge like in shoes.parts; the leg occupies z -2..2 so nothing but the head
    box wraps it and the face parts sit in front."""
    return {
        "head": (IN, IN + W, 0, H, Z0, Z1, "Main", face(eyes, features, seed=seed)),
    }


def bunny():
    it = Item("bunny_shoes", "Bunny Shoes", "feet", ["feet", "shoes", "animal", "bunny"],
              [("Main", "#f1e3d6"), ("Light", "#fff7ef"), ("Accent", "#f59ab5")], texel=TEXEL, size=256)
    lay = head("Main", "Light", "Accent", [(0.9, 1.15, 0.5, 0.75), (W - 1.4, 1.15, 0.5, 0.75)], lambda a: (cheeks(a), smile(a), whiskers(a)), 1)
    mid = IN + W / 2
    lay["nose"] = (mid - 0.35, mid + 0.35, 1.75, 2.2, Z1, Z1 + 0.2, "Accent", solid(250))
    for name, x in (("ear_a", IN + 0.45), ("ear_b", IN + W - 1.55)):
        lay[name] = (x, x + 1.1, H, H + 3.0, 2.2, 2.7, "Main", solid(244, 3))
        lay[name + "_in"] = (x + 0.2, x + 0.9, H + 0.2, H + 2.5, 2.7, 2.77, "Accent", solid(250))
    lay["tail"] = (mid - 0.8, mid + 0.8, 0.5, 2.1, Z0 - 0.5, Z0, "Light", solid(250, 5))
    return wear(it, lay)


def fox():
    it = Item("fox_shoes", "Fox Shoes", "feet", ["feet", "shoes", "animal", "fox"],
              [("Main", "#e8742a"), ("Light", "#fff1de"), ("Accent", "#2a1a14")], texel=TEXEL, size=256)
    lay = head("Main", "Light", "Accent", [(0.85, 1.1, 0.55, 0.5), (W - 1.4, 1.1, 0.55, 0.5)], None, 2)
    mid = IN + W / 2
    lay["muzzle"] = (mid - 1.15, mid + 1.15, 0.15, 1.7, Z1, Z1 + 0.45, "Light", solid(250, 4))
    lay["nose"] = (mid - 0.4, mid + 0.4, 1.35, 1.8, Z1 + 0.3, Z1 + 0.55, "Accent", solid(250))
    for name, x in (("cheek_a", IN), ("cheek_b", IN + W - 0.9)):
        lay[name] = (x, x + 0.9, 0.2, 1.3, Z1 - 0.05, Z1 + 0.25, "Light", solid(250, 6))
    for name, x in (("ear_a", IN + 0.3), ("ear_b", IN + W - 1.7)):
        lay[name] = (x, x + 1.4, H, H + 1.3, 2.2, 2.7, "Main", solid(244, 3))
        lay[name + "_top"] = (x + 0.25, x + 1.15, H + 1.3, H + 2.4, 2.25, 2.65, "Main", solid(244, 8))
        lay[name + "_tip"] = (x + 0.35, x + 1.05, H + 1.9, H + 2.55, 2.2, 2.7, "Accent", solid(250))
        lay[name + "_in"] = (x + 0.25, x + 1.15, H + 0.2, H + 1.2, 2.7, 2.77, "Light", solid(250))
    lay["tail"] = (mid - 0.9, mid + 0.9, 0.4, 2.2, Z0 - 0.7, Z0, "Main", solid(244, 9))
    lay["tail_tip"] = (mid - 0.7, mid + 0.7, 0.6, 1.8, Z0 - 1.1, Z0 - 0.7, "Light", solid(250, 2))
    return wear(it, lay)


def axolotl():
    it = Item("axolotl_shoes", "Axolotl Shoes", "feet", ["feet", "shoes", "animal", "axolotl"],
              [("Main", "#f7a8c4"), ("Light", "#ffd9e6"), ("Accent", "#e8588f")], texel=TEXEL, size=256)
    lay = head("Main", "Light", "Accent", [(0.55, 1.15, 0.42, 0.42), (W - 0.97, 1.15, 0.42, 0.42)], axolotl_mouth, 3)
    outer = IN + W
    for i, (y, length) in enumerate(((2.35, 1.2), (1.75, 1.0), (1.15, 0.8))):
        lay[f"gill{i}"] = (outer, outer + length, y, y + 0.4, 1.0, 1.9, "Accent", solid(244, 10 + i))
    lay["tail"] = (IN + W / 2 - 0.6, IN + W / 2 + 0.6, 0.2, 1.2, Z0 - 1.0, Z0, "Main", solid(244, 13))
    return wear(it, lay)


def forehead(art):
    for x in (1.5, 2.1, 2.7):
        art.put(rect(art, x - 0.07, 0.0, x + 0.07, 0.5), 120, 0.9)


def cat():
    it = Item("cat_shoes", "Cat Shoes", "feet", ["feet", "shoes", "animal", "cat"],
              [("Main", "#e7b98a"), ("Light", "#fff3e6"), ("Accent", "#f08aa8")], texel=TEXEL, size=256)
    lay = head("Main", "Light", "Accent", [(0.8, 1.15, 0.55, 0.62), (W - 1.35, 1.15, 0.55, 0.62)], lambda a: (smile(a), whiskers(a), forehead(a)), 15)
    mid = IN + W / 2
    lay["muzzle"] = (mid - 0.9, mid + 0.9, 0.35, 1.3, Z1, Z1 + 0.25, "Light", solid(250, 4))
    lay["nose"] = (mid - 0.3, mid + 0.3, 1.1, 1.45, Z1 + 0.15, Z1 + 0.35, "Accent", solid(250))
    for name, x in (("ear_a", IN + 0.25), ("ear_b", IN + W - 1.55)):
        lay[name] = (x, x + 1.3, H, H + 1.0, 2.2, 2.7, "Main", solid(244, 3))
        lay[name + "_top"] = (x + 0.3, x + 1.0, H + 1.0, H + 1.8, 2.25, 2.65, "Main", solid(244, 8))
        lay[name + "_in"] = (x + 0.25, x + 1.05, H + 0.15, H + 1.1, 2.7, 2.77, "Accent", solid(250))
    lay["tail"] = (mid - 0.5, mid + 0.5, 0.6, 3.0, Z0 - 0.4, Z0, "Main", solid(244, 9))
    return wear(it, lay)


def dog():
    it = Item("dog_shoes", "Dog Shoes", "feet", ["feet", "shoes", "animal", "dog"],
              [("Main", "#c58b57"), ("Light", "#f3e1c6"), ("Accent", "#4a2f1c")], texel=TEXEL, size=256)
    lay = head("Main", "Light", "Accent", [(0.85, 1.1, 0.5, 0.55), (W - 1.35, 1.1, 0.5, 0.55)], None, 16)
    mid = IN + W / 2
    lay["muzzle"] = (mid - 1.1, mid + 1.1, 0.2, 1.55, Z1, Z1 + 0.5, "Light", solid(250, 4))
    lay["nose"] = (mid - 0.45, mid + 0.45, 1.2, 1.7, Z1 + 0.35, Z1 + 0.6, "Accent", solid(250))
    outer = IN + W
    lay["ear"] = (outer, outer + 0.55, 0.9, 3.4, 1.0, 2.5, "Accent", solid(244, 5))
    lay["ear_top"] = (outer - 0.6, outer + 0.1, H, H + 0.55, 1.0, 2.5, "Accent", solid(244, 6))
    lay["tail"] = (mid - 0.45, mid + 0.45, 1.0, 2.7, Z0 - 0.8, Z0, "Main", solid(244, 9))
    return wear(it, lay)


def panda():
    it = Item("panda_shoes", "Panda Shoes", "feet", ["feet", "shoes", "animal", "panda"],
              [("Main", "#fbfbfb"), ("Light", "#ffffff"), ("Accent", "#25252b")], texel=TEXEL, size=256)

    def patches(art):
        for ex in (0.55, W - 1.55):
            d = np.hypot((art.X - ex - 0.5) / 0.62, (art.Y - 1.35) / 0.55)
            art.put((d < 1).astype(float), 40)
        for ex in (0.85, W - 1.35):
            art.put(rect(art, ex, 1.2, ex + 0.3, 1.5), 255)
        art.put(rect(art, W / 2 - 0.04, 2.35, W / 2 + 0.04, 2.7), 60)

    lay = head("Main", "Light", "Accent", [], patches, 17)
    mid = IN + W / 2
    lay["nose"] = (mid - 0.4, mid + 0.4, 1.7, 2.1, Z1, Z1 + 0.2, "Accent", solid(250))
    for name, x in (("ear_a", IN + 0.05), ("ear_b", IN + W - 1.15)):
        lay[name] = (x, x + 1.1, H - 0.1, H + 0.9, 2.2, 2.8, "Accent", solid(244, 3))
    lay["tail"] = (mid - 0.5, mid + 0.5, 0.8, 2.0, Z0 - 0.5, Z0, "Light", solid(250, 9))
    return wear(it, lay)


def build():
    return [bunny(), fox(), axolotl(), cat(), dog(), panda()]
