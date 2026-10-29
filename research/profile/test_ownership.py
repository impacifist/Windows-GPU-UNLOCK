"""Synthetic ownership joins; no private firmware needed."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('ownership', Path(__file__).with_name('analyze-ownership.py'))
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class OwnershipTests(unittest.TestCase):
    def fixture(self, root):
        (root / 'IMG00.BIN').write_bytes(b'test')
        text = ('binding handle=0xa image=0xa version=0x1 stop=0x1\n'
                'img size=0x4\nimg dump index=0000\n'
                'handle id=0xb\nprotocol guid=' + mod.PCI + '\n'
                'open agent=0xa controller=0xb attributes=0x10 count=0x1\n'
                'open agent=0xa controller=0xc attributes=0x8 count=0x1\n'
                'pci config-dwords=278210de\nhandle id=0xc\nprotocol guid=' + mod.GOP + '\n')
        (root / 'DIAG00.TXT').write_text(text, encoding='ascii')
        return text

    def test_links_require_same_agent_and_correct_attributes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            text = self.fixture(root)
            result = mod.analyze(root)
            self.assertEqual(result['ownership'][0]['image_indices'], [0])
            self.assertTrue(result['ownership'][1]['established'])
            self.assertFalse(result['release_authorized'])
            (root / 'DIAG00.TXT').write_text(text.replace('attributes=0x10', 'attributes=0x30'))
            self.assertEqual(len(mod.analyze(root)['ownership']), 2)
            for changed in (text.replace('controller=0xc attributes=0x8', 'controller=0xc attributes=0x2'),
                            text.replace('agent=0xa controller=0xc', 'agent=0xd controller=0xc')):
                (root / 'DIAG00.TXT').write_text(changed)
                self.assertEqual(len(mod.analyze(root)['ownership']), 1)

    def test_bad_length_and_duplicate_records_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            text = self.fixture(root)
            for changed in (text.replace('img size=0x4', 'img size=0x5'),
                            text + 'handle id=0xb\n',
                            text.replace('img size=0x4\n', ''),
                            text.replace('binding handle=0xa', 'binding handle=0x0'),
                            text.replace('img dump index=0000', 'img dump index=0000\nimg dump index=0000')):
                (root / 'DIAG00.TXT').write_text(changed)
                with self.assertRaises(ValueError):
                    mod.analyze(root)

    def test_missing_or_ambiguous_target_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            text = self.fixture(root)
            for changed in (text.replace('278210de', '220410de'),
                            text + 'handle id=0xd\npci config-dwords=278210de\n'):
                (root / 'DIAG00.TXT').write_text(changed)
                with self.assertRaises(ValueError):
                    mod.analyze(root)
