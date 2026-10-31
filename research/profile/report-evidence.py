"""Summarize existing capture evidence; stdout only, no execution or upload.

Reuses the offline checker on immutable input snapshots. Fingerprints identify
bytes, not their origin, separate boots, hardware qualification or unlock safety.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('evidence_checker', HERE / 'check-compatibility.py')
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)
VERSION = 1
OUTCOMES = ('unknown', 'not-run', 'direct-usable', 'display-intervention',
            'manual-recovery', 'failed')
BOOTS = ('unknown', 'full-shutdown', 'restart', 'sleep', 'hibernate')
SOURCE_FILES = ('report-evidence.py', 'check-compatibility.py', 'make-profile.py',
                'capture_graph.py', 'gpu_path.py', 'proven.json')
HASH = re.compile(r'[0-9a-f]{64}')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_regular(path, limit):
    # Do not follow a final symlink/reparse point for optional provenance inputs.
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
        raise ValueError('nonregular')
    with path.open('rb') as stream:
        if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
            raise ValueError('nonregular')
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError('limit')
    return data


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate key')
        result[key] = value
    return result


def build_record(manifest, efi):
    """Only compare local manifest metadata and EFI bytes; never execute either."""
    if manifest is None and efi is None:
        return {'status': 'not-supplied', 'capture_binding_verified': False}
    if manifest is None or efi is None:
        return {'status': 'invalid', 'code': 'manifest_and_efi_required',
                'capture_binding_verified': False}
    try:
        raw = read_regular(manifest, 1024 * 1024)
        value = json.loads(raw.decode('utf-8-sig'), object_pairs_hook=unique_object,
                           parse_constant=lambda _: (_ for _ in ()).throw(ValueError()))
        if not isinstance(value, dict) or value.get('Mode') != 'generic-read-only-probe-then-windows-usb':
            raise ValueError('mode')
        for flag in ('CanUnlock', 'CanDetach', 'WritesFirmwareVariables', 'HardwareTested'):
            if value.get(flag) is not False:
                raise ValueError('flags')
        expected = value.get('EfiSha256')
        if not isinstance(expected, str) or not HASH.fullmatch(expected):
            raise ValueError('hash')
        rows = value.get('Sources')
        if not isinstance(rows, list) or not 1 <= len(rows) <= 128:
            raise ValueError('sources')
        names = set()
        for row in rows:
            if not isinstance(row, dict):
                raise ValueError('row')
            name, sha = row.get('Path'), row.get('Sha256')
            if (not isinstance(name, str) or len(name) > 240 or
                    not re.fullmatch(r'[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*', name) or
                    any(part in ('.', '..') for part in name.split('/')) or
                    name.lower() in names or not isinstance(sha, str) or not HASH.fullmatch(sha)):
                raise ValueError('source')
            names.add(name.lower())
        payload = read_regular(efi, 64 * 1024 * 1024)
        if not payload or digest(payload) != expected:
            return {'status': 'invalid', 'code': 'efi_hash_mismatch',
                    'capture_binding_verified': False}
        return {'status': 'matching-local-efi', 'manifest_sha256': digest(raw),
                'efi_sha256': expected, 'efi_bytes': len(payload),
                'declared_source_count': len(rows), 'source_files_verified': False,
                'capture_binding_verified': False}
    except (OSError, ValueError, TypeError, UnicodeError, RecursionError):
        return {'status': 'invalid', 'code': 'invalid_build_metadata',
                'capture_binding_verified': False}


class EvidenceInputs(checker.Inputs):
    def read(self, name, limit):
        if not re.fullmatch(r'(?:DIAG[0-9]{2}\.TXT|IMG[0-9]{2,20}\.BIN|PROM\.BIN)', name):
            raise checker.InputLimit()
        if name not in self.cache:
            raw = read_regular(self.folder / name, min(limit, checker.TOTAL_LIMIT - self.total))
            self.total += len(raw)
            self.cache[name] = checker.Snapshot(name, raw)
        return self.cache[name]


def report(folder, *, windows_outcome='unknown', boot_kind='unknown',
           source_revision=None, manifest=None, efi=None, envelope=None):
    if windows_outcome not in OUTCOMES or boot_kind not in BOOTS:
        raise ValueError('invalid operator selection')
    if source_revision is not None and not re.fullmatch(r'[0-9a-f]{40}|[0-9a-f]{64}', source_revision):
        raise ValueError('invalid source revision')
    if windows_outcome == 'not-run' and boot_kind != 'unknown':
        raise ValueError('boot kind conflicts with not-run')
    sources = [{'path': name, 'sha256': digest(read_regular(HERE / name, 2 * 1024 * 1024))}
               for name in SOURCE_FILES]
    if envelope is None:
        envelope = json.loads(read_regular(HERE / 'proven.json', 1024 * 1024))
    inputs = EvidenceInputs(folder)
    checked = checker.check(folder, envelope, inputs=inputs)
    # Values are produced by the installed checker, never loaded from user JSON.
    records = [{'name': name, 'bytes': len(item.data), 'sha256': digest(item.data)}
               for name, item in sorted(inputs.cache.items())]
    build = build_record(manifest, efi)
    status = 'invalid_data' if build['status'] == 'invalid' else checked['status']
    gaps = []
    if source_revision is None:
        gaps.append('source_revision_not_supplied')
    if build['status'] == 'not-supplied':
        gaps.append('build_metadata_not_supplied')
    if build['status'] == 'invalid':
        gaps.append('build_metadata_invalid')
    if windows_outcome == 'unknown':
        gaps.append('windows_outcome_not_reported')
    if boot_kind == 'unknown':
        gaps.append('boot_kind_not_reported')
    return {
        'format': 'capture-evidence-1', 'reporter_version': VERSION,
        'status': status, 'hardware_tested': False, 'write_eligible': False,
        'unlock_proven': False, 'source_revision': {
            'value': source_revision, 'basis': 'operator-supplied', 'verified': False},
        'installed_tool_sources': sources,
        'operator': {'windows_outcome': windows_outcome, 'boot_kind': boot_kind,
                     'basis': 'operator-reported', 'verified': False},
        'build': build, 'files_read': records, 'evidence_gaps': gaps,
        'file_scope': 'Only immutable bytes consumed by this checker run; not a full folder inventory.',
        'compatibility': checked,
        'limitations': ['Hashes do not prove capture origin or distinct cold boots.',
                        'Build comparison does not bind captures to that EFI or verify source files.',
                        'No FWSEC parser execution, host replay, GPU access or Windows recovery verification.',
                        'No hardware qualification or unlock eligibility is granted.'],
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path)
    parser.add_argument('--windows-outcome', choices=OUTCOMES, default='unknown')
    parser.add_argument('--boot-kind', choices=BOOTS, default='unknown')
    parser.add_argument('--source-revision', help='full source revision, declared rather than verified')
    parser.add_argument('--build-manifest', type=Path)
    parser.add_argument('--efi', type=Path)
    args = parser.parse_args(argv)
    try:
        result = report(args.captures, windows_outcome=args.windows_outcome,
                        boot_kind=args.boot_kind, source_revision=args.source_revision,
                        manifest=args.build_manifest, efi=args.efi)
    except (OSError, ValueError, TypeError, KeyError, RecursionError):
        # Never echo parser exceptions: they can include private paths/raw data.
        result = {'format': 'capture-evidence-1', 'status': 'error',
                  'code': 'invalid_options_or_installation', 'hardware_tested': False,
                  'write_eligible': False, 'unlock_proven': False}
    print(json.dumps(result, ensure_ascii=True, indent=2))
    return 3 if result['status'] == 'error' else 0 if result['status'] == 'candidate' else 2


if __name__ == '__main__':
    sys.exit(main())
