"""Read-only XBE disassembly helper for Underground 2 audio investigation."""
import struct
import sys
from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

data = Path('assets/default.xbe').read_bytes()
base = struct.unpack_from('<I', data, 0x104)[0]
count, table = struct.unpack_from('<II', data, 0x11c)
sections = []
for i in range(count):
    h = struct.unpack_from('<9I', data, table - base + i * 56)
    sections.append((h[1], h[3], h[4]))

def read(va, size):
    for start, offset, length in sections:
        if start <= va < start + length:
            return data[offset + va - start:offset + va - start + size]
    raise ValueError(hex(va))

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.skipdata = True
if sys.argv[1] == 'refs':
    for arg in sys.argv[2:]:
        needle = struct.pack('<I', int(arg, 16))
        print('refs', arg)
        for start, offset, length in sections:
            chunk = data[offset:offset + length]
            at = chunk.find(needle)
            while at >= 0:
                print(hex(start + at))
                at = chunk.find(needle, at + 1)
else:
    va = int(sys.argv[1], 16)
    for ins in md.disasm(read(va, int(sys.argv[2], 16)), va):
        print(f'{ins.address:08X}: {ins.mnemonic} {ins.op_str}')
