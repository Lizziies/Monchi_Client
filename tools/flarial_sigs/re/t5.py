import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
# where is a function referenced from vtables (absolute pointers in .rdata) -> print slot
for a in sys.argv[1:]:
    t = int(a, 16)
    for p in xrefs_abs(t, ".rdata", 20):
        print(a, "slot at", hex(p))
