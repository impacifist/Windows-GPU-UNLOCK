"""Evidence packaging tests on synthetic captures, not hardware qualification."""
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def load(name, file):
    spec = importlib.util.spec_from_file_location(name, HERE / file)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


subject = load('evidence_subject', 'report-evidence.py')
fixtures = load('evidence_fixtures', 'test_compatibility.py')


class EvidenceReportTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.CompatibilityTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.folder = self.fixture.folder

    def report(self, **options):
        return subject.report(self.folder, envelope=self.fixture.envelope, **options)

    def build(self):
        efi = self.folder / 'private-local.efi'
        efi.write_bytes(b'synthetic EFI, never executable')
        value = {'Mode': 'generic-read-only-probe-then-windows-usb',
                 'CanUnlock': False, 'CanDetach': False,
                 'WritesFirmwareVariables': False, 'HardwareTested': False,
                 'EfiSha256': subject.digest(efi.read_bytes()),
                 'Sources': [{'Path': 'internal-boot/boot/probe.c', 'Sha256': 'a' * 64}],
                 'private_field': 'PRIVATE-HOST C:/Users/private'}
        manifest = self.folder / 'private-local.json'
        manifest.write_text(json.dumps(value), encoding='utf-8')
        return manifest, efi, value

    def test_candidate_remains_non_authorizing_and_outcome_unknown(self):
        self.fixture.populate()
        result = self.report()
        self.assertEqual(result['status'], 'candidate')
        self.assertEqual(result['operator']['windows_outcome'], 'unknown')
        self.assertFalse(result['operator']['verified'])
        for key in ('hardware_tested', 'write_eligible', 'unlock_proven'):
            self.assertIs(result[key], False)
        self.assertEqual(result['build']['status'], 'not-supplied')
        self.assertIn('build_metadata_not_supplied', result['evidence_gaps'])
        self.assertIn('windows_outcome_not_reported', result['evidence_gaps'])

    def test_hashes_identify_exact_consumed_bytes_without_re_read(self):
        self.fixture.populate()
        result = self.report()
        names = {row['name'] for row in result['files_read']}
        self.assertEqual(names, {'DIAG00.TXT', 'DIAG01.TXT', 'IMG00.BIN', 'PROM.BIN'})
        for row in result['files_read']:
            data = (self.folder / row['name']).read_bytes()
            self.assertEqual(row['bytes'], len(data))
            self.assertEqual(row['sha256'], hashlib.sha256(data).hexdigest())

    def test_snapshot_hash_is_not_a_later_file_read(self):
        self.fixture.populate()
        original = subject.checker.mp.analyze
        saved = (self.folder / 'DIAG00.TXT').read_bytes()

        def mutate_after_read(diag, *args):
            if diag.name == 'DIAG00.TXT':
                (self.folder / diag.name).write_bytes(b'PRIVATE-REPLACED')
            return original(diag, *args)

        with patch.object(subject.checker.mp, 'analyze', side_effect=mutate_after_read):
            result = self.report()
        row = next(row for row in result['files_read'] if row['name'] == 'DIAG00.TXT')
        self.assertEqual(row['sha256'], subject.digest(saved))
        self.assertEqual(result['status'], 'candidate')

    def test_missing_incomplete_and_mixed_inputs_keep_refusal(self):
        self.assertEqual(self.report()['status'], 'needs_data')
        self.fixture.populate(count=1)
        self.assertEqual(self.report()['status'], 'needs_data')
        first = self.fixture.populate(children=2, child_adr=0x10)
        self.fixture.populate(children=2, child_adr=0xa0)
        (self.folder / 'DIAG00.TXT').write_text(first, encoding='ascii')
        result = self.report()
        self.assertEqual(result['status'], 'invalid_data')
        self.assertIn('captures_disagree', [c['code'] for c in result['compatibility']['checks']])

    def test_malformed_text_does_not_escape_into_report(self):
        (self.folder / 'DIAG00.TXT').write_text('PRIVATE-HOST C:/Users/secret invalid', encoding='ascii')
        result = self.report()
        self.assertEqual(result['status'], 'invalid_data')
        self.assertNotIn('PRIVATE-HOST', json.dumps(result))
        self.assertNotIn('C:/Users', json.dumps(result))
        self.assertNotIn(str(self.folder), json.dumps(result))

    def test_outcomes_are_independent_of_checker_status(self):
        self.fixture.populate()
        for outcome in subject.OUTCOMES:
            result = self.report(windows_outcome=outcome)
            self.assertEqual(result['status'], 'candidate')
            self.assertEqual(result['operator']['windows_outcome'], outcome)
            self.assertIs(result['unlock_proven'], False)
        with self.assertRaises(ValueError):
            self.report(windows_outcome='not-run', boot_kind='restart')

    def test_source_revision_is_declared_only_and_tool_hashes_are_real(self):
        result = self.report(source_revision='a' * 40)
        self.assertFalse(result['source_revision']['verified'])
        self.assertEqual(result['source_revision']['value'], 'a' * 40)
        self.assertEqual(len(result['installed_tool_sources']), len(subject.SOURCE_FILES))
        for row in result['installed_tool_sources']:
            self.assertEqual(row['sha256'], subject.digest((HERE / row['path']).read_bytes()))
        for revision in ('HEAD', 'a' * 39, 'PRIVATE-PATH', 'A' * 40):
            with self.assertRaises(ValueError):
                self.report(source_revision=revision)

    def test_local_build_match_is_not_capture_provenance(self):
        manifest, efi, _ = self.build()
        result = self.report(manifest=manifest, efi=efi)
        self.assertEqual(result['build']['status'], 'matching-local-efi')
        self.assertFalse(result['build']['capture_binding_verified'])
        self.assertFalse(result['build']['source_files_verified'])
        self.assertNotIn('PRIVATE-HOST', json.dumps(result))
        self.assertNotIn('private-local', json.dumps(result))

    def test_hash_mismatch_overrides_candidate_without_claiming_recovery(self):
        self.fixture.populate()
        manifest, efi, _ = self.build()
        efi.write_bytes(b'changed')
        result = self.report(manifest=manifest, efi=efi, windows_outcome='direct-usable')
        self.assertEqual(result['status'], 'invalid_data')
        self.assertEqual(result['compatibility']['status'], 'candidate')
        self.assertEqual(result['build']['code'], 'efi_hash_mismatch')
        self.assertFalse(result['operator']['verified'])

    def test_manifest_refuses_bad_flags_paths_hashes_and_duplicate_keys(self):
        manifest, efi, value = self.build()
        for field, wrong in [('CanUnlock', True), ('CanDetach', 0), ('Mode', 'unlock'),
                             ('EfiSha256', 'invalid'), ('Sources', [])]:
            altered = dict(value, **{field: wrong})
            manifest.write_text(json.dumps(altered), encoding='utf-8')
            self.assertEqual(subject.build_record(manifest, efi)['status'], 'invalid')
        for name in ('../secret', '/secret', 'C:/private', 'a/./b', 'a\\b'):
            altered = dict(value, Sources=[{'Path': name, 'Sha256': 'a' * 64}])
            manifest.write_text(json.dumps(altered), encoding='utf-8')
            self.assertEqual(subject.build_record(manifest, efi)['status'], 'invalid')
        for raw in ('{"Mode":1,"Mode":2}', '{"value":NaN}', '{', '[' * 1200):
            manifest.write_text(raw, encoding='ascii')
            self.assertEqual(subject.build_record(manifest, efi)['status'], 'invalid')

    def test_optional_build_inputs_must_be_paired(self):
        manifest, efi, _ = self.build()
        for m, e in ((manifest, None), (None, efi)):
            self.assertEqual(subject.build_record(m, e)['code'], 'manifest_and_efi_required')

    def test_bounds_and_regular_files(self):
        file = self.folder / 'test'
        file.write_bytes(b'12345')
        with self.assertRaises(ValueError):
            subject.read_regular(file, 4)
        with self.assertRaises(ValueError):
            subject.read_regular(self.folder, 10)
        (self.folder / 'DIAG00.TXT').write_bytes(b'x' * (subject.checker.DIAG_LIMIT + 1))
        self.assertEqual(self.report()['status'], 'invalid_data')
        inputs = subject.EvidenceInputs(self.folder)
        with self.assertRaises(subject.checker.InputLimit):
            inputs.read('../private', 100)

    def test_no_execution_file_writes_or_unrelated_files(self):
        self.fixture.populate()
        (self.folder / 'UNRELATED.SECRET').write_text('PRIVATE-TOKEN')
        before = {p.name: p.read_bytes() for p in self.folder.iterdir()}
        with patch.object(subprocess, 'run', side_effect=AssertionError('no execution')):
            result = self.report()
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.folder.iterdir()})
        self.assertNotIn('UNRELATED', json.dumps(result))
        self.assertNotIn('PRIVATE-TOKEN', json.dumps(result))

    def test_cli_errors_are_sanitized_and_exit_codes_explicit(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = subject.main([str(self.folder), '--source-revision', 'PRIVATE-PATH'])
        self.assertEqual(code, 3)
        self.assertNotIn('PRIVATE-PATH', out.getvalue())
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = subject.main([str(self.folder)])
        self.assertEqual(code, 2)
        self.assertEqual(json.loads(out.getvalue())['status'], 'needs_data')


if __name__ == '__main__':
    unittest.main()
