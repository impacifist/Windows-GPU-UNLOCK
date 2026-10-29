import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('attended_result', Path(__file__).with_name('attended-result.py'))
ar = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ar)


class AttendedResultTests(unittest.TestCase):
    def test_returned_refusal_and_hold_are_not_call_hang(self):
        diag = ('ad104-falcon batch=0x01 offset=0x00111668 status=0x0000000000000000 value=0xbadf5620\n'
                'manual-power-off-required-no-key-can-resume status=0x8000000000000006\n')
        result = ar.summarize(diag)
        self.assertEqual(result['log_classification'], 'explicit_guard_hold')
        self.assertEqual(result['returned_refused_observations'][0]['offset'], '0x00111668')
        self.assertFalse(result['hardware_call_hang_established'])
        self.assertFalse(result['microcode_crash_established'])

    def test_handoff_marker_is_not_os_outcome(self):
        status = 'windows-StartImage-next status=0x0000000000000000\n'
        for outcome, supported in [('unknown', False), ('manual', False), ('automatic', True)]:
            result = ar.summarize('', status, outcome)
            self.assertEqual(result['automatic_windows_return_supported'], supported)
            self.assertFalse(result['unlock_established'])

    def test_conflicting_records_or_recollection_require_review(self):
        held = 'handoff-stopped-manual-power-off status=0x8000000000000006\n'
        handoff = 'windows-StartImage-next status=0x0000000000000000\n'
        for status, outcome in [(held + handoff, 'unknown'), (held, 'automatic')]:
            result = ar.summarize('', status, outcome)
            self.assertEqual(result['log_classification'], 'inconsistent')
            self.assertFalse(result['automatic_windows_return_supported'])

    def test_missing_or_failed_markers_remain_unknown(self):
        for text in ['', 'windows-StartImage-next status=0x8000000000000006\n', 'handoff-stopped']:
            self.assertEqual(ar.summarize(text)['log_classification'], 'outcome_unknown')

    def test_operator_recollection_alone_does_not_identify_a_run(self):
        result = ar.summarize('', '', 'automatic')
        self.assertEqual(result['log_classification'], 'outcome_unknown')
        self.assertFalse(result['automatic_windows_return_supported'])

    def test_transport_errors_and_prom_payload_not_refused_observations(self):
        text = ('ad104-falcon batch=0x01 offset=0x00111668 status=0x8000000000000007 value=0xbadf5620\n'
                'ad104-prom-full value=0xbadf5620\n')
        self.assertEqual(ar.summarize(text)['returned_refused_observations'], [])

    def test_recovery_preserves_refused_observation(self):
        text = 'ad104-falcon batch=0x01 offset=0x00111388 status=0x0000000000000000 value=0xbadf5620\n'
        status = ('refusal-rechecks-complete status=0x0000000000000000\n'
                  'refusal-decode-off-complete status=0x0000000000000000\n'
                  'windows-StartImage-next status=0x0000000000000000\n')
        result = ar.summarize(text, status, 'automatic')
        self.assertTrue(result['automatic_windows_return_supported'])
        self.assertFalse(result['returned_refused_observations'][0]['usable_data'])


if __name__ == '__main__':
    unittest.main()
