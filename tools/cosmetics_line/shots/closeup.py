"""Copies pets into a scratch folder moved in front of the player, so the preview camera can frame them up close."""
import json, pathlib, shutil, sys
src, dst = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
shutil.rmtree(dst, ignore_errors=True)
for id_, (dx, dy, dz) in {"pet_cat": (-10, 15, 9), "pet_dog": (10.5, 14, 9), "pet_dragon": (16, -5, 9), "pet_bunny": (-6, -5, 9)}.items():
    d = json.loads((src / id_ / "item.json").read_text())
    move = lambda p: [p[0] + dx, p[1] + dy, p[2] + dz]
    for b in d["bones"]:
        if b["name"] == "pet" and b["pivot"] == [0, 0, 0]:
            b.pop("physics", None)
        b["pivot"] = move(b["pivot"])
        for c in b.get("cubes", []):
            c["origin"] = move(c["origin"])
    (dst / id_).mkdir(parents=True)
    (dst / id_ / "item.json").write_text(json.dumps(d))
    shutil.copy(src / id_ / "tex.png", dst / id_ / "tex.png")
