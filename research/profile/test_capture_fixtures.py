"""Host-only capture-fixture portability checks; no original PROM/assets or EFI execution."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
UNLOCK=ROOT/'internal-boot/unlock'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang',default='clang')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--mutations',action='store_true')
    parser.add_argument('--local-profile',type=Path,help='Optional generated header with adjacent profile.json; compiler overlay only')
    args=parser.parse_args();out=args.output.resolve()
    if out.exists():parser.error('Output must be a new directory')
    out.mkdir(parents=True)
    flags=[args.clang,'-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
           '-Wno-unused-function','-D_CRT_SECURE_NO_WARNINGS','-I',str(UNLOCK/'compat'),
           '-I',str(ROOT/'deps/edk2/MdePkg/Include'),'-I',str(ROOT/'deps/edk2/MdePkg/Include/X64')]
    core=[UNLOCK/name for name in ('port.c','compat.c','vendor/unlock_610.c','vendor/prep_610.c',
        'vendor/layout_610.c','vendor/vbios_610.c','vendor/gfw_eval.c')]
    profiles=('B650M_3090FE','B660M_3070TI','B660M_3090TI','X870_3090TI','B660PLUS_3060','H610M_3060')
    variants=(('default-host',('-UGU_INTERNAL_BOOT',)),('manual',()),('automatic',('-DGU_AUTOMATIC_BOOT',)),
              ('headless',('-DGU_AUTOMATIC_BOOT','-DGU_HEADLESS_RESEARCH')),
              ('usb',('-DGU_AUTOMATIC_BOOT','-DGU_HEADLESS_RESEARCH','-DGU_USB_MEDIA')))
    results=[]
    def run(log,command,expected=0):
        done=subprocess.run([str(x) for x in command],cwd=ROOT,capture_output=True,text=True,timeout=60)
        log.write_text(done.stdout+done.stderr)
        if done.returncode!=expected:raise RuntimeError(str(log)+': '+(done.stdout+done.stderr)[-2500:])
        return done.stdout.strip()
    def fixture(folder,profile,extra=(),overlay=()):
        exe=folder/'fixture.exe'
        # Constructor/port tests only. Do not select a firmware transaction or
        # invent missing per-profile FWSEC pins to compile the unused vendor test.
        run(folder/'compile.log',[*flags,'-D__KERNEL__','-DGU_UEFI_PORT','-DGU_HOST_WRITE_TEST',
            '-DGU_INTERNAL_BOOT','-DGU_PROFILE_'+profile,*extra,*overlay,
            UNLOCK/'tests/test_capture_fixture.c',UNLOCK/'checks.c',*core,'-o',exe])
        return exe
    for profile in profiles:
        for mode,extra in variants:
            folder=out/(profile+'-'+mode);folder.mkdir()
            exe=fixture(folder,profile,extra)
            result=run(folder/'run.log',[exe]);print(profile+' '+mode+': '+result,flush=True)
            results.append({'profile':profile,'variant':mode,'result':result})
    if args.local_profile:
        header=args.local_profile.resolve()
        report=json.loads(header.with_name('profile.json').read_text())
        digest=hashlib.sha256(header.read_bytes()).hexdigest()
        if report['HeaderSha256']!=digest or report['HardwareTested'] is not False:
            raise RuntimeError('Expected unchanged synthetic generated LOCAL header')
        path=out/'generated-local.json';path.write_text(json.dumps({'version':0,'roots':[
            {'type':'file','name':str(UNLOCK/'profile-local.h'),'external-contents':str(header),'use-external-name':False}]}))
        for mode,extra in variants:
            folder=out/('LOCAL-'+mode);folder.mkdir()
            exe=fixture(folder,'LOCAL',extra,['-ivfsoverlay',str(path)])
            result=run(folder/'run.log',[exe]);print('LOCAL '+mode+': '+result,flush=True)
            results.append({'profile':'LOCAL','header_sha256':digest,'variant':mode,'result':result})
    # Synthetic LOCAL pin values are compiler overlays, never installed profiles.
    data=b'host-only SHA fixture\x00\xff\x10'
    good=out/'matching.bin';good.write_bytes(data)
    bad=out/'changed.bin';bad.write_bytes(data[:-1]+bytes([data[-1]^1]))
    local=out/'profile-local.h'
    local.write_text('#define GU_LOCAL_PROFILE_GENERATOR 1\n#define GU_CHIP_ID 0xb74000a1U\n'
        '#define GU_BOARD_ID 0x24c910deU\n#define GU_DEVICE_MEMORY_SIZE 0x2000U\n'
        '#define GU_PROM_SHA256 "'+hashlib.sha256(data).hexdigest()+'"\n')
    def overlay(folder,name,source,contents):
        replacement=folder/(name+'.h');replacement.write_text(contents)
        path=folder/(name+'.json');path.write_text(json.dumps({'version':0,'roots':[
            {'type':'file','name':str(source),'external-contents':str(replacement),'use-external-name':False}]}))
        return ['-ivfsoverlay',str(path)]
    local_flags=overlay(out,'local',UNLOCK/'profile-local.h',local.read_text())
    sha=out/'sha.exe'
    run(out/'sha-compile.log',[*flags,'-DGU_PROFILE_LOCAL',*local_flags,
        UNLOCK/'tests/test_sha256.c',UNLOCK/'sha256.c','-o',sha])
    run(out/'sha-match.log',[sha,good])
    run(out/'sha-changed.log',[sha,bad],1)
    foreign=overlay(out,'foreign',UNLOCK/'profile-local.h',local.read_text().replace(hashlib.sha256(data).hexdigest(),'0'*64))
    foreign_exe=out/'sha-foreign.exe'
    run(out/'sha-foreign-compile.log',[*flags,'-DGU_PROFILE_LOCAL',*foreign,
        UNLOCK/'tests/test_sha256.c',UNLOCK/'sha256.c','-o',foreign_exe])
    run(out/'sha-foreign.log',[foreign_exe,good],1)
    mutations=[]
    if args.mutations:
        app=(UNLOCK/'tests/test_app.c').read_text()
        before=('  fake.pci_attributes=(config[4]&1?EFI_PCI_IO_ATTRIBUTE_IO:0)|\n'
                '    (config[4]&2?EFI_PCI_IO_ATTRIBUTE_MEMORY:0)|(config[4]&4?EFI_PCI_IO_ATTRIBUTE_BUS_MASTER:0);\n')
        # The disconnect fixture also has this assignment. Remove only setup's copy.
        at=app.index('static void app_setup(')
        if before not in app[at:]:raise RuntimeError('Setup mutation no longer matches')
        broken=app[:at]+app[at:].replace(before,'',1)
        folder=out/'stale-cache-mutant';folder.mkdir()
        replacement=overlay(folder,'stale',UNLOCK/'tests/test_app.c',broken)
        exe=fixture(folder,profiles[0],overlay=replacement)
        run(folder/'run.log',[exe],1);mutations.append('stale-capture-cache')
        folder=out/'fixed-prom-mutant';folder.mkdir()
        source=(UNLOCK/'tests/test_sha256.c').read_text()
        if 'GU_PROM_SHA256' not in source:raise RuntimeError('SHA mutation no longer matches')
        broken=source.replace('GU_PROM_SHA256','"8ea297373e9e5d3600ff41ad9fae88f2d14c98c017d100669f7333981da673b4"')
        replacement=overlay(folder,'fixed',UNLOCK/'tests/test_sha256.c',broken)
        exe=folder/'sha.exe'
        run(folder/'compile.log',[*flags,'-DGU_PROFILE_LOCAL',*local_flags,*replacement,
            UNLOCK/'tests/test_sha256.c',UNLOCK/'sha256.c','-o',exe])
        run(folder/'run.log',[exe,good],1);mutations.append('hard-coded-prom')
    (out/'summary.json').write_text(json.dumps({'synthetic_only':True,'hardware_access':False,
        'efi_executed':False,'vendor_transaction_executed':False,'local_evidence_written':False,
        'fixture_jobs':results,'sha_match':True,'changed_sha_refused':True,'foreign_sha_refused':True,
        'compiled_mutations_detected':mutations},indent=2))
    print('PASS capture fixture matrix and selected-profile SHA checks; no hardware qualification')


if __name__=='__main__':main()
