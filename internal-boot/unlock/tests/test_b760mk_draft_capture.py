"""Draft B760M-K/RTX 3060 profile versus its own read-only capture. Preprocess-only; no GPU or EFI.

Every pinned value in GU_PROFILE_DRAFT_B760MK_3060 must equal what the capture
(DIAG00.TXT, PROM.BIN, IMG*.BIN) records, parsed with research/profile/make-profile.py.
The capture is private and not committed: set GU_B760MK_CAPTURE or keep it at
../scans/RTX3090-UNLOCK next to the repository.
"""
import argparse
import hashlib
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
CLANG = 'clang'
CAPTURE = Path(os.environ.get('GU_B760MK_CAPTURE', REPO.parent / 'scans' / 'RTX3090-UNLOCK'))
PROFILE = ['-DGU_PROFILE_DRAFT_B760MK_3060', '-DGU_DRAFT_REVIEW']

sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location('make_profile', REPO / 'research' / 'profile' / 'make-profile.py')
mp = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mp)

COMBO_FILE = '55987e62-1df6-41d1-9062-334fcac3e54d'
MACROS = ('GU_BOARD_ID', 'GU_BOARD_SUBSYSTEM', 'GU_CHIP_ID', 'GU_DEVICE_MEMORY_SIZE',
          'GU_BAR0_BASE', 'GU_BAR0_END', 'GU_PROM_SHA256', 'GU_ROOT_PORT_DEVICE',
          'GU_ROOT_PORT_FUNCTION', 'GU_RTX_PATH_BYTES', 'GU_CONSOLE_AGGREGATE',
          'GU_CONSOLE_TEXT_SIZE', 'GU_CONSOLE_TEXT_STOP', 'GU_CONSOLE_TEXT_VERSION',
          'GU_CONSOLE_GFX_SIZE', 'GU_CONSOLE_GFX_STOP', 'GU_CONSOLE_GFX_VERSION',
          'GU_COMBO_SIZE', 'GU_COMBO_STOP', 'GU_COMBO_VERSION',
          'GU_EDID_IMAGE_SIZE', 'GU_EDID_STOP', 'GU_EDID_TEXT_BYTES', 'GU_EDID_TEXT_SHA256',
          'GU_NVGOP_VERSION', 'GU_NVGOP_IMAGE_SIZE', 'GU_NVGOP_STOP',
          'GU_NVGOP_TEXT_BYTES', 'GU_NVGOP_TEXT_SHA256')


def profile_values():
    """Expand each pinned macro of the draft profile through the real headers."""
    names = (*MACROS, 'GU_CONSOLE_MULTI_CHILD')
    # Keys are indexes so the preprocessor cannot expand them.
    source = '#include "machine-profile.h"\n' + ''.join(f'@{i}@ {m}\n' for i, m in enumerate(MACROS))
    source += f'#ifdef GU_CONSOLE_MULTI_CHILD\n@{len(MACROS)}@ 1\n#else\n@{len(MACROS)}@ 0\n#endif\n'
    done = subprocess.run([CLANG, '-E', '-P', '-x', 'c', '-', '-I', str(ROOT), *PROFILE],
                          input=source, capture_output=True, text=True)
    if done.returncode:
        raise AssertionError(done.stderr)
    values = {}
    for line in done.stdout.splitlines():
        row = re.fullmatch(r'@(\d+)@ (.*)', line.strip())
        if not row:
            continue
        name, text = names[int(row[1])], row[2].strip()
        if text.startswith('"'):
            values[name] = text.strip('"')
        elif ',' in text:
            values[name] = bytes(int(v.strip(), 0) for v in text.split(','))
        else:
            values[name] = int(re.sub(r'[uUlL]+$', '', text), 0)
    return values


def capture():
    text = (CAPTURE / 'DIAG00.TXT').read_text(encoding='utf-8-sig').replace('\r\n', '\n')
    return text, text.split('\n')


def rtx_gop_paths(lines):
    """Device paths of every GOP that has one, as img-path-node records after its gop line."""
    paths, current = [], None
    for line in lines:
        if line.startswith('gop handle='):
            current = [] if line.endswith('has-path=1') else None
            if current is not None:
                paths.append(current)
        elif current is not None:
            row = re.fullmatch(r'img-path-node raw=([0-9a-f]+)', line)
            if row:
                current.append(bytes.fromhex(row[1]))
            elif not line.startswith('img '):
                current = None
    return [b''.join(nodes) for nodes in paths]


def driver_records(lines, key):
    return [b for b in mp.parse_bindings(lines) if b['file'] == key]


