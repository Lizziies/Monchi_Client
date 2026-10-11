import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
# lea-based vtable from sig: t7.py sigrva outname
a = int(sys.argv[1], 16)
off = a + 7 + i32(a + 3)
print("vtable", hex(off))
n = dumpvt(off, S + "/" + sys.argv[2], k=10)
print("len", n)
