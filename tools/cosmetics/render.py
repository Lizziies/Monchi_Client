"""Renders cosmetics on a player in Blender.

Usage: blender -b -P tools/cosmetics/render.py -- <out dir> <item dir> [<item dir> ...] [--views front,side,back,three,close]
       blender -P tools/cosmetics/render.py -- <out dir> <item dir> ... --live
       blender -b -P tools/cosmetics/render.py -- <out dir> <item dir> ... --anim back [--frames 60,200]

--live opens a window instead of rendering, plays the idle/walk/sprint/jump/sneak loop and
reloads the items whenever their files change. --anim renders that loop to <out dir>/<cam>.mp4,
or only the listed frames as stills.

The model is read from item.json and tex.png exactly as the client reads it, and bones move
with the same spring and cloth model as the client preview.
"""
import json
import math
import os
import sys

import bpy
from mathutils import Euler, Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
live = "--live" in argv
argv = [a for a in argv if a != "--live"]
anim_cam = None
stills = []
if "--frames" in argv:
    k = argv.index("--frames")
    stills = [int(f) for f in argv[k + 1].split(",")]
    argv = argv[:k] + argv[k + 2:]
if "--anim" in argv:
    k = argv.index("--anim")
    anim_cam = argv[k + 1]
    argv = argv[:k] + argv[k + 2:]
views = ["front", "three", "side", "back", "close"]
if "--views" in argv:
    k = argv.index("--views")
    views = argv[k + 1].split(",")
    argv = argv[:k] + argv[k + 2:]
out_dir, item_dirs = argv[0], argv[1:]
os.makedirs(out_dir, exist_ok=True)

if live:
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob)
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
items = bpy.data.collections.new("items")
scene.collection.children.link(items)

root = bpy.data.objects.new("mc", None)
scene.collection.objects.link(root)
root.rotation_euler = (math.radians(90), 0, 0)
FPS = 30
limbs = {}
records = []


def hexcol(s):
    s = s.lstrip("#")
    return [int(s[i:i + 2], 16) / 255 for i in (0, 2, 4)]


def srgb(c):
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def flat_material(name, rgb, rough=0.75):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*srgb(rgb), 1)
    bsdf.inputs["Roughness"].default_value = rough
    return m


def tex_material(name, image, tint, blended):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.85
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = image
    tex.interpolation = "Closest"
    mul = nt.nodes.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    mul.inputs[7].default_value = (*srgb(tint), 1)
    nt.links.new(tex.outputs["Color"], mul.inputs[6])
    nt.links.new(mul.outputs[2], bsdf.inputs["Base Color"])
    nt.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
    if blended and hasattr(m, "surface_render_method"):
        m.surface_render_method = "BLENDED"
    return m


def empty(name, parent, loc, coll):
    ob = bpy.data.objects.new(name, None)
    coll.objects.link(ob)
    ob.parent = parent
    ob.location = loc
    ob.rotation_mode = "XYZ"
    return ob


body = empty("body", root, (0, 0, 0), scene.collection)


def mesh_object(name, verts, faces, uvs, mat, parent, coll):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    layer = me.uv_layers.new()
    for poly in me.polygons:
        for li, vi in zip(poly.loop_indices, range(len(poly.loop_indices))):
            layer.data[li].uv = uvs[poly.index][vi]
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    ob.parent = parent
    return ob


def box(name, size, center, mat, parent):
    sx, sy, sz = (v / 2 for v in size)
    cx, cy, cz = center
    v = [(cx + x * sx, cy + y * sy, cz + z * sz) for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)]
    f = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    uv = [[(0, 0)] * 4 for _ in f]
    return mesh_object(name, v, f, uv, mat, parent, scene.collection)


