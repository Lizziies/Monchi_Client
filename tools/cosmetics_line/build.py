#!/usr/bin/env python3
"""Builds the new cosmetic line into cosmetics/line.

Usage: python3 tools/cosmetics_line/build.py [output dir]
"""
import json
import pathlib
import sys

here = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(here))

import animal_shoes
import animals
import bandanas
import capes
import clearance
import halo
import pets
import shoes
import underglow
import wings

out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else here.parent.parent / "cosmetics" / "line"
items = halo.build() + wings.build() + capes.build() + bandanas.build() + animals.build() + shoes.build() + animal_shoes.build() + underglow.build() + pets.build()
for item in items:
    hits = clearance.check(item)
    if hits:
        raise SystemExit(f"{item.id} clips into the player: " + ", ".join(f"{part} in {pose} ({n} texels)" for (part, pose), n in sorted(hits.items())))
    folder = item.write(out)
    kb = sum(f.stat().st_size for f in folder.iterdir()) / 1024
    print(f"{item.id:18} {item.slot:6} {len(item.bones):2} bones {item.cubes():3} cubes {kb:5.1f} KB")

index = out / "index.json"
known = {e["id"]: e for e in json.loads(index.read_text())["items"]} if index.exists() else {}
for i in items:
    known[i.id] = {"id": i.id, "name": i.name, "slot": i.slot}
listing = sorted(known.values(), key=lambda e: (e["slot"], e["id"]))
index.write_text(json.dumps({"items": listing}, indent=1) + "\n")
