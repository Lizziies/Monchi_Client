import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
import re
rd = SEC[".rdata"]
pat = sys.argv[1].encode()
lim = int(sys.argv[2]) if len(sys.argv) > 2 else 40
n = 0
for p in findall(pat, rd["rva"], rd["rva"] + rd["size"], 400):
    s = p
    while img[s - 1] >= 0x20 and img[s - 1] < 0x7f and p - s < 300: s -= 1
    e = img.find(b"\0", p)
    print(hex(s), img[s:min(e, s + 200)].decode("latin1"))
    n += 1
    if n >= lim: break
