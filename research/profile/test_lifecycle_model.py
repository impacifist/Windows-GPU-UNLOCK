import importlib.util
from pathlib import Path
import sys
import unittest
from dataclasses import replace

spec = importlib.util.spec_from_file_location('lifecycle_model', Path(__file__).with_name('lifecycle-model.py'))
model = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = model
spec.loader.exec_module(model)


class MockFirmware:
    def __init__(self, stage=0, fault=None):
        self.before = model.fixture()
        self.stage, self.fault = stage, fault
        self.calls = []

    def capture(self):
        current = model.predicted_snapshot(self.before, len(self.calls))
        if len(self.calls) == self.stage and self.fault:
            if self.fault == 'capture-exception':
                raise RuntimeError('simulated capture failure')
            if callable(self.fault):
                return self.fault(current)
        return current

    def attempt(self, action, mask):
        self.calls.append((action, mask))
        if self.fault == 'callback-exception' and len(self.calls) == self.stage:
            raise RuntimeError('simulated callback failure')
        return self.fault != 'callback-error' or len(self.calls) != self.stage


class LifecycleModelTests(unittest.TestCase):
    def test_success_never_grants_runtime_windows_or_unlock(self):
        firmware = MockFirmware()
        result = model.run_simulation(firmware)
        self.assertEqual(len(firmware.calls), 4)
        self.assertEqual(firmware.calls[-1], ('enable-original-io-mem-only', 0x300))
        for key in ('runtime_authorized', 'windows_authorized', 'unlock_authorized'):
            self.assertFalse(result[key])

    def test_each_callback_failure_stops_without_retry_or_restore(self):
        for stage in range(1, 5):
            with self.subTest(stage=stage):
                fw = MockFirmware(stage, 'callback-error')
                result = model.run_simulation(fw)
                self.assertEqual(len(fw.calls), stage)
                self.assertTrue(result['mutation_attempted'])
                self.assertEqual(result['outcome'], 'callback-failed-state-unknown')

    def test_every_stage_refuses_wrong_or_stale_observations(self):
        def config_drift(s):
            cfg = list(s.config)
            cfg[4] ^= 0x1000000
            return replace(s, config=tuple(cfg))
        def bme_in_config(s):
            cfg = list(s.config)
            cfg[1] ^= 4
            return replace(s, config=tuple(cfg))
        faults = [lambda s: replace(s, generation=0),
                  lambda s: replace(s, edges=s.edges | {'unexpected-consumer'}),
                  lambda s: replace(s, edges=s.edges | {'owner'}),
                  lambda s: replace(s, keyboard='lost'),
                  lambda s: replace(s, boot_image='changed'),
                  lambda s: replace(s, attributes=s.attributes ^ 0x400),
                  lambda s: replace(s, attributes=s.attributes ^ 0x18),
                  lambda s: replace(s, supported=0),
                  lambda s: replace(s, bar_descriptor=(64, 0, 0, 0, 0, 0)),
                  lambda s: replace(s, bdf=(0, 1, 0, 0)), config_drift, bme_in_config]
        for stage in range(1, 5):
            for fault in faults:
                fw = MockFirmware(stage, fault)
                # Adding 'owner' is no drift while it is expected to remain.
                expected = model.predicted_snapshot(fw.before, stage)
                if fault(expected) == expected:
                    continue
                with self.subTest(stage=stage, fault=fault):
                    result = model.run_simulation(fw)
                    if result['outcome'].startswith('synthetic-'):
                        self.fail('invalid postcondition accepted')
                    self.assertEqual(len(fw.calls), stage)

    def test_invalid_initial_identity_causes_zero_attempts(self):
        fw = MockFirmware()
        fw.before = replace(fw.before, owner_fingerprint=(0x7000b, 162016, 0x6ec, 'other'))
        result = model.run_simulation(fw)
        self.assertEqual(fw.calls, [])
        self.assertFalse(result['mutation_attempted'])

    def test_extra_command_bits_refused_before_attempt(self):
        fw = MockFirmware()
        config = list(fw.before.config)
        config[1] |= 0x100
        fw.before = replace(fw.before, config=tuple(config))
        self.assertFalse(model.run_simulation(fw)['mutation_attempted'])
        self.assertEqual(fw.calls, [])

    def test_exceptions_preserve_unknown_state_and_never_continue(self):
        for stage in range(1, 5):
            for fault in ('callback-exception', 'capture-exception'):
                fw = MockFirmware(stage, fault)
                result = model.run_simulation(fw)
                self.assertEqual(len(fw.calls), stage)
                self.assertTrue(result['mutation_attempted'])
                self.assertIn('state-unknown', result['outcome'])
                self.assertFalse(result['windows_authorized'])
        fw = MockFirmware(0, 'capture-exception')
        self.assertFalse(model.run_simulation(fw)['mutation_attempted'])
        self.assertEqual(fw.calls, [])
