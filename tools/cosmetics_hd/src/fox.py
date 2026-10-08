import math
import random

from lib import Item, clamp, hexrgb, lerp

PALETTE = {
    "O": hexrgb("#f47f2a"),
    "D": hexrgb("#d9601f"),
    "L": hexrgb("#ffa257"),
    "C": hexrgb("#fff6ea"),
    "c": hexrgb("#f3e1cf"),
    "K": hexrgb("#2a181d"),
    "B": hexrgb("#7a4330"),
    "b": hexrgb("#a35d3c"),
    "W": hexrgb("#ffffff"),
    "P": hexrgb("#ff8fa8"),
    "p": hexrgb("#ffc0cd"),
    "N": hexrgb("#24161a"),
    "S": hexrgb("#5a3529"),
    "I": hexrgb("#ffb8c2"),
}

EYE = [
    ".KKKK.",
    "KWWKKK",
    "KWWKKK",
    "KKKKKK",
    "KKKKKK",
    "KBBBWK",
    "KbBBBK",
    ".KKKK.",
]

FLAT = set("KWBbNPp")


def shade(c, k):
    return tuple(clamp(v * k, 0, 255) for v in c)


def mix(a, b, t):
    return tuple(lerp(x, y, t) for x, y in zip(a, b))


class Grain:
    def __init__(self, seed):
        rng = random.Random(seed)
        self.g = [[rng.random() for _ in range(64)] for _ in range(64)]

    def __call__(self, x, y, amount):
        return 1 - amount / 2 + amount * self.g[y % 64][x % 64]


def light(name, v):
    if name == "top":
        return 1.07
    if name == "bottom":
        return 0.8
    return 1.06 - 0.16 * v


def box_painter(face_fn, seed, grain=0.08):
    noise = Grain(seed)

    def run(canvas, faces):
        for name, (x, y, w, h) in faces.items():
            for py in range(h):
                for px in range(w):
                    c = face_fn(name, px, py, w, h)
                    if c is None or c == ".":
                        continue
                    if isinstance(c, str):
                        if c in FLAT:
                            canvas.set(x + px, y + py, (*PALETTE[c], 255))
                            continue
                        c = PALETTE[c]
                    k = light(name, (py + 0.5) / h) * noise(x + px, y + py, grain)
                    canvas.set(x + px, y + py, (*shade(c, k), 255))

    return run


def face_front(px, py, w, h):
    for ex in (3, w - 9):
        dx, dy = px - ex, py - 8
        if 0 <= dx < 6 and 0 <= dy < len(EYE) and EYE[dy][dx] != ".":
            return EYE[dy][dx]
    for bx in (2, w - 6):
        if 0 <= px - bx < 4 and py == 16:
            return "P"
        if 0 < px - bx < 3 and py in (15, 17):
            return "p"
    mid = abs(px - (w - 1) / 2)
    side = min(px, w - 1 - px)
    if py >= 17:
        return "C"
    if py >= 12 and side < (py - 11) * 1.1:
        return "C"
    if py >= 14 and mid < 4:
        return "C"
    if py <= 2:
        return "L" if mid < 6 - py * 1.5 else "O"
    if 6 <= py <= 13 and mid < 1.2:
        return "L"
    return "O"


def head_face(name, px, py, w, h):
    u, v = (px + 0.5) / w, (py + 0.5) / h
    if name == "front":
        return face_front(px, py, w, h)
    if name == "bottom":
        return "C"
    if name in ("left", "right"):
        front = 1 - u if name == "left" else u
        return "C" if v > 0.6 and front < 0.5 else "O"
    if name == "top":
        return "L" if abs(u - 0.5) < 0.18 and v > 0.5 else "O"
    return "O"


def snout_face(name, px, py, w, h):
    v = (py + 0.5) / h
    mid = abs(px - (w - 1) / 2)
    if name == "front":
        if py == 0 and mid < 1.6:
            return "N"
        if py == 1 and mid < 0.6:
            return "N"
        if py == h - 1 and 0.5 < mid < 1.6:
            return "B"
        return "C"
    if name == "top":
        return "N" if v > 0.7 and mid < 1.6 else "C"
    return "C"


def ear_face(name, px, py, w, h):
    u, v = (px + 0.5) / w, (py + 0.5) / h
    if name in ("front", "back"):
        half = 0.5 * v ** 0.8 + 0.05
        if abs(u - 0.5) > half:
            return "."
        if v < 0.3:
            return "S"
        if name == "front":
            inner = half - 0.17
            if abs(u - 0.5) < inner and v > 0.4:
                return "C" if v > 0.78 else "I"
        return "O"
    if name == "top":
        return "."
    if name == "bottom":
        return "O"
    return "O" if v > 0.62 else "."


