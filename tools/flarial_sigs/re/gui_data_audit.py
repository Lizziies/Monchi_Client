import argparse
import hashlib
import json
import struct

import lib


def check(address, mnemonic, operands):
    instruction = lib.dis(address, 1)[0]
    if instruction.mnemonic != mnemonic or instruction.op_str != operands:
        raise ValueError(f"unexpected instruction at {address:#x}: {instruction}")
    return f"{address:#x}: {mnemonic} {operands}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    evidence = [
        check(0x5DBC5ED, "mov", "rax, qword ptr [rax + 0x6d0]"),
        check(0x5DBC686, "mov", "r14, qword ptr [rbp + 0x10]"),
        check(0x5DBC887, "movss", "dword ptr [r14 + 0x5c], xmm0"),
        check(0x5DBC895, "divss", "xmm6, xmm0"),
        check(0x5DBC899, "movss", "dword ptr [r14 + 0x60], xmm6"),
        check(0x5DBC8EB, "call", "0x1563510"),
        check(0x15636B1, "movss", "dword ptr [rsi + 0x40], xmm0"),
        check(0x15636BB, "movss", "dword ptr [rsi + 0x44], xmm0"),
        check(0x15636C5, "movss", "dword ptr [rsi + 0x48], xmm0"),
        check(0x15636CF, "movss", "dword ptr [rsi + 0x4c], xmm0"),
        check(0x15636D9, "movss", "dword ptr [rsi + 0x50], xmm0"),
        check(0x15636E3, "movss", "dword ptr [rsi + 0x54], xmm0"),
        check(0x1566D04, "movss", "xmm0, dword ptr [rdx]"),
        check(0x1566D0D, "mulss", "xmm1, dword ptr [r8 + 4]"),
        check(0x1566D18, "mulss", "xmm1, dword ptr [r9 + 0xc]"),
        check(0x1566D33, "mov", "rcx, qword ptr [rcx + 0x98]"),
        check(0x1566D3D, "mov", "rax, qword ptr [rax + 0x6a8]"),
        check(0x1566D63, "mov", "r12d, dword ptr [rcx + 0x18]"),
        check(0x1566D72, "call", "0x1566550"),
        check(0x1566D7F, "call", "0x15662b0"),
        check(0x1566DC6, "cmp", "eax, 7"),
        check(0x1566DD8, "cvttss2si", "ebx, dword ptr [r15 + rax*4]"),
        check(0x1566E0E, "sub", "r13d, ebx"),
        check(0x1566E18, "cmp", "r12d, r13d"),
        check(0x1566E1B, "cmovg", "r13d, r12d"),
        check(0x1566E22, "cmovg", "r13d, eax"),
        check(0x1566E73, "addss", "xmm1, xmm0"),
        check(0x1566E89, "movss", "xmm0, dword ptr [r15 + r14*4]"),
        check(0x156638C, "mov", "rax, qword ptr [rax + 0x6f0]"),
        check(0x1566429, "cmp", "r14d, 4"),
        check(0x1566439, "mov", "rax, qword ptr [rax + 0x198]"),
        check(0x15664C3, "cmp", "eax, ecx"),
        check(0x15664C5, "cmovl", "ecx, eax"),
        check(0x15664C8, "cmp", "ecx, 2"),
        check(0x15664DD, "cmp", "ecx, 4"),
        check(0x15664EE, "cmp", "ecx, 6"),
        check(0x15664FA, "cmp", "ecx, 8"),
        check(0x1566631, "mov", "rax, qword ptr [rax + 0x6f0]"),
        check(0x156678C, "mov", "rax, qword ptr [rax + 0x6e0]"),
        check(0x15668E7, "sqrtss", "xmm1, xmm1"),
        check(0x15668F3, "divss", "xmm1, xmm0"),
        check(0x1566988, "cmovl", "ecx, eax"),
        check(0x5DC526D, "mov", "r8d, 0xfc"),
        check(0x5DC5297, "mov", "r8d, 0xfb"),
        check(0x5DC28D0, "mov", "rax, qword ptr [rcx + 0xe10]"),
        check(0x5DC5446, "mov", "esi, edx"),
        check(0x5DC5471, "mov", "rax, qword ptr [rax + 0x578]"),
        check(0x5DC5484, "mov", "r9, qword ptr [rcx + 0xd8]"),
        check(0x5DC5493, "mov", "r8d, 0xfc"),
        check(0x5DC54C0, "mov", "r8d, 0xfb"),
        check(0x5DC54D8, "cmp", "qword ptr [rax + 0x238], 0"),
        check(0x5DC54E2, "mov", "edx, esi"),
        check(0x5DC54E4, "mov", "r8b, 1"),
        check(0x5DC54E7, "call", "0x91cd60"),
        check(0x91CD76, "cmp", "dword ptr [rcx + 0x18], edx"),
        check(0x91CE17, "mov", "dword ptr [rsi + 0x18], ecx"),
        check(0x91CE21, "call", "0x212650"),
        check(0x91CE26, "test", "bl, bl"),
        check(0x91CE2E, "mov", "rcx, qword ptr [rax + 0x298]"),
    ]
    option_getter = lib.rva(lib.q(0xE9731B0 + 0x6A8))
    if option_getter != 0x5DC5220:
        raise ValueError("ClientInstance GUI option slot changed")
    option_setter = lib.rva(lib.q(0xE9731B0 + 215 * 8))
    if option_setter != 0x5DC5440:
        raise ValueError("ClientInstance GUI option setter slot changed")
    scale_table = struct.unpack_from("<8f", lib.img, 0xE7B9C40)
    if scale_table != tuple(float(i) for i in range(1, 9)):
        raise ValueError("GUI scale table changed")
    if struct.unpack_from("<f", lib.img, 0xE5C6370)[0] != 0.5:
        raise ValueError("GUI scale midpoint changed")
    assertion = lib.cstr(0xEDB571B)
    if "NonOwnerPointer<GuiData>" not in assertion:
        raise ValueError("GuiData type assertion changed")
    start, end = lib.func_range(0x5DBC530)
    xmm3 = [str(instruction) for instruction in
            lib.md.disasm(lib.img[start:end], start)
            if "xmm3" in instruction.op_str]
    report = {
        "version": "1.26.52.3",
        "image_sha256": hashlib.sha256(lib.img).hexdigest(),
        "assertion": assertion,
        "evidence": evidence,
        "xmm3_references": xmm3,
        "scale_table": scale_table,
        "scale_calculation_rva": "0x1566ce0",
        "option_getter_rva": hex(option_getter),
        "option_setter_rva": hex(option_setter),
        "option_setter_slot": 215,
        "integer_setter_rva": "0x91cd60",
        "option_change_dispatch_rva": "0x212650",
        "setter_uses_notifications": True,
        "setter_rejects_inherited_option": True,
        "option_ids": [0xfb, 0xfc],
        "runtime_verified": False,
    }
    with open(args.output, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
    print(f"verified {len(evidence)} instructions and GuiData type assertion")
    print(f"fourth float parameter references: {len(xmm3)}")


if __name__ == "__main__":
    main()
