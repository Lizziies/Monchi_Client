import math

import numpy as np

from animal_shoes import noise, rect
from item import Item
from shoes import TEXEL
from paint import Art
from shoes import box_uv

SIZES = (1.0, 0.72, 0.5)


def stem(face, art):
    v = art.Y / max(art.h, 1e-6)
    side = face in ("front", "back", "left", "right")
    g = 236 - 40 * v ** 2 if side else 236
    art.put(np.ones(art.X.shape), g * (1 - 0.06 * noise(art, 0.1)))
    if side:
        art.put((np.abs(np.mod(art.X, 0.38) - 0.19) < 0.03).astype(float), 196, 0.6)


def cap(face, art):
    side = face in ("front", "back", "left", "right")
    v = art.Y / max(art.h, 1e-6)
    base = 250 - (60 * v if side else 0)
    if face == "bottom":
        base = 205 + 0 * art.X
        art.put(np.ones(art.X.shape), base)
        art.put((np.abs(np.mod(art.X + art.Y, 0.32) - 0.16) < 0.04).astype(float), 160, 0.8)
        return
    art.put(np.ones(art.X.shape), base * (1 - 0.07 * noise(art, 0.12)))
    if face == "top":
        edge = np.minimum.reduce([art.X, art.w - art.X, art.Y, art.h - art.Y])
        art.put((edge < 0.18).astype(float), 200, 0.6)


def spot(face, art):
    art.put(np.ones(art.X.shape), 255)
    edge = np.minimum.reduce([art.X, art.w - art.X, art.Y, art.h - art.Y])
    art.put((edge < 0.05).astype(float), 225, 0.9)


MUSHROOMS = (
    (6.6, 1.0, 1.0, 0), (7.8, -2.6, 0.5, 1), (5.9, -4.2, 0.72, 2),
    (-6.8, 0.8, 0.72, 3), (-5.9, -3.4, 1.0, 4), (-7.9, 2.6, 0.5, 5),
    (1.6, 8.4, 0.72, 6), (-2.4, -8.3, 0.72, 7), (-0.8, 9.6, 0.5, 8),
)


def mushrooms():
    it = Item("mushroom_underglow", "Mushroom Underglow", "aura", ["aura", "mushrooms", "nature", "animated"],
              [("Cap", "#d12a2a"), ("Spots", "#fff3e2"), ("Stem", "#dcc6a2")], texel=TEXEL, size=256)
    uv = {}
    for s in SIZES:
        uv[s] = {
            "stem": box_uv(it, (1.1 * s, 1.3 * s, 1.1 * s), stem),
            "cap": box_uv(it, (2.9 * s, 0.6 * s, 2.9 * s), cap),
            "top": box_uv(it, (2.0 * s, 0.45 * s, 2.0 * s), cap),
            "spot": box_uv(it, (0.7 * s, 0.1, 0.7 * s), spot),
        }
    sway = {"stiffness": 20.0, "damping": 2.2, "inertia": 1.6}
    for x, z, s, n in MUSHROOMS:
        u = uv[s]
        sh, ch, th = 1.3 * s, 0.6 * s, 0.45 * s
        axis = "x" if n % 2 else "z"
        drive = {"air": [0, 0, 0], "sprint": [6, 0, 0], "speed": [4, 0, 0], "sneak": [-3, 0, 0]}
        bone = it.bone(f"shroom{n}", (x, 0.0, z), anim={"type": "sway", "axis": axis, "amplitude": 7.5 - 2.5 * s, "speed": round(0.32 + 0.06 * n, 2), "phase": round(n * 1.3, 2)},
                       physics={"type": "spring", **sway, "drive": drive})
        it.box(bone, (x - 0.55 * s, 0.03, z - 0.55 * s), (1.1 * s, sh - 0.03, 1.1 * s), u["stem"], "Stem")
        it.box(bone, (x - 1.45 * s, sh, z - 1.45 * s), (2.9 * s, ch, 2.9 * s), u["cap"], "Cap")
        it.box(bone, (x - 1.0 * s, sh + ch, z - 1.0 * s), (2.0 * s, th, 2.0 * s), u["top"], "Cap")
        top = sh + ch + th
        for dx, dz in ((-0.45, 0.35), (0.4, -0.45)):
            it.box(bone, (x + dx * s - 0.35 * s, top, z + dz * s - 0.35 * s), (0.7 * s, 0.1, 0.7 * s), u["spot"], "Spots")
        it.box(bone, (x + 0.9 * s - 0.3 * s, sh + ch - 0.02, z - 1.0 * s - 0.3 * s + 0.2), (0.6 * s, 0.1, 0.6 * s), u["spot"], "Spots")
    mote = box_uv(it, (0.3, 0.3, 0.3), spot)
    for n, (x, z, y) in enumerate(((6.4, 0.6, 3.2), (-6.3, -2.6, 3.6), (5.6, -4.0, 2.8), (-7.2, 2.2, 3.0), (1.2, 8.2, 3.3), (-2.0, -8.0, 3.5))):
        bone = it.bone(f"mote{n}", (x, y, z), anim={"type": "float", "axis": "y", "amplitude": 0.9, "speed": round(0.22 + 0.05 * n, 2), "phase": round(n * 1.9, 2)})
        it.box(bone, (x - 0.15, y - 0.15, z - 0.15), (0.3, 0.3, 0.3), mote, "Spots")
    return it


def ring(radius, y, n=64, width=0.28, height=0.12):
    pts = []
    for i in range(n + 1):
        a = 2 * math.pi * i / n
        pts.append((radius * math.sin(a), y, radius * math.cos(a), width, height, 0.5 + 0.5 * math.cos(a)))
    return pts


def neon():
    it = Item("neon_underglow", "Neon Underglow", "aura", ["aura", "neon", "ring", "animated"],
              [("Glow", "#2ee6ff"), ("Glow2", "#a24bff")], texel=TEXEL, size=64)
    outer = it.bone("ring_outer", (0, 0.14, 0), anim={"type": "spin", "axis": "y", "amplitude": 0, "speed": 0.22, "phase": 0})
    it.tube(outer, ring(6.4, 0.14), "Glow", "Glow2", sides=8, power=2.4, capped=False)
    inner = it.bone("ring_inner", (0, 0.1, 0), anim={"type": "spin", "axis": "y", "amplitude": 0, "speed": -0.34, "phase": 0})
    it.tube(inner, ring(5.5, 0.1, width=0.14, height=0.08), "Glow2", "Glow", sides=8, power=2.4, capped=False)
    spark = box_uv(it, (0.5, 0.5, 0.5), spot)
    orbit = it.bone("sparks", (0, 0.5, 0), anim={"type": "spin", "axis": "y", "amplitude": 0, "speed": 0.3, "phase": 0})
    for a in (0, 90, 180, 270):
        x, z = 6.4 * math.sin(math.radians(a)), 6.4 * math.cos(math.radians(a))
        it.box(orbit, (x - 0.25, 0.25, z - 0.25), (0.5, 0.5, 0.5), spark, "Glow" if a % 180 == 0 else "Glow2")
    return it


def build():
    return [mushrooms(), neon()]
