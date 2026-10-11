import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
# actvt.py regex [k] : for each Actor-family vtable (slot 25 == baseTick), count entries matching regex in first k instructions
rx = re.compile(sys.argv[1]); k = int(sys.argv[2]) if len(sys.argv) > 2 else 10
for p, st, i in slot_of(0x19f9e80):
    hits = []
    for n in range(0, 700):
        f = vt(st, n)
        if not in_text(f): break
        if n > 140 and (lea_refs(st + 8 * n)): break
        s = "; ".join(f"{x.mnemonic} {x.op_str}" for x in dis(f, k))
        if rx.search(s): hits.append(n)
    print(hex(st), n, hits[:20])
