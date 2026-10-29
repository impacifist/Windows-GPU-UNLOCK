"""Describe separate capture observations offline; never grant execution rights.

Research supplement to the upstream analyzer, not a replacement schema validator
or ownership-completeness classifier. Missing raw fields are not policy failures.
Reads local JSON only and prints fixed observation names, counts and booleans.
"""

import argparse
import hashlib
import json
from pathlib import Path


def index(value, count):
    return isinstance(value, int) and not isinstance(value, bool) and 0 <= value < count


def assess(data):
    if not isinstance(data, dict) or data.get('schema') != 4:
        raise ValueError('Expected a schema-4 capture')
    for flag in ('gpu_writes', 'unlock_attempted', 'hardware_support_validated'):
        if data.get(flag) is not False:
            raise ValueError('Unexpected capture authority flag: ' + flag)
    result = {
        'purpose': 'separate offline capture observations; no execution authority',
        'capture_origin_authenticated': False,
        'open_roles_interpreted': False,
        'identity_reported_unique': data.get('unique_expected_identity_observed') is True,
        'register_snapshot_reported_complete': data.get('register_snapshot', {}).get('complete') is True,
        'physical_prom_reported_complete': data.get('physical_prom', {}).get('complete') is True,
        'platform_enumeration_reported_complete': False,
        'target_handle_resolved': None, 'all_recorded_edge_indices_resolved': None,
        'recorded_gpu_agent_code_coverage': None, 'matching_recorded_code_hashes': 0,
        'active_edge_count': 0, 'null_or_unresolved_reference_slots': 0,
        'atomic_snapshot': False, 'release_authorized': False,
        'hardware_support_validated': False, 'unlock_ready': False,
    }
    platform = data.get('platform', {})
    if platform.get('available') is not True:
        return result
    if platform.get('atomic_snapshot') is not False or platform.get('release_authorized') is not False:
        raise ValueError('Observation cannot authorize release or claim atomicity')
    handles = platform.get('handles')
    if not isinstance(handles, list) or not 1 <= len(handles) <= 4096:
        return result
    if any(not isinstance(h, dict) or h.get('id') != i or isinstance(h.get('id'), bool)
           for i, h in enumerate(handles)):
        raise ValueError('Malformed handle inventory')
    result['platform_enumeration_reported_complete'] = platform.get('complete') is True
    result['target_handle_resolved'] = index(platform.get('target'), len(handles))
    # Literal coverage of recorded indices only. Attributes are deliberately
    # not interpreted: optional null controller slots and application agents
    # must not be mistaken for failed Driver Model ownership requirements.
    needed, unresolved_owner = set(), False
    for handle in handles:
        edges = handle.get('active_opens')
        if not isinstance(edges, list):
            raise ValueError('Missing active edge inventory')
        for edge in edges:
            result['active_edge_count'] += 1
            if result['active_edge_count'] > 4096:
                raise ValueError('Too many active edges')
            for field in ('agent', 'controller'):
                if not index(edge.get(field), len(handles)):
                    result['null_or_unresolved_reference_slots'] += 1
            if handle.get('gpu_descendant') is True:
                if index(edge.get('agent'), len(handles)):
                    needed.add(edge['agent'])
                else:
                    unresolved_owner = True
    if result['active_edge_count']:
        result['all_recorded_edge_indices_resolved'] = result['null_or_unresolved_reference_slots'] == 0
    records = platform.get('gpu_agents', [])
    if not isinstance(records, list) or len(records) > 32:
        raise ValueError('Malformed driver inventory')
    verified, seen = set(), set()
    for record in records:
        agent = record.get('agent')
        if not index(agent, len(handles)):
            unresolved_owner = True
            continue
        if agent in seen:
            raise ValueError('Duplicate driver agent')
        seen.add(agent)
        code = record.get('code')
        if code is None:
            continue
        length, raw = code.get('length'), code.get('hex')
        if not isinstance(length, int) or isinstance(length, bool) or not 0 < length <= 512 * 1024:
            raise ValueError('Invalid code length')
        if raw is None:
            continue
        if not isinstance(raw, str) or len(raw) != length * 2:
            raise ValueError('Invalid code bytes')
        if hashlib.sha256(bytes.fromhex(raw)).hexdigest() != code.get('loaded_sha256'):
            raise ValueError('Code hash mismatch')
        verified.add(agent)
    result['matching_recorded_code_hashes'] = len(verified)
    if needed or unresolved_owner:
        result['recorded_gpu_agent_code_coverage'] = not unresolved_owner and needed <= verified
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    args = parser.parse_args()
    try:
        if args.capture.stat().st_size > 8 * 1024 * 1024:
            raise ValueError('Capture exceeds 8 MiB')
        print(json.dumps(assess(json.loads(args.capture.read_text(encoding='utf-8-sig'))), indent=2))
    except (OSError, TypeError, ValueError, AttributeError) as error:
        parser.exit(1, str(error) + '\n')
