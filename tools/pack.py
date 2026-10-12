"""Packs a folder into the blob the launcher embeds.

Usage: python3 tools/pack.py <out.pack> <dir> [key as 64 hex digits]

With a key every file is sealed with ChaCha20 under its own name, and the list of files is sealed again as a whole.
The launcher writes the sealed files as they are; they are only ever opened in memory.
"""
import os
import struct
import sys

MASK = 0xFFFFFFFF
MASK64 = 0xFFFFFFFFFFFFFFFF


def rotl(v, n):
    return ((v << n) & MASK) | (v >> (32 - n))


def quarter(s, a, b, c, d):
    s[a] = (s[a] + s[b]) & MASK; s[d] = rotl(s[d] ^ s[a], 16)
    s[c] = (s[c] + s[d]) & MASK; s[b] = rotl(s[b] ^ s[c], 12)
    s[a] = (s[a] + s[b]) & MASK; s[d] = rotl(s[d] ^ s[a], 8)
    s[c] = (s[c] + s[d]) & MASK; s[b] = rotl(s[b] ^ s[c], 7)


def crypt(key, nonce, data):
    start = [0x61707865, 0x3320646e, 0x79622d32, 0x6b206574, *struct.unpack("<8I", key), 0, *struct.unpack("<3I", nonce)]
    out = bytearray()
    for counter in range((len(data) + 63) // 64):
        start[12] = counter
        s = list(start)
        for _ in range(10):
            quarter(s, 0, 4, 8, 12); quarter(s, 1, 5, 9, 13); quarter(s, 2, 6, 10, 14); quarter(s, 3, 7, 11, 15)
            quarter(s, 0, 5, 10, 15); quarter(s, 1, 6, 11, 12); quarter(s, 2, 7, 8, 13); quarter(s, 3, 4, 9, 14)
        out += struct.pack("<16I", *((a + b) & MASK for a, b in zip(s, start)))
    return bytes(a ^ b for a, b in zip(data, out))


def nonce_of(name):
    a, b = 0xcbf29ce484222325, 0x84222325cbf29ce4
    for c in name.lower().replace("\\", "/").encode():
        a = ((a ^ c) * 0x100000001b3) & MASK64
        b = (((b ^ c) * 0x100000001b3) + 0x9e3779b97f4a7c15) & MASK64
    return struct.pack("<Q", a) + struct.pack("<Q", b)[:4]


def seal(key, name, data):
    return crypt(key, nonce_of(name), data)


def main():
    out, src = sys.argv[1], sys.argv[2]
    key = bytes.fromhex(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] else None
    if key is not None and len(key) != 32:
        sys.exit("the key must be 64 hex digits")
    entries = []
    for root, _, names in os.walk(src):
        for name in names:
            if not name.endswith((".json", ".png")) or name == "preview.png":
                continue
            full = os.path.join(root, name)
            with open(full, "rb") as f:
                entries.append((os.path.relpath(full, src).replace(os.sep, "/"), f.read()))
    entries.sort()

    body = bytearray(struct.pack("<I", len(entries)))
    for rel, data in entries:
        path = rel.encode()
        if key:
            data = seal(key, rel, data)
        body += struct.pack("<H", len(path)) + path + struct.pack("<I", len(data)) + data
    with open(out, "wb") as f:
        f.write(b"MVLT" + seal(key, "#pack", bytes(body)) if key else b"MCOS" + bytes(body))


if __name__ == "__main__":
    main()
