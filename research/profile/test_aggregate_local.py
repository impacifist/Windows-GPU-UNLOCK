"""Generate LOCAL aggregate profiles and replay production code in host RAM.

All capture/image/PROM bytes and proof-envelope hashes are synthetic. Only the
FWSEC locator result is stubbed: this tests integration, not the PROM parser,
vendor transaction, EFI execution, or hardware compatibility.
"""
import argparse
import copy
import contextlib
import hashlib
import importlib.util
import itertools
import io
import json
from pathlib import Path
import re
import subprocess
import sys
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent


def load(name,file):
    spec=importlib.util.spec_from_file_location(name,HERE/file)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang',default='clang')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();out=args.output.resolve()
    if out.exists():parser.error('Output must be a new directory')
    out.mkdir(parents=True)
    data=load('aggregate_local_data','test_local_topology.py')
    images=load('aggregate_local_images','test_compatibility.py')
    refusals=load('aggregate_local_refusals','test_make_profile.py')
    envelope=json.loads((HERE/'proven.json').read_text())
    lengths={'60009':0x24426,'60010':0x20d33,'60015':0x2106f}
    envelope=copy.deepcopy(envelope)
    # Replace only code hashes for zero-filled executable sections in this test.
    # The actual installed proven.json and production files are never edited.
    for driver in envelope['nvgop']:
        if driver['version'] in lengths:driver['text_sha256']=hashlib.sha256(bytes(lengths[driver['version']])).hexdigest()
    for chip in envelope['chips']:
        for driver in chip['nvgop']+[driver for pins in chip.get('pci_nvgop',{}).values() for driver in pins]:
            if driver['version'] in lengths:driver['text_sha256']=hashlib.sha256(bytes(lengths[driver['version']])).hexdigest()
    (out/'platform-config.h').write_text('#define GU_INTERNAL_PARTITION_INITIALIZER {0}\n#define GU_WINDOWS_PARTITION_INITIALIZER {0}\n')
    summaries=[]
    def run(path,command,expect=0):
        result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
        path.write_text(result.stdout+result.stderr)
        if result.returncode!=expect:raise RuntimeError(str(path)+': '+(result.stdout+result.stderr)[-2500:])
        return result.stdout.strip()
    for children,(board,chip,memory) in itertools.product((1,2),(
        (0x220410de,0xb72000a1,0x6000),(0x220310de,0xb72000a1,0x5ff4),
        (0x248710de,0xb74000a1,0x3000),(0x250410de,0xb76000a1,0x3000),
        (0x248410de,0xb74000a1,0x2000),(0x24c910de,0xb74000a1,0x2000))):
        case=out/f'id{board:x}-children{children}';case.mkdir()
        capture=case/'capture';capture.mkdir()
        text=data.synthetic_capture(aggregate=True,no_edid=False,board=board,chip=chip,memory=memory,
                                    children=children,bus=0x2b if children==2 else 1,command=2 if children==2 else 3)
        if board==0x220410de and children==2:
            text=text.replace('38811462','145410de')  # Exercise the alternate foreign-profile selector.
        if board==0x250410de and children==2:
            # A real target route may coincide with the old AMD ancestry constant.
            # PCI identity, not that historical root-port number, selects the GPU.
            text=text.replace('path-node raw=010106000103','path-node raw=010106000108')
        # Bind the actual code_hash parser to the complete loaded PE images.
        for agent,index in ((3,0),(9,1)):
            pattern=rf'(^binding handle=0x{agent:x} .*?)(?=^binding handle=|^handles status=)'
            text,hits=re.subn(pattern,lambda m:m[1]+f'img dump index={index:02x}\n',text,flags=re.M|re.S)
            if hits!=1:raise RuntimeError('Missing synthetic binding')
        for index in range(envelope['min_captures']):(capture/f'DIAG{index:02d}.TXT').write_text(text)
        version='60009' if board in (0x220410de,0x248410de) else '60010'
        if board==0x24c910de:version='60015'
        nv_size={'60009':0x2be40,'60010':0x26ae0,'60015':0x26e00}[version]
        (capture/'IMG00.BIN').write_bytes(images.driver_image(nv_size,0x280,lengths[version]))
        (capture/'IMG01.BIN').write_bytes(images.driver_image(0x9000,0x1000,0x800))
        (capture/'PROM.BIN').write_bytes(bytes(1048576))
        refusal_log=io.StringIO()
        with patch.object(refusals,'ENVELOPE',envelope),patch.object(refusals,'failures',0), \
             patch.object(refusals.mp,'locate_fwsec',return_value={'image_off':0x100,'image_size':0x200}), \
             patch.object(sys,'argv',['test_make_profile.py',str(capture),'not-invoked']), \
             contextlib.redirect_stdout(refusal_log):
            refusal_exit=refusals.main()
        (case/'generator-refusals.log').write_text(refusal_log.getvalue())
        if refusal_exit:raise RuntimeError(refusal_log.getvalue())
        with patch.object(data.mp,'locate_fwsec',return_value={'image_off':0x100,'image_size':0x200}):
            header,report,fixtures=data.mp.generate(capture,Path('not-invoked'),envelope)
        (case/'profile-local.h').write_bytes(header.encode())
        (case/'profile.json').write_text(json.dumps(report,indent=2))
        if not report['Replay']['Aggregate'] or report['Replay']['ChildCount']!=children:raise RuntimeError('Wrong topology metadata')
        fixture=case/'fixture.h'
        run(case/'fixture.log',[sys.executable,'-I',str(ROOT/'internal-boot/boot/make-console-fixture.py'),
            str(capture/'DIAG00.TXT'),'--aggregate','--single-rtx','--no-igpu','--no-edid-child',f'--rtx-id={board:08x}','--output='+str(fixture)])
        overlay=case/'overlay.json'
        overlay.write_text(json.dumps({'version':0,'roots':[
            {'type':'file','name':str(ROOT/'internal-boot/unlock/profile-local.h'),'external-contents':str(case/'profile-local.h'),'use-external-name':False},
            {'type':'file','name':str(ROOT/'internal-boot/boot/build/aggregate/replay-fixture.h'),'external-contents':str(fixture),'use-external-name':False},
            {'type':'file','name':str(ROOT/'internal-boot/boot/build/r2/replay-fixture.h'),'external-contents':str(fixture),'use-external-name':False}]}))
        flags=[args.clang,'-std=c11','-O1','-Wall','-Wextra','-Werror','-fshort-wchar','-D_CRT_SECURE_NO_WARNINGS',
            '-DGU_PROFILE_LOCAL','-DGU_HEADLESS_RESEARCH','-DGU_SINGLE_RTX_CONSOLE','-DGU_FULL_UNLOCK',
            '-DGU_AUTOMATIC_BOOT','-DGU_AGGREGATE_CAPTURE_FIXTURE','-I',str(out),
            '-I',str(ROOT/'deps/edk2/MdePkg/Include'),'-I',str(ROOT/'deps/edk2/MdePkg/Include/X64'),'-ivfsoverlay',str(overlay)]
        for stage,source,arguments in (
            ('prepare','test_aggregate_prepare.c',[]),
            ('release','test_aggregate_full.c',[str(capture/name) for name in ('IMG01.BIN','IMG00.BIN','PROM.BIN')]),
            ('cold','test_cold_precheck.c',[])):
            exe=case/(stage+'.exe')
            sources=[str(ROOT/'internal-boot/boot'/source),str(ROOT/'internal-boot/boot/console-flow.c'),
                     str(ROOT/'internal-boot/unlock/checks.c'),str(ROOT/'internal-boot/unlock/sha256.c')]
            run(case/(stage+'-compile.log'),[*flags,*sources,'-o',str(exe)])
            result=run(case/(stage+'.log'),[str(exe),*arguments])
            print(case.name+' '+result,flush=True)
            summaries.append({'case':case.name,'stage':stage,'result':result})
        # Same negative harness as test-profile.ps1, not the aggregate-only one:
        # a foreign non-aggregate profile must compile, then reject this capture.
        foreign=case/'foreign.exe'
        foreign_flags=[part for part in flags if part not in (
            '-DGU_PROFILE_LOCAL','-DGU_HEADLESS_RESEARCH','-DGU_FULL_UNLOCK',
            '-DGU_AUTOMATIC_BOOT','-DGU_AGGREGATE_CAPTURE_FIXTURE')]
        subsystem=int(re.search(r'^#define GU_BOARD_SUBSYSTEM 0x([0-9a-f]+)U$',header,re.M)[1],16)
        reviewed=(('B650M_3090FE',0x220410de,0x145410de),('B660M_3070TI',0x248210de,0x50521462),('X870_3090TI',0x220310de,0x88701043))
        foreign_profile=next(name for name,pci,subsys in reviewed if (pci,subsys)!=(board,subsystem))
        foreign_flags+=['-DGU_PROFILE_'+foreign_profile,'-DGU_DIAGNOSTIC_ONLY','-DGU_CONSOLE_PREPARE']
        run(case/'foreign-compile.log',[*foreign_flags,*[str(ROOT/'internal-boot/boot'/name)
            for name in ('test_r2.c','research-input.c','console-flow.c')],'-o',str(foreign)])
        result=run(case/'foreign.log',[str(foreign)],expect=1)
        summaries.append({'case':case.name,'stage':'foreign rejection','profile':foreign_profile,'expected_exit':1,'result':result})
    (out/'summary.json').write_text(json.dumps({'synthetic_only':True,'hardware_access':False,
        'fwsec_locator_stubbed':True,'vendor_transaction_tested':False,'cases':summaries},indent=2))


if __name__=='__main__':main()
