"""Compile-only family/card/firmware boundary regressions. No GPU or EFI execution."""
import argparse
import itertools
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CLANG = 'clang'


def compile_source(source, defines=(), extra=()):
    return subprocess.run([CLANG, '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsyntax-only',
                           '-x', 'c', '-', '-I', str(ROOT), *defines, *extra],
                          input=source, capture_output=True, text=True)


class FamilyTests(unittest.TestCase):
    def test_six_profiles_select_exact_family_chip_and_memory(self):
        cases = [('B650M_3090FE', 102, 0xb72000a1, 0x6000),
                 ('X870_3090TI', 102, 0xb72000a1, 0x5ff4),
                 ('B660M_3090TI', 102, 0xb72000a1, 0x5ff4),
                 ('B660M_3070TI', 104, 0xb74000a1, 0x2000),
                 ('B660PLUS_3060', 104, 0xb74000a1, 0x3000),
                 ('H610M_3060', 106, 0xb76000a1, 0x3000),
                 ('B550_3060', 104, 0xb74000a1, 0x3000)]
        for profile, family, chip, memory in cases:
            with self.subTest(profile=profile):
                source = ('#include "machine-profile.h"\n'
                          f'_Static_assert(GU_GPU_FAMILY == {family}, "family");\n'
                          f'_Static_assert(GU_CHIP_ID == {chip}, "exact revision");\n'
                          f'_Static_assert(GU_DEVICE_MEMORY_SIZE == {memory}, "card VRAM");\n')
                done = compile_source(source, ['-DGU_PROFILE_' + profile])
                self.assertEqual(done.returncode, 0, done.stderr)

    def test_wrong_family_identity_memory_or_revision_is_refused(self):
        cases = [(102, 0x221610de, 0x6000, 0xb72000a1),
                 (102, 0x220410de, 0x3000, 0xb72000a1),
                 (102, 0x220410de, 0x6000, 0xb72000a2),
                 (104, 0x248210de, 0x3000, 0xb74000a1),
                 (104, 0x248710de, 0x2000, 0xb74000a1),
                 (106, 0x250410de, 0x2000, 0xb76000a1),
                 (106, 0x248710de, 0x3000, 0xb76000a1),
                 (204, 0x278610de, 0x3000, 0x194000a1),
                 (102, 0x220410de, 0x6000, 0xb74000a1)]
        for family, board, memory, chip in cases:
            with self.subTest(family=family, board=board, memory=memory, chip=chip):
                done = compile_source('#include "gpu-family.h"\n',
                    [f'-DGU_GPU_FAMILY={family}', f'-DGU_BOARD_ID={board}U',
                     f'-DGU_DEVICE_MEMORY_SIZE={memory}U', f'-DGU_CHIP_ID={chip}U'])
                self.assertNotEqual(done.returncode, 0)

    def test_profile_family_and_firmware_cannot_be_overridden(self):
        for define in ('GU_GPU_FAMILY=104', 'GU_FIRMWARE_PROFILE=2', 'GU_GPU_FAMILY_GA102=104',
                       'GU_FIRMWARE_AMI_B650M_3057=2'):
            with self.subTest(define=define):
                done = compile_source('#include "machine-profile.h"\n', ['-D' + define])
                self.assertNotEqual(done.returncode, 0)

    def test_legacy_local_headers_keep_all_admitted_ga102_combinations(self):
        for board, memory, no_edid in itertools.product((0x220410de, 0x220310de), (0x6000, 0x5ff4), (0, 1)):
            with self.subTest(board=board, memory=memory, no_edid=no_edid), tempfile.TemporaryDirectory() as directory:
                folder = Path(directory)
                local = folder / 'profile-local.h'
                local.write_text(f'#define GU_LOCAL_PROFILE_GENERATOR 1\n#define GU_CHIP_ID 0xb72000a1U\n'
                                 f'#define GU_BOARD_ID {board}U\n#define GU_DEVICE_MEMORY_SIZE {memory}U\n'
                                 f'#define GU_TARGET_BUS 0x2bU\n#define GU_NO_EDID_DRIVER {no_edid}\n'
                                 '#define GU_NVGOP_VERSION 0x60009U\n#define GU_NVGOP_TEXT_SHA256 "capture-specific"\n')
                overlay = folder / 'overlay.json'
                overlay.write_text(json.dumps({'version': 0, 'roots': [{'type': 'file',
                    'name': str(ROOT / 'profile-local.h'), 'external-contents': str(local), 'use-external-name': False}]}))
                source = ('#include "machine-profile.h"\n_Static_assert(GU_GPU_FAMILY == 102, "family");\n'
                          '_Static_assert(GU_NVGOP_VERSION == 0x60009U, "captured firmware");\n'
                          '#ifdef GU_FIRMWARE_PROFILE\n#error LOCAL inherited fixed firmware\n#endif\n')
                if no_edid:
                    source += '_Static_assert(GU_EDID_IMAGE_SIZE == 0, "no EDID");\n'
                done = compile_source(source, ['-DGU_PROFILE_LOCAL', '-DGU_HEADLESS_RESEARCH',
                    '-DGU_FULL_UNLOCK'], ['-ivfsoverlay', str(overlay)])
                self.assertEqual(done.returncode, 0, done.stderr)

    def test_probe_only_does_not_gain_write_traits(self):
        source = ('#include "machine-profile.h"\n'
                  '#if defined(GU_CHIP_ID) || defined(GU_DEVICE_MEMORY_SIZE) || defined(GU_PROM_SHA256) || defined(GU_GPU_FAMILY) || defined(GU_FIRMWARE_PROFILE)\n'
                  '#error Identity-only probe gained write traits\n#endif\n')
        for profile in ('GU_PROFILE_PROBE_H610M_3060', 'GU_PROFILE_PROBE_B550_3060'):
            base = ['-D' + profile, '-DGU_DIAGNOSTIC_ONLY', '-DGU_DIAGNOSTIC_PROBE']
            done = compile_source(source, base)
            self.assertEqual(done.returncode, 0, done.stderr)
            for flag in ('GU_FULL_UNLOCK', 'GU_EXPERIMENTAL_3090', 'GU_UEFI_PORT', 'GU_INTERNAL_BOOT',
                         'GU_CONSOLE_PREPARE', 'GU_AUTOMATIC_BOOT', 'GU_HEADLESS_RESEARCH'):
                with self.subTest(profile=profile, flag=flag):
                    self.assertNotEqual(compile_source(source, base + ['-D' + flag]).returncode, 0)

    def test_unknown_firmware_and_local_template_inheritance_are_refused(self):
        for defines in (['-DGU_FIRMWARE_PROFILE=99'],
                        ['-DGU_PROFILE_LOCAL', '-DGU_FIRMWARE_PROFILE=2'],
                        ['-DGU_PROFILE_PROBE_H610M_3060', '-DGU_FIRMWARE_PROFILE=4'],
                        ['-DGU_PROFILE_PROBE_B550_3060', '-DGU_FIRMWARE_PROFILE=4']):
            with self.subTest(defines=defines):
                self.assertNotEqual(compile_source('#include "firmware-profile.h"\n', defines).returncode, 0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--clang', default='clang')
    args = parser.parse_args()
    CLANG = args.clang
    unittest.main(argv=[__file__])
