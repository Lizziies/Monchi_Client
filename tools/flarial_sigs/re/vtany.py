import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
# vtany.py hexbytes [maxidx] [hexbytes2] -> vtables (lea targets) with an entry whose code starts with bytes (and optionally another entry with bytes2)
b = bytes.fromhex(sys.argv[1].replace(" ", ""))
mx = int(sys.argv[2]) if len(sys.argv) > 2 else 80
b2 = bytes.fromhex(sys.argv[3].replace(" ", "")) if len(sys.argv) > 3 else None
tg, sr = lea_index()
targets = set()
p = img.find(b, TX["rva"], TX["rva"] + TX["size"])
while p >= 0:
    targets.add(p); p = img.find(b, p + 1, TX["rva"] + TX["size"])
print("code hits", len(targets))
for t in np.unique(tg):
    t = int(t)
    hit = []; hit2 = []
    for k in range(mx):
        f = rva(q(t + 8 * k))
        if not in_text(f): break
        if f in targets: hit.append(k)
        if b2 and img[f:f + len(b2)] == b2: hit2.append(k)
    if hit and (not b2 or hit2):
        print(hex(t), hit, hit2, "leas", [hex(x) for x in lea_refs(t)][:2])
