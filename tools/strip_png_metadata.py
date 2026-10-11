#!/usr/bin/env python3
"""Remove text, EXIF and timestamps from first-party PNGs without changing pixels."""
import pathlib
import struct
import subprocess

REMOVE = {b'tEXt', b'zTXt', b'iTXt', b'eXIf', b'tIME'}


def clean(data):
    if not data.startswith(b'\x89PNG\r\n\x1a\n'):
        raise ValueError('invalid PNG signature')
    out, pos = data[:8], 8
    ended = False
    while pos < len(data):
        if pos + 12 > len(data):
            raise ValueError('truncated PNG chunk')
        size = struct.unpack('>I', data[pos:pos + 4])[0]
        end = pos + 12 + size
        if end > len(data):
            raise ValueError('truncated PNG payload')
        kind = data[pos + 4:pos + 8]
        if kind not in REMOVE:
            out += data[pos:end]
        pos = end
        if kind == b'IEND':
            ended = True
            break
    if not ended or pos != len(data):
        raise ValueError('invalid PNG end')
    return out


if __name__ == '__main__':
    names = subprocess.check_output(['git', 'ls-files', '-z']).decode('utf8').split('\0')
    changed = 0
    for name in names:
        if not name.endswith('.png') or name.startswith('vendor/'):
            continue
        path = pathlib.Path(name)
        data = path.read_bytes()
        out = clean(data)
        if out != data:
            path.write_bytes(out)
            changed += 1
    print(f'removed metadata from {changed} PNG files; compressed pixel chunks unchanged')
