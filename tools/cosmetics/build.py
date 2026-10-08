"""Builds the Monchi cosmetics (item.json) without Blender.

The items are made of smooth swept tubes (ears, tails, hats, cloth) instead of boxes.
Wings and capes come from tools/cosmetics_hd and get copied into the same folder.

Usage: python3 tools/cosmetics/build.py [output dir]
"""
import json
import math
import os
import shutil
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "cosmetics")


def lerp(a, b, t):
    return a + (b - a) * t


def smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def bezier(pts, n):
    out = []
    for i in range(n + 1):
        t = i / n
        p = list(pts)
        while len(p) > 1:
            p = [tuple(lerp(a, b, t) for a, b in zip(p[k], p[k + 1])) for k in range(len(p) - 1)]
        out.append(p[0])
    return out


class Item:
    def __init__(self, id, name, slot, tints):
        self.id, self.name, self.slot, self.tints = id, name, slot, tints
        self.bones = []

    def bone(self, name, pivot=(0, 0, 0), anim=None, rotation=(0, 0, 0)):
        b = {"name": name, "pivot": list(pivot), "rotation": list(rotation), "tubes": []}
        if anim:
            b["anim"] = anim
        self.bones.append(b)
        return b

    def tube(self, bone, path, tint, tint2=None, sides=16, power=2.0, capped=True, wave=None, two_sided=False):
        t = {"tint": tint, "sides": sides, "path": [[round(v, 3) for v in p] for p in path]}
        if tint2:
            t["tint2"] = tint2
        if power != 2.0:
            t["power"] = power
        if not capped:
            t["capped"] = False
        if two_sided:
            t["two_sided"] = True
        if wave:
            t["wave"] = wave
        bone["tubes"].append(t)

    def save(self):
        d = os.path.join(OUT, self.id)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        data = {"id": self.id, "name": self.name, "slot": self.slot, "tint": [{"name": n, "default": c} for n, c in self.tints], "bones": self.bones}
        with open(os.path.join(d, "item.json"), "w") as f:
            json.dump(data, f, separators=(",", ":"))


def cone(base, tip, r0, rx_scale, rz_scale, n=7, curve=1.2, tip_r=0.0):
    pts = []
    for i in range(n + 1):
        t = i / n
        p = [lerp(a, b, t) for a, b in zip(base, tip)]
        r = r0 * (1 - t ** curve) + tip_r * t
        pts.append((p[0], p[1], p[2], max(0.04, r * rx_scale), max(0.04, r * rz_scale)))
    return pts


def sphere(c, r, n=9, sx=1.0, sz=1.0):
    pts = []
    for i in range(n + 1):
        a = math.pi * i / n
        pts.append((c[0], c[1] - math.cos(a) * r, c[2], max(0.03, math.sin(a) * r * sx), max(0.03, math.sin(a) * r * sz)))
    return pts


def twitch(speed, phase, amp=14):
    return {"type": "twitch", "axis": "z", "amplitude": amp, "speed": speed, "phase": phase}


def cat_ears():
    it = Item("mochi_cat_ears", "Cat Ears", "head", [("Fur", "#f4e6d8"), ("Inner", "#ff9ec2")])
    for side, x, phase in (("l", 2.4, 0.0), ("r", -2.4, 2.1)):
        out = 1 if x > 0 else -1
        b = it.bone("ear_" + side, (x, 31.6, -0.4), twitch(0.55, phase, 9 * out * -1))
        base, tip = (x, 31.5, -0.4), (x + 1.1 * out, 36.6, -0.1)
        it.tube(b, cone(base, tip, 2.0, 1.0, 0.5, curve=1.5), "Fur")
        inner_base, inner_tip = (x, 31.7, 0.1), (x + 1.0 * out, 35.7, 0.25)
        it.tube(b, cone(inner_base, inner_tip, 1.3, 1.0, 0.45, curve=1.5), "Inner")
    return it


def dog_ears():
    it = Item("mochi_dog_ears", "Dog Ears", "head", [("Fur", "#b98458"), ("Tips", "#6e4a30")])
    for side, out, phase in (("l", 1, 0.0), ("r", -1, 1.4)):
        pivot = (3.6 * out, 31.7, -0.6)
        b = it.bone("ear_" + side, pivot, {"type": "sway", "axis": "z", "amplitude": 7, "speed": 0.7, "phase": phase})
        curve = bezier([(3.4 * out, 31.8, -0.6), (5.6 * out, 32.6, -0.6), (7.4 * out, 29.5, -0.6), (7.2 * out, 25.4, -0.6)], 10)
        pts = []
        for i, p in enumerate(curve):
            t = i / 10
            r = 1.9 * math.sin(math.pi * (0.12 + 0.82 * t) ) ** 0.7
            pts.append((p[0], p[1], p[2], max(0.1, r * 0.5), max(0.1, r * 1.1), t))
        it.tube(b, pts, "Fur", "Tips", sides=14)
    return it


