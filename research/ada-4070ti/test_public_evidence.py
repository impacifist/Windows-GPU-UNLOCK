"""Negative tests for offline evidence validation; no GPU dependencies."""

import copy
import hashlib
from pathlib import Path
import tempfile
import unittest

from check_public_evidence import validate_run, verify_sources


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.reference = dict.fromkeys(
            ('schema', 'python', 'torch', 'torch_cuda', 'gpu', 'capability',
             'benchmark_sha256', 'cublas_sha256', 'cublasLt_sha256',
             'cublas_version', 'math_mode', 'seed', 'warmup_seconds'), 'fixture')
        self.reference['initial_telemetry'] = {
            'fields': ['name', 'bus', 'id', 'driver', 'vbios', 'power'],
            'csv': 'GPU, bus, id, driver, vbios, 285', 'returncode': 0}
        self.reference['results'] = [{
            'mode': 'fixture', 'n': 1000, 'samples': 3, 'batch': 5,
            'compute_type': 0, 'input_type': 0, 'output_type': 'FP32',
            'output_sha256': 'a' * 64, 'full_256_exact': True,
            'sampled_large_exact': True, 'milliseconds': [1.0, 2.0, 3.0],
            'median_ms': 2.0, 'min_ms': 1.0, 'max_ms': 3.0,
            'median_tflops': 1.0}]
        self.run_data = copy.deepcopy(self.reference)

    def test_known_arithmetic(self):
        validate_run(self.run_data, self.reference)

    def test_rejects_changed_driver(self):
        self.run_data['initial_telemetry']['csv'] = 'GPU, bus, id, changed, vbios, 285'
        with self.assertRaisesRegex(ValueError, 'Identity'):
            validate_run(self.run_data, self.reference)

    def test_rejects_changed_output(self):
        self.run_data['results'][0]['output_sha256'] = 'b' * 64
        with self.assertRaisesRegex(ValueError, 'output_sha256'):
            validate_run(self.run_data, self.reference)

    def test_rejects_false_throughput(self):
        self.run_data['results'][0]['median_tflops'] = 2.0
        with self.assertRaisesRegex(ValueError, 'Recalculation'):
            validate_run(self.run_data, self.reference)

    def test_rejects_nonfinite_sample(self):
        self.run_data['results'][0]['milliseconds'][0] = float('nan')
        with self.assertRaisesRegex(ValueError, 'Invalid timing'):
            validate_run(self.run_data, self.reference)

    def test_rejects_missing_sample(self):
        self.run_data['results'][0]['milliseconds'].pop()
        with self.assertRaisesRegex(ValueError, 'Sample count'):
            validate_run(self.run_data, self.reference)

    def test_rejects_failed_numerical_check(self):
        self.run_data['results'][0]['full_256_exact'] = False
        with self.assertRaisesRegex(ValueError, 'validation absent'):
            validate_run(self.run_data, self.reference)

    def test_rejects_replaced_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'source.txt'
            path.write_bytes(b'reviewed')
            provenance = {'files': {'source.txt': hashlib.sha256(b'reviewed').hexdigest()}}
            verify_sources(root, provenance)
            path.write_bytes(b'replaced')
            with self.assertRaisesRegex(ValueError, 'Reviewed source hash mismatch'):
                verify_sources(root, provenance)

    def test_rejects_source_path_escape(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, 'Source path escapes'):
                verify_sources(Path(directory), {'files': {'../escape': '0' * 64}})


if __name__ == '__main__':
    unittest.main()
