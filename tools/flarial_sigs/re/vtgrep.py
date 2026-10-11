import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
# vtgrep.py vtable n regex  -> entries whose first k instructions match regex
v = int(sys.argv[1], 16)
n = int(sys.argv[2])
rx = re.compile(sys.argv[3])
k = int(sys.argv[4]) if len(sys.argv) > 4 else 12
for i in range(n):
    f = vt(v, i)
    if not in_text(f): break
    s = "; ".join(f"{x.mnemonic} {x.op_str}" for x in dis(f, k))
    if rx.search(s):
        print(f"{i:4d} {f:#x}: {s[:300]}")
