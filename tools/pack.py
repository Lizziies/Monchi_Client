"""Packs a folder of cosmetics (item.json, tex.png, index.json) into the blob the launcher embeds.

Usage: python3 tools/pack.py <out.pack> <cosmetics dir>
"""
import os
import struct
import sys

out, src = sys.argv[1], sys.argv[2]
entries = []
for root, _, names in os.walk(src):
    for name in names:
        if not name.endswith((".json", ".png")) or name == "preview.png":
            continue
        full = os.path.join(root, name)
        with open(full, "rb") as f:
            entries.append((os.path.relpath(full, src).replace(os.sep, "/"), f.read()))
entries.sort()

with open(out, "wb") as f:
    f.write(b"MCOS" + struct.pack("<I", len(entries)))
    for rel, data in entries:
        path = rel.encode()
        f.write(struct.pack("<H", len(path)) + path + struct.pack("<I", len(data)) + data)
