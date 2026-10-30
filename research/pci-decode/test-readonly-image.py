"""Link only the read collector and pure predicates into a synthetic EFI; never execute/stage."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang',default='clang')
    parser.add_argument('--lld-link',default='lld-link')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--stack-mutation',action='store_true')
    args=parser.parse_args();out=args.output.resolve()
    if out.exists():parser.error('Output must be a new directory')
    out.mkdir(parents=True)
    entry=out/'entry.c'
    entry.write_text('/* Link test only. No valid target or boot configuration. Never execute. */\n'
        '#include "efi-capture.h"\nstatic GU_DS_CAPTURE Capture;\nstatic GU_DS_CAPTURE_TARGET Target;\n'
        'static EFI_STATUS (*volatile Collect)(GU_DS_CAPTURE *,EFI_BOOT_SERVICES *,const GU_DS_CAPTURE_TARGET *)=GuDsEfiCapture;\n'
        'EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *table) {\n'
        ' Target.Handle=image;return Collect(&Capture,table->BootServices,&Target);\n}\n')
    flags=[args.clang,'-target','x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fno-builtin',
           '-fno-stack-protector','-mno-red-zone','-fshort-wchar','-Os','-Wall','-Wextra','-Werror','-fstack-usage',
           '-I',str(HERE),'-I',str(ROOT/'deps/edk2/MdePkg/Include'),'-I',str(ROOT/'deps/edk2/MdePkg/Include/X64')]
    sources=[HERE/'efi-capture.c',HERE/'snapshot.c',ROOT/'internal-boot/efi_runtime.c',entry]
    def run(log,command):
        result=subprocess.run([str(v) for v in command],capture_output=True,text=True,cwd=ROOT,timeout=30)
        log.write_text(result.stdout+result.stderr)
        return result
    def build(folder,inputs):
        objects=[]
        for i,source in enumerate(inputs):
            obj=folder/f'{i:02d}-{source.stem}.obj';objects.append(obj)
            r=run(folder/(obj.stem+'.log'),[*flags,'-c',source,'-o',obj])
            if r.returncode:raise RuntimeError('Readonly object compile failed: '+str(source))
            if obj.read_bytes()[:2]!=b'\x64\x86':raise RuntimeError('Unexpected COFF machine')
        binary=folder/'SYNTHETIC-READONLY-NOT-FOR-EXECUTION.efi';symbols=folder/'symbols.map'
        linked=run(folder/'link.log',[args.lld_link,'/nologo','/nodefaultlib','/subsystem:efi_application',
            '/entry:efi_main','/machine:x64','/base:0','/dynamicbase','/nxcompat','/timestamp:0',
            '/opt:ref','/opt:icf','/map:'+str(symbols),'/out:'+str(binary),*objects])
        frames=[]
        for usage in sorted(folder.glob('*.su')):
            for line in usage.read_text().splitlines():
                fields=line.split('\t')
                if len(fields)!=3 or not fields[1].isdigit() or fields[2]!='static':
                    raise RuntimeError('Unknown/dynamic stack usage record')
                frames.append({'function':fields[0],'bytes':int(fields[1])})
        if not frames:raise RuntimeError('Missing compiler stack usage data')
        return linked,binary,symbols,frames
    linked,binary,symbols,frames=build(out,sources)
    if linked.returncode:raise RuntimeError('Readonly EFI link failed; inspect link.log')
    if max(row['bytes'] for row in frames)>2048:raise RuntimeError('Readonly frame exceeds 2 KiB limit')
    forbidden=r'\b(?:GuDecodeSync|GuDsEfiRun|GuDsEfiInitialize|gpu_unlock_[A-Za-z_0-9]+)\b'
    if re.search(forbidden,symbols.read_text()):raise RuntimeError('Actuator symbol reached readonly image')
    spec=importlib.util.spec_from_file_location('readonly_verify_pe',ROOT/'internal-boot/unlock/verify_pe.py')
    verifier=importlib.util.module_from_spec(spec);spec.loader.exec_module(verifier)
    pe=verifier.inspect(binary,symbols)
    (out/'pe-verification.json').write_text(json.dumps(pe,indent=2))
    mutant=None
    if args.stack_mutation:
        folder=out/'automatic-workspace-mutant';folder.mkdir()
        broken=(HERE/'efi-capture.c').read_text()
        replacements=[('static WORK ActiveWork;\n',''),('static GU_DS_SNAPSHOT ActiveSnapshots[2];\n',''),
            ('WORK *w=&ActiveWork;GU_DS_SNAPSHOT *snapshots=ActiveSnapshots;',
             'WORK LocalWork;GU_DS_SNAPSHOT LocalSnapshots[2];WORK *w=&LocalWork;GU_DS_SNAPSHOT *snapshots=LocalSnapshots;')]
        for before,after in replacements:
            if before not in broken:raise RuntimeError('Stack mutation no longer matches')
            broken=broken.replace(before,after)
        source=folder/'efi-capture.c';source.write_text(broken)
        failed,_,_,bad_frames=build(folder,[source,*sources[1:]])
        if not failed.returncode or '__chkstk' not in failed.stderr+failed.stdout or max(row['bytes'] for row in bad_frames)<=4096:
            raise RuntimeError('Automatic-workspace mutant did not reproduce the stack-probe link failure')
        mutant={'compiled':True,'link_rejected':True,'max_frame_bytes':max(row['bytes'] for row in bad_frames)}
    inputs=[*sources,HERE/'snapshot.h',HERE/'efi-capture.h',HERE/'efi-binding.h',HERE/'efi-path.h',
            ROOT/'internal-boot/unlock/gpu-path.h']
    result={'synthetic_entry_only':True,'hardware_access':False,'efi_executed':False,'package_authorized':False,
            'actuator_sources_linked':False,'efi_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
            'frames':frames,'max_frame_bytes':max(row['bytes'] for row in frames),'stack_mutation':mutant,
            'source_sha256':{str(p.relative_to(ROOT) if p.is_relative_to(ROOT) else p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}}
    (out/'summary.json').write_text(json.dumps(result,indent=2))
    print('PASS readonly EFI link/PE and no-actuator symbol checks; max frame '+str(result['max_frame_bytes'])+' bytes; never executed')
    if mutant:print('PASS compiled automatic-workspace mutant rejected by __chkstk link failure')


if __name__=='__main__':main()
