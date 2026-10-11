import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
# usage: t6.py funcrva reg=name[,reg=name] [maxlines] [filterprefix]
f = int(sys.argv[1], 16)
init = dict(kv.split("=") for kv in sys.argv[2].split(","))
lim = int(sys.argv[3]) if len(sys.argv) > 3 else 200
pref = sys.argv[4] if len(sys.argv) > 4 else None
fr = func_range(f)
print("range", hex(fr[0]), hex(fr[1]))
n = 0
for a, t, p, dsp in trace(f, init):
    if pref and not p.startswith(pref): continue
    print(f"{a:#x}: {t:60s} {p} +{dsp:#x}")
    n += 1
    if n >= lim: break
