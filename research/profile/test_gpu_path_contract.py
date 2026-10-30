"""Native/Python path-contract agreement on bounded synthetic buffers only."""
import argparse
import importlib.util
import json
from pathlib import Path
import random
import subprocess

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('gpu_path_contract_subject',Path(__file__).with_name('gpu_path.py'))
subject=importlib.util.module_from_spec(spec);spec.loader.exec_module(subject)


def path(nodes,hid=3,uid=0):
    data=bytearray(bytes.fromhex('02010c00d041')+bytes([hid,0x0a])+uid.to_bytes(4,'little'))
    for i in range(nodes):data+=bytes([1,1,6,0,i%8 if i+1<nodes else 0,31-i%32 if i+1<nodes else 0])
    return bytes(data)+bytes.fromhex('02030800700301807fff0400')


def vectors():
    for nodes in range(1,18):
        for hid,uid in ((3,0),(8,0xffffffff)):
            good=path(nodes,hid,uid)
            yield good,len(good)
            for bound in range(len(good)):yield good,bound
            yield good+bytes(256-len(good)),256
            for at in range(len(good)):
                for mask in (1,0x40):
                    bad=bytearray(good);bad[at]^=mask;yield bytes(bad),len(bad)
    rng=random.Random(0x504349)
    for _ in range(2000):
        size=rng.randrange(1,257)
        yield bytes(rng.randrange(256) for _ in range(size)),rng.randrange(257)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang',default='clang')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();out=args.output.resolve()
    if out.exists():parser.error('Output must be a new directory')
    out.mkdir(parents=True)
    flags=[args.clang,'-std=c11','-O1','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS',
        '-I',str(ROOT/'deps/edk2/MdePkg/Include'),'-I',str(ROOT/'deps/edk2/MdePkg/Include/X64')]
    def run(name,command,input=None):
        result=subprocess.run(command,input=input,capture_output=True,text=True,cwd=ROOT)
        (out/(name+'.log')).write_text(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError(name+': '+(result.stdout+result.stderr)[-2000:])
        return result.stdout
    checks=out/'checks.exe';compare=out/'vectors.exe'
    for name,source,exe in (('checks','test_gpu_paths.c',checks),('vectors','test_gpu_path_vectors.c',compare)):
        run(name+'-compile',[*flags,str(ROOT/'internal-boot/unlock/tests'/source),'-o',str(exe)])
    print(run('checks',[str(checks)]).strip())
    cases=list(vectors())
    rows=run('vectors',[str(compare)],''.join(f'{bound} {data.hex()}\n' for data,bound in cases)).splitlines()
    if len(rows)!=len(cases):raise RuntimeError('Missing native results')
    accepted=0
    for index,((data,bound),row) in enumerate(zip(cases,rows)):
        native=tuple(map(int,row.split()))
        try:
            info=subject.decode_gpu_display_path(data.ljust(256,b'\0')[:bound],allow_trailing=True)
            expected=(1,info['controller_bytes'],info['path_bytes']);accepted+=1
        except ValueError:expected=(0,0,0)
        if native!=expected:raise RuntimeError(f'C/Python mismatch in vector {index}: {native} != {expected}')
    summary={'vectors':len(cases),'accepted':accepted,'rejected':len(cases)-accepted,'hardware_access':False}
    (out/'summary.json').write_text(json.dumps(summary,indent=2))
    print('PASS identical native/Python bounded path decisions: '+str(len(cases)))


if __name__=='__main__':main()
