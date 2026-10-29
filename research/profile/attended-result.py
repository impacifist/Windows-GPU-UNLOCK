"""Read-only summary of existing AD104 logs; never hardware or unlock authority."""
import argparse
import json
from pathlib import Path
import re


def summarize(diag, witness='', operator_return='unknown'):
    if operator_return not in ('unknown', 'automatic', 'manual'):
        raise ValueError('invalid operator outcome')
    records = diag + '\n' + witness

    def success(event):
        return re.search(r'(?m)^' + re.escape(event) +
                         r' status=0x0000000000000000\r?$', records) is not None

    held = bool(re.search(r'(?m)^(?:manual-power-off-required-no-key-can-resume|'
                          r'handoff-stopped(?:-manual-power-off)?) status=0x[0-9a-fA-F]{16}\r?$', records))
    handoff = success('windows-StartImage-next')
    refused = []
    for offset, status, value in re.findall(
            r'(?m)^ad104-(?:falcon|register) batch=0x[0-9a-fA-F]+ '
            r'offset=0x([0-9a-fA-F]{8}) status=0x([0-9a-fA-F]{16}) '
            r'value=0x([0-9a-fA-F]{8})\r?$', diag):
        if int(status, 16) == 0 and int(value, 16) & 0xffff0000 == 0xbadf0000:
            refused.append({'offset': '0x' + offset.lower(), 'value': '0x' + value.lower(),
                            'transport_status': 'success', 'usable_data': False})
    conflict = (held and handoff) or (held and operator_return == 'automatic')
    return {
        'schema_version': 1,
        'log_classification': 'inconsistent' if conflict else
                              'explicit_guard_hold' if held else
                              'handoff_intent_recorded' if handoff else 'outcome_unknown',
        'returned_refused_observations': refused,
        'guard_hold_recorded': held,
        'windows_handoff_intent_recorded': handoff,
        'refusal_rechecks_recorded': success('refusal-rechecks-complete'),
        'refusal_decode_off_recorded': success('refusal-decode-off-complete'),
        'operator_return': operator_return,
        'automatic_windows_return_supported': operator_return == 'automatic' and handoff and not conflict,
        'microcode_crash_established': False,
        'hardware_call_hang_established': False,
        'unlock_established': False,
        'limitations': 'Markers are evidence, not hardware attestation. Missing records do not prove a hang; '
                       'StartImage-next does not prove Windows boot. Review provenance and complete logs separately.'
    }


def read_bounded(path):
    with path.open('rb') as source:
        data = source.read(8 * 1024 * 1024 + 1)
    if len(data) > 8 * 1024 * 1024:
        raise ValueError('log exceeds 8 MiB limit')
    return data.decode('utf-8-sig')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('diag', type=Path)
    parser.add_argument('--status', type=Path)
    parser.add_argument('--operator-return', choices=('unknown', 'automatic', 'manual'), default='unknown')
    args = parser.parse_args()
    result = summarize(read_bounded(args.diag), read_bounded(args.status) if args.status else '', args.operator_return)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
