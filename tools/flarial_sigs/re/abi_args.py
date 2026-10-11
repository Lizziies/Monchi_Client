import re
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86_const import (X86_OP_MEM, X86_REG_R8, X86_REG_R9, X86_REG_RBP, X86_REG_RCX, X86_REG_RDX,
                                X86_REG_RSP, X86_REG_XMM1, X86_REG_XMM2, X86_REG_XMM3)

import lib

# How many arguments a game function really takes: argument registers read before they are written, and the
# highest stack argument slot read (entry rsp + 0x28 is the fifth argument).
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True
REGS = {X86_REG_RCX: 1, X86_REG_RDX: 2, X86_REG_R8: 3, X86_REG_R9: 4,
        X86_REG_XMM1: 2, X86_REG_XMM2: 3, X86_REG_XMM3: 4}


def args(fn, limit=0x400):
    depth = 0
    rbp = None
    seen = {}
    highest = 0
    for ins in md.disasm(lib.img[fn:fn + limit], fn):
        reads, writes = ins.regs_access()
        for reg, n in REGS.items():
            if reg not in seen and (reg in reads or reg in writes):
                seen[reg] = reg in reads and not (ins.mnemonic in ("xor", "xorps", "pxor") and ins.op_str.split(",")[0].strip() == ins.op_str.split(",")[-1].strip())
        if ins.mnemonic == "push":
            depth += 8
        elif ins.mnemonic == "sub" and ins.op_str.startswith("rsp,"):
            try:
                depth += int(ins.op_str.split(",")[1], 0)
            except ValueError:
                pass
        elif ins.mnemonic == "lea" and ins.op_str.startswith("rbp, [rsp"):
            m = re.search(r"rsp \+ (0x[0-9a-f]+|\d+)", ins.op_str)
            rbp = depth - (int(m.group(1), 0) if m else 0)
        for op in ins.operands:
            if op.type != X86_OP_MEM:
                continue
            base = op.mem.base
            if base == X86_REG_RSP:
                entry = op.mem.disp - depth
            elif base == X86_REG_RBP and rbp is not None:
                entry = op.mem.disp - rbp
            else:
                continue
            if entry >= 0x28 and entry < 0x28 + 8 * 16 and ins.mnemonic != "lea":
                highest = max(highest, 5 + (entry - 0x28) // 8)
        if ins.mnemonic in ("ret", "jmp") and ins.address > fn + 8:
            break
    regs = max([n for r, n in REGS.items() if seen.get(r)] or [0])
    return max(regs, highest)


if __name__ == "__main__":
    for a in sys.argv[1:]:
        print(a, args(int(a, 16)))