def region_uv(c, region, texel):
    w, h, d = c["uvs"]
    u, v = c["uv"]
    if c.get("flat"):
        if region > 1:
            return (u + 0.5, v + h + 0.5, u + 0.5, v + h + 0.5)
        u0, v0, u1, v1 = u, v, u + w, v + h
        if (region == 1) != bool(c.get("mirror")):
            u0, u1 = u1, u0
    else:
        u0, v0, u1, v1 = {
            4: (u + d, v, u + d + w, v + d),
            5: (u + d + w, v, u + d + 2 * w, v + d),
            3: (u, v + d, u + d, v + d + h),
            0: (u + d, v + d, u + d + w, v + d + h),
            2: (u + d + w, v + d, u + 2 * d + w, v + d + h),
        }.get(region, (u + 2 * d + w, v + d, u + 2 * d + 2 * w, v + d + h))
    if texel > 1 and abs(u1 - u0) > 1 and abs(v1 - v0) > 1:
        s = 0.5 if u1 > u0 else -0.5
        u0, u1, v0, v1 = u0 + s, u1 - s, v0 + 0.5, v1 - 0.5
    return u0, v0, u1, v1


def cube_object(name, c, base, texel, tw, th, mat, parent):
    ox, oy, oz = (c["origin"][i] - base[i] for i in range(3))
    sx, sy, sz = c["size"]
    if c.get("flat"):
        sz = max(sz, 0.02)
    x0, y0, z0, x1, y1, z1 = ox, oy, oz, ox + sx, oy + sy, oz + sz
    quads = [
        [(x0, y1, z1), (x1, y1, z1), (x1, y0, z1), (x0, y0, z1)],
        [(x1, y1, z0), (x0, y1, z0), (x0, y0, z0), (x1, y0, z0)],
        [(x1, y1, z1), (x1, y1, z0), (x1, y0, z0), (x1, y0, z1)],
        [(x0, y1, z0), (x0, y1, z1), (x0, y0, z1), (x0, y0, z0)],
        [(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)],
        [(x0, y0, z1), (x1, y0, z1), (x1, y0, z0), (x0, y0, z0)],
    ]
    verts, faces, uvs = [], [], []
    for i, q in enumerate(quads):
        u0, v0, u1, v1 = region_uv(c, i, texel)
        b = len(verts)
        verts += q
        faces.append((b, b + 3, b + 2, b + 1))
        corner = [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]
        uvs.append([(cu / tw, 1 - cv / th) for cu, cv in (corner[0], corner[3], corner[2], corner[1])])
    return mesh_object(name, verts, faces, uvs, mat, parent, items)


def physics_of(ph, kind):
    po = ph if isinstance(ph, dict) else {}
    drive = po.get("drive", {})
    cloth = kind == "cloth"
    p = {
        "stiffness": min(400, max(1, po.get("stiffness", 38 if cloth else 60))),
        "damping": min(60, max(0.1, po.get("damping", 2.4 if cloth else 7))),
        "inertia": min(5, max(0, po.get("inertia", 1))),
        "wind": min(4, max(0, po.get("wind", 1))),
        "hop": min(4, max(0, drive.get("hop", 0))),
    }
    for k in ("air", "sprint", "sneak", "speed"):
        p[k] = drive.get(k, [0, 0, 0])
    return p


