"""Bounded offline option-ROM chain inspection. Does not execute images."""
import argparse
import hashlib
import json
from pathlib import Path


def inspect(data):
    if not data or len(data) > 1024 * 1024:
        raise ValueError('invalid buffer size')
    images, offset = [], 0
    for _ in range(32):
        if offset + 26 > len(data) or data[offset:offset+2] != b'\x55\xaa':
            raise ValueError('missing bounded ROM header')
        relative = int.from_bytes(data[offset+24:offset+26], 'little')
        pcir = offset + relative
        if relative < 26 or pcir + 24 > len(data) or data[pcir:pcir+4] != b'PCIR':
            raise ValueError('invalid PCIR offset or signature')
        read16 = lambda at: int.from_bytes(data[at:at+2], 'little')
        length = read16(pcir+16)*512
        header = read16(pcir+10)
        if not length or offset+length > len(data) or header < 24 or relative+header > length:
            raise ValueError('invalid image or header length')
        if read16(pcir+4) != 0x10de or read16(pcir+6) != 0x2782:
            raise ValueError('unexpected identity')
        image = data[offset:offset+length]
        if sum(image) & 255:
            raise ValueError('image checksum failed')
        row = dict(offset=offset, bytes=length, code_type=data[pcir+20], checksum_mod256=0,
                   last=bool(data[pcir+21]&128), sha256=hashlib.sha256(image).hexdigest())
        if row['code_type'] == 3:
            payload = read16(offset+22)
            compression = read16(offset+12)
            if int.from_bytes(data[offset+4:offset+8], 'little') != 0xef1 or payload < 26 or payload >= length or compression not in (0, 1):
                raise ValueError('invalid EFI header')
            row.update(machine=read16(offset+10), subsystem=read16(offset+8),
                       compression=compression, payload_offset=payload)
            if compression == 1:
                if payload+8 > length:
                    raise ValueError('missing compression header')
                stream = offset+payload
                compressed = int.from_bytes(data[stream:stream+4], 'little')
                output = int.from_bytes(data[stream+4:stream+8], 'little')
                if not compressed or compressed+8 > length-payload or not 0 < output <= 8*1024*1024:
                    raise ValueError('invalid compressed extent')
                row.update(compressed_stream_bytes=compressed+8, decompressed_bytes_declared=output)
        images.append(row)
        offset += length
        if row['last']:
            if offset != len(data):
                raise ValueError('trailing bytes after final image')
            return dict(buffer_bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), images=images,
                        physical_prom_complete=False, images_executed=False)
    raise ValueError('missing final image within bound')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.write_bytes((json.dumps(inspect(args.rom.read_bytes()), indent=2)+'\n').encode())
