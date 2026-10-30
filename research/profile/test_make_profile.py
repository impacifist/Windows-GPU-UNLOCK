"""Refusal tests for make-profile.py, run on this machine's own probe captures.

Each case copies the captures to a temporary folder, changes one fact and
requires the generator to refuse. Offline only; writes nothing in the repo.

Usage: python -I test_make_profile.py CAPTURE_DIR FWSEC_LOCATE_EXE
"""
import importlib.util
import json
import re
import shutil
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('make_profile', HERE / 'make-profile.py')
mp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mp)
ENVELOPE = json.loads((HERE / 'proven.json').read_text())
failures = 0


def sub(path, pattern, replace):
    """Rewrite the first match of pattern in a capture; the case is void if it does not occur."""
    text = path.read_bytes().decode('utf-8-sig').replace('\r\n', '\n')
    new, n = re.subn(pattern, replace, text, count=1, flags=re.M)
    if n != 1:
        raise AssertionError(f'{path.name}: pattern {pattern!r} not found')
    path.write_bytes(new.encode())


def case(name, expect, mutate=None, envelope=None):
    global failures
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp) / 'capture'
        shutil.copytree(CAPTURES, work)
        diags = sorted(work.glob('DIAG[0-9][0-9].TXT'))
        if mutate:
            mutate(work, diags)
        try:
            mp.generate(work, LOCATOR, ENVELOPE if envelope is None else envelope)
            outcome = None
        except mp.Refused as error:
            outcome = str(error)
    ok = outcome is None if expect is None else outcome is not None and expect in outcome
    failures += not ok
    print(f'{"PASS" if ok else "FAIL"} {name}: {outcome or "accepted"}')


def hexword(value):
    return lambda m: m[1] + f'{value:08x}'


def rtx_gop(text):
    """The unique physical GOP; an aggregate may also record a zero framebuffer."""
    found = [m for m in re.finditer(r'^handle id=(0x[0-9a-f]+)\n(.*?)(?=^handle id=|^capture-complete=|\Z)', text, re.M | re.S)
             if any(int(base,16) for base in re.findall(r'^gop framebuffer=0x([0-9a-f]+)',m[2],re.M))]
    if len(found)!=1:
        raise AssertionError('Expected one physical GOP handle')
    return found[0]


def add_edid_open(path, attributes):
    """Add an active EdidDiscovered opener on the RTX GOP, opened by the driver that owns its TextOut."""
    text = path.read_bytes().decode('utf-8-sig').replace('\r\n', '\n')
    block = rtx_gop(text)
    handle = int(block[1], 16)
    edges=mp.parse_handles(text)[handle]['edges']
    agents = [a for p, a, c, at, n in edges if p == mp.TEXT_OUT and at == mp.BY_DRIVER]
    if not agents:
        agents=[a for p,a,c,at,n in edges if p==mp.GOP and at==mp.BY_DRIVER]
    line = 'protocol guid=' + mp.EDID_DISCOVERED + '\n'
    if len(agents) != 1 or line not in block[2]:
        raise AssertionError(f'{path.name}: RTX GOP has no unique console owner or no EdidDiscovered interface')
    at = block.start(2) + block[2].index(line) + len(line)
    header = re.match(r'opens status=0x0+ count=0x([0-9a-f]+)\n', text[at:])
    if not header:
        raise AssertionError('Missing complete EDID open snapshot')
    replacement = f'opens status=0x0000000000000000 count=0x{int(header[1],16)+1:016x}\n'
    replacement += f'open agent=0x{agents[0]:016x} controller=0x{handle:016x} attributes=0x{attributes:x} count=0x1\n'
    new = text[:at] + replacement + text[at + header.end():]
    path.write_bytes(new.encode())


