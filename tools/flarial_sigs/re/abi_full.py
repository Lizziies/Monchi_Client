import re, sys
sys.stdout.reconfigure(encoding="utf-8")
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86_const import X86_OP_MEM, X86_REG_RBP, X86_REG_RSP
import lib

# stack arguments read anywhere in one function (no early stop at the first ret): highest 8-byte slot above the
# return address, counting the fifth argument at entry rsp + 0x28
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True


def stack_args(fn):
    start, end = lib.func_range(fn)
    depth = 0
    rbp = None
    highest = 0
    slots = {}
    for ins in md.disasm(lib.img[start:end], start):
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
            if op.mem.index:
                continue
            if op.mem.base == X86_REG_RBP and rbp is not None:
                entry = op.mem.disp - rbp
            elif op.mem.base == X86_REG_RSP:
                entry = op.mem.disp - depth
            else:
                continue
            if entry >= 0x28 and entry < 0x28 + 8 * 24 and ins.mnemonic != "lea":
                n = 5 + (entry - 0x28) // 8
                slots[n] = slots.get(n, 0) + 1
                highest = max(highest, n)
    return highest, sorted(slots.items()), hex(end - start)


if __name__ == "__main__":
    for a in sys.argv[1:]:
        print(a, stack_args(int(a, 16)))
