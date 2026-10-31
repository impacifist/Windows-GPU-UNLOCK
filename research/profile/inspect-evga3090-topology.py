"""Offline EVGA3090 console-chain test. Reads saved logs only; grants no write eligibility."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('evga_profile_parser', HERE / 'make-profile.py')
mp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mp)
TEXT = '387477c2-69c7-11d2-8e39-00a0c969723b'
GFX = '55987e62-1df6-41d1-9062-334fcac3e54d'
TEXT_SPLITTER = '628a497d-2bf6-4264-8741-069dbd3399d6'


def linked(blocks, bindings, source, target, protocol, role):
    edges = blocks[source]['edges']
    owners = [agent for proto, agent, controller, attrs, count in edges
              if proto == protocol and controller == source and attrs == 0x10 and count == 1]
    if len(owners) != 1:
        return False
    owner = owners[0]
    identities = [b['file'] for b in bindings if b['handle'] == owner]
    return identities == [('guid', role)] and sum(
        proto == protocol and agent == owner and controller == target and attrs == 8 and count == 1
        for proto, agent, controller, attrs, count in edges) == 1


def inspect(text):
    text = text.replace('\r\n', '\n')
    if text.count('capture-complete=1') != 1 or 'diagnostic-result status=0x0000000000000000' not in text:
        raise ValueError('incomplete capture')
    blocks = mp.parse_handles(text)
    bindings = mp.parse_bindings(text.splitlines())
    displays = [(h, cfg) for h, b in blocks.items()
                if (cfg := mp.pci_config(b['raw'])) and cfg[2] >> 24 == 3]
    targets = [h for h, cfg in displays if cfg[0] == 0x220410de]
    if len(targets) != 1:
        raise ValueError('expected one EVGA-route RTX3090 PCI identity')
    prefix = mp.path_of(blocks[targets[0]]['raw'])
    if not prefix or len(prefix) < 4:
        raise ValueError('missing target PCI path')
    prefix = prefix[:-4]
    gops = [h for h, b in blocks.items() if mp.GOP in b['protocols']]
    physical = [h for h in gops if (path := mp.path_of(blocks[h]['raw'])) and path.startswith(prefix)]
    conout = re.findall(r'^console out=0x([0-9a-f]+) error=.*$', text, re.M)
    if len(gops) != 2 or len(physical) != 1 or len(conout) != 1:
        raise ValueError('capture does not match the two-GOP observation')
    physical = physical[0]
    virtual = next(h for h in gops if h != physical)
    console = int(conout[0], 16)
    if console not in blocks:
        raise ValueError('system console handle is absent')
    fb = re.findall(r'^gop framebuffer=0x([0-9a-f]+) bytes=0x([0-9a-f]+) .*$', blocks[virtual]['raw'], re.M)
    physical_fb = re.findall(r'^gop framebuffer=0x([0-9a-f]+) bytes=0x([0-9a-f]+) .*$', blocks[physical]['raw'], re.M)
    facts = {
        'one_pci_display_controller': len(displays) == 1,
        'two_gop_handles': len(gops) == 2,
        'one_target_pci_path_gop': True,
        'target_gop_has_framebuffer': len(physical_fb) == 1 and all(int(x, 16) > 0 for x in physical_fb[0]),
        'second_gop_has_no_device_path': mp.path_of(blocks[virtual]['raw']) is None,
        'second_gop_has_zero_framebuffer': len(fb) == 1 and all(int(x, 16) == 0 for x in fb[0]),
        'graphics_console_links_target_to_second_gop': linked(blocks, bindings, physical, virtual, mp.GOP, GFX),
        'text_console_links_second_gop_to_system_console': linked(blocks, bindings, virtual, console, TEXT, TEXT_SPLITTER),
        'system_console_has_text_but_no_gop': TEXT in blocks[console]['protocols'] and mp.GOP not in blocks[console]['protocols'],
    }
    return {'checks': facts, 'expected_console_chain_observed': all(facts.values()),
            'interpretation': 'Checks describe recorded relationships, not driver-code qualification or proof of another GPU.',
            'write_eligible': False, 'unlock_proven': False, 'hardware_accessed': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path)
    args = parser.parse_args()
    files = sorted(p for p in args.captures.glob('DIAG??.TXT') if re.fullmatch(r'DIAG\d{2}\.TXT', p.name))
    if not 1 <= len(files) <= 32:
        parser.error('expected 1 to 32 diagnostic files')
    results = []
    for path in files:
        try:
            with path.open('rb') as source:
                data = source.read(8 * 1024 * 1024 + 1)
            if len(data) > 8 * 1024 * 1024:
                raise ValueError('oversized log')
            result = inspect(data.decode('utf-8-sig'))
            result.update(capture=path.name, sha256=hashlib.sha256(data).hexdigest())
        except (OSError, UnicodeError, ValueError, mp.Refused):
            result = {'capture': path.name, 'expected_console_chain_observed': False,
                      'error': 'Capture is unreadable, malformed, incomplete or outside this observational test.',
                      'write_eligible': False, 'unlock_proven': False, 'hardware_accessed': False}
        results.append(result)
    print(json.dumps({'test': 'offline-evga3090-console-chain-v1', 'results': results}, indent=2))
    return 0 if all(r['expected_console_chain_observed'] for r in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
