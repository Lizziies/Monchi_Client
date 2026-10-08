import json
import pathlib

import numpy as np
from PIL import Image

import clearance

LIMIT = 80


class Item:
    def __init__(self, id, name, slot, tags, tints, texel=6, size=256):
        self.id, self.name, self.slot, self.tags, self.tints = id, name, slot, tags, tints
        self.texel, self.size = texel, size
        self.tex = np.zeros((size, size, 4), np.uint8)
        self.cursor = [1, 1, 0]
        self.bones = []

    def place(self, px):
        h, w = px.shape[:2]
        x, y, row = self.cursor
        if x + w + 1 > self.size:
            x, y, row = 1, y + row + 2, 0
        if y + h + 2 > self.size:
            raise RuntimeError(f"{self.id}: texture atlas is full")
        self.tex[y : y + h, x : x + w] = px
        self.cursor = [x + w + 1, y, max(row, h)]
        return x, y

    def bone(self, name, pivot, rotation=(0, 0, 0), anim=None, physics=None, parent=None):
        b = {"name": name, "pivot": [round(v, 3) for v in pivot], "rotation": [round(v, 3) for v in rotation]}
        if parent:
            b["parent"] = parent
        if anim:
            b["anim"] = anim
        if physics:
            b["physics"] = physics
        self.bones.append(b)
        return b

    def card(self, bone, origin, size, uv, tint, mirror=False):
        c = {"origin": [round(v, 3) for v in origin], "size": [round(v, 3) for v in size], "uv": list(uv), "flat": True, "tint": tint}
        if mirror:
            c["mirror"] = True
        bone.setdefault("cubes", []).append(c)

    def box(self, bone, origin, size, uv, tint):
        bone.setdefault("cubes", []).append({"origin": [round(v, 3) for v in origin], "size": [round(v, 3) for v in size], "uv": list(uv), "tint": tint})

    def tube(self, bone, path, tint, tint2=None, sides=16, power=2.0, capped=True, wave=None):
        t = {"tint": tint, "sides": sides, "path": [[round(v, 3) for v in p] for p in path]}
        if tint2:
            t["tint2"] = tint2
        if power != 2.0:
            t["power"] = power
        if not capped:
            t["capped"] = False
        if wave:
            t["wave"] = wave
        bone.setdefault("tubes", []).append(t)

    def cubes(self):
        return sum(len(b.get("cubes", [])) for b in self.bones)

    def write(self, root):
        if self.cubes() > LIMIT:
            raise RuntimeError(f"{self.id}: {self.cubes()} cubes, the limit is {LIMIT}")
        folder = pathlib.Path(root) / self.id
        folder.mkdir(parents=True, exist_ok=True)
        doc = {"id": self.id, "name": self.name, "slot": self.slot, "tags": self.tags}
        textured = self.cubes() > 0
        if textured:
            doc["texture"] = "tex.png"
            doc["texel"] = self.texel
        doc["tint"] = [{"name": n, "default": c} for n, c in self.tints]
        doc["bones"] = self.bones
        (folder / "item.json").write_text(json.dumps(doc, indent=1) + "\n")
        if textured:
            used = self.cursor[1] + self.cursor[2] + 2
            h = 1 << max(5, (used - 1).bit_length())
            Image.fromarray(self.tex[: min(h, self.size)], "RGBA").save(folder / "tex.png", optimize=True)
        return folder


LAG = 0.12
# two cards on one pivot come apart visibly once their angles differ by more than a few degrees, in the air the flap
# is doubled; the lagging card may therefore swing at most this much wider, and it rides the same spring
FOLLOW = 1.06
# the client scales a flap by up to 1.15 in walk and 2.1 in the air, a spring overshoots ~15%; the wing has to
# stay CLEAR degrees behind the body plane at the far end of the forward stroke, and the air drive moves the
# extra jump stroke backwards instead of into the arms
SWING = 1.15 * 1.15
CLEAR = 7.0


def wing_pair(item, layers, root, pivot=(2.2, 20.8, -2.6), sweep=22.0, lift=4.0, flap=13.0, speed=0.5, gap=0.09, physics=None, follow=1.3):
    """Layers are (art, tint) or (art, tint, "arm"). Every layer is a full card hanging from the same root.

    Arm layers ride the leading bone; the rest ride a second bone at the same pivot that swings wider and a
    little later, which reads as tip lag without any piece being able to separate from the root.
    """
    first = layers[0][0]
    w, h = first.w, first.h
    rx, ry = root
    cache = {}
    for art, *_ in layers:
        if id(art) not in cache:
            cache[id(art)] = item.place(art.pixels())
    placed = [(cache[id(l[0])], l[1], l[2] if len(l) > 2 else "feathers") for l in layers]
    groups = [g for g in ("feathers", "arm") if any(p[2] == g for p in placed)]
    single = len(groups) == 1
    reach = flap * (1 if single else follow) * SWING
    sweep = max(sweep, reach + CLEAR)
    base = physics or {"stiffness": 46.0, "damping": 6.0, "inertia": 1.1, "drive": {"air": [0, round(reach * 1.1, 1), 14], "sprint": [0, 20, 0], "speed": [0, 5, 0], "sneak": [0, 10, -4]}}
    start = len(item.bones)
    tries = sorted(((d, x) for d in np.arange(0, 3.01, 0.2) for x in (0, 0.2, 0.4, 0.6)), key=lambda t: t[0] + 1.5 * t[1])
    for drop, out in tries:
        del item.bones[start:]
        at = (pivot[0] + out, round(pivot[1] - drop, 2), pivot[2])
        for side, label in ((1, "r"), (-1, "l")):
            for group in groups:
                lagging = group == "feathers" and not single
                phys = dict(base)
                phys = phys | {"type": "spring", "drive": {k: [v[0], side * v[1], side * v[2]] for k, v in base["drive"].items()}}
                anim = {"type": "flap", "axis": "y", "amplitude": round(side * flap * (min(follow, FOLLOW) if lagging else 1), 2), "speed": speed, "phase": -LAG if lagging else 0.0}
                bone = item.bone(f"{group}_{label}", (side * at[0], at[1], at[2]), (0, side * sweep, side * lift), anim=anim, physics=phys)
                for i, (uv, tint, g) in enumerate(placed):
                    if g != group:
                        continue
                    x = at[0] - rx if side > 0 else -at[0] + rx - w
                    z = at[2] - 0.02 - i * gap
                    item.card(bone, (x, at[1] + ry - h, z), (w, h, 0.04), uv, tint, mirror=side < 0)
        if not clearance.check(item):
            break
    item.wing_pivot = at
    return item