def shift_bus(path):
    """Move both independently recorded target BDFs together for a synthetic test."""
    text = path.read_bytes().decode('utf-8-sig').replace('\r\n', '\n')
    target = re.search(r'^probe-target seg=0x[0-9a-f]+ bus=0x([0-9a-f]+)', text, re.M)
    old = int(target[1], 16)
    new = old + 1 if old < 255 else 254
    text = re.sub(r'^(probe-target seg=0x[0-9a-f]+ bus=0x)[0-9a-f]+', lambda m: m[1] + f'{new:x}', text, count=1, flags=re.M)
    blocks = list(re.finditer(r'^handle id=0x[0-9a-f]+\n(.*?)(?=^handle id=|^capture-complete=|\Z)', text, re.M | re.S))
    block = next(m for m in blocks if (c := mp.pci_config(m[1])) and c[0] & 0xffff == 0x10de and c[2] >> 24 == 3)
    changed, count = re.subn(r'^(pci location-status=0x[0-9a-f]+ segment=0x[0-9a-f]+ bus=0x)[0-9a-f]+',
                             lambda m: m[1] + f'{new:x}', block[1], count=1, flags=re.M)
    if count != 1:
        raise AssertionError('Missing target graph PCI location')
    path.write_text(text[:block.start(1)] + changed + text[block.end(1):], encoding='utf-8')


def shift_segment(path):
    """Move both independent RTX location observations to another segment."""
    text=path.read_bytes().decode('utf-8-sig').replace('\r\n','\n')
    target=re.search(r'^probe-target seg=0x([0-9a-f]+)',text,re.M)
    segment=int(target[1],16)^1
    text=re.sub(r'^(probe-target seg=0x)[0-9a-f]+',lambda m:m[1]+f'{segment:x}',text,count=1,flags=re.M)
    blocks=list(re.finditer(r'^handle id=0x[0-9a-f]+\n(.*?)(?=^handle id=|^capture-complete=|\Z)',text,re.M|re.S))
    block=next(m for m in blocks if (c:=mp.pci_config(m[1])) and c[0]&0xffff==0x10de and c[2]>>24==3)
    changed,count=re.subn(r'^(pci location-status=0x[0-9a-f]+ segment=0x)[0-9a-f]+',lambda m:m[1]+f'{segment:x}',block[1],count=1,flags=re.M)
    if count!=1:raise AssertionError('Missing target graph PCI segment')
    path.write_bytes((text[:block.start(1)]+changed+text[block.end(1):]).encode())


def duplicate_igpu(path):
    """Add an explicit second adapter, without assuming two spare PCI handles."""
    text=path.read_bytes().decode('utf-8-sig').replace('\r\n','\n')
    blocks=mp.parse_handles(text)
    source=next(handle for handle,block in blocks.items()
                if (config:=mp.pci_config(block['raw'])) and config[0]&0xffff!=0x10de and config[2]>>24==3)
    block=re.search(rf'^handle id=0x0*{source:x}\n.*?(?=^handle id=|^capture-complete=|\Z)',text,re.M|re.S)
    if not block:raise AssertionError('Missing integrated GPU block')
    added=re.sub(r'^handle id=0x[0-9a-f]+',f'handle id=0x{max(blocks)+1:x}',block[0],count=1)
    text=text.replace('capture-complete=1',added+'capture-complete=1',1)
    text,hits=re.subn(r'^(handles status=0x0+ count=0x)([0-9a-f]+)',
                     lambda m:m[1]+f'{int(m[2],16)+1:x}',text,count=1,flags=re.M)
    if hits!=1:raise AssertionError('Missing handle count')
    path.write_text(text,encoding='utf-8')


def damage_prom(path):
    data=bytearray(path.read_bytes())
    if not data:raise AssertionError('Empty baseline PROM')
    data[0]^=1
    path.write_bytes(data)


