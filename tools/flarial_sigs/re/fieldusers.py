import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
from collections import defaultdict
# fieldusers.py disp_hex [limit] : functions loading a pointer from [reg+disp]; then report accesses through that pointer
disp = int(sys.argv[1], 16)
lim = int(sys.argv[2]) if len(sys.argv) > 2 else 200
db = struct.pack("<I", disp)
# mov r64, [r64 + disp32]: REX.W 8B modrm(10 reg rm) disp32
s, e = TX["rva"], TX["rva"] + TX["size"]
funcs = set()
p = img.find(db, s, e)
n = 0
while p >= 0 and n < 200000:
    n += 1
    if img[p - 3] in (0x48, 0x49, 0x4C, 0x4D) and img[p - 2] == 0x8B and (img[p - 1] & 0xC0) == 0x80 and (img[p - 1] & 7) != 4:
        fr = func_start(p)
        if fr: funcs.add(fr[0])
    p = img.find(db, p + 1, e)
print(len(funcs), "functions")
acc = defaultdict(set)
for f in list(funcs)[:lim]:
    for r in ("rcx",):
        for a, t, pth, d in trace(f, {"rcx": "T"}):
            if pth == f"T[{disp:#x}]":
                acc[d].add(f)
for d in sorted(acc):
    print(hex(d), len(acc[d]), [hex(x) for x in list(acc[d])[:4]])
