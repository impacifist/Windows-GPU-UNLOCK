"""Offline diagnostic joins. Never executes firmware or authorizes release."""
import argparse
import hashlib
import json
from pathlib import Path
import re

PCI = '4cf5b200-68b8-4ca5-9eec-b23e3f50029a'
GOP = '9042a9de-23dc-4a38-96fb-7aded080516a'


def analyze(folder):
    folder = Path(folder)
    data = (folder / 'DIAG00.TXT').read_bytes()
    if len(data) > 8 * 1024 * 1024:
        raise ValueError('log exceeds diagnostic bound')
    text = data.decode('ascii')
    handles, images, binding = {}, [], None
    current = protocol = None
    size = None
    for line in text.splitlines():
        m = re.fullmatch(r'binding handle=0x([0-9a-f]+) image=0x([0-9a-f]+) version=0x([0-9a-f]+) stop=0x([0-9a-f]+)', line)
        if m:
            binding = int(m[1], 16)
            size = None
        m = re.fullmatch(r'img size=0x([0-9a-f]+)', line)
        if m:
            size = int(m[1], 16)
        m = re.fullmatch(r'img dump index=([0-9a-f]+)', line)
        if m:
            index = int(m[1], 16)
            if index >= 100 or any(x['index'] == index for x in images):
                raise ValueError('invalid or duplicate image index')
            raw = (folder / f'IMG{index:02}.BIN').read_bytes()
            if not binding or size is None or len(raw) != size or size > 4 * 1024 * 1024:
                raise ValueError('image length does not match log')
            # Only bounded identification hints are emitted, never arbitrary strings.
            hints = [s for s in ('NVIDIA', 'Nvidia', 'NvidiaGop', 'GraphicsConsole', 'ConsoleSplitter', 'BIOS[INT10] Video Driver')
                     if s.encode() in raw or s.encode('utf-16le') in raw]
            images.append(dict(index=index, binding=binding, bytes=size,
                               sha256=hashlib.sha256(raw).hexdigest(), hints=hints))
        m = re.fullmatch(r'handle id=0x([0-9a-f]+)', line)
        if m:
            current = int(m[1], 16)
            if current in handles:
                raise ValueError('duplicate handle')
            handles[current] = dict(opens=[], protocols=[])
            protocol = None
        m = re.fullmatch(r'protocol guid=([0-9a-f-]+)', line)
        if m and current is not None:
            protocol = m[1]
            handles[current]['protocols'].append(protocol)
        m = re.fullmatch(r'open agent=0x([0-9a-f]+) controller=0x([0-9a-f]+) attributes=0x([0-9a-f]+) count=0x([0-9a-f]+)', line)
        if m and current is not None:
            handles[current]['opens'].append(dict(protocol=protocol, agent=int(m[1], 16),
                controller=int(m[2], 16), attributes=int(m[3], 16)))
        if line.startswith('pci config-dwords=') and current is not None:
            handles[current]['pci'] = line.split('=', 1)[1].split()
    targets = [h for h, v in handles.items() if v.get('pci', [])[:1] == ['278210de']]
    if len(targets) != 1:
        raise ValueError('expected exactly one AD104 target')
    target = targets[0]
    owners = [o for o in handles[target]['opens'] if o['protocol'] == PCI and o['attributes'] in (16, 48)]
    children = [o for o in handles[target]['opens'] if o['protocol'] == PCI and o['attributes'] == 8]
    edges = []
    console_match = re.search(r'^console-out=0x([0-9a-f]+) ', text, re.MULTILINE)
    console = int(console_match[1], 16) if console_match else None
    for owner in owners:
        matches = [i for i in images if i['binding'] == owner['agent']]
        edges.append(dict(role='gpu-pci-by-driver', image_indices=[i['index'] for i in matches]))
        for child in children:
            if child['agent'] == owner['agent'] and GOP in handles.get(child['controller'], {}).get('protocols', []):
                edges.append(dict(role='gpu-owner-to-gop-child', established=True))
                child_handle = child['controller']
                for consumer in handles[child_handle]['opens']:
                    if consumer['attributes'] not in (16, 48):
                        continue
                    links = [o for o in handles[child_handle]['opens']
                             if o['protocol'] == consumer['protocol'] and o['attributes'] == 8
                             and o['agent'] == consumer['agent'] and o['controller'] == console]
                    if links:
                        edges.append(dict(role='gop-child-protocol-to-console-child',
                            protocol=consumer['protocol'], image_indices=[i['index'] for i in images
                            if i['binding'] == consumer['agent']]))
    for image in images:
        image.pop('binding')
    return dict(schema_version=1, log_sha256=hashlib.sha256(data).hexdigest(),
                images=images, unique_image_hashes=len({i['sha256'] for i in images}),
                ownership=edges, release_authorized=False, complete_loaded_image_coverage=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.capture)
    args.output.write_bytes((json.dumps(result, indent=2) + '\n').encode('utf-8'))
