"""Linux host verification for the experimental Z690-I/3090 Ti USB profile.

Uses production C code, captured graph/PROM, and verified firmware assets in RAM.
No firmware interfaces, GPU, or USB device are accessed by these tests.
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from z690i_fixture import prepare


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--clang', default='clang')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    unlock = repo / 'internal-boot/unlock'
    boot = repo / 'internal-boot/boot'
    out = repo / 'research/profile/build/z690i'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'test-results.json').unlink(missing_ok=True)
    capture = args.capture.resolve()
    assets = args.assets.resolve()
    prom = capture.parent / 'PROM.BIN'
    if hashlib.sha256(prom.read_bytes()).hexdigest() != '586baad14e09c28019499c7270aa608b878f8a87c457de13490fa7a855cfb30a':
        raise ValueError('Wrong card PROM')
    for asset in json.loads((unlock / 'assets.json').read_text()):
        data = (assets / asset['Name']).read_bytes()
        if len(data) != asset['Bytes'] or hashlib.sha256(data).hexdigest() != asset['Sha256']:
            raise ValueError('Firmware asset mismatch: ' + asset['Name'])
    capture_hash = prepare(capture, out)
    (out / 'platform-config.h').write_text('#define GU_INTERNAL_PARTITION_INITIALIZER {0,0,0,{0}}\n#define GU_WINDOWS_PARTITION_INITIALIZER {0,0,0,{0}}\n')
    flags = ['-std=c11', '-O1', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
             '-Wno-unused-function', '-fshort-wchar', '-ffunction-sections', '-fdata-sections',
             '-D_CRT_SECURE_NO_WARNINGS', '-D_aligned_malloc(n,a)=aligned_alloc((a),(n))',
             '-D_aligned_free=free', '-I' + str(out), '-I' + str(unlock / 'compat'),
             '-I' + str(repo / 'deps/edk2/MdePkg/Include'), '-I' + str(repo / 'deps/edk2/MdePkg/Include/X64')]
    profile = ['-DGU_PROFILE_Z690I_3090TI']
    core = ['port.c', 'compat.c', 'vendor/unlock_610.c', 'vendor/prep_610.c',
            'vendor/layout_610.c', 'vendor/vbios_610.c', 'vendor/gfw_eval.c']
    defs = ['-D__KERNEL__', '-DGU_UEFI_PORT', '-DGU_TARGET_PCI_ID=0x220310deU']
    results = {}

    def job(name, cwd, sources, defines=(), arguments=(), expected=0):
        exe = out / name
        command = [args.clang, *flags, *defines, *sources, '-Wl,--gc-sections', '-o', str(exe)]
        subprocess.run(command, cwd=cwd, check=True)
        run = subprocess.run([str(exe), *map(str, arguments)], cwd=cwd, text=True, capture_output=True)
        (out / (name + '.log')).write_text(run.stdout + run.stderr)
        results[name] = {'exit': run.returncode, 'expected': expected, 'command': command}
        print(f'{name}: exit={run.returncode} expected={expected}', flush=True)
        if run.returncode != expected:
            print(run.stdout[-4000:] + run.stderr[-4000:])
            raise RuntimeError('Host check failed: ' + name)
        return run

    located = job('fwsec-locate', unlock, [str(repo / 'research/profile/fwsec-locate.c'), 'vendor/vbios_610.c'], defs, [prom])
    print('FWSEC:', located.stdout.strip(), flush=True)
    job('checks', unlock, ['tests/test_checks.c', 'checks.c'], profile, [out / 'probe.txt'])
    job('checks-other-card', unlock, ['tests/test_checks.c', 'checks.c'], ['-DGU_PROFILE_B660M_3070TI'], [out / 'probe.txt'], 1)
    headless = ['-DGU_HEADLESS_RESEARCH', '-DGU_SINGLE_RTX_CONSOLE', '-DGU_FULL_UNLOCK', '-DGU_AUTOMATIC_BOOT']
    job('aggregate-capture', boot, ['test_aggregate_capture.c', '../unlock/checks.c', '../unlock/sha256.c'], profile + headless, [out])
    job('cold-precheck', boot, ['test_cold_precheck.c', 'console-flow.c', '../unlock/checks.c', '../unlock/sha256.c'], profile + headless)
    job('full-flow', boot, ['test_full_flow.c', 'full-flow.c'])
    job('port', unlock, ['tests/test_port.c', 'port.c'], profile + ['-DGU_HOST_WRITE_TEST'])
    job('write-lock', unlock, ['tests/test_preflight_lock.c', 'port.c'], profile)
    job('master-attributes', unlock, ['tests/test_master_attributes.c', *core], profile + defs + ['-DGU_HOST_WRITE_TEST'])
    latched = profile + defs + ['-DGU_EXPERIMENTAL_3090', '-DGU_ENTRY_LATCHED_SS1_FULL', '-DGU_INTERNAL_BOOT']
    job('transaction', unlock, ['tests/test_efi_transaction.c', *core], latched, [prom, assets, out / 'cold.txt'])
    app = ['app_io.c', 'ownership.c', 'format.c', 'sha256.c', 'checks.c', *core]
    job('headless-usb', unlock, ['tests/test_headless.c', *app], latched + ['-DGU_AUTOMATIC_BOOT', '-DGU_HEADLESS_RESEARCH', '-DGU_USB_MEDIA'], [prom, out / 'cold.txt', assets])
    job('headless-internal', unlock, ['tests/test_headless.c', *app], latched + ['-DGU_AUTOMATIC_BOOT', '-DGU_HEADLESS_RESEARCH'], [prom, out / 'cold.txt', assets])
    report = {'capture_sha256': capture_hash, 'hardware_tested': False, 'jobs': results}
    (out / 'test-results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS {len(results)} host jobs; no GPU access')


if __name__ == '__main__':
    main()
