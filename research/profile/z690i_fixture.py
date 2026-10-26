"""Build host-only replay inputs from a complete peecee diagnostic capture.

No profile admission is changed. This extracts observations for the C tests;
production checks decide whether the captured hardware and graph are accepted.
"""
import hashlib
import importlib.util
import re
import uuid
from pathlib import Path

spec = importlib.util.spec_from_file_location('profile_reader', Path(__file__).with_name('make-profile.py'))
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)


def prepare(capture, output):
    raw = capture.read_bytes()
    text = raw.decode('utf-8-sig').replace('\r\n', '\n')
    complete = re.findall(r'^capture-complete=.*$', text, re.M)
    results = re.findall(r'^diagnostic-result status=.*$', text, re.M)
    if complete != ['capture-complete=1'] or results != ['diagnostic-result status=0x0000000000000000']:
        raise ValueError('Need one complete successful capture')
    blocks = reader.parse_handles(text)
    bindings = reader.parse_bindings(text.splitlines())
    ids = {h: i + 1 for i, h in enumerate(blocks)}

    def one(values):
        values = list(values)
        if len(values) != 1:
            raise ValueError(f'Expected one role, found {len(values)}')
        return values[0]

    pci = one(h for h, b in blocks.items() if (c := reader.pci_config(b['raw'])) and c[0] == 0x220310de)
    prefix = reader.path_of(blocks[pci]['raw'])[:-4]
    physical = one(h for h, b in blocks.items() if reader.GOP in b['protocols'] and (reader.path_of(b['raw']) or b'').startswith(prefix))
    combo = reader.sole_driver(blocks, physical, reader.GOP)
    aggregate = one(c for proto, a, c, attr, count in blocks[physical]['edges'] if proto == reader.GOP and a == combo and attr == 8 and count == 1)
    console = int(re.search(r'^console out=0x([0-9a-f]+)', text, re.M)[1], 16)
    roles = {'PCI': pci, 'PRIMARY': physical, 'AGG': aggregate, 'CONSOLE': console}
    roles.update(NV=reader.sole_driver(blocks, pci, reader.PCI_IO),
                 EDID=reader.sole_driver(blocks, physical, reader.EDID_DISCOVERED),
                 COMBO=combo, GFX=reader.sole_driver(blocks, aggregate, reader.GOP),
                 TEXT=reader.sole_driver(blocks, aggregate, reader.TEXT_OUT))
    lines = [f'/* Diagnostic SHA256 {hashlib.sha256(raw).hexdigest()}; ordinal handles. */']
    for name, handle in roles.items():
        lines.append(f'#define FX_{name} ((EFI_HANDLE)(UINTN){ids[handle]})')
    lines.append(f'#define FX_EXTRA ((EFI_HANDLE)(UINTN){len(ids) + 1})')
    lines.append('static const EDGE CapturedEdges[]={')
    for h, block in blocks.items():
        for proto, agent, controller, attrs, count in block['edges']:
            if attrs in (1, 2, 4):
                continue
            if agent not in ids or (controller and controller not in ids):
                raise ValueError('Unresolved active graph edge')
            g = uuid.UUID(proto)
            gc = f'{{0x{g.time_low:x},0x{g.time_mid:x},0x{g.time_hi_version:x},{{' + ','.join(f'0x{x:x}' for x in g.bytes[8:]) + '}}'
            lines.append(f'{{(VOID *){ids[h]},(VOID *){ids[agent]},(VOID *){ids.get(controller, 0)},{gc},{attrs},{count}}},')
    lines.append('};')
    path = reader.path_of(blocks[physical]['raw'])
    lines.append('static const UINT8 CapturedPath[]={' + ','.join(str(x) for x in path) + '};')
    records = []
    for role in ('EDID', 'COMBO', 'GFX', 'TEXT', 'NV'):
        rec = reader.binding_for(bindings, roles[role])
        data = (capture.parent / f'IMG{rec["dump"]:02d}.BIN').read_bytes()
        if len(data) != rec['size']:
            raise ValueError('Image dump length mismatch')
        (output / f'{role}.bin').write_bytes(data)
        image_path = b''.join(rec['nodes'])
        lines.append(f'static UINT8 Path_{role}[]={{' + ','.join(str(x) for x in image_path) + '};')
        records.append(f'{{FX_{role},(VOID *){ids[rec["image"]]},0x{rec["version"]:x},0x{rec["size"]:x},0x{rec["stop"]-rec["base"]:x},{rec["code-type"]},Path_{role},"{role}.bin"}}')
    lines.append('static const DRIVER_CAPTURE CapturedDrivers[]={' + ',\n'.join(records) + '};')
    words = re.search(r'^cfg dwords=(.*)$', text, re.M)[1].split()
    if len(words) != 64:
        raise ValueError('Need 64 PCI dwords')
    lines.append('static const UINT32 CapturedConfig[]={' + ','.join('0x' + w for w in words) + '};')
    (output / 'capture.h').write_text('\n'.join(lines) + '\n')
    (output / 'probe.txt').write_text(''.join(f'config_offset=0x{i*4:x}\nvalue=0x{w}\n' for i, w in enumerate(words)))
    mmio, _ = reader.probe_mmio(text)
    chip, plm, g1, g2, g3, wpr, ss0, ss1, memory = (mmio[o] for o in reader.COLD)
    cold = ''.join(f'config[{i*4:x}]={" ".join(words[i:i+4])}\n' for i in range(0, 64, 4))
    cold += f'cold state: chip={reader.gu_hex(chip)} plm={reader.gu_hex(plm)} gfw={reader.gu_hex(g1)}/{reader.gu_hex(g2)}/{reader.gu_hex(g3)} WPR2_HI={reader.gu_hex(wpr)} SS0={reader.gu_hex(ss0)} SS1={reader.gu_hex(ss1)}\n'
    cold += f'cold observed: chip={reader.gu_hex(chip)} memory={reader.gu_hex(memory)} SS0={reader.gu_hex(ss0)} SS1={reader.gu_hex(ss1)}\n'
    (output / 'cold.txt').write_text(cold)
    return hashlib.sha256(raw).hexdigest()