def load_item(folder):
    with open(os.path.join(folder, "item.json")) as f:
        j = json.load(f)
    texel = max(1.0, min(8.0, j.get("texel", 1)))
    tints = [hexcol(t["default"]) for t in j.get("tint", [])]
    names = [t["name"] for t in j.get("tint", [])]
    image = None
    tex = os.path.join(folder, j.get("texture", "tex.png"))
    if os.path.isfile(tex):
        image = bpy.data.images.load(os.path.abspath(tex), check_existing=False)
    tw, th = image.size if image else (1, 1)
    alpha = list(image.pixels)[3::4] if image else []

    def translucent(c):
        w, h, d = c["uvs"]
        u, v = c["uv"]
        rw, rh = (w, h + 1) if c.get("flat") else (2 * d + 2 * w, d + h)
        for y in range(int(v), min(th, int(v + rh))):
            row = (th - 1 - y) * tw
            for x in range(int(u), min(tw, int(u + rw))):
                if 0.02 < alpha[row + x] < 0.98:
                    return True
        return False

    mats = {}
    for b in j["bones"]:
        pivot = b.get("pivot", [0, 0, 0])
        bone = empty(j["id"] + ":" + b["name"], body, pivot, items)
        bone.rotation_euler = Euler([math.radians(a) for a in b.get("rotation", [0, 0, 0])], "XYZ")
        ph = b.get("physics")
        kind = ph if isinstance(ph, str) else ph.get("type", "spring") if isinstance(ph, dict) else ""
        cubes = [dict(c) for c in b.get("cubes", [])]
        if kind == "cloth":
            cubes.sort(key=lambda c: -(c["origin"][1] + c["size"][1]))
        rec = {"bone": b, "ob": bone, "kind": kind, "order": cubes, "joints": [], "phys": physics_of(ph, kind)}
        records.append(rec)
        parent, prev = bone, pivot
        for n, c in enumerate(cubes):
            size = c["size"]
            c["uvs"] = c.get("uvsize") or [round(size[0] * texel), round(size[1] * texel), round(size[2] * texel)]
            tint = [1, 1, 1]
            if c.get("tint") in names:
                tint = tints[names.index(c["tint"])]
                if c.get("tint2") in names:
                    t2 = tints[names.index(c["tint2"])]
                    tint = [a + (b2 - a) * c.get("mix", 0) for a, b2 in zip(tint, t2)]
            blended = bool(image) and translucent(c)
            key = (tuple(round(x, 3) for x in tint), blended)
            if key not in mats:
                mats[key] = tex_material(j["id"], image, tint, blended) if image else flat_material(j["id"], tint)
            if kind == "cloth":
                joint = [c["origin"][0] + size[0] / 2, c["origin"][1] + size[1], c["origin"][2] + size[2] / 2]
                parent = empty(f"{b['name']}_joint{n}", parent, [joint[i] - prev[i] for i in range(3)], items)
                rec["joints"].append(parent)
                prev = joint
                cube_object(f"{b['name']}{n}", c, joint, texel, tw, th, mats[key], parent)
            else:
                cube_object(f"{b['name']}{n}", c, pivot, texel, tw, th, mats[key], bone)


def player():
    skin = flat_material("skin", hexcol("#e8b48f"))
    shirt = flat_material("shirt", hexcol("#4b3fc4"))
    trim = flat_material("trim", hexcol("#3fd6d0"))
    pants = flat_material("pants", hexcol("#2b2470"))
    hair = flat_material("hair", hexcol("#3a2a24"))
    eye = flat_material("eye", hexcol("#2a1d24"))
    white = flat_material("white", hexcol("#ffffff"))
    shoe = flat_material("shoe", hexcol("#1d1a2c"))
    box("head", (8, 8, 8), (0, 28, 0), skin, body)
    box("hair", (8.3, 2.6, 8.3), (0, 31, 0), hair, body)
    box("hair_back", (8.3, 6, 1.2), (0, 28.5, -3.6), hair, body)
    for x in (-2, 2):
        box("eye", (1.6, 1.8, 0.1), (x, 28.0, 4.05), eye, body)
        box("shine", (0.6, 0.6, 0.12), (x - 0.4, 28.5, 4.07), white, body)
    box("torso", (8, 12, 4), (0, 18, 0), shirt, body)
    box("stripe", (8.1, 1.2, 4.1), (0, 15.5, 0), trim, body)
    box("collar", (4, 1, 4.1), (0, 23.4, 0), trim, body)
    for side, name in ((1, "l"), (-1, "r")):
        arm = limbs["arm_" + name] = empty("arm_" + name, body, (side * 6, 24, 0), scene.collection)
        box("arm", (4, 12, 4), (0, -6, 0), skin, arm)
        box("sleeve", (4.1, 4, 4.1), (0, -2, 0), shirt, arm)
        leg = limbs["leg_" + name] = empty("leg_" + name, body, (side * 2, 12, 0), scene.collection)
        box("leg", (4, 12, 4), (0, -6, 0), pants, leg)
        box("shoe", (4.1, 2.5, 4.1), (0, -10.8, 0), shoe, leg)


