import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
v = 0xe9731b0
for i in [int(x) for x in sys.argv[1].split(",")]:
    f = vt(v, i)
    print("---", i, hex(f))
    pdis(f, int(sys.argv[2]) if len(sys.argv) > 2 else 30)
