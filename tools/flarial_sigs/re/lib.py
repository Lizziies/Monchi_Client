import json, mmap, re, struct, os
import numpy as np
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

S = os.environ.get("MONCHI_RE_DATA") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "dev-data", "flarial-sigs")
meta = json.load(open(S + "/mc.bin.json"))
BASE = meta["base"]
_f = open(S + "/mc.bin", "rb")
img = mmap.mmap(_f.fileno(), 0, access=mmap.ACCESS_READ)
SEC = {s["name"]: s for s in meta["sections"]}
TX = SEC[".text"]
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = False

def q(r): return struct.unpack_from("<Q", img, r)[0]
def d(r): return struct.unpack_from("<I", img, r)[0]
def i32(r): return struct.unpack_from("<i", img, r)[0]
def va(r): return BASE + r
def rva(v): return v - BASE

def cstr(r, n=200):
    end = min(r + n, len(img))
    e = img.find(b"\0", r, end)
    if e < 0: e = end
    return img[r:e].decode("latin1")

def dis(r, n=60, maxb=4000):
    out = []
    for ins in md.disasm(img[r:r + maxb], r):
        out.append(ins)
        if len(out) >= n: break
    return out

def pdis(r, n=60, maxb=4000):
    for ins in dis(r, n, maxb):
        print(f"{ins.address:#x}: {ins.mnemonic} {ins.op_str}")

def findall(pat, start=None, end=None, limit=50):
    start = start or 0
    end = end or len(img)
    res = []
    p = img.find(pat, start, end)
    while p >= 0 and len(res) < limit:
        res.append(p)
        p = img.find(pat, p + 1, end)
    return res

def sig(pattern, limit=3):
    tok = [None if "?" in t else int(t, 16) for t in pattern.split()]
    expr = b"".join(b"." if t is None else re.escape(bytes([t])) for t in tok)
    runs, cur = [], None
    for i, t in enumerate(tok):
        if t is None: cur = None; continue
        if cur is None: cur = [i, bytearray()]; runs.append(cur)
        cur[1].append(t)
    off, anchor = max(runs, key=lambda r: len(r[1]))
    anchor = bytes(anchor)
    rx = re.compile(expr, re.DOTALL)
    found = []
    s, e = TX["rva"], TX["rva"] + TX["size"]
    p = img.find(anchor, s, e)
    while p >= 0:
        b0 = p - off
        if rx.match(img, b0):
            found.append(b0)
            if len(found) >= limit: break
        p = img.find(anchor, p + 1, e)
    return found

_text = None
def _tx():
    global _text
    if _text is None:
        _text = np.frombuffer(img, dtype=np.uint8, count=TX["size"], offset=TX["rva"])
    return _text

def xrefs(target, limit=200):
    """positions p (rva of disp32) where rip-relative disp32 with next-ip p+4 points to target"""
    t = _tx()
    s = TX["rva"]
    res = []
    CH = 1 << 24
    for k in range(4):
        for c0 in range(k, len(t) - 4, CH):
            n = min(CH, len(t) - 4 - c0) // 4 * 4
            if n <= 0: continue
            arr = np.frombuffer(t[c0:c0 + n].tobytes(), dtype="<i4").astype(np.int64)
            pos = np.arange(c0, c0 + n, 4, dtype=np.int64) + s
            hit = np.nonzero(pos + 4 + arr == target)[0]
            for h in hit:
                res.append(int(pos[h]))
    res.sort()
    return res[:limit]

def xrefs_abs(target_rva, sec=".rdata", limit=200):
    v = struct.pack("<Q", va(target_rva))
    s = SEC[sec]
    return findall(v, s["rva"], s["rva"] + s["size"], limit)

def type_desc(name):
    return [p - 0x10 for p in findall(name.encode() + b"\0", SEC[".data"]["rva"], SEC[".data"]["rva"] + SEC[".data"]["size"], 5)]

def vtables(name):
    """name like .?AVClientInstance@@ -> list of (vtable_rva, col_offset)"""
    res = []
    for td in type_desc(name):
        rd = SEC[".rdata"]
        pat = struct.pack("<I", td)
        for p in findall(pat, rd["rva"], rd["rva"] + rd["size"], 100):
            col = p - 12
            if d(col) != 1 or d(col + 20) != col: continue
            for vp in xrefs_abs(col, ".rdata", 10):
                res.append((vp + 8, d(col + 4)))
    return res

