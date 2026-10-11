import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
# list string refs (lea/mov rip-relative into .rdata that look like strings) and calls in a function
f = int(sys.argv[1], 16)
b, e = func_range(f)
rd = SEC[".rdata"]
for ins in md.disasm(img[b:e], b):
    if "rip +" in ins.op_str:
        m = re.search(r"rip \+ (0x[0-9a-f]+)", ins.op_str)
        t = ins.address + ins.size + int(m.group(1), 16)
        if rd["rva"] <= t < rd["rva"] + rd["size"]:
            s = img[t:t + 80].split(b"\0")[0]
            if len(s) >= 4 and all(32 <= c < 127 for c in s):
                print(hex(ins.address), ins.mnemonic, repr(s.decode()))
    if ins.mnemonic == "call" and len(sys.argv) > 2:
        print(hex(ins.address), "call", ins.op_str)
