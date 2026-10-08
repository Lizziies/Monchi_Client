import math

import numpy as np

from item import Item
from paint import Art

K = 4  # texels per unit; keep every box size a multiple of 0.25 so the loader's rounding matches
FACES = ("top", "bottom", "west", "front", "east", "back")


def box(item, bone, origin, size, tint, paint):
    """Textured box in the Minecraft layout. paint(face, art) fills one face; art X/Y follow the face as seen from outside."""
    w, h, d = (round(v * K) for v in size)
    sheet = np.zeros((d + h, 2 * d + 2 * w, 4), np.uint8)
    spots = {"top": (d, 0, size[0], size[2]), "bottom": (d + w, 0, size[0], size[2]), "west": (0, d, size[2], size[1]),
             "front": (d, d, size[0], size[1]), "east": (d + w, d, size[2], size[1]), "back": (2 * d + w, d, size[0], size[1])}
    for face, (x, y, fw, fh) in spots.items():
        art = Art(fw, fh, K)
        paint(face, art)
        px = art.pixels()
        sheet[y : y + px.shape[0], x : x + px.shape[1]] = px
    uv = item.place(sheet)
    bone.setdefault("cubes", []).append({"origin": [round(v, 3) for v in origin], "size": [round(v, 3) for v in size], "uv": list(uv), "tint": tint})


