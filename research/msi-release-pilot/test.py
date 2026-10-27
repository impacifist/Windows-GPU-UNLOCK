#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build/run host-only Stage 1 simulations. No captures, EFI build or network.

Requires Python 3.9+ and Clang with ASan/UBSan on Linux (including WSL).
All outputs go to a new explicit directory; existing directories are refused.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from synthetic import generate
from controls import controls, guards

ROOT = Path(__file__).resolve().parents[2]
PILOT = Path(__file__).resolve().parent


def run(command, log, expected=0, contains=None):
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    log.write_text(result.stdout + result.stderr)
    if result.returncode != expected or (contains and contains not in result.stdout + result.stderr):
        raise RuntimeError(f'{log.name}: unexpected result {result.returncode}\n' + (result.stdout + result.stderr)[-2500:])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists():
        parser.error('output must be a new directory')
    if os.name != 'posix':
        parser.error('run on Linux or WSL; no firmware executable is supported')
    out.mkdir(parents=True)
    generate(ROOT, out)
    flags = ['clang', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
             '-fshort-wchar', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
             '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
             '-DGU_RELEASE_PILOT', '-DGU_PILOT_HOST_TEST', '-DGU_PILOT_STAGE=1',
             '-DGU_PILOT_SYNTHETIC', '-I', str(out),
             '-I', 'deps/edk2/MdePkg/Include', '-I', 'deps/edk2/MdePkg/Include/X64']
    (out / 'platform-config.h').write_text('#define GU_INTERNAL_PARTITION_INITIALIZER {0}\n#define GU_WINDOWS_PARTITION_INITIALIZER {0}\n')
    inputs = ['research/msi-release-pilot/test_pilot.c', 'internal-boot/unlock/checks.c',
              'internal-boot/unlock/sha256.c']
    binary = out / 'test-stage1'
    run(flags + inputs + ['-o', str(binary)], out / 'compile.log')
    result = run([str(binary)], out / 'suite.log', contains='PASS actual pilot entry/core stage 1:')
    print('\n'.join(line for line in result.stdout.splitlines() if line.startswith('PASS ')), flush=True)
    negative = controls(ROOT, PILOT, out, flags, inputs, run)
    rejected = guards(ROOT, PILOT, out, run)
    symbols = subprocess.check_output(['nm', str(binary)], text=True)
    for forbidden in ['GuPrepareConsole', 'GuReleaseGop', 'RestoreDecode', 'CapturePermit', 'GuUnlock']:
        if any(line.split()[-1] == forbidden for line in symbols.splitlines() if line.split()):
            raise RuntimeError('Unexpected linked capability: ' + forbidden)
    print('PASS host linked-capability isolation', flush=True)
    (out / 'results.json').write_text(json.dumps({'result': 'PASS_HOST_ONLY',
        'negative_controls': negative, 'build_rejections': rejected,
        'private_capture_inputs': [], 'hardware_tested': False, 'efi_executed': False,
        'compiler': subprocess.check_output(['clang', '--version'], text=True).splitlines()[0]}, indent=2) + '\n')


if __name__ == '__main__':
    main()