def tail_path(points, r0, r1, bulge, n=16, tip_round=0.35):
    curve = bezier(points, n)
    pts = []
    for i, p in enumerate(curve):
        t = i / n
        r = lerp(r0, r1, t) + bulge * math.sin(math.pi * t)
        if t > 1 - 0.12:
            r *= max(tip_round, math.cos((t - (1 - 0.12)) / 0.12 * math.pi / 2) ** 0.6)
        pts.append((p[0], p[1], p[2], r, r, smooth((t - 0.62) / 0.38)))
    return pts


def cat_tail():
    it = Item("mochi_cat_tail", "Cat Tail", "body", [("Fur", "#f4e6d8"), ("Tip", "#8a6a58")])
    b = it.bone("tail", (0, 13.2, -2.1), {"type": "sway", "axis": "y", "amplitude": 9, "speed": 0.45, "phase": 0})
    path = tail_path([(0, 13.2, -2.1), (0, 11.2, -5.5), (0, 14.5, -9.5), (0, 20.5, -7.5)], 1.2, 0.85, 0.12)
    it.tube(b, path, "Fur", "Tip", sides=14, wave={"axis": "x", "amplitude": 1.6, "speed": 0.55, "freq": 1.6})
    return it


def dog_tail():
    it = Item("mochi_dog_tail", "Dog Tail", "body", [("Fur", "#b98458"), ("Tip", "#f1e2cf")])
    b = it.bone("tail", (0, 13.4, -2.0), {"type": "wag", "axis": "y", "amplitude": 24, "speed": 2.1, "phase": 0})
    path = tail_path([(0, 13.4, -2.0), (0, 15.2, -5.4), (0, 19.2, -6.6), (0, 23.2, -4.6)], 1.3, 0.4, 1.2, tip_round=0.5)
    it.tube(b, path, "Fur", "Tip", sides=14, wave={"axis": "x", "amplitude": 0.7, "speed": 2.1, "freq": 1.0})
    return it


def bandana():
    it = Item("mochi_bandana", "Bandana", "face", [("Cloth", "#ff4f93"), ("Accent", "#ffe1ee")])
    b = it.bone("band", (0, 29, 0))
    it.tube(b, [(0, 28.2, 0, 4.42, 4.42), (0, 30.2, 0, 4.42, 4.42)], "Cloth", sides=28, power=7.0)
    it.tube(b, [(0, 29.55, 0, 4.47, 4.47), (0, 29.95, 0, 4.47, 4.47)], "Accent", sides=28, power=7.0)
    it.tube(b, sphere((2.2, 29.2, -4.6), 1.15, sx=1.0, sz=0.85), "Cloth", sides=12)
    for i, (x0, drift, phase) in enumerate(((2.0, 1.4, 0.0), (2.6, -0.6, 1.2))):
        wb = it.bone("tail%d" % i, (2.2, 29.0, -4.8), {"type": "sway", "axis": "z", "amplitude": 6, "speed": 0.8, "phase": phase})
        curve = bezier([(2.2, 29.0, -4.9), (x0 + 0.4, 26.5, -5.9 - i * 0.4), (x0 + drift, 23.6, -6.4), (x0 + drift * 1.4, 20.8, -6.1)], 12)
        pts = []
        for k, p in enumerate(curve):
            t = k / 12
            w = lerp(0.95, 0.55, t) if t < 0.85 else lerp(0.55, 0.0, (t - 0.85) / 0.15) + 0.05
            pts.append((p[0], p[1], p[2], w * 1.35, 0.08, t))
        it.tube(wb, pts, "Cloth", "Accent", sides=6, wave={"axis": "z", "amplitude": 1.4, "speed": 0.9, "freq": 2.2}, two_sided=True, capped=False)
    return it


def beanie():
    it = Item("mochi_beanie", "Beanie", "head", [("Main", "#5aa9ff"), ("Cuff", "#f4f4ff"), ("Pom", "#ffffff")])
    b = it.bone("hat", (0, 32, 0))
    dome = []
    for i in range(0, 9):
        a = math.radians(i * 80 / 8)
        dome.append((0, 31.6 + 4.2 * math.sin(a), 0, 4.65 * math.cos(a) ** 0.8 + 0.04, 4.65 * math.cos(a) ** 0.8 + 0.04, i / 8))
    it.tube(b, dome, "Main", "Cuff", sides=28, power=3.0)
    it.tube(b, [(0, 30.7, 0, 4.85, 4.85), (0, 31.1, 0, 5.0, 5.0), (0, 33.2, 0, 5.0, 5.0), (0, 33.7, 0, 4.82, 4.82)], "Cuff", sides=28, power=4.0)
    pb = it.bone("pom", (0, 36.4, 0), {"type": "bob", "axis": "y", "amplitude": 0.35, "speed": 0.8, "phase": 0})
    it.tube(pb, sphere((0, 36.9, 0), 1.7, n=10), "Pom", sides=14)
    return it


