from lib import Item, feather, region_painter

ROWS = [
    # name, count, length (first, last), width, fan angle (first, last), height offset, depth, tint
    ("primary", 10, (17.5, 11.0), 4.6, (66.0, -16.0), 0.0, -3.0, "Main"),
    ("secondary", 8, (12.0, 7.5), 4.0, (54.0, -4.0), -0.5, -3.2, "Main"),
    ("covert", 9, (7.0, 3.6), 3.3, (76.0, 8.0), 0.9, -3.4, "Accent"),
]


def blob(u, v):
    d = ((u - 0.5) ** 2 + (v - 0.5) ** 2) ** 0.5 * 2
    if d > 1:
        return None
    return (236 - 40 * d, 1.0 if d < 0.92 else (1 - d) / 0.08)


def build():
    it = Item("angel_wings", "Angel Wings", "wings", ["wings", "white", "feathers", "animated"], [("Main", "#ffffff"), ("Accent", "#ffe7a3")], texel=4, size=256)
    for side, label in ((1, "r"), (-1, "l")):
        for name, count, (l0, l1), width, (a0, a1), dy, z, tint in ROWS:
            for i in range(count):
                t = i / (count - 1)
                length = l0 + (l1 - l0) * t
                angle = a0 + (a1 - a0) * t
                pivot = (side * 2.4, 21.2 + dy, z - 0.012 * i)
                bone = it.bone(
                    f"{name}_{label}{i}",
                    pivot=pivot,
                    rot=(0, side * (16 + 10 * t), side * angle),
                    anim={"type": "flap", "axis": "y", "amplitude": side * (11 - 4 * t), "speed": 0.55, "phase": 0.16 * i + (0.4 if name == "covert" else 0)},
                    physics={"stiffness": 74 - 30 * t, "damping": 6.5 - 1.5 * t, "inertia": 0.8 + 0.8 * t,
                             "drive": {"air": [0, 0, side * 14], "sprint": [0, side * 16, 0], "speed": [0, side * 4, 0]}},
                )
                lt, wt = round(length * it.texel), round(width * it.texel)
                shape = feather(lt, wt, tip=0.1, curve=0.05 * (1 if i % 2 else -1), seed=7 + i * 13 + len(name))
                ox = pivot[0] if side > 0 else pivot[0] - length
                bone.flat((ox, pivot[1] - width / 2, pivot[2]), (length, width, 0.1), region_painter(shape), key=(name, i), tint=tint, mirror=side < 0)
        root = it.bone(f"root_{label}", pivot=(side * 2.4, 21.2, -3.5), rot=(0, side * 14, 0), anim={"type": "flap", "axis": "y", "amplitude": side * 7, "speed": 0.55, "phase": 0.0},
                    physics={"stiffness": 70, "damping": 6.5, "drive": {"sprint": [0, side * 16, 0]}})
        ox = 2.4 - 0.6 if side > 0 else -2.4 - 4.4
        root.flat((ox, 19.6, -3.5), (5.0, 3.6, 0.1), region_painter(blob), key="root", tint="Accent", mirror=side < 0)
    return it
