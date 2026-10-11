import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
a = int(sys.argv[1], 16)
n = int(sys.argv[2]) if len(sys.argv) > 2 else 80
if len(sys.argv) > 3 and sys.argv[3] == "f":
    a = func_start(a)[0]
pdis(a, n, 40000)
