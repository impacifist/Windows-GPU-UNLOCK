import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('rom_chain', Path(__file__).with_name('inspect-rom-chain.py'))
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def image(last=True):
    b = bytearray(512)
    b[:2] = b'\x55\xaa'
    b[24] = 32
    b[32:36] = b'PCIR'
    b[36:40] = bytes.fromhex('de108227')
    b[42] = 24
    b[48] = 1
    b[53] = 128 if last else 0
    b[-1] = (-sum(b)) & 255
    return b


class RomTests(unittest.TestCase):
    def test_two_images_and_exact_end(self):
        result = mod.inspect(image(False)+image())
        self.assertEqual(len(result['images']), 2)
        self.assertFalse(result['physical_prom_complete'])

    def test_corruption_and_missing_end_refused(self):
        for at in (0, 24, 32, 36, 42, 48, 511):
            b = image()
            b[at] ^= 255
            with self.subTest(at=at), self.assertRaises(ValueError):
                mod.inspect(b)
        for b in (image(False), image()+b'x', image()[:100], b''):
            with self.assertRaises(ValueError):
                mod.inspect(b)
