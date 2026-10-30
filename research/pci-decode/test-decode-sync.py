"""Compile/run a RAM-only experimental cache-sync model; never firmware or GPU code."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang', default='clang')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mutations', action='store_true', help='Require selected broken guards to fail the host model')
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        parser.error('Output must be a new directory')
    output.mkdir(parents=True)
    executable = output / 'decode-sync-host.exe'
    flags = [args.clang, '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror', '-D_CRT_SECURE_NO_WARNINGS',
             '-I', str(ROOT / 'deps/edk2/MdePkg/Include'), '-I', str(ROOT / 'deps/edk2/MdePkg/Include/X64'), '-I', str(HERE)]
    commands = [
        [*flags, str(HERE / 'test_decode_sync.c'), str(HERE / 'decode-sync.c'), str(HERE / 'snapshot.c'), '-o', str(executable)],
        [str(executable)],
        [*flags, str(HERE / 'test_efi_adapter.c'), str(HERE / 'efi-adapter.c'), str(HERE / 'decode-sync.c'), str(HERE / 'snapshot.c'),
         '-o', str(output / 'efi-adapter-host.exe')],
        [str(output / 'efi-adapter-host.exe')],
        [*flags, str(HERE / 'test_efi_capture.c'), str(HERE / 'efi-capture.c'), str(HERE / 'efi-adapter.c'),
         str(HERE / 'decode-sync.c'), str(HERE / 'snapshot.c'), '-o', str(output / 'efi-capture-host.exe')],
        [str(output / 'efi-capture-host.exe')],
    ]
    results = []
    for name, command in zip(('compile', 'run', 'adapter-compile', 'adapter-run','capture-compile','capture-run'), commands):
        result = subprocess.run(command, capture_output=True, text=True, cwd=ROOT, timeout=30)
        (output / f'{name}.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f'{name}: ' + (result.stdout + result.stderr)[-2500:])
        results.append({'stage': name, 'exit': result.returncode})
        if name.endswith('run'):
            print(result.stdout.strip())
    mutations = []
    if args.mutations:
        changes = {
            'warning-as-success': [('status!=EFI_SUCCESS', 'EFI_ERROR(status)')],
            'ignore-parent-bus-cycles': [('p->Bus==g->Bus || ', ''), ('p->Bus==s->Parents[j].Bus || ', '')],
            'ignore-late-parent-drift': [('for(UINTN i=0;i<b->ParentCount;i++)if(!DeviceEqual(&a->Parents[i],&b->Parents[i],FALSE))return FALSE;', '')],
            'compare-only-first-config-dword': [('for(UINTN i=0;i<64;i++)if(a->Config[i]!=b->Config[i])return FALSE;',
                                                'if(a->Config[0]!=b->Config[0])return FALSE;')],
            'do-not-record-write-attempt': [('t->Attempted=TRUE;', 't->Attempted=FALSE;')],
            'ignore-final-log-failure': [('if(o.Log(o.Context,"decode-verify-full-snapshot")!=TRUE)return Finish(t,GU_DS_LOG_FAILED);',
                                         '(void)o.Log(o.Context,"decode-verify-full-snapshot");')],
        }
        readonly_names={'ignore-parent-bus-cycles','ignore-late-parent-drift','compare-only-first-config-dword'}
        mutation_specs = [(name, 'snapshot.c' if name in readonly_names else 'decode-sync.c', 'test_decode_sync.c',
                           [str(HERE / ('decode-sync.c' if name in readonly_names else 'snapshot.c'))], replacements)
                          for name, replacements in changes.items()]
        adapter_changes = {
            'overwrite-real-not-started-error': [('if(!c->OwnershipObserved || c->OwnershipStatus==EFI_SUCCESS)',
                'if(c->OwnershipStatus==EFI_NOT_STARTED || c->OwnershipStatus==EFI_SUCCESS)')],
            'accept-missing-self-claim': [('owned!=1', 'owned>1')],
            'ignore-exclusive-agent': [('e->AgentHandle!=c->OwnerImage || ', '')],
            'accept-managing-co-owner': [('} else if(e->Attributes!=EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL && e->Attributes!=EFI_OPEN_PROTOCOL_GET_PROTOCOL &&\n'
                '              e->Attributes!=EFI_OPEN_PROTOCOL_TEST_PROTOCOL)', '} else if(FALSE)')],
            'free-final-owner-view-before-write': [('GU_DS_EFI_DEVICE *g=&c->Devices[0];\n    c->DisableInvoked=TRUE;',
                'DropOwner(c,held);held=NULL;\n    GU_DS_EFI_DEVICE *g=&c->Devices[0];\n    c->DisableInvoked=TRUE;')],
            'ignore-owner-cleanup-error': [('if(s!=EFI_SUCCESS){c->OwnershipFault=TRUE;return EFI_DEVICE_ERROR;}',
                'if(s!=EFI_SUCCESS){return EFI_SUCCESS;}')],
            'ignore-late-peer-capability-drift': [('!AllCaps(c) || ', ''), (' && AllCaps(c)', ''),
                ('static EFI_STATUS AdapterDisable(VOID *context) {', 'static EFI_STATUS AdapterDisable(VOID *context) {\n  (void)AllCaps;')],
            'accept-partial-locate-consumption': [(' || (UINTN)rest!=(UINTN)copy+d->Pin.PathBytes-4', '')],
            'same-sentinel-in-both-scans': [('pass?0x5a5a5a5aU:0xa5a5a5a5U', '0xa5a5a5a5U')],
            'allow-adapter-reinitialization': [('if(c->InitUsed!=FALSE)return EFI_ALREADY_STARTED;', '')],
        }
        mutation_specs += [(name, 'efi-adapter.c', 'test_efi_adapter.c', [str(HERE / 'decode-sync.c'),str(HERE / 'snapshot.c')], replacements)
                           for name, replacements in adapter_changes.items()]
        capture_changes = {
            'ignore-capture-gpu-pin': [('  if(!index && (out->Config[0]!=w->Target.PciId || out->Config[11]!=w->Target.Subsystem || out->Config[4]!=w->Target.Bar0))return EFI_NOT_READY;','')],
            'omit-capture-two-scan-match': [(' || !GuDecodeSnapshotMatches(&snapshots[1],&snapshots[0],FALSE)','')],
            'accept-capture-interface-replacement': [(' || (!first && io!=d->Pin.Io)','')],
            'check-only-last-capture-capability': [('for(UINTN i=0;i<w->Bound;i++)',
                'for(UINTN i=w->Bound?w->Bound-1:0;i<w->Bound;i++)')],
        }
        mutation_specs += [(name, 'efi-capture.c', 'test_efi_capture.c',
            [str(HERE / 'decode-sync.c'),str(HERE / 'snapshot.c'),str(HERE / 'efi-adapter.c')], replacements)
            for name, replacements in capture_changes.items()]
        for name, source_file, test_file, support, replacements in mutation_specs:
            broken = (HERE / source_file).read_text()
            for before, after in replacements:
                if before not in broken:
                    raise RuntimeError(f'Mutation no longer matches: {name}')
                broken = broken.replace(before, after)
            folder = output / name
            folder.mkdir()
            mutated = folder / source_file
            mutated.write_text(broken)
            binary = folder / 'broken-host.exe'
            compiled = subprocess.run([*flags, str(HERE / test_file), str(mutated), *support, '-o', str(binary)],
                                      capture_output=True, text=True, cwd=ROOT, timeout=30)
            (folder / 'compile.log').write_text(compiled.stdout + compiled.stderr)
            if compiled.returncode:
                raise RuntimeError(f'Mutant must compile before test detection: {name}')
            tested = subprocess.run([str(binary)], capture_output=True, text=True, cwd=ROOT, timeout=30)
            (folder / 'run.log').write_text(tested.stdout + tested.stderr)
            if not tested.returncode:
                raise RuntimeError(f'Host model failed to detect broken guard: {name}')
            mutations.append({'name': name, 'compiled': True, 'detected': True})
        print(f'PASS {len(mutations)} compiled guard mutations detected')
    (output / 'summary.json').write_text(json.dumps({
        'research_only': True, 'hardware_access': False, 'efi_executed': False,
        'correction_build_wired': False, 'readonly_collector_build_wired': True,
        'local_evidence_written': False, 'adapter_host_only': True,
        'results': results, 'mutations': mutations,
    }, indent=2))


if __name__ == '__main__':
    main()
