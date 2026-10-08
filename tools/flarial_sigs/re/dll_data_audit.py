import argparse
import hashlib
import json
from pathlib import Path

import lib


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    root = args.directory
    cache = json.loads((root / "signature_rvas.json").read_text(encoding="utf-8"))
    patterns = json.loads((root / "extracted/table.json").read_text(encoding="utf-8"))
    names = json.loads((root / "extracted/names.json").read_text(encoding="utf-8"))
    text = lib.SEC[".text"]
    identity = cache["image"]
    compatible = (identity["size_of_image"] == len(lib.img) and
                  identity["text_rva"] == text["rva"] and identity["text_size"] == text["size"])
    if not compatible:
        raise ValueError("cached image layout differs from the saved game image")
    scanned = {}

    def hits(pattern):
        if pattern not in scanned:
            scanned[pattern] = lib.sig(pattern, limit=3)
        return scanned[pattern]

    named = []
    for entry in cache["signatures"]:
        found = hits(entry["pattern"])
        named.append({"name": entry["name"], "pattern": entry["pattern"], "cached_rva": entry["rva"],
                      "hits": found, "matches_cached_rva": entry["rva"] in found,
                      "unique": found == [entry["rva"]]})
    table = [{"index": i, "pattern": pattern, "hits": hits(pattern)} for i, pattern in enumerate(patterns)]
    report = {
        "dll_sha256": hashlib.sha256((root / "Flarial.Client.Release.dll").read_bytes()).hexdigest(),
        "game_sha256": hashlib.sha256(lib.img).hexdigest(),
        "cached_image": identity,
        "layout_matches": compatible,
        "cache_count": len(named), "cache_unique": sum(row["unique"] for row in named),
        "cache_mismatches": [row["name"] for row in named if not row["matches_cached_rva"]],
        "dll_pattern_count": len(table), "dll_name_count": len(names),
        "dll_unique": sum(len(row["hits"]) == 1 for row in table),
        "dll_missing": sum(not row["hits"] for row in table),
        "dll_ambiguous": sum(len(row["hits"]) > 1 for row in table),
        "named": named, "table": table,
        "verification": "signature text against saved Minecraft image; no private code disassembly or live hooks",
    }
    (root / "audit.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"Cache: {report['cache_unique']}/{len(named)} unique at cached RVA; mismatches: {len(report['cache_mismatches'])}")
    print(f"DLL: {len(table)} patterns; {report['dll_unique']} unique, {report['dll_missing']} missing, {report['dll_ambiguous']} ambiguous")


if __name__ == "__main__":
    main()
