import importlib.util
from pathlib import Path
import sys
import unittest

spec = importlib.util.spec_from_file_location('evga_console_model', Path(__file__).with_name('evga3090-console-model.py'))
m = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = m
spec.loader.exec_module(m)


class ConsoleModelTests(unittest.TestCase):
    def test_modeled_order_never_grants_admission(self):
        result = m.replay(m.EXPECTED)
        self.assertTrue(result['model_passed'])
        self.assertFalse(result['write_eligible'])
        self.assertFalse(result['unlock_proven'])
        self.assertFalse(result['hardware_accessed'])

    def test_graphics_first_refused(self):
        self.assertFalse(m.replay(m.EXPECTED, tuple(reversed(m.ORDER)))['model_passed'])

    def test_unknown_consumer_refused(self):
        extra = m.Open('virtual', 'gop', 'unknown', 'virtual', 16)
        self.assertFalse(m.replay([*m.EXPECTED, extra])['model_passed'])

    def test_missing_third_consumer_refused(self):
        edges = [e for e in m.EXPECTED if e.agent != 'virtual_text']
        self.assertFalse(m.replay(edges)['model_passed'])

    def test_duplicate_refused(self):
        self.assertFalse(m.replay([*m.EXPECTED, next(iter(m.EXPECTED))])['model_passed'])

    def test_success_without_edge_change_refused(self):
        self.assertFalse(m.replay(m.EXPECTED, observed_after=[m.EXPECTED] * 3)['model_passed'])

    def test_incomplete_observations_refused(self):
        self.assertFalse(m.replay(m.EXPECTED, observed_after=[])['model_passed'])

    def test_callback_failure_refused(self):
        self.assertFalse(m.replay(m.EXPECTED, callbacks_succeeded=False)['model_passed'])

    def test_wrong_open_count_refused(self):
        old = next(iter(m.EXPECTED))
        changed = m.Open(old.source, old.protocol, old.agent, old.controller, old.attributes, 2)
        self.assertFalse(m.replay([e for e in m.EXPECTED if e != old] + [changed])['model_passed'])
