#!/usr/bin/env python3
import importlib.util
import json
import pathlib
import sys

here = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(here))
root = here.parent
only = set(sys.argv[1:])

items = []
for src in sorted((root / "src").glob("*.py")):
    if only and src.stem not in only:
        continue
    spec = importlib.util.spec_from_file_location(src.stem, src)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    made = mod.build()
    for item in made if isinstance(made, list) else [made]:
        item.write(root)
        items.append(item)
        print(f"{item.id:18} {item.slot:9} {item.cubes():3} cubes")

index = root / "index.json"
known = {}
if index.exists():
    for entry in json.loads(index.read_text()).get("items", []):
        known[entry["id"]] = entry
for item in items:
    known[item.id] = {"id": item.id, "name": item.name, "slot": item.slot, "tags": item.tags}
listing = sorted(known.values(), key=lambda e: (e["slot"], e["id"]))
index.write_text(json.dumps({"items": listing}, indent=1) + "\n")