@unittest.skipUnless((CAPTURE / 'DIAG00.TXT').is_file(), f'capture not found at {CAPTURE}')
class DraftCaptureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.values = profile_values()
        cls.text, cls.lines = capture()
        cls.mmio, _ = mp.probe_mmio(cls.text)

    def assertPinned(self, macro, observed):
        self.assertEqual(self.values[macro], observed, f'{macro} differs from the capture')

    def test_card_identity_matches_capture(self):
        cfg = re.search(r'^cfg dwords=(\S+)', self.text, re.M)
        subsystem = re.search(r'^subsystem=0x([0-9a-f]+)$', self.text, re.M)
        self.assertPinned('GU_BOARD_ID', int(cfg[1], 16))
        self.assertPinned('GU_BOARD_SUBSYSTEM', int(subsystem[1], 16))
        self.assertPinned('GU_CHIP_ID', self.mmio[0x0])
        self.assertPinned('GU_DEVICE_MEMORY_SIZE', self.mmio[0x1183a4])

    def test_bar0_window_matches_capture(self):
        bar0 = int(re.search(r'^probe-bar0 base=0x([0-9a-f]+)$', self.text, re.M)[1], 16)
        self.assertPinned('GU_BAR0_BASE', bar0)
        # The generator's 16 MiB non-prefetchable window rule (make-profile.py).
        self.assertPinned('GU_BAR0_END', bar0 + 0xffffff)

    def test_prom_digest_matches_dump_and_diag(self):
        dump = hashlib.sha256((CAPTURE / 'PROM.BIN').read_bytes()).hexdigest()
        diag = re.search(r'^prom sha256=([0-9a-f]{64})$', self.text, re.M)[1]
        self.assertEqual(dump, diag, 'PROM.BIN is not the dump DIAG00.TXT recorded')
        self.assertPinned('GU_PROM_SHA256', dump)

    def test_display_path_and_root_port_match_the_only_rtx_gop(self):
        paths = rtx_gop_paths(self.lines)
        self.assertEqual(len(paths), 1, 'iGPU-off capture must have exactly one GOP with a path')
        path = paths[0]
        self.assertPinned('GU_RTX_PATH_BYTES', path)
        root = path[12:18]
        self.assertEqual(root[:4], b'\x01\x01\x06\x00', 'second node must be the root-port PCI node')
        self.assertPinned('GU_ROOT_PORT_FUNCTION', root[4])
        self.assertPinned('GU_ROOT_PORT_DEVICE', root[5])

    def test_console_topology_is_single_child_aggregate(self):
        self.assertEqual(re.search(r'^gops=([0-9a-f]+)$', self.text, re.M)[1], '0002')
        self.assertTrue(driver_records(self.lines, ('guid', COMBO_FILE)), 'aggregate needs the combo splitter')
        self.assertPinned('GU_CONSOLE_AGGREGATE', 1)
        self.assertPinned('GU_CONSOLE_MULTI_CHILD', 0)

    def test_console_and_combo_fingerprints_match_capture(self):
        for prefix, key in (('GU_CONSOLE_TEXT', ('guid', mp.OUT_FILE)),
                            ('GU_CONSOLE_GFX', ('guid', mp.GFX_FILE)),
                            ('GU_COMBO', ('guid', COMBO_FILE))):
            with self.subTest(driver=prefix):
                records = driver_records(self.lines, key)
                self.assertTrue(records, f'{prefix} driver not in the capture')
                self.assertEqual({r['size'] for r in records}, {self.values[prefix + '_SIZE']})
                self.assertEqual({r['version'] for r in records}, {self.values[prefix + '_VERSION']})
                self.assertIn(self.values[prefix + '_STOP'], {r['stop'] - r['base'] for r in records})

    def test_edid_and_nvgop_fingerprints_match_capture(self):
        for prefix, key, rva, size in (('GU_EDID', ('guid', mp.MOUSE_FILE), mp.EDID_TEXT_RVA, 'GU_EDID_IMAGE_SIZE'),
                                       ('GU_NVGOP', ('name', mp.NVGOP_NAME), mp.NVGOP_TEXT_RVA, 'GU_NVGOP_IMAGE_SIZE')):
            with self.subTest(driver=prefix):
                records = driver_records(self.lines, key)
                self.assertEqual(len(records), 1, f'{prefix}: expected one binding')
                rec = records[0]
                self.assertPinned(size, rec['size'])
                self.assertPinned(prefix + '_STOP', rec['stop'] - rec['base'])
                code = mp.code_hash(CAPTURE, rec, rva)
                self.assertPinned(prefix + '_TEXT_BYTES', code['bytes'])
                self.assertPinned(prefix + '_TEXT_SHA256', code['sha256'])
                if prefix == 'GU_NVGOP':
                    self.assertPinned('GU_NVGOP_VERSION', rec['version'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--clang', default='clang')
    args, rest = parser.parse_known_args()
    CLANG = args.clang
    unittest.main(argv=[sys.argv[0], *rest])