def fur(base=232, spread=9, under=0.78, seed=0, stripes=None, light=None):
    def paint(face, art):
        rng = np.random.default_rng(seed + FACES.index(face))
        cells = rng.normal(0, spread, (art.ph * 3 // 3 + 1, art.pw + 1))
        noise = cells[(art.Y * K).astype(int).clip(0, cells.shape[0] - 1), (art.X * K).astype(int).clip(0, cells.shape[1] - 1)]
        g = base + noise
        if face == "bottom":
            g = g * under
        elif face != "top":
            g = g * (1 - (1 - under) * 0.6 * np.clip(art.Y / max(art.h, 0.1), 0, 1))
        if stripes and face in ("top", "west", "east"):
            along = art.X if face == "top" else art.X
            band = (np.sin(along * stripes) > 0.55) & (art.Y < art.h * (1.0 if face == "top" else 0.6))
            g = np.where(band, g * 0.72, g)
        if light and face in light:
            g = np.where(light[face](art), np.maximum(g, 248), g)
        art.put(np.ones(art.X.shape), g)
    return paint


def flat_color(value=240):
    def paint(face, art):
        art.put(np.ones(art.X.shape), value - (25 if face == "bottom" else 0))
    return paint


def eye(slit=False):
    def paint(face, art):
        art.put(np.ones(art.X.shape), 235)
        if face == "front":
            if slit:
                pupil = np.abs(art.X - art.w / 2) < art.w * 0.18
            else:
                pupil = (art.Y > art.h * 0.25)
            art.put(pupil.astype(float), 40)
            art.put(((art.X < art.w * 0.35) & (art.Y < art.h * 0.35)).astype(float), 255)
    return paint


def grow(item, scale, anchor):
    """Scales a finished pet about a ground point; the painted texels keep their size through an explicit uvsize."""
    ax = np.array(anchor, float)
    for b in item.bones:
        if b["pivot"] != [0, 0, 0] or b["name"] != "pet":
            b["pivot"] = [round(float(v), 3) for v in ax + (np.array(b["pivot"]) - ax) * scale]
        for c in b.get("cubes", []):
            if "uvsize" not in c:
                c["uvsize"] = [round(v * K) for v in c["size"]]
            c["origin"] = [round(float(v), 3) for v in ax + (np.array(c["origin"]) - ax) * scale]
            c["size"] = [round(v * scale, 3) for v in c["size"]]
    return item


def follower(item, side, lag=26.0):
    """Root bone at the player's centre: rotating it about Y moves the pet around the player, so the spring makes it trail behind while walking."""
    return item.bone("pet", (0, 0, 0), (0, 0, 0), physics={
        "type": "spring", "stiffness": 16.0, "damping": 4.0, "inertia": 0.5,
        "drive": {"speed": [0, side * lag, 0], "sprint": [0, side * lag * 0.45, 0]}})


def legs(item, parent, spots, height, width, swing=38.0):
    for n, (x, z, phase) in enumerate(spots):
        b = item.bone(f"leg{n}", (x, height, z), parent=parent, anim={"type": "walk", "axis": "x", "amplitude": swing, "speed": 1, "phase": phase})
        yield b, (x - width / 2, 0, z - width / 2), (width, height, width)


def cat():
    it = Item("pet_cat", "Cat Companion", "pet", ["pet", "cat", "animated"],
              [("Fur", "#f2a65a"), ("Belly", "#fff3e3"), ("Eyes", "#8fd14f"), ("Nose", "#ff8fa8")], texel=K, size=256)
    x = 11.0
    follower(it, 1)
    body = it.bone("body", (x, 3.0, 0), parent="pet", anim={"type": "hop", "axis": "y", "amplitude": 0.1, "speed": 0.35, "phase": 0})
    box(it, body, (x - 1.25, 2.0, -2.75), (2.5, 2.25, 5.5), "Fur", fur(stripes=3.2))
    head = it.bone("head", (x, 4.0, 2.4), parent="body", anim={"type": "sway", "axis": "y", "amplitude": 16.0, "speed": 0.16, "phase": 0},
                   physics={"type": "spring", "stiffness": 30.0, "damping": 5.0, "inertia": 0.6, "drive": {"speed": [6, 0, 0]}})
    box(it, head, (x - 1.5, 3.25, 2.25), (3.0, 2.75, 2.5), "Fur", fur(stripes=4.0, seed=3))
    for ex in (x - 1.25, x + 0.5):
        box(it, head, (ex, 6.0, 3.0), (0.75, 0.75, 0.5), "Fur", fur(seed=5))
        box(it, head, (ex + 0.25 if ex < x else ex + 0.0, 6.0, 3.5), (0.5, 0.5, 0.25), "Nose", flat_color(235))
    box(it, head, (x - 0.5, 3.25, 4.75), (1.0, 0.75, 0.5), "Belly", flat_color(245))
    box(it, head, (x - 0.25, 4.0, 4.75), (0.5, 0.25, 0.25), "Nose", flat_color(245))
    for ex in (x - 1.0, x + 0.5):
        box(it, head, (ex, 4.5, 4.7), (0.5, 0.5, 0.25), "Eyes", eye(slit=True))
    for b, o, s in legs(it, "body", ((x - 0.75, 1.75, 0.0), (x + 0.75, 1.75, math.pi), (x - 0.75, -2.0, math.pi), (x + 0.75, -2.0, 0.0)), 2.25, 0.75):
        box(it, b, o, s, "Fur", fur(seed=7))
    tail = it.bone("tail", (x, 3.9, -2.6), (42, 0, 0), parent="body", anim={"type": "wag", "axis": "y", "amplitude": 18.0, "speed": 0.55, "phase": 0},
                   physics={"type": "spring", "stiffness": 24.0, "damping": 3.5, "inertia": 1.2, "drive": {"speed": [-18, 0, 0]}})
    box(it, tail, (x - 0.25, 3.65, -7.0), (0.5, 0.5, 4.5), "Fur", fur(stripes=5.0, seed=9))
    return grow(it, 1.4, (x - 1.0, 0, 0))


def dog():
    it = Item("pet_dog", "Dog Companion", "pet", ["pet", "dog", "animated"],
              [("Fur", "#c98d55"), ("Belly", "#fff3e3"), ("Ears", "#7a4f2c"), ("Eyes", "#2a1f1a"), ("Collar", "#e23b3b"), ("Tag", "#ffd36b")], texel=K, size=256)
    x = -11.5
    follower(it, -1)
    body = it.bone("body", (x, 3.5, 0), parent="pet", anim={"type": "hop", "axis": "y", "amplitude": 0.12, "speed": 0.45, "phase": 0})
    box(it, body, (x - 1.5, 2.5, -3.0), (3.0, 2.75, 6.0), "Fur", fur(seed=11))
    box(it, body, (x - 1.25, 2.25, -2.5), (2.5, 0.25, 5.0), "Belly", flat_color(245))
    head = it.bone("head", (x, 5.0, 2.6), parent="body", anim={"type": "sway", "axis": "y", "amplitude": 14.0, "speed": 0.2, "phase": 1.0},
                   physics={"type": "spring", "stiffness": 30.0, "damping": 5.0, "inertia": 0.6, "drive": {"speed": [8, 0, 0]}})
    box(it, head, (x - 1.75, 4.0, 2.5), (3.5, 3.0, 3.0), "Fur", fur(seed=13))
    box(it, head, (x - 1.0, 4.0, 5.5), (2.0, 1.25, 1.5), "Belly", flat_color(245))
    box(it, head, (x - 0.5, 5.0, 6.75), (1.0, 0.5, 0.5), "Eyes", flat_color(200))
    box(it, head, (x - 0.5, 3.75, 6.25), (0.75, 0.25, 0.75), "Collar", flat_color(250))
    for ex in (x - 1.25, x + 0.5):
        box(it, head, (ex, 5.75, 5.45), (0.75, 0.75, 0.25), "Eyes", eye())
    for side in (-1, 1):
        ear = it.bone(f"ear{side}", (x + side * 1.75, 6.75, 3.6), (0, 0, side * 12), parent="head",
                      anim={"type": "sway", "axis": "z", "amplitude": side * 6.0, "speed": 0.5, "phase": 0.0},
                      physics={"type": "spring", "stiffness": 22.0, "damping": 3.0, "inertia": 1.6, "drive": {"air": [0, 0, side * 25], "speed": [0, 0, side * 8]}})
        box(it, ear, (x + side * 1.75 - (0.25 if side > 0 else 0.75) + (0.0 if side > 0 else 0.0), 4.75, 3.25), (1.0, 2.25, 0.75), "Ears", fur(seed=15, base=215))
    box(it, body, (x - 1.75, 4.75, 1.75), (3.5, 0.75, 1.0), "Collar", flat_color(240))
    box(it, body, (x - 0.25, 4.0, 2.6), (0.5, 0.75, 0.25), "Tag", flat_color(250))
    for b, o, s in legs(it, "body", ((x - 0.875, 2.0, 0.0), (x + 0.875, 2.0, math.pi), (x - 0.875, -2.25, math.pi), (x + 0.875, -2.25, 0.0)), 2.5, 1.0, swing=34.0):
        box(it, b, o, s, "Fur", fur(seed=17))
    tail = it.bone("tail", (x, 5.0, -2.9), (-58, 0, 0), parent="body", anim={"type": "wag", "axis": "y", "amplitude": 26.0, "speed": 1.4, "phase": 0})
    box(it, tail, (x - 0.375, 4.75, -5.9), (0.75, 0.75, 3.0), "Fur", fur(seed=19))
    return grow(it, 1.35, (x + 1.0, 0, 0))


def bunny():
    it = Item("pet_bunny", "Shoulder Bunny", "pet", ["pet", "bunny", "shoulder", "cute", "animated"],
              [("Fur", "#ffffff"), ("Inner", "#ffb3c8"), ("Eyes", "#2a1f2a"), ("Bow", "#ff6fae")], texel=K, size=256)
    x, y = 6.0, 24.5
    root = it.bone("pet", (x, y, 0), anim={"type": "hop", "axis": "y", "amplitude": 0.9, "speed": 0.7, "phase": 0},
                   physics={"type": "spring", "stiffness": 26.0, "damping": 3.6, "inertia": 1.0, "drive": {"sprint": [-10, 0, 0], "air": [8, 0, 0]}})
    box(it, root, (x - 1.0, y, -1.25), (2.0, 1.75, 2.5), "Fur", fur(base=240, spread=6, seed=21))
    box(it, root, (x - 0.5, y + 0.5, -1.75), (1.0, 1.0, 0.5), "Fur", fur(base=248, spread=4, seed=22))
    head = it.bone("head", (x, y + 1.75, 0.75), parent="pet", anim={"type": "sway", "axis": "z", "amplitude": 7.0, "speed": 0.35, "phase": 0})
    box(it, head, (x - 1.0, y + 1.25, 0.25), (2.0, 1.75, 1.75), "Fur", fur(base=242, spread=6, seed=23))
    box(it, head, (x - 0.25, y + 1.75, 2.0), (0.5, 0.25, 0.25), "Inner", flat_color(245))
    for ex in (x - 0.75, x + 0.25):
        box(it, head, (ex, y + 2.25, 1.95), (0.5, 0.5, 0.25), "Eyes", eye())
    for side, ex, ph in ((-1, x - 0.75, 0.0), (1, x + 0.25, 1.3)):
        ear = it.bone(f"ear{side}", (ex + 0.25, y + 3.0, 1.0), (-8, 0, side * 10), parent="head",
                      anim={"type": "twitch", "axis": "x", "amplitude": 22.0, "speed": 0.5, "phase": ph},
                      physics={"type": "spring", "stiffness": 24.0, "damping": 3.0, "inertia": 1.8, "drive": {"air": [-25, 0, 0], "sprint": [-30, 0, 0]}})
        box(it, ear, (ex, y + 3.0, 0.75), (0.5, 2.0, 0.5), "Fur", fur(base=242, spread=5, seed=25))
        box(it, ear, (ex + 0.125, y + 3.25, 1.25), (0.25, 1.5, 0.05), "Inner", flat_color(240))
    bow = it.bone("bow", (x + 0.5, y + 3.25, 1.0), parent="ear1")
    box(it, bow, (x + 0.75, y + 3.0, 0.75), (1.0, 0.5, 0.5), "Bow", flat_color(245))
    box(it, bow, (x - 0.25, y + 3.0, 0.75), (0.5, 0.5, 0.5), "Bow", flat_color(245))
    return it


def membrane(span, depth, flip=False):
    art = Art(span, depth, K)
    u, v = art.X / span, art.Y / depth
    if flip:
        u = 1 - u
    scallop = 0.22 * np.abs(np.sin(u * math.pi * 3))
    inside = (v > 0.08 + 0.5 * u ** 2) & (v < 1 - scallop - 0.25 * u)
    art.put(inside.astype(float), 210 - 50 * u + 10 * np.sin(art.X * 4))
    bones = np.zeros(art.X.shape, bool)
    for k in (0.33, 0.66, 1.0):
        bones |= np.abs(v - (0.1 + k * 0.62) * np.clip(u, 0, 1) - 0.08) < 0.05
    art.put((inside & bones).astype(float), 120)
    edge = (v > 0.03 + 0.5 * u ** 2) & (v < 0.16 + 0.5 * u ** 2) & (u < 0.98)
    art.put(edge.astype(float), 150)
    return art


def dragon():
    it = Item("pet_dragon", "Dragon Companion", "pet", ["pet", "dragon", "flying", "animated"],
              [("Scale", "#3e3158"), ("Belly", "#c2a6ff"), ("Wing", "#6c52a6"), ("Horn", "#f0e6d2"), ("Eyes", "#ffcc33")], texel=K, size=256)
    x, y = -15.0, 25.0
    root = it.bone("pet", (0, 0, 0), anim={"type": "float", "axis": "y", "amplitude": 0.9, "speed": 0.32, "phase": 0},
                   physics={"type": "spring", "stiffness": 12.0, "damping": 3.2, "inertia": 0.5, "drive": {"speed": [0, -30, 0], "sprint": [0, -14, 0]}})
    body = it.bone("body", (x, y, 0), parent="pet", physics={"type": "spring", "stiffness": 20.0, "damping": 3.0, "inertia": 1.0, "drive": {"speed": [10, 0, 0]}})
    scales = fur(base=225, spread=10, seed=31, stripes=6.0)
    box(it, body, (x - 1.0, y - 1.0, -2.5), (2.0, 2.0, 4.5), "Scale", scales)
    box(it, body, (x - 0.75, y - 1.1, -2.0), (1.5, 0.25, 3.75), "Belly", flat_color(240))
    for lx in (x - 0.75, x + 0.25):
        box(it, body, (lx, y - 1.75, 0.75), (0.5, 0.75, 0.5), "Scale", scales)
        box(it, body, (lx, y - 1.75, -1.75), (0.5, 0.75, 0.75), "Scale", scales)
    head = it.bone("head", (x, y + 0.75, 2.0), parent="body", anim={"type": "sway", "axis": "y", "amplitude": 12.0, "speed": 0.2, "phase": 0.5})
    box(it, head, (x - 1.0, y + 0.25, 2.0), (2.0, 1.75, 2.25), "Scale", fur(base=225, spread=8, seed=33))
    box(it, head, (x - 0.625, y + 0.25, 4.25), (1.25, 1.0, 1.25), "Scale", fur(base=230, spread=6, seed=34))
    for hx in (x - 0.75, x + 0.5):
        box(it, head, (hx, y + 2.0, 2.25), (0.25, 1.0, 0.25), "Horn", flat_color(245))
    for ex in (x - 1.05, x + 0.55):
        box(it, head, (ex, y + 1.25, 3.5), (0.5, 0.5, 0.5), "Eyes", eye(slit=True))
    for side in (-1, 1):
        px = x + side * 1.0
        wing = it.bone(f"wing{side}", (px, y + 0.75, 0.0), (90, 0, side * 8), parent="body",
                       anim={"type": "flap", "axis": "z", "amplitude": side * 34.0, "speed": 1.1, "phase": 0.0})
        art = membrane(5.5, 4.5, flip=side < 0)
        uv = it.place(art.pixels())
        ox = px if side > 0 else px - 5.5
        it.card(wing, (ox, y + 0.75 - 2.75, 0.0), (5.5, 4.5, 0.04), uv, "Wing")
    tail = it.bone("tail", (x, y - 0.25, -2.5), (12, 0, 0), parent="body", anim={"type": "sway", "axis": "y", "amplitude": 20.0, "speed": 0.45, "phase": 0},
                   physics={"type": "spring", "stiffness": 18.0, "damping": 3.0, "inertia": 1.4, "drive": {"speed": [-12, 0, 0]}})
    box(it, tail, (x - 0.5, y - 0.75, -5.5), (1.0, 1.0, 3.0), "Scale", scales)
    box(it, tail, (x - 0.25, y - 0.6, -7.75), (0.5, 0.5, 2.25), "Scale", scales)
    box(it, tail, (x - 0.5, y - 0.5, -8.5), (1.0, 0.25, 0.75), "Horn", flat_color(240))
    return grow(it, 1.2, (x - 1.0, y, 0))


def build():
    return [cat(), dog(), bunny(), dragon()]