_funcs = None
def func_start(r):
    """find function containing rva r via .pdata"""
    global _funcs
    if _funcs is None:
        p = SEC[".pdata"]
        a = np.frombuffer(img, dtype="<u4", count=p["size"] // 4 // 3 * 3, offset=p["rva"]).reshape(-1, 3)
        a = a[a[:, 0] != 0]
        _funcs = a[np.argsort(a[:, 0])]
    i = np.searchsorted(_funcs[:, 0], r, side="right") - 1
    if i < 0: return None
    b, e, u = (int(x) for x in _funcs[i])
    if b <= r < e:
        # follow chained unwind info to primary function
        return b, e
    return None

def vt(vtr, i): return rva(q(vtr + 8 * i))

def in_text(r): return TX["rva"] <= r < TX["rva"] + TX["size"]

def vt_len(v, maxn=3000):
    n = 0
    while n < maxn and in_text(vt(v, n)):
        n += 1
        if n > 0 and xrefs_abs_cache_hit(v + 8 * n): break
    return n

def xrefs_abs_cache_hit(r):
    return False

def dumpvt(v, path, n=None, k=8):
    n = n or vt_len(v)
    with open(path, "w", encoding="utf-8") as f:
        for i in range(n):
            fr = vt(v, i)
            s = "; ".join(f"{x.mnemonic} {x.op_str}" for x in dis(fr, k))
            f.write(f"{i:4d} {i*8:#6x} {fr:#x}: {s}\n")
    return n

from capstone import x86_const as X
mdd = Cs(CS_ARCH_X86, CS_MODE_64)
mdd.detail = True
_C = {"eax":"rax","ax":"rax","al":"rax","ebx":"rbx","bx":"rbx","bl":"rbx","ecx":"rcx","cx":"rcx","cl":"rcx","edx":"rdx","dx":"rdx","dl":"rdx",
      "esi":"rsi","si":"rsi","sil":"rsi","edi":"rdi","di":"rdi","dil":"rdi","ebp":"rbp","bp":"rbp","bpl":"rbp","esp":"rsp"}
def _canon(name):
    if name in _C: return _C[name]
    mm = re.match(r"(r\d+)[dwb]?$", name)
    return mm.group(1) if mm else name

def func_range(r):
    fr = func_start(r)
    return fr if fr else (r, r + 0x400)

def trace(start, init, end=None):
    if end is None: end = func_range(start)[1]
    regs = dict(init)
    out = []
    for ins in mdd.disasm(img[start:end], start):
        ops = ins.operands
        for o in ops:
            if o.type == X.X86_OP_MEM and o.mem.base and o.mem.index == 0:
                bn = _canon(ins.reg_name(o.mem.base))
                if bn in regs:
                    out.append((ins.address, f"{ins.mnemonic} {ins.op_str}", regs[bn], o.mem.disp))
        if ins.mnemonic == "call":
            for r in ("rax", "rcx", "rdx", "r8", "r9", "r10", "r11"): regs.pop(r, None)
            continue
        if ops and ops[0].type == X.X86_OP_REG and ins.mnemonic not in ("cmp", "test", "push"):
            full = ins.reg_name(ops[0].reg)
            dst = _canon(full)
            newp = None
            if ins.mnemonic in ("mov", "lea") and len(ops) == 2 and full == dst:
                s = ops[1]
                if s.type == X.X86_OP_REG:
                    sn = _canon(ins.reg_name(s.reg))
                    if sn in regs: newp = regs[sn]
                elif s.type == X.X86_OP_MEM and s.mem.base and s.mem.index == 0:
                    bn = _canon(ins.reg_name(s.mem.base))
                    if bn in regs:
                        newp = regs[bn] + (f"[{s.mem.disp:#x}]" if ins.mnemonic == "mov" else f"+{s.mem.disp:#x}")
            if newp is not None: regs[dst] = newp
            else: regs.pop(dst, None)
    return out

def ptrace(start, init, filt=None, end=None):
    for a, t, p, dsp in trace(start, init, end):
        if filt is None or filt(p, dsp):
            print(f"{a:#x}: {t:55s} {p}  +{dsp:#x}")

def run(code):
    exec(code, globals())

_lea = None
def lea_index():
    global _lea
    if _lea is None:
        src = np.load(S + "/lea_src.npy"); tgt = np.load(S + "/lea_tgt.npy")
        o = np.argsort(tgt)
        _lea = (tgt[o], src[o])
    return _lea

def lea_refs(t):
    tg, sr = lea_index()
    i = np.searchsorted(tg, t); j = np.searchsorted(tg, t, side="right")
    return [int(x) for x in sr[i:j]]

def vt_start(slot, maxback=1500):
    """walk back from a vtable slot to the nearest lea-referenced start"""
    tg, sr = lea_index()
    s = slot
    for _ in range(maxback):
        i = np.searchsorted(tg, s)
        if i < len(tg) and tg[i] == s:
            return s
        if not in_text(rva(q(s - 8))): return s
        s -= 8
    return None

def slot_of(fn):
    res = []
    for p in xrefs_abs(fn, ".rdata", 50):
        st = vt_start(p)
        res.append((p, st, (p - st) // 8 if st is not None else None))
    return res
