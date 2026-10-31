import unittest
import uuid
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location('inspect_recorded_esp', Path(__file__).with_name('inspect-recorded-esp.py'))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
inspect, inspect_capture, SFS, DEVICE_PATH = module.inspect, module.inspect_capture, module.SFS, module.DEVICE_PATH


class RecordedEspTests(unittest.TestCase):
    def setUp(self):
        self.guid = uuid.UUID('c36c3e86-02aa-4f76-af1b-96e327f9d1c1')
        node = bytearray(42)
        node[:4] = bytes.fromhex('04012a00')
        node[24:40] = self.guid.bytes_le
        node[40:42] = b'\x02\x02'
        self.entry = (f'handle id=0x01\nprotocol guid={SFS}\n'
                      f'protocol guid={DEVICE_PATH}\npath-node raw={node.hex()}\n')

    def test_target_and_protocols(self):
        expected = {'matching_path_handles': 1, 'matching_filesystem_handles': 1}
        self.assertEqual(inspect(self.entry + 'capture-complete=1\n' + self.entry + 'capture-complete=1', self.guid), [expected, expected])

    def test_wrong_guid_and_missing_protocol(self):
        self.assertEqual(inspect_capture(self.entry, uuid.uuid4())['matching_path_handles'], 0)
        self.assertEqual(inspect_capture(self.entry.replace(SFS, 'other'), self.guid)['matching_filesystem_handles'], 0)

    def test_partial_capture_refused(self):
        with self.assertRaises(ValueError):
            inspect(self.entry + 'capture-complete=1', self.guid)


if __name__ == '__main__':
    unittest.main()
