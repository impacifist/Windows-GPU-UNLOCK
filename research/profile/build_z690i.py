"""Test and cross-build the experimental Z690-I package on Linux.

Mirrors build-usb.ps1's child, wrapper and manifest contracts. Writes build
outputs only; it does not install files, change boot entries, or access a GPU.
Verify --windows-esp against the target's current system partition before use.
"""
import argparse
import hashlib
import json
import subprocess
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def entries(paths, base):
    return [{'Path': p.relative_to(base).as_posix(), 'Sha256': sha(p)}
            for p in sorted(set(paths), key=lambda p: p.as_posix().casefold())]


def input_files(repo):
    runtime = repo / 'internal-boot'
    paths = [p for p in runtime.rglob('*') if p.is_file() and p.suffix in ('.c', '.h', '.ps1')
             and 'build' not in p.relative_to(runtime).parts]
    paths += list((repo / 'deps/edk2/MdePkg/Include').rglob('*.h'))
    return paths + [runtime / 'local-config.json', runtime / 'unlock/assets.json',
                    repo / 'research/headless/build-unlock.ps1', Path(__file__).resolve()]


def guid_c(value):
    g = uuid.UUID(value)
    if not g.int:
        raise ValueError('Windows ESP must have a nonzero GPT GUID')
    return f'{{0x{g.time_low:x},0x{g.time_mid:x},0x{g.time_hi_version:x},{{' + ','.join(f'0x{x:x}' for x in g.bytes[8:]) + '}}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--windows-esp', required=True)
    parser.add_argument('--internal-config', type=Path,
                        help='Configure.ps1 local-config.json for a dedicated internal ESP')
    parser.add_argument('--clang', default='clang')
    parser.add_argument('--lld-link', default='lld-link')
    args = parser.parse_args()
    windows_guid = guid_c(args.windows_esp)
    repo = Path(__file__).resolve().parents[2]
    boot = repo / 'internal-boot/boot'
    unlock = repo / 'internal-boot/unlock'
    out = repo / 'research/profile/build/z690i'
    internal = args.internal_config is not None
    manifest_name = 'manifest.json' if internal else 'usb-manifest.json'
    (boot / 'build/headless' / manifest_name).unlink(missing_ok=True)
    # Use the same generated configuration contract as the PowerShell builder.
    config = boot / 'platform-config.h'
    marker = '/* Local USB configuration (build-usb.ps1). Internal partition unused under GU_USB_MEDIA. */'
    if internal:
        local = json.loads(args.internal_config.read_text(encoding='utf-8-sig'))
        if args.internal_config.resolve() != (repo / 'internal-boot/local-config.json').resolve():
            raise ValueError('Use Configure.ps1 configuration in this source tree')
        if (uuid.UUID(local['UnlockPartitionGuid']) == uuid.UUID(args.windows_esp)
                or uuid.UUID(local['WindowsPartitionGuid']) != uuid.UUID(args.windows_esp)
                or not uuid.UUID(local['UnlockPartitionGuid']).int or local['UnlockSize'] < 1024**3
                or sha(config).lower() != local['GeneratedHeaderSha256'].lower()):
            raise ValueError('Dedicated ESP configuration identity/hash mismatch')
        before_inputs = entries(input_files(repo), repo)
    else:
        if config.exists() and config.read_text().splitlines()[0] != marker:
            raise ValueError('platform-config.h belongs to another setup; move it aside first')
        config.write_text(marker + '\n#define GU_INTERNAL_PARTITION_INITIALIZER {0,0,0,{0}}\n'
                          '#define GU_WINDOWS_PARTITION_INITIALIZER ' + windows_guid + '\n')
    subprocess.run([sys.executable, str(Path(__file__).with_name('test_z690i.py')),
                    '--capture', str(args.capture.resolve()), '--assets', str(args.assets.resolve()),
                    '--clang', args.clang], check=True)
    common = ['-target', 'x86_64-pc-windows-msvc', '-std=c11', '-ffreestanding',
              '-fno-builtin', '-fno-stack-protector', '-mno-red-zone', '-fshort-wchar',
              '-Os', '-Wall', '-Wextra', '-Werror', '-DGU_PROFILE_Z690I_3090TI',
              '-I' + str(repo / 'deps/edk2/MdePkg/Include'),
              '-I' + str(repo / 'deps/edk2/MdePkg/Include/X64')]
    auto = ['-DGU_HEADLESS_RESEARCH', '-DGU_AUTOMATIC_BOOT'] + ([] if internal else ['-DGU_USB_MEDIA'])
    commands = []

    def run(command, cwd):
        commands.append([str(x) for x in command])
        subprocess.run(command, cwd=cwd, check=True)

    def build(cwd, destination, name, sources, defines):
        destination.mkdir(parents=True, exist_ok=True)
        objects = []
        for source in sources:
            obj = destination / (Path(source).stem + '.obj')
            run([args.clang, *common, *defines, '-c', source, '-o', str(obj)], cwd)
            objects.append(str(obj))
        efi = destination / (name + '.EFI')
        mapfile = destination / (name + '.map')
        run([args.lld_link, '/nologo', '/nodefaultlib', '/subsystem:efi_application',
             '/entry:efi_main', '/machine:x64', '/base:0', '/dynamicbase', '/nxcompat',
             '/timestamp:0', '/opt:ref', '/opt:icf', '/map:' + str(mapfile),
             '/out:' + str(efi), *objects], cwd)
        run([sys.executable, str(unlock / 'verify_pe.py'), str(efi), str(mapfile),
             '--output', str(destination / 'pe-verification.json')], cwd)
        return efi

    child_dir = unlock / 'build/experimental/entryLatchedSs1Full/internal/automatic/headless-research'
    if not internal:
        child_dir /= 'usb'
    child_sources = ['app.c', 'app_io.c', 'ownership.c', 'format.c', 'runtime.c', 'sha256.c',
                     'checks.c', 'port.c', 'compat.c', 'vendor/unlock_610.c', 'vendor/prep_610.c',
                     'vendor/layout_610.c', 'vendor/vbios_610.c', 'vendor/gfw_eval.c']
    child = build(unlock, child_dir,
                  'RTX3090-EXPERIMENTAL-entryLatchedSs1Full-INTERNAL-AUTO-HEADLESS-RESEARCH' + ('' if internal else '-USB'),
                  child_sources,
                  auto + ['-Wno-unused-parameter', '-D__KERNEL__', '-DGU_UEFI_PORT',
                          '-DGU_TARGET_PCI_ID=0x220310deU', '-DGU_EXPERIMENTAL_3090',
                          '-DGU_ENTRY_LATCHED_SS1_FULL', '-DGU_INTERNAL_BOOT',
                          '-I' + str(unlock / 'compat')])
    wrapper_dir = boot / 'build/headless'
    wrapper_dir.mkdir(parents=True, exist_ok=True)
    (wrapper_dir / 'child-contract.h').write_text(
        f'#define GU_UNLOCK_SHA256 "{sha(child)}"\n#define GU_UNLOCK_BYTES {child.stat().st_size}\n')
    wrapper = build(boot, wrapper_dir, 'HEADLESS',
                    ['handoff.c', 'full-release.c', 'full-flow.c', 'diagnostic.c',
                     '../unlock/checks.c', '../unlock/sha256.c', '../efi_runtime.c'],
                    auto + ['-DGU_SINGLE_RTX_CONSOLE', '-DGU_FULL_UNLOCK'])
    manifest = {'Format': 2, 'HardwareProfile': 'ASUS-ROG-STRIX-Z690-I-4601-RTX3090TI',
                'Mode': 'single-rtx-headless-unlock-then-windows' + ('' if internal else '-usb'), 'CanUnlock': True,
                'RequiresHumanKey': False, 'HardwareTested': False,
                'ChildSha256': sha(child), 'ChildBytes': child.stat().st_size,
                'EfiSha256': sha(wrapper), 'EfiBytes': wrapper.stat().st_size,
                'BuiltUtc': datetime.now(timezone.utc).isoformat(),
                'WindowsEspGuid': str(uuid.UUID(args.windows_esp)),
                'HostTestsSha256': sha(out / 'test-results.json')}
    if internal:
        if entries(input_files(repo), repo) != before_inputs:
            raise ValueError('Inputs changed during build; no package manifest published')
        headers = [p for p in unlock.rglob('*.h') if 'build' not in p.relative_to(unlock).parts]
        child_manifest = {'Mode': 'headless-research-automatic-internal-exact-card-unlock',
                          'HardwareProfile': manifest['HardwareProfile'], 'StockProfile': 'entryLatchedSs1Full',
                          'CanArm': True, 'EfiSha256': sha(child), 'HardwareTested': False,
                          'Sources': entries([unlock / s for s in child_sources], unlock),
                          'Headers': entries(headers, unlock)}
        child_manifest_path = child_dir / 'build-manifest.json'
        child_manifest_path.write_text(json.dumps(child_manifest, indent=2) + '\n')
        inputs = input_files(repo) + [wrapper_dir / 'child-contract.h']
        manifest.update(ChildManifestSha256=sha(child_manifest_path), Inputs=entries(inputs, repo))
    (wrapper_dir / manifest_name).write_text(json.dumps(manifest, indent=2) + '\n')
    (out / 'build-commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
