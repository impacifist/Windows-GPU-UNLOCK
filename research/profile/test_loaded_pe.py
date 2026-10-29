import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('loaded_pe', Path(__file__).with_name('normalize-loaded-pe.py'))
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def fixture():
    b = bytearray(1024)
    b[:2] = b'MZ'
    struct.pack_into('<I', b, 60, 64)
    b[64:68] = b'PE\0\0'
    struct.pack_into('<H', b, 70, 1)
    struct.pack_into('<H', b, 84, 240)
    struct.pack_into('<H', b, 88, 0x20b)
    struct.pack_into('<I', b, 144, len(b))
    struct.pack_into('<IIII', b, 336, 128, 512, 128, 400)
    b[512:516] = b'CODE'
    return b


class LoadedPeTests(unittest.TestCase):
    def test_raw_pointer_redirects_to_preserved_rva_bytes(self):
        b = fixture()
        result = mod.normalize(b)
        self.assertEqual(int.from_bytes(result[348:352], 'little'), 512)
        self.assertEqual(result[512:516], b'CODE')
        self.assertEqual(b[348:352], (400).to_bytes(4, 'little'))

    def test_truncation_and_out_of_range_sections_refused(self):
        for b in (fixture()[:500], b'', fixture()):
            if len(b) == 1024:
                struct.pack_into('<I', b, 340, 1000)
            with self.assertRaises(ValueError):
                mod.normalize(b)
