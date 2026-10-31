"""Summarize an existing inspection ZIP offline. Never extract or execute payloads."""
import argparse
import hashlib
import io
import json
import re
import zipfile

LIMIT = 32 * 1024 * 1024


def review(data):
    if len(data) > LIMIT:
        raise ValueError('Archive exceeds review limit')
    files = {}
    budget = LIMIT

    def scan(raw, depth=0):
        nonlocal budget
        with zipfile.ZipFile(io.BytesIO(raw)) as archive:
            for item in archive.infolist():
                if item.is_dir():
                    continue
                budget -= item.file_size
                if budget < 0:
                    raise ValueError('Expanded archive exceeds review limit')
                name = item.filename.replace('\\', '/').split('/')[-1]
                payload = archive.read(item)
                if name == 'results.zip' and depth == 0:
                    scan(payload, 1)
                elif name in ('inspection.json', 'package.json', 'result.json') or re.fullmatch(r'EVP\d{2}\.(TXT|ARMED)', name):
                    if name in files and files[name] != payload:
                        raise ValueError('Ambiguous duplicate: ' + name)
                    files[name] = payload
    scan(data)
    inspection = json.loads(files['inspection.json'].decode('utf-8-sig'))
    run = inspection['run']
    if not re.fullmatch(r'EVP\d{2}', run):
        raise ValueError('Invalid run identifier')
    log = files.get(run + '.TXT', b'').decode('utf-8-sig', errors='replace')
    gate = files.get(run + '.ARMED', b'')
    entries = inspection.get('current_entries', [])
    returns = re.findall(r'evga-unlock-transaction-return status=(0x[0-9a-fA-F]+)', log)
    ss1 = re.findall(r'EVGA admission cold:[^\r\n]*\bss1=(0x[0-9a-fA-F]+|[0-9]+)', log)
    return {
        'test': 'offline-evga-failure-review-v1',
        'run': run,
        'archive_sha256': hashlib.sha256(data).hexdigest(),
        'log_present': run + '.TXT' in files,
        'gate_present': run + '.ARMED' in files,
        'gate_prefix': gate.split(b'\0', 1)[0][:100].decode('ascii', errors='replace').strip(),
        'capture_count': log.count('capture-complete=1'),
        'transaction_return_recorded': 'evga-unlock-transaction-return' in log,
        'transaction_return_statuses': returns,
        'entry_ss1_observations': ss1,
        'entry_ss1_matches_current_profile': bool(ss1) and all(int(v, 0) == 7 for v in ss1),
        'log_tail': log.splitlines()[-35:],
        'additional_boot_entries': [e for e in entries if not e.get('previously_in_order')],
        'changed_existing_entries': [e for e in entries if e.get('previously_in_order') and not e.get('unchanged')],
        'package': json.loads(files['package.json'].decode('utf-8-sig')) if 'package.json' in files else None,
        'retry_authorized': False,
        'hardware_accessed': False,
        'interpretation': 'Recorded events only; absence of a flushed event does not prove hardware was untouched. Preserve the consumed slot.'
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive')
    args = parser.parse_args()
    with open(args.archive, 'rb') as stream:
        print(json.dumps(review(stream.read(LIMIT + 1)), indent=2))
