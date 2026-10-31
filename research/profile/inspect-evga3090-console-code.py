"""Offline console Stop-code fingerprint comparison; no hardware or admission changes."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('console_topology', Path(__file__).with_name('inspect-evga3090-topology.py'))
topology = importlib.util.module_from_spec(spec)
spec.loader.exec_module(topology)
mp = topology.mp


def fingerprint(folder, text):
    if not topology.inspect(text)['expected_console_chain_observed']:
        raise ValueError('console chain mismatch')
    bindings = mp.parse_bindings(text.replace('\r\n', '\n').splitlines())
    blocks = mp.parse_handles(text.replace('\r\n', '\n'))
    gops = [h for h, b in blocks.items() if mp.GOP in b['protocols']]
    physical = next(h for h in gops if mp.path_of(blocks[h]['raw']))
    virtual = next(h for h in gops if h != physical)
    result = {}
    for role, guid, source, protocol in [('graphics_console', topology.GFX, physical, mp.GOP),
                                        ('text_console', topology.TEXT_SPLITTER, virtual, topology.TEXT)]:
        agents = [agent for proto, agent, controller, attrs, count in blocks[source]['edges']
                  if proto == protocol and controller == source and attrs == 16 and count == 1]
        if len(agents) != 1:
            raise ValueError('ambiguous console owner')
        rec = mp.binding_for(bindings, agents[0])
        if rec['file'] != ('guid', guid):
            raise ValueError('unexpected owner identity')
        if 'dump' not in rec or not 0 <= rec['dump'] <= 255:
            raise ValueError('invalid dump index')
        path = folder / f'IMG{rec["dump"]:02d}.BIN'
        with path.open('rb') as source:
            image = source.read(mp.IMAGE_MAX + 1)
        if len(image) > mp.IMAGE_MAX or len(image) != rec['size']:
            raise ValueError('image size mismatch')
        sections, _, _ = mp.pe_sections(image)
        executable = [s for s in sections if s[3] & 0x20000000 and s[1] <= rec['stop'] - rec['base'] < s[1] + s[2]]
        if len(executable) != 1:
            raise ValueError('Stop section ambiguous')
        # Shared parser rejects relocations in the hashed executable section.
        code = mp.code_hash(folder, rec, executable[0][1])
        result[role] = {'identity': guid, 'dump': code['dump'], 'code_bytes': code['bytes'],
                        'code_sha256': code['sha256'], 'stop_rva': rec['stop'] - rec['base'],
                        'version': rec['version']}
    return result


def compare(rows):
    return len(rows) >= 2 and all(row['roles'] == rows[0]['roles'] for row in rows[1:])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path)
    args = parser.parse_args()
    files = sorted(p for p in args.captures.glob('DIAG??.TXT') if re.fullmatch(r'DIAG\d{2}\.TXT', p.name))
    rows = []
    error = None
    try:
        if not 2 <= len(files) <= 32:
            raise ValueError('expected 2 to 32 logs')
        for path in files:
            with path.open('rb') as source:
                raw = source.read(8 * 1024 * 1024 + 1)
            if len(raw) > 8 * 1024 * 1024:
                raise ValueError('oversized log')
            rows.append({'capture': path.name, 'sha256': hashlib.sha256(raw).hexdigest(),
                         'roles': fingerprint(args.captures, raw.decode('utf-8-sig'))})
    except (OSError, ValueError, UnicodeError, mp.Refused):
        error = 'Missing, malformed, ambiguous or unsupported saved capture/image; preserve files for review.'
    matched = error is None and compare(rows)
    print(json.dumps({'test': 'offline-evga3090-console-code-v1', 'results': rows,
                      'console_code_matches_across_captures': matched, 'error': error,
                      'interpretation': 'Repeatable saved code fingerprints only; no driver semantic qualification.',
                      'hardware_accessed': False, 'write_eligible': False, 'unlock_proven': False}, indent=2))
    return 0 if matched else 1


if __name__ == '__main__':
    raise SystemExit(main())
