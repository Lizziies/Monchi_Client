import argparse
import json
import re
import struct
from pathlib import Path

# Reads the signature pattern table and the signature name list out of the installed Flarial client as data
# (byte patterns only, no code). The pattern table is an array of {compiled bytes ptr, pattern text ptr, length}
# entries; the name list is an array of pointers to the names, in the order their signatures were first added.
# For the newest version block, name i belongs to pattern i.

PATTERN = re.compile(r"^[0-9A-F?]{1,2}(?: [0-9A-F?]{1,2})+$", re.I)
NAME = re.compile(r"^[A-Za-z_][\w:~<>]*$")


def sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    count = struct.unpack_from("<H", data, pe + 6)[0]
    size = struct.unpack_from("<H", data, pe + 20)[0]
    base = struct.unpack_from("<Q", data, pe + 24 + 24)[0]
    out = []
    for i in range(count):
        at = pe + 24 + size + i * 40
        name = data[at:at + 8].rstrip(b"\0").decode()
        _, rva, raw_size, raw = struct.unpack_from("<IIII", data, at + 8)
        out.append((name, rva, raw_size, raw))
    return base, out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dll", type=Path)
    parser.add_argument("out", type=Path)
    args = parser.parse_args()
    data = args.dll.read_bytes()
    base, secs = sections(data)

    def text(va):
        for _, rva, size, raw in secs:
            if rva <= va - base < rva + size:
                o = raw + va - base - rva
                end = data.find(b"\0", o)
                try:
                    return data[o:end].decode("ascii")
                except UnicodeDecodeError:
                    return None
        return None

    _, rva, size, raw = next(s for s in secs if s[0] == ".rdata")

    def entry(o):
        _, pattern, length = struct.unpack_from("<QQQ", data, o)
        s = text(pattern)
        return s if length < 4096 and s and PATTERN.match(s.strip()) else None

    def name(o):
        s = text(struct.unpack_from("<Q", data, o)[0])
        return s if s and NAME.match(s) else None

    def longest(test, stride):
        best, o = (0, 0), raw
        while o < raw + size - stride:
            if test(o):
                start = o
                while test(o):
                    o += stride
                if o - start > best[1] - best[0]:
                    best = (start, o)
            else:
                o += 8
        return best

    t0, t1 = longest(entry, 24)
    n0, n1 = longest(name, 8)
    table = [entry(o) for o in range(t0, t1, 24)]
    names = [name(o) for o in range(n0, n1, 8)]
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "table.json").write_text(json.dumps(table), encoding="utf-8")
    (args.out / "names.json").write_text(json.dumps(names), encoding="utf-8")
    print(f"{len(table)} patterns, {len(names)} names")


if __name__ == "__main__":
    main()
