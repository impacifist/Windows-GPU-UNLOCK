"""Production aggregate prepare/release with synthetic image pins and RAM EFI services.

VFS overlays change host test inputs only. No EFI is built, staged or executed.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang',default='clang')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    out=args.output.resolve()
    if out.exists():parser.error('Output must be a new directory')
    out.mkdir(parents=True)
    (out/'platform-config.h').write_text('#define GU_INTERNAL_PARTITION_INITIALIZER {0}\n#define GU_WINDOWS_PARTITION_INITIALIZER {0}\n')
    machine=(ROOT/'internal-boot/unlock/machine-profile.h').read_text()
    prom=hashlib.sha256(bytes(1048576)).hexdigest()
    machine,count=re.subn(r'(#define GU_PROM_SHA256 ")[0-9a-f]{64}("[\r\n])',lambda m:m[1]+prom+m[2],machine)
    if count!=6:raise RuntimeError('Unexpected fixed profile PROM scope')
    (out/'machine-profile.h').write_text(machine)
    firmware=(ROOT/'internal-boot/unlock/firmware-profile.h').read_text()
    # The B660M template and shared 60010 code have no relocations in hashed text.
    for macro,length in (('GU_EDID_TEXT_SHA256',0x37da),('GU_NVGOP_TEXT_SHA256',0x20d33)):
        pattern=rf'(#define {macro} ")[0-9a-f]{{64}}"'
        firmware=re.sub(pattern,lambda m:m[1]+hashlib.sha256(bytes(length)).hexdigest()+'"',firmware)
    (out/'firmware-profile.h').write_text(firmware)
    overlay={'version':0,'roots':[{'type':'file','name':str(ROOT/'internal-boot/unlock'/name),
            'external-contents':str(out/name),'use-external-name':False} for name in ('machine-profile.h','firmware-profile.h')]}
    (out/'overlay.json').write_text(json.dumps(overlay))
    summaries=[]
    spec=importlib.util.spec_from_file_location('aggregate_replay_synthetic_data',Path(__file__).with_name('test_local_topology.py'))
    data=importlib.util.module_from_spec(spec);spec.loader.exec_module(data)
    for profile in ('B660M_3090TI','B660PLUS_3060'):
        exe=out/(profile+'.exe')
        command=[args.clang,'-std=c11','-O1','-Wall','-Wextra','-Werror','-fshort-wchar','-D_CRT_SECURE_NO_WARNINGS',
                 '-DGU_PROFILE_'+profile,'-DGU_HEADLESS_RESEARCH','-DGU_SINGLE_RTX_CONSOLE','-DGU_FULL_UNLOCK','-DGU_AUTOMATIC_BOOT',
                 '-I',str(out),'-I',str(ROOT/'deps/edk2/MdePkg/Include'),'-I',str(ROOT/'deps/edk2/MdePkg/Include/X64'),
                 '-ivfsoverlay',str(out/'overlay.json'),str(ROOT/'internal-boot/boot/test_aggregate_full.c'),
                 str(ROOT/'internal-boot/boot/console-flow.c'),str(ROOT/'internal-boot/unlock/checks.c'),str(ROOT/'internal-boot/unlock/sha256.c'),'-o',str(exe)]
        compiled=subprocess.run(command,capture_output=True,text=True)
        (out/(profile+'-compile.log')).write_text(compiled.stdout+compiled.stderr)
        if compiled.returncode:raise RuntimeError(compiled.stderr[-3000:])
        result=subprocess.run([str(exe)],capture_output=True,text=True)
        (out/(profile+'.log')).write_text(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
        print(result.stdout.strip(),flush=True)
        summaries.append({'profile':profile,'result':result.stdout.strip()})
        prepare_command=[part.replace('test_aggregate_full.c','test_aggregate_prepare.c') for part in command[:-2]]
        prepare_exe=out/(profile+'-prepare.exe')
        compiled=subprocess.run([*prepare_command,'-o',str(prepare_exe)],capture_output=True,text=True)
        (out/(profile+'-prepare-compile.log')).write_text(compiled.stdout+compiled.stderr)
        if compiled.returncode:raise RuntimeError(compiled.stderr[-3000:])
        result=subprocess.run([str(prepare_exe)],capture_output=True,text=True)
        (out/(profile+'-prepare.log')).write_text(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
        print(result.stdout.strip(),flush=True)
        summaries.append({'profile':profile,'prepare_only':True,'result':result.stdout.strip()})
        board,subsystem,chip,memory,children,adr=(0x220310de,0x13017377,0xb72000a1,0x5ff4,1,0x20) if profile=='B660M_3090TI' else (0x248710de,0x153010de,0xb74000a1,0x3000,2,0)
        case=out/(profile+'-capture');case.mkdir()
        text=data.synthetic_capture(board=board,chip=chip,memory=memory,children=children,
                                    bus=1,aggregate=True,no_edid=False)
        text=text.replace('f5000000','82000000').replace('38811462',f'{subsystem:08x}')
        text=text.replace('path-node raw=010106000103','path-node raw=010106000001')
        text=text.replace('path-node raw=0203080070030180',f'path-node raw=02030800{adr:02x}030180')
        if profile=='B660PLUS_3060':
            # A distinct stderr interface and non-IO/MEM attribute bits must not
            # be normalized away by the replay adapter.
            text=text.replace('console out=0x6 error=0x6 input=0x6','console out=0x6 error=0x8 input=0x6')
            text=text.replace('handle id=0x8\n','handle id=0x8\nprotocol guid='+data.mp.TEXT_OUT+'\nopens status=0x0000000000000000 count=0x0\n')
            text=text.replace('00100003','00100002').replace('value=0x0000000000000300','value=0x0000000000004200')
        capture=case/'DIAG00.TXT';capture.write_text(text)
        fixture=case/'fixture.h'
        subprocess.run([sys.executable,'-I',str(ROOT/'internal-boot/boot/make-console-fixture.py'),str(capture),
                        '--aggregate','--single-rtx','--no-igpu','--no-edid-child',f'--rtx-id={board:08x}',
                        '--output='+str(fixture)],check=True,capture_output=True,text=True)
        captured_overlay=json.loads(json.dumps(overlay))
        captured_overlay['roots'].append({'type':'file','name':str(ROOT/'internal-boot/boot/build/aggregate/replay-fixture.h'),
            'external-contents':str(fixture),'use-external-name':False})
        overlay_path=case/'overlay.json';overlay_path.write_text(json.dumps(captured_overlay))
        capture_exe=case/'replay.exe'
        captured_command=[str(overlay_path) if part==str(out/'overlay.json') else part for part in command[:-2]]
        compiled=subprocess.run([*captured_command,'-DGU_AGGREGATE_CAPTURE_FIXTURE','-o',str(capture_exe)],capture_output=True,text=True)
        (case/'compile.log').write_text(compiled.stdout+compiled.stderr)
        if compiled.returncode:raise RuntimeError(compiled.stderr[-3000:])
        image_paths=[case/'edid.bin',case/'nvgop.bin',case/'prom.bin']
        for path,size in zip(image_paths,(0x9000,0x26ae0,1048576)):path.write_bytes(bytes(size))
        result=subprocess.run([str(capture_exe),*[str(path) for path in image_paths]],capture_output=True,text=True)
        (case/'replay.log').write_text(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
        print('CAPTURE '+result.stdout.strip(),flush=True)
        summaries.append({'profile':profile,'capture_graph':True,'result':result.stdout.strip()})
        refusals=0
        for index,path in enumerate(image_paths):
            for size in (0,2,path.stat().st_size-1,path.stat().st_size+1):
                bad=case/f'bad-{index}-{size}.bin';bad.write_bytes(bytes(size))
                inputs=image_paths.copy();inputs[index]=bad
                failed=subprocess.run([str(capture_exe),*[str(item) for item in inputs]],capture_output=True,text=True)
                if failed.returncode!=1 or 'input image size mismatch mode=0 calls=0' not in failed.stderr:
                    raise RuntimeError('Captured image length did not fail safely: '+failed.stdout+failed.stderr)
                refusals+=1
        summaries[-1]['image_refusals']=refusals
        prepare_command=[part.replace('test_aggregate_full.c','test_aggregate_prepare.c') for part in captured_command]
        prepare_exe=case/'prepare.exe'
        compiled=subprocess.run([*prepare_command,'-DGU_AGGREGATE_CAPTURE_FIXTURE','-o',str(prepare_exe)],capture_output=True,text=True)
        (case/'prepare-compile.log').write_text(compiled.stdout+compiled.stderr)
        if compiled.returncode:raise RuntimeError(compiled.stderr[-3000:])
        result=subprocess.run([str(prepare_exe)],capture_output=True,text=True)
        (case/'prepare.log').write_text(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
        print('CAPTURE '+result.stdout.strip(),flush=True)
        summaries.append({'profile':profile,'capture_graph':True,'prepare_only':True,'result':result.stdout.strip()})
    input_exe=out/'research-input.exe'
    command=[args.clang,'-std=c11','-O1','-Wall','-Wextra','-Werror','-fshort-wchar',
             '-I',str(ROOT/'deps/edk2/MdePkg/Include'),'-I',str(ROOT/'deps/edk2/MdePkg/Include/X64'),
             *[str(ROOT/'internal-boot/boot'/name) for name in ('test_research_input.c','research-input.c','console-flow.c')],'-o',str(input_exe)]
    compiled=subprocess.run(command,capture_output=True,text=True)
    (out/'research-input-compile.log').write_text(compiled.stdout+compiled.stderr)
    if compiled.returncode:raise RuntimeError(compiled.stderr[-3000:])
    result=subprocess.run([str(input_exe)],capture_output=True,text=True)
    (out/'research-input.log').write_text(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(result.stdout+result.stderr)
    print(result.stdout.strip(),flush=True)
    summaries.append({'input_policy':True,'result':result.stdout.strip()})
    (out/'summary.json').write_text(json.dumps({'synthetic_only':True,'hardware_access':False,'cases':summaries},indent=2))


if __name__=='__main__':main()
