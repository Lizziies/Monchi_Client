import argparse
import json
import struct
import time

import lib
import numpy as np


def candidates(displacement):
    start = lib.TX["rva"]
    end = start + lib.TX["size"]
    needle = struct.pack("<I", displacement)
    positions = []
    position = lib.img.find(needle, start, end)
    while position >= 0:
        modrm = lib.img[position - 1]
        sib_modrm = lib.img[position - 2]
        if modrm & 0xC0 == 0x80 or (
            sib_modrm & 0xC0 == 0x80 and sib_modrm & 7 == 4
        ):
            positions.append(position)
        position = lib.img.find(needle, position + 1, end)
    lib.func_start(start)
    starts = np.ascontiguousarray(lib._funcs[:, 0])
    ends = np.ascontiguousarray(lib._funcs[:, 1])
    addresses = np.asarray(positions, dtype=np.int64)
    indices = np.searchsorted(starts, addresses, side="right") - 1
    valid = indices >= 0
    indices = indices[valid]
    addresses = addresses[valid]
    indices = indices[addresses < ends[indices]]
    return set(int(value) for value in starts[indices])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("displacements")
    parser.add_argument("--output", required=True)
    parser.add_argument("--decode", action="store_true")
    args = parser.parse_args()
    started = time.perf_counter()
    offsets = [int(value, 16) for value in args.displacements.split(",")]
    matches = None
    counts = {}
    for offset in offsets:
        found = candidates(offset)
        counts[hex(offset)] = len(found)
        matches = found if matches is None else matches & found
    report = {
        "status": "unverified byte candidates, not confirmed offsets",
        "displacements": [hex(offset) for offset in offsets],
        "counts": counts,
        "functions": [hex(address) for address in sorted(matches)],
        "seconds": round(time.perf_counter() - started, 3),
    }
    if args.decode:
        decoded = []
        for address in sorted(matches):
            start, end = lib.func_range(address)
            bases = {}
            for instruction in lib.mdd.disasm(lib.img[start:end], start):
                for operand in instruction.operands:
                    if operand.type != lib.X.X86_OP_MEM or operand.mem.disp not in offsets:
                        continue
                    base = instruction.reg_name(operand.mem.base)
                    if base not in ("rsp", "rbp", "rip", ""):
                        bases.setdefault(base, set()).add(operand.mem.disp)
            if any(len(values) >= 4 for values in bases.values()):
                decoded.append({
                    "rva": hex(address),
                    "size": end - start,
                    "bases": {base: [hex(value) for value in sorted(values)]
                              for base, values in bases.items()},
                })
        report["decoded"] = decoded
        report["seconds"] = round(time.perf_counter() - started, 3)
    with open(args.output, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
    print(json.dumps({key: value for key, value in report.items()
                      if key not in ("functions", "decoded")}))
    print(f"intersection: {len(matches)}; saved to {args.output}")
    if args.decode:
        print(f"decoded non-stack clusters: {len(report['decoded'])}")


if __name__ == "__main__":
    main()