def stage():
    floor = bpy.data.meshes.new("floor")
    floor.from_pydata([(-200, -200, 0), (200, -200, 0), (200, 200, 0), (-200, 200, 0)], [], [(0, 1, 2, 3)])
    ob = bpy.data.objects.new("floor", floor)
    scene.collection.objects.link(ob)
    floor.materials.append(flat_material("floor", hexcol("#2a2f3a"), 0.9))
    world = bpy.data.worlds.new("world")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (*srgb(hexcol("#1b2130")), 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6

    def light(name, kind, loc, energy, size, color="#ffffff"):
        data = bpy.data.lights.new(name, kind)
        data.energy = energy
        data.color = srgb(hexcol(color))
        if kind == "AREA":
            data.size = size
        ob = bpy.data.objects.new(name, data)
        ob.location = loc
        scene.collection.objects.link(ob)
        ob.rotation_euler = (Vector((0, 0, 16)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()

    light("key", "AREA", (-30, -40, 50), 90000, 30, "#fff2e2")
    light("fill", "AREA", (40, -30, 25), 30000, 40, "#cfe0ff")
    light("rim", "AREA", (10, 45, 40), 60000, 25, "#ffd9f0")
    light("back", "AREA", (-35, 55, 30), 45000, 35, "#fff2e2")


CAMS = {
    "front": ((0, -70, 22), (0, 0, 18), 50),
    "three": ((-45, -55, 30), (-2, 0, 20), 50),
    "side": ((-72, 0, 24), (0, 0, 18), 50),
    "back": ((20, 68, 26), (0, 0, 18), 50),
    "close": ((-28, -26, 30), (-6.5, 0, 26), 70),
    "face": ((-12, -34, 29), (-6.4, -1, 27), 80),
    "cape": ((8, 52, 17), (0, 0, 14.5), 55),
    "walk": ((-62, 48, 34), (-2, 0, 20), 45),
    "catface": ((14, -34, 29), (6.8, -1, 27), 80),
    "catback": ((-24, 50, 22), (4, 0, 19), 55),
    "catwalk": ((62, 48, 34), (2, 0, 20), 45),
}


def camera(name):
    loc, target, lens = CAMS[name]
    cam = bpy.data.cameras.new(name)
    cam.lens = lens
    ob = bpy.data.objects.new(name, cam)
    scene.collection.objects.link(ob)
    ob.location = loc
    ob.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    scene.camera = ob


def render(name):
    camera(name)
    scene.render.filepath = os.path.join(out_dir, name + ".png")
    bpy.ops.render.render(write_still=True)


def smoothstep(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


# time, speed in blocks per second, sprint, sneak
SCRIPT = ((0.0, 0.0, False, False), (1.5, 4.3, False, False), (4.5, 5.6, True, False), (9.8, 4.3, False, False),
          (11.8, 1.3, False, True), (14.0, 0.0, False, False), (16.0, 0.0, False, False))
JUMPS = (7.6, 8.7)


def moving(t):
    fwd, sprint, sneak = 0.0, False, False
    for (t0, f0, s0, n0), (t1, f1, _, _) in zip(SCRIPT, SCRIPT[1:]):
        if t0 <= t < t1:
            fwd = f0 + (f1 - f0) * smoothstep(t1 - 0.4, t1, t)
            sprint, sneak = s0, n0
    height, up, air = 0.0, 0.0, False
    for j in JUMPS:
        p = (t - j) / 0.6
        if 0 <= p < 1:
            height, up, air = 1.25 * 4 * p * (1 - p), 1.25 * 4 * (1 - 2 * p) / 0.6, True
    return {"fwd": fwd, "side": 0.0, "up": up, "turn": 0.0, "sprint": sprint, "sneak": sneak, "air": air}, height


class Rig:
    def __init__(self):
        self.prev = None
        self.accF = self.accS = self.accU = 0.0
        self.air = self.sprint = self.sneak = 0.0
        self.clock = self.gait = 0.0
        self.dt = 1 / FPS
        self.m = None

    def follow(self, k, target, rate, dt):
        setattr(self, k, getattr(self, k) + (target - getattr(self, k)) * min(1.0, dt * rate))

    def step(self, dt, m):
        if self.prev:
            self.follow("accF", (m["fwd"] - self.prev["fwd"]) / dt, 12, dt)
            self.follow("accS", (m["side"] - self.prev["side"]) / dt, 12, dt)
            self.follow("accU", (m["up"] - self.prev["up"]) / dt, 12, dt)
        self.follow("air", 1.0 if m["air"] else 0.0, 9, dt)
        self.follow("sprint", 1.0 if m["sprint"] else 0.0, 6, dt)
        self.follow("sneak", 1.0 if m["sneak"] else 0.0, 8, dt)
        self.prev, self.m, self.dt = dict(m), m, dt
        self.clock += dt
        self.gait += dt * (11 if m["sprint"] else 7.5) * min(1.0, m["fwd"] / 3)


AXIS = {"x": 0, "y": 1, "z": 2}


def anim_target(b, phys, phase, r):
    out = list(b.get("rotation", [0, 0, 0]))
    move = [0.0, 0.0, 0.0]
    a = b.get("anim") or {}
    kind, ax, amp, ph = a.get("type", ""), AXIS.get(a.get("axis", "y"), 1), a.get("amplitude", 0), a.get("phase", 0)
    speed = min(1.2, max(0.0, r.m["fwd"] / 5.6))
    if kind == "flap":
        out[ax] += amp * (1 + 1.1 * r.air + 0.15 * speed) * math.sin(phase + ph)
    elif kind in ("sway", "wag"):
        out[ax] += amp * (1 + 0.6 * speed) * math.sin(phase + ph)
    elif kind == "twitch":
        out[ax] += amp * max(0.0, math.sin(phase * 0.5 + ph)) ** 8 * max(0.0, math.sin(phase * 3))
    elif kind == "spin":
        out[ax] += phase * 57.29578 + ph
    elif kind == "bob":
        move[1] = amp * math.sin(phase + ph)
    elif kind == "float":
        move[1] = amp * math.sin(phase + ph)
        move[0] = amp * 0.3 * math.sin(phase * 0.5 + ph)
    for i in range(3):
        out[i] += r.air * phys["air"][i] + r.sprint * phys["sprint"][i] + r.sneak * phys["sneak"][i] + speed * phys["speed"][i]
    if phys["hop"] > 0:
        move[1] += phys["hop"] * (abs(math.sin(r.gait)) * min(1.0, r.m["fwd"] / 3) * (1 + 0.6 * r.sprint) + 1.6 * r.air)
    return out, move


def spring_step(rec, s, r, h):
    target, _ = anim_target(rec["bone"], rec["phys"], s["phase"], r)
    p = rec["phys"]
    kick = (-8 * r.accF + 2 * r.accU, -1.5 * r.m["turn"], 8 * r.accS)
    for i in range(3):
        acc = p["stiffness"] * (target[i] - s["ang"][i]) - p["damping"] * s["vel"][i] + p["inertia"] * kick[i]
        s["vel"][i] += acc * h
        s["ang"][i] += s["vel"][i] * h


def cloth_step(rec, s, r, h):
    p, order = rec["phys"], rec["order"]
    n = len(order)
    th, om, ph, op = s["th"], s["om"], s["ph"], s["op"]
    wind = min(60, max(0, r.m["fwd"] * 5.2)) * p["wind"] + min(35, max(0, -r.m["up"] * 3)) + r.sneak * 7
    gravity, drag, link = p["stiffness"], p["damping"], 22
    for i in range(n):
        frac = i / (n - 1) if n > 1 else 0
        target = math.radians(wind) * (0.3 + 0.7 * frac)
        gust = 0.35 * math.sin(r.clock * 2.3 + i * 0.8) * (0.25 + min(1, r.m["fwd"] / 4))
        up, down = th[i - 1] if i else 0, th[i + 1] if i + 1 < n else th[i]
        acc = -gravity * math.sin(th[i] - target) - drag * om[i] + link * (up - th[i]) + link * 0.6 * (down - th[i])
        acc += p["inertia"] * (r.accF * 0.09 + r.accU * 0.03) * (0.4 + frac) + gust
        om[i] += acc * h
        up_r, down_r = ph[i - 1] if i else 0, ph[i + 1] if i + 1 < n else ph[i]
        acc_r = -gravity * 0.8 * math.sin(ph[i]) - drag * op[i] + link * (up_r - ph[i]) + link * 0.6 * (down_r - ph[i])
        acc_r += p["inertia"] * (r.accS * 0.09 - r.m["turn"] * 0.004) * (0.4 + frac) + gust * 0.4
        op[i] += acc_r * h
    z = 0.0
    for i in range(n):
        th[i] = min(1.9, max(-1.2, th[i] + om[i] * h))
        ph[i] = min(0.9, max(-0.9, ph[i] + op[i] * h))
        ln = order[i]["size"][1]
        nz = z - ln * math.sin(th[i])
        if nz > 0.35:
            th[i] = -math.asin(min(1, max(-1, (0.35 - z) / ln)))
            if om[i] > 0:
                om[i] *= -0.2
            nz = 0.35
        z = nz


def key(ob, path, value, frame):
    setattr(ob, path, value)
    ob.keyframe_insert(path, frame=frame)


def animate():
    r = Rig()
    sims = {}
    frames = int(SCRIPT[-1][0] * FPS)
    for f in range(1, frames + 1):
        m, height = moving((f - 1) / FPS)
        r.step(1 / FPS, m)
        swing = math.radians(48 if m["sprint"] else 32) * min(1.0, m["fwd"] / 3) * (1 - r.air)
        g = r.gait
        key(body, "location", (0, height * 8 - 1.2 * r.sneak, 0), f)
        key(limbs["leg_l"], "rotation_euler", (math.sin(g) * swing, 0, 0), f)
        key(limbs["leg_r"], "rotation_euler", (-math.sin(g) * swing, 0, 0), f)
        key(limbs["arm_l"], "rotation_euler", (-math.sin(g) * swing * 0.9 - r.air * 2.2, 0, 0), f)
        key(limbs["arm_r"], "rotation_euler", (math.sin(g) * swing * 0.9 - r.air * 2.2, 0, 0), f)
        for rec in records:
            b, n = rec["bone"], len(rec["order"])
            s = sims.get(id(rec))
            if s is None:
                s = sims[id(rec)] = {"ang": anim_target(b, rec["phys"], 0.0, r)[0], "vel": [0.0] * 3, "phase": 0.0,
                                     "th": [0.0] * n, "om": [0.0] * n, "ph": [0.0] * n, "op": [0.0] * n}
            a = b.get("anim") or {}
            rate = a.get("speed", 1) * ((1 + 2 * r.air + 0.4 * r.sprint) if a.get("type") == "flap" else 1)
            steps = max(1, math.ceil(r.dt * 120))
            h = r.dt / steps
            for _ in range(steps):
                s["phase"] += h * 2 * math.pi * rate
                if rec["kind"] == "cloth":
                    cloth_step(rec, s, r, h)
                elif rec["kind"] == "spring":
                    spring_step(rec, s, r, h)
            target, move = anim_target(b, rec["phys"], s["phase"], r)
            pose = s["ang"] if rec["kind"] == "spring" else target
            pivot = b.get("pivot", [0, 0, 0])
            key(rec["ob"], "location", [pivot[i] + move[i] for i in range(3)], f)
            key(rec["ob"], "rotation_euler", [math.radians(v) for v in pose], f)
            for k, joint in enumerate(rec["joints"]):
                pt, pp = (s["th"][k - 1], s["ph"][k - 1]) if k else (0.0, 0.0)
                key(joint, "rotation_euler", (s["th"][k] - pt, 0, s["ph"][k] - pp), f)
    scene.render.fps = FPS
    scene.frame_start, scene.frame_end = 1, frames
    scene.frame_set(1)


def stamp():
    t = 0.0
    for d in item_dirs:
        for f in ("item.json", "tex.png"):
            p = os.path.join(d, f)
            if os.path.isfile(p):
                t = max(t, os.path.getmtime(p))
    return t


def reload_items():
    records.clear()
    for ob in list(items.objects):
        data = ob.data
        bpy.data.objects.remove(ob)
        if data is not None and data.users == 0:
            bpy.data.meshes.remove(data)
    for m in [m for m in bpy.data.materials if m.users == 0]:
        bpy.data.materials.remove(m)
    for im in [im for im in bpy.data.images if im.users == 0]:
        bpy.data.images.remove(im)
    for d in item_dirs:
        try:
            load_item(d)
        except (OSError, ValueError, KeyError) as e:
            print("could not load", d, e)
    animate()


def watch():
    t = stamp()
    if t != watch.seen:
        watch.seen = t
        reload_items()
    return 0.5


def setup_view():
    loc, target, _ = CAMS["walk"]
    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type != "VIEW_3D":
                continue
            space = area.spaces.active
            space.shading.type = "RENDERED"
            space.overlay.show_floor = False
            space.overlay.show_axis_x = False
            space.overlay.show_axis_y = False
            space.overlay.show_relationship_lines = False
            space.overlay.show_extras = False
            r3d = space.region_3d
            r3d.view_perspective = "PERSP"
            r3d.view_location = Vector(target)
            r3d.view_distance = (Vector(loc) - Vector(target)).length
            r3d.view_rotation = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y")
            if not bpy.context.screen or not bpy.context.screen.is_animation_playing:
                with bpy.context.temp_override(window=window, area=area):
                    bpy.ops.screen.animation_play()
    return None


player()
stage()
scene.render.engine = "BLENDER_EEVEE" if "BLENDER_EEVEE" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE_NEXT"
scene.eevee.taa_render_samples = 64
scene.render.resolution_x = 900
scene.render.resolution_y = 900
scene.view_settings.view_transform = "Standard"
scene.view_settings.exposure = -1.1
if live:
    watch.seen = stamp()
    reload_items()
    bpy.app.timers.register(watch, first_interval=0.5, persistent=True)
    bpy.app.timers.register(setup_view, first_interval=0.3)
elif anim_cam:
    for d in item_dirs:
        load_item(d)
    animate()
    camera(anim_cam)
    for f in stills:
        scene.frame_set(f)
        scene.render.filepath = os.path.join(out_dir, f"{anim_cam}_{f:04d}.png")
        bpy.ops.render.render(write_still=True)
    if stills:
        sys.exit(0)
    scene.eevee.taa_render_samples = 16
    scene.render.resolution_x = scene.render.resolution_y = 720
    settings = scene.render.image_settings
    if hasattr(settings, "media_type"):
        settings.media_type = "VIDEO"
    settings.file_format = "FFMPEG"
    scene.render.ffmpeg.format = "MPEG4"
    scene.render.ffmpeg.codec = "H264"
    scene.render.ffmpeg.constant_rate_factor = "HIGH"
    scene.render.filepath = os.path.join(out_dir, anim_cam + ".mp4")
    bpy.ops.render.render(animation=True)
    print("rendered", scene.render.filepath)
else:
    for d in item_dirs:
        load_item(d)
    for v in views:
        render(v)
    print("rendered", ", ".join(views))
