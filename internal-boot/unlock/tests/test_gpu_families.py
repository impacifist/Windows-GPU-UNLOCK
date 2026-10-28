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
                 ('B760MK_3060', 106, 0xb76000a1, 0x3000)]
        for profile, family, chip, memory in cases:
            with self.subTest(profile=profile):
                source = ('#include "machine-profile.h"\n'
                          f'_Static_assert(GU_GPU_FAMILY == {family}, "family");\n'
                          f'_Static_assert(GU_CHIP_ID == {chip}, "exact revision");\n'
                          f'_Static_assert(GU_DEVICE_MEMORY_SIZE == {memory}, "card VRAM");\n')
                source += '_Static_assert(GU_TARGET_SEGMENT == 0, "reviewed segment stays zero");\n'
                done = compile_source(source, ['-DGU_PROFILE_' + profile])
                self.assertEqual(done.returncode, 0, done.stderr)

    def test_wrong_family_identity_memory_or_revision_is_refused(self):
        cases = [(102, 0x221610de, 0x6000, 0xb72000a1),
                 (102, 0x220410de, 0x3000, 0xb72000a1),
                 (102, 0x220410de, 0x6000, 0xb72000a2),
                 (104, 0x248210de, 0x3000, 0xb74000a1),
                 (104, 0x248710de, 0x2000, 0xb74000a1),
                 (104, 0x248410de, 0x3000, 0xb74000a1),
                 (104, 0x24c910de, 0x3000, 0xb74000a1),
                 (104, 0x24c910de, 0x2000, 0xb74000a2),
                 (102, 0x24c910de, 0x2000, 0xb72000a1),
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

    def test_rtx3070_uses_the_shared_ga104_implementation(self):
        done=compile_source('#include "gpu-family.h"\n_Static_assert(GU_GPU_FAMILY == 104, "GA104");\n',
            ['-DGU_PROFILE_LOCAL','-DGU_BOARD_ID=0x248410deU','-DGU_DEVICE_MEMORY_SIZE=0x2000U','-DGU_CHIP_ID=0xb74000a1U'])
        self.assertEqual(done.returncode,0,done.stderr)

    def test_rtx3060ti_uses_the_shared_ga104_implementation(self):
        done=compile_source('#include "gpu-family.h"\n_Static_assert(GU_GPU_FAMILY == 104, "GA104");\n',
            ['-DGU_PROFILE_LOCAL','-DGU_BOARD_ID=0x24c910deU','-DGU_DEVICE_MEMORY_SIZE=0x2000U','-DGU_CHIP_ID=0xb74000a1U'])
        self.assertEqual(done.returncode,0,done.stderr)

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
        base = ['-DGU_PROFILE_PROBE_H610M_3060', '-DGU_DIAGNOSTIC_ONLY', '-DGU_DIAGNOSTIC_PROBE']
        done = compile_source(source, base)
        self.assertEqual(done.returncode, 0, done.stderr)
        for flag in ('GU_FULL_UNLOCK', 'GU_EXPERIMENTAL_3090', 'GU_UEFI_PORT', 'GU_INTERNAL_BOOT',
                     'GU_CONSOLE_PREPARE', 'GU_AUTOMATIC_BOOT', 'GU_HEADLESS_RESEARCH'):
            with self.subTest(flag=flag):
                self.assertNotEqual(compile_source(source, base + ['-D' + flag]).returncode, 0)

    def test_local_topology_is_derived_without_changing_fixed_profiles(self):
        for aggregate, children in itertools.product((0, 1), (1, 2)):
            with self.subTest(aggregate=aggregate, children=children), tempfile.TemporaryDirectory() as directory:
                folder = Path(directory)
                local = folder / 'profile-local.h'
                base = ('#define GU_LOCAL_PROFILE_GENERATOR 1\n#define GU_CHIP_ID 0xb72000a1U\n'
                        '#define GU_BOARD_ID 0x220410deU\n#define GU_DEVICE_MEMORY_SIZE 0x6000U\n'
                        '#define GU_NO_EDID_DRIVER 0\n')
                local.write_text(base + f'#define GU_LOCAL_CONSOLE_AGGREGATE {aggregate}\n'
                                 f'#define GU_LOCAL_CHILD_COUNT {children}\n')
                overlay = folder / 'overlay.json'
                overlay.write_text(json.dumps({'version': 0, 'roots': [{'type': 'file',
                    'name': str(ROOT / 'profile-local.h'), 'external-contents': str(local), 'use-external-name': False}]}))
                flags = ['-DGU_PROFILE_LOCAL', '-DGU_HEADLESS_RESEARCH', '-DGU_FULL_UNLOCK']
                extra = ['-ivfsoverlay', str(overlay)]
                source = ('#include "machine-profile.h"\n'
                          f'_Static_assert(GU_CONSOLE_AGGREGATE == {aggregate}, "aggregate");\n'
                          f'_Static_assert(GU_LOCAL_CHILD_COUNT == {children}, "children");\n')
                if aggregate and children == 2:
                    source += '_Static_assert(GU_CONSOLE_MULTI_CHILD == 1 && GU_CONSOLE_CHILD_COUNT == 2, "multi child");\n'
                else:
                    source += '#ifdef GU_CONSOLE_MULTI_CHILD\n#error Unexpected multi-child path\n#endif\n'
                done = compile_source(source, flags, extra)
                self.assertEqual(done.returncode, 0, done.stderr)
                invalid = [base + '#define GU_LOCAL_CONSOLE_AGGREGATE 1\n',
                           base + '#define GU_LOCAL_CHILD_COUNT 1\n',
                           base + '#define GU_LOCAL_CONSOLE_AGGREGATE 2\n#define GU_LOCAL_CHILD_COUNT 1\n',
                           base + '#define GU_LOCAL_CONSOLE_AGGREGATE 1\n#define GU_LOCAL_CHILD_COUNT 3\n',
                           local.read_text().replace('GU_NO_EDID_DRIVER 0', 'GU_NO_EDID_DRIVER 1')]
                for index, header in enumerate(invalid):
                    if index == 4 and not aggregate:
                        continue  # No-EDID remains supported on non-aggregate firmware.
                    local.write_text(header)
                    done = compile_source('#include "machine-profile.h"\n', flags, extra)
                    self.assertNotEqual(done.returncode, 0, header)
                local.write_text(base + f'#define GU_LOCAL_CONSOLE_AGGREGATE {aggregate}\n'
                                 f'#define GU_LOCAL_CHILD_COUNT {children}\n')
                for override in ('GU_CONSOLE_AGGREGATE=1', 'GU_CONSOLE_MULTI_CHILD=1', 'GU_CONSOLE_CHILD_COUNT=2'):
                    done = compile_source('#include "machine-profile.h"\n', flags + ['-D' + override], extra)
                    self.assertNotEqual(done.returncode, 0, override)

    def test_unknown_firmware_and_local_template_inheritance_are_refused(self):
        for defines in (['-DGU_FIRMWARE_PROFILE=99'],
                        ['-DGU_PROFILE_LOCAL', '-DGU_FIRMWARE_PROFILE=2'],
                        ['-DGU_PROFILE_PROBE_H610M_3060', '-DGU_FIRMWARE_PROFILE=4']):
            with self.subTest(defines=defines):
                self.assertNotEqual(compile_source('#include "firmware-profile.h"\n', defines).returncode, 0)

    def test_three_four_children_are_only_nonaggregate_no_edid(self):
        for children,no_edid,aggregate in itertools.product((0,3,4,5),(0,1),(0,1)):
            with self.subTest(children=children,no_edid=no_edid,aggregate=aggregate),tempfile.TemporaryDirectory() as directory:
                folder=Path(directory);local=folder/'profile-local.h'
                local.write_text('#define GU_LOCAL_PROFILE_GENERATOR 1\n#define GU_CHIP_ID 0xb72000a1U\n'
                    '#define GU_BOARD_ID 0x220410deU\n#define GU_DEVICE_MEMORY_SIZE 0x6000U\n'
                    f'#define GU_NO_EDID_DRIVER {no_edid}\n#define GU_LOCAL_CONSOLE_AGGREGATE {aggregate}\n#define GU_LOCAL_CHILD_COUNT {children}\n')
                overlay=folder/'overlay.json'
                overlay.write_text(json.dumps({'version':0,'roots':[{'type':'file','name':str(ROOT/'profile-local.h'),
                    'external-contents':str(local),'use-external-name':False}]}))
                done=compile_source('#include "machine-profile.h"\n',['-DGU_PROFILE_LOCAL','-DGU_HEADLESS_RESEARCH','-DGU_FULL_UNLOCK'],['-ivfsoverlay',str(overlay)])
                self.assertEqual(done.returncode==0,children in (3,4) and no_edid==1 and aggregate==0,done.stderr)

    def test_local_segment_bounds_and_legacy_default_cannot_be_overridden(self):
        base=('#define GU_LOCAL_PROFILE_GENERATOR 1\n#define GU_CHIP_ID 0xb72000a1U\n'
              '#define GU_BOARD_ID 0x220410deU\n#define GU_DEVICE_MEMORY_SIZE 0x6000U\n'
              '#define GU_NO_EDID_DRIVER 0\n')
        for segment in (None,0,1,0xffff,-1,0x10000):
            with self.subTest(segment=segment),tempfile.TemporaryDirectory() as directory:
                folder=Path(directory);local=folder/'profile-local.h'
                local.write_text(base+('' if segment is None else f'#define GU_TARGET_SEGMENT {segment}\n'))
                overlay=folder/'overlay.json'
                overlay.write_text(json.dumps({'version':0,'roots':[{'type':'file','name':str(ROOT/'profile-local.h'),
                    'external-contents':str(local),'use-external-name':False}]}))
                flags=['-DGU_PROFILE_LOCAL','-DGU_HEADLESS_RESEARCH','-DGU_FULL_UNLOCK']
                source='#include "machine-profile.h"\n'
                if segment is None or 0<=segment<=0xffff:
                    source+=f'_Static_assert(GU_TARGET_SEGMENT == {segment or 0}, "captured segment");\n'
                done=compile_source(source,flags,['-ivfsoverlay',str(overlay)])
                if segment is None or 0<=segment<=0xffff:self.assertEqual(done.returncode,0,done.stderr)
                else:self.assertNotEqual(done.returncode,0)
        for profile in ('GU_PROFILE_B650M_3090FE','GU_PROFILE_LOCAL'):
            for segment in (0,1,0xffff):
                with self.subTest(profile=profile,override=segment):
                    done=compile_source('#include "machine-profile.h"\n',['-D'+profile,f'-DGU_TARGET_SEGMENT={segment}'])
                    self.assertNotEqual(done.returncode,0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--clang', default='clang')
    args = parser.parse_args()
    CLANG = args.clang
    unittest.main(argv=[__file__])
