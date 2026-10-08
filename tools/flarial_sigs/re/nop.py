import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
from collections import Counter
# nop.py TypeName [max]: functions that assert on NonOwnerPointer<TypeName>, with their this-relative field reads
t = sys.argv[1]
mx = int(sys.argv[2]) if len(sys.argv) > 2 else 15
rd = SEC[".rdata"]
pats = [f"NonOwnerPointer<{t}>::_get() const [T = {t}]".encode(), f"NonOwnerPointer<{t}>::access() const [T = {t}]".encode()]
funcs = Counter()
for pat in pats:
    for p in findall(pat, rd["rva"], rd["rva"] + rd["size"], 10):
        s = p
        while img[s - 1] != 0: s -= 1
        for x in lea_refs(s):
            fr = func_start(x)
            if fr: funcs[fr[0]] += 1
print(len(funcs), "functions")
for f, _ in list(funcs.items())[:mx]:
    acc = trace(f, {"rcx": "this"})
    sl = slot_of(f)
    print(hex(f), "slots", [(hex(st), i) for p, st, i in sl[:3]], "this-reads", sorted({hex(d) for a, tx, pth, d in acc if pth == "this"})[:12])
