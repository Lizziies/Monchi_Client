import argparse
import glob
import os
import re
import sys
from collections import defaultdict

import lib

# Which Flarial modules cannot work on the saved game image: the signature chain of the 1.26.52 core is replayed against
# the image, every hook is tied to the signatures it reads, and every module to the events it listens to.
# events that reach the core without a game hook of its own: Monchi forwards keys and mouse from the window
BRIDGED = {"KeyEvent", "MouseEvent"}

# hooks whose signatures are alternatives or belong to one event each, not all required together
HOOK_ALTERNATIVES = {
    "Client/Hook/Hooks/Game/ContainerScreenControllerHook": {
        "ContainerScreenControllerTickEvent": [["ContainerScreenController::tick"]],
        "ContainerSlotHoveredEvent": [["ContainerScreenController::_onContainerSlotHovered"]],
    },
    "Client/Hook/Hooks/Render/UIControl_updateCachedPositionHook": {
        "UIControlGetPositionEvent": [["UIControl::getPosition"], ["UIControl::_setCachedPosition"]],
    },
    "Client/Hook/Hooks/Render/BgfxFrameExtractorInsertHook": {
        "HurtColorEvent": [["BgfxFrameExtractor::_insertWriteOverlayUniform"], ["BgfxFrameExtractor::_insertWriteOverlayUniformBatched"]],
    },
    "Client/Hook/Hooks/Game/PacketHooks": {
        "PacketEvent": [["MinecraftPackets::createPacket"]],
        "PacketSendEvent": [["LoopbackPacketSender::sendPacket"]],
    },
    "Client/Hook/Hooks/Game/GameModeAttack": {
        "AttackEvent": [["GameMode::attack"]],
    },
}

# events that take several hooks working together instead of any one of them
EVENT_CHAINS = {
    "ItemRendererEvent": [["ItemInHandRenderer::renderItem", "ItemRenderer::render", "ItemRenderer::renderItemGroup"]],
}

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
VENDOR = os.path.join(ROOT, "vendor", "flarial", "src")
ADAPTED = os.path.join(ROOT, "dll", "src", "flarial")


def read(path):
    with open(path, encoding="utf-8", errors="ignore") as f:
        return f.read()


def release_text(text):
    return re.sub(r"#ifdef __DEBUG__.*?#endif", "", text, flags=re.S)


def merged_files(sub):
    files = {}
    for base in (VENDOR, ADAPTED):
        for p in glob.glob(os.path.join(base, sub, "**", "*"), recursive=True):
            if os.path.isfile(p) and p.endswith((".cpp", ".hpp", ".h")):
                files[os.path.relpath(p, base).replace("\\", "/")] = p
    return files


def init_order():
    text = read(os.path.join(ADAPTED, "Utils", "VersionUtils.cpp"))
    inits = re.findall(r"SigInit::(init\w+)", text)
    order = []
    for name in reversed(inits):
        if name not in order:
            order.append(name)
    return order


def sig_bodies():
    text = read(os.path.join(ADAPTED, "Utils", "Memory", "Game", "Sig", "SigInit.cpp"))
    parts = re.split(r"void SigInit::(init\w+)\(\)\s*\{", text)
    return {parts[i]: parts[i + 1] for i in range(1, len(parts), 2)}


def final_sigs():
    bodies = sig_bodies()
    sigs = {}
    for name in init_order():
        for line in bodies.get(name, "").splitlines():
            line = line.strip()
            if line.startswith("//"):
                continue
            m = re.match(r'ADD_SIG\("([^"]+)",\s*"([^"]+)"\)', line)
            if m:
                sigs[m.group(1)] = m.group(2)
                continue
            m = re.match(r'DEPRECATE_SIG\("([^"]+)"\)', line)
            if m:
                sigs.pop(m.group(1), None)
    return sigs


def resolve(sigs):
    status = {}
    for name, pattern in sigs.items():
        hits = lib.sig(pattern, 3)
        status[name] = len(hits)
    return status


def hook_groups():
    groups = defaultdict(lambda: {"sigs": set(), "events": set(), "files": []})
    for rel, path in merged_files("Client/Hook/Hooks").items():
        stem = re.sub(r"\.(cpp|hpp|h)$", "", rel)
        g = groups[stem]
        text = read(path)
        g["files"].append(rel)
        g["sigs"].update(re.findall(r'GET_SIG(?:_ADDRESS)?\("([^"]+)"\)', text))
        g["events"].update(re.findall(r"make_holder<(\w+)>", text))
    return groups


