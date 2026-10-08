import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
import lib; t = lib._tx()
s = TX["rva"]
rd = SEC[".rdata"]
lo, hi = rd["rva"], rd["rva"] + rd["size"]
# lea r64, [rip+disp32]: REX(48/4C) 8D modrm(00 rrr 101)
b0 = t[:-7]; b1 = t[1:-6]; b2 = t[2:-5]
m = ((b0 == 0x48) | (b0 == 0x4C)) & (b1 == 0x8D) & ((b2 & 0xC7) == 0x05)
idx = np.nonzero(m)[0]
disp = np.zeros(len(idx), dtype=np.int64)
raw = np.frombuffer(img, dtype=np.uint8, count=TX["size"], offset=TX["rva"])
for k in range(4):
    disp |= raw[idx + 3 + k].astype(np.int64) << (8 * k)
disp = np.where(disp >= 2**31, disp - 2**32, disp)
tgt = idx + s + 7 + disp
sel = (tgt >= lo) & (tgt < hi)
np.save(S + "/lea_src.npy", (idx[sel] + s).astype(np.int64))
np.save(S + "/lea_tgt.npy", tgt[sel].astype(np.int64))
print(sel.sum())
