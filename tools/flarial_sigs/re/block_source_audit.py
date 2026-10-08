import argparse
import json

import lib


def check(address, mnemonic, operands):
    instruction = lib.dis(address, 1)[0]
    if (instruction.mnemonic, instruction.op_str) != (mnemonic, operands):
        raise ValueError(f"unexpected instruction at {address:#x}")
    return f"{address:#x}: {mnemonic} {operands}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    slots = [(0xE9731B0, 30, 0x5DAEBB0), (0xE8554E0, 2, 0x2D80120),
             (0xE80A1A0, 12, 0x117C1A0), (0xE9A9D70, 12, 0x117C1A0)]
    for table, slot, function in slots:
        if lib.vt(table, slot) != function:
            raise ValueError(f"vtable slot changed: {table:#x}/{slot}")
    evidence = [
        check(0x5DAEBB6, "lea", "rdx, [rcx + 0x248]"),
        check(0x5DAECCA, "mov", "rsi, qword ptr [rcx + 0x1c8]"),
        check(0x5DAED08, "mov", "rax, qword ptr [rsi]"),
        check(0x5DAED0B, "mov", "rax, qword ptr [rax + 0x60]"),
        check(0x5DAED0F, "mov", "rcx, rsi"),
        check(0x2D80138, "mov", "eax, dword ptr [rdx + 4]"),
        check(0x2D8013B, "movsx", "ecx, word ptr [rcx + 0x3a]"),
        check(0x2D80147, "movsx", "ecx, word ptr [rdi + 0x38]"),
        check(0x2D80156, "mov", "eax, dword ptr [rdx]"),
        check(0x2D80158, "sar", "eax, 4"),
        check(0x2D8015F, "mov", "eax, dword ptr [rdx + 8]"),
        check(0x2D8016C, "mov", "rax, qword ptr [rax + 0x148]"),
        check(0x2D801D1, "movzx", "eax, word ptr [rsi + 8]"),
        check(0x2D801D5, "movzx", "r9d, word ptr [rsi]"),
        check(0x2D801F4, "mov", "rax, qword ptr [rax + 0x18]"),
        check(0x117C1A0, "mov", "rax, qword ptr [rcx + 0xf0]"),
        check(0xC5C5D5C, "call", "0x2d80120"),
        check(0xC5C5D61, "mov", "rax, qword ptr [rax + 0x68]"),
        check(0xC5C5D65, "mov", "rax, qword ptr [rax + 0xe0]"),
        check(0x2A6E13C, "mov", "rdi, qword ptr [rsi + 0x68]"),
        check(0x2A6E14B, "mov", "r14, qword ptr [rdi + 0xf8]"),
        check(0x2A6E152, "cmp", "qword ptr [rdi + 0x100], 0x10"),
        check(0x2A6E15C, "mov", "rdi, qword ptr [rdi + 0xe8]"),
        check(0x2A6E165, "add", "rdi, 0xe8"),
        check(0x303F7C1, "mov", "rbx, qword ptr [rax + 0x68]"),
        check(0x303F7D0, "mov", "r14, qword ptr [rbx + 0xf8]"),
        check(0x303F7D7, "cmp", "qword ptr [rbx + 0x100], 0x10"),
        check(0x303F7E1, "mov", "rbx, qword ptr [rbx + 0xe8]"),
        check(0x303F7EA, "add", "rbx, 0xe8"),
    ]
    report = {"version": "1.26.52.3", "evidence": evidence,
              "verified": ["ClientInstance block source slot", "actor dimension field",
                           "BlockSource block lookup slot and position argument",
                           "Block type pointer +0x68", "name string +0xe8"],
              "unverified": ["in-game Waila result"]}
    with open(args.output, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
    print(f"verified {len(evidence)} instructions; in-game Waila result remains untested")


if __name__ == "__main__":
    main()