def main():
    global CAPTURES, LOCATOR
    CAPTURES, LOCATOR = Path(sys.argv[1]), Path(sys.argv[2])
    case('real captures accepted', None)
    case('single capture', 'at least', lambda w, d: [p.unlink() for p in d[:-1]])
    case('captures disagree', 'disagree on GU_PROM_SHA256',
         lambda w, d: sub(d[0], r'^(prom sha256=)[0-9a-f]{64}', lambda m: m[1] + '0' * 64))
    case('unproven chip', 'no hardware-proven profile',
         lambda w, d: sub(d[-1], r'^(mmio off=0x00000000 status=0x0+ value=0x)[0-9a-f]{8}', hexword(0xdead00a1)))
    case('unproven memory size', 'not hardware-proven',
         lambda w, d: sub(d[-1], r'^(mmio off=0x001183a4 status=0x0+ value=0x)[0-9a-f]{8}', hexword(0x2800)))
    case('already unlocked', 'power-on state',
         lambda w, d: sub(d[-1], r'^(mmio off=0x0082381c status=0x0+ value=0x)[0-9a-f]{8}', hexword(0x88888888)))
    case('WPR2 already set', 'power-on state',
         lambda w, d: sub(d[-1], r'^(mmio off=0x001fa828 status=0x0+ value=0x)[0-9a-f]{8}', hexword(0x10)))
    case('second NVIDIA GPU', 'exactly one NVIDIA', lambda w, d: sub(d[-1], r'^probe targets=0001(?: cfg-incomplete=0000)?$', 'probe targets=0002 cfg-incomplete=0000'))
    case('RTX on bus 0', 'nonzero bus',
         lambda w, d: [sub(p, r'^(probe-target seg=0x[0-9a-f]+ bus=0x)[0-9a-f]+', lambda m: m[1] + '0') for p in d])
    case('RTX segment overflow', '16-bit PCI segment',
         lambda w, d: [sub(p, r'^(probe-target seg=0x)[0-9a-f]+', lambda m: m[1] + '10000') for p in d])
    case('captures disagree on segment', 'disagree on GU_TARGET_SEGMENT',lambda w,d:shift_segment(d[-1]))
    case('RTX on another segment in every capture', None,lambda w,d:[shift_segment(p) for p in d])
    case('probe/graph disagree on segment', 'PCI location',
         lambda w,d:sub(d[-1],r'^(probe-target seg=0x)([0-9a-f]+)',lambda m:m[1]+f'{int(m[2],16)^1:x}'))
    case('RTX at device 1', 'device 0, function 0',
         lambda w, d: [sub(p, r'^(probe-target seg=0x[0-9a-f]+ bus=0x[0-9a-f]+ dev=0x)[0-9a-f]+', lambda m: m[1] + '1') for p in d])
    # The bus is a per-machine constant: moving it in every capture is a different, still valid
    # machine; moving it in one capture only is a capture that does not describe the same boot.
    case('captures disagree on bus', 'disagree on GU_TARGET_BUS',
         lambda w, d: shift_bus(d[-1]))
    case('RTX on another bus in every capture', None,
         lambda w, d: [shift_bus(p) for p in d])
    try:
        info = mp.analyze(sorted(CAPTURES.glob('DIAG[0-9][0-9].TXT'))[-1], None, ENVELOPE)
    except (mp.Refused, IndexError):
        info = None  # the first case already reports captures the generator cannot accept
    # An installed iGPU is optional: the single-RTX console path never uses it. Taking it away, or
    # adding one, in every capture must still yield a profile; changing it in one capture must not.
    # Two or more non-NVIDIA display controllers are unreviewed.
    if info and info['igpu_id']:
        remove_igpu = lambda p: sub(p, r'^(pci config-dwords=(?!....10de)[0-9a-f]{8} [0-9a-f]{8} )03', lambda m: m[1] + '04')
        case('no integrated GPU in any capture', None, lambda w, d: [remove_igpu(p) for p in d])
        case('integrated GPU in only one capture', 'disagree on the integrated GPU', lambda w, d: remove_igpu(d[-1]))
    elif info:
        add_igpu = lambda p: sub(p, r'^(pci config-dwords=(?!....10de)[0-9a-f]{8} [0-9a-f]{8} )(?!03)[0-9a-f]{2}', lambda m: m[1] + '03')
        case('an integrated GPU appears in every capture', None, lambda w, d: [add_igpu(p) for p in d])
        case('integrated GPU in only one capture', 'disagree on the integrated GPU', lambda w, d: add_igpu(d[-1]))
        case('two integrated GPUs', 'at most one non-NVIDIA display controller', lambda w, d: [(add_igpu(p), duplicate_igpu(p)) for p in d])
    # The EDID driver is optional, but a driver other than the reviewed AMI one may never own the
    # EdidDiscovered interface of the RTX GOP, whether or not this machine has one.
    edid_refusal=('BY_DRIVER opener of '+mp.EDID_DISCOVERED) if info and info['aggregate'] else 'edid:'
    if info and info['no_edid_driver'] and info['child_count']>2:
        # An EDID owner also removes this topology's permission for 3/4 children,
        # so the exact topology guard rejects before driver fingerprint parsing.
        edid_refusal='console topology requires 1..2 unique NVIDIA-owned display children'
    case('unreviewed driver owns the EDID interface', edid_refusal,
         lambda w, d: [add_edid_open(p, mp.BY_DRIVER) for p in d])
    if info and info['no_edid_driver']:
        # No EDID driver here: any other live opener of EdidDiscovered is unreviewed too.
        case('live EDID opener on a machine without an EDID driver', 'active EdidDiscovered opener',
             lambda w, d: [add_edid_open(p, mp.BY_CHILD_CONTROLLER) for p in d])
    # full-release.c CapturePermit compares the PciIo attribute cache with the command register.
    attributes = r'^(pci-attributes status=0x)([0-9a-f]{16})( value=0x)([0-9a-f]{16})$'
    case('no PciIo attribute record', 'no PciIo attribute record',
         lambda w, d: sub(d[-1], attributes + r'\n', ''))
    case('PciIo attribute read failed', 'reading the PciIo attributes failed',
         lambda w, d: sub(d[-1], attributes, lambda m: m[1] + '8000000000000003' + m[3] + m[4]))
    case('PciIo attributes report bus mastering', 'disagree with the PCI command register',
         lambda w, d: sub(d[-1], attributes, lambda m: m[1] + m[2] + m[3] + f'{int(m[4], 16) | 0x400:016x}'))
    case('PciIo attributes lack memory decode', 'disagree with the PCI command register',
         lambda w, d: sub(d[-1], attributes, lambda m: m[1] + m[2] + m[3] + f'{int(m[4], 16) & ~0x200:016x}'))
    if info and info['aggregate']:
        case('unresolved aggregate console', 'console handles',
             lambda w,d: sub(d[-1],r'^(console out=)0x[0-9a-f]+',lambda m:m[1]+'0x0'))
    else:
        case('unreviewed ConOut', 'ConOut',
             lambda w, d: sub(d[-1], r'^(variable name=ConOut .*\n(?:.*\n)*?path-node raw=0101060000)00', lambda m: m[1] + '1f'))
    case('incomplete capture', 'complete, successful',
         lambda w, d: sub(d[-1], r'^diagnostic-result status=0x0+$', 'diagnostic-result status=0x8000000000000006'))
    case('PROM.BIN from another card', 'PROM.BIN is not',
         lambda w, d: damage_prom(w / 'PROM.BIN'))
    case('nvgop never released on proven hardware', 'has not released cleanly',
         envelope={**ENVELOPE, 'nvgop': []})
    case('chip family without proof', 'no hardware-proven profile', envelope={**ENVELOPE, 'chips': []})
    print(f'{failures} generator refusal case(s) failed' if failures else 'All generator refusal cases passed')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
