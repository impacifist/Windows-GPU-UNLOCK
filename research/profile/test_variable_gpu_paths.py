"""RAM-only console backend test for variable GPU PCI ancestry.

The default LOCAL header is a synthetic compiler overlay. --generated-profile
uses actual generation with synthetic proof hashes and a stubbed FWSEC locator.
Neither mode produces write-build evidence or physical compatibility evidence.
No driver code, EFI image, GPU or firmware services execute.
"""
import argparse
import contextlib
import copy
import hashlib
import importlib.util
import itertools
import io
import json
from pathlib import Path
import subprocess
import sys
import struct
import re
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent


def load(name,file):
    spec=importlib.util.spec_from_file_location(name,HERE/file)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def dump(path):
    rows=[];at=0
    while at<len(path):
        length=int.from_bytes(path[at+2:at+4],'little')
        if length<4 or at+length>len(path):raise ValueError('Invalid synthetic node')
        rows.append('path-node raw='+path[at:at+length].hex());at+=length
    return '\n'.join(rows)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang',default='clang')
    parser.add_argument('--topology',choices=('aggregate','nonaggregate','no-edid'),default='aggregate')
    parser.add_argument('--generated-profile',action='store_true')
    parser.add_argument('--gpu',choices=('3090','3070','3060ti'),default='3090')
    parser.add_argument('--children',nargs='+',type=int,choices=(1,2,3,4),default=(1,2))
    parser.add_argument('--segment',type=lambda value:int(value,0),default=0)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();out=args.output.resolve()
    if not 0<=args.segment<=0xffff:parser.error('Segment must be in 0..65535')
    if args.topology!='no-edid' and any(children>2 for children in args.children):parser.error('Three/four children require the no-EDID topology')
    if out.exists():parser.error('Output must be a new directory')
    out.mkdir(parents=True)
    data=load('variable_path_data','test_local_topology.py')
    vectors=load('variable_path_vectors','test_gpu_path_contract.py')
    contract=load('variable_path_contract','gpu_path.py')
    image_data=load('variable_path_images','test_compatibility.py')
    refusals=load('variable_path_refusals','test_make_profile.py')
    envelope=copy.deepcopy(json.loads((HERE/'proven.json').read_text()))
    nv_size,nv_text=(0x26e00,0x2106f) if args.gpu=='3060ti' else (0x2be40,0x24426)
    nv_version='60015' if args.gpu=='3060ti' else '60009'
    for driver in envelope['nvgop']+[driver for chip in envelope['chips'] for driver in chip['nvgop']]+[
            driver for chip in envelope['chips'] for pins in chip.get('pci_nvgop',{}).values() for driver in pins]:
        if driver['version']==nv_version:driver['text_sha256']=hashlib.sha256(bytes(nv_text)).hexdigest()
    (out/'platform-config.h').write_text('#define GU_INTERNAL_PARTITION_INITIALIZER {0}\n#define GU_WINDOWS_PARTITION_INITIALIZER {0}\n')
    images=[]
    for name,size in (('edid.bin',0x9000),('nvgop.bin',nv_size),('prom.bin',1048576)):
        image=out/name;image.write_bytes(bytes(size));images.append(str(image))
    flags=[args.clang,'-std=c11','-O1','-Wall','-Wextra','-Werror','-fshort-wchar','-D_CRT_SECURE_NO_WARNINGS',
        '-DGU_PROFILE_LOCAL','-DGU_HEADLESS_RESEARCH','-DGU_SINGLE_RTX_CONSOLE','-DGU_FULL_UNLOCK',
        '-DGU_AUTOMATIC_BOOT','-DGU_AGGREGATE_CAPTURE_FIXTURE','-I',str(out),
        '-I',str(ROOT/'deps/edk2/MdePkg/Include'),'-I',str(ROOT/'deps/edk2/MdePkg/Include/X64')]
    def run(log,command):
        result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
        log.write_text(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError(str(log)+': '+(result.stdout+result.stderr)[-2500:])
        return result.stdout.strip()
    summaries=[]
    aggregate=args.topology=='aggregate';no_edid=args.topology=='no-edid'
    card={'board':0x248410de,'chip':0xb74000a1,'memory':0x2000} if args.gpu=='3070' else {}
    if args.gpu=='3060ti':card={'board':0x24c910de,'chip':0xb74000a1,'memory':0x2000}
    board=card.get('board',0x220410de)
    for hops,hid,children in itertools.product((1,2,3,8,16),(3,8),args.children):
        case=out/f'hops{hops}-root{hid}-children{children}';case.mkdir()
        legacy=data.synthetic_capture(aggregate=aggregate,no_edid=no_edid,children=children,**card)
        baseline=data.analyze(legacy,envelope=envelope)
        primary=vectors.path(hops,hid,0 if hid==3 else 42)
        layout=contract.decode_gpu_display_path(primary)
        controller=primary[:layout['controller_bytes']]
        text=legacy.replace(dump(data.PCI_PATH),dump(controller))
        text=text.replace('probe-target seg=0x0000 ',f'probe-target seg=0x{args.segment:04x} ')
        text=text.replace('pci location-status=0x0 segment=0x0 ',f'pci location-status=0x0 segment=0x{args.segment:x} ')
        if text==legacy:raise RuntimeError('Synthetic route substitution did not apply')
        capture=case/'DIAG00.TXT';capture.write_text(text)
        fixture=case/'fixture.h'
        defines=dict(baseline['defines'])
        actual_primary=controller+data.RTX_PATH[-12:]
        defines.update(GU_RTX_PATH_BYTES='\\\n  '+','.join(str(byte) for byte in actual_primary),
            GU_TARGET_SEGMENT=f'0x{args.segment:x}U',
            GU_ROOT_PORT_DEVICE=str(layout['pci_nodes'][0][0])+'U',
            GU_ROOT_PORT_FUNCTION=str(layout['pci_nodes'][0][1])+'U',
            GU_NVGOP_TEXT_BYTES=f'0x{nv_text:x}U',GU_NVGOP_TEXT_SHA256='"'+hashlib.sha256(bytes(nv_text)).hexdigest()+'"')
        if not no_edid:
            defines.update(GU_EDID_TEXT_BYTES='0x800U',GU_EDID_TEXT_SHA256='"'+hashlib.sha256(bytes(0x800)).hexdigest()+'"')
        header=case/'profile-local.h'
        header.write_bytes(('#define GU_LOCAL_PROFILE_GENERATOR 1\n#define GU_PROFILE_NAME "LOCAL-SYNTHETIC-LONG-PATH"\n'+
            '\n'.join(f'#define {key} {value}' for key,value in defines.items())+'\n').encode())
        if args.generated_profile:
            for agent,index in ((3,0),(9,1)):
                if agent==9 and no_edid:continue
                pattern=rf'(^binding handle=0x{agent:x} .*?)(?=^binding handle=|^handles status=)'
                text,hits=re.subn(pattern,lambda m:m[1]+f'img dump index={index:02x}\n',text,flags=re.M|re.S)
                if hits!=1:raise RuntimeError('Missing synthetic image binding')
            for index in range(envelope['min_captures']):(case/f'DIAG{index:02d}.TXT').write_text(text)
            (case/'IMG00.BIN').write_bytes(image_data.driver_image(nv_size,0x280,nv_text))
            if not no_edid:(case/'IMG01.BIN').write_bytes(image_data.driver_image(0x9000,0x1000,0x800))
            (case/'PROM.BIN').write_bytes(bytes(1048576))
            log=io.StringIO()
            with patch.object(refusals,'ENVELOPE',envelope),patch.object(refusals,'failures',0), \
                 patch.object(refusals.mp,'locate_fwsec',return_value={'image_off':0x100,'image_size':0x200}), \
                 patch.object(sys,'argv',['test_make_profile.py',str(case),'not-invoked']),contextlib.redirect_stdout(log):
                refusal_exit=refusals.main()
            (case/'generator-refusals.log').write_text(log.getvalue())
            if refusal_exit:raise RuntimeError(log.getvalue())
            with patch.object(data.mp,'locate_fwsec',return_value={'image_off':0x100,'image_size':0x200}):
                generated,report,_=data.mp.generate(case,Path('not-invoked'),envelope)
            header.write_bytes(generated.encode())
            (case/'profile.json').write_text(json.dumps(report,indent=2))
            # Native replay reads the exact loaded PE bytes that generation hashed.
            images=[str(case/name) for name in ('IMG01.BIN','IMG00.BIN','PROM.BIN')]
        run(case/'fixture.log',[sys.executable,'-I',str(ROOT/'internal-boot/boot/make-console-fixture.py'),str(capture),
            *(['--aggregate'] if aggregate else []),*(['--no-edid-driver'] if no_edid else []),
            '--single-rtx','--no-igpu','--no-edid-child',f'--rtx-id={board:08x}','--output='+str(fixture)])
        if hashlib.sha256(capture.read_bytes()).hexdigest() not in fixture.read_text():
            raise RuntimeError('Fixture does not identify the generated profile capture')
        overlay=case/'overlay.json'
        overlay.write_text(json.dumps({'version':0,'roots':[
            {'type':'file','name':str(ROOT/'internal-boot/unlock/profile-local.h'),'external-contents':str(header),'use-external-name':False},
            {'type':'file','name':str(ROOT/'internal-boot/boot/build/aggregate/replay-fixture.h'),'external-contents':str(fixture),'use-external-name':False},
            {'type':'file','name':str(ROOT/'internal-boot/boot/build/r2/replay-fixture.h'),'external-contents':str(fixture),'use-external-name':False}]}))
        config=case/'config.bin';config.write_bytes(struct.pack('<64I',*data.configuration(board=board)))
        release_args=images if aggregate else [*(images[1:] if no_edid else images),str(config)]
        variants=(('prepare','test_aggregate_prepare.c' if aggregate else 'test_r2.c',[]),
                  ('release','test_aggregate_full.c' if aggregate else 'test_headless_full.c',release_args))
        for stage,source,arguments in variants:
            exe=case/(stage+'.exe')
            stage_flags=flags
            extra=[]
            if not aggregate:
                stage_flags=[flag for flag in flags if flag!='-DGU_AGGREGATE_CAPTURE_FIXTURE']
                if stage=='prepare':
                    stage_flags=[flag for flag in stage_flags if flag not in ('-DGU_HEADLESS_RESEARCH','-DGU_FULL_UNLOCK','-DGU_AUTOMATIC_BOOT')]
                    stage_flags+=['-DGU_DIAGNOSTIC_ONLY','-DGU_CONSOLE_PREPARE']
                    extra=[str(ROOT/'internal-boot/boot/research-input.c')]
            run(case/(stage+'-compile.log'),[*stage_flags,'-ivfsoverlay',str(overlay),*extra,
                str(ROOT/'internal-boot/boot'/source),str(ROOT/'internal-boot/boot/console-flow.c'),
                str(ROOT/'internal-boot/unlock/checks.c'),str(ROOT/'internal-boot/unlock/sha256.c'),'-o',str(exe)])
            result=run(case/(stage+'.log'),[str(exe),*arguments])
            summaries.append({'case':case.name,'path_bytes':len(actual_primary),'controller_bytes':len(controller),
                'stage':stage,'result':result})
        probe=case/'probe.txt'
        probe.write_text(''.join(f'config_offset=0x{i*4:x}\nvalue=0x{word:08x}\n' for i,word in enumerate(data.configuration(board=board))))
        checks=case/'checks.exe'
        run(case/'checks-compile.log',[*flags,'-ivfsoverlay',str(overlay),
            str(ROOT/'internal-boot/unlock/tests/test_checks.c'),str(ROOT/'internal-boot/unlock/checks.c'),'-o',str(checks)])
        result=run(case/'checks.log',[str(checks),str(probe)])
        summaries.append({'case':case.name,'path_bytes':len(actual_primary),'controller_bytes':len(controller),
            'stage':'checks and path classification','result':result})
        for stage,source,extra,sources in (
            ('fresh-identity','../unlock/tests/test_refresh_identity.c',['-DGU_HOST_WRITE_TEST'],
                [str(ROOT/'internal-boot/unlock/ownership.c'),str(ROOT/'internal-boot/unlock/port.c')]),
            ('cold','test_cold_precheck.c',[],[str(ROOT/'internal-boot/boot/console-flow.c'),
                str(ROOT/'internal-boot/unlock/checks.c'),str(ROOT/'internal-boot/unlock/sha256.c')]),
            ('pci-layout','../unlock/tests/test_pci_layout.c',['-DGU_HOST_WRITE_TEST'],
                [str(ROOT/'internal-boot/unlock/checks.c'),str(ROOT/'internal-boot/unlock/port.c')])):
            exe=case/(stage+'.exe')
            run(case/(stage+'-compile.log'),[*flags,*extra,'-ivfsoverlay',str(overlay),
                str(ROOT/'internal-boot/boot'/source),*sources,'-o',str(exe)])
            result=run(case/(stage+'.log'),[str(exe)])
            summaries.append({'case':case.name,'path_bytes':len(actual_primary),'controller_bytes':len(controller),
                'stage':stage,'result':result})
        print(case.name+': prepare and release passed; synthetic backend overlay only',flush=True)
    (out/'summary.json').write_text(json.dumps({'synthetic_only':True,'hardware_access':False,
        'profile_generated':args.generated_profile,'fwsec_locator_stubbed':args.generated_profile,
        'synthetic_proof_envelope':True,'local_evidence_written':False,'gpu':args.gpu,'segment':args.segment,'topology':args.topology,'cases':summaries},indent=2))


if __name__=='__main__':main()
