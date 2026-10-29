# ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md.
"""Extract an ASRock research candidate without authorizing an unlock build."""
import argparse
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('profile_parser', HERE / 'make-profile.py')
parser = importlib.util.module_from_spec(spec)
spec.loader.exec_module(parser)


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('captures', nargs=2, type=Path)
    args.add_argument('--output', required=True, type=Path)
    args = args.parse_args()
    # Isolated research assumptions, never written into proven.json or consumed
    # by the build evidence gate. Hash/bounds and ownership parsers remain real.
    research = {
        'probe_device_ids': ['250310de'], 'class_revision': '030000a1',
        'chips': [{'chip': 'b76000a1', 'family': 'GA106', 'memory': ['3000']}],
        'nvgop': [{'version': '6000b', 'text_sha256':
                   '61246a83c52e4ca5d93d2aa878c1844f61a7bfe5f26d70a701706fb20f348921'}],
    }
    results = [parser.analyze(folder / 'DIAG00.TXT', folder, research)
               for folder in args.captures]
    left, right = (dict(result['defines']) for result in results)
    changes = {key: [left[key], right[key]] for key in left if left[key] != right[key]}
    if set(changes) - {'GU_RTX_PATH_BYTES'}:
        raise ValueError(f'Unresolved profile differences: {changes}')
    if results[0]['config'] != results[1]['config']:
        raise ValueError('PCI snapshots disagree')
    if results[0]['prom_sha256'] != results[1]['prom_sha256']:
        raise ValueError('PROM digests disagree')
    if 'GU_RTX_PATH_BYTES' in changes:
        def route(value):
            return bytes(int(part) for part in value.replace('\\', '').split(','))
        a, b = (route(value) for value in changes['GU_RTX_PATH_BYTES'])
        if len(a) != 36 or len(b) != 36 or a[:28] != b[:28] or a[32:] != b[32:]:
            raise ValueError('GPU ancestry or display path shape changed')
    args.output.mkdir(parents=True, exist_ok=True)
    report = {
        'status': 'research-candidate-only', 'write_eligible': False,
        'hardware_unlock_tested': False, 'assumptions': research,
        'differences': changes, 'captures': results,
        'pending': ['driver-release replay', 'production transaction simulation',
                    'profile rejection tests', 'supervised hardware unlock and recovery'],
    }
    (args.output / 'candidate.json').write_text(json.dumps(report, indent=2) + '\n')
    header = ['/* Research candidate only; not a supported unlock profile. */',
              '#error Research-only candidate: unlock validation is incomplete']
    header += [f'#define {key} {value}' for key, value in right.items()]
    (args.output / 'candidate.h').write_text('\n'.join(header) + '\n')
    print('Research candidate extracted; no unlock build eligibility granted.')


if __name__ == '__main__':
    main()
