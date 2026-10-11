import sys; sys.stdout.reconfigure(encoding="utf-8")
from lib import *
# afterv.py vtable n vcalloff : for each slot, find "call [vt+vcalloff]" on this and report how rax result is used right after
v = int(sys.argv[1], 16); n = int(sys.argv[2]); off = int(sys.argv[3], 16)
for i in range(n):
    f = vt(v, i)
    if not in_text(f): break
    ins = dis(f, 25)
    for k, x in enumerate(ins):
        if x.mnemonic == "mov" and f"+ {off:#x}]" in x.op_str and x.op_str.startswith("rax, qword ptr [rax"):
            tail = "; ".join(f"{y.mnemonic} {y.op_str}" for y in ins[k + 1:k + 6])
            print(i, hex(f), tail[:200])
            break
