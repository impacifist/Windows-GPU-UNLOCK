"""Offline active-edge scope map. Never authorizes a disconnect."""
import argparse
import hashlib
import json
from pathlib import Path
import re

PCI = '4cf5b200-68b8-4ca5-9eec-b23e3f50029a'
EDID = '1c0c34f6-d380-41fa-a049-8ad06c1a66aa'
GOP = '9042a9de-23dc-4a38-96fb-7aded080516a'
POINTERS = {'8d59d32b-c655-4ae9-9b15-f25904992a43', '31878c87-0b75-11d5-9a4f-0090273fc14d'}


def inspect(text):
    text = '\n'.join(text.splitlines())
    handle = protocol = None
    edges, targets, images, protocols = [], [], {}, {}
    rows = list(re.finditer(r'^binding handle=0x([0-9a-f]+) image=0x([0-9a-f]+) version=0x[0-9a-f]+ stop=0x[0-9a-f]+$', text, re.M))
    for i, row in enumerate(rows):
        block = text[row.end():rows[i+1].start() if i+1 < len(rows) else len(text)]
        dump = re.search(r'^img dump index=([0-9a-f]+)$', block, re.M)
        if dump:
            images[int(row[1], 16)] = int(dump[1], 16)
    seen = set()
    for line in text.splitlines():
        m = re.fullmatch(r'handle id=0x([0-9a-f]+)', line)
        if m:
            handle = int(m[1], 16)
            if handle in seen:
                raise ValueError('duplicate handle')
            seen.add(handle)
            protocols[handle] = set()
            protocol = None
        m = re.fullmatch(r'protocol guid=([a-f0-9-]+)', line)
        if m:
            protocol = m[1]
            if handle is not None:
                protocols[handle].add(protocol)
        m = re.fullmatch(r'open agent=0x([0-9a-f]+) controller=0x([0-9a-f]+) attributes=0x([0-9a-f]+) count=0x([0-9a-f]+)', line)
        if m and handle is not None:
            agent, controller, attributes, count = (int(v, 16) for v in m.groups())
            if attributes in (1, 2, 4):
                continue
            if not protocol or not agent or (not controller and attributes != 32) or not count:
                raise ValueError('invalid active edge')
            edges.append((handle, protocol, agent, controller, attributes, count))
        if line.startswith('pci config-dwords=278210de '):
            targets.append(handle)
    if len(targets) != 1:
        raise ValueError('ambiguous AD104 target')
    target = targets[0]
    owners = {e[2] for e in edges if e[0] == target and e[1] == PCI and e[4] in (16, 48)}
    if len(owners) != 1:
        raise ValueError('expected one PCI driver owner')
    children = {e[3] for e in edges if e[0] == target and e[1] == PCI and e[4] == 8
                and e[2] in owners and GOP in protocols.get(e[3], set())}
    if len(children) != 1:
        raise ValueError('expected one observed display child')
    child = next(iter(children))
    consumers = {e[2] for e in edges if e[0] in (target, child)}
    report = []
    for agent in sorted(consumers, key=lambda a: (images.get(a, 1000), a)):
        owned = [e for e in edges if e[2] == agent]
        outside = [e for e in owned if e[0] not in (target, child)]
        protocols = {e[1] for e in owned}
        report.append(dict(image_index=images.get(agent), active_edges=len(owned),
                           opened_protocol_handles=len({e[0] for e in owned}),
                           outside_gpu_handles=len({e[0] for e in outside}),
                           protocols=sorted(protocols),
                           shared_edid_pointer_role=EDID in protocols and bool(POINTERS & protocols)))
    return dict(active_edges_total=len(edges), consumers=report,
                runtime_disconnect_authorized=False,
                limitations=['Roles are observed opens, not a Stop side-effect trace.',
                             'Keyboard dependencies and all indirect calls remain unresolved.'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = args.log.read_bytes()
    if len(raw) > 8*1024*1024:
        raise ValueError('oversized log')
    result = inspect(raw.decode('ascii'))
    result['log_sha256'] = hashlib.sha256(raw).hexdigest()
    args.output.write_bytes((json.dumps(result, indent=2)+'\n').encode())
