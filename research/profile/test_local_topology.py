"""Capture-free admission regressions; generated text is synthetic, never hardware evidence."""
import hashlib
import importlib.util
from pathlib import Path
import re
import subprocess
import struct
import sys
import tempfile
import unittest
import uuid

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('local_topology_profile', HERE / 'make-profile.py')
mp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mp)
PROM_HASH = hashlib.sha256(bytes(1048576)).hexdigest()
END = bytes.fromhex('7fff0400')
PCI_PATH = bytes.fromhex('02010c00d041030a00000000010106000103010106000000')
RTX_PATH = PCI_PATH + bytes.fromhex('0203080070030180') + END
ENVELOPE = {'probe_device_ids': ['220410de'], 'class_revision': '030000a1',
            'chips': [{'chip': 'b72000a1', 'family': 'GA102', 'memory': ['6000', '5ff4'],
                       'pci_memory': {'220410de': ['6000', '5ff4']},
                       'nvgop': [{'version': '60009', 'text_sha256': 'd045cee2113e461c7c678ec21542b1d0f18356765c0fc6a0bf2dcf824088e011'}]}]}


def configuration(command=3, board=0x220410de):
    cfg = [0] * 64
    for at, value in {0: board, 1: 0x00100000 | command, 2: 0x030000a1,
                      4: 0xf5000000, 11: 0x38811462, 13: 0x40,
                      16: 0x00014801, 18: 0x00020010, 19: 1 << 28}.items():
        cfg[at] = value
    return cfg


