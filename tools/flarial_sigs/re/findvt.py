import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
# findvt.py "hexbytes_of_getter" slotindex -> vtables having that getter at slotindex
pat = sys.argv[1]; idx = int(sys.argv[2])
for h in sig(pat, 200):
    for p, st, i in slot_of(h):
        if i == idx:
            print(hex(h), "vtable", hex(st), "leas", [hex(x) for x in lea_refs(st)][:4])
