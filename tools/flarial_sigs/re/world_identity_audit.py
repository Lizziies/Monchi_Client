import argparse
import json

import lib
from block_source_audit import check


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    if lib.cstr(0xEC6C5C5) != "LevelName":
        raise ValueError("world name tag changed")
    for table in [0xE80A1A0, 0xE80A440, 0xE9A9D70, 0xEB43E40]:
        if lib.vt(table, 2) != 0x230E560:
            raise ValueError("dimension ID getter changed")
    evidence = [
        check(0x230E563, "mov", "ecx, dword ptr [rcx + 0x1a0]"),
        check(0x230E569, "mov", "dword ptr [rdx], ecx"),
        check(0x5FC419, "mov", "rsi, rcx"),
        check(0x5FC79A, "mov", "dword ptr [rsi + 0x340], eax"),
        check(0x5FC7D3, "lea", "rax, [rip + 0xe66fdeb]"),
        check(0x5FC7DE, "mov", "qword ptr [rbp - 0x28], 9"),
        check(0x5FC7ED, "call", "0x2548e30"),
        check(0x5FC804, "lea", "r14, [rsi + 0x2a8]"),
        check(0x5FC81E, "mov", "rax, qword ptr [rsi + 0x2c0]"),
        check(0x5FC83D, "mov", "r14, qword ptr [rsi + 0x2a8]"),
        check(0x5FC844, "mov", "qword ptr [rsi + 0x2b8], r15"),
        check(0x117E934, "lea", "rsi, [rcx + 0x258]"),
        check(0x117E959, "mov", "rax, qword ptr [rcx + 0x270]"),
        check(0x117E97F, "mov", "qword ptr [rcx + 0x268], rdi"),
    ]
    report = {"version": "1.26.52.3", "evidence": evidence,
              "dimension_id": "Dimension+0x1a0, getter slot 2",
              "world_name": "LevelData+0x2a8, LevelName NBT tag",
              "world_id": "Level+0x258, folder/id string",
              "live_verified": False}
    with open(args.output, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
    print(f"verified {len(evidence)} instructions and four dimension getter slots")


if __name__ == "__main__":
    main()
