# SPDX-License-Identifier: GPL-2.0-only
"""Fault-seeded controls over the real Stage 1 code, using Clang VFS overlays."""
import json


def controls(root, pilot, out, flags, inputs, run):
    cases = [
        ('attribute-mask', [('stage1-policy.h', '(attributes==0x200ULL || attributes==0x300ULL)',
            '((attributes&0x300ULL)==0x200ULL || (attributes&0x300ULL)==0x300ULL)')], [],
            'FAIL missing decode, unexpected BME and extra IO remain rejected'),
        ('raw-io', [('stage1-policy.h', '(command&7U)==2U', '(command&6U)==2U'),
                    ('stage1-policy.h', '(Full.Config[1]&7U)!=2U', '(Full.Config[1]&6U)!=2U'),
                    ('stage1-policy.h', '(cfg[1]&7U)!=2U', '(cfg[1]&6U)!=2U')], [],
            'FAIL Stage 1 raw safe decoding before every simulated MMIO read'),
        ('physical-text', [('core.c', 'if(s==EFI_SUCCESS && text!=Splitter.Text)s=EFI_NOT_READY;', '')], [],
            'FAIL Stage 1 adversarial state or transport fault rejects'),
        ('graph', [('core.c', 'if(s==EFI_SUCCESS && !(ExpectedGraph', 'if(0 && s==EFI_SUCCESS && !(ExpectedGraph')], [],
            'FAIL false splitter success never reaches later stages'),
        ('attributes', [('core.c', 'if(s==EFI_SUCCESS && attrs!=expected)s=EFI_NOT_READY;', '(void)expected;')], [],
            'FAIL baseline verification identifies attribute drift without accepting it'),
        ('fresh-key', [('entry.c', "if(s==EFI_SUCCESS)s=WaitKey('K');", '')], [],
            'FAIL entry cancellation, timeout or transport failure halts'),
        ('general-authority', [('stage1-policy.h', 'Full.Captured || Full.Released || Full.Owned || Full.Restored || PreparedReady ||', '')], [],
            'FAIL Stage 1 adversarial state or transport fault rejects'),
        ('terminal-preflight', [('core.c', 'if(Splitter.Started || Splitter.Ended){Splitter.Ready=FALSE;Faulted=TRUE;return EFI_ACCESS_DENIED;}', '')], [],
            'FAIL same-boot preflight cannot renew authority'),
        ('prom-recheck', [('stage1-policy.h', 's=Stage1ReadSafe("prom-raw-safety");', 's=EFI_SUCCESS;')], ['raw'],
            'FAIL Stage 1 raw safe decoding before every simulated MMIO read'),
        ('failure-emission', [('entry.c', 'PilotReportFailure(Record,NULL,s);', '(void)0;')], ['evidence'],
            'FAIL failure field preserves exact observed value or validity'),
        ('raw-checkpoint', [('core.c', 'Stage1ReadSafe("verify-cold-raw-safety")', 'Stage1ReadSafe("verify-bar")')], ['raw'],
            'FAIL failure checkpoint accurately identifies rejected prerequisite'),
        ('first-failure', [('stage1-diagnostics.h', 'status==EFI_SUCCESS || Stage1Failure.Failed', 'status==EFI_SUCCESS')], ['evidence'],
            'FAIL failure checkpoint accurately identifies rejected prerequisite'),
        ('truncated-config', [('stage1-diagnostics.h', 'for(UINTN i=0;i<64;i++) {', 'for(UINTN i=0;i<1;i++) {')], ['evidence'],
            'FAIL complete PCI diagnostic contains all 64 words'),
        ('duplicate-config', [('stage1-diagnostics.h', 'label[20]="0123456789abcdef"[i&15];', 'label[20]=\'0\';')], ['evidence'],
            'FAIL PCI diagnostic indices are unique and bounded'),
        ('corrupt-config', [('stage1-diagnostics.h', 'log(ctx,label,d->Config[i])', 'log(ctx,label,d->Config[i]^1U)')], ['evidence'],
            'FAIL every PCI diagnostic word equals the independent expected observation'),
    ]
    for name, edits, selection, expected in cases:
        replacements = {}
        for filename, needle, replacement in edits:
            content = replacements.get(filename, (pilot / filename).read_text())
            if content.count(needle) != 1:
                raise RuntimeError(f'{name}: mutation target is ambiguous or absent')
            replacements[filename] = content.replace(needle, replacement)
        overlay_files = []
        for filename, content in replacements.items():
            changed = out / (name + '-' + filename)
            changed.write_text(content)
            overlay_files.append({'type': 'file', 'name': str(pilot / filename), 'external-contents': str(changed)})
        overlay = out / (name + '-overlay.json')
        overlay.write_text(json.dumps({'version': 0, 'roots': overlay_files}))
        binary = out / (name + '-control')
        run(flags + ['-ivfsoverlay', str(overlay)] + inputs + ['-o', str(binary)], out / (name + '-compile.log'))
        run([str(binary)] + selection, out / (name + '-run.log'), expected=1, contains=expected)
        print('PASS fault-seeded control: ' + name, flush=True)
    return [name for name, _, _, _ in cases]


