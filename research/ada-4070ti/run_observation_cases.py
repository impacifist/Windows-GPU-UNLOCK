"""Apply literal observation reporting to host-generated fixtures; local files only."""

import copy
import hashlib
import json
from pathlib import Path

from capture_observations import assess

HERE = Path(__file__).resolve().parent


def main():
    baseline_path = HERE / 'build/host-capture.json'
    baseline = json.loads(baseline_path.read_text())
    paths = [baseline_path] + sorted((HERE / 'build').glob('graph-*.json'))
    paths += sorted((HERE / 'build').glob('prom-*.json'))
    paths += sorted((HERE / 'build').glob('register-*.json'))
    if len(paths) != 16:
        raise ValueError('Run the console host tests first; expected 16 synthetic fixtures')
    rows = []
    for path in paths:
        data = copy.deepcopy(baseline)
        payload = json.loads(path.read_text())
        if path.name.startswith('graph-'): data['platform'] = payload
        elif path.name.startswith('prom-'): data['physical_prom'] = payload
        elif path.name.startswith('register-'): data['register_snapshot'] = payload
        observed = assess(data)
        for flag in ('hardware_support_validated', 'unlock_ready', 'release_authorized',
                     'capture_origin_authenticated', 'open_roles_interpreted'):
            if observed[flag] is not False:
                raise ValueError('Unexpected authority or role inference')
        if path.name.startswith('prom-') and observed['physical_prom_reported_complete']:
            raise ValueError('Failed PROM fixture was reported complete')
        if path.name.startswith('register-') and observed['register_snapshot_reported_complete']:
            raise ValueError('Failed register fixture was reported complete')
        if path.stem == 'graph-target-absent' and observed['target_handle_resolved'] is not False:
            raise ValueError('Absent target was resolved')
        if path.stem in ('graph-binding-missing', 'graph-image-missing', 'graph-code-malformed', 'graph-agent-absent'):
            if observed['recorded_gpu_agent_code_coverage'] is not False:
                raise ValueError('Missing code/agent fixture claimed recorded code coverage')
        keys = ('platform_enumeration_reported_complete', 'target_handle_resolved',
                'all_recorded_edge_indices_resolved', 'recorded_gpu_agent_code_coverage',
                'matching_recorded_code_hashes', 'register_snapshot_reported_complete',
                'physical_prom_reported_complete', 'null_or_unresolved_reference_slots')
        rows.append({'fixture': path.stem,
                     'fixture_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                     'observations': {key: observed[key] for key in keys}})
    result = {'scope': 'literal field/code coverage of synthetic host fixtures',
              'hardware_accessed': False, 'fixtures_checked': len(rows),
              'open_roles_interpreted': False, 'release_authorized': False,
              'hardware_support_validated': False, 'unlock_ready': False,
              'capture_origin_authenticated': False, 'results': rows,
              'research_source_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in (HERE / 'capture_observations.py', Path(__file__))}}
    output = HERE / 'build/observation-results.json'
    output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print('PASS: 16 synthetic fixtures; separate raw-field/code observations; no execution authority')


if __name__ == '__main__':
    main()
