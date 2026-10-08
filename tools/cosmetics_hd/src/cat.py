import math
import random

from lib import Item, clamp, hexrgb, lerp

PALETTE = {
    "G": hexrgb("#a29cad"),
    "L": hexrgb("#c4bfcc"),
    "S": hexrgb("#6e687c"),
    "C": hexrgb("#f7f4f8"),
    "c": hexrgb("#e6e0e9"),
    "K": hexrgb("#221a24"),
    "E": hexrgb("#62c27c"),
    "e": hexrgb("#a6e08a"),
    "W": hexrgb("#ffffff"),
    "P": hexrgb("#ff9db5"),
    "N": hexrgb("#f57f9c"),
    "I": hexrgb("#ffb8c6"),
}

EYE = [
    ".KKKK.",
    "KWWKEK",
    "KWWKEK",
    "KEEKEK",
    "KeEKEK",
    "KeEKWK",
    ".KKKK.",
]

FLAT = set("KWEePN")


def shade(c, k):
    return tuple(clamp(v * k, 0, 255) for v in c)


def light(name, v):
    if name == "top":
        return 1.07
    if name == "bottom":
        return 0.8
    return 1.06 - 0.16 * v


def box_painter(face_fn, seed, grain=0.07):
    rng = random.Random(seed)
    noise = [[rng.random() for _ in range(64)] for _ in range(64)]

    def run(canvas, faces):
        for name, (x, y, w, h) in faces.items():
            for py in range(h):
                for px in range(w):
                    c = face_fn(name, px, py, w, h)
                    if c is None or c == ".":
                        continue
                    if c in FLAT:
                        canvas.set(x + px, y + py, (*PALETTE[c], 255))
                        continue
                    k = light(name, (py + 0.5) / h) * (1 - grain / 2 + grain * noise[(y + py) % 64][(x + px) % 64])
                    canvas.set(x + px, y + py, (*shade(PALETTE[c], k), 255))

    return run


