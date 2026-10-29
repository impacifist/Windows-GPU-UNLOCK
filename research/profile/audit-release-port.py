"""Offline mismatch report, never runtime authority for driver release."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def audit(text):
    text = '\n'.join(text.splitlines())
    target = re.findall(r'^probe-target seg=0x([0-9a-f]+) bus=0x([0-9a-f]+) dev=0x([0-9a-f]+) fun=0x([0-9a-f]+)$', text, re.M)
    config = re.findall(r'^cfg dwords=([^\r\n]+)$', text, re.M)
    if len(target) != 1 or len(config) != 1:
        raise ValueError('ambiguous or missing target/configuration')
    cfg = [int(v, 16) for v in config[0].split()]
    if len(cfg) != 64 or cfg[0] != 0x278210de or cfg[11] != 0x51321462:
        raise ValueError('not the captured MSI AD104 identity')
    location = [int(v, 16) for v in target[0]]
    reasons = []
    if location != [0, 1, 0, 0]:
        reasons.append('reference-requires-0000:01:00.0')
    if cfg[1] & 4:
        reasons.append('reference-rejects-active-bus-mastering')
    records = []
    pattern = r'^binding handle=0x([0-9a-f]+) image=0x([0-9a-f]+) version=0x([0-9a-f]+) stop=0x([0-9a-f]+)$'
    rows = list(re.finditer(pattern, text, re.M))
    for index, match in enumerate(rows):
        block = text[match.end():rows[index+1].start() if index+1 < len(rows) else len(text)]
        base = re.search(r'^img base=0x([0-9a-f]+)$', block, re.M)
        size = re.search(r'^img size=0x([0-9a-f]+)$', block, re.M)
        dump = re.search(r'^img dump index=([0-9a-f]+)$', block, re.M)
        if base and size and dump:
            records.append(dict(base=int(base[1], 16), bytes=int(size[1], 16),
                index=int(dump[1], 16), version=int(match[3], 16), stop=int(match[4], 16)))
    null_callbacks = []
    for match in rows:
        if int(match[2], 16) == 0:
            stop = int(match[4], 16)
            for row in records:
                if row['base'] <= stop < row['base']+row['bytes']:
                    null_callbacks.append(dict(image_index=row['index'], stop_rva=stop-row['base']))
    # This fingerprint is the observed BIOS INT10 owner, not a native NVIDIA profile.
    bios = [r for r in records if r['index'] == 29]
    if len(bios) != 1:
        raise ValueError('expected captured IMG29 lifecycle evidence')
    row = bios[0]
    observed = dict(version=row['version'], image_bytes=row['bytes'], stop_rva=row['stop']-row['base'])
    expected = dict(version=0x7000b, image_bytes=162016, stop_rva=0x6ec)
    if observed != expected:
        reasons.append('observed-owner-does-not-match-reference-native-nvidia-fingerprint')
    return dict(reference='gpu-unlock-ada@4e7fefcc/windows/ad104-msi/msi_release.c',
                observed_bdf=':'.join(f'{n:x}' for n in location), command=cfg[1]&65535,
                observed_owner=observed, reference_owner=expected,
                null_image_callbacks_within_captured_images=null_callbacks,
                mismatches=reasons, runtime_release_authorized=False,
                limitations=['Snapshot mismatches are not a runtime preflight.',
                             'IMG29 association is specific to this capture family.',
                             'Matching these fields alone cannot authorize release.'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = args.log.read_bytes()
    if len(raw) > 8*1024*1024:
        raise ValueError('oversized capture')
    result = audit(raw.decode('ascii'))
    result['log_sha256'] = hashlib.sha256(raw).hexdigest()
    args.output.write_bytes((json.dumps(result, indent=2)+'\n').encode())