def guards(root, pilot, out, run):
    check = out / 'guard.c'
    check.write_text('#include "research/msi-release-pilot/profile.h"\n')
    base = ['clang', '-fsyntax-only', '-I', str(root), '-I', str(out)]
    positive = ['-DGU_RELEASE_PILOT', '-DGU_PILOT_STAGE=1', '-DGU_PILOT_HOST_TEST', '-DGU_PILOT_SYNTHETIC']
    run(base + positive + [str(check)], out / 'guard-positive.log')
    cases = [
        ('missing-stage', ['-DGU_RELEASE_PILOT'], 'Explicit fixed Stage 1'),
        ('stage0', ['-DGU_RELEASE_PILOT','-DGU_PILOT_STAGE=0'], 'Explicit fixed Stage 1'),
        ('stage2', ['-DGU_RELEASE_PILOT','-DGU_PILOT_STAGE=2'], 'Explicit fixed Stage 1'),
        ('stage3', ['-DGU_RELEASE_PILOT','-DGU_PILOT_STAGE=3'], 'Explicit fixed Stage 1'),
        ('stage4', ['-DGU_RELEASE_PILOT','-DGU_PILOT_STAGE=4'], 'Explicit fixed Stage 1'),
        ('production', positive + ['-DGU_FULL_UNLOCK'], 'Pilot must not combine'),
        ('profile', positive + ['-DGU_PROFILE_MSI_X570_3080TI'], 'Pilot must not combine'),
        ('synthetic-without-host', ['-DGU_RELEASE_PILOT','-DGU_PILOT_STAGE=1','-DGU_PILOT_SYNTHETIC'], 'Synthetic identity is host-test-only'),
        ('synthetic-firmware', positive + ['-ffreestanding'], 'Synthetic identity is host-test-only'),
        ('host-halt-firmware', ['-DGU_RELEASE_PILOT','-DGU_PILOT_STAGE=1','-DGU_PILOT_HOST_TEST','-ffreestanding'], 'Host halt hook is forbidden'),
    ]
    for profile in ('GU_PROFILE_B650M_3090FE', 'GU_PROFILE_B660M_3070TI',
                    'GU_PROFILE_X870_3090TI', 'GU_PROFILE_B660PLUS_3060',
                    'GU_PROFILE_H610M_3060', 'GU_PROFILE_PROBE_H610M_3060',
                    'GU_PROFILE_LOCAL'):
        cases.append(('conflicting-' + profile.lower(), positive + ['-D' + profile],
                      'Pilot must not combine'))
    for name, definitions, expected in cases:
        run(base + definitions + [str(check)], out / ('guard-' + name + '.log'), expected=1, contains=expected)
    print(f'PASS build isolation: positive control and {len(cases)} rejections', flush=True)
    return [name for name, _, _ in cases]
