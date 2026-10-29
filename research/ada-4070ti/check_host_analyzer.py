"""Cross-check the reviewed Python analyzer using host-generated mock JSON only.

No firmware capture, device access, network, or GPU library is used. The one
external module loaded is the pinned, inspected pure-JSON analyzer.
"""

import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import unittest

HERE = Path(__file__).resolve().parent


class AnalyzerChecks(unittest.TestCase):
    analyze = None
    baseline = None
    characterized = []

    def evaluate(self, data):
        return self.analyze(data, self.baseline['build_id'])

    def test_original_mock_is_inventory_only(self):
        result = self.evaluate(copy.deepcopy(self.baseline))
        self.assertTrue(result['identity_observed'])
        self.assertTrue(result['physical_prom_complete'])
        self.assertTrue(result['register_snapshot_complete'])
        self.assertFalse(result['unlock_ready'])
        self.assertFalse(result['hardware_support_validated'])

    def test_capture_failures_remain_incomplete(self):
        paths = sorted((HERE / 'build').glob('prom-*.json')) + sorted((HERE / 'build').glob('register-*.json'))
        self.assertEqual(len(paths), 10)
        for path in paths:
            with self.subTest(case=path.stem):
                data = copy.deepcopy(self.baseline)
                section = 'physical_prom' if path.name.startswith('prom-') else 'register_snapshot'
                data[section] = json.loads(path.read_text())
                result = self.evaluate(data)
                self.assertFalse(result[section + '_complete'])
                self.assertFalse(result['unlock_ready'])
                self.characterized.append({'case': path.stem, 'accepted_as_partial': True,
                                           'unlock_ready': result['unlock_ready']})

    def test_graph_limits_are_still_inventory_only(self):
        paths = sorted((HERE / 'build').glob('graph-*.json'))
        self.assertEqual(len(paths), 5)
        fingerprints = []
        for path in paths:
            with self.subTest(case=path.stem):
                data = copy.deepcopy(self.baseline)
                data['platform'] = json.loads(path.read_text())
                result = self.evaluate(data)
                self.assertTrue(result['platform_graph_complete'])
                self.assertFalse(result['unlock_ready'])
                self.assertFalse(result['hardware_support_validated'])
                fingerprints.append(result['stable_fingerprint'])
                self.characterized.append({'case': path.stem,
                    'platform_graph_complete': result['platform_graph_complete'],
                    'verified_code_captures': result['driver_code_captures_verified'],
                    'unlock_ready': result['unlock_ready']})
        # Fingerprint is the target inventory subset, not an ownership hash.
        self.assertEqual(len(set(fingerprints)), 1)

    def test_rejects_claimed_write_or_release_authority(self):
        for flag in ('gpu_writes', 'unlock_attempted', 'hardware_support_validated'):
            data = copy.deepcopy(self.baseline)
            data[flag] = True
            with self.subTest(flag=flag), self.assertRaises(ValueError):
                self.evaluate(data)
        for flag in ('release_authorized', 'atomic_snapshot'):
            data = copy.deepcopy(self.baseline)
            data['platform'] = json.loads((HERE / 'build/graph-target-absent.json').read_text())
            data['platform'][flag] = True
            with self.subTest(flag=flag), self.assertRaises(ValueError):
                self.evaluate(data)

    def test_rejects_corrupt_or_short_prom(self):
        for mutation in ('short', 'hash', 'attempts'):
            data = copy.deepcopy(self.baseline)
            prom = data['physical_prom']
            if mutation == 'short': prom['hex'] = prom['hex'][:-2]
            if mutation == 'hash': prom['sha256'] = '0' * 64
            if mutation == 'attempts': prom['read_attempts'] = 260
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                self.evaluate(data)

    def test_rejects_inconsistent_register_snapshot(self):
        for mutation in ('missing', 'offset', 'attempts', 'final_chip'):
            data = copy.deepcopy(self.baseline)
            snapshot = data['register_snapshot']
            if mutation == 'missing': snapshot['samples'].pop()
            if mutation == 'offset': snapshot['samples'][1]['offset'] = '0xffffffff'
            if mutation == 'attempts': snapshot['read_attempts'] = 999
            if mutation == 'final_chip': snapshot['final_chip'] = '0x00000000'
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                self.evaluate(data)

    def test_rejects_corrupt_graph_code_or_edge(self):
        for mutation in ('code', 'edge'):
            data = copy.deepcopy(self.baseline)
            platform = json.loads((HERE / 'build/graph-target-absent.json').read_text())
            data['platform'] = platform
            if mutation == 'code': platform['gpu_agents'][0]['code']['hex'] = '00' * 16
            if mutation == 'edge': platform['handles'][1]['active_opens'][0]['agent'] = 4096
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                self.evaluate(data)

    def test_rejects_wrong_build_id_and_target_count(self):
        with self.assertRaises(ValueError):
            self.analyze(copy.deepcopy(self.baseline), 'wrong-build')
        data = copy.deepcopy(self.baseline)
        data['target_id_count'] = 2
        with self.assertRaises(ValueError):
            self.evaluate(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('checkout', type=Path)
    args = parser.parse_args()
    root = args.checkout.resolve()
    provenance = json.loads((HERE / 'host-source-provenance.json').read_text())
    for name, expected in provenance['files'].items():
        path = (root / name).resolve()
        if not path.is_relative_to(root) or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            parser.error('Reviewed source mismatch: ' + name)
    spec = importlib.util.spec_from_file_location('reviewed_ad104_analyzer',
        root / 'windows/ad104-msi/diagnostic/tools/analyze.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    AnalyzerChecks.analyze = staticmethod(module.analyze)
    AnalyzerChecks.baseline = json.loads((HERE / 'build/host-capture.json').read_text())
    result = unittest.TextTestRunner(stream=sys.stderr, verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(AnalyzerChecks))
    if not result.wasSuccessful():
        return 1
    summary = {'scope': 'pure Python analyzer with synthetic host captures',
        'reviewed_source_commit': provenance['commit'], 'hardware_accessed': False,
        'test_methods_passed': result.testsRun, 'characterizations': AnalyzerChecks.characterized,
        'research_source_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'limit': 'Partial/graph-complete is inventory metadata; support and unlock readiness remain false.'}
    (HERE / 'build/analyzer-results.json').write_text(json.dumps(summary, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
