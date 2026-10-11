import sys, re, struct, json
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lib
img = lib.img
# Every option is built by one of a few constructors that take the option's number in edx; the save name
# ("gfx_...", "ctrl_...") is put together just before, short names through lea, longer ones through movups.
# Checked against the bindings already in use: gfx_field_of_view = 47, gfx_viewbobbing = 38.
start, end = 0x20b0000, 0x2110000
name_rx = re.compile(r'^[a-z][a-z0-9]*_[a-z0-9_]{2,60}$')
names = []  # (address, name)
labels = []
p = start
for ins_start in range(start, end):
    b = img[ins_start:ins_start + 3]
    if b in (b'\x48\x8d\x05', b'\x48\x8d\x15', b'\x4c\x8d\x05', b'\x0f\x10\x05', b'\x48\x8d\x0d'):
        tgt = ins_start + 7 + struct.unpack_from('<i', img, ins_start + 3)[0]
        if 0 < tgt < len(img) - 80:
            s = lib.cstr(tgt, 64)
            if name_rx.match(s):
                names.append((ins_start, s))
            elif s.startswith('options.'):
                labels.append((ins_start, s))
print(len(names), 'name references', len(labels), 'label references', file=sys.stderr)
out = {}
for i, (a, n) in enumerate(names):
    nxt = names[i + 1][0] if i + 1 < len(names) else a + 900
    stop = min(a + 900, nxt)
    # short form: the number is stored into a local right before the name
    back = img[a - 30:a]
    m = list(re.finditer(rb'(?<!\x48)\xc7\x85.{4}(.{4})', back, re.DOTALL))
    num = None
    how = ''
    if m:
        v = struct.unpack('<I', m[-1].group(1))[0]
        if 0 < v < 1200:
            num, how = v, 'local'
    if num is None:
        edx = None
        for ins in lib.dis(a, 120, stop - a):
            if ins.address >= stop:
                break
            if ins.mnemonic == 'mov' and re.match(r'edx, 0x[0-9a-f]+$', ins.op_str):
                edx = int(ins.op_str.split(', ')[1], 16)
            elif ins.mnemonic == 'mov' and re.match(r'edx, \d+$', ins.op_str):
                edx = int(ins.op_str.split(', ')[1])
            elif ins.mnemonic == 'call' and ins.op_str.startswith('0x') and edx is not None and edx not in (0x20, 0xa0, 0x10, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80):
                num, how = edx, 'ctor ' + ins.op_str
                break
            elif ins.mnemonic == 'call':
                edx = None if edx in (0x20, 0xa0, 0x10, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80) else edx
    lab = next((l for la, l in labels if a < la < stop), '')
    if num is not None:
        out[n] = {'id': num, 'how': how, 'label': lab, 'at': hex(a)}
json.dump(out, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'data', 'options_1.26.52.json'), 'w'), indent=1)
want = re.compile(sys.argv[1] if len(sys.argv) > 1 else '^gfx_')
for n in sorted(out):
    if want.search(n):
        print(f"{n}: {out[n]['id']}  {out[n]['label']}  [{out[n]['how']}]")
print(len(out), 'options with a number', file=sys.stderr)
