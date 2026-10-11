import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
# bytes.py "hex pattern" [limit] -> code hits + vtable slots for each
pat = sys.argv[1]
lim = int(sys.argv[2]) if len(sys.argv) > 2 else 10
for h in sig(pat, lim):
    sl = slot_of(h)
    print(hex(h), [(hex(p), hex(st) if st else None, i) for p, st, i in sl[:6]], len(sl))
