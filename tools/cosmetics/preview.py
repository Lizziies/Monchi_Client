"""Builds a single-file html preview of every cosmetic on a minecraft body.

Usage: python3 tools/cosmetics/preview.py [cosmetics dir] [output html]
"""
import base64
import json
import os
import sys

here = os.path.dirname(__file__)
src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "cosmetics")
out = sys.argv[2] if len(sys.argv) > 2 else "monchi_cosmetics.html"

entries = []
for name in sorted(os.listdir(src)):
    path = os.path.join(src, name, "item.json")
    if not os.path.isfile(path):
        continue
    with open(path) as f:
        item = json.load(f)
    tex = os.path.join(src, name, item.get("texture", "tex.png"))
    data = None
    if os.path.isfile(tex):
        with open(tex, "rb") as f:
            data = "data:image/png;base64," + base64.b64encode(f.read()).decode()
    entries.append({"json": item, "tex": data})

with open(os.path.join(here, "preview.html")) as f:
    html = f.read()
with open(out, "w") as f:
    f.write(html.replace("__DATA__", json.dumps(entries, separators=(",", ":"))))
print(out, os.path.getsize(out) // 1024, "kb")
