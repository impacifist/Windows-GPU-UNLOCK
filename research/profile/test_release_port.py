import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('release_port', Path(__file__).with_name('audit-release-port.py'))
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def fixture():
    cfg = [0]*64
    cfg[0], cfg[1], cfg[11] = 0x278210de, 7, 0x51321462
    return ('probe-target seg=0x0000 bus=0x000b dev=0x0000 fun=0x0000\r\n'
            'cfg dwords='+' '.join(f'{n:08x}' for n in cfg)+' \r\n'
            'binding handle=0xa image=0xa version=0x24 stop=0x107c4\r\n'
            'img base=0x10000\r\nimg size=0x4800\r\nimg dump index=001d\r\n'
            'binding handle=0xb image=0x0 version=0x10 stop=0x12b38\r\n')


class ReleasePortTests(unittest.TestCase):
    def test_reference_mismatches_and_null_image_callback(self):
        result = mod.audit(fixture())
        self.assertEqual(len(result['mismatches']), 3)
        self.assertEqual(result['null_image_callbacks_within_captured_images'],
                         [dict(image_index=29, stop_rva=0x2b38)])
        self.assertFalse(result['runtime_release_authorized'])

    def test_bad_or_ambiguous_input_refused(self):
        for text in ('', fixture().replace('278210de', '220410de'),
                     fixture()+fixture(), fixture().replace('img dump index=001d', 'img dump index=0000')):
            with self.assertRaises(ValueError):
                mod.audit(text)
