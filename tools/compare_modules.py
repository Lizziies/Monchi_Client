"""Inventory module dependencies against a separate Flarial source checkout."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def normalize(name):
    return re.sub(r"[^a-z0-9]", "", name.lower())


def calls(code, pattern):
    return sorted(set(re.findall(pattern, code)))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--flarial", type=Path, default=Path(".reference/flarial"))
    parser.add_argument("--out", type=Path, default=Path("dev-data/data/out/module-comparison.json"))
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = root / "dll/src/modules"
    registered = calls((source / "Manager.cpp").read_text(encoding="utf-8-sig"), r"add<([\w]+)>\(\)")
    own = {}
    for file in source.rglob("*.hpp"):
        code = file.read_text(encoding="utf-8-sig")
        classes = list(re.finditer(r"\bclass\s+(\w+)\s*:\s*public\s+\w+", code))
        for i, match in enumerate(classes):
            body = code[match.start():classes[i + 1].start() if i + 1 < len(classes) else len(code)]
            own[match[1]] = {"source": file.relative_to(root).as_posix(), "code": body}
    flarial = args.flarial.resolve()
    if args.verify:
        upstream = root / ".reference/flarial"
        count = 0
        for file in flarial.rglob("*"):
            if not file.is_file() or file.name in ("UPSTREAM.json", "README.md"):
                continue
            original = upstream / file.relative_to(flarial)
            if not original.is_file() or hashlib.sha256(file.read_bytes()).digest() != hashlib.sha256(original.read_bytes()).digest():
                raise SystemExit(f"Upstream mismatch: {file}")
            count += 1
        print(f"{count} imported files match the pinned upstream checkout")
    external = flarial / "src/Client/Module/Modules"
    aliases = {"ThirdPersonNametag": "NametagModifier", "BlockBreakIndicator": "BreakProgress",
               "BreakProgress": "BlockBreakIndicator",
               "HiveStats": "HiveStat", "JavaInventoryHotkeys": "JavaInventoryHotkeys",
               "NoScroll": "DisableMouseWheel", "LowHealth": "LowHealthIndicator",
               "DirectionHud": "DirectionHUD", "MumbleLink": "MumbleLink"}
    directories = {normalize(p.name): p for p in external.iterdir() if p.is_dir()}
    used, rows = set(), []
    for name in registered:
        local = own.get(name, {"source": "", "code": ""})
        folder = directories.get(normalize(aliases.get(name, name)))
        row = {"monchi": name, "source": local["source"],
               "game_calls": calls(local["code"], r"\bgame::(\w+)\s*\("),
               "effects": calls(local["code"], r"fx::Id::(\w+)"),
               "requirements": calls(local["code"], r'need::(?:have|sigs)\(.*?"([\w.]+)"'),
               "flarial": folder.name if folder else None,
               "status": "source comparison; in-game effect unverified"}
        if folder:
            used.add(folder)
            code = "\n".join(p.read_text(encoding="utf-8-sig", errors="replace") for p in sorted(folder.rglob("*")) if p.suffix in (".cpp", ".hpp"))
            row["flarial_sources"] = [p.relative_to(flarial).as_posix() for p in sorted(folder.rglob("*")) if p.suffix in (".cpp", ".hpp")]
            row["flarial_events"] = calls(code, r"(?:Listen(?:Ordered)?|Deafen)\(this,\s*(\w+)")
            row["flarial_signatures"] = calls(code, r'GET_SIG_ADDRESS\("([^"]+)"\)')
            row["flarial_options"] = calls(code, r'Options::getOption\("([^"]+)"\)')
        rows.append(row)
    missing = []
    for folder in sorted(set(directories.values()) - used):
        code = "\n".join(p.read_text(encoding="utf-8-sig", errors="replace") for p in folder.rglob("*") if p.suffix in (".cpp", ".hpp"))
        missing.append({"flarial": folder.name,
                        "events": calls(code, r"Listen(?:Ordered)?\(this,\s*(\w+)"),
                        "signatures": calls(code, r'GET_SIG_ADDRESS\("([^"]+)"\)')})
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps({"modules": rows, "without_direct_match": missing}, indent=2), encoding="utf-8")
    print(f"{len(rows)} Monchi modules, {len(used)} direct source matches, {len(missing)} Flarial folders without a direct match")


if __name__ == "__main__":
    main()
