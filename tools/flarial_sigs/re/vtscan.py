import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
# vtscan.py idx hexbytes [idx hexbytes ...] -> lea-referenced vtables whose slot idx points at code starting with bytes
conds = [(int(sys.argv[i]), bytes.fromhex(sys.argv[i + 1].replace(" ", ""))) for i in range(1, len(sys.argv), 2)]
tg, sr = lea_index()
seen = set()
for t in np.unique(tg):
    t = int(t)
    ok = True
    for idx, b in conds:
        f = rva(q(t + 8 * idx))
        if not in_text(f) or img[f:f + len(b)] != b: ok = False; break
    if not ok: continue
    if not all(in_text(rva(q(t + 8 * k))) for k in range(max(c[0] for c in conds))): continue
    print(hex(t), "leas", [hex(x) for x in lea_refs(t)][:3])
