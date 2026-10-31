#!/usr/bin/env python3
"""Read recorded UEFI graph captures; never access firmware or modify files."""
import argparse
import hashlib
import json
import re
import uuid
from pathlib import Path

HANDLE = re.compile(r'^handle id=(0x[0-9a-f]+)$')
NODE = re.compile(r'^path-node raw=([0-9a-f]+)$')
SFS = '964e5b22-6459-11d2-8e39-00a0c969723b'
DEVICE_PATH = '09576e91-6d3f-11d2-8e39-00a0c969723b'


def inspect_capture(lines, target):
    handles = {}
    current = None
    for line in lines.splitlines():
        match = HANDLE.fullmatch(line)
        if match:
            current = match.group(1)
            handles.setdefault(current, {'protocols': set(), 'match': False})
            continue
        if current is None:
            continue
        if line.startswith('protocol guid='):
            handles[current]['protocols'].add(line[14:].strip())
        match = NODE.fullmatch(line)
        if match:
            raw = bytes.fromhex(match.group(1))
            if (len(raw) == 42 and raw[:4] == bytes.fromhex('04012a00')
                    and raw[40:42] == b'\x02\x02'
                    and uuid.UUID(bytes_le=raw[24:40]) == target):
                handles[current]['match'] = True
    paths = [entry for entry in handles.values() if entry['match']]
    filesystem = [entry for entry in paths if {SFS, DEVICE_PATH} <= entry['protocols']]
    return {'matching_path_handles': len(paths), 'matching_filesystem_handles': len(filesystem)}


def inspect(text, target):
    parts = text.split('capture-complete=1')
    if len(parts) != 3:
        raise ValueError('Expected exactly two complete graph captures')
    return [inspect_capture(part, target) for part in parts[:2]]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('partition_guid', type=uuid.UUID)
    args = parser.parse_args()
    raw = args.capture.read_bytes()
    result = inspect(raw.decode('utf-8-sig', errors='strict'), args.partition_guid)
    print(json.dumps({'schema': 'recorded-esp-graph-v1', 'sha256': hashlib.sha256(raw).hexdigest(),
                      'captures': result, 'hardware_accessed': False}, indent=2))


if __name__ == '__main__':
    main()
