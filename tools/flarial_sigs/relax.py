import json, re, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
S = sys.argv[1]; root = sys.argv[2]
meta = json.load(open(S + "/mc.bin.json"))
img = open(S + "/mc.bin", "rb").read()
text = next(s for s in meta["sections"] if s["name"] == ".text")
code = img[text["rva"]:text["rva"] + text["size"]]
src = open(root + "/vendor/flarial/src/Utils/Memory/Game/Sig/SigInit.cpp", encoding="utf-8").read()
blocks = re.split(r"void SigInit::init\w+\(\)\s*\{", src)[1:]
cands = {}
for block in reversed(blocks):
    for m in re.finditer(r'(ADD_SIG|DEPRECATE_SIG)\("([^"\n]+)"(?:,\s*"([^"\n]+)")?\)', block):
        kind, name, pat = m.groups()
        if kind == "DEPRECATE_SIG": cands.pop(name, None)
        else: cands[name] = pat
md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True

def tokens(p): return [None if "?" in t else int(t, 16) for t in p.split()]

def relax(tok, level):
    raw = bytes(0 if t is None else t for t in tok)
    out = list(tok); pos = 0
    for ins in md.disasm(raw, 0):
        pos = ins.address + ins.size
        off = ins.address
        # x86 operand encoding: displacement and immediate positions from capstone
        dsz, isz = ins.disp_size, ins.imm_size
        doff, ioff = ins.disp_offset, ins.imm_offset
        grp_branch = ins.group(1) or ins.group(2) or ins.group(7)  # jump, call, branch_relative
        if isz and (grp_branch or isz >= 4 or level >= 3):
            for i in range(off + ioff, min(off + ioff + isz, len(out))): out[i] = None
        if dsz and (dsz >= 4 or level >= 2):
            for i in range(off + doff, min(off + doff + dsz, len(out))): out[i] = None
    for i in range(pos, len(out)): out[i] = None if level >= 1 else out[i]
    return out

def find(tok, limit=2):
    while tok and tok[-1] is None: tok = tok[:-1]
    if not tok: return []
    expr = b"".join(b"." if t is None else re.escape(bytes([t])) for t in tok)
    runs, cur = [], None
    for i, t in enumerate(tok):
        if t is None: cur = None; continue
        if cur is None: cur = [i, bytearray()]; runs.append(cur)
        cur[1].append(t)
    off, anchor = max(runs, key=lambda r: len(r[1]))
    anchor = bytes(anchor)
    rx = re.compile(expr, re.DOTALL)
    found = []; p = code.find(anchor)
    while p >= 0:
        b = p - off
        if b >= 0 and rx.match(code, b):
            found.append(b)
            if len(found) >= limit: break
        p = code.find(anchor, p + 1)
    return found

res = {}
for name, pat in sorted(cands.items()):
    tok = tokens(pat); got = None
    for level in range(4):
        r = relax(tok, level) if level else tok
        if sum(t is not None for t in r) < 6: break
        h = find(r)
        if len(h) == 1: got = (level, h[0]); break
        if not h: continue
        break  # ambiguous: more relaxing won't help
    res[name] = {"pattern": pat, "level": got[0] if got else None, "rva": hex(text["rva"] + got[1]) if got else None}
    print(f"{name:55s} {('L%d %s' % (got[0], res[name]['rva'])) if got else '-'}", flush=True)
json.dump(res, open(S + "/relaxed.json", "w"), indent=1)
print(sum(v["rva"] is not None for v in res.values()), "/", len(res))
