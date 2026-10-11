import json, os, re, sys
sys.stdout.reconfigure(encoding="utf-8")
import abi_args
import module_deps as d

# Flarial's own cache of resolved signatures for this game build (name, pattern, rva), checked against the saved image:
# for each name the game function's real argument count next to the callbacks the public hooks register for it.
CACHE = os.path.expandvars(r"%LOCALAPPDATA%/Flarial/Client/cache/signature_rvas.json")


def arity(params):
    params = params.strip()
    if not params or params == "void":
        return 0
    depth = 0
    n = 1
    for ch in params:
        depth += ch in "<(["
        depth -= ch in ">)]"
        if ch == "," and depth == 0:
            n += 1
    return n


def callbacks(text):
    found = []
    for m in re.finditer(r"static\s+[\w:<>\*&\s]+?\s+(\w+)\s*\(([^)]*)\)\s*\{", text):
        found.append((m.group(1), arity(m.group(2))))
    return found


def main():
    cache = {s["name"]: s for s in json.load(open(CACHE, encoding="utf-8"))["signatures"]}
    groups = d.hook_groups()
    users = {}
    for stem, g in groups.items():
        for sig in g["sigs"]:
            users.setdefault(sig, []).append(stem)
    rows = []
    for name in sorted(users):
        if name not in cache:
            continue
        stems = users[name]
        cbs = []
        for stem in stems:
            for rel in groups[stem]["files"]:
                path = d.merged_files("Client/Hook/Hooks").get(rel)
                if path:
                    cbs += callbacks(d.read(path))
        game = abi_args.args(cache[name]["rva"])
        rows.append((name, hex(cache[name]["rva"]), game, sorted({c for c in cbs}), stems))
    for name, rva, game, cbs, stems in rows:
        print(f"{name:55s} {rva:>10s} game={game}  hooks={[os.path.basename(s) for s in stems]}  callbacks={cbs}")


if __name__ == "__main__":
    main()