def synthetic_capture(*, bus=0x2b, igpu=False, no_edid=True, children=2, command=3, memory=0x6000, child_adr=0x20,
                      board=0x220410de, chip=0xb72000a1, aggregate=False):
    cfg = configuration(command, board)
    handles = {i: {} for i in (1, 2, 3, 4, 5, 6, 8, 10, 11)}
    siblings=[7]+list(range(20,20+max(0,children-2))) if children>1 else []
    for child in siblings:
        handles[child] = {}
    if not no_edid:
        handles[9] = {}
    def protocol(h, p, opens=(), detail=''):
        handles[h][p] = (list(opens), detail)
    def path(value):
        rows, at = [], 0
        while at < len(value):
            length = int.from_bytes(value[at + 2:at + 4], 'little')
            assert length >= 4 and at + length <= len(value)
            rows.append('path-node raw=' + value[at:at + length].hex())
            at += length
        return '\n'.join(rows) + '\n'
    protocol(1, mp.PCI_IO, [(3, 1, 16, 1)],
             f'pci location-status=0x0 segment=0x0 bus=0x{bus:x} device=0x0 function=0x0\n'
             'pci config-status=0x0000000000000000\npci config-dwords=' + ' '.join(f'{v:08x}' for v in cfg[:16]) + '\n')
    child_edges = ([(3, 2, 8, 1)] if children else []) + [(3, child, 8, 1) for child in siblings]
    protocol(1, mp.PATH, child_edges, path(PCI_PATH + END))
    protocol(2, mp.PATH, detail=path(RTX_PATH))
    protocol(2, mp.GOP, [(5, 2, 16, 1)],
             'gop framebuffer=0x000000fcc0000000 bytes=0x00000000007e9000 mode=0x0 max-mode=0x9\n'
             'gop width=0x780 height=0x438 stride=0x780\ngop pixel-format=0x0 info-bytes=0x24\n')
    protocol(2, mp.TEXT_OUT, [(4, 2, 16, 1), (4, 6, 8, 1)])
    protocol(2, mp.EDID_DISCOVERED, [(3, 2, 2, 1)] if no_edid else [(9, 2, 16, 1)])
    for index, child in enumerate(siblings):
        second = bytearray(RTX_PATH); second[28] = child_adr+index
        protocol(child, mp.PATH, detail=path(second))
        protocol(child, mp.EDID_DISCOVERED, [] if no_edid else [(9, child, 16, 1)])
    for h, image in ((3, 3), (4, 10), (5, 11)):
        protocol(h, '18a031ab-b443-4d1a-a5c0-0c09261e9f71', detail=f'driver image=0x{image:x} binding=0x{h:x}\n')
    protocol(6, '387477c1-69c7-11d2-8e39-00a0c969723b')
    protocol(6, mp.TEXT_OUT)
    other = [0x164e1002, 2, 0x030000a1 if igpu else 0x060000a1] + [0] * 13
    protocol(8, mp.PCI_IO, detail='pci config-status=0x0000000000000000\npci config-dwords=' + ' '.join(f'{v:08x}' for v in other) + '\n')
    protocol(8, '12345678-0000-0000-0000-000000000000', [(10, 8, 16, 1)])  # Protected unrelated ownership.
    for h in (10, 11):
        protocol(h, '5b1b31a1-9562-11d2-8e3f-00a0c969723b')
    if not no_edid:
        protocol(9, '18a031ab-b443-4d1a-a5c0-0c09261e9f71', detail='driver image=0x9 binding=0x9\n')
    if aggregate:
        handles[12]={};handles[13]={};handles[14]={}
        del handles[2][mp.TEXT_OUT]
        protocol(2,mp.GOP,[(13,2,16,1),(13,12,8,1)],
                 'gop framebuffer=0x000000fcc0000000 bytes=0x00000000007e9000 mode=0x0 max-mode=0x9\n'
                 'gop width=0x780 height=0x438 stride=0x780\ngop pixel-format=0x0 info-bytes=0x24\n')
        protocol(12,mp.GOP,[(5,12,16,1)],'gop framebuffer=0x0 bytes=0x0 mode=0x0 max-mode=0x1\n')
        protocol(12,mp.TEXT_OUT,[(4,12,16,1),(4,6,8,1)])
        protocol(13,'18a031ab-b443-4d1a-a5c0-0c09261e9f71',detail='driver image=0xe binding=0xd\n')
        protocol(14,'5b1b31a1-9562-11d2-8e3f-00a0c969723b')
    lines = [mp._graph.MARKER, 'console out=0x6 error=0x6 input=0x6',
             'variable name=ConOut initial-status=0x8000000000000005 bytes=0x24', path(RTX_PATH).rstrip(),
             'variable name=ConOutDev initial-status=0x800000000000000e bytes=0x0',
             f'probe-target seg=0x0000 bus=0x{bus:04x} dev=0x0000 fun=0x0000',
             'probe targets=0001 cfg-incomplete=0000', 'cfg dwords=' + ' '.join(f'{v:08x}' for v in cfg),
             f'pci-attributes status=0x0000000000000000 value=0x{0x200 | (0x100 if command & 1 else 0):016x}',
             'prom sha256=' + PROM_HASH]
    if aggregate:
        resource=struct.pack('<BHBBBQQQQQBB',0x8a,43,0,0,0,32,cfg[4],0xffffff,0,0x1000000,0x79,0)
        lines.append('pci-bar0-resource raw='+resource.hex())
    cold = [chip, 0xffffff8f, 0x8b8f, 0x3ff, 0x10, 0, 0x03000062, 6, memory]
    lines += [f'mmio off=0x{at:08x} status=0x0000000000000000 value=0x{value:08x}' for at, value in zip(mp.COLD, cold)]
    nv_size,nv_stop,nv_version=(0x26ae0,0x678,0x60010) if board in (0x220310de,0x248710de,0x250410de,0x220a10de) else (0x2be40,0x698,0x60009)
    if board==0x24c910de:nv_size,nv_stop,nv_version=0x26e00,0x678,0x60015
    out_size,out_stop,gfx_size,gfx_stop=(0x6a40,0x104c,0x3bc0,0xb2c) if aggregate else (0x60c0,0x1290,0x39e0,0xa3c)
    bindings = [(4, 10, 0x400000, out_size, out_stop, 0x10, 3, mp.OUT_FILE),
                (5, 11, 0x500000, gfx_size, gfx_stop, 0x10, 3, mp.GFX_FILE),
                (3, 3, 0x100000, nv_size, nv_stop, nv_version, 3, None)]
    if not no_edid:
        bindings.append((9, 9, 0x600000, 0x9000, 0x14d4 if aggregate else 0x157c, 1, 5, mp.MOUSE_FILE))
    if aggregate:
        bindings.append((13,14,0x700000,0x2160,0x88c,0x11,3,mp.COMBO_FILE))
    for agent, image, base, size, stop, version, code_type, guid in bindings:
        node = bytes.fromhex('04061400') + uuid.UUID(guid).bytes_le if guid else bytes.fromhex('04041c00') + 'nvgop-ga1xx\0'.encode('utf-16-le')
        device=1 if agent==3 else 0
        protocol(image,'5b1b31a1-9562-11d2-8e3f-00a0c969723b',detail=
                 f'image parent=0x0 device=0x{device:x} base=0x{base:x} size=0x{size:x} code-type=0x{code_type:x}\n'
                 'image-file-path-node raw='+node.hex()+'\nimage-file-path-node raw='+END.hex()+'\n')
        lines += [f'binding handle=0x{agent:x} image=0x{image:x} version=0x{version:x} stop=0x{base + stop:x}',
                  f'img base=0x{base:x}', f'img size=0x{size:x}', f'img code-type=0x{code_type:x}',
                  'img-path-node raw=' + node.hex(), 'img-path-node raw=' + END.hex()]
    lines.append(f'handles status=0x0000000000000000 count=0x{len(handles):x}')
    for h, protocols in sorted(handles.items()):
        lines.append(f'handle id=0x{h:x}')
        for guid, (opens, detail) in protocols.items():
            lines += ['protocol guid=' + guid, f'opens status=0x0000000000000000 count=0x{len(opens):x}']
            lines += [f'open agent=0x{a:x} controller=0x{c:x} attributes=0x{attrs:x} count=0x{n:x}' for a, c, attrs, n in opens]
            if detail:
                lines.append(detail.rstrip())
    lines += ['capture-complete=1', 'diagnostic-result status=0x0000000000000000']
    return '\n'.join(lines) + '\n'


