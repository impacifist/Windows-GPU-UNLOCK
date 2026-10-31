import importlib.util
from pathlib import Path
import unittest
spec = importlib.util.spec_from_file_location('console_code', Path(__file__).with_name('inspect-evga3090-console-code.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
class ConsoleCodeTests(unittest.TestCase):
    def test_matching(self):
        self.assertTrue(m.compare([{'roles': {'a': 1}}, {'roles': {'a': 1}}]))
    def test_changed_code(self):
        self.assertFalse(m.compare([{'roles': {'a': 1}}, {'roles': {'a': 2}}]))
    def test_one_capture_refused(self):
        self.assertFalse(m.compare([{'roles': {}}]))
    def test_empty_refused(self):
        self.assertFalse(m.compare([]))
    def test_incomplete_log_refused(self):
        with self.assertRaises(ValueError):
            m.fingerprint(Path('.'), 'capture-complete=0')