def wizard_hat():
    it = Item("mochi_wizard_hat", "Wizard Hat", "head", [("Main", "#6a3fd1"), ("Band", "#ffc94d"), ("Tip", "#b48cff")])
    b = it.bone("brim", (0, 32, 0))
    it.tube(b, [(0, 31.9, 0, 7.6, 7.6), (0, 32.4, 0, 7.7, 7.7), (0, 32.8, 0, 6.2, 6.2), (0, 33.2, 0, 4.6, 4.6)], "Main", sides=32)
    it.tube(b, [(0, 33.0, 0, 4.75, 4.75), (0, 34.5, 0, 4.55, 4.55)], "Band", sides=28)
    cb = it.bone("cone", (0, 34, 0), {"type": "sway", "axis": "z", "amplitude": 3, "speed": 0.5, "phase": 0})
    curve = bezier([(0, 33.2, 0), (0, 38, 0.2), (1.2, 42.2, -0.8), (3.8, 44.4, -3.0)], 14)
    pts = []
    for i, p in enumerate(curve):
        t = i / 14
        r = 4.5 * (1 - t) ** 0.95 + 0.07
        pts.append((p[0], p[1], p[2], r, r, t))
    it.tube(cb, pts, "Main", "Tip", sides=24, wave={"axis": "z", "amplitude": 0.9, "speed": 0.5, "freq": 1.2})
    return it


def sneakers():
    it = Item("mochi_sneakers", "Sneakers", "feet", [("Main", "#ff6fa8"), ("Sole", "#ffffff"), ("Accent", "#2a2630")])
    for side, x, phase in (("l", 2.0, 0.0), ("r", -2.0, math.pi)):
        b = it.bone("foot_" + side, (x, 3.4, 0), {"type": "sway", "axis": "x", "amplitude": 5, "speed": 0.9, "phase": phase})
        sole = [(x, 0.7, -2.45, 0.2, 0.2), (x, 0.7, -2.2, 1.55, 0.6), (x, 0.7, -1.2, 2.2, 0.7), (x, 0.7, 2.4, 2.2, 0.7), (x, 0.7, 3.5, 1.7, 0.66), (x, 0.7, 3.95, 0.9, 0.5)]
        it.tube(b, sole, "Sole", sides=14, power=4.0)
        toe = [(x, 1.95, -0.5, 1.9, 1.3), (x, 1.95, 1.0, 2.0, 1.35), (x, 1.75, 2.6, 1.9, 1.15), (x, 1.55, 3.6, 1.35, 0.8), (x, 1.5, 3.9, 0.5, 0.4)]
        it.tube(b, toe, "Main", sides=14, power=2.6)
        ankle = [(x, 0.9, 0, 2.28, 2.3), (x, 3.4, 0, 2.25, 2.28), (x, 4.1, 0, 2.3, 2.34)]
        it.tube(b, ankle, "Main", "Accent", sides=18, power=4.0)
        for k in range(3):
            z = 0.9 + 0.8 * k
            it.tube(b, [(x - 1.15, 2.7 - 0.12 * k, z, 0.12, 0.12), (x + 1.15, 2.7 - 0.12 * k, z, 0.12, 0.12)], "Sole", sides=6)
    return it


def copy_hd():
    root = os.path.join(os.path.dirname(__file__), "..", "cosmetics_hd")
    for name in sorted(os.listdir(root)):
        src = os.path.join(root, name)
        if not os.path.isfile(os.path.join(src, "item.json")):
            continue
        dst = os.path.join(OUT, name)
        shutil.rmtree(dst, ignore_errors=True)
        os.makedirs(dst)
        for f in ("item.json", "tex.png"):
            if os.path.isfile(os.path.join(src, f)):
                shutil.copy(os.path.join(src, f), dst)


def write_index():
    items = []
    for name in sorted(os.listdir(OUT)):
        path = os.path.join(OUT, name, "item.json")
        if os.path.isfile(path):
            with open(path) as f:
                j = json.load(f)
            items.append({"id": j["id"], "name": j["name"], "slot": j["slot"]})
    with open(os.path.join(OUT, "index.json"), "w") as f:
        json.dump({"items": items}, f, indent=1)


def main():
    os.makedirs(OUT, exist_ok=True)
    for make in (cat_ears, dog_ears, cat_tail, dog_tail, bandana, beanie, wizard_hat, sneakers):
        make().save()
    copy_hd()
    write_index()
    print("built cosmetics")


main()
