"""Build/run console-only synthetic tests. Never build or boot firmware.

Requires Windows, Python 3.9+, a reviewed gpu-unlock-ada checkout and MSVC.
Writes only this folder's ignored build/ tree and prints a compact summary.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import struct

HERE = Path(__file__).resolve().parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, cwd, env, label, output):
    result = subprocess.run([str(x) for x in command], cwd=cwd, env=env,
                            capture_output=True, text=True, timeout=120)
    (output / (label + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(label + ' failed; see build/' + label + '.log')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('checkout', type=Path)
    parser.add_argument('--vcvars', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This runner requires Windows/MSVC')
    root, vcvars = args.checkout.resolve(), args.vcvars.resolve()
    provenance = json.loads((HERE / 'host-source-provenance.json').read_text())
    for name, expected in provenance['files'].items():
        path = (root / name).resolve()
        if not path.is_relative_to(root) or sha(path) != expected:
            parser.error('Reviewed input mismatch: ' + name)
    # cmd is used only to read the compiler environment from the explicit local
    # trusted MSVC setup file. Compiler and test invocations use argument arrays.
    if any(c in str(vcvars) for c in '"&|<>%^!\r\n') or not vcvars.is_file():
        parser.error('Invalid MSVC setup path')
    comspec = Path(os.environ.get('SystemRoot', r'C:\Windows')) / 'System32/cmd.exe'
    env_result = subprocess.run(
        '"' + str(comspec) + '" /d /s /c ""' + str(vcvars) + '" >nul && set"',
        capture_output=True, text=True, timeout=60, check=True)
    env = {key.upper(): value for key, value in os.environ.items()}
    for line in env_result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key, value = line.split('=', 1)
            env[key.upper()] = value
    for key in ('CL', '_CL_', 'LINK', '_LINK_'):
        env.pop(key, None)
    compiler = Path(env['VCTOOLSINSTALLDIR']) / 'bin/Hostx64/x64/cl.exe'
    port = root / 'windows/ad104-msi'
    diagnostic = port / 'diagnostic'
    build = HERE / 'build'
    build.mkdir(exist_ok=True)
    (build / 'host_build_id.h').write_text(
        '#define PROBE_BUILD_ID "host-only-synthetic-4e7fefcc"\n', encoding='ascii')
    (build / 'quiet_parser.h').write_text(
        '#define GPU_UNLOCK_DIAGNOSTIC_H\n'
        '#define GU_NOTE(...) ((void)0)\n#define GU_ERROR(...) ((void)0)\n'
        '#define GU_DIAG(result) (result)\n', encoding='ascii')
    include = ['/I' + str(port / 'vendor/edk2/Include'),
               '/I' + str(port / 'vendor/edk2/Include/X64')]
    common = [compiler, '/nologo', '/W4', '/WX', '/Od', '/std:c11', '/UNDEBUG',
              '/D_CRT_SECURE_NO_WARNINGS', '/TC']
    targets = [
        ('upstream-host', [diagnostic / 'tests/host_tests.c',
                           diagnostic / 'src/core.c', diagnostic / 'src/sample.c',
                           diagnostic / 'src/platform.c'],
         include + ['/FI' + str(build / 'host_build_id.h')]),
        ('upstream-platform', [diagnostic / 'tests/platform_tests.c',
                               diagnostic / 'src/core.c', diagnostic / 'src/platform.c'], include),
        ('upstream-vbios', [root / 'tests/test_vbios_610.c', port / 'vendor/vbios_610.c'],
         # The Linux fixture intentionally narrows byte writes, negates an
         # unsigned checksum and shadows an optional integration variable.
         ['/I' + str(port / 'vendor'), '/wd4244', '/wd4146', '/wd4456']),
        ('capture-cases', [HERE / 'host_capture_cases.c', diagnostic / 'src/core.c',
                           diagnostic / 'src/sample.c', diagnostic / 'src/platform.c'],
         include + ['/I' + str(diagnostic / 'tests'), '/FI' + str(build / 'host_build_id.h')]),
        ('platform-cases', [HERE / 'host_platform_cases.c', diagnostic / 'src/core.c',
                            diagnostic / 'src/platform.c'],
         include + ['/I' + str(diagnostic / 'tests')]),
        ('parser-cases', [HERE / 'host_parser_cases.c', port / 'vendor/vbios_610.c'],
         ['/I' + str(root / 'tests'), '/I' + str(port / 'vendor'),
          '/wd4244', '/wd4146', '/wd4456']),
        ('guarded-rom-cases', [HERE / 'host_guarded_rom_cases.c', port / 'vendor/vbios_610.c'],
         ['/I' + str(root / 'tests'), '/I' + str(port / 'vendor'),
          '/FI' + str(build / 'quiet_parser.h'), '/wd4244', '/wd4146', '/wd5105']),
    ]
    summaries = []
    for label, sources, flags in targets:
        objects = build / label
        objects.mkdir(exist_ok=True)
        exe = build / (label + '.exe')
        run(common + flags + ['/Fo' + str(objects) + os.sep,
                             '/Fe' + str(exe)] + sources,
            HERE, env, label + '-compile', build)
        # Refuse to execute anything other than an x64 Windows console PE.
        image = exe.read_bytes()
        pe = struct.unpack_from('<I', image, 0x3c)[0]
        if (image[:2] != b'MZ' or image[pe:pe+4] != b'PE\0\0' or
            struct.unpack_from('<H', image, pe+4)[0] != 0x8664 or
            struct.unpack_from('<H', image, pe+24+68)[0] != 3):
            raise RuntimeError(label + ' is not an x64 console executable')
        result = run([exe], HERE, env, label, build)
        summaries.append({'target': label, 'exit_code': result.returncode,
                          'pe_subsystem': 'Windows console (3)',
                          'summary': result.stdout.strip(), 'binary_sha256': sha(exe)})
    data = {'scope': 'Windows console executables; synthetic UEFI callbacks and ROMs only',
            'reviewed_source_commit': provenance['commit'],
            'source_inputs_checked': len(provenance['files']),
            'compiler_version': env.get('VCTOOLSVERSION'),
            'research_source_sha256': {p.name: sha(p) for p in
                [Path(__file__), *sorted(HERE.glob('host_*_cases.c'))]},
            'firmware_built': False, 'hardware_accessed': False,
            'targets': summaries}
    (build / 'host-results.json').write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(data, indent=2))


if __name__ == '__main__':
    main()
