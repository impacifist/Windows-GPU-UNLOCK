"""Profile selection/fingerprint and unsupported-route regressions; no hardware."""
import argparse
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
CLANG = 'clang'


def macros(*defines):
    result = subprocess.run([CLANG, '-dM', '-E', '-x', 'c', '-', *defines],
                            input='#include "machine-profile.h"\n', cwd=ROOT,
                            capture_output=True, text=True)
    values = dict(re.findall(r'^#define (GU_\w+) (.*)$', result.stdout, re.M))
    return result, values


class ProfileTests(unittest.TestCase):
    def test_exact_gpu_and_shared_machine_fingerprints(self):
        result, new = macros('-DGU_PROFILE_B660M_3090TI')
        self.assertEqual(result.returncode, 0, result.stderr)
        _, sibling = macros('-DGU_PROFILE_B660M_3070TI')
        _, ga102 = macros('-DGU_PROFILE_X870_3090TI')
        for key, expected in {'GU_BOARD_ID': 0x220310de, 'GU_BOARD_SUBSYSTEM': 0x13017377,
                              'GU_BAR0_BASE': 0x82000000, 'GU_BAR0_END': 0x82ffffff,
                              'GU_CONSOLE_AGGREGATE': 1}.items():
            with self.subTest(key=key):
                self.assertEqual(int(new[key].rstrip('ULul'), 0), expected)
        for key in ('GU_CHIP_ID', 'GU_DEVICE_MEMORY_SIZE'):
            self.assertEqual(new[key], ga102[key])
        shared = [key for key in sibling if key.startswith(('GU_CONSOLE_', 'GU_EDID_', 'GU_NVGOP_', 'GU_COMBO_', 'GU_ROOT_PORT_'))]
        shared.append('GU_RTX_PATH_BYTES')
        self.assertGreater(len(shared), 15)
        for key in shared:
            with self.subTest(key=key):
                self.assertEqual(new[key], sibling[key])
        self.assertNotEqual(new['GU_PROM_SHA256'], sibling['GU_PROM_SHA256'])
        self.assertNotEqual(new['GU_PROM_SHA256'], ga102['GU_PROM_SHA256'])

    def test_default_and_mutually_exclusive_selection(self):
        result, default = macros()
        self.assertEqual(result.returncode, 0)
        self.assertIn('B650M', default['GU_PROFILE_NAME'])
        for profile in ('B650M_3090FE', 'B660M_3070TI', 'X870_3090TI', 'B660PLUS_3060', 'H610M_3060', 'PROBE_H610M_3060', 'B760MK_3060', 'LOCAL'):
            with self.subTest(profile=profile):
                result, _ = macros('-DGU_PROFILE_B660M_3090TI', '-DGU_PROFILE_' + profile)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('Select exactly one machine profile', result.stderr)

    def test_manual_write_and_usb_wrapper_remain_unavailable(self):
        commands = [
            [str(ROOT / 'build.ps1'), '-HardwareProfile', 'b660m-3090ti', '-ExperimentalUnlock', '-StockProfile', 'entryLatchedSs1Full'],
            [str(ROOT.parents[1] / 'research/headless/build-usb.ps1'), '-WindowsEspGuid', '11111111-1111-1111-1111-111111111111', '-HardwareProfile', 'b660m-3090ti'],
        ]
        for command in commands:
            with self.subTest(script=command[0]):
                result = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', *command],
                                        capture_output=True, text=True, errors='replace')
                self.assertNotEqual(result.returncode, 0)
                output = result.stdout + result.stderr
                self.assertTrue('only ships a headless write path' in output or 'ParameterArgumentValidationError' in output, output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--clang', default='clang')
    args = parser.parse_args()
    CLANG = args.clang
    unittest.main(argv=[__file__])
