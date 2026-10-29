import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('consumers', Path(__file__).with_name('map-active-consumers.py'))
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def fixture():
    return (f'handle id=0x1\nprotocol guid={mod.PCI}\n'
            'open agent=0xa controller=0x1 attributes=0x10 count=0x1\n'
            'open agent=0xa controller=0x2 attributes=0x8 count=0x1\n'
            'pci config-dwords=278210de 00000007\n'
            f'handle id=0x2\nprotocol guid={mod.GOP}\nprotocol guid={mod.EDID}\n'
            'open agent=0xb controller=0x2 attributes=0x10 count=0x1\n'
            f'handle id=0x3\nprotocol guid={sorted(mod.POINTERS)[0]}\n'
            'open agent=0xb controller=0x3 attributes=0x10 count=0x1\n')


class ConsumerTests(unittest.TestCase):
    def test_shared_role_detected_but_never_authorized(self):
        result = mod.inspect(fixture())
        shared = [c for c in result['consumers'] if c['shared_edid_pointer_role']]
        self.assertEqual(len(shared), 1)
        self.assertEqual(shared[0]['outside_gpu_handles'], 1)
        self.assertFalse(result['runtime_disconnect_authorized'])

    def test_passive_pointer_open_is_not_active_coupling(self):
        text = fixture().replace('agent=0xb controller=0x3 attributes=0x10',
                                 'agent=0xb controller=0x3 attributes=0x2')
        self.assertFalse(any(c['shared_edid_pointer_role'] for c in mod.inspect(text)['consumers']))

    def test_ambiguous_or_broken_graph_refused(self):
        for text in ('', fixture()+fixture(), fixture().replace('controller=0x2 attributes=0x8', 'controller=0x0 attributes=0x8'),
                     fixture().replace(f'protocol guid={mod.GOP}\n', ''),
                     fixture().replace('agent=0xa controller=0x2', 'agent=0xc controller=0x2')):
            with self.assertRaises(ValueError):
                mod.inspect(text)

    def test_null_controller_application_exclusive_is_supported(self):
        text = fixture()+f'handle id=0x4\nprotocol guid={mod.PCI}\nopen agent=0xc controller=0x0 attributes=0x20 count=0x1\n'
        self.assertEqual(mod.inspect(text)['active_edges_total'], 5)
