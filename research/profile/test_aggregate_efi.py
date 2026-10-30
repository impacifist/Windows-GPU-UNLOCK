"""Freestanding compile/link/PE regression for synthetic generated LOCAL headers.

Build artifacts stay in a new ignored directory, never execute, and are not an
authorized package. This deliberately does not write or bypass LOCAL evidence.
Partition GUIDs are zero; the child contract refers only to the test build.
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
UNLOCK=ROOT/'internal-boot/unlock'
BOOT=ROOT/'internal-boot/boot'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang',default='clang')
    parser.add_argument('--lld-link',default='lld-link')
    parser.add_argument('--profiles',type=Path,required=True,help='Completed test_aggregate_local.py output')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();out=args.output.resolve();profiles=args.profiles.resolve()
    if out.exists():parser.error('Output must be a new directory')
    inputs=json.loads((profiles/'summary.json').read_text())
    if inputs.get('synthetic_only') is not True or inputs.get('hardware_access') is not False:
        parser.error('Expected synthetic host replay inputs')
    cases=sorted({row['case'] for row in inputs['cases'] if row['stage']=='release'})
    if not cases:parser.error('No completed LOCAL release cases')
    if any(not re.fullmatch(r'(id[0-9a-f]{8}|hops(1|2|3|8|16)-root[38])-children[1-4]',name) for name in cases):
        parser.error('Unexpected synthetic case path')
    out.mkdir(parents=True)
    (out/'NOT-A-PACKAGE.txt').write_text('Synthetic compile regression only. No LOCAL evidence or package authorization. Do not stage or execute.\n')
    platform=out/'platform-config.h'
    platform.write_text('#define GU_INTERNAL_PARTITION_INITIALIZER {0}\n#define GU_WINDOWS_PARTITION_INITIALIZER {0}\n')
    spec=importlib.util.spec_from_file_location('aggregate_efi_verify_pe',UNLOCK/'verify_pe.py')
    verifier=importlib.util.module_from_spec(spec);spec.loader.exec_module(verifier)
    base=[args.clang,'-target','x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fno-builtin',
        '-fno-stack-protector','-mno-red-zone','-fshort-wchar','-Os','-Wall','-Wextra','-Werror',
        '-I',str(ROOT/'deps/edk2/MdePkg/Include'),'-I',str(ROOT/'deps/edk2/MdePkg/Include/X64')]
    child_sources=['app','app_io','ownership','format','runtime','sha256','checks','port','compat',
        'vendor/unlock_610','vendor/prep_610','vendor/layout_610','vendor/vbios_610','vendor/gfw_eval']
    wrapper_sources=[BOOT/name for name in ('handoff.c','full-release.c','full-flow.c','diagnostic.c')]
    wrapper_sources += [UNLOCK/'checks.c',UNLOCK/'sha256.c',ROOT/'internal-boot/efi_runtime.c']
    summaries=[]
    def run(log,command):
        result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
        log.write_text(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError(str(log)+': '+(result.stdout+result.stderr)[-3000:])
    def build(case,kind,sources,flags,overlay):
        folder=case/kind;folder.mkdir();objects=[]
        for index,source in enumerate(sources):
            obj=folder/f'{index:02d}-{source.stem}.obj';objects.append(str(obj))
            run(folder/(obj.stem+'.log'),[*base,*flags,'-ivfsoverlay',str(overlay),'-c',str(source),'-o',str(obj)])
        binary=folder/'SYNTHETIC-NOT-FOR-EXECUTION.efi';map_file=folder/'symbols.map'
        run(folder/'link.log',[args.lld_link,'/nologo','/nodefaultlib','/subsystem:efi_application',
            '/entry:efi_main','/machine:x64','/base:0','/dynamicbase','/nxcompat','/timestamp:0',
            '/opt:ref','/opt:icf','/map:'+str(map_file),'/out:'+str(binary),*objects])
        result=verifier.inspect(binary,map_file)
        (folder/'pe-verification.json').write_text(json.dumps(result,indent=2))
        return binary,result
    for name in cases:
        case=out/name;case.mkdir();header=profiles/name/'profile-local.h'
        # Verify the exact header provenance emitted by the host generator run.
        report=json.loads((profiles/name/'profile.json').read_text())
        if hashlib.sha256(header.read_bytes()).hexdigest()!=report['HeaderSha256']:
            raise RuntimeError('Generated header changed: '+name)
        if type(report['Replay']['Aggregate']) is not bool or report['HardwareTested'] is not False:
            raise RuntimeError('Expected an untested generated LOCAL header')
        contract=case/'child-contract.h'
        contract.write_text('#error Synthetic child contract is not bound yet\n')
        overlay=case/'overlay.json'
        overlay.write_text(json.dumps({'version':0,'roots':[
            {'type':'file','name':str(UNLOCK/'profile-local.h'),'external-contents':str(header),'use-external-name':False},
            {'type':'file','name':str(BOOT/'platform-config.h'),'external-contents':str(platform),'use-external-name':False},
            {'type':'file','name':str(BOOT/'build/headless/child-contract.h'),'external-contents':str(contract),'use-external-name':False}]}))
        shared=['-DGU_PROFILE_LOCAL','-DGU_HEADLESS_RESEARCH','-DGU_AUTOMATIC_BOOT','-DGU_USB_MEDIA']
        child,child_result=build(case,'child',[UNLOCK/(source+'.c') for source in child_sources],
            shared+['-Wno-unused-parameter','-D__KERNEL__','-DGU_UEFI_PORT',
                '-DGU_TARGET_PCI_ID=0x'+report['Replay']['RtxId']+'U','-DGU_EXPERIMENTAL_3090',
                '-DGU_ENTRY_LATCHED_SS1_FULL','-DGU_INTERNAL_BOOT','-I',str(UNLOCK/'compat')],overlay)
        contract.write_text('#define GU_UNLOCK_SHA256 "'+child_result['sha256']+'"\n#define GU_UNLOCK_BYTES '+str(child.stat().st_size)+'\n')
        _,wrapper_result=build(case,'wrapper',wrapper_sources,
            shared+['-DGU_SINGLE_RTX_CONSOLE','-DGU_FULL_UNLOCK'],overlay)
        summaries.append({'case':name,'header_sha256':report['HeaderSha256'],
            'child':child_result,'wrapper':wrapper_result})
        print(name+': child and wrapper PE verified; not executed',flush=True)
    (out/'summary.json').write_text(json.dumps({'synthetic_inputs':True,'hardware_access':False,
        'efi_executed':False,'local_evidence_written':False,'package_authorized':False,'cases':summaries},indent=2))


if __name__=='__main__':main()
