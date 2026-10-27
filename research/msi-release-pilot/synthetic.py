# SPDX-License-Identifier: GPL-2.0-only
"""Deterministic test data, constructed without reading captures or firmware.

The model encodes a minimal PCI capability list and five required ownership
edges plus unrelated background ownership. It is not a captured machine.
Generated hashes exercise the real SHA-256 checks; no hash result is mocked.
"""
import hashlib
from pathlib import Path


def pattern(size, seed):
    return bytes((i * 37 + (i >> 8) * 17 + seed * 53) & 255 for i in range(size))


def generate(root: Path, out: Path):
    sizes = [('PROM', 1048576, 1), ('NVGOP_TEXT', 0x24426, 2),
             ('CONSOLE_GFX', 0x1c70, 3), ('CONSOLE_TEXT', 0x4e9a, 4)]
    header = ['/* Generated synthetic identities; host tests only. */',
              '#if !defined(GU_PILOT_SYNTHETIC) || !defined(GU_PILOT_HOST_TEST) || __STDC_HOSTED__ != 1',
              '#error Synthetic fixture cannot enter firmware', '#endif']
    for name, size, seed in sizes:
        macro = 'GU_' + name + '_SHA256'
        header += [f'#undef {macro}', f'#define {macro} "{hashlib.sha256(pattern(size, seed)).hexdigest()}"']
    (out / 'synthetic-profile.h').write_text('\n'.join(header) + '\n')

    # Synthetic capability chain: PM v1 at 0x40, PCIe v2 endpoint at 0x48.
    cfg = [0] * 64
    for index, value in {0: 0x220810de, 1: 0x00100002, 2: 0x030000a1,
                         4: 0xfa000000, 11: 0x88031043, 13: 0x40,
                         16: 0x00014801, 18: 0x00020010, 19: 1 << 28}.items():
        cfg[index] = value
    roles = {'RtxPci': 1, 'Rtx': 2, 'NvAgent': 3, 'NvImage': 3,
             'OutAgent': 4, 'OutImage': 10, 'GfxAgent': 5, 'GfxImage': 11,
             'Console': 6, 'Input': 6, 'Error': 6, 'Amd': 0, 'AmdPci': 0}
    rows = [
        '{FX_RtxPci,FX_NvAgent,FX_RtxPci,EFI_PCI_IO_PROTOCOL_GUID,16,1}',
        '{FX_RtxPci,FX_NvAgent,FX_Rtx,EFI_DEVICE_PATH_PROTOCOL_GUID,8,1}',
        '{FX_Rtx,FX_GfxAgent,FX_Rtx,EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,16,1}',
        '{FX_Rtx,FX_OutAgent,FX_Rtx,EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID,16,1}',
        '{FX_Rtx,FX_OutAgent,FX_Console,EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID,8,1}',
    ]
    background = '{(EFI_HANDLE)7,(EFI_HANDLE)8,(EFI_HANDLE)9,{0x12345678,0,0,{0}},16,1}'
    values = ','.join(hex(x) for x in cfg)
    lines = ['/* Synthetic, minimal role model; no capture parser or raw addresses. */',
             '#define FX_HANDLES 11']
    lines += [f'#define FX_{role} ((EFI_HANDLE){value})' for role, value in roles.items()]
    lines += ['static const EDGE FixtureEdges[]={' + ','.join(rows + [background]) + '};',
              'static const UINT32 FixtureFullConfig[64]={' + values + '};',
              'static const UINT32 FixtureRtxConfig[16]={' + ','.join(hex(x) for x in cfg[:16]) + '};',
              'static const UINT32 FixtureAmdConfig[16]={0};',
              'static const UINT32 FixtureCold[9]={0xb72000a1,0x8b8f,0x3ff,0x10,0,0xffffff8f,0x11112222,7,0x3000};',
              'static const UINT64 FixtureMode[7]={0xfcc0000000,8294400,0,1,1920,1080,1920};']
    (out / 'replay-fixture.h').write_text('\n'.join(lines) + '\n')
    (out / 'stage0-replay.h').write_text(
        '/* Synthetic independent PCI/attribute mismatch, not a Stage 0 capture. */\n'
        'static const UINT32 Stage0Config[64]={' + values + '};\n'
        'static const UINT64 Stage0Attributes=0x300,Stage0ExpectedAttributes=0x200;\n'
        'static const UINTN Stage0GraphRecords=6;\n'
        'static const EDGE Stage0RelevantEdges[]={' + ','.join(rows) + '};\n')
    sources = sorted((root / 'research/msi-release-pilot').glob('*'))
    sources += [root / name for name in ['internal-boot/boot/prepare-console.c',
                'internal-boot/boot/prepare-console.h', 'internal-boot/boot/media.h',
                'internal-boot/unlock/checks.c', 'internal-boot/unlock/checks.h',
                'internal-boot/unlock/port.h', 'internal-boot/unlock/sha256.c',
                'internal-boot/unlock/sha256.h']]
    identity = ''.join(str(path.relative_to(root)) + ':' + hashlib.sha256(path.read_bytes()).hexdigest() + '\n'
                       for path in sources if path.is_file())
    (out / 'stage1-build.h').write_text('#define GU_STAGE1_SOURCE_ID "' + hashlib.sha256(identity.encode()).hexdigest() + '"\n')
