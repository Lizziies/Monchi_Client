import math

import numpy as np

from item import Item
from paint import Art


def ring(radius, y, rx, rz, n=40, shine=0.0):
    pts = []
    for i in range(n + 2):
        a = 2 * math.pi * i / n
        mix = shine * (0.5 + 0.5 * math.cos(a - 2.2)) ** 3
        pts.append((radius * math.sin(a), y, radius * math.cos(a), rx, rz, mix))
    return pts


def halo():
    it = Item("halo", "Halo", "head", ["head", "halo", "gold", "animated"], [("Gold", "#ffcf5a"), ("Shine", "#fff8e2")], texel=8, size=32)
    bone = it.bone(
        "ring", (0, 35.2, 0), (-9, 0, 0),
        anim={"type": "float", "axis": "y", "amplitude": 0.7, "speed": 0.45, "phase": 0},
        physics={"type": "spring", "stiffness": 26.0, "damping": 3.6, "inertia": 1.2, "drive": {"air": [-10, 0, 0], "sprint": [-14, 0, 0], "speed": [-5, 0, 0]}},
    )
    it.tube(bone, ring(3.35, 35.2, 0.3, 0.46, shine=1.0), "Gold", "Shine", sides=12, power=2.6, capped=False)
    it.tube(bone, ring(2.9, 35.2, 0.09, 0.2, shine=0.0), "Shine", sides=8, capped=False)
    star = Art(1.6, 1.6, 8)
    dx, dy = np.abs(star.X - 0.8), np.abs(star.Y - 0.8)
    shape = (dx * dy < 0.035) & (dx + dy < 0.8)
    star.put(shape.astype(float), 255 - 90 * np.clip(np.hypot(dx, dy) / 0.8, 0, 1))
    uv = it.place(star.pixels())
    glints = it.bone("glints", (0, 35.2, 0), anim={"type": "sparkle", "axis": "y", "amplitude": 1, "speed": 0.55, "phase": 0})
    for a, y, size in ((40, 35.9, 1.2), (165, 34.7, 0.9), (285, 35.6, 1.05)):
        x, z = 3.6 * math.sin(math.radians(a)), 3.6 * math.cos(math.radians(a))
        it.card(glints, (x - size / 2, y - size / 2, z), (size, size, 0.02), uv, "Shine")
    return it


def build():
    return [halo()]