def display_names():
    names = {}
    for rel, path in merged_files("Client/Module/Modules").items():
        parts = rel.split("/")
        if len(parts) < 4:
            continue
        key = parts[3] if parts[3] != "Misc" else "/".join(parts[3:5])
        stem = os.path.splitext(parts[-1])[0]
        if stem != key.split("/")[-1]:
            continue
        m = re.search(r':\s*(?:\w+::)?Module\(\s*"([^"]+)"\s*,', read(path))
        if m and key not in names:
            names[key] = m.group(1)
    return names


def modules():
    mods = {}
    for rel, path in merged_files("Client/Module/Modules").items():
        parts = rel.split("/")
        if len(parts) < 4:
            continue
        name = parts[3] if parts[3] not in ("Misc",) else "/".join(parts[3:5])
        m = mods.setdefault(name, {"events": set(), "sigs": set()})
        text = release_text(read(path))
        m["events"].update(re.findall(r"Listen\(this,\s*(\w+),", text))
        m["sigs"].update(re.findall(r'GET_SIG(?:_ADDRESS)?\("([^"]+)"\)', text))
    return mods


def event_alternatives(groups, triggers, event):
    if event in EVENT_CHAINS:
        return EVENT_CHAINS[event]
    alts = []
    for stem in triggers.get(event, []):
        override = HOOK_ALTERNATIVES.get(stem, {}).get(event)
        if override:
            alts.extend(override)
        elif groups[stem]["sigs"]:
            alts.append(sorted(groups[stem]["sigs"]))
        else:
            return None
    return alts or None


def needs(groups, triggers, mods, names):
    out = []
    for key, m in sorted(mods.items()):
        name = names.get(key)
        if not name:
            continue
        for sig in sorted(m["sigs"]):
            out.append((name, sig, [[sig]]))
        for event in sorted(m["events"] - BRIDGED):
            alts = event_alternatives(groups, triggers, event)
            if alts:
                out.append((name, event, alts))
    return out


def write_inc(path, rows):
    lines = ["// generated by tools/flarial_sigs/re/module_deps.py --inc: what each Flarial module needs from the game,",
             "// as groups of alternatives; a group holds the signatures one hook or call needs together"]
    for name, why, alts in rows:
        groups = ", ".join("{" + ", ".join(f'"{s}"' for s in alt) + "}" for alt in alts)
        lines.append(f'{{"{name}", "{why}", {{{groups}}}}},')
    text = "\n".join(lines) + "\n"
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output")
    parser.add_argument("--inc")
    args = parser.parse_args()
    sigs = final_sigs()
    status = resolve(sigs)
    present = {n for n, c in status.items() if c >= 1}
    groups = hook_groups()
    triggers = defaultdict(list)
    for stem, g in groups.items():
        for e in g["events"]:
            triggers[e].append(stem)

    mods = modules()
    rows = []
    wanted = needs(groups, triggers, mods, display_names())
    if args.inc:
        write_inc(args.inc, wanted)
    by_module = defaultdict(list)
    for name, why, alts in wanted:
        if not any(all(s in present for s in alt) for alt in alts):
            missing = sorted({s for s in alts[0] if s not in present})
            by_module[name].append(f"{why} (needs {', '.join(missing)})")
    names = display_names()
    for key in sorted(mods):
        name = names.get(key)
        if name:
            rows.append((name, by_module.get(name, [])))

    out = []
    out.append(f"signatures in the 1.26.52 chain: {len(sigs)}, found in the image: {len(present)}, "
               f"missing: {len(sigs) - len(present)}, ambiguous: {sum(1 for c in status.values() if c > 1)}")
    out.append("")
    broken = [r for r in rows if r[1]]
    out.append(f"modules: {len(rows)}, with at least one missing binding: {len(broken)}")
    out.append("")
    for name, why in rows:
        out.append(f"{name}: " + ("ok" if not why else "; ".join(why)))
    text = "\n".join(out) + "\n"
    if args.output:
        with open(args.output, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    sys.stdout.reconfigure(encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
