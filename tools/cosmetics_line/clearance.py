"""Checks that no visible texel of a cosmetic ends up inside the player in any pose the client can produce."""
import math

import numpy as np

MARGIN = 0.15
BODY = {
    "head": ((-4, 24, -4), (4, 32, 4)),
    "torso": ((-4, 12, -2), (4, 24, 2)),
    "arm_r": ((4, 12, -2), (8, 24, 2)),
    "arm_l": ((-8, 12, -2), (-4, 24, 2)),
    "leg_r": ((0, 0, -2), (4, 12, 2)),
    "leg_l": ((-4, 0, -2), (0, 12, 2)),
}
# an item may rest on the part it is worn on, but not sink deeper than INSET into it
WORN_ON = {"head": "head", "face": "head", "body": "torso", "waist": "torso", "back": "torso", "shoulder": "torso", "feet": ("leg_r", "leg_l")}
INSET = 0.8
# fwd speed share, sprint, air, sneak: mirrors animTarget/springStep in tools/cosmetics/preview.html
POSES = {"idle": (0, 0, 0, 0), "walk": (0.77, 0, 0, 0), "sprint": (1.0, 1, 0, 0), "jump": (0.27, 0, 1, 0), "sneak": (0.3, 0, 0, 1)}
OVERSHOOT = 1.15


def rotation(deg):
    x, y, z = (math.radians(v) for v in deg)
    rx = np.array([[1, 0, 0], [0, math.cos(x), -math.sin(x)], [0, math.sin(x), math.cos(x)]])
    ry = np.array([[math.cos(y), 0, math.sin(y)], [0, 1, 0], [-math.sin(y), 0, math.cos(y)]])
    rz = np.array([[math.cos(z), -math.sin(z), 0], [math.sin(z), math.cos(z), 0], [0, 0, 1]])
    return rz @ ry @ rx


def box_points(item, cube):
    """Points on the faces of a solid cube; feet items skip the top faces and every bottom above the ground, they sit inside the leg."""
    ox, oy, oz = cube["origin"]
    sx, sy, sz = cube["size"]
    xs, ys, zs = (np.linspace(0, s, max(2, int(s / 0.5) + 1)) for s in (sx, sy, sz))
    pts = []
    for y in ys:
        for x in xs:
            pts += [(x, y, 0), (x, y, sz)]
        for z in zs:
            pts += [(0, y, z), (sx, y, z)]
    feet = item.slot == "feet"
    for x in xs:
        for z in zs:
            if not feet or oy < 0.5:
                pts.append((x, 0, z))
            if not feet:
                pts.append((x, sy, z))
    return np.array(pts, float) + np.array([ox, oy, oz])


def card_points(item, cube):
    if not cube.get("flat"):
        return box_points(item, cube)
    k = item.texel
    w, h = cube["size"][0], cube["size"][1]
    pw, ph = round(w * k), round(h * k)
    u, v = cube["uv"]
    alpha = item.tex[v : v + ph, u : u + pw, 3]
    py, px = np.nonzero(alpha > 76)
    fx = (px + 0.5) / k
    if cube.get("mirror"):
        fx = w - fx
    ox, oy, oz = cube["origin"]
    return np.stack([ox + fx, oy + h - (py + 0.5) / k, np.full(fx.shape, oz + cube["size"][2] / 2)], axis=1)


def tube_points(tube):
    """Centre line plus six points on each ring, at the smaller radius, so the check stays on the safe side."""
    out = []
    for x, y, z, rx, rz, *_ in tube["path"]:
        r = min(rx, rz)
        out += [(x, y, z), (x + r, y, z), (x - r, y, z), (x, y + r, z), (x, y - r, z), (x, y, z + r), (x, y, z - r)]
    return np.array(out, float)


def poses(bone):
    rot = np.array(bone["rotation"], float)
    anim = bone.get("anim") or {}
    phys = bone.get("physics") if isinstance(bone.get("physics"), dict) else {}
    drive = (phys or {}).get("drive", {})
    axis = {"x": 0, "y": 1, "z": 2}.get(anim.get("axis", "y"), 1)
    for name, (speed, sprint, air, sneak) in POSES.items():
        offset = np.zeros(3)
        for key, f in (("speed", speed), ("sprint", sprint), ("air", air), ("sneak", sneak)):
            offset += f * np.array(drive.get(key, [0, 0, 0]), float)
        swings = [0.0]
        if anim.get("type") in ("flap", "sway", "wag"):
            gain = 1 + 1.1 * air + 0.15 * speed if anim["type"] == "flap" else 1 + 0.6 * speed
            a = anim["amplitude"] * gain * OVERSHOOT
            swings = [-a, -a * 0.5, 0.0, a * 0.5, a]
        for s in swings:
            r = rot + offset
            r[axis] += s
            yield name, r


def check(item):
    """Every bone is tested in every pose; a child follows its parent's pose with the same name and swing step."""
    hits = {}
    worn = WORN_ON.get(item.slot, ())
    worn = (worn,) if isinstance(worn, str) else worn
    names = [b.get("name", "") for b in item.bones]
    parents = [names.index(b["parent"]) if b.get("parent") in names else -1 for b in item.bones]
    table = []
    for bone in item.bones:
        by = {}
        for name, r in poses(bone):
            by.setdefault(name, []).append(r)
        table.append(by)

    def place(i, pts, name, k):
        bone = item.bones[i]
        rs = table[i][name]
        pivot = np.array(bone["pivot"], float)
        pts = (pts - pivot) @ rotation(rs[min(k, len(rs) - 1)]).T + pivot
        return place(parents[i], pts, name, k) if parents[i] >= 0 else pts

    for i, bone in enumerate(item.bones):
        parts = [card_points(item, c) for c in bone.get("cubes", [])] + [tube_points(t) for t in bone.get("tubes", [])]
        if not parts:
            continue
        pts = np.concatenate(parts)
        for name in POSES:
            steps = max(len(table[j][name]) for j in range(len(item.bones)))
            for k in range(steps):
                world = place(i, pts, name, k)
                for part, (lo, hi) in BODY.items():
                    pad = -INSET if part in worn else MARGIN
                    lo, hi = np.array(lo) - pad, np.array(hi) + pad
                    inside = np.all((world > lo) & (world < hi), axis=1)
                    if inside.any():
                        key = (part, name)
                        hits[key] = max(hits.get(key, 0), int(inside.sum()))
    return hits
