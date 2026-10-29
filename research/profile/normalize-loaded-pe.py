"""Prepare a private analysis copy of a loaded PE snapshot; never executes it."""
import argparse
from pathlib import Path
import struct


def normalize(data):
    b = bytearray(data)
    if len(b) < 64 or b[:2] != b'MZ':
        raise ValueError('not a DOS/PE snapshot')
    pe = int.from_bytes(b[60:64], 'little')
    if pe+24 > len(b) or b[pe:pe+4] != b'PE\0\0':
        raise ValueError('invalid PE header')
    count = int.from_bytes(b[pe+6:pe+8], 'little')
    optional = int.from_bytes(b[pe+20:pe+22], 'little')
    table = pe+24+optional
    if not 0 < count <= 96 or optional < 64 or table+count*40 > len(b):
        raise ValueError('invalid section table')
    if int.from_bytes(b[pe+24:pe+26], 'little') != 0x20b:
        raise ValueError('requires PE32+')
    if int.from_bytes(b[pe+80:pe+84], 'little') != len(b):
        raise ValueError('snapshot must match SizeOfImage')
    ranges = []
    for i in range(count):
        at = table+40*i
        rva = int.from_bytes(b[at+12:at+16], 'little')
        length = int.from_bytes(b[at+16:at+20], 'little')
        if not length:
            continue
        if rva < table+count*40 or rva+length > len(b):
            raise ValueError('section outside loaded snapshot')
        if any(rva < end and rva+length > start for start, end in ranges):
            raise ValueError('overlapping loaded sections')
        ranges.append((rva, rva+length))
        struct.pack_into('<I', b, at+20, rva)
    return bytes(b)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError('analysis output must not already exist')
    raw = args.snapshot.read_bytes()
    if len(raw) > 4*1024*1024:
        raise ValueError('snapshot exceeds capture limit')
    args.output.write_bytes(normalize(raw))
