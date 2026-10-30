"""Compile/run actual parent-enabled diagnostic in host RAM; no EFI execution/staging."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang',default='clang')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();out=args.output.resolve()
    if out.exists():parser.error('Output must be a new directory')
    out.mkdir(parents=True)
    sources=['internal-boot/boot/test_probe_parent.c','internal-boot/boot/probe.c','internal-boot/boot/probe-parent.c',
             'research/pci-decode/efi-capture.c','research/pci-decode/snapshot.c','internal-boot/unlock/sha256.c']
    flags=[args.clang,'-std=c11','-O1','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS',
           '-DGU_DIAGNOSTIC_ONLY','-DGU_DIAGNOSTIC_PROBE','-DGU_PROBE_GENERIC','-DGU_PROBE_AUTO_BUS',
           '-DGU_PROBE_PARENT_BASELINE','-I',str(ROOT/'deps/edk2/MdePkg/Include'),'-I',str(ROOT/'deps/edk2/MdePkg/Include/X64')]
    results=[]
    for bus in (1,43,255):
        for segment in (0,1,0xffff):
            name=f'bus{bus}-segment{segment}';exe=out/(name+'.exe')
            for stage,command in (('compile',[*flags,f'-DGU_TEST_PROBE_BUS={bus}',f'-DGU_PARENT_TEST_SEGMENT={segment}',
                                               *[str(ROOT/p) for p in sources],'-o',str(exe)]),('run',[str(exe)])):
                r=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=30)
                (out/(name+'-'+stage+'.log')).write_text(r.stdout+r.stderr)
                if r.returncode:raise RuntimeError(name+' '+stage+': '+(r.stdout+r.stderr)[-2000:])
            results.append({'bus':bus,'segment':segment,'result':r.stdout.strip()});print(name+': '+r.stdout.strip(),flush=True)
    legacy=[]
    common=[args.clang,'-std=c11','-O1','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS',
            '-I',str(ROOT/'deps/edk2/MdePkg/Include'),'-I',str(ROOT/'deps/edk2/MdePkg/Include/X64')]
    cases=[('diagnostic','test_diagnostic.c',[])]
    cases += [(p,'test_probe.c',['-DGU_DIAGNOSTIC_PROBE','-DGU_PROFILE_'+p]) for p in
              ('B650M_3090FE','B660M_3070TI','B660M_3090TI','X870_3090TI','B660PLUS_3060')]
    cases += [(p,'test_probe_generic.c',['-DGU_DIAGNOSTIC_ONLY','-DGU_DIAGNOSTIC_PROBE','-DGU_USB_MEDIA','-DGU_PROFILE_'+p])
              for p in ('H610M_3060','PROBE_H610M_3060')]
    cases += [('generic-old','test_probe_generic.c',['-DGU_DIAGNOSTIC_ONLY','-DGU_DIAGNOSTIC_PROBE','-DGU_PROBE_GENERIC','-DGU_PROBE_AUTO_BUS'])]
    for name,source,defines in cases:
        exe=out/(name+'.exe');inputs=[ROOT/'internal-boot/boot'/source]
        if name!='diagnostic':inputs += [ROOT/'internal-boot/boot/probe.c',ROOT/'internal-boot/unlock/sha256.c']
        for stage,command in (('compile',[*common,*defines,*map(str,inputs),'-o',str(exe)]),('run',[str(exe)])):
            r=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=30)
            (out/(name+'-'+stage+'.log')).write_text(r.stdout+r.stderr)
            if r.returncode:raise RuntimeError(name+' '+stage+': '+(r.stdout+r.stderr)[-2000:])
        legacy.append({'case':name,'result':r.stdout.strip()});print(name+': passed',flush=True)
    guards=[]
    for index,bad in enumerate(('-DGU_FULL_UNLOCK','-DGU_UEFI_PORT','-UGU_PROBE_GENERIC')):
        r=subprocess.run([*flags,bad,'-fsyntax-only',str(ROOT/'internal-boot/boot/probe-parent.c')],
                         cwd=ROOT,capture_output=True,text=True,timeout=30)
        (out/f'guard-{index}.log').write_text(r.stdout+r.stderr)
        if r.returncode==0 or 'Parent baseline is restricted' not in r.stderr:raise RuntimeError('Readonly parent compile guard failed')
        guards.append(bad)
    (out/'summary.json').write_text(json.dumps({'hardware_access':False,'efi_executed':False,
        'actual_producer_mocked':True,'cases':results,'legacy':legacy,'compile_guards':guards},indent=2))


if __name__=='__main__':main()