def analyze(text, envelope=ENVELOPE):
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder) / 'DIAG00.TXT'
        path.write_text(text, encoding='ascii')
        return mp.analyze(path, None, envelope)


def add_open(text, handle, protocol, row):
    """Add a recorded opener while keeping the declared row count consistent."""
    start = text.index(f'handle id=0x{handle:x}\n')
    end = text.find('\nhandle id=', start + 1)
    if end < 0:
        end = text.index('\ncapture-complete=', start)
    end += 1  # Include the final newline, also for the last handle in a capture.
    block = text[start:end]
    pattern = r'(protocol guid=' + re.escape(protocol) + r'\nopens status=0x0000000000000000 count=0x)([0-9a-f]+)(\n)'
    changed, count = re.subn(pattern, lambda m: m[1] + f'{int(m[2],16)+1:x}' + m[3] + row + '\n', block, count=1)
    assert count == 1
    return text[:start] + changed + text[end:]


class LocalTopologyTests(unittest.TestCase):
    def test_no_edid_child_capacity_matches_the_runtime(self):
        runtime=(HERE.parents[1]/'internal-boot/boot/full-release.c').read_text()
        values=re.findall(r'^#define GU_HEADLESS_MAX_DISPLAY_CHILDREN ([0-9]+)$',runtime,re.M)
        self.assertEqual(values,[str(mp.MAX_NO_EDID_CHILDREN)])
        for children in (3,4):
            self.assertEqual(analyze(synthetic_capture(children=children))['child_count'],children)
            for aggregate,no_edid in ((False,False),(True,False),(True,True)):
                with self.subTest(children=children,aggregate=aggregate,no_edid=no_edid),self.assertRaises(mp.Refused):
                    analyze(synthetic_capture(children=children,aggregate=aggregate,no_edid=no_edid))
        for children in (0,5):
            with self.subTest(children=children),self.assertRaisesRegex(mp.Refused,'1..4 unique'):
                analyze(synthetic_capture(children=children))

    def test_last_child_remains_part_of_admission(self):
        for children in (3,4):
            text=synthetic_capture(children=children)
            last=20+children-3
            block=re.search(rf'(^handle id=0x{last:x}\n)(.*?)(?=^handle id=|^capture-complete=)',text,re.M|re.S)
            self.assertIsNotNone(block)
            for changed in (
                    block[2].replace('path-node raw=010106000103','path-node raw=010106000104'),
                    re.sub(r'^path-node raw=02030800[0-9a-f]{8}$','path-node raw=0203080070030180',block[2],flags=re.M),
                    block[2].replace('protocol guid='+mp.EDID_DISCOVERED+'\nopens status=0x0000000000000000 count=0x0\n','')):
                self.assertNotEqual(changed,block[2])
                with self.assertRaises(mp.Refused):analyze(text[:block.start(2)]+changed+text[block.end(2):])
            for old,new in ((f'open agent=0x3 controller=0x{last:x} attributes=0x8 count=0x1',
                             f'open agent=0x4 controller=0x{last:x} attributes=0x8 count=0x1'),
                            (f'controller=0x{last:x} attributes=0x8 count=0x1',
                             'controller=0x7 attributes=0x8 count=0x1')):
                self.assertIn(old,text)
                with self.assertRaises(mp.Refused):analyze(text.replace(old,new))
            with self.assertRaisesRegex(mp.Refused,'active EdidDiscovered opener'):
                analyze(add_open(text,last,mp.EDID_DISCOVERED,f'open agent=0x4 controller=0x{last:x} attributes=0x10 count=0x1'))

    def test_valid_local_matrix(self):
        for bus in (1, 0x2b, 255):
            for igpu in (False, True):
                for no_edid in (False, True):
                    for children in (1, 2):
                        for command in (2, 3):
                            for memory in (0x6000, 0x5ff4):
                                with self.subTest(bus=bus, igpu=igpu, no_edid=no_edid, children=children, command=command, memory=memory):
                                    result = analyze(synthetic_capture(bus=bus, igpu=igpu, no_edid=no_edid, children=children, command=command, memory=memory))
                                    self.assertEqual(result['no_edid_driver'], no_edid)
                                    self.assertEqual(result['igpu_id'] is not None, igpu)
                                    self.assertEqual(dict(result['defines'])['GU_TARGET_BUS'], f'0x{bus:x}U')
                                    self.assertEqual(dict(result['defines'])['GU_LOCAL_CHILD_COUNT'], str(children))
                                    self.assertIs(result['aggregate'], False)
                                    self.assertEqual(result['child_count'], children)
                                    self.assertEqual(len(result['child_paths']), children)

    def test_graph_damage_is_not_absence(self):
        good = synthetic_capture()
        cases = [good.replace('count=0x1\nopen agent=0x3 controller=0x1', 'count=0x2\nopen agent=0x3 controller=0x1', 1),
                 good.replace('open agent=0x3 controller=0x2 attributes=0x8 count=0x1', 'open agent=0x3 controller=0x2 attributes=0x8 count=0x0'),
                 good.replace('open agent=0x3 controller=0x2 attributes=0x8 count=0x1', 'open agent=0x3 controller=0x2 attributes=0x8 count=0x2'),
                 good.replace('open agent=0x3 controller=0x2 attributes=0x8 count=0x1', 'open agent=0x4 controller=0x2 attributes=0x8 count=0x1'),
                 good.replace('open agent=0x3 controller=0x7 attributes=0x8 count=0x1', 'open agent=0x3 controller=0xff attributes=0x8 count=0x1'),
                 good.replace('open agent=0x3 controller=0x2 attributes=0x2 count=0x1', 'open agent=0x9 controller=0x2 attributes=0x10 count=0x1 trailing'),
                 good.replace('opens status=0x0000000000000000', 'opens status=0x8000000000000007', 1),
                 good.replace('capture-complete=1', 'capture-complete=1\ncapture-complete=0'),
                 good.replace('handle id=0xb', 'handle id=0xa'),
                 re.sub(r'handle id=0x7\n.*?(?=handle id=)', '', good, count=1, flags=re.S)]
        for text in cases:
            with self.subTest(text=text[-150:]), self.assertRaises(mp.Refused):
                analyze(text)

    def test_bus_boundaries(self):
        for bus in (0, 256):
            with self.subTest(bus=bus), self.assertRaisesRegex(mp.Refused, 'nonzero bus'):
                analyze(synthetic_capture(bus=bus))
        text = synthetic_capture(bus=0x2b).replace('pci location-status=0x0 segment=0x0 bus=0x2b',
                                                  'pci location-status=0x0 segment=0x0 bus=0x1')
        with self.assertRaisesRegex(mp.Refused, 'RTX PCI location'):
            analyze(text)

    def test_diagnostic_zero_extended_fields(self):
        text = synthetic_capture()
        text = re.sub(r'open agent=0x([0-9a-f]+) controller=0x([0-9a-f]+) attributes=0x([0-9a-f]+) count=0x([0-9a-f]+)',
                      lambda m: 'open agent=0x{:016x} controller=0x{:016x} attributes=0x{:016x} count=0x{:016x}'.format(*(int(v,16) for v in m.groups())), text)
        self.assertTrue(analyze(text)['no_edid_driver'])
        for field in ('attributes', 'count'):
            changed = re.sub(field + r'=0x[0-9a-f]+', field + '=0x0000000100000000', text, count=1)
            with self.subTest(field=field), self.assertRaises(mp.Refused):
                analyze(changed)

    def test_missing_pci_snapshot_is_not_no_igpu(self):
        text = synthetic_capture(igpu=True)
        start = text.index('handle id=0x8\n')
        end = text.index('handle id=0xa\n', start)
        block = re.sub(r'^pci config-(?:status|dwords)=.*\n', '', text[start:end], flags=re.M)
        with self.assertRaisesRegex(mp.Refused, 'Incomplete graph PCI configuration'):
            analyze(text[:start] + block + text[end:])

    def test_competing_owner_is_not_filtered_by_count(self):
        for handle, protocol in ((2, mp.TEXT_OUT), (2, mp.GOP), (1, mp.PCI_IO)):
            text = add_open(synthetic_capture(), handle, protocol,
                            f'open agent=0xa controller=0x{handle:x} attributes=0x10 count=0x2')
            with self.subTest(protocol=protocol), self.assertRaisesRegex(mp.Refused, 'expected one count-1 BY_DRIVER'):
                analyze(text)

    def test_malformed_reserved_records_are_not_ignored(self):
        text = synthetic_capture()
        marker = 'protocol guid=' + mp.EDID_DISCOVERED + '\nopens status=0x0000000000000000 count=0x0\n'
        for bad in ('open\tagent=0x9 controller=0x7 attributes=0x10 count=0x1', 'open',
                    'protocol\tguid=' + mp.EDID_DISCOVERED):
            damaged = text.replace(marker, marker + bad + '\n', 1)
            self.assertNotEqual(damaged, text)
            with self.subTest(bad=bad), self.assertRaises(mp.Refused):
                analyze(damaged)

    def test_no_edid_children_have_complete_distinct_paths(self):
        with self.assertRaisesRegex(mp.Refused, 'unique complete paths'):
            analyze(synthetic_capture(child_adr=RTX_PATH[28]))
        text = synthetic_capture()
        start = text.index('handle id=0x7\n')
        end = text.index('handle id=0x8\n', start)
        block = text[start:end]
        for bad in (re.sub(r'^path-node.*\n', '', block, flags=re.M),
                    block.replace('path-node raw=010106000103', 'path-node raw=010106000104')):
            self.assertNotEqual(bad, block)
            with self.assertRaisesRegex(mp.Refused, 'unique complete paths'):
                analyze(text[:start] + bad + text[end:])

    def test_edid_driver_children_also_require_complete_distinct_paths(self):
        with self.assertRaisesRegex(mp.Refused, 'unique complete paths'):
            analyze(synthetic_capture(no_edid=False, child_adr=RTX_PATH[28]))
        for old, new in (('controller=0x7 attributes=0x8 count=0x1', 'controller=0x7 attributes=0x8 count=0x2'),
                         ('controller=0x7 attributes=0x8 count=0x1', 'controller=0xff attributes=0x8 count=0x1')):
            text = synthetic_capture(no_edid=False)
            self.assertIn(old, text)
            with self.assertRaises(mp.Refused):
                analyze(text.replace(old, new))

    def test_fixture_conversion_matrix(self):
        script = HERE.parents[1] / 'internal-boot/boot/make-console-fixture.py'
        for igpu in (False, True):
            for no_edid in (False, True):
                for children in ((1,2,3,4) if no_edid else (1,2)):
                    with self.subTest(igpu=igpu, no_edid=no_edid, children=children), tempfile.TemporaryDirectory() as folder:
                        capture, output = Path(folder) / 'DIAG.TXT', Path(folder) / 'fixture.h'
                        capture.write_text(synthetic_capture(igpu=igpu, no_edid=no_edid, children=children, child_adr=0xa0), encoding='ascii')
                        options = ['--single-rtx', '--no-edid-child', '--output=' + str(output)]
                        if not igpu:
                            options.append('--no-igpu')
                        if no_edid:
                            options.append('--no-edid-driver')
                        done = subprocess.run([sys.executable, '-I', str(script), str(capture), *options], capture_output=True, text=True)
                        self.assertEqual(done.returncode, 0, done.stderr)
                        fixture = output.read_text()
                        self.assertIn(f'#define FX_NO_EDID_DRIVER {int(no_edid)}', fixture)
                        self.assertIn(f'#define FX_EXTRA_DISPLAY_CHILD {int(children>1)}', fixture)
                        self.assertIn(f'#define FX_EXTRA_DISPLAY_CHILD_COUNT {children-1}',fixture)
                        if children > 1:
                            self.assertIn('#define FX_EDID_CHILD_PATH_BYTES', fixture)

    def test_fixture_flags_cannot_invent_absence(self):
        script = HERE.parents[1] / 'internal-boot/boot/make-console-fixture.py'
        for text, option, message in ((synthetic_capture(igpu=True), '--no-igpu', 'disagrees with the display inventory'),
                                      (synthetic_capture(no_edid=False), '--no-edid-driver', 'active EdidDiscovered owner'),
                                      (synthetic_capture().replace('capture-complete=1', 'capture-complete=0'), '--no-igpu', 'complete, successful')):
            with self.subTest(option=option), tempfile.TemporaryDirectory() as folder:
                capture, output = Path(folder) / 'DIAG.TXT', Path(folder) / 'fixture.h'
                capture.write_text(text, encoding='ascii')
                done = subprocess.run([sys.executable, '-I', str(script), str(capture), '--single-rtx', '--no-edid-child', option,
                                       '--output=' + str(output)], capture_output=True, text=True)
                self.assertNotEqual(done.returncode, 0)
                self.assertIn(message, done.stderr)
                self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
