import argparse
import json

from capstone.x86_const import X86_REG_RCX, X86_REG_RDX

import lib


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    pattern = '55 41 57 41 56 41 55 41 54 56 57 53 B8 D8 12 00 00'
    hits = lib.sig(pattern)
    if hits != [0x5A933F0]:
        raise ValueError('candidate image changed')
    first = {}
    for instruction in lib.mdd.disasm(lib.img[hits[0]:hits[0] + 200], hits[0]):
        reads, writes = instruction.regs_access()
        for reg in [X86_REG_RCX, X86_REG_RDX]:
            if reg not in first and (reg in reads or reg in writes):
                first[reg] = (reg in reads, instruction)
        if instruction.mnemonic == 'call' and instruction.address != 0x5A93401:
            break
    expected = {X86_REG_RCX: 0x5A93463, X86_REG_RDX: 0x5A93478}
    for reg, address in expected.items():
        read, instruction = first[reg]
        if read or instruction.address != address:
            raise ValueError('incoming argument analysis changed')
    evidence = [f'{instruction.address:#x}: {instruction.mnemonic} {instruction.op_str}'
                for read, instruction in first.values()]
    report = {'version': '1.26.52.3', 'candidate_rva': hex(hits[0]),
              'expected_arguments': ['ScreenView*', 'MinecraftUIRenderContext*'],
              'evidence': evidence, 'status': 'rejected: both arguments overwritten before use',
              'replacement_verified': False}
    with open(args.output, 'w', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
    print(report['status'])
    for item in evidence:
        print(item)


if __name__ == '__main__':
    main()
