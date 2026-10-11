import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
for a in sys.argv[1:]:
    for p, st, i in slot_of(int(a, 16)):
        print(a, "slot", hex(p), "vtable", hex(st) if st else None, "index", i, "leas", [hex(x) for x in lea_refs(st)][:4] if st else None)
