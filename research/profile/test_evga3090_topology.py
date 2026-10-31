import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('evga_topology', Path(__file__).with_name('inspect-evga3090-topology.py'))
topology = importlib.util.module_from_spec(spec)
spec.loader.exec_module(topology)


class TopologyTests(unittest.TestCase):
    def setUp(self):
        self.text = ('console out=0x4 error=0x4 input=0x4\n'
                     'capture-complete=1\ndiagnostic-result status=0x0000000000000000\n')
        pci = 'path-node raw=02010c00d041030a00000000\npath-node raw=010106000001\n'
        end = 'path-node raw=7fff0400\n'
        cfg = [0x220410de, 3, 0x030000a1] + [0] * 13
        self.blocks = {
            1: {'protocols': [], 'edges': [], 'raw': 'pci config-status=0x0000000000000000\n'
                + 'pci config-dwords=' + ' '.join(f'{v:08x}' for v in cfg) + '\n' + pci + end},
            2: {'protocols': [topology.mp.GOP], 'edges': [
                (topology.mp.GOP, 10, 2, 16, 1), (topology.mp.GOP, 10, 3, 8, 1)],
                'raw': pci + 'path-node raw=0203080000000000\n' + end +
                'gop framebuffer=0x100000 bytes=0x200000 mode=0x0\n'},
            3: {'protocols': [topology.mp.GOP, topology.TEXT], 'edges': [
                (topology.TEXT, 11, 3, 16, 1), (topology.TEXT, 11, 4, 8, 1)],
                'raw': 'gop framebuffer=0x0 bytes=0x0 mode=0x0\n'},
            4: {'protocols': [topology.TEXT], 'edges': [], 'raw': ''}}
        self.bindings = [{'handle': 10, 'file': ('guid', topology.GFX)},
                         {'handle': 11, 'file': ('guid', topology.TEXT_SPLITTER)}]

    def inspect(self):
        with patch.object(topology.mp, 'parse_handles', return_value=self.blocks), \
                patch.object(topology.mp, 'parse_bindings', return_value=self.bindings):
            return topology.inspect(self.text)

    def test_expected_chain_does_not_grant_eligibility(self):
        result = self.inspect()
        self.assertTrue(result['expected_console_chain_observed'])
        self.assertFalse(result['write_eligible'])
        self.assertFalse(result['unlock_proven'])

    def test_windows_crlf_is_accepted(self):
        self.text = self.text.replace('\n', '\r\n')
        self.assertTrue(self.inspect()['expected_console_chain_observed'])

    def test_wrong_child_agent_does_not_establish_shared_driver(self):
        self.blocks[2]['edges'][1] = (topology.mp.GOP, 99, 3, 8, 1)
        self.assertFalse(self.inspect()['expected_console_chain_observed'])

    def test_repeated_owner_is_ambiguous(self):
        self.blocks[2]['edges'].append(self.blocks[2]['edges'][0])
        self.assertFalse(self.inspect()['expected_console_chain_observed'])

    def test_wrong_binding_identity_is_not_qualified(self):
        self.bindings[0]['file'] = ('guid', topology.TEXT_SPLITTER)
        self.assertFalse(self.inspect()['expected_console_chain_observed'])

    def test_nonunit_count_is_not_expected_chain(self):
        self.blocks[3]['edges'][1] = (topology.TEXT, 11, 4, 8, 2)
        self.assertFalse(self.inspect()['expected_console_chain_observed'])

    def test_second_framebuffer_prevents_pathless_virtual_inference(self):
        self.blocks[3]['raw'] = 'gop framebuffer=0x1000 bytes=0x1000 mode=0x0\n'
        self.assertFalse(self.inspect()['expected_console_chain_observed'])

    def test_missing_physical_framebuffer_is_not_accepted(self):
        self.blocks[2]['raw'] = self.blocks[2]['raw'].replace('framebuffer=0x100000', 'framebuffer=0x0')
        self.assertFalse(self.inspect()['expected_console_chain_observed'])

    def test_missing_console_handle_refuses(self):
        del self.blocks[4]
        with self.assertRaises(ValueError):
            self.inspect()

    def test_partial_capture_refuses(self):
        self.text = self.text.replace('capture-complete=1', '')
        with self.assertRaises(ValueError):
            self.inspect()


if __name__ == '__main__':
    unittest.main()