def body_face(name, px, py, w, h):
    u, v = (px + 0.5) / w, (py + 0.5) / h
    if name == "top":
        return "D" if abs(u - 0.5) < 0.14 else "O"
    if name == "bottom":
        return "C"
    if name == "front":
        return "C" if abs(u - 0.5) < 0.32 else "O"
    if name in ("left", "right"):
        return "C" if v > 0.72 else "O"
    return "O"


def bib_face(name, px, py, w, h):
    if name == "front" and py == h - 1 and px % 2:
        return "c"
    return "C"


def paw_face(name, px, py, w, h):
    if name == "bottom":
        return "S"
    return "S" if (py + 0.5) / h > 0.4 else "D"


def tuft_face(name, px, py, w, h):
    return "L" if name == "top" else "C"


def tail_face(white_from):
    def face(name, px, py, w, h):
        u, v = (px + 0.5) / w, (py + 0.5) / h
        if v >= white_from:
            return "C"
        if name == "bottom":
            return "C" if white_from <= 1 else "O"
        streak = 0.5 + 0.5 * math.sin(px * 1.3 + py * 0.7)
        return mix(PALETTE["O"], PALETTE["D"], 0.28 * streak)

    return face


def spring(stiffness, damping, inertia=1.0, **drive):
    p = {"stiffness": stiffness, "damping": damping, "inertia": inertia}
    if drive:
        p["drive"] = drive
    return p


def build():
    it = Item("shoulder_fox", "Shoulder Fox", "back", ["pet", "fox", "animated"], [], texel=4, size=256)
    cx = -7.0
    hop = 0.32

    body = it.bone("body", pivot=(cx, 24.0, 0.0), anim={"type": "bob", "axis": "y", "amplitude": 0.14, "speed": 0.35, "phase": 0},
                   physics=spring(90, 10, 0.5, hop=hop, sprint=[6, 0, 0]))
    body.box((cx - 1.8, 24.0, -2.2), (3.6, 2.8, 4.2), box_painter(body_face, 1))
    body.box((cx - 1.2, 24.5, 1.9), (2.4, 1.9, 0.7), box_painter(bib_face, 5, 0.04))
    for x in (cx - 1.5, cx + 0.6):
        body.box((x, 23.2, 2.0), (0.9, 1.3, 0.9), box_painter(paw_face, 10), key="paw")

    head = it.bone("head", pivot=(cx, 26.4, 1.4), rot=(0, 0, 9), anim={"type": "sway", "axis": "y", "amplitude": 5, "speed": 0.16, "phase": 0.7},
                   physics=spring(70, 9, 0.6, hop=hop))
    head.box((cx - 3.0, 26.0, -0.6), (6.0, 5.0, 4.6), box_painter(head_face, 2))
    head.box((cx - 0.9, 26.25, 4.0), (1.8, 1.0, 0.6), box_painter(snout_face, 3, 0.03))
    head.box((cx - 0.35, 31.0, 1.2), (0.7, 0.9, 0.7), box_painter(lambda *a: "O", 6))
    for x in (cx - 3.6, cx + 2.7):
        head.box((x, 26.0, 1.4), (0.9, 1.4, 2.4), box_painter(tuft_face, 4), key="tuft")

    for side, dx, phase in (("l", -1.95, 0.0), ("r", 1.95, 2.4)):
        ear = it.bone("ear_" + side, pivot=(cx + dx, 30.9, 1.4), rot=(0, 0, 12 if dx < 0 else -12),
                      anim={"type": "twitch", "axis": "z", "amplitude": 18 if dx < 0 else -18, "speed": 0.45, "phase": phase},
                      physics=spring(160, 12, 0.4, hop=hop, sprint=[-20, 0, 0], air=[-12, 0, 0], sneak=[-10, 0, 0]))
        ear.box((cx + dx - 1.3, 30.8, 1.0), (2.6, 3.0, 0.8), box_painter(ear_face, 20), key="ear")

    tail = it.bone("tail", pivot=(cx, 25.2, -2.4), rot=(0, 0, 36), anim={"type": "sway", "axis": "z", "amplitude": 7, "speed": 0.35, "phase": 0},
                   physics=spring(46, 5.0, 1.0, hop=hop, sprint=[8, 0, 0], air=[10, 0, 0], speed=[3, 0, 0]))
    y = 25.6
    widths = (2.4, 3.0, 3.5, 3.9, 4.1, 4.0, 3.6, 3.0, 2.2, 1.2)
    depths = (1.8, 2.2, 2.6, 2.9, 3.0, 2.9, 2.6, 2.2, 1.6, 1.0)
    whites = (2, 2, 2, 2, 2, 2, 0.45, 0, 0, 0)
    for i, (w, d, white) in enumerate(zip(widths, depths, whites)):
        h = 1.6
        y -= h - (0.45 if i else 0)
        tail.box((cx - w / 2, y, -2.1 - d), (w, h, d), box_painter(tail_face(white), 30 + i))
    return it
