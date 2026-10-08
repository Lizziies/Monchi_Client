import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
t = int(sys.argv[1], 16)
for x in xrefs(t):
    fr = func_start(x)
    print(hex(x), "func", hex(fr[0]) if fr else None)
