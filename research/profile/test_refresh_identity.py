"""Run actual PCI refresh code with RAM callbacks; no GPU or firmware execution."""
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
    results=[]
    for profile in ('B650M_3090FE','B660M_3070TI','B660M_3090TI','X870_3090TI','B660PLUS_3060','H610M_3060'):
        for headless in (False,True):
            name=profile+('-headless' if headless else '-manual');exe=out/(name+'.exe')
            command=[args.clang,'-std=c11','-O1','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS',
                '-DGU_HOST_WRITE_TEST','-DGU_PROFILE_'+profile,*(['-DGU_HEADLESS_RESEARCH'] if headless else []),
                '-I',str(ROOT/'deps/edk2/MdePkg/Include'),'-I',str(ROOT/'deps/edk2/MdePkg/Include/X64'),
                str(ROOT/'internal-boot/unlock/tests/test_refresh_identity.c'),str(ROOT/'internal-boot/unlock/ownership.c'),
                str(ROOT/'internal-boot/unlock/port.c'),'-o',str(exe)]
            for stage,cmd in (('compile',command),('run',[str(exe)])):
                result=subprocess.run(cmd,capture_output=True,text=True,cwd=ROOT)
                (out/(name+'-'+stage+'.log')).write_text(result.stdout+result.stderr)
                if result.returncode:raise RuntimeError(name+' '+stage+': '+(result.stdout+result.stderr)[-1800:])
                if stage=='run':results.append({'case':name,'result':result.stdout.strip()})
    (out/'summary.json').write_text(json.dumps({'hardware_access':False,'synthetic_only':True,'cases':results},indent=2))
    print(f'PASS {len(results)} manual/headless PCI refresh configurations; RAM-only')


if __name__=='__main__':main()
