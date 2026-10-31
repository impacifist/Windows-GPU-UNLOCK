"""Offline MSI driver identity gate and negative tests. Never executes firmware."""
import argparse
import importlib.util
import json
from pathlib import Path

spec = importlib.util.spec_from_file_location('code', Path(__file__).with_name('inspect-evga3090-console-code.py'))
code = importlib.util.module_from_spec(spec)
spec.loader.exec_module(code)
PINS = {
    'graphics_console': ('bbfbb62cb30815f58b751879b431fff7d0a2021568b448f873407221728fb246', 1896, 17),
    'text_console': ('3a14de3d645d704fdd327e9e814c7b8928d98ee734d777823d3abe389d628aaa', 4284, 16),
}


def admitted(roles):
    return set(roles) == set(PINS) and all(
        (roles[k]['code_sha256'], roles[k]['stop_rva'], roles[k]['version']) == v for k, v in PINS.items())


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('captures', type=Path)
args = parser.parse_args()
logs = sorted(args.captures.glob('DIAG??.TXT'))
if len(logs) != 2:
    raise ValueError('Expected exactly the two reviewed captures')
rows = []
for log in logs:
    roles = code.fingerprint(args.captures, log.read_text(encoding='utf-8-sig'))
    if not admitted(roles):
        raise ValueError('Driver identity changed; no hardware test authorized')
    checks = 0
    for role in roles:
        for field, bad in [('code_sha256', '0'*64), ('stop_rva', 0), ('version', 0)]:
            changed = {k: dict(v) for k, v in roles.items()}
            changed[role][field] = bad
            if admitted(changed):
                raise AssertionError('Changed driver pin accepted')
            checks += 1
    rows.append({'capture': log.name, 'exact_msi_pins_match': True, 'negative_cases_passed': checks})
print(json.dumps({'test': 'christian-driver-pin-gate-v1', 'results': rows,
                  'hardware_accessed': False, 'driver_stop_semantics_qualified': False,
                  'unlock_proven': False, 'next': 'Return this JSON. Do not boot an EVGA EFI.'}, indent=2))
