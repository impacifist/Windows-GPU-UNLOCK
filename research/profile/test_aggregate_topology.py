"""Aggregate discovery and metadata are synthetic evidence, never hardware approval."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('aggregate_synthetic_data',HERE/'test_local_topology.py')
data=importlib.util.module_from_spec(spec);spec.loader.exec_module(data)
mp=data.mp


def inspect(text, mutate=None):
    blocks=mp.parse_handles(text)
    if mutate:mutate(blocks)
    return mp.aggregate_roles(text,blocks,1,2,mp.parse_bindings(text.splitlines()))


class AggregateTopologyTests(unittest.TestCase):
    def test_aggregate_fixture_maps_combo_and_installed_protocols(self):
        for children in (1,2):
            with tempfile.TemporaryDirectory() as folder:
                source=Path(folder)/'DIAG00.TXT';target=Path(folder)/'fixture.h'
                text=data.synthetic_capture(aggregate=True,no_edid=False,children=children)
                source.write_text(text)
                done=subprocess.run([sys.executable,'-I',str(HERE.parents[1]/'internal-boot/boot/make-console-fixture.py'),
                     str(source),'--single-rtx','--aggregate','--no-igpu','--no-edid-child','--output='+str(target)],capture_output=True,text=True)
                self.assertEqual(done.returncode,0,done.stderr)
                header=target.read_text()
                for name in ('FX_Agg','FX_ComboAgent','FX_ComboImage','FX_CONSOLE_AGGREGATE','FixtureInstalledProtocols'):
                    self.assertIn(name,header)
                self.assertIn(f'#define FX_EXTRA_DISPLAY_CHILD {children-1}',header)
                ordinal={handle:index+1 for index,handle in enumerate(mp.parse_handles(text))}
                for role,handle in {'Agg':12,'ComboAgent':13,'ComboImage':14,'OutAgent':4,
                                    'OutImage':10,'GfxAgent':5,'GfxImage':11}.items():
                    self.assertIn(f'#define FX_{role} ((EFI_HANDLE){ordinal[handle]})',header)
                installed=header.split('FixtureInstalledProtocols[]={',1)[1].split('};',1)[0]
                self.assertEqual(installed.count('{(EFI_HANDLE)'),sum(len(block['protocols']) for block in mp.parse_handles(text).values()))
                self.assertIn('0x387477c1,0x69c7,0x11d2',installed)  # Input interface has no open records.
                for value in ('#define FX_RTX_WIDTH 0x780ULL','#define FX_RTX_PIXEL_FORMAT 0x0ULL',
                              '#define FX_RTX_INFO_BYTES 0x24ULL','#define FX_COMBO_IMAGE_SIZE 0x2160U',
                              '#define FX_COMBO_STOP 0x88cU','FixtureFullRtxConfig[64]','FixtureCold[9]'):
                    self.assertIn(value,header)

    def test_aggregate_fixture_refuses_incomplete_or_conflicting_replay_data(self):
        good=data.synthetic_capture(aggregate=True,no_edid=False)
        variants=[good.replace('gop pixel-format=0x0 info-bytes=0x24\n',''),
                  good.replace('gop width=0x780','gop width=0x100000000'),
                  good.replace('cfg dwords=220410de','cfg dwords=220310de'),
                  good.replace('mmio off=0x00000000 status=0x0000000000000000','mmio off=0x00000000 status=0x0000000000000007'),
                  good.replace('image parent=0x0 device=0x1 base=0x100000','image parent=0x0 device=0x6 base=0x100000'),
                  good.replace('image parent=0x0 device=0x1 base=0x100000 size=0x2be40',
                               'image parent=0x0 device=0x1 base=0x100000 size=0x2be41'),
                  good.replace('image-file-path-node raw=04061400','image-file-path-node raw=04061401')]
        for text in variants:
            with self.subTest(text=text),tempfile.TemporaryDirectory() as folder:
                source=Path(folder)/'DIAG00.TXT';target=Path(folder)/'fixture.h';source.write_text(text)
                done=subprocess.run([sys.executable,'-I',str(HERE.parents[1]/'internal-boot/boot/make-console-fixture.py'),
                     str(source),'--single-rtx','--aggregate','--no-igpu','--no-edid-child','--output='+str(target)],capture_output=True,text=True)
                self.assertEqual(done.returncode,1,done.stderr)
                self.assertFalse(target.exists())

    def test_aggregate_fixture_rejects_conflicting_binding_images(self):
        with tempfile.TemporaryDirectory() as folder:
            source=Path(folder)/'DIAG00.TXT';target=Path(folder)/'fixture.h'
            text=data.synthetic_capture(aggregate=True,no_edid=False)
            source.write_text(text.replace('driver image=0xe binding=0xd','driver image=0xb binding=0xd'))
            done=subprocess.run([sys.executable,'-I',str(HERE.parents[1]/'internal-boot/boot/make-console-fixture.py'),
                 str(source),'--single-rtx','--aggregate','--no-igpu','--no-edid-child','--output='+str(target)],capture_output=True,text=True)
            self.assertNotEqual(done.returncode,0)
            self.assertIn('graph and binding image records disagree',done.stderr)
            self.assertFalse(target.exists())

    def test_recognizes_one_and_two_child_roles(self):
        for children in (1,2):
            text=data.synthetic_capture(aggregate=True,no_edid=False,children=children)
            result=inspect(text)
            self.assertEqual(result['aggregate'],12)
            self.assertEqual(result['children'],[2] if children==1 else [2,7])
            self.assertEqual(result['roles'],{'nvgop':3,'combo':13,'edid':9,'gfx':5,'out':4})

    def test_recognition_generates_exact_topology_and_combo_pins(self):
        for children in (1,2):
            result=data.analyze(data.synthetic_capture(aggregate=True,no_edid=False,children=children))
            self.assertIs(result['aggregate'],True)
            self.assertEqual(result['child_count'],children)
            defines=dict(result['defines'])
            self.assertEqual(defines['GU_LOCAL_CONSOLE_AGGREGATE'],'1')
            self.assertEqual(defines['GU_LOCAL_CHILD_COUNT'],str(children))
            self.assertEqual(defines['GU_COMBO_SIZE'],'0x2160U')
            self.assertEqual(defines['GU_COMBO_STOP'],'0x88cU')
            self.assertEqual(defines['GU_COMBO_VERSION'],'0x11U')
            self.assertEqual(defines['GU_CONSOLE_TEXT_STOP'],'0x104cU')
            self.assertEqual(defines['GU_CONSOLE_GFX_STOP'],'0xb2cU')

    def test_missing_edid_is_not_recognized_as_supported_aggregate(self):
        with self.assertRaises(mp.Refused):
            inspect(data.synthetic_capture(aggregate=True,no_edid=True))

    def test_foreign_missing_and_extra_owners_are_rejected(self):
        text=data.synthetic_capture(aggregate=True,no_edid=False)
        mutations=[
            lambda b:b[2]['edges'].pop(),
            lambda b:b[2]['edges'].append((mp.TEXT_OUT,4,2,16,1)),
            lambda b:b[12]['edges'].append((mp.GOP,4,12,16,1)),
            lambda b:b[7]['edges'].append((mp.TEXT_OUT,4,7,16,1)),
            lambda b:b[7]['protocols'].add(mp.GOP),
            lambda b:b[1]['edges'].append((mp.PATH,3,8,8,1)),
            lambda b:b[1]['edges'].append((mp.TEXT_OUT,4,1,16,1)),
            lambda b:b[8]['edges'].append((mp.TEXT_OUT,13,8,16,1)),
            lambda b:b[12]['protocols'].add(mp.PATH),
            lambda b:b[6]['protocols'].remove(mp.TEXT_IN),
        ]
        for change in mutations:
            with self.subTest(change=change),self.assertRaises(mp.Refused):inspect(text,change)

    def test_virtual_gop_shape_path_and_console_ambiguity(self):
        text=data.synthetic_capture(aggregate=True,no_edid=False)
        variants=[text.replace('gop framebuffer=0x0 bytes=0x0','gop framebuffer=0x1 bytes=0x0'),
                  text.replace('gop framebuffer=0x0 bytes=0x0','gop framebuffer=0x0 bytes=0x1'),
                  text.replace('console out=0x6 error=0x6 input=0x6','console out=0x6 error=0x6 input=0x6\nconsole out=0x6 error=0x6 input=0x6'),
                  text.replace('controller=0xc attributes=0x8','controller=0x6 attributes=0x8'),
                  text.replace('open agent=0x9 controller=0x7','open agent=0xa controller=0x7')]
        variants += [text.replace('binding handle=0x9 image=0x9 version=0x1 ',
                                  'binding handle=0x9 image=0x9 version=0x2 '),
                     text.replace('stop=0x70088c','stop=0x800000')]
        for changed in variants:
            with self.subTest(changed=changed),self.assertRaises(mp.Refused):inspect(changed)

    def test_single_physical_gop_is_not_aggregate(self):
        self.assertIsNone(inspect(data.synthetic_capture(no_edid=False)))


if __name__=='__main__':unittest.main()
