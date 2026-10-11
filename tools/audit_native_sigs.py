import argparse
import json
import re
import struct
import ctypes
from pathlib import Path


def code_sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Not a PE image")
    count = struct.unpack_from("<H", data, pe + 6)[0]
    size = struct.unpack_from("<H", data, pe + 20)[0]
    sections = []
    for index in range(count):
        at = pe + 24 + size + index * 40
        _, rva, length, offset = struct.unpack_from("<IIII", data, at + 8)
        flags = struct.unpack_from("<I", data, at + 36)[0]
        if flags & 0x20000000:
            sections.append((rva, data[offset:offset + length]))
    return sections


def hits(pattern, sections):
    tokens = pattern.split()
    expr = b"".join(b"." if "?" in token else re.escape(bytes([int(token, 16)])) for token in tokens)
    compiled = re.compile(b"(?=" + expr + b")", re.DOTALL)
    runs = []
    start = 0
    while start < len(tokens):
        if "?" in tokens[start]:
            start += 1
            continue
        end = start
        while end < len(tokens) and "?" not in tokens[end]:
            end += 1
        runs.append((start, bytes(int(token, 16) for token in tokens[start:end])))
        start = end
    if not runs:
        return []
    offset, anchor = max(runs, key=lambda run: len(run[1]))
    found = []
    for rva, data in sections:
        position = data.find(anchor)
        while position >= 0:
            begin = position - offset
            if begin >= 0 and compiled.match(data, begin):
                found.append(hex(rva + begin))
                if len(found) == 2:
                    return found
            position = data.find(anchor, position + 1)
    return found


def live_sections(pid, base, executable=True):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_bool, ctypes.c_uint32]
    kernel.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                       ctypes.POINTER(ctypes.c_size_t)]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    process = kernel.OpenProcess(0x410, False, pid)
    if not process:
        raise ctypes.WinError(ctypes.get_last_error())
    def read(at, size):
        buffer = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        if not kernel.ReadProcessMemory(process, at, buffer, size, ctypes.byref(got)) or got.value != size:
            raise ctypes.WinError(ctypes.get_last_error())
        return buffer.raw
    try:
        header = read(base, 4096)
        pe = struct.unpack_from("<I", header, 0x3C)[0]
        count = struct.unpack_from("<H", header, pe + 6)[0]
        size = struct.unpack_from("<H", header, pe + 20)[0]
        result = []
        for index in range(count):
            at = pe + 24 + size + index * 40
            length, rva = struct.unpack_from("<II", header, at + 8)
            flags = struct.unpack_from("<I", header, at + 36)[0]
            if bool(flags & 0x20000000) == executable and flags & 0x40000000 and length:
                result.append((rva, read(base + rva, length)))
        return result
    finally:
        kernel.CloseHandle(process)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("exe", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pid", type=int)
    parser.add_argument("--base", type=lambda value: int(value, 0))
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1] / "vendor/flarial/src/Utils/Memory/Game/Sig/SigInit.cpp"
    blocks = re.split(r"void SigInit::init\w+\(\)\s*\{", source.read_text(encoding="utf-8"))[1:]
    candidates = {}
    for block in reversed(blocks):
        for match in re.finditer(r'(ADD_SIG|DEPRECATE_SIG)\("([^"\n]+)"(?:,\s*"([^"\n]+)")?\)', block):
            kind, name, pattern = match.groups()
            if kind == "DEPRECATE_SIG":
                candidates.pop(name, None)
            else:
                candidates[name] = pattern
    sections = live_sections(args.pid, args.base) if args.pid and args.base else code_sections(args.exe.read_bytes())
    results = [{"name": name, "matches": hits(pattern, sections), "pattern": pattern}
               for name, pattern in sorted(candidates.items())]
    report = {"exe": str(args.exe), "candidateSet": "upstream init260 inheritance",
              "note": "Unique byte matches do not validate function ABI, offsets or module behavior.",
              "results": results}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unique = sum(len(row["matches"]) == 1 for row in results)
    print(f"{unique}/{len(results)} unique candidates; report: {args.output}")


if __name__ == "__main__":
    main()
