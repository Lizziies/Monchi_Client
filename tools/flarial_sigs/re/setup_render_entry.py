import argparse
import json

from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86_const import X86_REG_RCX, X86_REG_RDX

import lib

# ScreenView::setupAndRender on 1.26.52.3, checked from three sides:
# - the MinecraftUIRenderContext constructor writes the context vtable and stores ClientInstance at +8 and
#   ScreenContext at +0x10 (Flarial's offsets for both),
# - its caller passes (ScreenView*, &context) to the entry, which reads both registers before writing them,
# - the entry owns the lambdas RTTI names as ScreenView::_update (inlined) and calls context vtable slot 9,
#   the slot whose function owns the flushImages lambda.
PATTERN = ("55 41 57 41 56 41 55 41 54 56 57 53 B8 C8 12 00 00 E8 ? ? ? ? 48 29 C4 48 8D AC 24 80 00 00 00 "
           "44 0F 29 BD 30 12 00 00 44 0F 29 B5 20 12 00 00")
ENTRY = 0x4839060
CTOR = 0x662F910
CALLER_SITE = 0x485781E
CONTEXT_VTABLE = 0xE9958B0
FLUSH_IMAGES = 0x66336A0


def check(ok, what):
    if not ok:
        raise ValueError(what)
    return what


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    evidence = []
    evidence.append(check(lib.sig(PATTERN) == [ENTRY], "pattern matches only the entry"))
    ctor = lib.dis(CTOR, 16, 0x60)
    store = next(i for i in ctor if i.mnemonic == "lea" and "rip +" in i.op_str)
    target = store.address + store.size + int(store.op_str.split("rip + ")[1].rstrip("]"), 16)
    evidence.append(check(target == CONTEXT_VTABLE, "constructor installs the context vtable"))
    text = [f"{i.mnemonic} {i.op_str}" for i in ctor]
    evidence.append(check("mov qword ptr [rcx + 8], rdx" in text and "mov qword ptr [rcx + 0x10], r8" in text,
                          "constructor stores ClientInstance at +8 and ScreenContext at +0x10"))
    site = lib.dis(CALLER_SITE, 4, 0x20)
    evidence.append(check(site[0].op_str == hex(CTOR) and site[2].op_str == "rdx, [rbp - 0x50]"
                          and site[3].op_str == hex(ENTRY), "caller builds the context and passes it to the entry"))
    first = {}
    for ins in md.disasm(lib.img[ENTRY:ENTRY + 0x100], ENTRY):
        reads, writes = ins.regs_access()
        for reg in (X86_REG_RCX, X86_REG_RDX):
            if reg not in first and (reg in reads or reg in writes):
                first[reg] = reg in reads
    evidence.append(check(first.get(X86_REG_RCX) and first.get(X86_REG_RDX), "entry reads rcx and rdx first"))
    evidence.append(check(lib.vt(CONTEXT_VTABLE, 9) == FLUSH_IMAGES, "context vtable slot 9 is flushImages"))
    report = {"version": "1.26.52.3", "entry_rva": hex(ENTRY), "pattern": PATTERN, "evidence": evidence}
    with open(args.output, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
    for line in evidence:
        print(line)


if __name__ == "__main__":
    main()