def tabby(px, py, base="G"):
    return "S" if (px + py // 2) % 5 == 0 else base


def face_front(px, py, w, h):
    for ex in (3, w - 9):
        dx, dy = px - ex, py - 6
        if 0 <= dx < 6 and 0 <= dy < len(EYE) and EYE[dy][dx] != ".":
            return EYE[dy][dx]
    mid = (w - 1) / 2
    m = abs(px - mid)
    if py == 12 and m < 1.6:
        return "N"
    if py == 13 and m < 0.6:
        return "N"
    if py == 14 and m < 0.6:
        return "K"
    if py == 15 and 0.6 < m < 1.6:
        return "K"
    for bx in (1, w - 5):
        if 0 <= px - bx < 4 and py == 13:
            return "P"
        if 0 < px - bx < 3 and py in (12, 14):
            return "P" if py == 14 else "I"
    if py <= 4 and (abs(m - 0.0) < 0.6 or abs(m - 2.5) < 0.6) and py < 4 - (m > 1) * 1:
        return "S"
    if py >= 15 and m < 6:
        return "C"
    if py >= 11 and m < 4.0:
        return "C"
    side = min(px, w - 1 - px)
    if py >= 13 and side < 2:
        return "C"
    if side < 2 and py in (7, 9):
        return "S"
    return "G"


def head_face(name, px, py, w, h):
    u, v = (px + 0.5) / w, (py + 0.5) / h
    if name == "front":
        return face_front(px, py, w, h)
    if name == "bottom":
        return "C"
    if name == "top":
        return "S" if abs(u - 0.5) < 0.3 and (py % 3 == 0) and v > 0.3 else "G"
    if name in ("left", "right"):
        front = 1 - u if name == "left" else u
        if v > 0.6 and front < 0.45:
            return "C"
        return "S" if py % 4 == 1 and v < 0.6 else "G"
    return "S" if py % 4 == 1 else "G"


def ear_face(name, px, py, w, h):
    u, v = (px + 0.5) / w, (py + 0.5) / h
    if name in ("front", "back"):
        half = 0.5 * v ** 0.75 + 0.05
        if abs(u - 0.5) > half:
            return "."
        if name == "front" and abs(u - 0.5) < half - 0.2 and v > 0.3:
            return "I"
        return "G" if v > 0.2 else "S"
    if name == "top":
        return "."
    if name == "bottom":
        return "G"
    return "G" if v > 0.6 else "."


def body_face(name, px, py, w, h):
    u, v = (px + 0.5) / w, (py + 0.5) / h
    if name == "bottom":
        return "C"
    if name == "front":
        return "C" if abs(u - 0.5) < 0.3 else "G"
    if name == "top":
        return "S" if py % 3 == 0 else "G"
    return "C" if v > 0.75 else tabby(px, py)


def bib_face(name, px, py, w, h):
    return "c" if name == "front" and py == h - 1 and px % 2 else "C"


def paw_face(name, px, py, w, h):
    return "C" if name != "bottom" else "I"


def tail_face(dark):
    def face(name, px, py, w, h):
        return "S" if dark else "G"

    return face


def whisker_face(name, px, py, w, h):
    return "C"


def spring(stiffness, damping, inertia=1.0, **drive):
    p = {"stiffness": stiffness, "damping": damping, "inertia": inertia}
    if drive:
        p["drive"] = drive
    return p


def bezier(pts, t):
    p = list(pts)
    while len(p) > 1:
        p = [tuple(lerp(a, b, t) for a, b in zip(p[k], p[k + 1])) for k in range(len(p) - 1)]
    return p[0]


def build():
    it = Item("shoulder_cat", "Shoulder Cat", "back", ["pet", "cat", "animated"], [], texel=4, size=256)
    cx = 7.0
    hop = 0.3

    body = it.bone("body", pivot=(cx, 24.0, 0.0), anim={"type": "bob", "axis": "y", "amplitude": 0.12, "speed": 0.3, "phase": 1.0},
                   physics=spring(90, 10, 0.5, hop=hop, sprint=[6, 0, 0]))
    body.box((cx - 1.7, 24.0, -1.9), (3.4, 3.0, 3.8), box_painter(body_face, 1))
    body.box((cx - 1.1, 24.4, 1.8), (2.2, 1.8, 0.6), box_painter(bib_face, 2, 0.03))
    for x in (cx - 1.45, cx + 0.55):
        body.box((x, 23.3, 1.7), (0.9, 1.2, 0.9), box_painter(paw_face, 3), key="paw")

    head = it.bone("head", pivot=(cx, 26.4, 1.2), rot=(0, 0, -8), anim={"type": "sway", "axis": "y", "amplitude": 5, "speed": 0.14, "phase": 2.0},
                   physics=spring(70, 9, 0.6, hop=hop))
    head.box((cx - 2.7, 26.0, -0.8), (5.4, 4.6, 4.4), box_painter(head_face, 4))
    for x in (cx - 3.2, cx + 2.4):
        head.box((x, 26.0, 1.0), (0.8, 1.2, 2.2), box_painter(lambda n, *a: "C" if n != "top" else "G", 5), key="cheek")
    for side in (-1, 1):
        for k, y in enumerate((26.9, 27.7)):
            x = cx + side * 2.5 - (1.5 if side < 0 else 0)
            head.box((x, y - 0.35 * k, 3.35), (1.5, 0.2, 0.2), box_painter(whisker_face, 6), key="whisker")

    for side, dx, phase in (("l", -1.75, 0.6), ("r", 1.75, 2.9)):
        ear = it.bone("ear_" + side, pivot=(cx + dx, 30.5, 1.0), rot=(0, 0, 14 if dx < 0 else -14),
                      anim={"type": "twitch", "axis": "z", "amplitude": 20 if dx < 0 else -20, "speed": 0.5, "phase": phase},
                      physics=spring(160, 12, 0.4, hop=hop, sprint=[-18, 0, 0], air=[-12, 0, 0], sneak=[-10, 0, 0]))
        ear.box((cx + dx - 1.05, 30.4, 0.7), (2.1, 2.2, 0.6), box_painter(ear_face, 7), key="ear")

    tail = it.bone("tail", pivot=(cx, 24.6, -2.0), anim={"type": "sway", "axis": "z", "amplitude": 9, "speed": 0.28, "phase": 0},
                   physics=spring(34, 3.8, 1.3, hop=hop, sprint=[12, 0, 0], air=[10, 0, 0]))
    curve = [(cx - 0.2, 24.2), (cx + 0.9, 18.5), (cx + 0.2, 13.0), (cx - 3.4, 13.4), (cx - 3.6, 16.6)]
    n = 34
    for i in range(n):
        x, y = bezier(curve, i / (n - 1))
        s = 1.25 - 0.2 * (i / (n - 1))
        dark = i >= n - 5 or i % 7 in (2, 3)
        tail.box((x - s / 2, y - s / 2, -2.55 - s / 2), (s, s, s), box_painter(tail_face(dark), 10))
    return it
